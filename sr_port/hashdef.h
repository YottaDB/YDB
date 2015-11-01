/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define hash(x,s) (((int4)(((x)->val.i1 << 1) ^ (x)->val.i2) & MAXPOSINT4) % (int4) (s))
#define ht_free(table)        free((table)->base)     /* based on allocation in ht_init() */

typedef union {
	char 			txt[8];
	struct {uint4 i1, i2;} 	val;
} mname;

typedef struct tab_ent {
	mname 		nb;
	struct tab_ent	*link;
	char 		*ptr;
} ht_entry;

typedef struct htab_desc_struct{
	ht_entry	*base;
	ht_entry	*r;
	unsigned int	count;
	unsigned int	size;
	unsigned int	trigger_size;
} htab_desc;

ht_entry *ht_get(htab_desc *table, mname *target);
ht_entry *ht_put(htab_desc *table, mname *target, char *stash);
void ht_init(htab_desc *table, unsigned int req_ht_size);
void ht_grow(htab_desc *table);

