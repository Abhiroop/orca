#include <interface.h>
#include "HashFunction.h"

#include <string.h>
#include <math.h>

static char *
nmconv (t_array *name)
{
  register char *s = malloc(name->a_sz+1);
 
  strncpy(s, &((char *)(name->a_data))[name->a_offset], name->a_sz);
  s[name->a_sz] = 0;
  return s;
}


static unsigned int
hash (char *str, register int len)
{
  char   *s;
  int     res;
  char   *map = "MattiasForsberg";
  char   *map_ptr;

  map_ptr = map;
  s = str;
  res = 0;

  while (strcmp (s, "") != 0)
  {
      if (map_ptr == '\0')
	  map_ptr = map;

      res = (res + (*s * (*map_ptr - '0'))) % 100000;
      s++;
      map_ptr++;
  }

  return abs (res);
}
 


t_integer
f_HashFunction__Hash (t_string   *s, t_integer  i)
{
  char *temp;
  t_integer   res;

  temp = nmconv (s);
  res = hash (temp, i);
  free (temp);
  return res;
}

