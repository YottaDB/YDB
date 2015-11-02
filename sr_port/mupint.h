/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPINT_H
#define MUPINT_H

/*  requires gdsroot.h */

#define	NO_ONLINE_ERR_MSG	"ONLINE qualifier for this region will be ignored"

typedef	struct global_list_struct
{
	block_id			root;
	struct global_list_struct	*link;
	block_id			path[MAX_BT_DEPTH + 1];
	uint4 				offset[MAX_BT_DEPTH + 1];
	unsigned char			nct;
	unsigned char			act;
	unsigned char			ver;
	char				key[MAX_MIDENT_LEN + 1];	/* max key length plus one for printf terminator */
	int4				keysize;			/* length of the key */
} global_list;
#ifdef BIGENDIAN
typedef struct
{	unsigned int	two : 4;
	unsigned int	one : 4;
} sub_num;
#else
typedef struct
{	unsigned int	one : 4;
	unsigned int	two : 4;
} sub_num;
#endif

boolean_t mu_int_blk(block_id blk, char level, boolean_t is_root, unsigned char *bot_key,
	int bot_len, unsigned char *top_key, int top_len, boolean_t eb_ok);
boolean_t mu_int_fhead(void);
boolean_t mu_int_init(void);
void mu_int_reg(gd_region *reg, boolean_t *return_value);
int mu_int_getkey(unsigned char *key_buff, int keylen);
uchar_ptr_t mu_int_read(block_id blk, enum db_ver *ondsk_blkver);
void mu_int_err(int err, boolean_t do_path, boolean_t do_range, unsigned char *bot, int has_bot,
	unsigned char *top, int has_top, unsigned int level);
void mu_int_maps(void);
void mu_int_write(block_id blk, uchar_ptr_t ptr);

#endif
