#include <interface.h>
#include "Strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static char *
nmconv (t_array *name)
{
  register char *s = malloc(name->a_sz+1);
 
  strncpy(s, &((char *)(name->a_data))[name->a_offset], name->a_sz);
  s[name->a_sz] = 0;
  return s;
}
 
/*
t_array *
mnconv (char *s)
{
  t_array *res = (t_array *) malloc (sizeof (t_array));
  
  res->a_offset = 1;
  res->a_sz = strlen (s);
  res->a_data = (char *) malloc (strlen (s) + res->a_offset);
  strncpy (&(res->a_data[res->a_offset]), s, res->a_sz);

  return res;
}
*/


t_boolean
f_Strings__EmptyString (t_string  *str)
{
  char *s, *temp;
  int   empty;

  temp = s = nmconv (str);

  while (!isdigit (*s) && (*s != '\0') && (*s != '\t') && (*s != '\n') && (*s != ' '))
    {
      s++;
    }

  empty = (*s == '\0');
  free (temp);
  return empty;

}
    



char *
next_word (char *s)
{
  char   *buff = (char *) malloc (20);
  char   *res = buff;

  while ((*s != '\0') && (*s != '\t') && (*s != '\n') && (*s != ' '))
    {
      *buff = *s;
      buff++; s++;
    }

  *buff = '\0';

  return res;
}


t_integer
f_Strings__NextInteger (t_string  *s)
{
    char *temp1, *temp2;
    int   res;

    temp1 = nmconv (s);
    temp2 = next_word (temp1);
    res = atoi (temp2);
    free (temp1);
    free (temp2);
    
    return res;
}

void
f_Strings__ReduceString (t_string *str, t_string *res)
{
  char  *s;
  char *temp;

  temp = s = nmconv (str);
  
  while ((*s != '\0') && (*s != '\t') && (*s != '\n') && (*s != ' '))
  {
      s++;
  }
  while ((*s == '\t') || (*s == '\n') || (*s == ' '))
  {
      s++;
  }

/*   res->a_offset = 0; */
/*   res->a_sz = strlen (s); */
/*   res->a_data = &(str->a_data[s - temp + str->a_offset]); */
  str->a_offset += s - temp;
  str->a_sz -= s - temp;
  free (temp);

}




/* void */
/* f_Strings__AppendChar (t_string *str, t_char c, t_string *res) */
/* { */
/*     char *temp; */
/*     char *buff_p; */
/*     char *free_mem; */

/*     free_mem = temp = nmconv (str); */
/*     res->a_data = buff_p = (char *) malloc (sizeof (char) * (strlen (temp) + 2)); */
/*     res->a_offset = 0; */
/*     res->a_sz = strlen (temp) + 1; */
/*     while (*temp != '\0') */
/* 	*buff_p++ = *temp++; */
/*     *buff_p++ = c; */
/*     *buff_p = '\n'; */

/*     free (free_mem); */
/*     free (str->a_data);  */

/* }     */
    
