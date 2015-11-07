/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

typedef	struct glist_struct
{
	struct glist_struct	*next;
	gd_region		*reg;
	gv_namehead		*gvt;
	gvnh_reg_t		*gvnh_reg;
} glist;

#define GNAME(GLIST)	((GLIST)->gvt->gvname.var_name)

#define	DO_ROOT_SEARCH_FALSE	FALSE
#define	DO_ROOT_SEARCH_TRUE	TRUE

#define	RECORDSTAT_REGION_LIT		" (region "
#define	RECORDSTAT_REGION_LITLEN	STR_LIT_LEN(RECORDSTAT_REGION_LIT)

#define	PRINT_REG_FALSE		FALSE
#define	PRINT_REG_TRUE		TRUE

#define	ISSUE_RECORDSTAT_MSG(GL_PTR, GBLSTAT, PRINT_REG)						\
{													\
	char	gbl_name_buff[MAX_MIDENT_LEN + 2 + RECORDSTAT_REGION_LITLEN + MAX_RN_LEN + 1];		\
					/* 2 for null and '^', MAX_RN_LEN for region name,		\
					 * RECORDSTAT_REGION_LITLEN for " (region " and 1 for ")" */	\
	int	gbl_buff_index;										\
													\
	gbl_name_buff[0]='^';										\
	memcpy(&gbl_name_buff[1], GNAME(GL_PTR).addr, GNAME(GL_PTR).len);				\
	gbl_buff_index = 1 + GNAME(GL_PTR).len;								\
	if (PRINT_REG && (NULL != GL_PTR->gvnh_reg->gvspan))						\
	{												\
		MEMCPY_LIT(&gbl_name_buff[gbl_buff_index], RECORDSTAT_REGION_LIT);			\
		gbl_buff_index += RECORDSTAT_REGION_LITLEN;						\
		memcpy(&gbl_name_buff[gbl_buff_index], gl_ptr->reg->rname, gl_ptr->reg->rname_len);	\
		gbl_buff_index += gl_ptr->reg->rname_len;						\
		gbl_name_buff[gbl_buff_index++] = ')';							\
	}												\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_RECORDSTAT, 6, gbl_buff_index, gbl_name_buff,	\
		GBLSTAT.recknt, GBLSTAT.keylen, GBLSTAT.datalen, GBLSTAT.reclen);			\
}

#define	DO_OP_GVNAME(GL_PTR)								\
{											\
	GBLREF	gv_namehead		*gv_target;					\
	GBLREF	gd_region		*gv_cur_region;					\
											\
	gv_target = GL_PTR->gvt;							\
	gv_cur_region = GL_PTR->reg;							\
	change_reg();									\
	assert((NULL != gv_target) && (DIR_ROOT != gv_target->root));			\
	SET_GV_CURRKEY_FROM_GVT(gv_target);	/* needed by gvcst_root_search */	\
	GVCST_ROOT_SEARCH;								\
}

typedef struct
{
	int recknt;
	int reclen;
	int keylen;
	int datalen;
} mu_extr_stats;

#define	MU_EXTR_STATS_INIT(TOT)					\
{								\
	TOT.recknt = TOT.reclen = TOT.keylen = TOT.datalen = 0;	\
}

#define	MU_EXTR_STATS_ADD(DST, SRC)		\
{						\
	DST.recknt += SRC.recknt;		\
	if (DST.reclen < SRC.reclen)		\
		DST.reclen = SRC.reclen;	\
	if (DST.keylen < SRC.keylen)		\
		DST.keylen = SRC.keylen;	\
	if (DST.datalen < SRC.datalen)		\
		DST.datalen = SRC.datalen;	\
}

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
int mu_extr_getblk(unsigned char *ptr, unsigned char *encrypted_buff_ptr);
#if defined(GTM_CRYPT)
boolean_t mu_extr_gblout(glist *gl_ptr, mu_extr_stats *st, int format, boolean_t is_any_file_encrypted);
#elif defined(UNIX)
boolean_t mu_extr_gblout(glist *gl_ptr, mu_extr_stats *st, int format);
#else	/* VMS */
boolean_t mu_extr_gblout(glist *gl_ptr, struct RAB *outrab, mu_extr_stats *st, int format);
#endif

#ifdef UNIX
#define WRITE_BIN_EXTR_BLK(BUFF, BSIZE, WRITE_4MORE_BYTES, CRYPT_INDEX)		\
{										\
	GBLREF	io_pair		io_curr_device;					\
	mval			val;						\
	unsigned short		total_size;					\
										\
	total_size = BSIZE + ((WRITE_4MORE_BYTES) ? SIZEOF(CRYPT_INDEX) : 0);	\
	/* Write the cummulative size of the subsequent 2 op_write()s */	\
	val.mvtype = MV_STR;							\
	val.str.addr = (char *)(&total_size);					\
	val.str.len = SIZEOF(total_size);					\
	op_write(&val);								\
	if ((WRITE_4MORE_BYTES))						\
	{									\
		val.mvtype = MV_STR;						\
		val.str.addr = (char *)(&CRYPT_INDEX);				\
		val.str.len = SIZEOF(CRYPT_INDEX);				\
		op_write(&val);							\
	}									\
	/* Write the actual block */						\
	val.mvtype = MV_STR;							\
	val.str.addr = (char *)(BUFF);						\
	val.str.len = BSIZE;							\
	op_write(&val);								\
	io_curr_device.out->dollar.x = 0;					\
	io_curr_device.out->dollar.y = 0;					\
}
#define WRITE_EXTR_LINE(BUFF, BSIZE)						\
{										\
	mval	val;								\
	val.mvtype = MV_STR;							\
	val.str.addr = (char *)(BUFF);						\
	val.str.len = BSIZE;							\
	op_write(&val);								\
	op_wteol(1);								\
}
#elif defined(VMS)
#define WRITE_BIN_EXTR_BLK(PTR, SIZE, DUMMY1, DUMMY2) 				\
{										\
	unsigned short size;							\
	int status;								\
										\
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
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);		\
		mupip_exit(status);						\
	}									\
}
#define WRITE_EXTR_LINE(PTR, SIZE)					\
{									\
	int status;							\
	(outrab)->rab$l_rbf = (unsigned char *)(PTR);			\
	(outrab)->rab$w_rsz = (SIZE);					\
	status = sys$put((outrab));					\
	if (status != RMS$_NORMAL)					\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) status);	\
}
#endif

