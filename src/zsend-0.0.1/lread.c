/*
  lread.c: simple sexp-like data structures in C.
	   useful for communication between emacs and C client programs

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

   TODO

   add tag checking on CAR, CDR, etc?
 */

#include <setjmp.h>

#include "lread.h"
#include <stdio.h>
#include <string.h> 	/* for strlen() */

Value *
vmake_cons(Value *car, Value *cdr)
{
   Value *v = ALLOC_VALUE();
   v->tag = cons;
   VCAR(v) = car;
   VCDR(v) = cdr;
   return v;
}

Value *
vmake_symbol(int length, char *data)
{
   Value *v = ALLOC_VALUE();
   v->tag = symbol;
   VSLENGTH(v) = length;
   VSDATA(v) = data;
   return v;
}

Value *
vmake_symbol_c(char *s)
{
   Value *v = ALLOC_VALUE();
   v->tag = symbol;
   VSLENGTH(v) = strlen(s);
   VSDATA(v) = s;
   return v;
}

Value *
vmake_string(int length, char *data)
{
   Value *v = ALLOC_VALUE();
   v->tag = string;
   VSLENGTH(v) = length;
   VSDATA(v) = data;
   return v;
}

Value *
vmake_string_c(char *s)
{
   Value *v = ALLOC_VALUE();
   v->tag = string;
   VSLENGTH(v) = strlen(s);
   VSDATA(v) = s;
   return v;
}

char *
vextract_string_c(Value *v)
{
   char *s = (char *) malloc(VSLENGTH(v) + 1);
   memcpy(s, VSDATA(v), VSLENGTH(v));
   s[VSLENGTH(v)] = '\0';
   return s;
}

Value *
vmake_integer(int n)
{
   Value *v = ALLOC_VALUE();
   v->tag = integer;
   VINTEGER(v) = n;
   return v;
}

Value *
vmake_var(enum Vtag tag, void **value)
{
   Value *v = ALLOC_VALUE();
   v->tag = var;
   VVTAG(v) = tag;
   VVDATA(v) = value;
   return v;
}

int
vlength(Value *l)
{
    int i;
    for (i=0; VTAG(l) == cons; i++, l = VCDR(l))
	;
    return i;
}

typedef struct {
   jmp_buf abort;		/* nonlocal exit for abort */

   char *input_string;		/* input string */
   int buflen;			/* amount left in input string */
   char *buf;			/* pointer into input */

   int strbuflen;		/* length of scratch buffer */
   char *strbuf;		/* scratch buffer for building strings */
} Globals;

Value *read_value(Globals *g);
Value *read_list(Globals *g);

#define PEEK_CHAR(g)	(*(g)->buf)
#define NEXT_CHAR(g)	((g)->buflen > 0 ? \
			 (void) ((g)->buf++,((g)->buflen--)) : \
			 (void) (ABORT(g, 23)))
#define ABORT(g, code)	longjmp((g)->abort, (code))

/* A pox on languages without coroutines. */
/* I don't feel like putting the entire state of the parser in data
 * structures that I can save and restore myself, so if EOF is
 * encountered while parsing the parser will have to start from
 * scratch when it gets more data */

void
expand_strbuf(Globals *g)
{
   if (g->strbuflen == 0) {
      g->strbuflen = 128;
      g->strbuf = (char *) malloc(g->strbuflen);
   }
   else {
      int newbuflen = 3 * g->strbuflen / 2;
      char *newbuf = (char *) malloc(newbuflen);
      memcpy(newbuf, g->strbuf, g->strbuflen);
      free(g->strbuf);
      g->strbuf = newbuf;
      g->strbuflen = newbuflen;
   }
}

int parse(int slen, char *s, Value **v)
{
   Globals g;
   int jmpret;

   if (0 == (jmpret = setjmp(g.abort))) {	/* successful parse */
      g.input_string = s;
      g.buflen = slen;
      g.buf = g.input_string;
      g.strbuflen = 0;
      g.strbuf = NULL;
      expand_strbuf(&g);
      *v = read_value(&g);
      return g.buf - g.input_string;
   }
   else {			/* return from nonlocal abort */
      free(g.strbuf);
      *v = NULL;
      return 0;
   }
}

int
read_escape(Globals *g, char *c)
{
   int valid = 1;
   int nc = PEEK_CHAR(g);

   switch (nc) {
    case '\n':
      valid = 0;
      NEXT_CHAR(g);
      break;
    case 'n':
      *c = '\n';
      NEXT_CHAR(g);
      break;
    case 't':
      *c = '\t';
      NEXT_CHAR(g);
      break;
    default:
      if (nc >= '0' && nc <= '7') {
	int digits;
	/* handle octal \nnn notation */
	*c = 0;
	for (digits = 0; digits < 3; digits++) {
	  if (nc < '0' || nc > '7')
	    break;
	  *c = (*c * 8) + (nc - '0');
	  NEXT_CHAR(g);
	  nc = PEEK_CHAR(g);
	}
      } else {
	/* backslash followed by some random char, like \q.
	 * (some of these are actually valid, but I don't think prin1 will
         * produce them, so it's not too critical). */
	*c = nc;
	NEXT_CHAR(g);
      }
      break;
   }
   return valid;
}

Value *
read_string(Globals *g)
{
   int strpos = 0;
   Value *v;
   char c;

#define ADD_CHAR(c)	\
   if (strpos >= g->strbuflen) \
      expand_strbuf(g);		\
   g->strbuf[strpos++] = (c)

   while (1) {
      switch (PEEK_CHAR(g)) {
       case '\"':
	 NEXT_CHAR(g);
	 v = ALLOC_VALUE();
	 v->tag = string;
	 v->value.s.length = strpos;
	 v->value.s.string = (char *) malloc(v->value.s.length);
	 memcpy(v->value.s.string, g->strbuf, v->value.s.length);
	 return v;
	 break;
       case '\\':
	 NEXT_CHAR(g);
	 if (read_escape(g, &c))
	    ADD_CHAR(c);
	 break;
       default:
	 ADD_CHAR(PEEK_CHAR(g));
	 NEXT_CHAR(g);
	 break;
      }
   }
}

/* characters
(
)
"
\
<white>
<character>
<number>
 */

Value *
read_num_or_symbol(Globals *g)
{
   Value *v;
   int strpos = 0;
   int i;
   int is_integer;

#define ADD_CHAR(c)	\
   if (strpos >= g->strbuflen) \
      expand_strbuf(g);		\
   g->strbuf[strpos++] = (c)

   while (g->buflen > 0) {
      switch (PEEK_CHAR(g)) {
       case ' ':
       case '\t':
       case '\n':
       case '\0':
       case '\"':
       case '(':
       case ')':
       case '.':
	 goto done;
	 break;
       case '\\':
	 NEXT_CHAR(g);
	 ADD_CHAR(PEEK_CHAR(g));
	 NEXT_CHAR(g);
	 break;
       default:
	 ADD_CHAR(PEEK_CHAR(g));
	 NEXT_CHAR(g);
	 break;
      }
   }
   ABORT(g, 23);

 done:
   /* is this a number or a symbol? */
   /* assume integer to start */
   is_integer = 1;

   /* assume no empty strings? */

   /* if the first character is '+' or '-' and that's not the only */
   /* character it can still be an integer */
   i = 0;
   if (strpos > 0) {
      if (g->strbuf[0] == '-' || g->strbuf[0] == '+') {
	 if (strpos > 1) {
	    i = 1;
	 } else {
	    is_integer = 0;
	 }
      }
   }

   while (is_integer && i < strpos) {
      if (g->strbuf[i] < '0' || g->strbuf[i] > '9')
	 is_integer = 0;
      i++;
   }

   if (is_integer) {
      /* it's an integer */
      v = ALLOC_VALUE();
      v->tag = integer;
      ADD_CHAR('\0');
      v->value.integer.i = atoi(g->strbuf);
   }
   else {
      /* it's a symbol */
      if (3 == strpos &&
	  !memcmp(g->strbuf, "nil", 3)) {
	 v = NULL;
      } else {
	 v = ALLOC_VALUE();
	 v->tag = symbol;
	 v->value.s.length = strpos;
	 v->value.s.string = (char *) malloc(v->value.s.length);
	 memcpy(v->value.s.string, g->strbuf, v->value.s.length);
      }
   }
   return v;
}

Value *
read_value(Globals *g)
{
   while (g->buflen > 0) {
      switch (PEEK_CHAR(g)) {
       case ' ':
       case '\t':
       case '\n':
       case '\0':
	 NEXT_CHAR(g);
	 break;
       case '\"':			/* begin string */
	 NEXT_CHAR(g);
	 return read_string(g);
	 break;
       case '(':
	 NEXT_CHAR(g);
	 return read_list(g);
	 break;
       case ')':
       case '.':
	 return NULL;
	 break;
       default:
	 return read_num_or_symbol(g);
	 break;
      }
   }
   ABORT(g, 23);
}

Value *
read_list(Globals *g)
{
   Value *list;
   Value **tail;
   Value *v;

   tail = &list;
   while (g->buflen > 0) {
      if (NULL == (v = read_value(g))) {
	 switch (PEEK_CHAR(g)) {

	  case ')':
	    if (tail != NULL) {		/* if no last cdr yet, use nil */
	       *tail = NULL;
	    }
	    NEXT_CHAR(g);
	    return list;
	    break;

	  case '.':			/* set last cdr explicitly */
	    NEXT_CHAR(g);
	    *tail = read_value(g);
	    if (*tail == NULL) {
	       /* badly formed input ??? */
	       ABORT(g, 13);
	    }
	    tail = NULL;
	    break;

	  default:
	    /* badly formed input ??? */
	    ABORT(g, 13);
	    break;
	 }
      }
      else {			/* read a value, add it to the list */
	 if (NULL == tail) {
	    /* two values after a . in a list.  very bad! ??? */
	    ABORT(g, 13);
	 }
	 *tail = ALLOC_VALUE();
	 (*tail)->tag = cons;
	 (*tail)->value.cons.car = v;
	 tail = &(*tail)->value.cons.cdr;
      }
   }
   ABORT(g, 23);	/* added this  -dkindred */
}

void free_value(Value *v)
{
   switch(VTAG(v)) {
    case cons:
      free_value(v->value.cons.car);
      free_value(v->value.cons.cdr);
      break;
    case string:
    case symbol:
      free(v->value.s.string);
      break;
    default:
      break;
   }
   free(v);
}

void prin(FILE *f, Value *v);

void
prinlis(FILE *f, Value *v, int first)
{
   switch(VTAG(v)) {
    case cons:				/* continue printing list */
      if (! first)
	 putc(' ', f);
      prin(f, v->value.cons.car);
      prinlis(f, v->value.cons.cdr, 0);
      break;
    case nil:				/* last elt in list */
      putc(')', f);
      break;
    default:				/* dotted pair */
      putc(' ', f);
      putc('.', f);
      putc(' ', f);
      prin(f, v);
      putc(')', f);
      break;
   }
}

void
prin(FILE *f, Value *v)
{
   switch (VTAG(v)) {
    case nil:
      fputs("\'()", f);
      break;
    case cons:
      putc('(', f);
      prinlis(f, v, 1);
      break;
    case string:
      /* ??? do quoting of '"' ??? */
      putc('\"', f);
      fwrite(v->value.s.string, 1, v->value.s.length, f);
      putc('\"', f);
      break;
    case symbol:
      /* ??? do quoting of all whitespace and special chars ??? */
      fwrite(v->value.s.string, 1, v->value.s.length, f);
      break;
    case integer:
      fprintf(f, "%ld", v->value.integer.i);
      break;
    default:
      fputs("#<huh?>", f);
      break;
   }
}

#define CHECK_TAG(v, t) if (VTAG(v) != (t)) return 0

int
eqv(Value *v1, Value *v2)
{

   switch (v1->tag) {
/*
    case any:
      return 1;
      break;
 */
    case nil:
      CHECK_TAG(v2, nil);
      return 1;
      break;
    case cons:
      CHECK_TAG(v2, cons);
      return (eqv(VCAR(v1), VCAR(v2)) &&
	      eqv(VCDR(v1), VCDR(v2)));
      break;
    case string:
      CHECK_TAG(v2, string);
      return (VSLENGTH(v1) == VSLENGTH(v2) &&
	      0 == memcmp(VSDATA(v1), VSDATA(v2), VSLENGTH(v1)));
      break;
    case symbol:
      CHECK_TAG(v2, symbol);
      return (VSLENGTH(v1) == VSLENGTH(v2) &&
	      0 == memcmp(VSDATA(v1), VSDATA(v2), VSLENGTH(v1)));
      break;
    case integer:
      CHECK_TAG(v2, integer);
      return (VINTEGER(v1) == VINTEGER(v2));
      break;
    case var:
      if (VVTAG(v1) != any)
	 CHECK_TAG(v2, VVTAG(v1));
      return 1;
      break;
    default:
      fprintf(stderr,"eqv(): bad tag: %d\n",(int)(v1->tag));
      /* die? */
      return 0;
      break;
   }
}

Value *
assqv(Value *key, Value *assoc)
{
   Value *pair;

   /* cdr on through */
   while (VTAG(assoc) == cons) {
      pair = VCAR(assoc);
      if (VTAG(pair) == cons && eqv(VCAR(pair), key)) {
	 return pair;
      }
      assoc = VCDR(assoc);
   }
   return NULL;
}

int
destructure(Value *pattern, Value *match)
{
   switch (VTAG(pattern)) {
    case any:
      return 1;
      break;
    case nil:
      CHECK_TAG(match, nil);
      return 1;
      break;
    case cons:
      CHECK_TAG(match, cons);
      return (destructure(VCAR(pattern), VCAR(match)) &&
	      destructure(VCDR(pattern), VCDR(match)));
      break;
    case string:
      CHECK_TAG(match, string);
      return (VSLENGTH(pattern) == VSLENGTH(match) &&
	      0 == memcmp(VSDATA(pattern), VSDATA(match), VSLENGTH(pattern)));
      break;
    case symbol:
      CHECK_TAG(match, symbol);
      return (VSLENGTH(pattern) == VSLENGTH(match) &&
	      0 == memcmp(VSDATA(pattern), VSDATA(match), VSLENGTH(pattern)));
      break;
    case integer:
      CHECK_TAG(match, integer);
      return (VINTEGER(pattern) == VINTEGER(match));
      break;
    case var:
      if (VVTAG(pattern) != any)
	 CHECK_TAG(match, VVTAG(pattern));
      if (VVDATA(pattern) != NULL)
	 *VVDATA(pattern) = (void *) match;
      return 1;
      break;
    default:
      fprintf(stderr,"destructure(): bad tag: %d\n",(int)VTAG(pattern));
      /* die? */
      return 0;
      break;
   }
}

#ifdef TEST

read_and_parse()
{
#define BUFLEN 512
   char buf[BUFLEN];	/* this will have to be dynamically expanded */
   int bufpos = 0;
   int ret;
   Value *v = NULL;
   Value *match_data;
   Value *pattern = vmake_cons(vmake_symbol_c("integer"),
			       vmake_var(integer, (void **) &match_data));

   while (1) {
      ret = read(0, buf + bufpos, BUFLEN - bufpos);
      if (ret < 0) {
	 perror("read");
	 exit(1);
      }
      else {
	 bufpos += ret;

	 do {
	    if (v != NULL) {
	       free_value(v);
	       v = NULL;
	    }
	    ret = parse(bufpos, buf, &v);
	    if (ret > 0) {
	       memmove(buf, buf + ret, bufpos - ret);
	       bufpos -= ret;
	       printf("parsed: ");
	       prin(stdout, v);
	       fputc('\n', stdout);

	       if (destructure(pattern, v)) {
		  printf("match_data = ");
		  prin(stdout, match_data);
		  fputc('\n', stdout);
	       }
	       else {
		  printf("destructure failed\n");
	       }

	       free_value(v);
	    }
	    else
	       printf("EOF\n");
	 } while (ret > 0);
      }
   }
}

main(int argc, char *argv[])
{
   read_and_parse();
#if 0
      Value *v;
      v = ALLOC_VALUE();

      v->tag = cons;
      v->value.cons.car = ALLOC_VALUE();
      v->value.cons.car->tag = symbol;
      v->value.cons.car->value.s.length = 6;
      v->value.cons.car->value.s.string = "symbol";

      v->value.cons.cdr = ALLOC_VALUE();
      v->value.cons.cdr->tag = cons;

      v->value.cons.cdr->value.cons.car = ALLOC_VALUE();
      v->value.cons.cdr->value.cons.car->tag = string;
      v->value.cons.cdr->value.cons.car->value.s.length = 6;
      v->value.cons.cdr->value.cons.car->value.s.string = "string";

      v->value.cons.cdr->value.cons.cdr = ALLOC_VALUE();
      v->value.cons.cdr->value.cons.cdr->tag = integer;
      v->value.cons.cdr->value.cons.cdr->value.integer.i = 23;
      prin(stdout, v);
      fputc('\n', stdout);
#endif
}
#endif
