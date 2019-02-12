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
%token SC_STYLES
%token SC_SLIDE
%token SC_NARRATIVE
%token SC_PRESTITLE
%token SC_SLIDETITLE
%token SC_FOOTER
%token SC_TEXTFRAME
%token SC_IMAGEFRAME
%token SC_BP

%token SC_FRAMEOPTS

%token SC_FONT
%token SC_TYPE
%token SC_PAD
%token SC_ALIGN
%token SC_FGCOL
%token SC_BGCOL

%token SC_STRING
%token SC_OPENBRACE
%token SC_CLOSEBRACE

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
| SC_STRING   { printf("Text line '%s'\n", $1); }
;

stylesheet:
  SC_STYLES SC_OPENBRACE { printf("Here comes the stylesheet\n"); }
   style_narrative       { printf("Stylesheet - narrative\n"); }
   style_slide           { printf("Stylesheet - slide\n"); }
  SC_CLOSEBRACE
;


/* Can be in narrative or slide */

prestitle:
  SC_PRESTITLE SC_STRING { $$ = $2; }
;

bulletpoint:
 SC_BP SC_STRING { $$ = $2; }
;

/* ------ Slide contents ------ */

slide:
  SC_SLIDE SC_OPENBRACE { printf("start of slide\n"); }
   slide_parts
  SC_CLOSEBRACE { printf("end of slide\n"); }
;

slide_parts:
  %empty
| slide_parts slide_part
;

slide_part:
  prestitle
| imageframe
| textframe
| SC_FOOTER
| slidetitle
;

imageframe:
  SC_IMAGEFRAME frame_options SC_STRING { printf("image frame '%s'\n", $SC_STRING); }
;

textframe:
  SC_TEXTFRAME frame_options multi_line_string { printf("text frame '%s'\n", $3); }
| SC_TEXTFRAME frame_options SC_OPENBRACE multi_line_string SC_CLOSEBRACE { printf("text frame m\n"); }

multi_line_string:
  SC_STRING { printf("string '%s'\n", $1); }
| multi_line_string SC_STRING { printf("more string '%s'\n", $2); }
| bulletpoint
| multi_line_string bulletpoint
;

frame_options:
  SC_FRAMEOPTS { printf("got some options: '%s'\n", $1); }
;

slidetitle:
  SC_SLIDETITLE SC_STRING { $$ = $2; }
;


/* ------ Stylesheet ------ */

style_narrative:
  SC_NARRATIVE SC_OPENBRACE style_narrative_def SC_CLOSEBRACE { printf("narrative style\n"); }
;

style_narrative_def:
  %empty
| style_narrative_def style_prestitle
| style_narrative_def styledef
;

style_slide:
  SC_SLIDE SC_OPENBRACE style_slide_def SC_CLOSEBRACE { printf("slide style\n"); }
;

style_slide_def:
  %empty
| style_slide_def style_prestitle
| style_slide_def styledef
;

style_prestitle:
  SC_PRESTITLE SC_OPENBRACE styledefs SC_CLOSEBRACE { printf("prestitle style\n"); }
;

styledefs:
  %empty
| styledefs styledef
;

styledef:
  SC_FONT SC_STRING  { printf("font def: '%s'\n", $2); }
| SC_TYPE SC_STRING  { printf("type def: '%s'\n", $2); }
| SC_PAD SC_STRING   { printf("pad def: '%s'\n", $2); }
| SC_FGCOL SC_STRING { printf("fgcol def: '%s'\n", $2); }
| SC_BGCOL SC_STRING { printf("bgcol def: '%s'\n", $2); }
| SC_ALIGN SC_STRING { printf("align def: '%s'\n", $2); }
;

%%

void scerror(const char *s) {
	printf("Error\n");
}
