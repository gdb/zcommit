/*
   lread.h  Header file for elisp reader and destructurer
   Copyright (C) 1992 Nick Thompson (nix@cs.cmu.edu)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   This code is useful for communicating with emacs, particularly in
   subprocesses which are started by emacs.  It allows you to do structured
   ipc by passing printed s-expressions between emacs and the subprocess -
   strings are parsed into a union "Value" by this code and there is also a
   fairly convenient way to extract data.
   
  */

#include <stdlib.h>	/* for malloc() */

enum Vtag { any, nil, cons, string, symbol, integer, var };

typedef struct Value Value;
struct Value {
   enum Vtag tag;
   union {
      /* tag nil has no data */
      struct { Value *car, *cdr; } cons;
      struct { int length; char *string; } s;	/* tag string or symbol */
      struct { long i; } integer;
      struct { enum Vtag tag; void **value; } var;
   } value;
};

#define ALLOC_VALUE()	((Value *) malloc(sizeof(Value)))

#define VTAG(v) (v?((v)->tag):nil)

extern Value *vmake_cons(Value *car, Value *cdr);
#define VCAR(v) ((v)->value.cons.car)
#define VCDR(v) ((v)->value.cons.cdr)

extern Value *vmake_symbol(int length, char *data);
extern Value *vmake_symbol_c(char *s);
extern Value *vmake_string(int length, char *data);
extern Value *vmake_string_c(char *s);
extern char *vextract_string_c(Value *v);
#define VSLENGTH(v) ((v)->value.s.length)
#define VSDATA(v) ((v)->value.s.string)

extern Value *vmake_integer(int n);
#define VINTEGER(v) ((v)->value.integer.i)

extern Value *vmake_var(enum Vtag tag, void **value);
#define VVTAG(v) ((v)->value.var.tag)
#define VVDATA(v) ((v)->value.var.value)

extern Value *assqv(Value *key, Value *assoc);
extern int vlength(Value *l);

extern int eqv();
extern int parse();
extern void free_value();
