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

%{
  extern int sclex();
  extern int scparse();
  void scerror(const char *s);
%}

%define api.value.type {char *}
%define api.token.prefix {SC_}
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

%%

storycode:
  %empty
| storycode scblock
;

scblock:
  stylesheet  { printf("That was the stylesheet\n"); }
| prestitle   { printf("prestitle: '%s'\n", $1); }
| bulletpoint { printf("* '%s'\n", $1); }
| slide
| STRING   { printf("Text line '%s'\n", $1); }
;

stylesheet:
  STYLES OPENBRACE { printf("Here comes the stylesheet\n"); }
   style_narrative       { printf("Stylesheet - narrative\n"); }
   style_slide           { printf("Stylesheet - slide\n"); }
  CLOSEBRACE
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
  SLIDE OPENBRACE { printf("start of slide\n"); }
   slide_parts
  CLOSEBRACE { printf("end of slide\n"); }
;

slide_parts:
  %empty
| slide_parts slide_part
;

slide_part:
  prestitle
| imageframe
| textframe
| FOOTER
| slidetitle
;

imageframe:
  IMAGEFRAME frame_options STRING { printf("image frame '%s'\n", $STRING); }
;

textframe:
  TEXTFRAME frame_options multi_line_string { printf("text frame '%s'\n", $3); }
| TEXTFRAME frame_options OPENBRACE multi_line_string CLOSEBRACE { printf("text frame m\n"); }

multi_line_string:
  STRING { printf("string '%s'\n", $1); }
| multi_line_string STRING { printf("more string '%s'\n", $2); }
| bulletpoint { printf("string *%s\n", $1); }
| multi_line_string bulletpoint { printf("more string *%s\n", $1); }
;

/* There can be any number of options */
frame_options:
  %empty
| frame_options frame_option
;

/* Each option is enclosed in square brackets */
frame_option:
  SQOPEN frameopt SQCLOSE { printf("got an option: '%s'\n", $2); }
;

frameopt:
  geometry
| alignment
;

geometry:
  length TIMES length PLUS length PLUS length { $$ = "geom"; printf("Geometry\n"); }
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
  VALUE UNIT
;


/* ------ Stylesheet ------ */

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

void scerror(const char *s) {
	printf("Error\n");
}
