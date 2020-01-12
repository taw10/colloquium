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

%option noyywrap nounput noinput never-interactive nounistd
%option reentrant
%option bison-bridge bison-locations

%s cond_geom
%s cond_font
%s cond_filename
%s cond_paraspace
%s cond_pad
%s cond_align
%s cond_col
%s cond_sqb
%x cond_stringesc
%x cond_runtext
%x cond_prerun
%s cond_image
%s cond_image_filename

%%

STYLES            { return SC_STYLES; }
PRESTITLE         { return SC_PRESTITLE; }
SLIDETITLE        { return SC_SLIDETITLE; }
NARRATIVE         { return SC_NARRATIVE; }
SLIDE             { return SC_SLIDE; }
ENDOFPRESENTATION { return SC_EOP; }
SEGMENT_START     { return SC_SEG_START; }
SEGMENT_END       { return SC_SEG_END; }
BP                { return SC_BP; }
GEOMETRY          { BEGIN(cond_geom); return SC_GEOMETRY; }
TEXT              { return SC_TEXTFRAME; }
FOOTER            { return SC_FOOTER; }
FONT[ ]           { BEGIN(cond_font); return SC_FONT; }
PAD               { BEGIN(cond_pad); return SC_PAD; }
PARASPACE         { BEGIN(cond_paraspace); return SC_PARASPACE; }
ALIGN             { BEGIN(cond_align); return SC_ALIGN; }
FGCOL             { BEGIN(cond_col); return SC_FGCOL; }
BGCOL             { BEGIN(cond_col); return SC_BGCOL; }
<cond_col>VERT         { return SC_VERT; }
<cond_col>VERTICAL     { return SC_VERT; }
<cond_col>HORIZ        { return SC_HORIZ; }
<cond_col>HORIZONTAL   { return SC_HORIZ; }
<cond_align,cond_sqb>(?i:left)   { return SC_LEFT; }
<cond_align,cond_sqb>(?i:center) { return SC_CENTER; }
<cond_align,cond_sqb>(?i:right)  { return SC_RIGHT; }

<cond_font>.*\n   { yylval->str = strdup(yytext);
                    yylval->str[yyleng-1] = '\0';
                    BEGIN(0);
                    lineno++;
                    return SC_FONTNAME; }

<INITIAL>IMAGE    { BEGIN(cond_image); return SC_IMAGEFRAME; }
<cond_image>:[ ]       { BEGIN(cond_image_filename); return SC_TEXT_START; }
<cond_image_filename>[^\n]*  { yylval->str = strdup(yytext);
                          lineno++;
                          return SC_FILENAME; }

<INITIAL>:        { BEGIN(cond_prerun); return SC_TEXT_START; }
<cond_prerun>[ ]       { BEGIN(cond_runtext); }
<cond_runtext>[\\]     { BEGIN(cond_stringesc); }
<cond_stringesc>.      { yylval->str = strdup(yytext); BEGIN(cond_runtext);  return SC_RUN_TEXT; }
<cond_runtext>[\*]     { return '*'; }
<cond_runtext>[/]      { return '/'; }
<cond_runtext>[_]      { return '_'; }
<cond_runtext>[^\\\*/_\n]* { yylval->str = strdup(yytext);
                        yylval->str[yyleng] = '\0';
                        return SC_RUN_TEXT; }
<cond_runtext>\n       { BEGIN(0); lineno++; }
<cond_prerun>\n        { BEGIN(0); lineno++; }

\n                { BEGIN(0); lineno++; }

"["               { sqb_caller = YY_START; BEGIN(cond_sqb); return '['; }
"]"               { BEGIN(sqb_caller); return ']'; }
[{]               { return '{'; }
[}]               { return '}'; }
[. ]              {}
[0-9\.]+          { yylval->val = atof(yytext); return SC_VALUE; }
[uf]              { yylval->character = yytext[0]; return SC_UNIT; }
[+]               { return '+'; }
[x]               { return 'x'; }
[,]               { return ','; }
<cond_col>#[[:xdigit:]]{6}    { yylval->str = strdup(yytext); return SC_HEXCOL; }


%%
