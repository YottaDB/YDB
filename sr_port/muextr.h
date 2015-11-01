/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef	struct glist_struct
{
	struct glist_struct *next;
	mval name;
	unsigned char nbuf[1]; /* dummy entry */
} glist;

typedef struct
{
	int recknt;
	int reclen;
	int keylen;
	int datalen;
} mu_extr_stats;

typedef struct coll_hdr_struct
{
	unsigned char	act;
	unsigned char	nct;
	unsigned char	ver;
	unsigned char	pad;
} coll_hdr;

#define MU_FMT_GO		0
#define MU_FMT_BINARY		1
#define MU_FMT_GOQ		2
#define MU_FMT_ZWR		3
#define GOQ_BLK_SIZE		2048
#define FORMAT_STR_MAX_SIZE 50
#define LABEL_STR_MAX_SIZE 128

#define BIN_HEADER_LABEL	"GDS BINARY EXTRACT LEVEL 3"
#define BIN_HEADER_DATEFMT	"YEARMMDD2460SS"
#define BIN_HEADER_NUMSZ	5
#define BIN_HEADER_LABELSZ	32
#define BIN_HEADER_BLKOFFSET	(sizeof BIN_HEADER_LABEL - 1 + sizeof BIN_HEADER_DATEFMT - 1)
#define BIN_HEADER_RECOFFSET	(sizeof BIN_HEADER_LABEL - 1 + sizeof BIN_HEADER_DATEFMT - 1 + BIN_HEADER_NUMSZ)
#define BIN_HEADER_KEYOFFSET	(sizeof BIN_HEADER_LABEL - 1 + sizeof BIN_HEADER_DATEFMT - 1 + 2 * BIN_HEADER_NUMSZ)
#define BIN_HEADER_SZ		87
#define EXTR_HEADER_LEVEL(extr_lbl)	*(extr_lbl + sizeof(BIN_HEADER_LABEL) - 2)
					/* the assumption here is - level wont go beyond a single char representation */

char *mu_extr_ident(mstr *a);
void  mu_extract(void);
int mu_extr_getblk(unsigned char *ptr);		/***type int added***/
#ifdef UNIX
#define WRITE_BIN_EXTR_BLK(BUFF, BSIZE)		\
{						\
	mval	val;				\
	val.mvtype = MV_STR;			\
	val.str.addr = (char *)(&BSIZE);	\
	val.str.len = sizeof(BSIZE);		\
	op_write(&val);				\
	val.mvtype = MV_STR;			\
	val.str.addr = (char *)(BUFF);		\
	val.str.len = BSIZE;			\
	op_write(&val);				\
}
#define WRITE_EXTR_LINE(BUFF, BSIZE)		\
{						\
	mval	val;				\
	val.mvtype = MV_STR;			\
	val.str.addr = (char *)(BUFF);		\
	val.str.len = BSIZE;			\
	op_write(&val);				\
	op_wteol(1);				\
}
boolean_t mu_extr_gblout(mval *gn, mu_extr_stats *st, int format);
#elif defined(VMS)
#define WRITE_BIN_EXTR_BLK(PTR, SIZE) 			\
{							\
	int status;					\
	(outrab)->rab$l_rbf = (unsigned char *)(PTR);	\
	(outrab)->rab$w_rsz = (SIZE);			\
	status = sys$put((outrab));			\
	if (!(status & 1)) 				\
	{						\
		rts_error(VARLSTCNT(1) status);		\
		mupip_exit(status);			\
	}						\
}
#define WRITE_EXTR_LINE(PTR, SIZE) 			\
{							\
	int status;					\
	(outrab)->rab$l_rbf = (unsigned char *)(PTR);	\
	(outrab)->rab$w_rsz = (SIZE);			\
	status = sys$put((outrab));			\
	if (status != RMS$_NORMAL) \
		rts_error(VARLSTCNT(1) status);		\
}
boolean_t mu_extr_gblout(mval *gn, struct RAB *outrab, mu_extr_stats *st, int format);
#else
#error UNSUPPORTED PLATFORM
#endif

