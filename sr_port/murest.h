/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
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

//If i_hdr is changed then update INC_HEADER_LABEL_DFLT and the version check at mupip_restore.c:316

#define INC_HDR_LABEL_SZ		26
#define INC_HDR_DATE_SZ			14
#define INC_HEADER_LABEL_V5_NOENCR	"GDSV5   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL_V6_ENCR	"GDSV6   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL_V7		"GDSV7   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL_V8		"GDSV8   INCREMENTAL BACKUP"
#define INC_HEADER_LABEL_DFLT		INC_HEADER_LABEL_V8

typedef struct i_hdr
{
	char		label[INC_HDR_LABEL_SZ];
	char		date[INC_HDR_DATE_SZ];
	char		reg[MAX_RN_LEN];
	trans_num	start_tn;
	trans_num	end_tn;
	block_id	db_total_blks;
	uint4		blk_size;
	int4		filler;	/* make 8-byte alignment explicit */
	block_id	blks_to_upgrd;
	uint4		is_encrypted;
	char		encryption_hash[GTMCRYPT_RESERVED_HASH_LEN];
	char		encryption_hash2[GTMCRYPT_RESERVED_HASH_LEN];
	boolean_t	non_null_iv;
	block_id	encryption_hash_cutoff;
	trans_num	encryption_hash2_start_tn;
} inc_header;

void murgetlst(void);
