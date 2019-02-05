/*
 * sc2_parse.y
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
  // stuff from flex that bison needs to know about:
  extern int sclex();
  extern int scparse();
  extern FILE *scin;

  void scerror(const char *s);
%}

%union {
  int ival;
  float fval;
  char *sval;
}

%token STORYCODE TYPE
%token END

%token <ival> INT
%token <fval> FLOAT
%token <sval> STRING

%%

storycode:
  header template body_section footer {
      printf("End of storycode\n");
    }
  ;
header:
  STORYCODE FLOAT {
      printf("Reading Storycode version $2\n");
    }
  ;
template:
  typelines
  ;
typelines:
  typelines typeline
  | typeline
  ;
typeline:
  TYPE STRING {
      printf("type\n");
      free($2);
    }
  ;
body_section:
  body_lines
  ;
body_lines:
  body_lines body_line
  | body_line
  ;
body_line:
  INT INT INT INT STRING {
      printf("type\n");
      free($5);
    }
  ;
footer:
  END
  ;

%%

void scerror(const char *s) {
	printf("Error\n");
}
