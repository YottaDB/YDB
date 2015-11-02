/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef	struct inc_list
{
	mstr			input_file;
	struct inc_list		*next;
} inc_list_struct;

#define INC_HDR_LABEL_SZ	26
#define INC_HDR_DATE_SZ		14
#define V5_INC_HEADER_LABEL	"GDSV5   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL	"GDSV6   INCREMENTAL BACKUP"

typedef struct i_hdr
{	char		label[INC_HDR_LABEL_SZ];
	char		date[INC_HDR_DATE_SZ];
	char		reg[MAX_RN_LEN];
	trans_num	start_tn;
	trans_num	end_tn;
	uint4		db_total_blks;
	uint4		blk_size;
	int4		blks_to_upgrd;
	boolean_t	is_encrypted;
} inc_header;

void murgetlst(void);

