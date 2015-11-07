/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define DEF_RM_WIDTH		32767
#define DEF_RM_LENGTH		66
#define MAX_RMS_RECORDSIZE	32767
#define MAX_RMS_UDF_RECORD	65535
#define MAX_RMS_ANSI_BLOCK	65635
#define MIN_RMS_ANSI_BLOCK	14
#define RMS_DISK_BLOCK		512
#define RMS_DEF_MBC		16
#define RMS_MAX_MBC		127

#define GTM_ACE_BIGREC		1	/* application flag */
#define GTM_ACE_FAC		0xF6	/* application facility */
#define GTM_ACE_LABEL		'GTMB'
#define GTM_ACE_LAB_OFF		2	/* ACE$K_LENGTH / sizeof uint4 */
#define GTM_ACE_RFM_OFF		3
#define GTM_ACE_MRS_OFF		4
#define GTM_ACE_SIZE		5

/* ***************************************************** */
/* *********** structure for RMS driver **************** */
/* ***************************************************** */

typedef struct
{
	struct RAB	r;
	struct FAB	f;
	unsigned int	l_mrs;		/* fab mrs */
	unsigned int	l_rsz;		/* rab rsz */
	unsigned int	l_usz;		/* rab usz */
	boolean_t	largerecord;
	unsigned short	promask;
	char		b_rfm;		/* logical fab rfm */
	uint4		bufsize;	/* size of buffers */
	char		*outbuf_start;	/* always real start of outbuf */
	char		*outbuf;
	char		*outbuf_pos;
	char		*outbuf_top;	/* smw 20031119 not used */
	char		*inbuf;
	char		*inbuf_pos;
	char		*inbuf_top;
	char		*block_buffer;	/* used for large records */
}d_rm_struct;	/*  rms		*/

int4 iorm_jbc(struct NAM *nam, mval *pp, mstr *que, bool delete);
