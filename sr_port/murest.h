/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#define INC_HDR_LABEL_SZ		26
#define INC_HDR_DATE_SZ			14
#define INC_HEADER_LABEL_V5_NOENCR	"GDSV5   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL_V6_ENCR	"GDSV6   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL_V7		"GDSV7   INCREMENTAL BACKUP"

typedef struct i_hdr
{
	char		label[INC_HDR_LABEL_SZ];
	char		date[INC_HDR_DATE_SZ];
	char		reg[MAX_RN_LEN];
	trans_num	start_tn;
	trans_num	end_tn;
	uint4		db_total_blks;
	uint4		blk_size;
	int4		blks_to_upgrd;
	uint4		is_encrypted;
	char            encryption_hash[GTMCRYPT_RESERVED_HASH_LEN];
	char		encryption_hash2[GTMCRYPT_RESERVED_HASH_LEN];
	boolean_t	non_null_iv;
	block_id	encryption_hash_cutoff;
	trans_num	encryption_hash2_start_tn;
} inc_header;

void murgetlst(void);
