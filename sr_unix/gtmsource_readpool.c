/****************************************************************
 *								*
 * Copyright (c) 2006-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"

#include <sys/time.h>
#include <errno.h>
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
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "repl_ctl.h"
#include "repl_errno.h"
#include "repl_dbg.h"
#include "memcoherency.h"
#include "repl_tr_good.h"
#include "min_max.h"
#include "repl_instance.h"

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	gtmsource_state_t	gtmsource_state;

int gtmsource_readpool(uchar_ptr_t buff, int *data_len, int maxbufflen, boolean_t read_multiple, qw_num stop_read_at)
{
	uint4 			jnldata_len, read_size, jnlpool_size, avail_data;
	uint4			first_tr_len, num_tr_read, orig_first_tr_len, orig_tr_len, tr_len;
	int64_t			read, wrap_size; /* This can be negative. Must handle a signed 4G value */
	uchar_ptr_t		buf_top, tr_p;
	jnlpool_ctl_ptr_t	jctl;
	gtmsource_local_ptr_t	gtmsource_local;
	sm_uc_ptr_t		jnldata_base;
	jnldata_hdr_ptr_t	jnl_header;
	qw_num			read_addr, avail_data_qw;
	seq_num			read_jnl_seqno, jnl_seqno, next_read_seqno, next_histinfo_seqno;
	jnl_record		*jnlrec;
	boolean_t		trim_this_seqno;
	uchar_ptr_t		trim_buff;

	jctl = jnlpool->jnlpool_ctl;
	jnlpool_size = jctl->jnlpool_size;
	DEBUG_ONLY(jnl_seqno = jctl->jnl_seqno;) /* jnl_seqno is used in an assert below. jnl_seqno is a local variable for
						  * debugging purposes since shared memory can change from the time the assert
						  * fails to the time the core gets created
						  */
	jnldata_base = jnlpool->jnldata_base;
	gtmsource_local = jnlpool->gtmsource_local;
	do
	{
		read = gtmsource_local->read;
		read_addr = gtmsource_local->read_addr;
		assert(stop_read_at > read_addr); /* there should be data to be read, if not how did we end up here? */
		read_jnl_seqno = gtmsource_local->read_jnl_seqno;
		assert(read_jnl_seqno <= gtmsource_local->next_histinfo_seqno);
		if (read_jnl_seqno == gtmsource_local->next_histinfo_seqno)
		{	/* Request a REPL_HISTREC message be sent first before sending any more seqnos across */
			gtmsource_state = gtmsource_local->gtmsource_state = GTMSOURCE_SEND_NEW_HISTINFO;
			return 0;
		}
		next_histinfo_seqno = gtmsource_local->next_histinfo_seqno;
		next_read_seqno = read_jnl_seqno;
		if (!jnlpool_hasnt_overflowed(jctl, jnlpool_size, read_addr))
			break; /* overflow happened, or about to happen */
		/* No overflow yet. Before we read the content (including the jnldata_len read below), we have to ensure
		 * we read up-to-date content. We rely on the memory barrier done in jnlpool_hasnt_overflowed for this.
		 */
		assert(read + SIZEOF(jnldata_hdr_struct) <= jnlpool_size);
		jnl_header = (jnldata_hdr_ptr_t)(jnldata_base + read);
		first_tr_len = jnldata_len = jnl_header->jnldata_len;
		if (read_multiple)
		{
			assert(stop_read_at >= read_addr);
			avail_data_qw = stop_read_at - read_addr;
			assert(maxbufflen <= MAXPOSINT4); /* to catch the case of change in type of maxbufflen */
			avail_data = (uint4)MIN(avail_data_qw, (qw_num)maxbufflen);
			assert(next_read_seqno < next_histinfo_seqno);
			read_multiple = (first_tr_len < avail_data) && ((next_read_seqno + 1) < next_histinfo_seqno);
			if (read_multiple)
				jnldata_len = avail_data;
		}
		if (SIZEOF(jnldata_hdr_struct) < jnldata_len && jnldata_len <= jnlpool_size)
		{
			read_size = jnldata_len - SIZEOF(jnldata_hdr_struct);
			if (0 < read_size && read_size <= maxbufflen)
			{
				if (0 < (wrap_size = ((int64_t)read - (jnlpool_size - jnldata_len))))
					read_size -= wrap_size;
				memcpy(buff, (sm_uc_ptr_t)jnl_header + SIZEOF(jnldata_hdr_struct), read_size);
				if (0 < wrap_size)
					memcpy(buff + read_size, jnldata_base, wrap_size);
				jnlrec = (jnl_record *)buff;
				/* Now that we have read the content, we have to ensure that we haven't read content
				 * that may have been overwritten. We rely on the memory barrier done in
				 * "jnlpool_hasnt_overflowed" for this. In addition, check for JRT_BAD (this is indication
				 * of a ERR_JNLPOOLRECOVERY situation) and if so switch to READ_FILE.
				 */
				if ((jnlpool_hasnt_overflowed(jctl, jnlpool_size, read_addr))
							&& (JRT_BAD != jnlrec->prefix.jrec_type))
				{	/* No overflow. Only now are we guaranteed a good value of "first_tr_len". */
					/* Check if this seqno was processed by "repl_phase2_salvage". In that case, it
					 * would contain just one NULL record so copy just that and ignore the rest of
					 * the reserved space (if any). Note that if the rest of the reserved space is
					 * 0 bytes long, then we don't need any special processing.
					 */
					first_tr_len -= SIZEOF(jnldata_hdr_struct);
					assert(0 == (first_tr_len % JNL_WRT_END_MODULUS));
					assert(first_tr_len >= NULL_RECLEN);
					orig_first_tr_len = first_tr_len;
					trim_buff = NULL;
					if ((JRT_NULL == jnlrec->prefix.jrec_type) && (NULL_RECLEN != first_tr_len))
					{
						first_tr_len = NULL_RECLEN;
						trim_buff = buff + NULL_RECLEN;
					}
#					ifdef REPL_DEBUG
					assert(repl_tr_good(buff, first_tr_len, read_jnl_seqno));
					num_tr_read = 1;
#					endif
					next_read_seqno++;
					assert(next_read_seqno <= next_histinfo_seqno);
					if (read_multiple)
					{	/* Although stop_read_at - read_addr contains no partial transaction, it
						 * is possible that stop_read_at - read_addr is more than maxbufflen, and
						 * hence we read fewer bytes than stop_read_at - read_addr; scan what we
						 * read to figure out if the tail is an incomplete transaction.
						 */
						assert((orig_first_tr_len + SIZEOF(jnldata_hdr_struct)) < jnldata_len);
							/* above must hold if multiple transactions were read */
						tr_p = buff + orig_first_tr_len;
						buf_top = buff + jnldata_len - SIZEOF(jnldata_hdr_struct);
						while (SIZEOF(jnldata_hdr_struct) < (buf_top - tr_p))
						{	/* more than hdr available */
							orig_tr_len = tr_len = ((jnldata_hdr_ptr_t)tr_p)->jnldata_len;
							assert(0 == (tr_len % JNL_WRT_END_MODULUS));
							assert(0 < tr_len);
							assert(tr_len <= jnlpool_size);
							if (tr_len <= (buf_top - tr_p)) /* transaction completely read */
							{
								jnlrec = (jnl_record *)(tr_p + SIZEOF(jnldata_hdr_struct));
								if (JRT_BAD == jnlrec->prefix.jrec_type)
								{	/* This is indication of a ERR_JNLPOOLRECOVERY situation
									 * and so we need to switch to READ_FILE for this seqno.
									 * Do that by stopping the READ_POOL until the seqno before
									 * this one. So break out of loop that reads more seqnos.
									 */
									break;
								}
								if ((JRT_NULL == jnlrec->prefix.jrec_type)
									&& ((NULL_RECLEN + SIZEOF(jnldata_hdr_struct)) != tr_len))
								{
									trim_this_seqno = TRUE;
									tr_len = NULL_RECLEN + REPL_MSG_HDRLEN;
								} else
									trim_this_seqno = FALSE;
								/* The message type and len assignments are a violation of
								 * layering; ideally, this should be done in
								 * gtmsource_process(), but we choose to do it here for
								 * performance reasons. If we have to do it in
								 * gtmsource_process(), we have to scan the buffer again.
								 */
								((repl_msg_ptr_t)tr_p)->type = REPL_TR_JNL_RECS;
								((repl_msg_ptr_t)tr_p)->len = tr_len;
#								ifdef REPL_DEBUG
								assert(repl_tr_good(tr_p + REPL_MSG_HDRLEN,
									tr_len - REPL_MSG_HDRLEN,
									read_jnl_seqno + num_tr_read));
								num_tr_read++;
#								endif
								if (NULL != trim_buff)
								{
									memmove(trim_buff, tr_p, tr_len);
									trim_buff += tr_len;
								} else if (trim_this_seqno)
									trim_buff = tr_p + tr_len;
								next_read_seqno++;
								tr_p += orig_tr_len;
								if (next_read_seqno >= next_histinfo_seqno)
								{	/* Don't read more than boundary of next histinfo */
									assert(next_read_seqno == next_histinfo_seqno);
									break;
								}
							} else
							{
								REPL_DPRINT5("Partial transaction read since jnldata_len"
									" %llu larger than maxbufflen %d, tr_len %d, "
									"remaining buffer %d\n", avail_data_qw, maxbufflen,
									tr_len, buf_top - tr_p);
								break;
							}
						}
#						ifdef REPL_DEBUG
						if (0 != (buf_top - tr_p))
						{
							REPL_DPRINT4("Partial tr header read since jnldata_len "
								"%llu larger than maxbufflen %d, incomplete header"
								" length %d\n", avail_data_qw, maxbufflen,
								buf_top - tr_p);
						}
#						endif
						jnldata_len = (uint4)((tr_p - buff) + SIZEOF(jnldata_hdr_struct));
						assert(0 == (jnldata_len % JNL_WRT_END_MODULUS));
						wrap_size = ((int64_t)read - (jnlpool_size - jnldata_len));
					}
					REPL_DPRINT4("Pool read seqno : "INT8_FMT" Num Tr read : %d Total Tr len : %d\n",
					       INT8_PRINT(read_jnl_seqno), num_tr_read, jnldata_len);
					REPL_DPRINT4("Read %u : Next read : %ld : %s\n", read,
						(0 > wrap_size) ? read + jnldata_len : wrap_size,
						(0 > wrap_size) ? "" : " READ WRAPPED");
					assert(next_read_seqno <= next_histinfo_seqno);
					/* Before sending the seqnos, check if a new histinfo got concurrently written */
					assert(gtmsource_local->next_histinfo_num <= gtmsource_local->num_histinfo);
					if ((gtmsource_local->next_histinfo_num == gtmsource_local->num_histinfo)
						&& (gtmsource_local->num_histinfo != jnlpool->repl_inst_filehdr->num_histinfo))
					{	/* We are sending seqnos of the last histinfo (that is open-ended) and
						 * there has been at least one histinfo concurrently added to this instance
						 * file compared to what is in our private memory. Set the next histinfo's
						 * start_seqno and redo the read with the new "next_histinfo_seqno".
						 */
						assert(MAX_SEQNO == gtmsource_local->next_histinfo_seqno);
						gtmsource_set_next_histinfo_seqno(TRUE);
							/* Set the next histinfo's start_seqno and redo the read */
						if (GTMSOURCE_WAITING_FOR_CONNECTION == gtmsource_state)
						{	/* Connection reset in "gtmsource_set_next_histinfo_seqno" */
							return 0;
						}
						continue;
					}
					read = ((0 > wrap_size) ? read + jnldata_len : wrap_size);
					read_addr += jnldata_len;
					read_jnl_seqno = next_read_seqno;
					assert(read_jnl_seqno <= gtmsource_local->next_histinfo_seqno);
					assert(stop_read_at >= read_addr);
					assert(jnl_seqno >= read_jnl_seqno - 1);
					/* In the rare case when we read the transaction read_jnl_seqno just as
					 * it becomes available and before the GTM process that wrote it updates
					 * jctl->jnl_seqno in t_end/tp_tend, we may return from this function
					 * with read_jnl_seqno one more than jctl->jnl_seqno. This is such a rare
					 * case that we don't want to add a wait loop for jctl->jnl_seqno to become
					 * equal to read_jnl_seqno. We expect that by the time we send the just read
					 * transaction(s) using socket I/O, jctl->jnl_seqno would have been updated.
					 * In any case, we prevent ourselves from misinterpreting this condition when
					 * read_jnl_seqno is compared against jctl->jnl_seqno in gtmsource_process(),
					 * gtmsource_get_jnlrecs() and gtmsource_showbacklog().
					 */
					assert(read == read_addr % jnlpool_size);
					gtmsource_local->read = read;
					gtmsource_local->read_addr = read_addr;
					gtmsource_local->read_jnl_seqno = read_jnl_seqno;
					*data_len = first_tr_len;
					if (NULL != trim_buff)
						jnldata_len = (uint4)((trim_buff - buff) + SIZEOF(jnldata_hdr_struct));
					return (jnldata_len);
				}
			} else if (0 < read_size && jnlpool_hasnt_overflowed(jctl, jnlpool_size, read_addr))
			{ /* Buffer cannot accommodate data */
				*data_len = read_size;
				return (-1);
			} /* else
			   * We read a corrupt (overwritten) large value, or read_size == 0, both of which imply overflow.
			   * read_size == 0 => overflow because every transaction generates non-zero bytes of jnl data */
		} /* else
		   * We read a corrupt (overwritten) large value, or read 0, both of which imply overflow.
		   * jnldata_len == 0 => overflow because every transaction generates non-zero bytes of jnl data */
		break;
	} while (TRUE);
	*data_len = -1;
	return (-1); /* Error indication */
}
