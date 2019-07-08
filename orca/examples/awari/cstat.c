#include <interface.h>
#include "stat.h"

static int goedelcnt[128];

void f_stat__IncGoedelcnt(t_integer v_cpu) {
	goedelcnt[v_cpu]++;
}

void f_stat__PrintStat(t_integer v_cpu) {
	printf("cpu: %d, goedelcnt = %d\n", v_cpu, goedelcnt[v_cpu]);
}
