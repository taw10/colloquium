/*
 * presentation.c
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>

#include "presentation.h"
#include "slide_window.h"
#include "frame.h"
#include "imagestore.h"
#include "render.h"
#include "sc_interp.h"
#include "utils.h"


void free_presentation(struct presentation *p)
{
	/* FIXME: Loads of stuff leaks here */
	g_object_unref(p->file);
	g_object_unref(p->stylesheet_from);
	imagestore_destroy(p->is);
	free(p);
}


char *get_titlebar_string(struct presentation *p)
{
	if ( p == NULL || p->file == NULL ) {
		return strdup(_("(untitled)"));
	} else {
		char *bn = g_file_get_basename(p->file);
		return bn;
	}
}


static void find_and_load_stylesheet(struct presentation *p, GFile *file)
{
	GFile *ssfile;

	if ( file != NULL ) {

		/* First choice: /same/directory/<presentation>.ss */
		gchar *ssuri = g_file_get_uri(file);
		if ( ssuri != NULL ) {
			size_t l = strlen(ssuri);
			if ( ssuri[l-3] == '.' && ssuri[l-2] == 's' && ssuri[l-1] =='c' ) {
				ssuri[l-1] = 's';
				ssfile = g_file_new_for_uri(ssuri);
				p->stylesheet = stylesheet_load(ssfile);
				p->stylesheet_from = ssfile;
				g_free(ssuri);
			}
		}

		/* Second choice: /same/directory/stylesheet.ss */
		if ( p->stylesheet == NULL ) {
			GFile *parent = g_file_get_parent(file);
			if ( parent != NULL ) {
				ssfile = g_file_get_child(parent, "stylesheet.ss");
				if ( ssfile != NULL ) {
					p->stylesheet = stylesheet_load(ssfile);
					p->stylesheet_from = ssfile;
				}
				g_object_unref(parent);
			}
		}

	}

	/* Third choice: <cwd>/stylesheet.ss */
	if ( p->stylesheet == NULL ) {
		ssfile = g_file_new_for_path("./stylesheet.ss");
		p->stylesheet = stylesheet_load(ssfile);
		p->stylesheet_from = ssfile;
	}

	/* Fourth choice: internal default stylesheet */
	if ( p->stylesheet == NULL ) {
		ssfile = g_file_new_for_uri("resource:///uk/me/bitwiz/Colloquium/default.ss");
		p->stylesheet = stylesheet_load(ssfile);
		p->stylesheet_from = NULL;
		g_object_unref(ssfile);
	}

	/* Last resort is NULL stylesheet and SCInterpreter's defaults */
	/* We keep a reference to the GFile */
}


struct presentation *new_presentation(const char *imagestore)
{
	struct presentation *new;

	new = calloc(1, sizeof(struct presentation));
	if ( new == NULL ) return NULL;

	new->file = NULL;
	new->stylesheet_from = NULL;

	new->scblocks = NULL;

	/* Default slide size */
	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	new->completely_empty = 1;
	new->saved = 1;
	new->stylesheet = NULL;
	new->is = imagestore_new(imagestore);

	new->lang = pango_language_get_default();

	find_and_load_stylesheet(new, NULL);

	return new;
}


int save_presentation(struct presentation *p, GFile *file, GFile *ssfile)
{
	GFileOutputStream *fh;
	int r;
	int sr;
	GError *error = NULL;

	fh = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
	if ( fh == NULL ) {
		fprintf(stderr, _("Open failed: %s\n"), error->message);
		return 1;
	}
	r = save_sc_block(G_OUTPUT_STREAM(fh), p->scblocks);
	if ( r ) {
		fprintf(stderr, _("Couldn't save presentation\n"));
	}
	g_object_unref(fh);

	if ( ssfile != NULL ) {
		char *uri = g_file_get_uri(ssfile);
		printf(_("Saving stylesheet to %s\n"), uri);
		g_free(uri);
		sr = stylesheet_save(p->stylesheet, ssfile);
		if ( sr ) {
			fprintf(stderr, _("Couldn't save stylesheet\n"));
		}
		if ( p->stylesheet_from != ssfile ) {
			if ( p->stylesheet_from != NULL ) {
				g_object_unref(p->stylesheet_from);
			}
			p->stylesheet_from = ssfile;
			g_object_ref(p->stylesheet_from);
		}
	} else {
		fprintf(stderr, _("Not saving the stylesheet\n"));
		sr = 0;
	}

	if ( r || sr ) return 1;

	imagestore_set_parent(p->is, g_file_get_parent(file));

	if ( p->file != file ) {
		if ( p->file != NULL ) g_object_unref(p->file);
		p->file = file;
		g_object_ref(p->file);
	}
	p->saved = 1;
	update_titlebar(p->narrative_window);
	return 0;
}


int slide_number(struct presentation *p, SCBlock *sl)
{
	SCBlock *bl = p->scblocks;
	int n = 0;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			n++;
			if ( bl == sl ) return n;
		}
		bl = sc_block_next(bl);
	}

	return 0;
}


int num_slides(struct presentation *p)
{
	SCBlock *bl = p->scblocks;
	int n = 0;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			n++;
		}
		bl = sc_block_next(bl);
	}

	return n;
}


SCBlock *first_slide(struct presentation *p)
{
	SCBlock *bl = p->scblocks;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			return bl;
		}
		bl = sc_block_next(bl);
	}

	fprintf(stderr, "Couldn't find first slide!\n");
	return NULL;
}


SCBlock *last_slide(struct presentation *p)
{
	SCBlock *bl = p->scblocks;
	SCBlock *l = NULL;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			l = bl;
		}
		bl = sc_block_next(bl);
	}

	if ( l == NULL ) {
		fprintf(stderr, "Couldn't find last slide!\n");
	}
	return l;
}


SCBlock *next_slide(struct presentation *p, SCBlock *sl)
{
	SCBlock *bl = sl;
	int found = 0;

	while ( bl != NULL ) {
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			if ( found ) return bl;
		}
		if ( bl == sl ) {
			found = 1;
		}
		bl = sc_block_next(bl);
	}

	fprintf(stderr, "Couldn't find next slide!\n");
	return NULL;
}


SCBlock *prev_slide(struct presentation *p, SCBlock *sl)
{
	SCBlock *bl = p->scblocks;
	SCBlock *l = NULL;

	while ( bl != NULL ) {
		if ( bl == sl ) {
			if ( l == NULL ) return sl; /* Already on first slide */
			return l;
		}
		if ( safe_strcmp(sc_block_name(bl), "slide") == 0 ) {
			l = bl;
		}
		bl = sc_block_next(bl);
	}

	fprintf(stderr, "Couldn't find prev slide!\n");
	return NULL;
}


static void set_slide_size_from_stylesheet(struct presentation *p)
{
	char *result;

	result = stylesheet_lookup(p->stylesheet, "$.slide", "size");
	if ( result != NULL ) {
		float v[2];
		if ( parse_double(result, v) == 0 ) {
			p->slide_width = v[0];
			p->slide_height = v[1];
		}
	}
}


int load_presentation(struct presentation *p, GFile *file)
{
	int r = 0;
	char *everything;

	assert(p->completely_empty);

	if ( !g_file_load_contents(file, NULL, &everything, NULL, NULL, NULL) ) {
		fprintf(stderr, _("Failed to load '%s'\n"), g_file_get_uri(file));
		return 1;
	}

	p->scblocks = sc_parse(everything);
	g_free(everything);

	p->lang = pango_language_get_default();

	if ( p->scblocks == NULL ) r = 1;

	if ( r ) {
		p->completely_empty = 1;
		fprintf(stderr, _("Parse error.\n"));
		return r;  /* Error */
	}

	p->stylesheet = NULL;

	find_and_load_stylesheet(p, file);

	set_slide_size_from_stylesheet(p);

	assert(p->file == NULL);
	p->file = file;
	g_object_ref(file);

	imagestore_set_parent(p->is, g_file_get_parent(file));

	return 0;
}

