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

#ifdef GTM_CRYPT
/* The following structure is used by mupip extract/load to store the hash of all the extracted dat files. These hashes will be
 * stored right after the extract header is written. Each hash  will be referred by it's index number at the beginning of each
 * record. This information will be used during mupip load to decrypt a particular record. Although the structure contains only
 * one field, we need to access the hashes by index. */
typedef struct muextr_hash_hdr_struct
{
	char	gtmcrypt_hash[GTMCRYPT_HASH_LEN];
} muext_hash_hdr;

typedef muext_hash_hdr	*muext_hash_hdr_ptr_t;
#endif

#define MU_FMT_GO		0
#define MU_FMT_BINARY		1
#define MU_FMT_GOQ		2
#define MU_FMT_ZWR		3
#define GOQ_BLK_SIZE		2048
#define FORMAT_STR_MAX_SIZE 50
#define LABEL_STR_MAX_SIZE 128
#define EXTR_DEFAULT_LABEL	"GT.M MUPIP EXTRACT"
/* In unix, the binary extract label was bumped to 5 as part of the db encryption changes. Since db encryption is not supported in
 * VMS, we keep the label at 4 for VMS. Whenever the extract label needs to be bumped up next, if possible, try to get both Unix
 * and VMS back to common label (might need to add a field in the extract header to indicate whether encryption is supported
 * or not as part of this change). */
#ifdef UNIX
#define V4_BIN_HEADER_VERSION	"4"
#define V4_BIN_HEADER_LABEL	"GDS BINARY EXTRACT LEVEL "V4_BIN_HEADER_VERSION
#define V5_BIN_HEADER_VERSION  	"5"
#define V5_BIN_HEADER_SZ		92 /* V4 (GTM V5.0) binary header stores null collation information [5 bytes numeric] */
#define V5_BIN_HEADER_NUMSZ	5
#define V5_BIN_HEADER_LABEL	"GDS BINARY EXTRACT LEVEL "V5_BIN_HEADER_VERSION
#define V5_BIN_HEADER_RECOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT) + V5_BIN_HEADER_NUMSZ)
#define V5_BIN_HEADER_KEYOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT) + 2 * V5_BIN_HEADER_NUMSZ)
#define V5_BIN_HEADER_NULLCOLLOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT) + 3 * V5_BIN_HEADER_NUMSZ)
#define BIN_HEADER_VERSION  	"6" /* spanning nodes allow max_rec_len to 7 digits*/
#define BIN_HEADER_LABEL        "GDS BINARY EXTRACT LEVEL "BIN_HEADER_VERSION
#define BIN_HEADER_VERSION_ENCR	"7" /* follow convention of low bit of version indicating encryption */
#define BIN_HEADER_LABEL_ENCR   "GDS BINARY EXTRACT LEVEL "BIN_HEADER_VERSION_ENCR
#define BIN_HEADER_SZ		100
#define BIN_HEADER_NUMSZ	7
#else
#define BIN_HEADER_VERSION  	"4"
#define BIN_HEADER_NUMSZ	5
#define BIN_HEADER_SZ		92 /* V4 (GTM V5.0) binary header stores null collation information [5 bytes numeric] */
#endif
#define BIN_HEADER_LABEL        "GDS BINARY EXTRACT LEVEL "BIN_HEADER_VERSION
#define BIN_HEADER_DATEFMT	"YEARMMDD2460SS"
#define BIN_HEADER_LABELSZ	32
#define BIN_HEADER_BLKOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT))
#define BIN_HEADER_RECOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT) + BIN_HEADER_NUMSZ)
#define BIN_HEADER_KEYOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT) + 2 * BIN_HEADER_NUMSZ)
#define BIN_HEADER_NULLCOLLOFFSET	(STR_LIT_LEN(BIN_HEADER_LABEL) + STR_LIT_LEN(BIN_HEADER_DATEFMT) + 3 * BIN_HEADER_NUMSZ)
#define V3_BIN_HEADER_SZ	87
#define EXTR_HEADER_LEVEL(extr_lbl)	*(extr_lbl + SIZEOF(BIN_HEADER_LABEL) - 2)
					/* the assumption here is - level wont go beyond a single char representation */
#define MAX_BIN_WRT		ROUND_DOWN(MAX_RMS_RECORDSIZE, SIZEOF(int))

char *mu_extr_ident(mstr *a);
void  mu_extract(void);
int mu_extr_getblk(unsigned char *ptr);		/***type int added***/
#if defined(UNIX) && defined(GTM_CRYPT)
boolean_t mu_extr_gblout(mval *gn, mu_extr_stats *st, int format, muext_hash_hdr_ptr_t hash_array,
									boolean_t is_any_file_encrypted);
#elif defined(UNIX)
boolean_t mu_extr_gblout(mval *gn, mu_extr_stats *st, int format);
#endif
#ifdef UNIX
#define WRITE_BIN_EXTR_BLK(BUFF, BSIZE)		\
{						\
	GBLREF	io_pair		io_curr_device;	\
	mval	val;				\
	val.mvtype = MV_STR;			\
	val.str.addr = (char *)(&BSIZE);	\
	val.str.len = SIZEOF(BSIZE);		\
	op_write(&val);				\
	val.mvtype = MV_STR;			\
	val.str.addr = (char *)(BUFF);		\
	val.str.len = BSIZE;			\
	op_write(&val);				\
	io_curr_device.out->dollar.x = 0;	\
	io_curr_device.out->dollar.y = 0;	\
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
#elif defined(VMS)
#define WRITE_BIN_EXTR_BLK(PTR, SIZE) 						\
{										\
	unsigned short size;							\
	int status;								\
	if (MAX_BIN_WRT < (SIZE))						\
		size = MAX_BIN_WRT;						\
	else									\
		size = (SIZE);							\
	(outrab)->rab$w_rsz = size;						\
	(outrab)->rab$l_rbf = (unsigned char *)(PTR);				\
	status = sys$put((outrab));						\
	if ((MAX_BIN_WRT < (SIZE)) && (RMS$_NORMAL == status))			\
	{									\
		assert(MAX_BIN_WRT == size);					\
		(outrab)->rab$w_rsz = (SIZE) - MAX_BIN_WRT;			\
		(outrab)->rab$l_rbf = (unsigned char *)(PTR);			\
		(outrab)->rab$l_rbf += MAX_BIN_WRT;				\
		status = sys$put((outrab));					\
	}									\
	if (!(status & 1)) 							\
	{									\
		rts_error(VARLSTCNT(1) status);					\
		mupip_exit(status);						\
	}									\
}
#define WRITE_EXTR_LINE(PTR, SIZE) 			\
{							\
	int status;					\
	(outrab)->rab$l_rbf = (unsigned char *)(PTR);	\
	(outrab)->rab$w_rsz = (SIZE);			\
	status = sys$put((outrab));			\
	if (status != RMS$_NORMAL)			\
		rts_error(VARLSTCNT(1) status);		\
}


boolean_t mu_extr_gblout(mval *gn, struct RAB *outrab, mu_extr_stats *st, int format);
#else
#error UNSUPPORTED PLATFORM
#endif

