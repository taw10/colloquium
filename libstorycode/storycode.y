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

  #include "narrative.h"
  #include "narrative_priv.h"
  #include "slide.h"
  #include "stylesheet.h"

  #include "scparse_priv.h"

}

%union {
  Stylesheet *ss;
  Narrative *n;
  Slide *s;
  char *str;
  struct {
     char *text;
     enum narrative_run_type type;
  } str_w_type;
  struct length len;
  struct length lenquad[4];
  struct frame_geom geom;
  char character;
  double val;
  struct colour col;
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

%define parse.trace

%token STYLES
%token SLIDE
%token EOP
%token NARRATIVE
%token PRESTITLE
%token SLIDETITLE
%token FOOTER
%token TEXTFRAME
%token IMAGEFRAME
%token FILENAME
%token BP
%token FONT GEOMETRY PAD ALIGN FGCOL BGCOL PARASPACE
%token VERT HORIZ
%token LEFT CENTER RIGHT
%token FONTNAME RUN_TEXT
%token SQOPEN SQCLOSE
%token UNIT VALUE HEXCOL
%token TEXT_START

%type <n> narrative
%type <s> slide
%type <ss> stylesheet
%type <str> slide_prestitle
%type <str> FONTNAME
%type <str> imageframe
%type <str> slide_bulletpoint
%type <str> frameopt
%type <str> FILENAME
%type <str> RUN_TEXT
%type <str_w_type> text_run
%type <geom> geometry
%type <lenquad> lenquad
%type <col> colour
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
    ctx->n = narrative_new();

    /* The slide currently being created.
     * Will be added to the narrative when complete */
    ctx->s = slide_new();

    ctx->max_runs = 32;
    ctx->runs = malloc(ctx->max_runs*sizeof(char *));
    ctx->run_types = malloc(ctx->max_runs*sizeof(enum narrative_run_type));
    if ( (ctx->runs == NULL) || (ctx->run_types == NULL) ) ctx->max_runs = 0;
    reset_runs(ctx);
}

%{


static void copy_col(struct colour *to, struct colour from)
{
    int i;
    for ( i=0; i<4; i++ ) to->rgba[i] = from.rgba[i];
    to->hexcode = from.hexcode;
}


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


void reset_runs(struct scpctx *ctx)
{
    ctx->n_runs = 0;
    ctx->mask = 0;
    ctx->alignment = ALIGN_INHERIT;
}


void add_run(struct scpctx *ctx, char *str, enum narrative_run_type type)
{
    if ( ctx->n_runs == ctx->max_runs ) {
        char **nruns;
        enum narrative_run_type *ntype;
        nruns = realloc(ctx->runs, (ctx->max_runs+32)*sizeof(char *));
        ntype = realloc(ctx->run_types, (ctx->max_runs+32)*sizeof(enum narrative_run_type));
        if ( (nruns == NULL) || (ntype == NULL) ) return;
        ctx->max_runs += 32;
        ctx->runs = nruns;
        ctx->run_types = ntype;
    }

    ctx->runs[ctx->n_runs] = str;
    ctx->run_types[ctx->n_runs] = type;
    ctx->n_runs++;
}


void set_style(struct scpctx *ctx, const char *element)
{
    if ( ctx->mask & STYMASK_GEOM ) stylesheet_set_geometry(narrative_get_stylesheet(ctx->n),
                                                            element, ctx->geom);
    if ( ctx->mask & STYMASK_FONT ) stylesheet_set_font(narrative_get_stylesheet(ctx->n),
                                                        element, ctx->font);
    if ( ctx->mask & STYMASK_ALIGNMENT ) stylesheet_set_alignment(narrative_get_stylesheet(ctx->n),
                                                                  element, ctx->alignment);
    if ( ctx->mask & STYMASK_PADDING ) stylesheet_set_padding(narrative_get_stylesheet(ctx->n),
                                                              element, ctx->padding);
    if ( ctx->mask & STYMASK_PARASPACE ) stylesheet_set_paraspace(narrative_get_stylesheet(ctx->n),
                                                                  element, ctx->paraspace);
    if ( ctx->mask & STYMASK_FGCOL ) stylesheet_set_fgcol(narrative_get_stylesheet(ctx->n),
                                                          element, ctx->fgcol);
    if ( ctx->mask & STYMASK_BGCOL ) stylesheet_set_background(narrative_get_stylesheet(ctx->n),
                                                               element, ctx->bggrad,
                                                               ctx->bgcol, ctx->bgcol2);
    ctx->mask = 0;
    ctx->alignment = ALIGN_INHERIT;
}

%}

%%

/* The only thing a "presentation" really needs is narrative */
presentation:
  stylesheet narrative
| stylesheet
| narrative
;


/* ------ Narrative ------ */

narrative:
  narrative_el { }
| narrative narrative_el  { }
;

narrative_el:
  PRESTITLE TEXT_START text_line  { narrative_add_prestitle(ctx->n, ctx->runs, ctx->run_types, ctx->n_runs);
                                    reset_runs(ctx); }
| BP TEXT_START text_line         { narrative_add_bp(ctx->n, ctx->runs, ctx->run_types, ctx->n_runs);
                                    reset_runs(ctx); }
| TEXT_START text_line            { narrative_add_text(ctx->n, ctx->runs, ctx->run_types, ctx->n_runs);
                                    reset_runs(ctx); }
| slide                { }
| EOP                  { narrative_add_eop(ctx->n); }
;

text_line:
  %empty
| text_line text_run   { add_run(ctx, $2.text, $2.type); }
;

text_run:
  RUN_TEXT         { $$.text = $1;  $$.type = NARRATIVE_RUN_NORMAL; }
| '*' RUN_TEXT '*' { $$.text = $2;  $$.type = NARRATIVE_RUN_BOLD; }

/* -------- Slide -------- */

slide:
  SLIDE '{' slide_parts '}'  { narrative_add_slide(ctx->n, ctx->s);
                               /* New work in progress object */
                               ctx->s = slide_new(); }
;

slide_parts:
  %empty
| slide_parts slide_part
;

slide_part:
  slide_prestitle { slide_add_prestitle(ctx->s, ctx->runs, ctx->n_runs);
                    reset_runs(ctx); }
| imageframe      { slide_add_image(ctx->s, $1, ctx->geom);
                    reset_runs(ctx); }
| textframe       { slide_add_text(ctx->s, ctx->runs, ctx->n_runs,
                                   ctx->geom, ctx->alignment);
                    reset_runs(ctx); }
| FOOTER          { slide_add_footer(ctx->s); }
| slidetitle      { slide_add_slidetitle(ctx->s, ctx->runs, ctx->n_runs);
                    reset_runs(ctx); }
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
  IMAGEFRAME frame_options TEXT_START FILENAME { $$ = $FILENAME; }
;

textframe:
  TEXTFRAME frame_options multi_line_string         { }
| TEXTFRAME frame_options '{' multi_line_string '}' { }
;

text_line_with_start:
  TEXT_START text_line { }
;

multi_line_string:
  text_line_with_start                { }
| multi_line_string text_line_with_start { }
| slide_bulletpoint                   { }
| multi_line_string slide_bulletpoint { }
;

slide_bulletpoint:
  BP TEXT_START text_line { }
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
  VALUE ',' VALUE ',' VALUE ',' VALUE { $$.rgba[0] = $1;  $$.rgba[1] = $3;
                                        $$.rgba[2] = $5;  $$.rgba[3] = $7;
                                        $$.hexcode = 0;  }
| HEXCOL { double col[3];
           if ( hex_to_double($1, col) ) {
               $$.rgba[0] = col[0];  $$.rgba[1] = col[1];
               $$.rgba[2] = col[2];  $$.rgba[3] = 1.0;
               $$.hexcode = 1;
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
  '}' { }
;

style_narrative:
  NARRATIVE '{' style_narrative_def '}' { }
;

style_narrative_def:
  %empty
| style_narrative_def style_narrative_prestitle
| style_narrative_def style_narrative_bp
| style_narrative_def styledef { set_style(ctx, "NARRATIVE"); }
;

style_narrative_prestitle:
  PRESTITLE '{' styledefs '}' { set_style(ctx, "NARRATIVE.PRESTITLE"); }
;

style_narrative_bp:
  BP '{' styledefs '}' { set_style(ctx, "NARRATIVE.BP"); }
;

style_slide:
  SLIDE '{' style_slide_def '}' { }
;

style_slide_def:
  %empty
  /* Call set_style() immediately */
| style_slide_def background            { set_style(ctx, "SLIDE"); }
| style_slide_def slide_geom            { set_style(ctx, "SLIDE"); }
  /* The ones below will call set_style() themselves */
| style_slide_def style_slide_prestitle { }
| style_slide_def style_slide_text      { }
| style_slide_def style_slide_title     { }
| style_slide_def style_slide_footer    { }
;

background:
  BGCOL colour       { copy_col(&ctx->bgcol, $2);
                       ctx->bggrad = GRAD_NONE;
                       ctx->mask |= STYMASK_BGCOL; }
| BGCOL gradtype colour colour  { copy_col(&ctx->bgcol, $3);
                                  copy_col(&ctx->bgcol2, $4);
                                  ctx->bggrad = $2;
                                  ctx->mask |= STYMASK_BGCOL; }
;

slide_geom:
  GEOMETRY geometry  { ctx->geom = $2;
                       ctx->mask |= STYMASK_GEOM; }
;

style_slide_prestitle:
  PRESTITLE '{' styledefs '}' { set_style(ctx, "SLIDE.PRESTITLE"); }
;

style_slide_title:
  SLIDETITLE '{' styledefs '}' { set_style(ctx, "SLIDE.SLIDETITLE"); }
;

style_slide_text:
  TEXTFRAME '{' styledefs '}' { set_style(ctx, "SLIDE.TEXT"); }
;

style_slide_footer:
  FOOTER '{' styledefs '}' { set_style(ctx, "SLIDE.FOOTER"); }
;

styledefs:
  %empty
| styledefs styledef
;

styledef:
  FONT FONTNAME      { ctx->font = $2;
                       ctx->mask |= STYMASK_FONT; }
| GEOMETRY geometry  { ctx->geom = $2;
                       ctx->mask |= STYMASK_GEOM; }
| PAD lenquad        { for ( int i=0; i<4; i++ ) ctx->padding[i] = $2[i];
                       ctx->mask |= STYMASK_PADDING; }
| PARASPACE lenquad  { for ( int i=0; i<4; i++ ) ctx->paraspace[i] = $2[i];
                       ctx->mask |= STYMASK_PARASPACE; }
| FGCOL colour       { copy_col(&ctx->fgcol, $2);
                       ctx->mask |= STYMASK_FGCOL; }
| background         { /* Handled in rule 'background' */ }
| ALIGN alignment    { ctx->alignment = $2;
                       ctx->mask |= STYMASK_ALIGNMENT; }
;

%%

void scerror(struct scpctx *ctx, const char *s) {
	printf("Storycode parse error at line %i\n", lineno);
}
