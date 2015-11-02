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

#ifndef _ZWRITE_H_INCLUDED
#define _ZWRITE_H_INCLUDED

#define ZWRITE_END	0
#define ZWRITE_ASTERISK 1
#define ZWRITE_UPPER	2
#define ZWRITE_ALL	3
#define ZWRITE_VAL	4
#define ZWRITE_BOTH	5
#define ZWRITE_LOWER	6
#define ZWRITE_PATTERN	7

#define ZWR_HTAB_INIT_SIZE  12		/* Initial # elems in ZWR addr hash table */
/* Number of entries in zwr_zav_blk */
#define ZWR_ZAV_BLK_CNT	    ((GTM64_ONLY(256)NON_GTM64_ONLY(128) - SIZEOF(storElem) - SIZEOF(zwr_zav_blk)) / SIZEOF(zwr_alias_var))

#include "hashtab_addr.h"
#include "lv_val.h"
#include "gtm_malloc.h"

enum zwr_init_types
{
	zwr_patrn_mident, 	/* Input parm is mident addr */
	zwr_patrn_mval		/* Input parm is mval addr (describing string pattern) */
};

/* Structures associated with tracking aliases during ZWRite */
typedef struct zwr_alias_var_struct
{
	boolean_t	value_printed;
	GTM64_ONLY(int4	filler;)
	mident		zwr_var;	/* Base var name for this entry */
} zwr_alias_var;

typedef struct zwr_zav_blk_struct
{
	zwr_alias_var			*zav_base, *zav_free, *zav_top;
	struct zwr_zav_blk_struct	*next;
	/* note this block (when allocated) will also contain the zwr_alias_var array */
} zwr_zav_blk;

typedef struct zwr_hash_table_struct
{
	boolean_t			cleaned;
	GTM64_ONLY(int4			filler;)
	hash_table_addr			h_zwrtab;
	zwr_zav_blk			*first_zwrzavb;
	zwr_alias_var			*zav_flist;
} zwr_hash_table;

/* Structures used to keep track of the subscript(s) for the current var/value being processed */
typedef struct zwr_sub_lst_struct
{
	struct
	{
		unsigned char		subsc_type;
		mval			*actual;
		mval			*first, *second;
	} subsc_list[1];
} zwr_sub_lst;

typedef struct lvzwrite_datablk_struct
{
	enum zwr_init_types		zwr_intype;
	boolean_t			fixed;
	boolean_t			zav_added;	/* This call resulted in a zwr_alias_var added to hash tab */
	unsigned short			subsc_count;
	unsigned short			curr_subsc;
	uint4				mask;
	mval				*pat;
	mident				*curr_name;
	zwr_sub_lst			*sub;
	struct lvzwrite_datablk_struct	*prev;
} lvzwrite_datablk;

typedef struct gvzwrite_datablk_struct
{
	boolean_t			type;
	unsigned short			subsc_count;
	unsigned short			curr_subsc;
	boolean_t			fixed;
	uint4				mask;
	mval				*pat;
	unsigned char			*old_key;
	unsigned char			*old_targ;
	zwr_sub_lst			*sub;
	gd_binding			*old_map;
	gd_binding			*old_map_top;
	gd_region			*gd_reg;
	boolean_t			gv_last_subsc_null;
	boolean_t			gv_some_subsc_null;
} gvzwrite_datablk;

void gvzwr_arg(int t, mval *a1, mval *a2);
void gvzwr_init(unsigned short t, mval *val, int4 pat);
void gvzwr_out(void);
void gvzwr_var(uint4 data, int4 n);
void lvzwr_arg(int t, mval *a1, mval *a2);
void lvzwr_init(enum zwr_init_types t, mval *val);
void lvzwr_out(lv_val *lvp);
unsigned char *lvzwr_key(unsigned char *buff, int size);

#endif
