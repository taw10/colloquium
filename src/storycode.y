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
%token SC_PRESTITLE
%token SC_STRING

%%

storycode:
    %empty
  | scblock '\n' storycode { printf("End of storycode\n"); }
  ;

scblock:
    stylesheet
  | prestitle

stylesheet:
    SC_STYLES ':'

prestitle:
    SC_PRESTITLE ':' SC_STRING { printf("Presentation title: '%s'\n", $1); }

%%

void scerror(const char *s) {
	printf("Error\n");
}
