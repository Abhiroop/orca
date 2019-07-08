#include <interface.h>
#include <time.h>

extern FILE	*curr_out;

extern char compilation_date[];
extern char Orca_compilation_command[];
extern char C_compilation_command[];
extern char link_command[];

void
f_Time__PrintCompilationInfo(void)
{
  m_print(curr_out, "***********************************************************\n", 60);
  m_print(curr_out, "Compilation date: ", 18);
  m_print(curr_out, compilation_date, strlen(compilation_date));
  m_print(curr_out, "\nOrca compilation command: ", 27);
  m_print(curr_out, Orca_compilation_command, strlen(Orca_compilation_command));
  m_print(curr_out, "\nC compilation command: ", 24);
  m_print(curr_out, C_compilation_command, strlen(C_compilation_command));
  m_print(curr_out, "\nLink command: ", 15);
  m_print(curr_out, link_command, strlen(link_command));
  m_print(curr_out, "\n", 1);
  m_print(curr_out, "***********************************************************\n", 60);
}

void
f_Time__PrintTime(t_string *a, t_integer delta)
{
  char	buf[512];
  time_t tm = time((time_t *) 0);

  sprintf(buf, "Date: %.24s", ctime(&tm));
  m_print(curr_out, buf, strlen(buf));
  m_print(curr_out, "; Application: ", 15);
  if (a->a_sz > 0) {
    m_print(curr_out, &((char *)(a->a_data))[a->a_offset], a->a_sz);
  }
  m_print(curr_out, "; #CPUs: ", 9);
  sprintf(buf, "%d", (int) m_ncpus());
  m_print(curr_out, buf, strlen(buf));
  m_print(curr_out, "; Time: ", 8);
  sprintf(buf, "%.3f sec.\n\n", (double) delta/1000.0);
  m_print(curr_out, buf, strlen(buf));
  f_Time__PrintCompilationInfo();
}
