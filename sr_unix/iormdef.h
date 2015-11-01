/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IORMDEF_H
#define IORMDEF_H

#define DEF_RM_WIDTH		32767
#define DEF_RM_RECORDSIZE	32767
#define DEF_RM_LENGTH		66

#define	DEF_RM_PADCHAR		' '	/* SPACE */

#ifdef UNICODE_SUPPORTED
#define SET_WIDTH_BYTES				\
	width_bytes = 1;			\
	width_chset = iod->ichset;		\
	switch (iod->ichset)			\
	{					\
	case CHSET_UTF16:			\
	case CHSET_UTF16BE:			\
	case CHSET_UTF16LE:			\
		width_bytes = 2;		\
		width_chset = iod->ichset;	\
	}					\
	switch (iod->ochset)			\
	{					\
	case CHSET_UTF16:			\
	case CHSET_UTF16BE:			\
	case CHSET_UTF16LE:			\
		width_bytes = 2;		\
		width_chset = iod->ochset;	\
	}
#else
#define SET_WIDTH_BYTES	width_bytes = 1;
#endif


/* Operations for this device type */
#define RM_NOOP		0
#define RM_WRITE	1
#define RM_READ		2

/* ***************************************************** */
/* *********** structure for RMS driver **************** */
/* ***************************************************** */

typedef struct
{
	boolean_t	fixed;		/* Fixed format file */
	boolean_t	noread;
	boolean_t	stream;
	boolean_t	fifo;
	boolean_t	done_1st_read;	/* If UTF16, we need to know if this is the first read or not to check for BOM */
	boolean_t	done_1st_write;	/* If UTF16, we need to know if this is the first write or not to check for BOM */
	boolean_t	crlast;		/* If not M, the last character seen was a CR so if LF is next, ignore it */
	boolean_t	def_width;	/* WIDTH has not been changed */
	boolean_t	def_recsize;	/* RECORDSIZE has not been changed */
	unsigned int	lastop;		/* Last operation done on file */
	int		fildes;		/* File descriptor */
	int		bufsize;	/* Size of inbuf */
	int		outbufsize;	/* Size of outbuf */
	int4		recordsize;	/* Size of record in bytes */
	int4		padchar;	/* Character to use for padding */
	int		out_bytes;	/* Number of bytes output for this fixed record */
	uint4		bom_buf_cnt;	/* Count of bytes in BOM buffer */
	uint4		bom_buf_off;	/* Next available byte in BOM buffer */
	unsigned char	bom_buf[4];	/* Buffer area for BOM assembly */
	unsigned char	*inbuf;		/* Input buffer area */
	unsigned char	*inbuf_pos;	/* Put next char in inbuf here */
	unsigned char	*inbuf_off;	/* Next char to take from inbuf */
	unsigned char	*inbuf_top;	/* Last char (+1) in inbuf */
	unsigned char	*outbuf;	/* Output buffer area */
	FILE		*filstr;
}d_rm_struct;	/*  rms		*/

#ifdef __MVS__
#define NATIVE_NL	0x15		/* EBCDIC */
#else
#define NATIVE_NL	'\n'		/* ASCII */
#endif

int gtm_utf_bomcheck(io_desc *iod, gtm_chset_t *chset, unsigned char *buffer, int len);
int iorm_get_bom(io_desc *io_ptr);
int iorm_get(io_desc *io_ptr);
int iorm_write_utf_ascii(io_desc *iod, char *string, int len);
void iorm_write_utf(mstr *v);
void iorm_readfl_badchar(mval *vmvalptr, int datalen, int delimlen, unsigned char *delimptr, unsigned char *strend);

#endif
