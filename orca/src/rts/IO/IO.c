/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* This file contains some I/O routines used by orca programs.
   Temporary version, hack. ???
*/

/* $Id: IO.c,v 1.16 1998/01/21 10:58:26 ceriel Exp $ */

#include <interface.h>

int m_scan(FILE *, char *, void *);

FILE *curr_out;
FILE *curr_in;

static void IOerror(char *s)
{
	m_liberr("I/O error", s);
}

void f_InOut__WriteInt(t_integer i)
{
  char	buf[20];

  sprintf(buf, "%ld", (long) i);
  m_print(curr_out, buf, strlen(buf));
}

void f_InOut__WriteShortInt(t_integer i)
{
  char	buf[20];

  sprintf(buf, "%d", (int) i);
  m_print(curr_out, buf, strlen(buf));
}

void f_InOut__WriteLongInt(t_longint i)
{
  char	buf[20];

  sprintf(buf, "%ld", (long) i);
  m_print(curr_out, buf, strlen(buf));
}

void f_InOut__WriteReal(t_real r)
{
  char	buf[64];

  sprintf(buf, "%g", (double) r);
  m_print(curr_out, buf, strlen(buf));
}

void f_InOut__WriteLongReal(t_longreal r)
{
  char	buf[64];

  sprintf(buf, "%g", (double) r);
  m_print(curr_out, buf, strlen(buf));
}

void f_InOut__WriteChar(t_char c)
{
  char buf[2];

  buf[0] = c;
  m_print(curr_out, buf, 1);
}

void f_InOut__WriteString(t_string *string)
{
  if (string->a_sz <= 0) return;
  m_print(curr_out, &((char *)(string->a_data))[string->a_offset], string->a_sz);
}

void f_InOut__WriteLn(void) {
  char buf[2];

  buf[0] = '\n';
  m_print(curr_out, buf, 1);
}

void f_InOut__ReadChar(t_char *c)
{
  if (m_scan(curr_in, "%c", c) <= 0) IOerror("character expected");
}

void f_InOut__ReadInt(t_integer *i)
{
  int k;
  if (m_scan(curr_in, "%d", &k) <= 0) IOerror("integer expected");
  *i = k;
}

void f_InOut__ReadShortInt(t_shortint *i)
{
  int k;
  if (m_scan(curr_in, "%d", &k) <= 0) IOerror("integer expected");
  *i = k;
}

void f_InOut__ReadLongInt(t_longint *i)
{
  long k;
  if (m_scan(curr_in, "%ld", &k) <= 0) IOerror("integer expected");
  *i = k;
}

void f_InOut__ReadLongReal(t_longreal *i)
{
  double k;
  if (m_scan(curr_in, "%lf", &k) <= 0) IOerror("real expected");
  *i = k;
}

void f_InOut__ReadShortReal(t_shortreal *i)
{
  double k;
  if (m_scan(curr_in, "%lf", &k) <= 0) IOerror("real expected");
  *i = k;
}

void f_InOut__ReadReal(t_real *i)
{
  double k;
  if (m_scan(curr_in, "%lf", &k) <= 0) IOerror("real expected");
  *i = k;
}

void f_InOut__ReadString(t_string *s)
{
  char buf[2048];
  int len;

  buf[0] = 0;
  if (m_scan(curr_in, "%s", buf) <= 0) IOerror("string expected");
  len = strlen(buf);
  free_string(s);
  a_allocate(s, 1, 1, 1, len);
  strncpy(&((char *)(s->a_data))[s->a_offset], buf, s->a_sz);
}

t_boolean f_InOut__OpenOutputFile(t_string *string)
{
  char *fn = m_malloc(string->a_sz+1);
  FILE *f;

  fn[string->a_sz] = 0;
  strncpy(fn, &((char *)(string->a_data))[string->a_offset], string->a_sz);
  f = fopen(fn, "w");
  m_free(fn);
  if (f == NULL) return 0;
  if (curr_out != stdout) fclose(curr_out);
  curr_out = f;
  return 1;
}

t_boolean f_InOut__AppendOutputFile(t_string *string)
{
  char *fn = m_malloc(string->a_sz+1);
  FILE *f;

  fn[string->a_sz] = 0;
  strncpy(fn, &((char *)(string->a_data))[string->a_offset], string->a_sz);
  f = fopen(fn, "a");
  m_free(fn);
  if (f == NULL) return 0;
  if (curr_out != stdout) fclose(curr_out);
  curr_out = f;
  return 1;
}

t_boolean f_InOut__OpenInputFile(t_string *string)
{
  char *fn = m_malloc(string->a_sz+1);
  FILE *f;

  fn[string->a_sz] = 0;
  strncpy(fn, &((char *)(string->a_data))[string->a_offset], string->a_sz);
  f = fopen(fn, "r");
  m_free(fn);
  if (f == NULL) return 0;
  if (curr_in != stdin) fclose(curr_in);
  curr_in = f;
  return 1;
}

void
f_InOut__CloseOutput(void)
{
  if (curr_out != stdout) fclose(curr_out);
  curr_out = stdout;
}

void
f_InOut__CloseInput(void)
{
  if (curr_in != stdin) fclose(curr_in);
  curr_in = stdin;
}

t_char
f_InOut__Ahead(void)
{
  int c;

  c = getc(curr_in);
  if (c != EOF) ungetc(c, curr_in);
  return c;
}

t_boolean
f_InOut__Eof(void)
{
  (void) f_InOut__Ahead();
  return feof(curr_in) != 0  ? 1 : 0;	/* not return feof(curr_in) because
					   it is not guaranteed to be either
					   0 or 1.
					*/
}

t_boolean
f_InOut__Eoln(void)
{
  return f_InOut__Ahead() == '\n';
}

void
f_InOut__Flush(void)
{
  fflush(curr_out);
}

void
f_InOut__SetBuffering(int kind)
{
  switch(kind) {
  case 0:
	setvbuf(curr_out, (char *) 0, _IOFBF, (size_t) 0);
	break;
  case 1:
	setvbuf(curr_out, (char *) 0, _IOLBF, (size_t) 0);
	break;
  default:
	setvbuf(curr_out, (char *) 0, _IONBF, (size_t) 0);
	break;
  }
}

void ini_InOut__InOut(void) {
  static int done;

  if (! done) {
	curr_out = stdout;
	curr_in = stdin;
	done = 1;
  }
}
