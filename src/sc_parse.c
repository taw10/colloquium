/*
 * sc_parse.c
 *
 * Copyright © 2013-2018 Thomas White <taw@bitwiz.org.uk>
 *
 * This file is part of Colloquium.
 *
 * Colloquium is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <glib.h>
#include <gio/gio.h>

#include "sc_parse.h"
#include "utils.h"

struct _scblock
{
	char *name;
	char *options;
	char *contents;

	SCBlock *next;
	SCBlock *child;
};


SCBlock *sc_block_new()
{
	SCBlock *bl;

	bl = calloc(1, sizeof(SCBlock));
	if ( bl == NULL ) return NULL;

	return bl;
}


SCBlock *sc_block_next(const SCBlock *bl)
{
	assert(bl != NULL);
	return bl->next;
}


SCBlock *sc_block_child(const SCBlock *bl)
{
	return bl->child;
}


const char *sc_block_name(const SCBlock *bl)
{
	return bl->name;
}


const char *sc_block_options(const SCBlock *bl)
{
	return bl->options;
}


const char *sc_block_contents(const SCBlock *bl)
{
	return bl->contents;
}


static SCBlock *sc_find_previous(SCBlock *top, SCBlock *find)
{
	if ( top->next == find ) return top;

	if ( top->child != NULL ) {
		SCBlock *t = sc_find_previous(top->child, find);
		if ( t != NULL ) return t;
	}
	if ( top->next != NULL ) {
		SCBlock *t = sc_find_previous(top->next, find);
		if ( t != NULL ) return t;
	}
	return NULL;
}


/* Add a new block before "bl" */
SCBlock *sc_block_prepend(SCBlock *bl, SCBlock *top)
{
	SCBlock *bln;
	SCBlock *prev;

	prev = sc_find_previous(top, bl);
	if ( prev == NULL ) {
		fprintf(stderr, _("Couldn't find previous\n"));
		return NULL;
	}

	bln = sc_block_new();
	if ( bln == NULL ) return NULL;

	prev->next = bln;
	bln->next = bl;
	return bln;
}


/* Append "bln" after "bl" */
void sc_block_append_p(SCBlock *bl, SCBlock *bln)
{
	if ( bl != NULL ) {
		bln->next = bl->next;
		bl->next = bln;
	}
}


/* Insert a new block after "bl".  "name", "options" and "contents"
 * will not be copied.  Returns the block just created, or NULL on error.
 * If *blfp points to NULL, it will updated to point at the new block.  */
SCBlock *sc_block_append(SCBlock *bl, char *name, char *opt, char *contents,
                         SCBlock **blfp)
{
	SCBlock *bln = sc_block_new();

	if ( bln == NULL ) return NULL;

	bln->name = name;
	bln->options = opt;
	bln->contents = contents;
	bln->child = NULL;
	bln->next = NULL;

        sc_block_append_p(bl, bln);

	if ( (blfp != NULL) && (*blfp == NULL) ) {
		*blfp = bln;
	}

	return bln;
}


/* Insert a new block at the end of the chain starting 'bl'.
 * "name", "options" and "contents" will not be copied.  Returns the block just
 * created, or NULL on error.  */
SCBlock *sc_block_append_end(SCBlock *bl, char *name, char *opt, char *contents)
{
	SCBlock *bln = sc_block_new();

	if ( bln == NULL ) return NULL;

	while ( bl->next != NULL ) {
		bln->next = bl->next;
		bl = bl->next;
	};

	return sc_block_append(bl, name, opt, contents, NULL);
}


void sc_block_append_block(SCBlock *bl, SCBlock *bln)
{

	if ( bl == NULL ) return;

	while ( bl->next != NULL ) bl = bl->next;

	bl->next = bln;
	bln->next = NULL;
}


/* Append a new block to the chain inside "parent".
 * "name", "options" and "contents" will not be copied.  Returns the block just
 * created, or NULL on error. */
SCBlock *sc_block_append_inside(SCBlock *parent,
                                char *name, char *opt, char *contents)
{
	SCBlock *bln;
	SCBlock *bl;
	SCBlock **ptr;

	bl = parent->child;
	if ( bl != NULL ) {
		while ( bl->next != NULL ) bl = bl->next;
		ptr = &bl->next;
	} else {
		ptr = &parent->child;
	}

	bln = sc_block_new();
	if ( bln == NULL ) return NULL;

	bln->name = name;
	bln->options = opt;
	bln->contents = contents;
	bln->child = NULL;
	bln->next = NULL;

	*ptr = bln;

	return bln;
}


/* Insert a new block to the chain, just after "afterme".
 * "name", "options" and "contents" will not be copied.  Returns the block just
 * created, or NULL on error. */
SCBlock *sc_block_insert_after(SCBlock *afterme,
                               char *name, char *opt, char *contents)
{
	SCBlock *bl;

	bl = sc_block_new();
	if ( bl == NULL ) return NULL;

	bl->name = name;
	bl->options = opt;
	bl->contents = contents;
	bl->child = NULL;
	bl->next = afterme->next;
	afterme->next = bl;

	return bl;
}


static SCBlock *sc_find_parent(SCBlock *top, SCBlock *find)
{
	if ( top->child == find ) return top;
	if ( top->next == find ) return top;

	if ( top->child != NULL ) {
		SCBlock *t = sc_find_parent(top->child, find);
		if ( t != NULL ) return t;
	}
	if ( top->next != NULL ) {
		SCBlock *t = sc_find_parent(top->next, find);
		if ( t != NULL ) return t;
	}
	return NULL;
}


/* Unlink "deleteme", which is somewhere under "top" */
static int sc_block_unlink(SCBlock **top, SCBlock *deleteme)
{
	SCBlock *parent = sc_find_parent(*top, deleteme);
	if ( parent == NULL ) {
		/* Maybe it's the first block? */
		if ( *top == deleteme ) {
			fprintf(stderr, _("Unlinking at top\n"));
			*top = (*top)->next;
			return 0;
		} else {
			fprintf(stderr, _("Couldn't find block parent!\n"));
			return 1;
		}
	}

	if ( parent->next == deleteme ) {
		parent->next = deleteme->next;
	}

	if ( parent->child == deleteme ) {
		parent->child = deleteme->next;
	}
	return 0;
}


/* Delete "deleteme", which is somewhere under "top" */
int sc_block_delete(SCBlock **top, SCBlock *deleteme)
{
	int r;
	r = sc_block_unlink(top, deleteme);
	if ( !r ) {
		sc_block_free(deleteme);
	}
	return r;
}


/* Frees "bl" and all its children (but not the blocks following it) */
void sc_block_free(SCBlock *bl)
{
	if ( bl == NULL ) return;

	if ( bl->child != NULL ) {
		SCBlock *ch = bl->child;
		while ( ch != NULL ) {
			SCBlock *next = ch->next;
			sc_block_free(ch);
			ch = next;
		}
	}

	free(bl);
}


/* Serialise one block (including children) */
char *serialise_sc_block(const SCBlock *bl)
{
	char *a;
	SCBlock *ch;
	size_t len = 3;

	if ( bl == NULL ) return strdup("");

	if ( bl->name != NULL ) len += 1+strlen(bl->name);
	if ( bl->options != NULL ) len += 2+strlen(bl->options);
	if ( bl->contents != NULL ) len += 2+strlen(bl->contents);
	a = malloc(len);
	if ( a == NULL ) return NULL;
	a[0] = '\0';

	if ( bl->name == NULL ) {
		strcat(a, bl->contents);
	} else if ( strcmp(bl->name, "newpara") == 0 ) {
		strcat(a, "\n");

	} else {

		strcat(a, "\\");
		strcat(a, bl->name);
		if ( bl->options != NULL ) {
			strcat(a, "[");
			strcat(a, bl->options);
			strcat(a, "]");
		}
		if ( (bl->contents != NULL) || (bl->child != NULL) ) {
			strcat(a, "{");
		}
		if ( bl->contents != NULL ) {
			strcat(a, bl->contents);
		}

		/* Special case to prevent "\somethingSome text" */
		if ( (bl->name != NULL) && (bl->options == NULL)
		  && (bl->contents == NULL) && (bl->next != NULL)
		  && (bl->next->name == NULL) && (bl->child == NULL) )
		{
			strcat(a, "{}");
		}

	}

	/* Add ALL child blocks of this one */
	ch = bl->child;
	while ( ch != NULL ) {

		char *anew;
		char *c = serialise_sc_block(ch);
		if ( c == NULL ) {
			free(a);
			return NULL;
		}

		len += strlen(c);

		anew = realloc(a, len);
		if ( anew == NULL ) {
			return NULL;
		} else {
			a = anew;
		}

		strcat(a, c);
		free(c);

		ch = ch->next;

	}

	if ( (bl->name != NULL) &&
	     ((bl->contents != NULL) || (bl->child != NULL)) ) {
		strcat(a, "}");
	}

	return a;
}


int save_sc_block(GOutputStream *fh, const SCBlock *bl)
{
	while ( bl != NULL ) {
		GError *error = NULL;
		char *a = serialise_sc_block(bl);
		gssize r;
		if ( a == NULL ) {
			fprintf(stderr, _("Failed to serialise block\n"));
			return 1;
		}
		r = g_output_stream_write(fh, a, strlen(a), NULL, &error);
		if ( r == -1 ) {
			fprintf(stderr, _("Write failed: %s\n"), error->message);
			return 1;
		}
		free(a);
		bl = bl->next;
	}
	return 0;
}


static void recursive_show_sc_blocks(const char *prefix, const SCBlock *bl)
{
	while ( bl != NULL ) {
		show_sc_block(bl, prefix);
		bl = bl->next;
	}
}


void show_sc_block(const SCBlock *bl, const char *prefix)
{
	printf("%s (%p) ", prefix,  bl);
	if ( bl == NULL ) return;
	if ( bl->name != NULL ) printf("\\%s ", bl->name);
	if ( bl->options != NULL ) printf("[%s] ", bl->options);
	if ( bl->contents != NULL ) printf("{%s} ", bl->contents);
	printf("\n");

	if ( bl->child != NULL ) {
		char new_prefix[strlen(prefix)+3];
		strcpy(new_prefix, "  ");
		strcat(new_prefix, prefix);
		recursive_show_sc_blocks(new_prefix, bl->child);
	}
}


void show_sc_blocks(const SCBlock *bl)
{
	recursive_show_sc_blocks("", bl);
}


static int get_subexpr(const char *sc, char *bk, char **pcontents, int *err)
{
	size_t ml;
	int i;
	int bct = 1;
	int found = 0;
	char *contents;

	*err = 0;

	ml = strlen(sc);
	contents = malloc(ml+1);
	if ( contents == NULL ) {
		*err = -1;
		return 0;
	}
	*pcontents = contents;

	for ( i=0; i<ml; i++ ) {
		if ( sc[i] == bk[0] ) {
			bct++;
		} else if ( sc[i] == bk[1] ) {
			bct--;
		}
		if ( bct == 0 ) {
			found = 1;
			break;
		}
		contents[i] = sc[i];
	}

	if ( (!found) || (bct != 0) ) {
		*err = 1;
		return 0;
	}

	contents[i] = '\0';
	return i+1;
}


static size_t read_block(const char *sc, char **pname, char **options,
                         char **contents, int *err)
{
	size_t l, i, j;
	char *name;
	int done;

	*err = 0;

	l = strlen(sc);
	i = 0;  j = 0;
	name = malloc(l+1);
	if ( name == NULL ) {
		*err = 1;
		return 0;
	}

	done = 0;
	do {

		char c = sc[i];

		if ( isalnum(c) ) {
			name[j++] = c;
			i++;
		} else {
			/* Found the end of the name */
			done = 1;
		}

	} while ( !done && (i<l) );

	name[j] = '\0';
	*pname = name;

	if ( sc[i] == '[' ) {

		i += get_subexpr(sc+i+1, "[]", options, err) + 1;
		if ( *err ) {
			printf(_("Couldn't find end of options '%s'\n"), sc+i);
			return 0;
		}

	} else {
		*options = NULL;
	}

	if ( sc[i] == '{' ) {

		i += get_subexpr(sc+i+1, "{}", contents, err) + 1;
		if ( *err ) {
			printf(_("Couldn't find end of content '%s'\n"), sc+i);
			return 0;
		}

	} else {
		*contents = NULL;
	}

	return i+1;
}


static void separate_newlines(SCBlock *bl)
{
	while ( bl != NULL ) {

		char *npos;
		const char *contents = sc_block_contents(bl);

		if ( contents != NULL ) {
			npos = strchr(contents, '\n');
			if ( npos != NULL ) {
				SCBlock *nb = NULL;
				if ( npos == contents ) {
					bl->name = strdup("newpara");
					bl->contents = NULL;
					nb = bl;
				} else {
					sc_block_append(bl, strdup("newpara"),
					                NULL, NULL, &nb);
				}

				/* Add any text after the \n */
				if ( strlen(npos+1) > 0 ) {
					sc_block_append(nb, NULL, NULL,
					                strdup(npos+1), &nb);
				}
				npos[0] = '\0';
			}
		}

		if ( sc_block_child(bl) != NULL ) {
			separate_newlines(sc_block_child(bl));
		}

		bl = sc_block_next(bl);

	}
}


SCBlock *sc_parse(const char *sc)
{
	SCBlock *bl;
	SCBlock *blf = NULL;
	char *tbuf;
	size_t len, i, j;

	if ( sc == NULL ) return NULL;

	if ( strlen(sc) == 0 ) {
		SCBlock *bl = sc_block_new();
		sc_block_set_contents(bl, g_strdup(""));
		return bl;
	}

	bl = NULL;

	len = strlen(sc);
	tbuf = malloc(len+1);
	if ( tbuf == NULL ) {
		sc_block_free(bl);
		return NULL;
	}

	i = 0;  j = 0;
	do {

		if ( sc[i] == '\\' ) {

			int err;
			char *name = NULL;
			char *opt = NULL;
			char *contents = NULL;

			/* Is this an escaped backslash? */
			if ( sc[i+1] == '\\' ) {
				tbuf[j++] = '\\';
				i += 2;  /* Skip both backslashes */
				continue;
			}

			/* No, it's a real block. Dispatch the previous block */
			if ( j != 0 ) {
				tbuf[j] = '\0';
				bl = sc_block_append(bl, NULL, NULL,
				                     strdup(tbuf), &blf);
				if ( bl == NULL ) {
					fprintf(stderr, _("Block add failed.\n"));
					sc_block_free(blf);
					free(tbuf);
					return NULL;
				}
				j = 0;
			}

			i += read_block(sc+i+1, &name, &opt, &contents, &err);
			if ( err ) {
				printf(_("Parse error\n"));
				sc_block_free(blf);
				free(tbuf);
				return NULL;
			}

			bl = sc_block_append(bl, name, opt, contents, &blf);
			if ( bl == NULL ) {
				fprintf(stderr, _("Block add failed.\n"));
				sc_block_free(blf);
				free(tbuf);
				return NULL;
			}
			bl->child = sc_parse(contents);
			free(bl->contents);
			bl->contents = NULL;

		} else {

			tbuf[j++] = sc[i++];
		}

	} while ( i<len );

	/* Add final block, if it exists */
	if ( j > 0 ) {

		/* Leftover buffer is empty? */
		if ( (j==1) && (tbuf[0]=='\0') ) return bl;

		tbuf[j] = '\0';
		bl = sc_block_append(bl, NULL, NULL, tbuf, &blf);
		if ( bl == NULL ) {
			fprintf(stderr, _("Block add failed.\n"));
			sc_block_free(blf);
			free(tbuf);
			return NULL;
		}
		j = 0;
	}

	separate_newlines(blf);

	return blf;
}


void sc_block_set_name(SCBlock *bl, char *nam)
{
	if ( bl == NULL ) {
		fprintf(stderr, _("sc_block_set_name: NULL block\n"));
		return;
	}
	free(bl->name);
	bl->name = nam;
}


void sc_block_set_options(SCBlock *bl, char *opt)
{
	free(bl->options);
	bl->options = opt;
}


void sc_block_set_contents(SCBlock *bl, char *con)
{
	g_free(bl->contents);
	bl->contents = con;
}


void sc_insert_text(SCBlock *b1, size_t o1, const char *t)
{
	size_t len;
	char *cnew;
	char *tmp;
	char *p1;

	if ( b1->contents == NULL ) {
		b1->contents = strdup(t);
		return;
	}
	len = strlen(b1->contents)+1+strlen(t);

	cnew = realloc(b1->contents, len);
	if ( cnew == NULL ) return;

	tmp = malloc(len);
	if ( tmp == NULL ) {
		free(cnew);
		return;
	}

	p1 = cnew + o1;
	strcpy(tmp, p1);
	strcpy(p1, t);
	strcpy(p1+strlen(t), tmp);
	free(tmp);
	b1->contents = cnew;
}


void sc_insert_block(SCBlock *b1, int o1, SCBlock *ins)
{
	SCBlock *second;
	char *p1 = g_utf8_offset_to_pointer(b1->contents, o1);

	/* Create a new block containing the second half of b1 */
	second = sc_block_new();
	sc_block_set_contents(second, g_strdup(p1));

	/* Chop off b1 at the insertion point */
	sc_block_set_contents(b1, g_utf8_substring(b1->contents, 0, o1));

	/* Link the new block into the chain */
	SCBlock *old_next = b1->next;
	b1->next = ins;
	ins->next = second;
	second->next = old_next;
}


/* Delete text from SCBlock contents.  o2=-1 means "to the end".
 * Returns the number of bytes deleted. */
size_t scblock_delete_text(SCBlock *b, ssize_t o1, ssize_t o2)
{
	size_t len;

	if ( b->contents == NULL ) {
		fprintf(stderr, _("Deleting text from block \\%s\n"), b->name);
		return 0;
	}

	if ( (o2 != -1) && (o1 > o2) ) {
		ssize_t t = o2;
		o2 = o1;
		o1 = t;
	}

	len = strlen(b->contents);
	if ( o2 < 0 ) o2 = len;
	if ( (o1 >= o2) || (o1 > len) || (o2 > len) ) {
		fprintf(stderr, _("Invalid delete: %i %i %i\n"),
		        (int)o1, (int)o2, (int)len);
		return 0;
	}
	memmove(b->contents+o1, b->contents+o2, len-o2+1);

	return o2-o1;
}


static char *s_strdup(const char *a)
{
	if ( a == NULL ) return NULL;
	return strdup(a);
}


SCBlock *sc_block_split(SCBlock *bl, size_t pos)
{
	SCBlock *n = sc_block_new();

	if ( bl->child != NULL ) {
		fprintf(stderr, _("Splitting a block with a child!\n"));
		return NULL;
	}

	/* Second block */
	n->name = s_strdup(bl->name);
	n->options = s_strdup(bl->options);
	if ( bl->contents != NULL ) {
		n->contents = strdup(bl->contents+pos);
		/* Truncate the first block */
		bl->contents[pos] = '\0';
	} else {
		n->contents = NULL;
	}

	n->next = bl->next;
	bl->next = n;

	return n;
}


/* Return a new block which is the parent for "bl" */
SCBlock *sc_block_new_parent(SCBlock *bl, const char *name)
{
	SCBlock *n = sc_block_new();
	if ( n == NULL ) return NULL;
	n->name = s_strdup(name);
	n->child = bl;
	return n;
}
