/* A Bison parser, made by GNU Bison 2.7.1.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_KATEDESC_KATE_PARSER_H_INCLUDED
# define YY_KATEDESC_KATE_PARSER_H_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int katedesc_debug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     KATE = 258,
     DEFS = 259,
     LANGUAGE = 260,
     COMMENT = 261,
     CATEGORY = 262,
     DEFINE = 263,
     MACRO = 264,
     STYLE = 265,
     REGION = 266,
     CURVE = 267,
     TEXT = 268,
     BACKGROUND = 269,
     COLOR = 270,
     POSITION = 271,
     SIZE = 272,
     DEFAULT = 273,
     METRIC = 274,
     HALIGN = 275,
     VALIGN = 276,
     HLEFT = 277,
     HCENTER = 278,
     HRIGHT = 279,
     VTOP = 280,
     VCENTER = 281,
     VBOTTOM = 282,
     POINTS = 283,
     EVENT = 284,
     STARTS = 285,
     ENDS = 286,
     AT = 287,
     START = 288,
     END = 289,
     TIME = 290,
     DURATION = 291,
     ARROW = 292,
     FROM = 293,
     TO = 294,
     MAPPING = 295,
     NONE = 296,
     FRAME = 297,
     MOTION = 298,
     BEZIER_CUBIC = 299,
     LINEAR = 300,
     CATMULL_ROM = 301,
     BSPLINE = 302,
     STATIC = 303,
     SEMANTICS = 304,
     EXTERNAL = 305,
     INTERNAL = 306,
     ALIGNMENT = 307,
     RG = 308,
     BA = 309,
     FOR = 310,
     ALPHA = 311,
     TIMEBASE = 312,
     MARKER = 313,
     POINTER = 314,
     SIMPLE_TIMED_GLYPH_MARKER = 315,
     SIMPLE_TIMED_GLYPH_STYLE_MORPH = 316,
     GLYPH = 317,
     PAUSE = 318,
     IN = 319,
     MORPH = 320,
     SECONDARY = 321,
     PATH = 322,
     SECTION = 323,
     PERIODIC = 324,
     DIRECTIONALITY = 325,
     L2R_T2B = 326,
     R2L_T2B = 327,
     T2B_R2L = 328,
     T2B_L2R = 329,
     BITMAP = 330,
     PALETTE = 331,
     COLORS = 332,
     FONT = 333,
     RANGE = 334,
     FIRST = 335,
     LAST = 336,
     CODE = 337,
     POINT = 338,
     USER = 339,
     SOURCE = 340,
     PNG = 341,
     DRAW = 342,
     VISIBLE = 343,
     ID = 344,
     BOLD = 345,
     ITALICS = 346,
     UNDERLINE = 347,
     STRIKE = 348,
     JUSTIFY = 349,
     BASE = 350,
     OFFSET = 351,
     GRANULE = 352,
     RATE = 353,
     SHIFT = 354,
     WIDTH = 355,
     HEIGHT = 356,
     CANVAS = 357,
     LEFT = 358,
     TOP = 359,
     RIGHT = 360,
     BOTTOM = 361,
     MARGIN = 362,
     MARGINS = 363,
     HORIZONTAL = 364,
     VERTICAL = 365,
     CLIP = 366,
     PRE = 367,
     MARKUP = 368,
     LOCAL = 369,
     WRAP = 370,
     WORD = 371,
     META = 372,
     NUMBER = 373,
     UNUMBER = 374,
     STRING = 375,
     FLOAT = 376,
     COLORSPEC = 377,
     IDENTIFIER = 378,
     MACRO_BODY = 379
   };
#endif
/* Tokens.  */
#define KATE 258
#define DEFS 259
#define LANGUAGE 260
#define COMMENT 261
#define CATEGORY 262
#define DEFINE 263
#define MACRO 264
#define STYLE 265
#define REGION 266
#define CURVE 267
#define TEXT 268
#define BACKGROUND 269
#define COLOR 270
#define POSITION 271
#define SIZE 272
#define DEFAULT 273
#define METRIC 274
#define HALIGN 275
#define VALIGN 276
#define HLEFT 277
#define HCENTER 278
#define HRIGHT 279
#define VTOP 280
#define VCENTER 281
#define VBOTTOM 282
#define POINTS 283
#define EVENT 284
#define STARTS 285
#define ENDS 286
#define AT 287
#define START 288
#define END 289
#define TIME 290
#define DURATION 291
#define ARROW 292
#define FROM 293
#define TO 294
#define MAPPING 295
#define NONE 296
#define FRAME 297
#define MOTION 298
#define BEZIER_CUBIC 299
#define LINEAR 300
#define CATMULL_ROM 301
#define BSPLINE 302
#define STATIC 303
#define SEMANTICS 304
#define EXTERNAL 305
#define INTERNAL 306
#define ALIGNMENT 307
#define RG 308
#define BA 309
#define FOR 310
#define ALPHA 311
#define TIMEBASE 312
#define MARKER 313
#define POINTER 314
#define SIMPLE_TIMED_GLYPH_MARKER 315
#define SIMPLE_TIMED_GLYPH_STYLE_MORPH 316
#define GLYPH 317
#define PAUSE 318
#define IN 319
#define MORPH 320
#define SECONDARY 321
#define PATH 322
#define SECTION 323
#define PERIODIC 324
#define DIRECTIONALITY 325
#define L2R_T2B 326
#define R2L_T2B 327
#define T2B_R2L 328
#define T2B_L2R 329
#define BITMAP 330
#define PALETTE 331
#define COLORS 332
#define FONT 333
#define RANGE 334
#define FIRST 335
#define LAST 336
#define CODE 337
#define POINT 338
#define USER 339
#define SOURCE 340
#define PNG 341
#define DRAW 342
#define VISIBLE 343
#define ID 344
#define BOLD 345
#define ITALICS 346
#define UNDERLINE 347
#define STRIKE 348
#define JUSTIFY 349
#define BASE 350
#define OFFSET 351
#define GRANULE 352
#define RATE 353
#define SHIFT 354
#define WIDTH 355
#define HEIGHT 356
#define CANVAS 357
#define LEFT 358
#define TOP 359
#define RIGHT 360
#define BOTTOM 361
#define MARGIN 362
#define MARGINS 363
#define HORIZONTAL 364
#define VERTICAL 365
#define CLIP 366
#define PRE 367
#define MARKUP 368
#define LOCAL 369
#define WRAP 370
#define WORD 371
#define META 372
#define NUMBER 373
#define UNUMBER 374
#define STRING 375
#define FLOAT 376
#define COLORSPEC 377
#define IDENTIFIER 378
#define MACRO_BODY 379



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 2053 of yacc.c  */
#line 2200 "kate_parser.y"

  int number;
  unsigned int unumber;
  kate_float fp;
  const char *string;
  char *dynstring;
  kate_style *style;
  kate_region *region;
  kate_curve *curve;
  uint32_t color;


/* Line 2053 of yacc.c  */
#line 318 "kate_parser.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int katedesc_parse (void *YYPARSE_PARAM);
#else
int katedesc_parse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int katedesc_parse (void);
#else
int katedesc_parse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_KATEDESC_KATE_PARSER_H_INCLUDED  */
