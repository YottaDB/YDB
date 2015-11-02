/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define ZWRITE_END 0
#define ZWRITE_ASTERISK 1
#define ZWRITE_UPPER 2
#define ZWRITE_ALL 3
#define ZWRITE_VAL 4
#define ZWRITE_BOTH 5
#define ZWRITE_LOWER 6
#define ZWRITE_PATTERN 7

typedef struct zwr_sub_lst_struct
{
	struct
	{
		unsigned char subsc_type;
		mval *actual;
		mval *first,*second;
	} subsc_list[1];
} zwr_sub_lst;

typedef struct
{
	bool name_type;
	unsigned char subsc_count;
	unsigned char curr_subsc;
	mval *pat;
	mident *curr_name;
	bool fixed;
	uint4 mask;
	struct zwr_sub_lst *sub;
} lvzwrite_struct;

typedef struct
{
	bool		type;
	unsigned short	subsc_count;
	unsigned short	curr_subsc;
	mval		*pat;
	bool		fixed;
	uint4	mask;
	unsigned char	*old_key;
	unsigned char	*old_targ;
	struct zwr_sub_lst *sub;
	gd_binding	*old_map;
	gd_binding	*old_map_top;
	gd_region	*gd_reg;
} gvzwrite_struct;

void gvzwr_arg(int t, mval *a1, mval *a2);
void gvzwr_init(unsigned short t, mval *val, int4 pat);
void gvzwr_out(void);
void gvzwr_var(uint4 data, int4 n);
void lvzwr_arg(int t, mval *a1, mval *a2);
void lvzwr_init(bool t, mval *val);
void lvzwr_out(mval *val);
unsigned char *lvzwr_key(unsigned char *buff, int size);
