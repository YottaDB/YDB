/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IO_H
#define IO_H

#ifdef USING_ICONV
#define _OSF_SOURCE
#include <iconv.h>
#undef _OSF_SOURCE
#endif
#include <stdarg.h>

#include "gt_timer.h"
#include "stack_frame.h"
#include "mv_stent.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

error_def(ERR_BADCHSET);
error_def(ERR_UTF16ENDIAN);

#define INSERT			TRUE
#define NO_INSERT		FALSE
#define IO_SEQ_WRT		1
#define IO_RD_ONLY		2
#define ESC_LEN			16
#define MAX_DEVCTL_LENGTH	256
#define IO_ESC			0x1b
#define MAX_DEV_TYPE_LEN	7
#define DD_BUFLEN		80

#define ASCII_ESC 		27	/*	this ASCII value is needed on any platform	*/
#define EBCDIC_ESC		39
#define ASCII_CR		13
#define EBCDIC_CR		13
#define ASCII_LF		10
#define EBCDIC_LF		37
#define ASCII_FF		12
#define EBCDIC_FF		12
#define ASCII_BS		8
#define EBCDIC_BS		22
#define VT			11

#define CHAR_FILTER 128
#define ESC1 1
#define ESC2 2
#define ESC_MASK (ESC1+ESC2)

#define START 	0
#define AFTESC 	1
#define SEQ1 	2
#define SEQ2 	3
#define SEQ3 	4
#define SEQ4 	5
#define FINI 	6
#define BADESC 	7

#define DEFAULT_IOD_LENGTH	55
#define DEFAULT_IOD_WIDTH	80
#define DEFAULT_IOD_WRAP	TRUE

#define TCPDEF_WIDTH	255
#define TCPDEF_LENGTH	66

#define MAX_BLK_SZ	65535

#define DLRZKEYLEN	1024

#define BADCHAR_DEVICE_MSG	"BADCHAR error raised on input"
#define UNAVAILABLE_DEVICE_MSG	"Resource temporarily unavailable"
#define SOCKETPOOLNAME		"YGTMSOCKETPOOL"
#define SOCDEVTYPENAME		"socket"

#define LOGNAME_LEN 255	/* Maximum possible length of name in "io_log_name.dollar_io[]". Enforced by "get_log_name". */

typedef unsigned char params;

/*
 * The enum nl below conflicts with curses.h on AIX.   At some point
 * These names should be expanded to less generic identifiers
 * to avoid conflicts with prototype header files.
 */
enum io_dev_type
{	tt,		/* terminal	*/
	mt,		/* mag tape - obsolete but left so devices aren't renumbered - could reuse */
	rm,		/* rms		*/
	us,		/* user device driver */
	mb,		/* mail box - obsolete but left so devices aren't renumbered - could reuse */
	nl,		/* null device	*/
	ff,		/* fifo device  */
	gtmsocket,	/* socket device, socket is already used by sys/socket.h */
	pi,		/* pipe */
	n_io_dev_types	/* terminator	*/
};

enum io_dev_state
{	dev_never_opened,
	dev_closed,
	dev_open,
	n_io_dev_states
};

typedef struct
{
	struct	io_desc_struct	*in;
	struct	io_desc_struct	*out;
}io_pair;


typedef struct io_desc_struct
{
	io_pair				pair;
	struct io_log_name_struct	*trans_name;
	struct io_log_name_struct	*name;
	mstr				error_handler;
	unsigned int			length;
	unsigned int			width;
	bool				perm;		/* permanent	*/
	bool				wrap;		/* if FALSE trunc */
	enum io_dev_type		type;
	enum io_dev_state		state;
	struct
	{	unsigned int	x;
		unsigned int	y;
		unsigned short	zeof;
		unsigned short	za;
		unsigned char	zb[ESC_LEN];
 		char		key[DD_BUFLEN];
 		char		device[DD_BUFLEN];
		char		*devicebuffer;
		int		devicebufferlen;
	}dollar;
	unsigned char			esc_state;
	void				*dev_sp;
	struct dev_dispatch_struct	*disp_ptr;
#if defined(KEEP_zOS_EBCDIC)
	iconv_t				input_conv_cd;
	iconv_t				output_conv_cd;
	enum code_set_type		in_code_set;
	enum code_set_type		out_code_set;
#endif
	boolean_t			newly_created;
#ifdef __MVS__
	gtm_chset_t			file_chset;	/* from file tag */
	gtm_chset_t			process_chset;	/* how to do conversion */
	unsigned int			file_tag;
	boolean_t			text_flag;
	boolean_t			is_ichset_default;
	boolean_t			is_ochset_default;
#endif
	gtm_chset_t			ichset;
	gtm_chset_t			ochset;
	int4				write_filter;
} io_desc;

/*
 * ICHSET: UTF-16
 * First READ: BOM
 * Transition to UTF-16BE or UTF-16LE based on BOM
 *
 * ICHSET: UTF-16
 * First READ: Not BOM
 * Transition to UTF-16BE per standard
 *
 * ICHSET: UTF-16LE (or UTF-16BE)
 * First READ: BOM or not BOM
 * Do nothing, assume input is in specified endian format. Pass input to application (if BOM present, it is treated as ZWNBS)
 *
 * OCHSET: UTF-16
 * First WRITE: Transition to UTF-16BE, write BOM
 *
 * OCHSET: UTF-16LE (or UTF-16BE)
 * First WRITE: Do not WRITE BOM. All output in specified endian format
 */

typedef struct io_log_name_struct
{
	io_desc		*iod;		/* io descriptor	*/
	struct io_log_name_struct
			*next;		/* linked list		*/
	unsigned char	len;		/* name length		*/
	char		dollar_io[1];	/* _$IO hidden variable	*/
}io_log_name;

io_log_name *get_log_name(mstr *v, bool insert);

/* wttab is not used in the IO dispatch, but used in the user defined dispatch for ious*. Even though all the entries are NULL in
 * the IO dispatch table are NULL in the IO dispatch tables, they have to remain. */
typedef struct dev_dispatch_struct
{
	short	(*open)(io_log_name *, mval *, int, mval *, uint8);
	void	(*close)(io_desc *, mval *);
	void	(*use)(io_desc *, mval *);
	int	(*read)(mval *, uint8);
	int	(*rdone)(mint *, uint8);
	void	(*write)(mstr *);
	void	(*wtone)(int);
	void	(*wteol)(int4, io_desc *);
	void	(*wtff)(void);
	void	(*wttab)(int4);
	void	(*flush)(io_desc *);
	int	(*readfl)(mval *, int4, uint8);
	void	(*iocontrol)(mstr *, int4, va_list);
	void	(*dlr_device)(mstr *);
	void	(*dlr_key)(mstr *);
	void	(*dlr_zkey)(mstr *);
} dev_dispatch_struct;

/* io_ prototypes */
void io_rundown(int rundown_type);
void io_init(boolean_t term_ctrl);
bool io_is_rm(mstr *name);
bool io_is_sn(mstr *tn);
struct mv_stent_struct *io_find_mvstent(io_desc *io_ptr, boolean_t clear_mvstent);
boolean_t io_open_try(io_log_name *naml, io_log_name *tl, mval *pp, uint8 nsec_timeout, mval *mspace);
enum io_dev_type io_type(mstr *tn);
void io_init_name(void);

#define ioxx_open(X)		short io##X##_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, uint8 nsec_timeout)
#define ioxx_dummy(X)		short io##X##_dummy(io_log_name *dev_name, mval *pp, int fd, mval *mspace, uint8 msec_timeout)
#define ioxx_close(X)		void io##X##_close(io_desc *iod, mval *pp)
#define ioxx_use(X)		void io##X##_use(io_desc *iod, mval *pp)
#define ioxx_read(X)		int io##X##_read(mval *v, uint8 nsec_timeout)
#define ioxx_rdone(X)		int io##X##_rdone (mint *v, uint8 nsec_timeout)
#define ioxx_write(X)		void io##X##_write(mstr *v)
#define ioxx_wtone(X)		void io##X##_wtone(int c)
#define ioxx_wteol(X)		void io##X##_wteol(int4 cnt, io_desc *iod)
#define ioxx_wtff(X)		void io##X##_wtff(void)
#define ioxx_wttab(X)		void io##X##_wttab(int4 x)
#define ioxx_flush(X)		void io##X##_flush(io_desc *iod)
#define ioxx_readfl(X)		int io##X##_readfl(mval *v, int4 width, uint8 nsec_timeout)
#define xx_iocontrol(X)		void X##_iocontrol(mstr *mn, int4 argcnt, va_list args)
#define xx_dlr_device(X)	void X##_dlr_device(mstr *d)
#define xx_dlr_key(X)		void X##_dlr_key(mstr *d)
#define xx_dlr_zkey(X)		void X##_dlr_zkey(mstr *d)

/* Following definitions have a pattern that most of the routines follow. Only exceptions are:
 *	1. ioff_open() is an extra routine
 *	2. iopi_open() is an extra routine
 *	3. iopi_iocontrol()  to handle write /writeof
 *	4. iosocket_dlr_zkey() is only for sockets
 */

#define ioxx(X) ioxx_##X(tt); ioxx_##X(mt); ioxx_##X(rm); ioxx_##X(nl); ioxx_##X(us); ioxx_##X(socket)
#define xxdlr(X) xx_iocontrol(X); xx_dlr_device(X); xx_dlr_key(X)
#define xxdlrzk(X) xx_iocontrol(X); xx_dlr_device(X); xx_dlr_key(X); xx_dlr_zkey(X)

/* prototypes for dispatch functions */

ioxx(open);
ioxx(close);
ioxx(rdone);
ioxx(use);
ioxx(read);
ioxx(readfl);
ioxx(write);
ioxx(wtone);
ioxx(wteol);
ioxx(wtff);
ioxx(dummy);
ioxx(flush);
xxdlrzk(nil);
xxdlr(ious);
xxdlrzk(iosocket);
ioxx_open(ff);
ioxx_open(pi);
xxdlrzk(iopi);	/* we need iopi_iocontrol(), iopi_dlr_device(), iopi_dlr_key() and iopi_dlr_zkey() */
xxdlr(iott);	/* we need iott_iocontrol(), iott_dlr_device() and iott_dlr_key() */
ioxx_wttab(us);
/* iott_ prototypes */
uchar_ptr_t iott_escape(uchar_ptr_t strin, uchar_ptr_t strtop, io_desc *io_ptr);

/* iosocket_ prototypes */
boolean_t iosocket_listen(io_desc *iod, unsigned short len);
boolean_t iosocket_wait(io_desc *iod, uint8 timepar);
void iosocket_poolinit(void);
void iosocket_pass_local(io_desc *iod, pid_t pid, uint8 timeout, int argcnt, va_list args);
void iosocket_accept_local(io_desc *iod, mval *handlevar, pid_t pid, uint8 timeout, int argcnt, va_list args);

/* tcp_ prototypes used by mupip */
int tcp_open(char *host, unsigned short port, uint8 timeout, boolean_t passive);

bool same_device_check(mstr tname, char *buf);

#define	iotype(O,X,Y,Z) 								\
{ 										\
	O##_open, X##_close, X##_use, X##_read, X##_rdone, X##_write, 		\
	X##_wtone, X##_wteol, X##_wtff, NULL, X##_flush, X##_readfl,		\
	Y##_iocontrol, Y##_dlr_device, Y##_dlr_key, Z##_dlr_zkey 				\
}
#define ionil_dev 												\
{														\
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL	\
}

int outc(int ch);

void get_dlr_device(mval *v);
void get_dlr_key(mval *v);
void get_dlr_zkey(mval *v);


void flush_pio(void);
void write_text_newline_and_flush_pio(mstr *text);

void remove_rms(io_desc *ciod);
void iosocket_destroy(io_desc *ciod);

#define MAX_CHSET_NAME	64
#define TAB_BUF_SZ	128
#define READTIMESTR	"READ"

LITREF unsigned char spaces_block[];

#if defined(KEEP_zOS_EBCDIC)

LITREF unsigned char ebcdic_spaces_block[];

#define SPACES_BLOCK ((ascii != io_curr_device.out->out_code_set) ? \
		ebcdic_spaces_block : spaces_block)
#define RM_SPACES_BLOCK ((ascii != iod->out_code_set) ?		\
		ebcdic_spaces_block : spaces_block)


#ifdef __MVS__

#define SET_CODE_SET(CODE_SET, CODE_SET_STR)			\
{								\
	if (!strcmp(CODE_SET_STR, OUTSIDE_CH_SET))		\
		CODE_SET = ebcdic;				\
	else							\
		CODE_SET = ascii;				\
}
#else

#define SET_CODE_SET(CODE_SET, CODE_SET_STR)

#endif /* __MVS__ */

#else /* !KEEP_zOS_EBCDIC */

#define SPACES_BLOCK spaces_block
#define RM_SPACES_BLOCK spaces_block

#endif /* KEEP_zOS_EBCDIC */

#define ICONV_OPEN_CD(DESC_CD, CODE_SRC, CODE_TARGET)		\
{								\
	if (!strcmp(CODE_TARGET, CODE_SRC))			\
		DESC_CD = NO_XLAT;				\
	else if (!strcmp(CODE_TARGET, "ISO8859-1"))		\
		DESC_CD = EBCDIC_TO_ASCII;			\
	else							\
		DESC_CD = ASCII_TO_EBCDIC;			\
}

#define ICONV_CLOSE_CD(DESC_CD)	(DESC_CD = NO_XLAT)

#define ICONVERT(CD, SRC, IN_LEN_PTR, DEST, OUT_LEN_PTR)	\
{								\
	if (EBCDIC_TO_ASCII == CD)				\
		ebc_to_asc(*(DEST), *(SRC), *(IN_LEN_PTR));	\
	else if (ASCII_TO_EBCDIC == CD)				\
		asc_to_ebc(*(DEST), *(SRC), *(IN_LEN_PTR));	\
}

/* iosocket_open needs to prevent changing chset sometimes */
#define SET_ENCODING_VALIDATE(CHSET, CHSET_MSTR, VALIDATE)									\
{																\
	int 	chset_idx;													\
																\
	chset_idx = verify_chset(CHSET_MSTR);											\
	VALIDATE;														\
	if (0 <= chset_idx)													\
		(CHSET) = (gtm_chset_t)chset_idx;										\
	else															\
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_BADCHSET, 2, (CHSET_MSTR)->len, (CHSET_MSTR)->addr);		\
}
#define SET_ENCODING(CHSET, CHSET_MSTR)	SET_ENCODING_VALIDATE(CHSET, CHSET_MSTR,)

#define GET_ADDR_AND_LEN(ADDR, LEN)												\
{																\
	ADDR = (char *)(pp->str.addr + p_offset + 1);										\
	LEN = (int)(*(pp->str.addr + p_offset));										\
}

/* Set the UTF-16 variant IFF not already set. Use the UTF16 variant to set the new CHSET (for SOCKET devices) */
#define CHECK_UTF16_VARIANT_AND_SET_CHSET_SOCKET(VARIANT, DEV_CHSET, TMP_CHSET, ASSERT_SOCKETPTR_NULL)				\
{																\
	DEBUG_ONLY(														\
	if (IS_UTF16_CHSET(VARIANT)												\
		&& (IS_UTF16_CHSET(DEV_CHSET) && (CHSET_UTF16 != DEV_CHSET)))							\
		assert(VARIANT == DEV_CHSET);											\
	)															\
	if (IS_UTF16_CHSET(TMP_CHSET) && (CHSET_UTF16 != TMP_CHSET))								\
	{															\
		if (!IS_UTF16_CHSET(VARIANT))											\
			VARIANT = TMP_CHSET;											\
		else if (TMP_CHSET != VARIANT)											\
		{														\
			ASSERT_SOCKETPTR_NULL;											\
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_UTF16ENDIAN, 4, chset_names[VARIANT].len,			\
				chset_names[VARIANT].addr, chset_names[TMP_CHSET].len, chset_names[TMP_CHSET].addr);		\
		}														\
	}															\
	if (IS_UTF16_CHSET(TMP_CHSET) && IS_UTF16_CHSET(VARIANT))								\
		DEV_CHSET = VARIANT;												\
	else															\
		DEV_CHSET = TMP_CHSET;												\
}
/* Set the UTF-16 variant IFF not already set. Use the UTF16 variant to set the new CHSET (for IORM*) */
#define CHECK_UTF16_VARIANT_AND_SET_CHSET(VARIANT, DEV_CHSET, TMP_CHSET) 	\
		CHECK_UTF16_VARIANT_AND_SET_CHSET_SOCKET(VARIANT, DEV_CHSET, TMP_CHSET,)

/* Establish a GT.M I/O condition handler if one is not already active and the principal device is the current one. */
#define ESTABLISH_GTMIO_CH(IOD, SET_CH)												\
{																\
	GBLREF io_pair		io_std_device;											\
	GBLREF boolean_t	in_prin_gtmio;											\
																\
	intrpt_state_t		prev_intrpt_state;										\
																\
	if (CHANDLER_EXISTS && (&gtmio_ch != active_ch->ch) && (NULL != (IOD)->out)						\
			&& (NULL != io_std_device.out) && ((IOD)->out == io_std_device.out))					\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_GTMIO_CH_SET, prev_intrpt_state);							\
		ESTABLISH(gtmio_ch);												\
		SET_CH = TRUE;													\
		in_prin_gtmio = TRUE;												\
		ENABLE_INTERRUPTS(INTRPT_IN_GTMIO_CH_SET, prev_intrpt_state);							\
	} else															\
		SET_CH = FALSE;													\
}

/* Establish a GT.M I/O condition handler with return if one is not already active and the principal device is the current one. */
#define ESTABLISH_RET_GTMIO_CH(IOD, VALUE, SET_CH)										\
{																\
	GBLREF io_pair		io_std_device;											\
	GBLREF boolean_t	in_prin_gtmio;											\
																\
	intrpt_state_t		prev_intrpt_state;										\
																\
	if (CHANDLER_EXISTS && (&gtmio_ch != active_ch->ch) && (NULL != (IOD)->out)						\
			&& (NULL != io_std_device.out) && ((IOD)->out == io_std_device.out))					\
	{															\
		DEFER_INTERRUPTS(INTRPT_IN_GTMIO_CH_SET, prev_intrpt_state);							\
		ESTABLISH_RET(gtmio_ch, VALUE);											\
		SET_CH = TRUE;													\
		in_prin_gtmio = TRUE;												\
		ENABLE_INTERRUPTS(INTRPT_IN_GTMIO_CH_SET, prev_intrpt_state);							\
	} else															\
		SET_CH = FALSE;													\
}

/* Revert a GT.M I/O condition handler if one was set (based on the passed argument). */
#define REVERT_GTMIO_CH(IOD, SET_CH)												\
{																\
	GBLREF boolean_t		in_prin_gtmio;										\
	DEBUG_ONLY(GBLREF io_pair	io_std_device;)										\
																\
	intrpt_state_t		prev_intrpt_state;										\
																\
	if (SET_CH)														\
	{															\
		assert((&gtmio_ch == active_ch->ch) && (NULL != (IOD)->out)							\
			&& (NULL != io_std_device.out) && ((IOD)->out == io_std_device.out));					\
		DEFER_INTERRUPTS(INTRPT_IN_GTMIO_CH_SET, prev_intrpt_state);							\
		in_prin_gtmio = FALSE;												\
		REVERT;														\
		ENABLE_INTERRUPTS(INTRPT_IN_GTMIO_CH_SET, prev_intrpt_state);							\
	}															\
}

#define DEF_EXCEPTION(PP, P_OFF, IOD)							\
MBSTART {										\
	mval	*tmpMV;									\
											\
	/* Note: It is possible that PP->str.addr points to the stringpool.		\
	 * In this case, it could get relocated if a "stp_gcol" call happens		\
	 * inside the "op_commarg" call below. We want the relocated value to		\
	 * be stored in "IOD->error_handler" at the end of this macro. Hence		\
	 * the push of a temporary mval onto the M-stack. Not doing this can cause	\
	 * the "cache_put" call inside "op_commarg" to potentially store indirection	\
	 * code that points to a stale stringpool.					\
	 */										\
	PUSH_MV_STENT(MVST_MVAL); /* needed to protect from stp_gcol */			\
	tmpMV = &mv_chain->mv_st_cont.mvs_mval;						\
	tmpMV->mvtype = MV_STR;								\
	tmpMV->str.len = (int)(*(PP->str.addr + P_OFF));				\
	tmpMV->str.addr = (char *)(PP->str.addr + P_OFF + 1);				\
	if (!(ZTRAP_ENTRYREF & TREF(ztrap_form)))					\
	{										\
		op_commarg(tmpMV, indir_linetail);					\
		op_unwind();								\
	}										\
	IOD->error_handler = tmpMV->str;						\
	s2pool(&IOD->error_handler);							\
	POP_MV_STENT();									\
} MBEND

#define	ONE_COMMA		"1,"
#define DEVICE_BUFFER_FULL	"dollar.device BUFFER FULL! CHECK dollar.devicebuffer"

#define ALLOCATE_DOLLAR_DEVICE_BUFFER_IF_REQUIRED(IOD, LEN, WRITEBUFFER_SET)			\
MBSTART {											\
	/* This macro checks if there's enough space in dollar.device[] buffer to accommodate	\
	 * 	the error message (length = LEN). If not, it allocates dollar.devicebuffer, 	\
	 * 	but first tries to reuse if allocated before, and LEN will fit in.		\
	 * It then sets WRITEBUFFER_SET to the appropriate buffer to be written into. 		\
	 */											\
	if (DD_BUFLEN < LEN)	/* Won't fit into the buffer. allocate space. */		\
	{											\
		/* Reuse previously allocated buffer if the new STR fits in */			\
		if (IOD->dollar.devicebuffer && (LEN > IOD->dollar.devicebufferlen))		\
		{										\
			free(IOD->dollar.devicebuffer);						\
			IOD->dollar.devicebuffer = NULL;					\
		}										\
		if (!IOD->dollar.devicebuffer)							\
		{										\
			IOD->dollar.devicebuffer = malloc(LEN);					\
			IOD->dollar.devicebufferlen = LEN;					\
		}										\
		WRITEBUFFER_SET = IOD->dollar.devicebuffer;					\
		MEMCPY_LIT(&IOD->dollar.device[0], DEVICE_BUFFER_FULL);				\
	} else											\
		WRITEBUFFER_SET = &IOD->dollar.device[0];					\
} MBEND

#define SET_DOLLARDEVICE_ERRSTR(IOD, ERRSTR)							\
MBSTART {											\
	char	*WRITEBUFFER;									\
	int	LEN;										\
												\
	LEN = STRLEN(ERRSTR) + 1;	/* +1 for NULL */					\
	ALLOCATE_DOLLAR_DEVICE_BUFFER_IF_REQUIRED(IOD, LEN, WRITEBUFFER);			\
	memcpy(WRITEBUFFER, ERRSTR, LEN);							\
} MBEND

#define SET_DOLLARDEVICE_ONECOMMA_ERRSTR(IOD, ERRSTR, ERRLEN)					\
MBSTART {											\
	char	*WRITEBUFFER;									\
	int	LEN;										\
												\
	ERRLEN = STRLEN(ERRSTR);								\
	LEN = STR_LIT_LEN(ONE_COMMA) + ERRLEN + 1;	/* +1 for NULL */			\
	ALLOCATE_DOLLAR_DEVICE_BUFFER_IF_REQUIRED(IOD, LEN, WRITEBUFFER);			\
	memcpy(WRITEBUFFER, ONE_COMMA, STR_LIT_LEN(ONE_COMMA));					\
	WRITEBUFFER += STR_LIT_LEN(ONE_COMMA);							\
	memcpy(WRITEBUFFER, ERRSTR, ERRLEN + 1);						\
} MBEND

#define SET_DOLLARDEVICE_ONECOMMA_ERRSTR1_ERRSTR2(IOD, ERRSTR1, ERRLEN1, ERRSTR2, ERRLEN2)	\
MBSTART {											\
	char	*WRITEBUFFER;									\
	int	LEN;										\
												\
	ERRLEN1 = STRLEN(ERRSTR1);	/* No NULL char */					\
	ERRLEN2 = STRLEN(ERRSTR2);								\
	LEN = STR_LIT_LEN(ONE_COMMA) + ERRLEN1 + ERRLEN2 + 1; /* +1 for NULL */			\
	ALLOCATE_DOLLAR_DEVICE_BUFFER_IF_REQUIRED(IOD, LEN, WRITEBUFFER);			\
	memcpy(WRITEBUFFER, ONE_COMMA, STR_LIT_LEN(ONE_COMMA));					\
	WRITEBUFFER += STR_LIT_LEN(ONE_COMMA);							\
	memcpy(WRITEBUFFER, ERRSTR1, ERRLEN1);							\
	WRITEBUFFER += ERRLEN1;									\
	memcpy(WRITEBUFFER, ERRSTR2, ERRLEN2 + 1);						\
} MBEND

#define SET_DOLLARDEVICE_ONECOMMA_STRERROR(IOD, ERRNO)						\
MBSTART {											\
	char	*ERRSTR, *WRITEBUFFER;								\
	int	ERRLEN, LEN;									\
												\
	ERRSTR = STRERROR(ERRNO);								\
	ERRLEN = STRLEN(ERRSTR);								\
	LEN = STR_LIT_LEN(ONE_COMMA) + ERRLEN + 1; /* +1 for NULL */				\
	ALLOCATE_DOLLAR_DEVICE_BUFFER_IF_REQUIRED(IOD, LEN, WRITEBUFFER);			\
	memcpy(WRITEBUFFER, ONE_COMMA, STR_LIT_LEN(ONE_COMMA));					\
	WRITEBUFFER += STR_LIT_LEN(ONE_COMMA);							\
	memcpy(WRITEBUFFER, ERRSTR, ERRLEN + 1);						\
} MBEND

#define PUT_DOLLAR_DEVICE_INTO_MSTR(IOD, MSTR)							\
MBSTART {											\
	if (memcmp(IOD->dollar.device, DEVICE_BUFFER_FULL, STR_LIT_LEN(DEVICE_BUFFER_FULL)))	\
	{											\
		MSTR->len = STRLEN(IOD->dollar.device);						\
		MSTR->addr = IOD->dollar.device;						\
	} else											\
	{											\
		assert(IOD->dollar.devicebuffer);						\
		MSTR->addr = IOD->dollar.devicebuffer;						\
		MSTR->len = STRLEN(IOD->dollar.devicebuffer);					\
	}											\
} MBEND

/* The below macro is called before an "op_close" that is followed by a restore of "io_curr_device".
 * In that case, we need to make sure the restored "io_curr_device" after the "op_close()" does not point
 * to the just now closed/freed device. The "IN_IS_CURR_DEVICE" and "OUT_IS_CURR_DEVICE" output parameters
 * are used by the caller after the "op_close()" to adjust "io_curr_device" as appropriate.
 */
#define SAVE_IN_OUT_IS_CURR_DEVICE(SAVE_DEVICE, IN_IS_CURR_DEVICE, OUT_IS_CURR_DEVICE)	\
MBSTART {										\
	GBLREF io_pair		io_curr_device;						\
											\
	IN_IS_CURR_DEVICE = (io_curr_device.in == SAVE_DEVICE.in);			\
	OUT_IS_CURR_DEVICE = (io_curr_device.out == SAVE_DEVICE.out);			\
} MBEND

/* The below macro is invoked after an "op_close()" that is preceded by a SAVE_IN_OUT_IS_CURR_DEVICE macro call.
 * After restoring "io_curr_device" to the "SAVE_DEVICE" input macro parameter, we check the other 2 parameters.
 * If they are TRUE it means "SAVE_DEVICE.in" and/or "SAVE_DEVICE.out" were closed as part of the just recently
 * done "op_close()" call and so we adjust "io_curr_device.in" and "io_curr_device.out" respectively to point to
 * $PRINCIPAL (i.e. "io_std_device") as we do not want "io_curr_device" to point to a closed/freed device. (YDB#828)
 */
#define	RESTORE_IO_CURR_DEVICE(SAVE_DEVICE, IN_IS_CURR_DEVICE, OUT_IS_CURR_DEVICE)	\
MBSTART {										\
	GBLREF io_pair		io_curr_device, io_std_device;				\
											\
	/* Assert that if IN_IS_CURR_DEVICE is TRUE, that "io_curr_device.in" got	\
	 * automatically adjusted to point to "io_std_device.in" (by "op_close()").	\
	 */										\
	assert(!IN_IS_CURR_DEVICE || (io_curr_device.in == io_std_device.in));		\
	/* Similar assert for "OUT_IS_CURR_DEVICE" */					\
	assert(!OUT_IS_CURR_DEVICE || (io_curr_device.out == io_std_device.out));	\
	/* Restore "io_curr_device" first */						\
	io_curr_device = SAVE_DEVICE;							\
	/* Adjust "io_curr_device.in" if it points to a closed device. */		\
	if (IN_IS_CURR_DEVICE)								\
		io_curr_device.in = io_std_device.in;					\
	/* Adjust "io_curr_device.in" if it points to a closed device. */		\
	if (OUT_IS_CURR_DEVICE)								\
		io_curr_device.out = io_std_device.out;					\
} MBEND
#endif /* IO_H */
