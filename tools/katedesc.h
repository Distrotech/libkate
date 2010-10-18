/* Copyright (C) 2008 Vincent Penquerc'h.
   This file is part of the Kate codec library.
   Written by Vincent Penquerc'h.

   Use, distribution and reproduction of this library is governed
   by a BSD style source license included with this source in the
   file 'COPYING'. Please read these terms before distributing. */


#ifndef KATE_katedesc_h_GUARD
#define KATE_katedesc_h_GUARD

#include <ogg/ogg.h>

extern int katedesc_error(const char *string);
extern int yywarning(const char *string);

extern void set_macro_mode(void);
extern void unset_macro_mode(void);
extern void add_macro(const char *name,const char *body);
extern void free_macros(void);

extern int write_headers(FILE *f);
extern void update_stream_time(kate_state *k,FILE *fout,kate_int64_t endt);
extern int send_packet(FILE *f,ogg_packet *op,kate_int64_t t);
extern void cancel_packet(ogg_packet *op);

extern void cleanup_lexer(void);

extern int nlines;

extern char *katedesc_text;
extern FILE *katedesc_out;
union YYSTYPE;
extern int yylex(union YYSTYPE *lvalp);

extern int nerrors;
extern int nwarnings;

extern kate_state k;
extern kate_info ki;
extern kate_comment kc;

extern char base_path[];

#endif

