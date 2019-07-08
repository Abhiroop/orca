extern void mm_reset_stats(void);
extern void mm_print_stats(void);

#define f_RtsStats__reset_stats()	mm_reset_stats()
#define f_RtsStats__print_stats()	mm_print_stats()
#define ini_RtsStats__RtsStats()
