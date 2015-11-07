/****************************************************************
 *								*
 *	Copyright 2006, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtm_time.h"
#include <sys/time.h>
#include <errno.h>
#include "gtm_inet.h"	/* Required for gtmsource.h */
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_comm.h"
#include "repl_dbg.h"
#include "repl_log.h"
#include "repl_errno.h"
#include "iosp.h"
#include "gt_timer.h"
#include "gtmsource_heartbeat.h"
#include "relqop.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	int			gtmsource_sock_fd;
GBLREF	boolean_t		gtmsource_logstats;
GBLREF	int			gtmsource_log_fd;
GBLREF 	FILE			*gtmsource_log_fp;
GBLREF  gtmsource_state_t       gtmsource_state;
GBLREF	gd_addr          	*gd_header;

GBLDEF	boolean_t			heartbeat_stalled = TRUE;
GBLDEF	repl_heartbeat_que_entry_t	*repl_heartbeat_que_head = NULL;
GBLDEF	repl_heartbeat_que_entry_t	*repl_heartbeat_free_head = NULL;
GBLDEF	volatile time_t			gtmsource_now;
GBLDEF	time_t				last_sent_time, earliest_sent_time;

error_def(ERR_REPLCOMM);
error_def(ERR_TEXT);

static	int				heartbeat_period, heartbeat_max_wait;

void gtmsource_heartbeat_timer(TID tid, int4 interval_len, int *interval_ptr)
{
	assert(0 != gtmsource_now);
	UNIX_ONLY(assert(*interval_ptr == heartbeat_period);)	/* interval_len and interval_ptr are dummies on VMS */
	gtmsource_now += heartbeat_period;			/* cannot use *interval_ptr on VMS */
	REPL_DPRINT2("Starting heartbeat timer with %d s\n", heartbeat_period);
	start_timer((TID)gtmsource_heartbeat_timer, heartbeat_period * 1000, gtmsource_heartbeat_timer, SIZEOF(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
}

int gtmsource_init_heartbeat(void)
{
	int				num_q_entries;
	repl_heartbeat_que_entry_t	*heartbeat_element;

	assert(NULL == repl_heartbeat_que_head);

	heartbeat_period = jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD];
	heartbeat_max_wait = jnlpool.gtmsource_local->connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT];
	num_q_entries = DIVIDE_ROUND_UP(heartbeat_max_wait, heartbeat_period) + 2;
	REPL_DPRINT4("Initialized heartbeat, heartbeat_period = %d s, heartbeat_max_wait = %d s, num_q_entries = %d\n",
			heartbeat_period, heartbeat_max_wait, num_q_entries);
	if (!(repl_heartbeat_que_head = (repl_heartbeat_que_entry_t *)malloc(num_q_entries * SIZEOF(repl_heartbeat_que_entry_t))))
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			LEN_AND_LIT("Error in allocating heartbeat queue"), errno);

	memset(repl_heartbeat_que_head, 0, num_q_entries * SIZEOF(repl_heartbeat_que_entry_t));
	repl_heartbeat_free_head = repl_heartbeat_que_head + 1;
	*(time_t *)&repl_heartbeat_que_head->heartbeat.ack_time[0] = 0;
	*(time_t *)&repl_heartbeat_free_head->heartbeat.ack_time[0] = 0;
	for (heartbeat_element = repl_heartbeat_free_head + 1, num_q_entries -= 2;
	     num_q_entries > 0;
	     num_q_entries--, heartbeat_element++)
	{
		insqt((que_ent_ptr_t)heartbeat_element, (que_ent_ptr_t)repl_heartbeat_free_head);
	}
	last_sent_time = gtmsource_now = time(NULL);
	/* Ideally, we should use the Greatest Common Factor of heartbeat_period and GTMSOURCE_LOGSTATS_INTERVAL as the time keeper
	 * interval. As it stands now, we may not honor GTMSOURCE_LOGSTATS_INTERVAL if user specifies a heartbeat value
	 * larger than GTMSOURCE_LOGSTATS_INTERVAL. When we make GTMSOURCE_LOGSTATS_INTERVAL a user configurable parameter,
	 * this code may have to be revisited. Also, modify the check in gtmsource_process (prev_now != (save_now = gtmsource_now))
	 * to be something like (hearbeat_period < difftime((save_now = gtmsource_now), prev_now)). Vinaya 2003, Sep 08
	 */
	start_timer((TID)gtmsource_heartbeat_timer, heartbeat_period * 1000, gtmsource_heartbeat_timer, SIZEOF(heartbeat_period),
			&heartbeat_period); /* start_timer expects time interval in milli seconds, heartbeat_period is in seconds */
	heartbeat_stalled = FALSE;
	earliest_sent_time = 0;
	return (SS_NORMAL);
}

int gtmsource_stop_heartbeat(void)
{
	cancel_timer((TID)gtmsource_heartbeat_timer);
	if (NULL != repl_heartbeat_que_head)
		free(repl_heartbeat_que_head);
	repl_heartbeat_que_head = NULL;
	repl_heartbeat_free_head = NULL;
	last_sent_time = 0;
	earliest_sent_time = 0;
	gtmsource_now = 0;
	heartbeat_stalled = TRUE;
	REPL_DPRINT1("Stopped heartbeat\n");
	return (SS_NORMAL);
}

boolean_t gtmsource_is_heartbeat_overdue(time_t *now, repl_heartbeat_msg_t *overdue_heartbeat)
{

	repl_heartbeat_que_entry_t	*heartbeat_element;
	double				time_elapsed;
	unsigned char			seq_num_str[32], *seq_num_ptr;

#ifndef REPL_DISABLE_HEARTBEAT
	if (0 == earliest_sent_time ||
	    (time_elapsed = difftime(*now, earliest_sent_time)) <= (double)heartbeat_max_wait)
		return (FALSE);

	heartbeat_element = (repl_heartbeat_que_entry_t *)remqh((que_ent_ptr_t)repl_heartbeat_que_head);
	if (NULL == heartbeat_element)
	{
		assert(FALSE);
		return (FALSE);
	}

	memcpy(overdue_heartbeat, &heartbeat_element->heartbeat, SIZEOF(repl_heartbeat_msg_t));

	REPL_DPRINT5("Overdue heartbeat - SEQNO : "INT8_FMT" time : %ld now : %ld difftime : %00.f\n",
		     INT8_PRINT(*(seq_num *)&overdue_heartbeat->ack_seqno[0]), *(time_t *)&overdue_heartbeat->ack_time[0], *now,
		     time_elapsed);

	insqt((que_ent_ptr_t)heartbeat_element, (que_ent_ptr_t)repl_heartbeat_free_head);

	return (TRUE);
#else
	return (FALSE);
#endif
}

int gtmsource_send_heartbeat(time_t *now)
{
	repl_heartbeat_que_entry_t	*heartbeat_element;
	unsigned char			*msg_ptr;				/* needed for REPL_{SEND,RECV}_LOOP */
	int				tosend_len, sent_len, sent_this_iter;	/* needed for REPL_SEND_LOOP */
	int				status;					/* needed for REPL_{SEND,RECV}_LOOP */
	unsigned char			seq_num_str[32], *seq_num_ptr;


	heartbeat_element = (repl_heartbeat_que_entry_t *)remqh((que_ent_ptr_t)repl_heartbeat_free_head);
	if (NULL == heartbeat_element) /* Too many pending heartbeats, send later */
		return (SS_NORMAL);

	QWASSIGN(*(seq_num *)&heartbeat_element->heartbeat.ack_seqno[0], jnlpool.jnlpool_ctl->jnl_seqno);
	*(time_t *)&heartbeat_element->heartbeat.ack_time[0] = *now;

	heartbeat_element->heartbeat.type = REPL_HEARTBEAT;
	heartbeat_element->heartbeat.len = MIN_REPL_MSGLEN;
	REPL_SEND_LOOP(gtmsource_sock_fd, &heartbeat_element->heartbeat, MIN_REPL_MSGLEN, REPL_POLL_NOWAIT)
	{
		gtmsource_poll_actions(FALSE);  /* Recursive call */
		if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state ||
		    GTMSOURCE_CHANGING_MODE == gtmsource_state)
			return (SS_NORMAL);
	}

	if (SS_NORMAL == status)
	{
		insqt((que_ent_ptr_t)heartbeat_element, (que_ent_ptr_t)repl_heartbeat_que_head);
		last_sent_time = *now;
		if (0 == earliest_sent_time)
			earliest_sent_time = last_sent_time;

		REPL_DPRINT4("HEARTBEAT sent with time %ld SEQNO "INT8_FMT" at %ld\n",
			     *(time_t *)&heartbeat_element->heartbeat.ack_time[0],
			     INT8_PRINT(*(seq_num *)&heartbeat_element->heartbeat.ack_seqno[0]), time(NULL));

		return (SS_NORMAL);
	}

	if (EREPL_SEND == repl_errno && REPL_CONN_RESET(status))
	{
		repl_log(gtmsource_log_fp, TRUE, TRUE, "Connection reset while attempting to send heartbeat. Status = %d ; %s\n",
				status, STRERROR(status));
		repl_close(&gtmsource_sock_fd);
		gtmsource_state = GTMSOURCE_WAITING_FOR_CONNECTION;
		return (SS_NORMAL);
	}
	if (EREPL_SEND == repl_errno)
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			LEN_AND_LIT("Error sending HEARTBEAT message. Error in send"), status);

	if (EREPL_SELECT == repl_errno)
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			LEN_AND_LIT("Error sending HEARTBEAT message. Error in select"), status);

	GTMASSERT;
}

int gtmsource_process_heartbeat(repl_heartbeat_msg_t *heartbeat_msg)
{
	repl_heartbeat_que_entry_t	*heartbeat_element;
	seq_num				ack_seqno;
	gd_region			*reg, *region_top;
	sgmnt_addrs			*csa;
	unsigned char			seq_num_str[32], *seq_num_ptr;

	QWASSIGN(ack_seqno, *(seq_num *)&heartbeat_msg->ack_seqno[0]);

	REPL_DPRINT4("HEARTBEAT received with time %ld SEQNO "INT8_FMT" at %ld\n",
		     *(time_t *)&heartbeat_msg->ack_time[0], INT8_PRINT(ack_seqno), time(NULL));

	for (heartbeat_element = (repl_heartbeat_que_entry_t *)remqh((que_ent_ptr_t)repl_heartbeat_que_head);
	     NULL !=  heartbeat_element&&
	     *(time_t *)&heartbeat_msg->ack_time[0] >= earliest_sent_time;
	     heartbeat_element = (repl_heartbeat_que_entry_t *)remqh((que_ent_ptr_t)repl_heartbeat_que_head))
	{
		insqt((que_ent_ptr_t)heartbeat_element, (que_ent_ptr_t)repl_heartbeat_free_head);
		earliest_sent_time =
			*(time_t *)&((repl_heartbeat_que_entry_t *)
			((unsigned char *)repl_heartbeat_que_head + repl_heartbeat_que_head->que.fl))->heartbeat.ack_time[0];
	}

	if (NULL != heartbeat_element)
		insqh((que_ent_ptr_t)heartbeat_element, (que_ent_ptr_t)repl_heartbeat_que_head);

	return (SS_NORMAL);
}
