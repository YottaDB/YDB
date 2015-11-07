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

#define DEF_MSG_ARGS 16

typedef	struct msgstruct
{
	unsigned short	arg_cnt;	/* argument count (# longwords) */
	unsigned short	def_opts;	/* default message options	*/
	unsigned	msg_number;	/* message number		*/
	unsigned short	fp_cnt;		/* number of fao parameters	*/
	unsigned short	new_opts;	/* new message options		*/
	union
	{
		unsigned char	*cp;
		unsigned 	n;
	} fp[DEF_MSG_ARGS];			/* fao parameter		*/
} msgtype;

#define SHORT_MSG_SIZE (SIZEOF(msgtype) / SIZEOF(int4) - 5)
#define LONG_MSG_SIZE (SIZEOF(msgtype) / SIZEOF(int4) - 1)
#define MID_MSG_SIZE (SIZEOF(msgtype) / SIZEOF(int4) - 3)
#define FAO_ARG SIZEOF(int4)
#define MSG_PRINT(ARG_CNT, OPTS, NUM, FP_CNT, FP2, FP3, FP4, FP5)	\
{									\
	msg->arg_cnt = (ARG_CNT);					\
	msg->new_opts = msg->def_opts = (OPTS);				\
	msg->msg_number = (NUM);					\
	msg->fp_cnt = (FP_CNT);						\
	msg->fp[0].n = SIZEOF(gt_lit) - 1;				\
	msg->fp[1].cp = gt_lit;						\
	msg->fp[2].n = (FP2);						\
	msg->fp[3].n = (FP3);						\
	msg->fp[4].n = (FP4);						\
	msg->fp[5].n = (FP5);						\
	sys$putmsg(msg, 0, 0, 0);					\
}
