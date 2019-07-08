/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifdef INLINE
#undef INLINE
#undef OUTLINE
#endif

#ifdef INLINE_FUNCTIONS

#ifdef __GNUC__
#define INLINE	static __inline__
#else
#define INLINE	static
#endif

#else

#define INLINE
#define OUTLINE

#endif
