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

  #include "presentation.h"
  #include "narrative.h"
  #include "slide.h"
  #include "stylesheet.h"

  #include "storycode_parse.h"
%}

%option prefix="sc"
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
SIZE              { return SC_SIZE; }
(?i:left)         { return SC_LEFT; }
(?i:center)       { return SC_CENTER; }
(?i:right)        { return SC_RIGHT; }
<string>.*\n      { sclval.str = strdup(yytext); sclval.str[yyleng-1] = '\0'; BEGIN(0); return SC_STRING; }
"["               { return SC_SQOPEN; }
"]"               { return SC_SQCLOSE; }
:[ ]              { BEGIN(string); }
:\n               { sclval.str = strdup(""); return SC_STRING; }
[{]               { return SC_OPENBRACE; }
[}]               { return SC_CLOSEBRACE; }
[.\n ]            {}
[0-9\.]+          { /* FIXME: lval */ return SC_VALUE; }
[uf]              { return SC_UNIT; }
[+]               { return SC_PLUS; }
[x]               { return SC_TIMES; }


%%
