/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CACHE_H
#define CACHE_H

/* Macros to add debugging to objcode cache for indirects. To enable, uncomment line below */
/*#define DEBUG_CACHE */
#ifdef DEBUG_CACHE
#  define DBGCACHE(x) DBGFPF(x)
#else
#  define DBGCACHE(x)
#endif

typedef struct {
	mstr		str;
	uint4		code;
} icode_str;	/* For indirect code source. */

typedef struct cache_ent
{
	mstr		obj;
	icode_str	src;
	int		refcnt;			/* Number of indirect source code pointing to same cache entry */
	int		zb_refcnt;		/* Number of zbreak action entry pointing to same cache entry */
} cache_entry;

/* Following is the indirect routine header build as part of an indirect code object */
typedef struct ihead_struct
{
	cache_entry	*indce;
	int4		vartab_off;
	int4		vartab_len;
	int4		temp_mvals;
	int4		temp_size;
	int4		fixup_vals_off;	/* literal mval table offset */
	int4		fixup_vals_num;	/* literal mval table's mval count */
} ihdtyp;

#define ICACHE_TABLE_INIT_SIZE 	64	/* Use 1K memory initially */
#define ICACHE_SIZE 		ROUND_UP2(SIZEOF(cache_entry), NATIVE_WSIZE)

/* We allow cache_table to grow till we hit memory or entry maximums. If more memory is needed, we do compaction.
 * Current default limits (overrideable by environment variable): 128 entries, 128KB of object code on all platforms
 * except IA64 architecture which gets 256KB due to its less compact instruction forms.
 */
#define MAX_CACHE_MEMSIZE	(IA64_ONLY(256) NON_IA64_ONLY(128))
#define MAX_CACHE_ENTRIES	128

void indir_lits(ihdtyp *ihead);
void cache_init(void);
mstr *cache_get(icode_str *indir_src);
void cache_put(icode_str *src, mstr *object);
void cache_table_rebuild(void);
void cache_stats(void);

#endif
