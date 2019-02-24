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

  int lineno = 0;
%}

%option prefix="sc"
%option noyywrap nounput noinput
%s string
%s geom
%s font
%s paraspace
%s pad
%s align
%s col
%s sqb

%%

STYLES            { return SC_STYLES; }
PRESTITLE         { return SC_PRESTITLE; }
SLIDETITLE        { return SC_SLIDETITLE; }
NARRATIVE         { return SC_NARRATIVE; }
SLIDE             { return SC_SLIDE; }
BP                { return SC_BP; }
GEOMETRY          { BEGIN(geom); return SC_GEOMETRY; }
TEXT              { return SC_TEXTFRAME; }
IMAGE             { return SC_IMAGEFRAME; }
FOOTER            { return SC_FOOTER; }
FONT              { BEGIN(font); return SC_FONT; }
PAD               { BEGIN(pad); return SC_PAD; }
PARASPACE         { BEGIN(paraspace); return SC_PARASPACE; }
ALIGN             { BEGIN(align); return SC_ALIGN; }
FGCOL             { BEGIN(col); return SC_FGCOL; }
BGCOL             { BEGIN(col); return SC_BGCOL; }
SIZE              { return SC_SIZE; }
<align,sqb>(?i:left)   { return SC_LEFT; }
<align,sqb>(?i:center) { return SC_CENTER; }
<align,sqb>(?i:right)  { return SC_RIGHT; }
<string,font>.*\n { sclval.str = strdup(yytext);
                    sclval.str[yyleng-1] = '\0';
                    BEGIN(0);
                    lineno++;
                    return SC_STRING; }
"["               { BEGIN(sqb); return SC_SQOPEN; }
"]"               { BEGIN(0); return SC_SQCLOSE; }
<INITIAL>:[ ]     { BEGIN(string); }
:\n               { sclval.str = strdup(""); lineno++; return SC_STRING; }
[{]               { return '{'; }
[}]               { return '}'; }
[. ]              {}
\n                { BEGIN(0); lineno++; }
[0-9\.]+          { sclval.val = atof(yytext); return SC_VALUE; }
[uf]              { sclval.character = yytext[0]; return SC_UNIT; }
[+]               { return '+'; }
[x]               { return 'x'; }
[,]               { return ','; }
<col>#[[:xdigit:]]{6}    { sclval.str = strdup(yytext); return SC_HEXCOL; }


%%
