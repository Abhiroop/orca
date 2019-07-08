#ifndef _SYS_ASM_
#define _SYS_ASM_


#ifdef __GNUC__
__inline__ static void
flush_to_memory(void)
{
	__asm__ volatile("" : : : "memory");
}

#define fast_asm_ldstub(ptr, val) \
        __asm__ volatile("ldstub [%1], %0" : "=r" (val) : "r" (ptr) : "memory")

#else

#define flush_to_memory()		/* nothing, hope for the best */
#define fast_asm_ldstub(flag, val)	((val) = *(flag), *(flag) = 1)

#endif
 

#ifdef __GNUC__
__inline__
#endif
static int
tas( int *flag)
{
    register int val;
 
    fast_asm_ldstub(flag, val);
    return val;
}

#endif /* _SYS_ASM_ */
