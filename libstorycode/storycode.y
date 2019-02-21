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
  struct frame_geom geom;
  char character;
  double val;
}

%{
  #include <stdio.h>
  #include <stdlib.h>

  extern int sclex();
  extern int scparse();
  void scerror(struct scpctx *ctx, const char *s);
%}

%token STYLES SLIDE
%token NARRATIVE
%token PRESTITLE
%token SLIDETITLE
%token FOOTER
%token TEXTFRAME
%token IMAGEFRAME
%token BP

%token FONT TYPE PAD ALIGN FGCOL BGCOL

%token LEFT CENTER RIGHT

%token STRING
%token OPENBRACE CLOSEBRACE
%token SQOPEN SQCLOSE
%token PLUS TIMES
%token UNIT VALUE

%type <p> presentation
%type <n> narrative
%type <s> slide
%type <ss> stylesheet
%type <str> prestitle
%type <str> STRING
%type <str> imageframe
%type <str> bulletpoint
%type <str> frameopt
%type <geom> geometry
%type <len> length
%type <str> slidetitle
%type <character> UNIT
%type <val> VALUE

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

    frameopts_reset(ctx);
}

%{
void frameopts_reset(struct scpctx *ctx)
{
    ctx->geom_set = 0;
    ctx->geom.x.len = 0.0;
    ctx->geom.y.len = 0.0;
    ctx->geom.w.len = 1.0;
    ctx->geom.h.len = 1.0;
    ctx->geom.x.unit = LENGTH_FRAC;
    ctx->geom.y.unit = LENGTH_FRAC;
    ctx->geom.w.unit = LENGTH_FRAC;
    ctx->geom.h.unit = LENGTH_FRAC;
}

void str_reset(struct scpctx *ctx)
{
    ctx->n_str = 0;
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
%}

%%

presentation:
  stylesheet narrative  { presentation_add_stylesheet(ctx->p, ctx->ss);
                          presentation_add_narrative(ctx->p, ctx->n);  }
| narrative             { presentation_add_narrative(ctx->p, ctx->n);  }
;

narrative:
  narrative_el            { }
| narrative narrative_el  { }
;

narrative_el:
  prestitle   { narrative_add_prestitle(ctx->n, $1); }
| bulletpoint { narrative_add_bp(ctx->n, $1); }
| slide       { narrative_add_slide(ctx->n, $1); }
| STRING      { narrative_add_text(ctx->n, $1); }
;

/* Can be in narrative or slide */

prestitle:
  PRESTITLE STRING { $$ = $2; }
;

bulletpoint:
  BP STRING { $$ = $2; }
;


/* ------ Slide contents ------ */

slide:
  SLIDE OPENBRACE slide_parts CLOSEBRACE  { presentation_add_slide(ctx->p, ctx->s);
                                            narrative_add_slide(ctx->n, ctx->s);
                                            /* New work in progress object */
                                            ctx->s = slide_new(); }
;

slide_parts:
  %empty
| slide_parts slide_part
;

slide_part:
  prestitle   { slide_add_prestitle(ctx->s, $1); str_reset(ctx); }
| imageframe  { slide_add_image(ctx->s, $1, ctx->geom);
                frameopts_reset(ctx);
                str_reset(ctx); }
| textframe   { slide_add_text(ctx->s, ctx->str, ctx->n_str, ctx->geom);
                frameopts_reset(ctx);
                str_reset(ctx); }
| FOOTER      { slide_add_footer(ctx->s); }
| slidetitle  { slide_add_slidetitle(ctx->s, $1); str_reset(ctx); }
;

imageframe:
  IMAGEFRAME frame_options STRING { $$ = $STRING; }
;

textframe:
  TEXTFRAME frame_options multi_line_string                      { }
| TEXTFRAME frame_options OPENBRACE multi_line_string CLOSEBRACE { }
;

multi_line_string:
  STRING                        { add_str(ctx, $1); }
| multi_line_string STRING      { add_str(ctx, $2); }
| bulletpoint                   { add_str(ctx, $1); }
| multi_line_string bulletpoint { add_str(ctx, $2); }
;

/* There can be any number of options */
frame_options:
  %empty
| frame_options frame_option
;

/* Each option is enclosed in square brackets */
frame_option:
  SQOPEN frameopt SQCLOSE { }
;

frameopt:
  geometry   {}
| alignment  {}
;

geometry:
  length TIMES length PLUS length PLUS length { $$.x = $1;  $$.y = $3;  $$.w = $5;  $$.h = $7;
                                                ctx->geom = $$;   ctx->geom_set = 1; }
;

alignment:
  LEFT
| CENTER
| RIGHT
;

slidetitle:
  SLIDETITLE STRING { $$ = $2; }
;

length:
  VALUE UNIT  { $$.len = $VALUE;
                if ( $UNIT == 'f' ) $$.unit = LENGTH_UNIT;
                if ( $UNIT == 'u' ) $$.unit = LENGTH_FRAC;  }
;


/* ------ Stylesheet ------ */

stylesheet:
  STYLES OPENBRACE
   style_narrative
   style_slide
  CLOSEBRACE  { printf("stylesheet\n"); }
;

style_narrative:
  NARRATIVE OPENBRACE style_narrative_def CLOSEBRACE { printf("narrative style\n"); }
;

style_narrative_def:
  %empty
| style_narrative_def style_prestitle
| style_narrative_def styledef
;

style_slide:
  SLIDE OPENBRACE style_slide_def CLOSEBRACE { printf("slide style\n"); }
;

style_slide_def:
  %empty
| style_slide_def style_prestitle
| style_slide_def styledef
;

style_prestitle:
  PRESTITLE OPENBRACE styledefs CLOSEBRACE { printf("prestitle style\n"); }
;

styledefs:
  %empty
| styledefs styledef
;

styledef:
  FONT STRING  { printf("font def: '%s'\n", $2); }
| TYPE STRING  { printf("type def: '%s'\n", $2); }
| PAD STRING   { printf("pad def: '%s'\n", $2); }
| FGCOL STRING { printf("fgcol def: '%s'\n", $2); }
| BGCOL STRING { printf("bgcol def: '%s'\n", $2); }
| ALIGN STRING { printf("align def: '%s'\n", $2); }
;

%%

void scerror(struct scpctx *ctx, const char *s) {
	printf("Storycode parse error at %i-%i\n", yylloc.first_line, yylloc.first_column);
}
