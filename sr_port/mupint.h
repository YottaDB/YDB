/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

enum sn_type
{
        SN_NOT = 0,
        SPAN_NODE,
        SN_CHUNK
};

typedef struct struct_spanode_integ{
	uint4		sn_type;           		/* 0: not in spanning node. 1: spanning node. 2:spanning fragment*/
	uint4		span_prev_blk;          	/* Left sibling of current block of spanning node */
	uint4		span_blk_cnt;        		/* Count of the blocks of spanning node seen so far */
	uint4		span_tot_blks;        		/* Total blks in the spanning nodes to be integ-checked */
	uint4		span_node_sz;        		/* Size of the spanning node value */
	uint4		span_frag_off;			/* Spanning node fragment offset */
	uint4		key_len;			/* Length of the key of spanning node */
	uint4		val_len;			/* Length of the val of spanning node */
	uint4		sn_cnt;				/* Spanning node count */
	uint4		sn_blk_cnt;			/* Count of the blocks used by the spanning node */
unsigned char	span_node_buf[MAX_KEY_SZ];	/* Spanning node key */
}span_node_integ;

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
