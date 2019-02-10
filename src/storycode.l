/*
 * storycode.l
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
  #define YYDEBUG 1
  #include "storycode.tab.h"
%}

%option noyywrap nounput noinput
%s string

%%

STYLES            { return SC_STYLES; }
PRESTITLE         { return SC_PRESTITLE; }
SLIDETITLE        { return SC_SLIDETITLE; }
NARRATIVE         { return SC_NARRATIVE; }
SLIDE             { return SC_SLIDE; }
BP                { return SC_BP; }
TYPE              { return SC_TYPE; }
TEXT              { return SC_TEXTFRAME; }
IMAGE             { return SC_IMAGEFRAME; }
FOOTER            { return SC_FOOTER; }
FONT              { return SC_FONT; }
PAD               { return SC_PAD; }
ALIGN             { return SC_ALIGN; }
FGCOL             { return SC_FGCOL; }
BGCOL             { return SC_BGCOL; }
<string>.*\n      { sclval = strdup(yytext); sclval[yyleng-1] = '\0'; BEGIN(0); return SC_STRING; }
"[".*"]"          { sclval = strdup(yytext); return SC_FRAMEOPTS; }
:[ ]              { BEGIN(string); }
:\n               { sclval = strdup(""); return SC_STRING; }
[{]               { return SC_OPENBRACE; }
[}]               { return SC_CLOSEBRACE; }
[.\n ]            {}


%%
