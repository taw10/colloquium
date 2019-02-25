/*
 * storycode.y
 *
 * Copyright © 2019 Thomas White <taw@bitwiz.org.uk>
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

%define api.token.prefix {SC_}
%define api.prefix {sc}
%locations

%code requires {

  #include "presentation.h"
  #include "narrative.h"
  #include "slide.h"
  #include "stylesheet.h"

  #include "scparse_priv.h"
}

%union {
  Presentation *p;
  Stylesheet *ss;
  Narrative *n;
  Slide *s;
  char *str;
  struct length len;
  struct length lenquad[4];
  struct frame_geom geom;
  char character;
  double val;
  double rgba[4];
  enum alignment align;
  enum gradient grad;
}

%{
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>

  extern int sclex();
  extern int scparse();
  void scerror(struct scpctx *ctx, const char *s);
  extern int lineno;
%}

%token STYLES
%token SLIDE
%token NARRATIVE
%token PRESTITLE
%token SLIDETITLE
%token FOOTER
%token TEXTFRAME
%token IMAGEFRAME
%token BP
%token FONT GEOMETRY PAD ALIGN FGCOL BGCOL PARASPACE
%token VERT HORIZ
%token LEFT CENTER RIGHT
%token STRING
%token SQOPEN SQCLOSE
%token UNIT VALUE SIZE HEXCOL

%type <p> presentation
%type <n> narrative
%type <s> slide
%type <ss> stylesheet
%type <str> narrative_prestitle
%type <str> slide_prestitle
%type <str> STRING
%type <str> imageframe
%type <str> slide_bulletpoint
%type <str> narrative_bulletpoint
%type <str> frameopt
%type <geom> geometry
%type <lenquad> lenquad
%type <rgba> colour
%type <str> HEXCOL
%type <len> length
%type <align> alignment
%type <str> slidetitle
%type <character> UNIT
%type <val> VALUE
%type <grad> gradtype

%parse-param { struct scpctx *ctx };
%initial-action
{
    ctx->p = presentation_new();

    /* These are the objects currently being created.  They will be
     * added to the presentation when they're complete */
    ctx->n = narrative_new();
    ctx->ss = stylesheet_new();
    ctx->s = slide_new();

    ctx->n_str = 0;
    ctx->max_str = 32;
    ctx->str = malloc(ctx->max_str*sizeof(char *));
    if ( ctx->str == NULL ) ctx->max_str = 0;

    ctx->mask = 0;
}

%{

static int hex_to_double(const char *v, double *r)
{
    char c[5];

    if ( strlen(v) != 7 ) return 0;
    if ( v[0] != '#' ) return 0;  /* should've already been blocked by lexxer */

    c[0] = '0';  c[1] = 'x';  c[4] = '\0';

    c[2] = v[1]; c[3] = v[2];
    r[0] = strtod(c, NULL) / 255.0;

    c[2] = v[3]; c[3] = v[4];
    r[1] = strtod(c, NULL) / 255.0;

    c[2] = v[5]; c[3] = v[6];
    r[2] = strtod(c, NULL) / 255.0;

    return 1;
}

void str_reset(struct scpctx *ctx)
{
    ctx->n_str = 0;
    ctx->mask = 0;
    ctx->alignment = ALIGN_INHERIT;
}

void add_str(struct scpctx *ctx, char *str)
{
    if ( ctx->n_str == ctx->max_str ) {
        char **nstr = realloc(ctx->str, (ctx->max_str+32)*sizeof(char *));
        if ( nstr == NULL ) return;
        ctx->max_str += 32;
    }

    ctx->str[ctx->n_str++] = str;
}

void set_style(struct scpctx *ctx, enum style_element element)
{
    if ( ctx->mask & STYMASK_GEOM ) stylesheet_set_geometry(ctx->ss, element, ctx->geom);
    if ( ctx->mask & STYMASK_FONT ) stylesheet_set_font(ctx->ss, element, ctx->font);
    if ( ctx->mask & STYMASK_ALIGNMENT ) stylesheet_set_alignment(ctx->ss, element, ctx->alignment);
    if ( ctx->mask & STYMASK_PADDING ) stylesheet_set_padding(ctx->ss, element, ctx->padding);
    if ( ctx->mask & STYMASK_PARASPACE ) stylesheet_set_paraspace(ctx->ss, element, ctx->paraspace);
    if ( ctx->mask & STYMASK_FGCOL ) stylesheet_set_fgcol(ctx->ss, element, ctx->fgcol);
    if ( ctx->mask & STYMASK_BGCOL ) stylesheet_set_background(ctx->ss, element, ctx->bggrad,
                                                               ctx->bgcol, ctx->bgcol2);
    ctx->mask = 0;
    ctx->alignment = ALIGN_INHERIT;
}

%}

%%

/* The only thing a "presentation" really needs is narrative */
presentation:
  stylesheet narrative  { presentation_add_stylesheet(ctx->p, ctx->ss);
                          presentation_add_narrative(ctx->p, ctx->n);  }
| narrative             { presentation_add_narrative(ctx->p, ctx->n);  }
;


/* ------ Narrative ------ */

narrative:
  narrative_el            { }
| narrative narrative_el  { }
;

narrative_el:
  narrative_prestitle   { narrative_add_prestitle(ctx->n, $1); }
| narrative_bulletpoint { narrative_add_bp(ctx->n, $1); }
| slide                 { narrative_add_slide(ctx->n, $1); }
| STRING                { narrative_add_text(ctx->n, $1); }
;

narrative_prestitle:
  PRESTITLE STRING { $$ = $2; }
;

narrative_bulletpoint:
  BP STRING { $$ = $2; }
;


/* -------- Slide -------- */

slide:
  SLIDE '{' slide_parts '}'  { presentation_add_slide(ctx->p, ctx->s);
                                            narrative_add_slide(ctx->n, ctx->s);
                                            /* New work in progress object */
                                            ctx->s = slide_new(); }
;

slide_parts:
  %empty
| slide_parts slide_part
;

slide_part:
  slide_prestitle { slide_add_prestitle(ctx->s, ctx->str, ctx->n_str);
                    str_reset(ctx); }
| imageframe      { slide_add_image(ctx->s, $1, ctx->geom);
                    str_reset(ctx); }
| textframe       { slide_add_text(ctx->s, ctx->str, ctx->n_str,
                                   ctx->geom, ctx->alignment);
                    str_reset(ctx); }
| FOOTER          { slide_add_footer(ctx->s); }
| slidetitle      { slide_add_slidetitle(ctx->s, ctx->str, ctx->n_str);
                    str_reset(ctx); }
;

slide_prestitle:
  PRESTITLE frame_options multi_line_string         { }
| PRESTITLE frame_options '{' multi_line_string '}' { }
;

slidetitle:
  SLIDETITLE frame_options multi_line_string         { }
| SLIDETITLE frame_options '{' multi_line_string '}' { }
;

imageframe:
  IMAGEFRAME frame_options STRING { $$ = $STRING; }
;

textframe:
  TEXTFRAME frame_options multi_line_string         { }
| TEXTFRAME frame_options '{' multi_line_string '}' { }
;

multi_line_string:
  STRING                              { add_str(ctx, $1); }
| multi_line_string STRING            { add_str(ctx, $2); }
| slide_bulletpoint                   { add_str(ctx, $1); }
| multi_line_string slide_bulletpoint { add_str(ctx, $2); }
;

slide_bulletpoint:
  BP STRING { $$ = $2; }
;

/* There can be any number of options */
frame_options:
  %empty
| frame_options frame_option
;

/* Each option is enclosed in square brackets */
frame_option:
  SQOPEN frameopt SQCLOSE
;

frameopt:
  geometry   { ctx->geom = $1; }
| alignment  { ctx->alignment = $1; }
;

/* Primitives for describing styles (used in frame options and stylesheets) */
geometry:
  length 'x' length '+' length '+' length { $$.w = $1;  $$.h = $3;
                                            $$.x = $5;  $$.y = $7; }
;

lenquad:
  length ',' length ',' length ',' length { $$[0] = $1;  $$[1] = $3;
                                            $$[2] = $5;  $$[3] = $7; }
;

colour:
  VALUE ',' VALUE ',' VALUE ',' VALUE { $$[0] = $1;  $$[1] = $3;
                                        $$[2] = $5;  $$[3] = $7; }
| HEXCOL { double col[3];
           if ( hex_to_double($1, col) ) {
               $$[0] = col[0]; $$[1] = col[1]; $$[2] = col[2]; $$[3] = 1.0;
           }
         }
;

alignment:
  LEFT     { $$ = ALIGN_LEFT; }
| CENTER   { $$ = ALIGN_CENTER; }
| RIGHT    { $$ = ALIGN_RIGHT; }
;

length:
  VALUE UNIT  { $$.len = $VALUE;
                if ( $UNIT == 'u' ) $$.unit = LENGTH_UNIT;
                if ( $UNIT == 'f' ) $$.unit = LENGTH_FRAC; }
;

gradtype:
  VERT { $$ = GRAD_VERT; }
| HORIZ { $$ = GRAD_HORIZ; }
;


/* ------ Stylesheet ------ */

stylesheet:
  STYLES '{'
   style_narrative
   style_slide
  '}'  { }
;

style_narrative:
  NARRATIVE '{' style_narrative_def '}' { }
;

style_narrative_def:
  %empty
| style_narrative_def style_narrative_prestitle
| style_narrative_def styledef
;

style_narrative_prestitle:
  PRESTITLE '{' styledefs '}' { }
;

style_slide:
  SLIDE '{' style_slide_def '}' { }
;

style_slide_def:
  %empty
  /* Doesn't need set_style() */
| style_slide_def style_slidesize       { }
  /* Call set_style() immediately */
| style_slide_def background            { set_style(ctx, STYEL_SLIDE); }
  /* The ones below will call set_style() themselves */
| style_slide_def style_slide_prestitle { }
| style_slide_def style_slide_text      { }
| style_slide_def style_slide_title     { }
;

style_slidesize:
  SIZE length 'x' length { if ( ($2.unit != LENGTH_UNIT)
                               || ($4.unit != LENGTH_UNIT) )
                             {
                                fprintf(stderr, "Wrong slide size units\n");
                             } else {
                                stylesheet_set_slide_default_size(ctx->ss, $2.len, $4.len);
                             }
                           }
;

background:
  BGCOL colour       { for ( int i=0; i<4; i++ ) ctx->bgcol[i] = $2[i];
                       ctx->bggrad = GRAD_NONE;
                       ctx->mask |= STYMASK_BGCOL; }
| BGCOL gradtype colour colour  { for ( int i=0; i<4; i++ ) ctx->bgcol[i] = $3[i];
                                  for ( int i=0; i<4; i++ ) ctx->bgcol2[i] = $4[i];
                                  ctx->bggrad = $2;
                                  ctx->mask |= STYMASK_BGCOL; }
;

style_slide_prestitle:
  PRESTITLE '{' styledefs '}' { set_style(ctx, STYEL_SLIDE_PRESTITLE); }
;

style_slide_title:
  SLIDETITLE '{' styledefs '}' { set_style(ctx, STYEL_SLIDE_SLIDETITLE); }
;

style_slide_text:
  TEXTFRAME '{' styledefs '}' { set_style(ctx, STYEL_SLIDE_TEXT); }
;

styledefs:
  %empty
| styledefs styledef
;

styledef:
  FONT STRING        { ctx->font = $2;
                       ctx->mask |= STYMASK_FONT; }
| GEOMETRY geometry  { ctx->geom = $2;
                       ctx->mask |= STYMASK_GEOM; }
| PAD lenquad        { for ( int i=0; i<4; i++ ) ctx->padding[i] = $2[i];
                       ctx->mask |= STYMASK_PADDING; }
| PARASPACE lenquad  { for ( int i=0; i<4; i++ ) ctx->paraspace[i] = $2[i];
                       ctx->mask |= STYMASK_PARASPACE; }
| FGCOL colour       { for ( int i=0; i<4; i++ ) ctx->fgcol[i] = $2[i];
                       ctx->mask |= STYMASK_FGCOL; }
| background         { /* Handled in rule 'background' */ }
| ALIGN alignment    { ctx->alignment = $2;
                       ctx->mask |= STYMASK_ALIGNMENT; }
;

%%

void scerror(struct scpctx *ctx, const char *s) {
	printf("Storycode parse error at line %i\n", lineno);
}
