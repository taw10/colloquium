/*
 * sc_lex.l
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
  #include "sc_parse.tab.h"

%}
%%
StoryCode      { return STORYCODE; }
type           { return TYPE; }
end            { return END; }
[ \t\n]           ;
[0-9]+\.[0-9]+    { sclval.fval = atof(yytext); return FLOAT; }
[0-9]+            { sclval.ival = atoi(yytext); return INT; }
[a-zA-Z0-9]+      {
  sclval.sval = strdup(yytext);
  return STRING;
}
%%
