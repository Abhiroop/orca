/* LogModuleSrc.c
 * 
 * This file contains the external C-module written to take care of
 * messages created by Orca programs. The functions print messages
 * to stdout and to statically defined files. These files are:
 * "Errors" and "Warnings" for the respective messages.
 *
 */

#include <interface.h>
#include "LogModule.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


 
static char *
nmconv (t_array *name)
{
  register char *s = malloc(name->a_sz+1);
 
  strncpy(s, &((char *)(name->a_data))[name->a_offset], name->a_sz);
  s[name->a_sz] = 0;
  return s;
}
 


/* OrcaToC
 *
 * OrcaToC takes a Orca string and convertes it into a normal
 * NULL terminated C style character pointer. It allocates
 * memory for this string and returns a pointer to this
 * memory. 
 *
 *
 */
char *
OrcaToC (t_string   *s)
{
    return nmconv (s);
}




/* fmessage
 *
 * fmessage prints a message to a file. The file must be open
 * and if it is not, an error will occur. 
 * A message has four parts:
 *    Type    : [MESSAGE | ERROR | WARNING]
 *    CPU     : integer
 *    Time    : HH.MM.SS    (Current time)
 *    Message : The actual message 
 *
 * An example message is:
 * WARNING: CPU 0: [17.14.55] Out of memory
 */
void
fmessage (FILE   *fp, t_char *type, int   cpu, t_char  *m)
{
    time_t     *t;
    struct tm  *tp;
    char       *s;

    t = (time_t *) malloc (sizeof(time_t));
    time (t);
    tp = localtime (t);
    s = (char *) malloc (sizeof (char) * 22);

    strftime (s, 21, "%Y-%m-%d %H:%M:%S", tp);

    fprintf (fp, "%s: CPU %d: [%s] %s\n", type, cpu, s, m);
    free (s);
    free (t);
}



/* message
 *
 * message prints a message on stdout using standard format
 *
 */
void
message (t_char *type, int   cpu, t_char  *m)
{
    fmessage (stdout, type, cpu, m);
}



/* f_LogModule__LogMessage
 *
 * f_LogModule__LogMessage is the interface towards Orca for
 * printing messages. The message will be printed in standard
 * format.
 */
void
f_LogModule__LogMessage (t_string  *m, t_integer cpu)
{
    message ("MESSAGE", cpu, OrcaToC (m));
}



/* f_LogModule__LogMessageInt
 *
 * f_LogModule__LogMessageInt is the interface towards Orca for
 * printing messages with an integer following. The message will 
 * be printed in standard format.
 */
void
f_LogModule__LogMessageInt (t_string  *m, t_integer  i, t_integer cpu)
{
    char   *s;

    s = (char *) malloc (sizeof (char) * (strlen (OrcaToC (m)) + 15));
    sprintf (s, "%s %d", OrcaToC (m), i);
    message ("MESSAGE", cpu, s);
    free (s);
}



/* f_LogModule__LogMessageReal
 *
 * f_LogModule__LogMessage is the interface towards Orca for
 * printing messages with a real following. The message will 
 * be printed in standard format.
 */
void
f_LogModule__LogMessageReal (t_string  *m, t_real  r, t_integer cpu)
{
    char   *s;

    s = (char *) malloc (sizeof (char) * (strlen (OrcaToC (m)) + 20));
    sprintf (s, "%s %.4f", OrcaToC (m), r);
    message ("MESSAGE", cpu, s);
    free (s);
}



/* f_LogModule__LogWarning
 *
 * f_LogModule__LogWarning is the interface towards Orca for
 * printing warnings. The warning will be printed in standard
 * format to stdout and appended to the file Warnings.
 */
void
f_LogModule__LogWarning (t_string  *m, t_integer cpu)
{
    FILE   *fp;

    printf ("\007");
    message ("WARNING", cpu, OrcaToC (m));
    if ((fp = fopen ("Warnings", "a")) != NULL) 
    {
	fmessage (fp, "WARNING", cpu, OrcaToC(m));
	fclose (fp);
    }
}



/* f_LogModule__LogError
 *
 * f_LogModule__LogError is the interface towards Orca for
 * printing Errors. The Error will be printed in standard
 * format to stdout and appended to the file Errors.
 */
void
f_LogModule__LogError (t_string  *m, t_integer cpu)
{
    FILE   *fp;

    printf ("\007");
    message ("ERROR", cpu,  OrcaToC (m));
    if ((fp = fopen ("Errors", "a")) != NULL) 
    {
	fmessage (fp, "ERROR", cpu, OrcaToC(m));
	fclose (fp);
    }
}




void
LogStoreMessageOn (char   *m, char   *f, t_integer cpu)
{
    FILE *fp;

    if ((fp = fopen (f, "a")) != NULL)
    {
	fmessage (fp, "LOG", cpu, m);
	fclose (fp);
    }
}



/* f_LogModule__LogStoreMessageOn
 *
 * f_LogModule__StoreMessageOn is the interface towards Orca for
 * printing messages to any file. The message 
 * will be appended in standard format to the file specified.
 */
void
f_LogModule__LogStoreMessageOn (t_string   *m, t_string   *f, t_integer cpu)
{
    LogStoreMessageOn (OrcaToC (m), OrcaToC (f), cpu);
}



/* f_LogModule__LogStoreMessage
 *
 * f_LogModule__StoreMessage is the interface towards Orca for
 * printing messages to a special repository file. The message 
 * will be appended in standard format to the file repository.
 */
void
f_LogModule__LogStoreMessage (t_string   *m, t_integer cpu)
{
    LogStoreMessageOn (OrcaToC (m), "repository", cpu);
}




/* f_LogModule__LogStatusReport
 *
 * f_LogModule__LogStatusReport prints a customized status report
 * to stdout. The purpose is that this will be displayed continously
 * during execution.
 */
void
f_LogModule__LogStatusReport  (t_string   *prod_stat,
			       t_string   *eval_stat,
			       t_integer   no_of_jobs,
			       t_integer   improved_jobs,
			       t_integer   no_of_solved,
			       t_integer   best_value)
{
    message ("STATUS REPORT", 0, "");
    printf ("   Producer status:      %s\n", OrcaToC (prod_stat));
    printf ("   Evaluator status:     %s\n", OrcaToC (eval_stat));
    printf ("   No of jobs produced:  %d\n", no_of_jobs);
    printf ("   No of jobs improved:  %d\n", improved_jobs);
    printf ("   Evaluated:            %d\n", no_of_solved);
    printf ("   Best value so far:    %d\n", best_value); 

}
			  






void
f_LogModule__Flush ()
{
    fflush (stdout);
}




