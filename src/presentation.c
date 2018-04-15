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
	int final = 0;

	/* FIXME: Loads of stuff leaks here */
	free(p->uri);
	imagestore_destroy(p->is);
	free(p);

	if ( final ) {
		gtk_main_quit();
	}
}


char *get_titlebar_string(struct presentation *p)
{
	if ( p == NULL || p->uri == NULL ) {
		return strdup(_("(untitled)"));
	} else {
		GFile *f = g_file_new_for_uri(p->uri);
		char *bn = g_file_get_basename(f);
		g_object_unref(f);
		return bn;
	}
}


struct presentation *new_presentation(const char *imagestore)
{
	struct presentation *new;

	new = calloc(1, sizeof(struct presentation));
	if ( new == NULL ) return NULL;

	new->uri = NULL;

	new->scblocks = NULL;

	/* Default slide size */
	new->slide_width = 1024.0;
	new->slide_height = 768.0;

	new->completely_empty = 1;
	new->saved = 1;
	new->stylesheet = NULL;
	new->is = imagestore_new(imagestore);

	new->lang = pango_language_get_default();

	return new;
}


int save_presentation(struct presentation *p, GFile *file)
{
	GFileOutputStream *fh;
	int r;
	GError *error = NULL;

	fh = g_file_replace(file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
	if ( fh == NULL ) {
		fprintf(stderr, _("Open failed: %s\n"), error->message);
		return 1;
	}
	r = save_sc_block(G_OUTPUT_STREAM(fh), p->scblocks);
	g_object_unref(fh);

	if ( r ) return 1;

	imagestore_set_parent(p->is, g_file_get_parent(file));
	p->uri = g_file_get_uri(file);
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

	fprintf(stderr, _("Couldn't find first slide!\n"));
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
		fprintf(stderr, _("Couldn't find last slide!\n"));
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

	fprintf(stderr, _("Couldn't find next slide!\n"));
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

	fprintf(stderr, _("Couldn't find prev slide!\n"));
	return NULL;
}


int replace_stylesheet(struct presentation *p, SCBlock *ss)
{
	/* Create style sheet from union of old and new,
	 * preferring items from the new one */

	/* If there was no stylesheet before, add a dummy one */
	if ( p->stylesheet == NULL ) {
		p->stylesheet = sc_block_append_end(p->scblocks,
		                                    "stylesheet", NULL, NULL);
	}

	/* Cut the old stylesheet out of the presentation,
	 * and put in the new one */
	sc_block_substitute(&p->scblocks, p->stylesheet, ss);
	p->stylesheet = ss;

	return 0;
}


SCBlock *find_stylesheet(SCBlock *bl)
{
	while ( bl != NULL ) {

		const char *name = sc_block_name(bl);

		if ( (name != NULL) && (strcmp(name, "stylesheet") == 0) ) {
			return bl;
		}

		bl = sc_block_next(bl);

	}

	return NULL;
}


static void install_stylesheet(struct presentation *p)
{
	if ( p->stylesheet != NULL ) {
		fprintf(stderr, _("Duplicate style sheet!\n"));
		return;
	}

	p->stylesheet = find_stylesheet(p->scblocks);

	if ( p->stylesheet == NULL ) {
		fprintf(stderr, _("No style sheet.\n"));
	}
}


static void set_slide_size_from_stylesheet(struct presentation *p)
{
	SCInterpreter *scin;
	double w, h;
	int r;

	if ( p->stylesheet == NULL ) return;

	scin = sc_interp_new(NULL, NULL, NULL, NULL);
	sc_interp_run_stylesheet(scin, p->stylesheet);  /* ss == NULL is OK */
	r = sc_interp_get_slide_size(scin, &w, &h);
	sc_interp_destroy(scin);

	if ( r == 0 ) {
		p->slide_width = w;
		p->slide_height = h;
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

	install_stylesheet(p);
	set_slide_size_from_stylesheet(p);

	assert(p->uri == NULL);
	p->uri = g_file_get_uri(file);
	imagestore_set_parent(p->is, g_file_get_parent(file));

	return 0;
}

