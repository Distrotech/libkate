/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


%{

#define KATE_INTERNAL
#include "kate_internal.h"

#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include "kate/kate.h"
#include "katedesc.h"
#include "kate_parser.h"

#define YY_DECL extern int katedesc_lex(YYSTYPE *lvalp)

static int previous_state=-1;

static size_t nmacros=0;
static char **macro_names=NULL;
static char **macro_bodies=NULL;

static void replace_macro(const char *name);
static const char *find_macro(const char *name);

static void process_nlines(const char *text);

static int freeze_nlines=0;

static YY_BUFFER_STATE main_buffer=NULL;

int nlines=1;

/* string   [a-zA-Z ,\.;:!\?\-\(\)\'\`0-9#/\\:@%_]+ */

%}

%option nounput
%option yymore
%option never-interactive

%s NORMAL
%x BLOCK_COMMENT MACRO_1 MACRO_2

string [^\<\>\&]+
nqstring [^\<\>\&\"]+
/* single_quoted_string \'[^\<\>\&']*\' */
/* double_quoted_string \"[^\<\>\&"]*\" */
/* single_quoted_string \'[^']*\' */
/* double_quoted_string \"[^"\n]*\" */
double_quoted_string \"[^"]*\"

digit       [0-9]
hexdigit    [0-9a-fA-F]
fp1         [-+]?{digit}+\.?([eE][-+]?{digit}+)?
fp2         [-+]?{digit}*\.{digit}+([eE][-+]?{digit}+)?
fp          {fp1}|{fp2}
unumber10   \+?{digit}+
unumber16   \+?0x{hexdigit}+
number10    [-+]?{digit}+
number16    [-+]?0x{hexdigit}+
color6      #{hexdigit}{6}
color8      #{hexdigit}{8}
identifier  [a-zA-Z_][a-zA-Z_0-9]*
macro       ${identifier}
entity      &#{digit}+;

hash_comment           #[^\n]*\n
cplusplus_comment      \/\/[^\n]*\n
eol_comment            {hash_comment}|{cplusplus_comment}

%%


<INITIAL>(\xff\xfe)|(\xfe\xff)      {
                                      fprintf(stderr,"This file seems to be encoded in UTF-16, Kate only supports UTF-8\n");
                                      fprintf(stderr,"You will need to convert it to UTF-8 first (eg, use iconv)\n");
                                    }
<INITIAL>\xef\xbb\xbf               {BEGIN(NORMAL);}
<INITIAL>.                          {yyless(0);BEGIN(NORMAL);}

<NORMAL>"/*"                 {previous_state=NORMAL;BEGIN(BLOCK_COMMENT);}
<BLOCK_COMMENT>[^*\n]+       {}
<BLOCK_COMMENT>"*"+[^*/\n]   {}
<BLOCK_COMMENT>[\r\n]        {if (!freeze_nlines) nlines++;}
<BLOCK_COMMENT>"*"+"/"       {BEGIN(previous_state);}

<MACRO_1>[ \t]+              {}
<MACRO_1>{identifier}        {BEGIN(MACRO_2);lvalp->string=yytext;return IDENTIFIER;}

<MACRO_2>\\\n                {yytext[yyleng-2]=' ';yymore();}
<MACRO_2>[^\n\\]+            {yymore();}
<MACRO_2>\n                  {BEGIN(previous_state);lvalp->string=yytext;process_nlines(yytext);return MACRO_BODY;}

{eol_comment}                {if (!freeze_nlines) nlines++;}
[ \t]+                       {}
[\r\n]                       {if (!freeze_nlines) nlines++;}

kate                         {return KATE;}

{macro}                      {replace_macro(katedesc_text+1);}

defs                         {return DEFS;}
event                        {return EVENT;}
language                     {return LANGUAGE;}
comment                      {return COMMENT;}
define                       {return DEFINE;}
style                        {return STYLE;}
region                       {return REGION;}
curve                        {return CURVE;}
text                         {return TEXT;}
background                   {return BACKGROUND;}
bg                           {return BACKGROUND;}
color                        {return COLOR;}
marker                       {return MARKER;}
position                     {return POSITION;}
size                         {return SIZE;}
halign                       {return HALIGN;}
valign                       {return VALIGN;}
default                      {return DEFAULT;}
points                       {return POINTS;}
hleft                        {return HLEFT;}
hcenter                      {return HCENTER;}
hright                       {return HRIGHT;}
vtop                         {return VTOP;}
vcenter                      {return VCENTER;}
vbottom                      {return VBOTTOM;}
starts                       {return STARTS;}
ends                         {return ENDS;}
at                           {return AT;}
start                        {return START;}
end                          {return END;}
time                         {return TIME;}
duration                     {return DURATION;}
-->                          {return ARROW;}
motion                       {return MOTION;}
mapping                      {return MAPPING;}
frame                        {return FRAME;}
none                         {return NONE;}
semantics                    {return SEMANTICS;}
external                     {return EXTERNAL;}
internal                     {return INTERNAL;}
alignment                    {return ALIGNMENT;}
rg                           {return RG;}
ba                           {return BA;}
for                          {return FOR;}
from                         {return FROM;}
to                           {return TO;}
macro                        {return MACRO;}
timebase                     {return TIMEBASE;}
morph                        {return MORPH;}
secondary                    {return SECONDARY;}
path                         {return PATH;}
section                      {return SECTION;}
directionality               {return DIRECTIONALITY;}
palette                      {return PALETTE;}
bitmap                       {return BITMAP;}
colors                       {return COLORS;}
font                         {return FONT;}
range                        {return RANGE;}
first                        {return FIRST;}
last                         {return LAST;}
code                         {return CODE;}
point                        {return POINT;}
user                         {return USER;}
source                       {return SOURCE;}
draw                         {return DRAW;}
visible                      {return VISIBLE;}
category                     {return CATEGORY;}
id                           {return ID;}
bold                         {return BOLD;}
italics                      {return ITALICS;}
underline                    {return UNDERLINE;}
strike                       {return STRIKE;}
periodic                     {return PERIODIC;}
granule                      {return GRANULE;}
rate                         {return RATE;}
shift                        {return SHIFT;}
width                        {return WIDTH;}
height                       {return HEIGHT;}
left                         {return LEFT;}
top                          {return TOP;}
right                        {return RIGHT;}
bottom                       {return BOTTOM;}
margin                       {return MARGIN;}
margins                      {return MARGINS;}
horizontal                   {return HORIZONTAL;}
vertical                     {return VERTICAL;}
png                          {return PNG;}
justify                      {return JUSTIFY;}
clip                         {return CLIP;}
pre                          {return PRE;}
markup                       {return MARKUP;}
canvas                       {return CANVAS;}
local                        {return LOCAL;}
offset                       {return OFFSET;}
wrap                         {return WRAP;}
word                         {return WORD;}
meta                         {return META;}



left_to_right|l2r|ltr|ltr_ttb|l2r_t2b        {return L2R_T2B;}
right_to_left|r2l|rtl|rtl_ttb|r2l_t2b        {return R2L_T2B;}
ttb_rtl|t2b_r2l                              {return T2B_R2L;}
ttb_ltr|t2b_l2r                              {return T2B_L2R;}

pixels                       {lvalp->number=kate_pixel;return METRIC;}
pixel                        {lvalp->number=kate_pixel;return METRIC;}
percentage                   {lvalp->number=kate_percentage;return METRIC;}
percent                      {lvalp->number=kate_percentage;return METRIC;}
millionths                   {lvalp->number=kate_millionths;return METRIC;}

static                       {return STATIC;}
linear                       {return LINEAR;}
catmull_rom                  {return CATMULL_ROM;}
bezier_cubic                 {return BEZIER_CUBIC;}
bspline                      {return BSPLINE;}

simple_timed_glyph_marker          {return SIMPLE_TIMED_GLYPH_MARKER;}
simple_timed_glyph_style_morph     {return SIMPLE_TIMED_GLYPH_STYLE_MORPH;}
glyph                              {return GLYPH;}
pointer                            {return POINTER;}
in                                 {return IN;}
pause                              {return PAUSE;}

{double_quoted_string}         {
                                 katedesc_text[strlen(katedesc_text)-1]=0;
                                 lvalp->string=katedesc_text+1;
                                 process_nlines(katedesc_text);
                                 return STRING;
                               }
{unumber10}                    {lvalp->unumber=strtoul(katedesc_text,NULL,10);return UNUMBER;}
{unumber16}                    {lvalp->unumber=strtoul(katedesc_text,NULL,16);return UNUMBER;}
{number10}                     {lvalp->number=strtol(katedesc_text,NULL,10);return NUMBER;}
{number16}                     {lvalp->number=strtol(katedesc_text,NULL,16);return NUMBER;}
{fp}                           {lvalp->fp=(kate_float)atof(katedesc_text);return FLOAT;}
{color6}                       {lvalp->color=strtol(katedesc_text+1,NULL,16);return COLORSPEC;}
{color8}                       {lvalp->color=strtol(katedesc_text+1,NULL,16);return COLORSPEC;}


<<EOF>>                      {
                               katedesc_text="<EOF>";
                               if (freeze_nlines) --freeze_nlines;
                               yy_delete_buffer(YY_CURRENT_BUFFER);
                               if (main_buffer) {
                                 yy_switch_to_buffer(main_buffer);
                                 main_buffer=NULL;
                               }
                               else yyterminate();
                             }

.                            {return katedesc_text[0];}


%%

int katedesc_wrap()
{
  return 1;
}

void set_macro_mode()
{
  BEGIN(MACRO_1);
}

void unset_macro_mode()
{
  BEGIN(NORMAL);
}

static void replace_macro(const char *name)
{
  const char *body=find_macro(name);
  if (body) {
    ++freeze_nlines;
    main_buffer=YY_CURRENT_BUFFER;
    yy_scan_string(body);
  }
}

void add_macro(const char *name,const char *body)
{
  size_t nlen,blen;

  if (!name || !body) {
    katedesc_error("name and body cannot be null");
    exit(-1);
  }

  ++nmacros;
  macro_names=(char**)kate_realloc(macro_names,nmacros*sizeof(char*));
  macro_bodies=(char**)kate_realloc(macro_bodies,nmacros*sizeof(char*));
  if (!macro_names || !macro_bodies) {
    katedesc_error("out of memory");
    exit(-1);
  }

  nlen=strlen(name);
  blen=strlen(body);
  macro_names[nmacros-1]=kate_malloc(nlen+1);
  macro_bodies[nmacros-1]=kate_malloc(blen+1);
  if (!macro_names[nmacros-1] || !macro_bodies[nmacros-1]) {
    katedesc_error("out of memory");
    exit(-1);
  }
  strcpy(macro_names[nmacros-1],name);
  strcpy(macro_bodies[nmacros-1],body);
}

static const char *find_macro(const char *name)
{
  size_t n;
  if (!name) {
    katedesc_error("macro name cannot be null");
    exit(-1);
  }
  for (n=0;n<nmacros;++n) {
    if (!strcmp(macro_names[n],name)) return macro_bodies[n];
  }
  katedesc_error("undefined macro");
  return NULL;
}

void free_macros(void)
{
  size_t n;
  for (n=0;n<nmacros;++n) {
    kate_free(macro_names[n]);
    kate_free(macro_bodies[n]);
  }
  kate_free(macro_names);
  kate_free(macro_bodies);
}

static int get_nlines(const char *text)
{
  int text_nlines=0;
  if (!text) {
    katedesc_error("null text");
    return 0;
  }
  while (*text) if (*text++=='\n') ++text_nlines;
  return text_nlines;
}

static void process_nlines(const char *text)
{
  if (!freeze_nlines) {
    nlines+=get_nlines(text);
  }
}

void cleanup_lexer(void)
{
#ifdef FLEX_SCANNER
#if (YY_FLEX_MAJOR_VERSION+0)*0x10000+(YY_FLEX_MINOR_VERSION+0)*0x100+(YY_FLEX_SUBMINOR_VERSION+0)>=0x020509
  katedesc_lex_destroy();
#endif
#endif
}

