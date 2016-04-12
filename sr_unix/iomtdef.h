/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __IOMTDEF_H__
#define __IOMTDEF_H__

/* iomtdef.h UNIX - mag tape header file */

#define MAX_VARREC_SZ 9995
#define MAX_FIXREC_SZ 65535
#define MAX_REC_SZ 65535	/* maximum value that can be specified for the RECORDSIZE device parameter */
#define MAX_BLK_SZ 65535
/* 512 is Motorola restriction */
#define MIN_BLK_SZ 512
#define MIN_VARREC_SZ 5
#define MIN_FIXREC_SZ 1

enum mt_op
{
        mt_null, mt_write, mt_read, mt_eof, mt_eof2, mt_rewind, mt_tm, mt_tm2
};

#define MTDEF_BUF_SZ 1024
#define MTDEF_REC_SZ 1020
#define MTDEF_PG_WIDTH 255
#define DEF_MT_LENGTH 66

#define MT_RECHDRSIZ 4

#define MTLAB_DOS11 (1 << 0)
#define MTLAB_ANSI (1 << 1)

#define MTL_VOL1 (1 << 0)
#define MTL_HDR1 (1 << 1)
#define MTL_HDR2 (1 << 2)
#define MTL_EOF1 (1 << 3)
#define MTL_EOF2 (1 << 4)

#define ANSI_LAB_LENGTH 80
/* THESE DEFINITIONS ARE TAKEN FROM VMS SYS$LIBRARY IODEF.H	    */

/*                                                                          */
/* *** START LOGICAL I/O FUNCTION CODES ***                                 */
/*                                                                          */
#define IO_WRITEMARK 28                /*WRITE TAPE MARK                   */
#define IO_WRITELBLK 32                /*WRITE LOGICAL BLOCK               */
#define IO_READLBLK 33                 /*READ LOGICAL BLOCK                */
#define IO_WRITEOF 40                  /*WRITE END OF FILE                 */
#define IO_REWIND 36                   /*REWIND TAPE                       */
#define IO_SKIPFILE 37                 /*SKIP FILES                        */
#define IO_SKIPRECORD 38               /*SKIP RECORDS                      */

/*                                                                          */
/* FUNCTION MODIFIER BIT DEFINITIONS                                        */
/*                                                                          */

#define IO_M_DATACHECK 16384
#define IO_M_INHRETRY 32768
#define IO_M_INHEXTGAP 4096
#define IO_M_ERASE 1024

/* THESE DEFINITIONS ARE TAKEN FROM VMS SYS$LIBRARY MTDEF.H  */

#define MT_M_BOT 65536

/* Used to support Unix drives */
#define MT_M_READ 0
#define MT_M_WRITE 1
#define MT_BUSY 333
#define MT_TAPERROR 666

/* *************************************************************** */
/* *************  structure for the magtape ********************** */
/* *************************************************************** */

enum mt_type_info { mt_disk, mt_tape, mt_file };

typedef struct
{	enum mt_type_info mt_type;
	bool is_seekable;               /* Use lseek for all repositioning.
					 */
	bool filemarks_supported;       /* Filemarks are supported.
					 */
	bool req_extra_filemark;        /* Requires extra filemark for EOM.
					 */
        bool can_bksp_by_filemark;      /* Is capable of backspacing by
					 * filemarks.  A backspace leaves
					 * you at the end of the previous
					 * file.  FALSE implies that a
					 * backspace command leaves you at
					 * the BEGINNING of the previous file.
					 */
	int4 block_size;                /* minimum block size */
}m_cap;	/* magtape characteristics */

typedef struct
{
	int4		access_id;	/* channel to access magtape 	*/
	uint4	read_mask;
	uint4	write_mask;
	uint4	record_sz;
	uint4	block_sz;
	unsigned short	filepos;
	unsigned short	recpos;
	unsigned short	mode;
	char filename[64];
	unsigned char	*buffer;
	unsigned char	*bufftop;
	unsigned char	*buffptr;
	m_cap           cap;
	int		bufftoggle;
	bool		ebcdic;
	unsigned char	labeled;
	mstr		rec;
	enum mt_op	last_op;
	bool		newversion;
	bool		read_only;
	bool		wrap;
	bool		fixed;
	bool		stream;
} d_mt_struct;

int iomt_info(d_mt_struct *mt);
uint4 iomt_rdlblk(d_mt_struct *mt_ptr, uint4 mask, iosb *stat_blk, void *buff, int size);
uint4 iomt_wtlblk(uint4 channel, uint4 mask, iosb *stat_blk, void *buff, int size);
uint4 iomt_sense(d_mt_struct *mt, iosb *io_status_blk);
uint4 iomt_opensp(io_log_name *dev_name, d_mt_struct *mtdef);

#endif
