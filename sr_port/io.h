/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifdef USING_ICONV
#define _OSF_SOURCE
#include <iconv.h>
#undef _OSF_SOURCE
#else /* no iconv.h - must define size_t on VMS platform */
#ifdef VMS
#include <sys/types.h>
#endif
#endif
#define INSERT			TRUE
#define NO_INSERT		FALSE
#define IO_SEQ_WRT		1
#define IO_RD_ONLY		2
#define ESC_LEN			16
#define MAX_DEVCTL_LENGTH	256
#define IO_ESC			0x1b
#define MAX_DEV_TYPE_LEN	7

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

typedef unsigned char params;

/*
 * The enum nl below conflicts with curses.h on AIX.   At some point
 * These names should be expanded to less generic identifiers
 * to avoid conflicts with prototype header files.
 */
enum io_dev_type
{	tt,		/* terminal	*/
	mt,		/* mag tape	*/
	rm,		/* rms		*/
	us,		/* user device driver */
	mb,		/* mail box	*/
	nl,		/* null device	*/
	ff,		/* fifo device  */
	tcp,		/* TCP socket  */
	gtmsocket,	/* socket device, socket is already used by sys/socket.h */
	n_io_dev_types	/* terminator	*/
};

enum io_dev_state
{	dev_never_opened,
	dev_closed,
	dev_open,
	n_io_dev_states
};

enum code_set_type
{
	ascii,
	ebcdic
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
	unsigned short			length;
	unsigned short			width;
	bool				perm;		/* permanent	*/
	bool				wrap;		/* if FALSE trunc */
	enum io_dev_type		type;
	enum io_dev_state		state;
	struct
	{	unsigned short	x;
		unsigned short	y;
		unsigned short	zeof;
		unsigned short	za;
		unsigned char	zb[ESC_LEN];
	}dollar;
	unsigned char			esc_state;
	void				*dev_sp;
	struct dev_dispatch_struct	*disp_ptr;
	iconv_t				input_conv_cd;
	iconv_t				output_conv_cd;
	enum code_set_type		in_code_set;
	enum code_set_type		out_code_set;
	int4				write_filter;
}io_desc;

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
	short	(*open)(io_log_name *, mval *, int, mval *, int4);
	void	(*close)(io_desc *, mval *);
	void	(*use)(io_desc *, mval *);
	short	(*read)(mval *, int4);
	short	(*rdone)(mint *, int4);
	void	(*write)(mstr *);
	void	(*wtone)(unsigned char);
	void	(*wteol)(short, io_desc *);
	void	(*wtff)(void);
	void	(*wttab)(short);
	void	(*flush)(io_desc *);
	short	(*readfl)(mval *, int4, int4);
	void	(*iocontrol)(mstr *);
	void	(*dlr_device)(mstr *);
	void	(*dlr_key)(mstr *);
}dev_dispatch_struct;

/* io_ prototypes */
void io_rundown(int rundown_type);
void io_init(bool term_ctrl);
bool io_is_rm(mstr *name);
bool io_is_sn(mstr *tn);
#ifdef UNIX
bool io_is_tt(char *name);
#endif
bool io_open_try(io_log_name *naml, io_log_name *tl, mval *pp, int4 timeout, mval *mspace);
enum io_dev_type io_type(mstr *tn);
void io_init_name(void);

#define ioxx_open(X)		short io##X##_open(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
#define ioxx_dummy(X)		short io##X##_dummy(io_log_name *dev_name, mval *pp, int fd, mval *mspace, int4 timeout)
#define ioxx_close(X)		void io##X##_close(io_desc *iod, mval *pp)
#define ioxx_use(X)		void io##X##_use(io_desc *iod, mval *pp)
#define ioxx_read(X)		short io##X##_read(mval *v, int4 t)
#define ioxx_rdone(X)		short io##X##_rdone (mint *v, int4 timeout)
#define ioxx_write(X)		void io##X##_write(mstr *v)
#define ioxx_wtone(X)		void io##X##_wtone(unsigned char c)
#define ioxx_wteol(X)		void io##X##_wteol(short cnt, io_desc *iod)
#define ioxx_wtff(X)		void io##X##_wtff(void)
#define ioxx_wttab(X)		void io##X##_wttab(short x)
#define ioxx_flush(X)		void io##X##_flush(io_desc *iod)
#define ioxx_readfl(X)		short io##X##_readfl(mval *v, int4 width, int4 timeout)
#define xx_iocontrol(X)		void X##_iocontrol(mstr *d)
#define xx_dlr_device(X)	void X##_dlr_device(mstr *d)
#define xx_dlr_key(X)		void X##_dlr_key(mstr *d)

/* Following definitions have a pattern that most of the routines follow. Only exception is:
 *      1. ioff_open() is an extra routine
 */

#define ioxx(X) ioxx_##X(tt);ioxx_##X(mt);ioxx_##X(rm);ioxx_##X(mb);ioxx_##X(nl);ioxx_##X(us);ioxx_##X(tcp);ioxx_##X(socket)
#define xxdlr(X) xx_iocontrol(X);xx_dlr_device(X);xx_dlr_key(X)

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
xxdlr(nil);
xxdlr(ious);
xxdlr(iotcp);
xxdlr(iosocket);
ioxx_open(ff);
ioxx_wttab(us);
/* iott_ prototypes */
uchar_ptr_t iott_escape(uchar_ptr_t strin, uchar_ptr_t strtop, io_desc *io_ptr);

/* iomt_ prototypes */
void iomt_getrec(io_desc *dv);
void iomt_rdstream(uint4 len, void *str, io_desc *dv);
int iomt_readblk(io_desc *dv);
void iomt_vlflush(io_desc *dv);
void iomt_wrtblk(io_desc *dv);
int iomt_wrtinit(io_desc *dv);
void iomt_wtansilab(io_desc *dv, uint4 labs);
uint4 iomt_reopen(io_desc *dv, unsigned short mode, int rewind);
void iomt_closesp(int4 channel);
void iomt_eof(io_desc *dev);
void iomt_erase(io_desc *dev);
void iomt_qio(io_desc *iod, uint4 mask, uint4 parm);
void iomt_rddoslab(io_desc *dv);
void iomt_rdansiend(io_desc *dv);
void iomt_rdansistart(io_desc *dv);
void iomt_rewind(io_desc *dev);
void iomt_skipfile(io_desc *dev, int count);
void iomt_skiprecord(io_desc *dev, int count);
void iomt_tm(io_desc *dev);
void iomt_wtdoslab(io_desc *dv);


/* iosocket_ prototypes */
boolean_t iosocket_listen(io_desc *iod, unsigned short len);
boolean_t iosocket_wait(io_desc *iod, int4 timepar);
void iosocket_poolinit(void);

/* iotcp_ prototypes */
char *iotcp_name2ip(char *name);
int iotcp_fillroutine(void);
int iotcp_getlsock(io_log_name *dev);
void iotcp_rmlsock(io_desc *iod);

/* tcp_ prototypes */
int tcp_open(char *host, unsigned short port, int4 timeout, boolean_t passive);

/* iomb_ prototypes */
int iomb_dataread (int timeout);

bool same_device_check(mstr tname, char *buf);

#define	iotype(O,X,Y) 								\
{ 										\
	O##_open, X##_close, X##_use, X##_read, X##_rdone, X##_write, 		\
	X##_wtone, X##_wteol, X##_wtff, NULL, X##_flush, X##_readfl,		\
	Y##_iocontrol, Y##_dlr_device, Y##_dlr_key 				\
}

#ifdef __sparc
int outc(char ch);
#else
int outc(int ch);
#endif

void get_dlr_device(mval *v);
void get_dlr_key(mval *v);


void flush_pio(void);

void remove_rms(io_desc *ciod);

dev_dispatch_struct *io_get_fgn_driver(mstr *s);

#define MAX_CHSET_NAME	64
#define TAB_BUF_SZ	128

LITREF unsigned char spaces_block[];
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

#endif

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
