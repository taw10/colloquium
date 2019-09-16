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

  #include "narrative.h"
  #include "slide.h"
  #include "stylesheet.h"

  #include "storycode_parse.h"

  int lineno = 0;
  int sqb_caller = 0;
%}

%option prefix="sc"
%option noyywrap nounput noinput
%s geom
%s font
%s filename
%s paraspace
%s pad
%s align
%s col
%s sqb
%x stringesc
%x runtext
%s image
%s image_filename

%%

STYLES            { return SC_STYLES; }
PRESTITLE         { return SC_PRESTITLE; }
SLIDETITLE        { return SC_SLIDETITLE; }
NARRATIVE         { return SC_NARRATIVE; }
SLIDE             { return SC_SLIDE; }
ENDOFPRESENTATION { return SC_EOP; }
BP                { return SC_BP; }
GEOMETRY          { BEGIN(geom); return SC_GEOMETRY; }
TEXT              { return SC_TEXTFRAME; }
FOOTER            { return SC_FOOTER; }
FONT[ ]           { BEGIN(font); return SC_FONT; }
PAD               { BEGIN(pad); return SC_PAD; }
PARASPACE         { BEGIN(paraspace); return SC_PARASPACE; }
ALIGN             { BEGIN(align); return SC_ALIGN; }
FGCOL             { BEGIN(col); return SC_FGCOL; }
BGCOL             { BEGIN(col); return SC_BGCOL; }
<col>VERT         { return SC_VERT; }
<col>VERTICAL     { return SC_VERT; }
<col>HORIZ        { return SC_HORIZ; }
<col>HORIZONTAL   { return SC_HORIZ; }
<align,sqb>(?i:left)   { return SC_LEFT; }
<align,sqb>(?i:center) { return SC_CENTER; }
<align,sqb>(?i:right)  { return SC_RIGHT; }

<font>.*\n        { sclval.str = strdup(yytext);
                    sclval.str[yyleng-1] = '\0';
                    BEGIN(0);
                    lineno++;
                    return SC_FONTNAME; }

<INITIAL>IMAGE    { BEGIN(image); return SC_IMAGEFRAME; }
<image>:[ ]       { BEGIN(image_filename); return SC_TEXT_START; }
<image_filename>[^\n]*  { sclval.str = strdup(yytext);
                    lineno++;
                    return SC_FILENAME; }

<INITIAL>:[ ]     { BEGIN(runtext); return SC_TEXT_START; }
<runtext>[\\]     { BEGIN(stringesc); }
<stringesc>.      { sclval.str = strdup(yytext); BEGIN(runtext);  return SC_RUN_TEXT; }
<runtext>[\*]     { return '*'; }
<runtext>[/]      { return '/'; }
<runtext>[_]      { return '_'; }
<runtext>[^\\\*/_\n]* { sclval.str = strdup(yytext);
                    sclval.str[yyleng] = '\0';
                    return SC_RUN_TEXT; }
<runtext>\n       { BEGIN(0); lineno++; }

\n                { BEGIN(0); lineno++; }

"["               { sqb_caller = YY_START; BEGIN(sqb); return SC_SQOPEN; }
"]"               { BEGIN(sqb_caller); return SC_SQCLOSE; }
[{]               { return '{'; }
[}]               { return '}'; }
[. ]              {}
[0-9\.]+          { sclval.val = atof(yytext); return SC_VALUE; }
[uf]              { sclval.character = yytext[0]; return SC_UNIT; }
[+]               { return '+'; }
[x]               { return 'x'; }
[,]               { return ','; }
<col>#[[:xdigit:]]{6}    { sclval.str = strdup(yytext); return SC_HEXCOL; }


%%
