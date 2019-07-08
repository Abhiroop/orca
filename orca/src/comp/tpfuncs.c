/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* $Id: tpfuncs.c,v 1.9 1998/05/15 10:31:03 ceriel Exp $ */

#include	<stdio.h>
#include	<assert.h>
#include	"ansi.h"
#include	"def.h"
#include	"type.h"
#include	"main.h"
#include	"generate.h"
#include	"scope.h"
#include	"error.h"

/*
   Generation of Orca-type specific functions:
   - comparison
	For every Orca type for which comparison is allowed and for which
	no equivalent in C exists, a routine is generated with the following
	interface:
		int <cmpfunc>(void *a, void *b);
	The function returns 1 if the values are equal, 0 if not equal.
   - assignment
	For every constructed Orca type a routine is generated with the
	following interface:
		void <assignfunc>(void *dst, void *src);
   - free-ing
	For every constructed Orca type a routine is generated with the
	following interface:
		void <freefunc>(void *dst);
*/

_PROTOTYPE(static void gen_array_cmp, (t_type *tp));
_PROTOTYPE(static void gen_record_cmp, (t_type *tp));
_PROTOTYPE(static void gen_union_cmp, (t_type *tp));
_PROTOTYPE(static void gen_set_cmp, (t_type *tp));
_PROTOTYPE(static void gen_bag_cmp, (t_type *tp));
_PROTOTYPE(static void gen_array_assign, (t_type *tp));
_PROTOTYPE(static void gen_fixedarray_assign, (t_type *tp));
_PROTOTYPE(static void gen_record_assign, (t_type *tp));
_PROTOTYPE(static void gen_union_assign, (t_type *tp));
_PROTOTYPE(static void gen_set_assign, (t_type *tp));
_PROTOTYPE(static void gen_bag_assign, (t_type *tp));
_PROTOTYPE(static void gen_graph_assign, (t_type *tp));
_PROTOTYPE(static void gen_object_assign, (t_type *tp));
_PROTOTYPE(static void gen_array_free, (t_type *tp));
_PROTOTYPE(static void gen_record_free, (t_type *tp));
_PROTOTYPE(static void gen_union_free, (t_type *tp));
_PROTOTYPE(static void gen_set_free, (t_type *tp));
_PROTOTYPE(static void gen_bag_free, (t_type *tp));
_PROTOTYPE(static void gen_graph_free, (t_type *tp));
_PROTOTYPE(static void gen_object_free, (t_type *tp));

#define gen_generic_dynamic_if(tp) \
	do { \
		if ((tp)->tp_fund == T_GENERIC) { \
			fprintf(fc, "#if dynamic_%s\n", (tp)->tp_tpdef); \
		} \
	} while (0)

#define gen_generic_endif(tp) \
	do { \
		if ((tp)->tp_fund == T_GENERIC) { \
			fputs("#endif\n", fc); \
		} \
	} while (0)

void
gen_compare_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	/* For simple types and nodenames no comparison functions are generated.
	*/
	if (tp->tp_fund & (T_ISSIMPLEARG|T_NODENAME)) {
	    return;
	}

	/* For types for which comparison is not allowed, no comparison
	   functions are generated.
	*/
	if (tp->tp_flags & T_NOEQ) return;

	/* Interface is identical for all types, accepting void pointers. */
	if (exported) {
		/* Produce prototype in include file. */
	    fprintf(fh,	"int %s(void *a, void *b);\n",
			tp->tp_comparefunc);
	}
	fprintf(fc,	"%sint %s(void *aa, void *bb) {\n"
			"    %s *a=aa; %s *b=bb;\n",
	    ! exported ? "static " : "",
	    tp->tp_comparefunc,
	    tp->tp_tpdef,
	    tp->tp_tpdef);

	switch(tp->tp_fund) {
	case T_ARRAY:
		gen_array_cmp(tp);
		break;
	case T_RECORD:
		gen_record_cmp(tp);
		break;
	case T_UNION:
		gen_union_cmp(tp);
		break;
	case T_SET:
		gen_set_cmp(tp);
		break;
	case T_BAG:
		gen_bag_cmp(tp);
		break;
	default:
		crash("gen_compare_func");
		break;
	}
	fputs("}\n", fc);
}

static void
gen_array_cmp(tp)
	t_type	*tp;
{
	t_type	*t = element_type_of(tp);
	int	i;

	if (! (tp->tp_flags & T_CONSTBNDS)) {
	    fputs(	"    size_t off;\n", fc);
	}

	if (! (t->tp_fund & (T_ISSIMPLEARG|T_NODENAME))) {
	    /* Need some help variables for constructed types. */
	    fprintf(fc,	"    %s *p, *q;\n    int i;\n", t->tp_tpdef);
	}

	/* Compare addresses. */
	fputs(		"    if (a == b) return 1;\n", fc);

	if (! (tp->tp_flags & T_CONSTBNDS)) {
	    /* Check sizes. */
	    fputs(	"    if (a->a_sz <= 0) return b->a_sz <= 0;\n", fc);

	    /* Check bounds. */
	    for (i = 0; i < tp->arr_ndim; i++) {
		fprintf(fc,
			"    if (a_lb(a, %d) != a_lb(b, %d)) return 0;\n"
			"    if (a_ne(a, %d) != a_ne(b, %d)) return 0;\n",
		    i, i, i, i);
	    }
	    fprintf(fc,	"    off = a->a_offset * sizeof(%s);\n",
		t->tp_tpdef);
	}

	/* Compare elements, if simple use a memcmp for the whole
	   array, if not, compare one by one.
	*/

	if (t->tp_fund & (T_ISSIMPLEARG|T_NODENAME)) {
	    if (tp->tp_flags & T_CONSTBNDS) {
		fputs(	"    return memcmp(a, b, sizeof(*a)) == 0;\n", fc);
	    }
	    else {
		fprintf(fc,
			"    return memcmp((char *)(a->a_data) + off, (char *) (b->a_data) + off, sizeof(%s) * a->a_sz) == 0;\n",
		    t->tp_tpdef);
		}
	}
	else {
	    if (! (tp->tp_flags & T_CONSTBNDS)) {
		fputs(	"    p = (void *)((char *) (a->a_data) + off);\n"
			"    q = (void *)((char *) (b->a_data) + off);\n"
			"    for (i = a->a_sz; i; p++, q++, i--) {\n", fc);
	    }
	    else {
		fprintf(fc,
			"    p = a->a_data;\n"
			"    q = b->a_data;\n"
			"    for (i = %d; i; p++, q++, i--) {\n",
		    tp->arr_size);
	    }
	    fprintf(fc,	"        if (! %s(p, q)) return 0;\n"
	    		"    }\n"
			"    return 1;\n",
		t->tp_comparefunc);
	}
}

static void
gen_record_cmp(tp)
	t_type	*tp;
{
	t_def *deflist = tp->rec_scope->sc_def;

	/* Just compare element by element. */
	while (deflist) {
	    if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
		t_type *t = deflist->df_type;
		if (deflist->df_kind == D_OFIELD &&
		    (deflist->df_flags & (D_UPPER_BOUND|D_LOWER_BOUND))) {
		}
		else if (t->tp_fund & T_ISSIMPLEARG) {
		    fprintf(fc,
			"    if (a->%s != b->%s) return 0;\n",
					deflist->df_name,
					deflist->df_name);
		}
		else {
		    fprintf(fc,
			"    if (! %s(&(a->%s), &(b->%s))) return 0;\n",
					t->tp_comparefunc,
					deflist->df_name,
					deflist->df_name);
		}
	    }
	    deflist = deflist->df_nextinscope;
	}
	fputs(		"    return 1;\n", fc);
}

static void
gen_union_cmp(tp)
	t_type	*tp;
{
	t_def	*deflist = tp->rec_scope->sc_def;
	t_type	*t = element_type_of(tp);

	/* Make sure both unions are initialized. */
	fputs(		"    if (! a->u_init && ! b->u_init) return 1;\n"
			"    if (! a->u_init || ! b->u_init) return 0;\n", fc);

	/* Compare tag field. */
	fprintf(fc,	"    if (a->f_%s != b->f_%s) return 0;\n",
	    deflist->df_idf->id_text,
	    deflist->df_idf->id_text);

	/* Switch on tag field to determine variant, compare variant. */
	fprintf(fc,	"    switch(a->f_%s) {\n", deflist->df_idf->id_text);
	while ((deflist = deflist->df_nextinscope)) {
	    fprintf(fc,	"    case %ld:\n",
		deflist->fld_tagvalue->nd_int);
	    t = deflist->df_type;
	    if (t->tp_fund & T_ISSIMPLEARG) {
		fprintf(fc,
			"	return a->u_el.%s == b->u_el.%s;\n",
		    deflist->df_name,
		    deflist->df_name);
	    }
	    else {
		fprintf(fc,
			"	return %s(&(a->u_el.%s), &(b->u_el.%s));\n",
		    t->tp_comparefunc,
		    deflist->df_name,
		    deflist->df_name);
	    }
	}
	fputs(		"    }\n"
			"    return 0;\n", fc);
}

static void
gen_set_cmp(tp)
	t_type	*tp;
{
	/* Set comparison is done by checking that each element in one set
	   has a counterpart in the other.
	*/
	t_type	*t = element_type_of(tp);

	fputs(		"    t_elem *ae;\n", fc);

	/* Compare number of elements. */
	fputs(		"    if (a->s_nelem != b->s_nelem) return 0;\n", fc);

	/* Walk through one set, searching for each element in the other set. */
	fprintf(fc,	"    ae = a->s_elem;\n"
			"    while (ae) {\n"
			"        int i;\n"
			"        for (i = 0; i < MAXELC; i++) {\n"
			"            if (ae->e_mask & (1 << i)) {\n"
			"                if (! s_member(b, &%s, ae->e_buf+i*sizeof(%s))) return 0;\n"
			"            }\n"
			"        }\n"
			"        ae = ae->e_next;\n"
			"    }\n"
			"    return 1;\n",
	  tp->tp_descr,
	  t->tp_tpdef);
}

static void
gen_bag_cmp(tp)
	t_type	*tp;
{
	/* Bag comparison is done by making a copy of one of the bags
	   and for every element in the other bag checking that it is
	   present in the copy, and then removing it from the copy.
	*/
	t_type	*t = element_type_of(tp);

	fputs(		"    t_elem *ae;\n"
			"    t_bag bcopy;\n"
			"    int i;\n"
			"    memset(&bcopy, '\\0', sizeof(t_bag));\n"

	/* Compare number of elements. */
			"    if (a->s_nelem != b->s_nelem) return 0;\n", fc);

	/* Copy bag. */
	fprintf(fc,	"    %s(&bcopy, b);\n", tp->tp_assignfunc);

	/* Walk through the elements of the other bag. */
	fprintf(fc,	"    ae = a->s_elem;\n"
			"    while (ae) {\n"
			"        int i;\n"
			"        for (i = 0; i < MAXELC; i++) {\n"
			"            if ((ae->e_mask & (1 << i)) &&\n"
	      		"                ! b_delel(&bcopy, &%s, ae->e_buf+i*sizeof(%s))) {\n"
			"                %s(&bcopy);\n"
			"                return 0;\n"
			"            }\n"
			"        }\n"
			"        ae = ae->e_next;\n"
			"    }\n"
			"    %s(&bcopy);\n"
			"    return 1;\n",
	  tp->tp_descr,
	  t->tp_tpdef,
	  tp->tp_freefunc,
	  tp->tp_freefunc);
}

void
gen_assign_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	/* No special assignment functions for simple types or nodenames. */
	if (tp->tp_fund & (T_ISSIMPLEARG|T_NODENAME)) return;

	/* All assignment functions have the same interface, through void
	   pointers.
	*/
	if (exported) {
		/* Produce prototype in include file. */
	    fprintf(fh,	"void %s(void *a, void *b);\n",
		tp->tp_assignfunc);
	}
	fprintf(fc,	"%svoid %s(void *dd, void *ss) {\n"
			"    %s *dst = dd, *src = ss;\n",
	    ! exported ? "static " : "",
	    tp->tp_assignfunc,
	    tp->tp_tpdef);

	switch(tp->tp_fund) {
	case T_ARRAY:
		gen_array_assign(tp);
		break;
	case T_RECORD:
		gen_record_assign(tp);
		break;
	case T_UNION:
		gen_union_assign(tp);
		break;
	case T_SET:
		gen_set_assign(tp);
		break;
	case T_BAG:
		gen_bag_assign(tp);
		break;
	case T_GRAPH:
		gen_graph_assign(tp);
		break;
	case T_OBJECT:
		gen_object_assign(tp);
		break;
	default:
		crash("gen_assign_func");
		break;
	}
	fputs("}\n", fc);
}

static void
gen_fixedarray_assign(tp)
	t_type	*tp;
{
	t_type	*t = element_type_of(tp);

	fprintf(fc,	"    %s *p = dst->a_data;\n"
			"    %s *q = src->a_data;\n\n"
			"    if (dst == src) return;\n",
	    t->tp_tpdef,
	    t->tp_tpdef);

	if (t->tp_flags & T_DYNAMIC) {
	    gen_generic_dynamic_if(t);
	    fprintf(fc,	"    memset(p, '\\0', sizeof(*dst));\n"
	    		"    for (i=%d; i>0; i--, p++, q++) {\n"
	    		"	%s(p, q);\n"
	    		"    }\n",
		tp->arr_size,
		t->tp_assignfunc);
	    if (t->tp_fund == T_GENERIC) {
		fputs(	"#else\n"
			"    memcpy(p, q, sizeof(*dst));\n"
			"#endif\n", fc);
	    }
	}
	else {
	    fputs(	"    memcpy(p, q, sizeof(*dst));\n", fc);
	}
}

static void
gen_array_assign(tp)
	t_type	*tp;
{
	t_type	*t = element_type_of(tp);
	int	i;

	if (t->tp_flags & T_DYNAMIC) {
		gen_generic_dynamic_if(t);
		fputs(	"    int	i;\n", fc);
		gen_generic_endif(t);
	}
	if (tp->tp_flags & T_CONSTBNDS) {
		gen_fixedarray_assign(tp);
		return;
	}
	fprintf(fc,	"    size_t off = sizeof(%s) * src->a_offset;\n"
			"    %s *p,*q = (void *)((char *)(src->a_data)+off);\n"
			"    size_t sz = src->a_sz * sizeof(%s);\n\n"
			"    if (dst == src) return;\n"
			"    if (dst->a_sz != src->a_sz) {\n"
			"	void *m;\n"
			"	if (dst->a_sz > 0) %s(dst);\n"
			"	dst->a_sz = src->a_sz;\n"
			"	dst->a_offset = src->a_offset;\n",
	    t->tp_tpdef,
	    t->tp_tpdef,
	    t->tp_tpdef,
	    tp->tp_freefunc);

	for (i = 0; i < tp->arr_ndim; i++) {
	    fprintf(fc,	"	dst->a_dims[%d] = src->a_dims[%d];\n", i, i);
	}

	fputs(		"	if (src->a_sz <= 0) return;\n"
			"	m = m_malloc(sz);\n", fc);

	if (t->tp_flags & T_DYNAMIC) {
	    fputs(	"	memset(m, '\\0', sz);\n", fc);
	}

	fputs(		"	dst->a_data = (char *)m - off;\n"
			"    } else {\n", fc);

	for (i = 0; i < tp->arr_ndim; i++) {
	    fprintf(fc,	"	dst->a_dims[%d] = src->a_dims[%d];\n", i, i);
	}

	fprintf(fc,	"	dst->a_data = (%s *)(dst->a_data) - (src->a_offset - dst->a_offset);\n"
			"	dst->a_offset = src->a_offset;\n"
			"    }\n"
			"    if (src->a_sz <= 0) return;\n"
			"    p = (void *)((char *)(dst->a_data) + off);\n",
	    t->tp_tpdef);

	if (t->tp_flags & T_DYNAMIC) {
	    gen_generic_dynamic_if(t);
	    fprintf(fc,	"    for (i = dst->a_sz; i > 0; i--, p++, q++) {\n"
	    		"	%s(p, q);\n"
	    		"    }\n",
		t->tp_assignfunc);
	    if (t->tp_fund == T_GENERIC) {
		fputs(	"#else\n"
			"    memcpy(p, q, sz);\n"
			"#endif\n", fc);
	    }
	}
	else {
	    fputs(	"    memcpy(p, q, sz);\n", fc);
	}
}

static void
gen_record_assign(tp)
	t_type	*tp;
{
	t_def *deflist = tp->rec_scope->sc_def;

	fprintf(fc, "    if (dst == src) return;\n");

	/* Assign element by element. */
	while (deflist) {
	    if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
		t_type *t = deflist->df_type;
		if (deflist->df_kind == D_OFIELD &&
		    (deflist->df_flags & (D_UPPER_BOUND|D_LOWER_BOUND))) {
		}
		else if (t->tp_fund & T_ISSIMPLEARG) {
		    fprintf(fc,
		    "    dst->%s = src->%s;\n",
			deflist->df_name,
			deflist->df_name);
		}
		else {
		    fprintf(fc,
		    "    %s(&(dst->%s), &(src->%s));\n",
			t->tp_assignfunc,
			deflist->df_name,
			deflist->df_name);
		}
	    }
	    deflist = deflist->df_nextinscope;
	}
}

static void
gen_union_assign(tp)
	t_type	*tp;
{
	t_def	*deflist = tp->rec_scope->sc_def;
	char	*tagid = deflist->df_idf->id_text;

	fputs(		"    if (dst == src) return;\n", fc);
	if (tp->tp_flags & T_DYNAMIC) {
	    fprintf(fc,	"    %s(dst);\n", tp->tp_freefunc);
	}
	fprintf(fc,	"    if (!src->u_init) { dst->u_init=0; return; }\n"
			"    dst->u_init = 1;\n"
			"    dst->f_%s = src->f_%s;\n"
			"    switch(dst->f_%s) {\n",
	    tagid,
	    tagid,
	    tagid);

	deflist = deflist->df_nextinscope;	/* skip tag field */
	while (deflist) {
	    t_type	*t = deflist->df_type;
	    fprintf(fc,	"    case %ld:\n", deflist->fld_tagvalue->nd_int);
	    if (t->tp_fund & T_ISSIMPLEARG) {
		fprintf(fc,
			"	dst->u_el.%s == src->u_el.%s;\n"
			"	break;\n",
					deflist->df_name,
					deflist->df_name);
	    }
	    else {
		fprintf(fc,
			"	%s(&(dst->u_el.%s), &(src->u_el.%s));\n"
			"	break;\n",
		    			t->tp_assignfunc,
		    			deflist->df_name,
		    			deflist->df_name);
	    }
	    deflist = deflist->df_nextinscope;
	}
	fputs(		"    }\n", fc);
}

static void
gen_set_assign(tp)
	t_type	*tp;
{
	t_type	*t = element_type_of(tp);

	fprintf(fc,	"    t_elem *de, *se;\n"
			"    int i, j = MAXELC;\n"
			"\n"
			"    if (src == dst) return;\n"
			"    if (dst->s_nelem > 0) %s(dst);\n"
			"    dst->s_nelem = src->s_nelem;\n"
			"    dst->s_elem = 0;\n"
			"    for (se = src->s_elem; se; se = se->e_next) {\n"
			"        for (i = MAXELC-1; i >= 0; i--) if (se->e_mask & (1 << i)) {\n"
			"            if (j == MAXELC) {\n"
			"                size_t sz = sizeof(t_elemhdr)+MAXELC * sizeof(%s);\n"
			"                j = 0; de = m_malloc(sz);\n",
	    tp->tp_freefunc,
	    t->tp_tpdef);

	if (t->tp_flags & T_DYNAMIC) {
	    fputs(	"                memset(de, '\\0', sz);\n", fc);
	}
	else {
	    fputs(	"                de->e_count = 0;\n"
			"                de->e_mask = 0;\n", fc);
	}
	fputs(		"                de->e_next = dst->s_elem;\n"
			"                dst->s_elem = de;\n"
			"            }\n", fc);
	if (t->tp_fund & T_ISSIMPLEARG) {
	    fprintf(fc,	"            *(((%s *) (de->e_buf)) + j) = *(((%s *) (se->e_buf)) + i);\n",
		t->tp_tpdef,
		t->tp_tpdef);
	}
	else {
	    fprintf(fc,	"            %s((%s *)(de->e_buf) + j, (%s *)(se->e_buf) + i);\n",
		t->tp_assignfunc,
		t->tp_tpdef,
		t->tp_tpdef);
	}
	fputs(		"            de->e_mask |= (1 << j);\n"
			"            de->e_count++;\n"
			"            j++;\n"
			"        }\n"
			"    }\n", fc);
}

static void
gen_bag_assign(tp)
	t_type	*tp;
{
	gen_set_assign(tp);
}

static void
gen_graph_assign(tp)
	t_type	*tp;
{
	t_type	*roottp = tp->gra_root;
	t_type	*nodetp = tp->gra_node;

	fprintf(fc,	"    t_mt *sn, *dn;\n"
			"    int i;\n\n"
			"    if (dst == src) return;\n"
			"    %s(dst);\n"
			"    dst->g_size = src->g_size;\n",
	    tp->tp_freefunc);

	if (roottp) {
	    fprintf(fc,	"    %s(&(dst->g_root), &(src->g_root));\n",
		roottp->tp_assignfunc);
	}

	fprintf(fc,	"    if (src->g_size == 0) return;\n"
			"    dst->g_mt = m_malloc(src->g_size * sizeof(t_mt));\n"
			"    dst->g_freelist = src->g_freelist;\n"
			"    sn = src->g_mt; dn = dst->g_mt;\n"
			"    for (i = src->g_size; i > 0; i--, sn++, dn++) {\n"
			"        dn->g_age = sn->g_age;\n"
			"        if (nodeisfree(sn)) {\n"
			"            dn->g_nextfree = sn->g_nextfree;\n"
			"        } else {\n"
		 	"            dn->g_node = g_getnode(dst, sizeof(%s));\n"
			"            %s(dn->g_node, sn->g_node);\n"
			"        }\n"
			"    }\n",
	    nodetp->tp_tpdef,
	    nodetp->tp_assignfunc);
}

static void
gen_object_assign(tp)
	t_type	*tp;
{
	t_type	*t = record_type_of(tp);

	/* Partitioned object assignment not implemented yet. */
	if (tp->tp_flags & T_PART_OBJ) {
		/* ???? */
		return;
	}

#ifdef OBJECT_ASSIGN_CREATES_NEW_OBJECT
	/* If this define is set, doing an object assignment creates a new
	   object. The old object is deleted (which may just decrease a
	   reference count.
	*/
	fprintf(fc,	"    int op_flags = 0;\n\n"
			"    %s(dst);\n"
			"    dst->o_fields = m_malloc(sizeof(%s));\n"
			"    memset(dst->o_fields, 0, sizeof(%s));\n"
			"    o_init_rtsdep(dst, &%s, (char *) 0);\n"
			"    if (! o_isshared(src) || o_start_read(src)) {\n"
			"        %s(dst->o_fields, src->o_fields);\n"
			"        if (o_isshared(src)) o_end_read(src);\n"
			"    } else {\n"
			"        void *argv[1];\n"
			"        argv[0] = dst;\n"
			"        DoOperation(src, &op_flags, &%s, /* READOBJ */ 0, 0, argv);\n"
			"    }\n",
		tp->tp_freefunc,
		t->tp_tpdef,
		t->tp_tpdef,
		tp->tp_descr,
		t->tp_assignfunc,
		tp->tp_descr);
#else /* not OBJECT_ASSIGN_CREATES_NEW_OBJECT */
	/* If the define is not set, an assignment to an object changes the
	   value of the object.
	*/
	fprintf(fc,	"    int op_flags = 0;\n"
			"    t_object tmp;\n"
			"    void *argv[1];\n"
			"    int src_local;\n"
			"    int dst_local;\n"
			"\n"
			"    if (dst == src) return;\n"
			"    argv[0] = &tmp;\n"
			"    if (! dst->o_fields) {\n"
			"        dst->o_fields = m_malloc(sizeof(%s));\n"
			"        memset(dst->o_fields, 0, sizeof(%s));\n"
			"        o_init_rtsdep(dst, &%s, (char *) 0);\n"
			"    }\n"
			"    src_local = ! o_isshared(src);\n"
			"    dst_local = ! o_isshared(dst);\n"
			"    if (dst_local && (src_local || o_start_read(src))) {\n"
			"        %s(dst->o_fields, src->o_fields);\n"
			"        if (! src_local) o_end_read(src);\n"
			"        return;\n"
			"    }\n"
			"    if (src_local && (dst_local || o_start_write(dst))) {\n"
			"        %s(dst->o_fields, src->o_fields);\n"
			"        if (! dst_local) o_end_write(dst, 1);\n"
			"        return;\n"
			"    }\n"
			"    tmp.o_fields = m_malloc(sizeof(%s));\n"
			"    memset(tmp.o_fields, 0, sizeof(%s));\n"
			"    o_init_rtsdep(&tmp, &%s, (char *) 0);\n"
			"    if (src_local || o_start_read(src)) {\n"
			"        %s(tmp.o_fields, src->o_fields);\n"
			"        if (! src_local) o_end_read(src);\n"
			"    } else {\n"
			"        DoOperation(src, &op_flags, &%s, /* READOBJ */ 0, 0, argv);\n"
			"    }\n"
			"    if (dst_local || o_start_write(dst)) {\n"
			"        %s(dst->o_fields, tmp.o_fields);\n"
			"        if (! dst_local) o_end_write(dst, 1);\n"
			"    } else {\n"
			"        DoOperation(dst, &op_flags, &%s, /* WRITEOBJ */ 1, 0, argv);\n"
			"    }\n"
			"    o_free(&tmp);\n"
			"    m_free(tmp.o_fields);\n",
		t->tp_tpdef,
		t->tp_tpdef,
		tp->tp_descr,
		t->tp_assignfunc,
		t->tp_assignfunc,
		t->tp_tpdef,
		t->tp_tpdef,
		tp->tp_descr,
		t->tp_assignfunc,
		tp->tp_descr,
		t->tp_assignfunc,
		tp->tp_descr);
#endif /* not OBJECT_ASSIGN_CREATES_NEW_OBJECT */
}

void
gen_free_func(tp, exported)
	t_type	*tp;
	int	exported;
{
	/* No free-ing functions for simple types and nodenames. */
	if (tp->tp_fund & (T_ISSIMPLEARG|T_NODENAME)) return;

	/* No free-ing functions for statically sized types. */
	if (!(tp->tp_flags & T_DYNAMIC)) return;

	/* All free-ing functions have the same interface, using a void
	   pointer.
	*/
	if (exported) {
		/* Produce prototype in include file. */
	    fprintf(fh,	"void %s(void *);\n",
		tp->tp_freefunc);
	}
	fprintf(fc,	"%svoid %s(void *d) {\n    %s *dst = d;\n",
	    ! exported ? "static " : "",
	    tp->tp_freefunc,
	    tp->tp_tpdef);

	switch(tp->tp_fund) {
	case T_ARRAY:
		gen_array_free(tp);
		break;
	case T_RECORD:
		gen_record_free(tp);
		break;
	case T_UNION:
		gen_union_free(tp);
		break;
	case T_SET:
		gen_set_free(tp);
		break;
	case T_BAG:
		gen_bag_free(tp);
		break;
	case T_GRAPH:
		gen_graph_free(tp);
		break;
	case T_OBJECT:
		gen_object_free(tp);
		break;
	default:
		crash("gen_free_func");
		break;
	}
	fputs(		"}\n", fc);
}

static void
gen_array_free(tp)
	t_type	*tp;
{
	t_type	*t = element_type_of(tp);

	if (!(tp->tp_flags & T_CONSTBNDS)) {
	    fputs(	"    if (dst->a_sz <= 0) return;\n", fc);
	}

	if (t->tp_flags & T_DYNAMIC) {
	    gen_generic_dynamic_if(t);
	    fputs(	"    {   int i;\n", fc);
	    if (tp->tp_flags & T_CONSTBNDS) {
		fprintf(fc,
			"        %s *p = dst->a_data;\n"
			"        for (i = %d; i > 0; i--, p++) {\n",
		    t->tp_tpdef,
		    tp->arr_size);
	    }
	    else {
		fprintf(fc,
			"        %s *p = ((%s *) (dst->a_data)) + dst->a_offset;\n"
			"        for (i = dst->a_sz; i > 0; i--, p++) {\n",
		    t->tp_tpdef,
		    t->tp_tpdef);
	    }
	    fprintf(fc,
			"            %s(p);\n"
			"        }\n"
			"    }\n",
		t->tp_freefunc);
	    gen_generic_endif(t);
	}

	if (!(tp->tp_flags & T_CONSTBNDS)) {
	    fprintf(fc,	"    m_free(((%s *) (dst->a_data)) + dst->a_offset);\n"
			"    dst->a_sz = 0;\n",
		t->tp_tpdef);
	}
}

static void
gen_record_free(tp)
	t_type	*tp;
{
	t_def *deflist = tp->rec_scope->sc_def;

	/* Free element by element. */
	while (deflist) {
	    if (deflist->df_kind & (D_FIELD|D_OFIELD)) {
		t_type *t = deflist->df_type;
		if (t->tp_flags & T_DYNAMIC) {
		    gen_generic_dynamic_if(t);
		    fprintf(fc,
			"    %s(&(dst->%s));\n",
					t->tp_freefunc,
					deflist->df_name);
		    gen_generic_endif(t);
		}
	    }
	    deflist = deflist->df_nextinscope;
	}
}

static void
gen_union_free(tp)
	t_type	*tp;
{
	t_def	*deflist = tp->rec_scope->sc_def;

	fprintf(fc,	"    if (! dst->u_init) {  return; }\n"
			"    switch(dst->f_%s) {\n",
	    deflist->df_idf->id_text);

	while ((deflist = deflist->df_nextinscope)) {
	    t_type	*t = deflist->df_type;
	    if (t->tp_flags & T_DYNAMIC) {
		gen_generic_dynamic_if(t);
		fprintf(fc,
			"    case %ld:\n"
			"	%s(dst->u_el.%s);\n"
			"	break;\n",
					deflist->fld_tagvalue->nd_int,
					t->tp_freefunc,
					deflist->df_name);
		gen_generic_endif(t);
	    }
	}
	fputs(		"    }\n",		fc);
}

static void
gen_set_free(tp)
	t_type	*tp;
{
	t_type	*t = element_type_of(tp);

	fputs(		"    t_elem *e;\n", fc);
	if (t->tp_flags & T_DYNAMIC) {
	    gen_generic_dynamic_if(t);
	    fprintf(fc,	"    for (e = dst->s_elem; e; e = e->e_next) {\n"
			"        int i;\n"
			"        for (i = 0; i < MAXELC; i++) {\n"
			"            if (e->e_mask & (1 << i)) %s((%s *)(e->e_buf) + i);\n"
			"        }\n"
			"    }\n",
		t->tp_freefunc,
		t->tp_tpdef);
	    gen_generic_endif(t);
	}
	fputs(		"    e = dst->s_elem;\n"
			"    while (e) {\n"
			"        dst->s_elem = e->e_next;\n"
			"        m_free(e);\n"
			"        e = dst->s_elem;\n"
			"    }\n"
			"    dst->s_nelem = 0;\n",
	    fc);
}

static void
gen_bag_free(tp)
	t_type	*tp;
{
	gen_set_free(tp);
}

static void
gen_graph_free(tp)
	t_type	*tp;
{
	t_type	*nodetp = tp->gra_node;

	if (nodetp->tp_flags & T_DYNAMIC) {
	    gen_generic_dynamic_if(nodetp);
	    fprintf(fc,	"    int i;\n"
			"    t_mt *gn = dst->g_mt;\n"
			"    for (i = dst->g_size; i > 0; i--, gn++) if (! nodeisfree(gn) && gn->g_node) {\n"
			"        %s(gn->g_node);\n"
			"    }\n",
		nodetp->tp_freefunc);

	    gen_generic_endif(nodetp);
	}
	fputs(		"    g_freeblocks(dst);\n", fc);
	if (tp->gra_root && (tp->gra_root->tp_flags & T_DYNAMIC)) {
	    gen_generic_dynamic_if(tp->gra_root);
	    fprintf(fc, "    %s(&(dst->g_root));\n",
		tp->gra_root->tp_freefunc);
	    gen_generic_endif(tp->gra_root);
	}
}

static void
gen_object_free(tp)
	t_type	*tp;
{
	t_type	*t = record_type_of(tp);

	if (tp->tp_flags & T_PART_OBJ) {
	    fputs(	"    do_free_instance(*dst);\n", fc);
	    return;
	}
	fputs(		"    if (dst->o_fields && o_free(dst)) {\n", fc);
	if (t->tp_flags & T_DYNAMIC) {
	    gen_generic_dynamic_if(t);
	    fprintf(fc,	"        %s(dst->o_fields);\n",
		t->tp_freefunc);
	    gen_generic_endif(t);
	}
	fputs(		"        m_free(dst->o_fields);\n"
			"    }\n",
	    fc);
}
