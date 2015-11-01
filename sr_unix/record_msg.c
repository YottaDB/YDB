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

#include "mdef.h"
#include "gtm_string.h"

#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include "record_msg.h"

#define MSG_RCD_CNT	500

#ifdef __MVS__
struct msgbuf
{
	long	mtype;
	char	mtext[1];
};
#endif

GBLDEF int	msgnow = 0;
GBLDEF int	msgnxt = 0;
GBLDEF int	msgcnt = 0;
GBLDEF struct
{
	time_t		time;		/* time of message */
	long		mtype;		/* message type - this use of long is in accordance with the "standard" for UNIX */
	int		msqid;		/* ID of message queue */
	int		msize;		/* size of message text */
	char		caller[8];	/* caller's name */
	char		text[8];	/* first 8 bytes of message text */
}	message_record[MSG_RCD_CNT];
GBLDEF int	dmn_flush[MSG_RCD_CNT];


void	record_msg(int msqid, struct msgbuf *msgp, int msgsz, char *caller)
{
	time_t	time_val;

	msgnow = msgnxt;
	msgcnt++;
	msgnxt = (msgnxt + 1)%MSG_RCD_CNT;

	message_record[msgnow].time  = time(&time_val);
	message_record[msgnow].mtype = msgp->mtype;
	message_record[msgnow].msqid = msqid;
	message_record[msgnow].msize = msgsz;
	strncpy(message_record[msgnow].caller, caller, sizeof(message_record[msgnow].caller));
	memcpy(message_record[msgnow].text, msgp->mtext, 8);
	dmn_flush[msgnow] = 0;

}


void	record_dmn_flush(int checkpoint)
{
	dmn_flush[msgnow] |= (1 << checkpoint);
}
