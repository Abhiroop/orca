/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: unix.c,v 1.1 1998/06/11 12:00:58 ceriel Exp $ */

/*
MODULE SPECIFICATION unix;
	TYPE iobuf = string;
	FUNCTION open(name: string; mode: integer): integer;
	FUNCTION creat(name: string; mode: integer): integer;
	FUNCTION read(	fildes: integer;
			buffer: OUT iobuf;
			nbytes: integer) : integer;
	FUNCTION write(	fildes: integer;
			buffer: iobuf;
			nbytes: integer): integer;
	FUNCTION close(fildes: integer): integer;
	FUNCTION getpid(): integer;
	FUNCTION getuid(): integer;
	FUNCTION geteuid(): integer;
	FUNCTION getgid(): integer;
	FUNCTION getegid(): integer;
	FUNCTION link(name1, name2: string): integer;
	FUNCTION lseek(fildes, offset, whence: integer): integer;
	FUNCTION unlink(name: string): integer;

  FUNCTION WriteChar(fildes: integer; c : char);
  FUNCTION WriteInt(fildes: integer; i : integer);
  FUNCTION WriteReal(fildes: integer; r : real);
  FUNCTION WriteLn(fildes: integer; );
  FUNCTION WriteSpaces(fildes: integer; n : integer);
  FUNCTION WriteString(fildes: integer; str: string);

  FUNCTION ReadChar(fildes: integer; ): char;
  FUNCTION ReadInt(fildes: integer; ): integer;
  FUNCTION ReadReal(fildes: integer; ): real;
  FUNCTION ReadString(fildes: integer; ): string;

END;
*/

#include <unistd.h>
#include <fcntl.h>
#include <interface.h>
#include "unix.h"

static char *
nmconv(t_array *name)
{
  register char *s = m_malloc(name->a_sz+1);

  strncpy(s, &((char *)(name->a_data))[name->a_offset], name->a_sz);
  s[name->a_sz] = 0;
  return s;
}

int
f_unix__open(t_array *name, int mode)
{
  char *s = nmconv(name);
  int r = open(s, mode);
  m_free(s);
  return r;
}

int
f_unix__creat(t_array *name, int mode)
{
  char *s = nmconv(name);
  int r = creat(s, mode);
  m_free(s);
  return r;
}

extern tp_dscr td_string;

int
f_unix__read(int fildes, t_array *buffer, int nbytes)
{
  if (buffer->a_sz != nbytes) {
	if (buffer->a_sz > 0) free_string(buffer);
	else buffer->a_offset = 1;
	a_allocate(buffer, 1, sizeof(t_char), buffer->a_offset, buffer->a_offset+nbytes-1);
  }
  return read(fildes, &((char *)(buffer->a_data))[buffer->a_offset], nbytes);
}

int
f_unix__write(int fildes, t_array *buffer, int nbytes)
{
  return write(fildes, &((char *)(buffer->a_data))[buffer->a_offset], nbytes > buffer->a_sz ? buffer->a_sz : nbytes);
}

int
f_unix__close(int fildes)
{
  return close(fildes);
}

#ifndef AMOEBA
int
f_unix__getpid(void)
{
  return getpid();
}

int
f_unix__getuid(void)
{
  return getuid();
}

int
f_unix__geteuid(void)
{
  return geteuid();
}

#ifndef PARIX
int
f_unix__getgid(void)
{
  return getgid();
}

int
f_unix__getegid(void)
{
  return getegid();
}

#endif
#endif

int
f_unix__link(t_array *name1, t_array *name2)
{
  char *s1 = nmconv(name1), *s2 = nmconv(name2);
  int r = link(s1, s2);
  m_free(s1);
  m_free(s2);
  return r;
}

int
f_unix__lseek(int fildes, int offset, int whence)
{
  return lseek(fildes, (long) offset, whence);
}

int
f_unix__unlink(t_array *name)
{
  char *s = nmconv(name);
  int r = unlink(s);
  m_free(s);
  return r;
}


/*** formattted file output  ***/


void
f_unix__WriteInt(int fildes, int i)
{
  char s[50];
  int n, nbytes;

  sprintf(s, "%d", i);
  nbytes = strlen(s);
  n = write(fildes, s, nbytes);
  assert (n == nbytes);
}

void
f_unix__WriteReal(int fildes, t_real r)
{
  char s[50];
  int nbytes, n;

  sprintf(s, "%f", (double) r);
  nbytes = strlen(s);
  n = write(fildes, s, nbytes);
  assert (n == nbytes);
}

void
f_unix__WriteChar(int fildes, t_char c)
{
  int n;

  n = write(fildes, &c, 1);
  assert (n == 1);
}

void
f_unix__WriteString(int fildes, t_array *string)
{
  int n = write(fildes, string, string->a_sz);
  assert(n == string->a_sz);
}


void
f_unix__WriteLn(int fildes)
{
  char c = '\n';
  int n = write(fildes, &c, 1);
  assert(n == 1);
}

void
f_unix__WriteSpaces(int fildes, int nr)
{
  int i, n;
  char c = ' ';

  for (i = 0; i < nr; i++) {
	n = write(fildes, &c, 1);
	assert(n == 1);
  }
}

static int
readstring(int fildes, char buf[], int bufsiz)
{
  int i,n;
  char c;

  for (;;) {
	n = read(fildes, &c, 1);
	if (n != 1) {
	    c = ' ';
	}
	if (c != ' ' && c != '\n') break;
  }
  i = 0;
  while (c != ' ' && c != '\n') {
	assert(i < bufsiz);
	buf[i] = c;
	i++;
	n = read(fildes, &c, 1);
	if (n != 1) {
	    c = ' ';
	}
  }
  buf[i] = '\0';
  return i;
}


int
f_unix__ReadInt(int fildes)
{
  char buf[100];
  int r;

  readstring(fildes, buf, 100);
  sscanf(buf, "%d", &r);
  return r;
}

t_real
f_unix__ReadReal(int fildes)
{
  char buf[100];
  double r;

  readstring(fildes, buf, 100);
  sscanf(buf, "%lf", &r);
  return r;
}

t_char
f_unix__ReadChar(int fildes)
{
  char c;

  read(fildes, &c, 1);
  return c;
}

void
f_unix__ReadString(int fildes, t_array *res)
{
    int n;
    char buf[1000];
    register char *p = buf, *q;

    n = readstring(fildes, buf, 1000);
    free_string(res);
    a_allocate(res, 1, sizeof(t_char), 1, n);
    q = &((char *)(res->a_data))[res->a_offset];
    while (n > 0) {
	*q++ = *p++;
	n--;
    }
}
