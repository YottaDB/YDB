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

/* mu_reorg.h
	This has definitions for mupip reorg utility.
	To include this gv_cur_region must be defined and
	cdb_sc.h and copy.h must be included.
*/
#ifndef FLUSH
#define FLUSH 1
#endif
#ifndef NOFLUSH
#define NOFLUSH 0
#endif

/* Following definitions give some default values for reorg */
#define DATA_FILL_TOLERANCE 10
#define INDEX_FILL_TOLERANCE 10

/*********************************************************************
	block_number is used for swap
 *********************************************************************/
#define INCR_BLK_NUM(block_number)      		\
	(block_number)++; 				\
        if (IS_BITMAP_BLK(block_number)) 		\
		(block_number)++;			\
	assert(!IS_BITMAP_BLK(block_number));

/* DECR_BLK_NUM has no check for bit-map because it is always followed by INCR_BLK_NUM */
#define DECR_BLK_NUM(block_number)      (block_number)--

/*********************************************************************
	Get global variable length.
	Start scanning from KEY_BASE.
	rPtr1 = unsigned char pointer already defined
	KEY_BASE = where to start scan
	KEY_LEN = length found
 **********x**********************************************************/
#define GET_GBLNAME_LEN(KEY_LEN, KEY_BASE)		\
{							\
	for (rPtr1 = (KEY_BASE); ; )			\
	{						\
		if (0 == *rPtr1++) 			\
			break;				\
	}						\
	KEY_LEN = (int)(rPtr1 - (KEY_BASE));		\
}

/**********************************************************************
	Get key length scanning from key_base.
	rPtr1 = unsigned char pointer already defined
	KEY_BASE = where to start scan
	KEY_LEN = length returned
 **********************************************************************/
#define GET_KEY_LEN(KEY_LEN, KEY_BASE)			\
{							\
	for (rPtr1 = (KEY_BASE); ; )			\
	{						\
		if ((0 == *rPtr1++) && (0 == *rPtr1)) 	\
			break;				\
	}						\
	KEY_LEN = (int)(rPtr1 + 1 - (KEY_BASE));	\
}

/***********************************************************************
	Get compression count of SECOND_KEY with resprect to FIRST_KEY
	CMPC = returned compression count
	rPtr1, rPtr2 are unsigned character pointer defined earlier
 ************************************************************************/
#define GET_CMPC(CMPC, FIRST_KEY, SECOND_KEY)				\
{									\
	CMPC = 0;							\
	if ((FIRST_KEY) != (SECOND_KEY))				\
	{								\
		for (rPtr1 = (FIRST_KEY), rPtr2 = (SECOND_KEY); 	\
			CMPC < MAX_KEY_SZ;				\
				(CMPC)++)				\
		{							\
			if (*rPtr1++ != *rPtr2++)			\
				break; 					\
		}							\
	}								\
}

/************************************************************************
	validate a reocrd from
		LEVEL, REC_SIZE, KEYLEN and KEYCMPC
 ************************************************************************/
#define INVALID_RECORD(LEVEL, REC_SIZE, KEYLEN, KEYCMPC) 		\
	(( ((0 == (LEVEL)) && (2 >= (KEYLEN)) )	||			\
	(BSTAR_REC_SIZE > ((REC_SIZE) + (0 == (LEVEL) ? 1 : 0)) ) || 	\
	(gv_cur_region->max_key_size < ((int)(KEYLEN) + (KEYCMPC))) || 	\
	(gv_cur_region->max_rec_size < (REC_SIZE) ) ) ? TRUE:FALSE )

/*************************************************************************
	Process a record and read.
	Input Parameter:
		LEVEL = where reading
		REC_BASE = Starting address of record
	Output Parameter:
		KEY_CMPC = Key compression count
		REC_SIZE = record size
		KEY = pointer to key read
		KEY_LEN = Key length
		STATUS = Status of read
 *************************************************************************/
#define READ_RECORD(LEVEL, REC_BASE, KEY_CMPC, REC_SIZE, KEY, KEY_LEN, STATUS)			\
{												\
	GET_USHORT(temp_ushort, &(((rec_hdr_ptr_t)(REC_BASE))->rsiz));				\
	REC_SIZE = temp_ushort;									\
	KEY_CMPC = ((rec_hdr_ptr_t)(REC_BASE))->cmpc;						\
	if (0 != (LEVEL) && BSTAR_REC_SIZE == (REC_SIZE))					\
	{											\
		KEY_LEN = 0;									\
		STATUS = cdb_sc_starrecord;							\
	}											\
	else											\
	{											\
		for (rPtr1 = (KEY) + KEY_CMPC, rPtr2 = (REC_BASE) + SIZEOF(rec_hdr);		\
			gv_cur_region->max_key_size - 1 > (rPtr2 - (REC_BASE) - SIZEOF(rec_hdr)) && \
			gv_cur_region->max_key_size - 1 > (rPtr1 - (KEY)) ;)	\
		{										\
			if ((0 == (*rPtr1++ = *rPtr2++)) && (0 == *rPtr2))			\
				break;								\
		}										\
		*rPtr1++ = *rPtr2++;								\
		KEY_LEN = (int)(rPtr2 - (REC_BASE) - SIZEOF(rec_hdr));					\
		if ((gv_cur_region->max_rec_size < (REC_SIZE)) 		||			\
			(gv_cur_region->max_key_size < ((int)(KEY_LEN)+ (KEY_CMPC))) ||		\
			(BSTAR_REC_SIZE > ((REC_SIZE) + ((0 == (LEVEL)) ? 1 : 0))) ||		\
			(2 >= (KEY_LEN)) || (0 != *(rPtr1 - 1) || 0 != *(rPtr1 - 2)))		\
			STATUS = cdb_sc_blkmod;							\
		else										\
			STATUS = cdb_sc_normal;							\
	}											\
}

enum reorg_options {	DEFAULT = 0,
			SWAPHIST = 0x0001,
			NOCOALESCE = 0x0002,
			NOSPLIT = 0x0004,
			NOSWAP = 0x0008,
			DETAIL = 0x0010};
