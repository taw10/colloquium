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

  /* NB These structures look very similar to ones in stylesheet.c.
   * However, those structures are not exposed in the API, since there's no
   * need other than re-using them for a slightly different purpose here. */
  enum parse_style_mask
  {
    STYMASK_GEOM = 1<<0,
    STYMASK_FONT = 1<<1,
    STYMASK_ALIGNMENT = 1<<2,
    STYMASK_PADDING = 1<<3,
    STYMASK_PARASPACE = 1<<4,
    STYMASK_FGCOL = 1<<5,
    STYMASK_BGCOL = 1<<6,
  };

  struct parse_style
  {
    enum parse_style_mask mask;
    struct frame_geom geom;
    char *font;
    enum alignment alignment;
    struct length padding[4];
    struct length paraspace[4];
    struct colour fgcol;
    enum gradient bggrad;
    struct colour bgcol;
    struct colour bgcol2;
  };

  struct parse_paragraph {
    struct text_run *runs;
    int n_runs;
    int max_runs;
  };

  struct parse_many_paragraphs {
    struct parse_paragraph *paras;
    int n_paras;
    int max_paras;
  };

}

%union {

  Stylesheet *ss;
  Narrative *n;
  Slide *slide;
  SlideItem *slide_item;

  char *str;
  struct text_run run;
  struct parse_paragraph para;
  struct parse_many_paragraphs many_paragraphs;

  struct parse_style style;

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
  void scerror(Narrative *n, const char *s);
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
%token UNIT VALUE HEXCOL
%token TEXT_START

%type <n> narrative
%type <slide> slide
%type <slide> slide_parts
%type <slide_item> slide_part
%type <slide_item> imageframe
%type <slide_item> slidetitle
%type <slide_item> textframe
%type <slide_item> slide_prestitle
%type <many_paragraphs> multi_line_string
%type <para> text_line
%type <para> slide_bulletpoint
%type <para> text_line_with_start
%type <run> text_run
%type <str> RUN_TEXT
%type <str> one_or_more_runs

%type <ss> stylesheet
%type <style> style_narrative_bp
%type <style> style_narrative_prestitle
%type <style> styledefs
%type <style> styledef
%type <style> slide_geom
%type <style> background
%type <style> frame_options
%type <style> frame_option
%type <style> frameopt
%type <style> style_slide_prestitle
%type <style> style_slide_text
%type <style> style_slide_title
%type <style> style_slide_footer

%type <str> FONTNAME
%type <str> FILENAME

%type <geom> geometry
%type <lenquad> lenquad
%type <col> colour
%type <str> HEXCOL
%type <len> length
%type <align> alignment
%type <character> UNIT
%type <val> VALUE
%type <grad> gradtype

%parse-param { Narrative *n };

%{

static void merge_style(struct parse_style *combined, struct parse_style inp)
{
    int i;

    switch ( inp.mask ) {

        case STYMASK_GEOM :
        combined->geom = inp.geom;
        break;

        case STYMASK_FONT :
        combined->font = inp.font;
        break;

        case STYMASK_ALIGNMENT :
        combined->alignment = inp.alignment;
        break;

        case STYMASK_PADDING :
        for ( i=0; i<4; i++ ) combined->padding[i] = inp.padding[i];
        break;

        case STYMASK_PARASPACE :
        for ( i=0; i<4; i++ ) combined->paraspace[i] = inp.paraspace[i];
        break;

        case STYMASK_FGCOL :
        copy_col(&combined->fgcol, inp.fgcol);
        break;

        case STYMASK_BGCOL :
        copy_col(&combined->bgcol, inp.bgcol);
        copy_col(&combined->bgcol2, inp.bgcol2);
        combined->bggrad = inp.bggrad;
        break;

        default :
        printf("Can't merge style %i\n", inp.mask);
        return;

    }

    combined->mask |= inp.mask;
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


void push_paragraph(struct parse_many_paragraphs *mp, struct parse_paragraph p)
{
    if ( mp->n_paras == mp->max_paras ) {
        struct parse_paragraph *nparas;
        nparas = realloc(mp->paras, (mp->max_paras+8)*sizeof(struct parse_paragraph));
        if ( nparas == NULL ) return;
        mp->max_paras += 8;
        mp->paras = nparas;
    }

    mp->paras[mp->n_paras++] = p;
}


struct text_run **combine_paras(struct parse_many_paragraphs mp, int **pn_runs)
{
    struct text_run **combined_paras;
    int *n_runs;
    int i;

    combined_paras = malloc(mp.n_paras * sizeof(struct text_run *));
    n_runs = malloc(mp.n_paras * sizeof(int));
    for ( i=0; i<mp.n_paras; i++ ) {
        if ( mp.paras[i].n_runs > 0 ) {
            combined_paras[i] = mp.paras[i].runs;
            n_runs[i] = mp.paras[i].n_runs;
        } else {
            /* Create a single dummy run */
            struct text_run *run;
            run = malloc(sizeof(struct text_run));
            run->text = strdup("");
            run->type = TEXT_RUN_NORMAL;
            combined_paras[i] = run;
            n_runs[i] = 1;
        }
    }

    *pn_runs = n_runs;
    return combined_paras;
}


void set_stylesheet(Narrative *n, struct parse_style *style, const char *element)
{
    Stylesheet *ss = narrative_get_stylesheet(n);
    if ( style->mask & STYMASK_GEOM ) stylesheet_set_geometry(ss, element, style->geom);
    if ( style->mask & STYMASK_FONT ) stylesheet_set_font(ss, element, style->font);
    if ( style->mask & STYMASK_ALIGNMENT ) stylesheet_set_alignment(ss, element, style->alignment);
    if ( style->mask & STYMASK_PADDING ) stylesheet_set_padding(ss, element, style->padding);
    if ( style->mask & STYMASK_PARASPACE ) stylesheet_set_paraspace(ss, element, style->paraspace);
    if ( style->mask & STYMASK_FGCOL ) stylesheet_set_fgcol(ss, element, style->fgcol);
    if ( style->mask & STYMASK_BGCOL ) stylesheet_set_background(ss, element, style->bggrad,
                                                               style->bgcol, style->bgcol2);
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
  PRESTITLE TEXT_START text_line  { narrative_add_prestitle(n, $3.runs, $3.n_runs); }
| BP TEXT_START text_line         { narrative_add_bp(n, $3.runs, $3.n_runs); }
| TEXT_START text_line            { narrative_add_text(n, $2.runs, $2.n_runs); }
| slide                           { narrative_add_slide(n, $1); }
| EOP                             { narrative_add_eop(n); }
;

text_line: { $<para>$.n_runs = 0;
             $<para>$.max_runs = 0;
             $<para>$.runs = NULL;
           }
  %empty
| text_line text_run   {
                         if ( $<para>$.n_runs == $<para>$.max_runs ) {
                             struct text_run *nruns;
                             nruns = realloc($<para>$.runs,
                                             ($<para>$.max_runs+8)*sizeof(struct text_run));
                             if ( nruns != NULL ) {
                                $<para>$.max_runs += 8;
                                $<para>$.runs = nruns;
                            }
                         }
                         if ( $<para>$.n_runs < $<para>$.max_runs ) {
                            $<para>$.runs[$<para>$.n_runs++] = $2;
                         }
                       }

;

/* FIXME: Modifiers might be nested or overlap, e.g.
 *      _hello *there*, world_
 *      _hello *there_, world*
 */

text_run:
  RUN_TEXT         { $$.text = $1;  $$.type = TEXT_RUN_NORMAL; }
| '*' one_or_more_runs '*' { $$.text = $2;  $$.type = TEXT_RUN_BOLD; }
| '/' one_or_more_runs '/' { $$.text = $2;  $$.type = TEXT_RUN_ITALIC; }
| '_' one_or_more_runs '_' { $$.text = $2;  $$.type = TEXT_RUN_UNDERLINE; }

one_or_more_runs:
  RUN_TEXT { $$ = $1; }
| one_or_more_runs RUN_TEXT { char *nt;
                              size_t len;
                              len = strlen($1) + strlen($2) + 1;
                              nt = malloc(len);
                              if ( nt != NULL ) {
                                  nt[0] = '\0';
                                  strcat(nt, $1);
                                  strcat(nt, $2);
                                  free($1);
                                  $$ = nt;
                              } else {
                                  $$ = strdup("ERROR");
                              }
                            }
;

/* -------- Slide -------- */

slide:
  SLIDE '{' slide_parts '}'  { $$ = $3; }
;

slide_parts: { $<slide>$ = slide_new(); }
  %empty
| slide_parts slide_part { slide_add_item($<slide>$, $2); }
;

slide_part:
  slide_prestitle { $$ = $1; }
| textframe       { $$ = $1; }
| slidetitle      { $$ = $1; }
| imageframe      { $$ = $1; }
| FOOTER          { $$ = slide_item_footer(); }
;

slide_prestitle:
  PRESTITLE multi_line_string { struct text_run **cp;
                                int *n_runs;
                                cp = combine_paras($2, &n_runs);
                                $$ = slide_item_prestitle(cp, n_runs, $2.n_paras);
                              }
| PRESTITLE '{' multi_line_string '}' { struct text_run **cp;
                                        int *n_runs;
                                        cp = combine_paras($3, &n_runs);
                                        $$ = slide_item_prestitle(cp, n_runs, $3.n_paras);
                                      }
;

slidetitle:
  SLIDETITLE multi_line_string { struct text_run **cp;
                                   int *n_runs;
                                   cp = combine_paras($2, &n_runs);
                                   $$ = slide_item_slidetitle(cp, n_runs, $2.n_paras);
                                 }
| SLIDETITLE '{' multi_line_string '}' { struct text_run **cp;
                                         int *n_runs;
                                         cp = combine_paras($3, &n_runs);
                                         $$ = slide_item_slidetitle(cp, n_runs, $3.n_paras);
                                       }
;

imageframe:
  IMAGEFRAME frame_options TEXT_START FILENAME { if ( $2.mask & STYMASK_GEOM ) {
                                                     $$ = slide_item_image($4, $2.geom);
                                                 } else {
                                                     printf("Image frame must have geometry.\n");
                                                 }
                                               }
;

textframe:
  TEXTFRAME frame_options multi_line_string { struct text_run **cp;
                                              int *n_runs;
                                              cp = combine_paras($3, &n_runs);
                                              $$ = slide_item_text(cp, n_runs, $3.n_paras,
                                                                   $2.geom, $2.alignment);
                                            }
| TEXTFRAME frame_options '{' multi_line_string '}' { struct text_run **cp;
                                                      int *n_runs;
                                                      cp = combine_paras($4, &n_runs);
                                                      $$ = slide_item_text(cp, n_runs, $4.n_paras,
                                                                           $2.geom, $2.alignment);
                                                    }
;

text_line_with_start:
  TEXT_START text_line { $$ = $2; }
;

slide_bulletpoint:
  BP TEXT_START text_line { $$ = $3; }
;

multi_line_string:   { $<many_paragraphs>$.n_paras = 0;
                       $<many_paragraphs>$.paras = NULL;
                       $<many_paragraphs>$.max_paras = 0;  }
  text_line_with_start                   { push_paragraph(&$<many_paragraphs>$, $2); }
| multi_line_string text_line_with_start { push_paragraph(&$<many_paragraphs>$, $2); }
| slide_bulletpoint                      { push_paragraph(&$<many_paragraphs>$, $1); }
| multi_line_string slide_bulletpoint    { push_paragraph(&$<many_paragraphs>$, $2); }
;

/* There can be any number of options */
frame_options: { $<style>$.mask = 0;  $<style>$.alignment = ALIGN_INHERIT; }
  %empty
| frame_options frame_option { merge_style(&$<style>$, $2); }
;

/* Each option is enclosed in square brackets */
frame_option:
  '[' frameopt ']' { $$ = $2; }
;

frameopt:
  geometry   { $$.geom = $1;  $$.mask = STYMASK_GEOM; }
| alignment  { $$.alignment = $1; $$.mask = STYMASK_ALIGNMENT; }
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
  STYLES '{' stylesheet_blocks '}' { }
;

stylesheet_blocks: { }
  %empty
| stylesheet_blocks stylesheet_block { }
;

stylesheet_block:
  NARRATIVE '{' style_narrative_defs '}' { }
| SLIDE '{' style_slide_defs '}' { }
;

style_narrative_defs:
  %empty
| style_narrative_defs style_narrative_def { }
;

style_slide_defs:
  %empty
| style_slide_defs style_slide_def { }
;

style_narrative_def:
  style_narrative_prestitle { set_stylesheet(n, &$1, "NARRATIVE.PRESTITLE"); }
| style_narrative_bp        { set_stylesheet(n, &$1, "NARRATIVE.BP"); }
| styledef                  { set_stylesheet(n, &$1, "NARRATIVE"); }
;

style_slide_def:
  background            { set_stylesheet(n, &$1, "SLIDE"); }
| slide_geom            { set_stylesheet(n, &$1, "SLIDE"); }
| style_slide_prestitle { set_stylesheet(n, &$1, "SLIDE.PRESTITLE"); }
| style_slide_text      { set_stylesheet(n, &$1, "SLIDE.TEXT"); }
| style_slide_title     { set_stylesheet(n, &$1, "SLIDE.SLIDETITLE"); }
| style_slide_footer    { set_stylesheet(n, &$1, "SLIDE.FOOTER"); }
;

style_narrative_prestitle:
  PRESTITLE '{' styledefs '}' { $$ = $3; }
;

style_narrative_bp:
  BP '{' styledefs '}' { $$ = $3; }
;

background:
  BGCOL colour       { copy_col(&$$.bgcol, $2);
                       $$.bggrad = GRAD_NONE;
                       $$.mask = STYMASK_BGCOL; }
| BGCOL gradtype colour colour  { copy_col(&$$.bgcol, $3);
                                  copy_col(&$$.bgcol2, $4);
                                  $$.bggrad = $2;
                                  $$.mask = STYMASK_BGCOL; }
;

slide_geom:
  GEOMETRY geometry  { $$.geom = $2; $$.mask = STYMASK_GEOM; }
;

style_slide_prestitle:
  PRESTITLE '{' styledefs '}' { $$ = $3; }
;

style_slide_title:
  SLIDETITLE '{' styledefs '}' { $$ = $3; }
;

style_slide_text:
  TEXTFRAME '{' styledefs '}' { $$ = $3; }
;

style_slide_footer:
  FOOTER '{' styledefs '}' { $$ = $3; }
;

styledefs: { $<style>$.mask = 0;  $<style>$.alignment = ALIGN_INHERIT; }
  %empty
| styledefs styledef { merge_style(&$$, $2); }
;

styledef:
  FONT FONTNAME      { $$.font = $2; $$.mask = STYMASK_FONT; }
| GEOMETRY geometry  { $$.geom = $2; $$.mask = STYMASK_GEOM; }
| PAD lenquad        { for ( int i=0; i<4; i++ ) $$.padding[i] = $2[i];
                       $$.mask = STYMASK_PADDING; }
| PARASPACE lenquad  { for ( int i=0; i<4; i++ ) $$.paraspace[i] = $2[i];
                       $$.mask = STYMASK_PARASPACE; }
| FGCOL colour       { copy_col(&$$.fgcol, $2); $$.mask = STYMASK_FGCOL; }
| background         { $$ = $1; }
| ALIGN alignment    { $$.alignment = $2; $$.mask = STYMASK_ALIGNMENT; }
;

%%

void scerror(Narrative *n, const char *s) {
	printf("Storycode parse error at line %i\n", lineno);
}
