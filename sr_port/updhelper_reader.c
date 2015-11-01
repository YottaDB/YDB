/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"
#include "gtm_inet.h"

#include <sys/mman.h>
#include <errno.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "cdb_sc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "error.h"
#include "repl_dbg.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_shutdcode.h"
#include "gvcst_protos.h"

#ifdef VMS
#include <ssdef.h>
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <secdef.h>
#include <psldef.h>
#include <lckdef.h>
#include <syidef.h>
#include <xab.h>
#include <prtdef.h>
#endif
#include "op.h"
#include "targ_alloc.h"
#include "dpgbldir.h"
#include "repl_log.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "interlock.h"
#include "aswp.h"
#include "add_inter.h"
#include "read_db_files_from_gld.h"
#include "updproc.h"
#include "wcs_sleep.h"
#include "sleep_cnt.h"
#include "rel_quant.h"
#include "performcaslatchcheck.h"
#include "updproc_get_gblname.h"
#include "gtmmsg.h"

#ifdef REPL_DEBUG
#include "format_targ_key.h"
#include "util.h"
#endif

#define MAX_LCNT 100

GBLREF	void			(*call_on_signal)();
GBLREF	gv_namehead		*gv_target;
GBLREF	gv_key			*gv_currkey;
GBLREF  gv_key                  *gv_altkey;
GBLREF  gd_region               *gv_cur_region;
GBLREF  int4			gv_keysize;
GBLREF  gd_addr                 *gd_header;
GBLREF	uint4			process_id;	/* for SWAPLOCK */
GBLREF	recvpool_addrs		recvpool;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	int			updhelper_log_fd;
GBLREF	FILE			*updhelper_log_fp;
GBLREF	int			num_additional_processors;
#ifdef VMS
GBLREF  uint4                   image_count;
#endif
GBLREF boolean_t		disk_blk_read;
LITREF	int			jrt_update[JRT_RECTYPES];
static	uint4			last_pre_read_offset;

int updhelper_reader(void)
{
	uint4			pre_read_offset;
	int			lcnt;
	boolean_t		continue_reading;

	call_on_signal = updhelper_reader_sigstop;
	updhelper_init(UPD_HELPER_READER);
	repl_log(updhelper_log_fp, TRUE, TRUE, "Helper reader started. PID %d [0x%X]\n", process_id, process_id);
	gv_currkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_altkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_currkey->top = gv_altkey->top = gv_keysize;
	gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
	gv_altkey->base[0] = gv_currkey->base[0] = '\0';
	last_pre_read_offset = 0;
	continue_reading = TRUE;
	do
	{
		REPL_DPRINT1("Will wait\n");
		for (lcnt = 1; continue_reading && last_pre_read_offset ==
						(pre_read_offset = recvpool.upd_helper_ctl->pre_read_offset); )
		{
			SHORT_SLEEP(lcnt);
			if (lcnt <= MAX_LCNT)
				lcnt++;
			continue_reading = NO_SHUTDOWN == helper_entry->helper_shutdown;
		}
		last_pre_read_offset = pre_read_offset;
	} while (continue_reading && updproc_preread());
	updhelper_reader_end();
	return SS_NORMAL;
}

boolean_t updproc_preread(void)
{
	boolean_t		good_record, was_wrapped;
	uint4			pre_read_offset;
	int			rec_len, cnt, retries, spins, maxspins, key_len;
	enum jnl_record_type	rectype;
	mstr_len_t		val_len;
	mstr			mname;
	sm_uc_ptr_t		readaddrs;	/* start of current rec in pool */
	sm_uc_ptr_t		limit_readaddrs;
	jnl_record		*rec;
	jnl_string		*keystr;
	sgmnt_addrs 		*csa;
	sgmnt_data_ptr_t	csd;
	enum cdb_sc		status;
	gd_region               *reg, *r_top;
	char           		gv_mname[MAX_KEY_SZ];
	recvpool_ctl_ptr_t	recvpool_ctl;
	upd_proc_local_ptr_t	upd_proc_local;
	gtmrecv_local_ptr_t	gtmrecv_local;
	upd_helper_ctl_ptr_t	upd_helper_ctl;
#ifdef REPL_DEBUG
	unsigned char 		buff[MAX_ZWR_KEY_SZ], *end;
	uint4			write, write_wrap;
#endif
	error_def(ERR_DBCCERR);
	error_def(ERR_ERRCALL);
	maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
	upd_proc_local = recvpool.upd_proc_local;
	recvpool_ctl = recvpool.recvpool_ctl;
	gtmrecv_local = recvpool.gtmrecv_local;
	upd_helper_ctl = recvpool.upd_helper_ctl;
	csa = NULL;
	pre_read_offset = last_pre_read_offset;
	while (last_pre_read_offset == upd_helper_ctl->pre_read_offset)
	{
		if (NO_SHUTDOWN != helper_entry->helper_shutdown)
			return FALSE;
		if (GTMRECV_NO_RESTART != gtmrecv_local->restart)
		{
			last_pre_read_offset = 0;
			return TRUE;
		}
		if (recvpool_ctl->write == upd_proc_local->read)
		{
			REPL_DPRINT4("Wait: write==read::pre_read_offset = %x read = %x write = %x\n",
				pre_read_offset, upd_proc_local->read, recvpool_ctl->write);
			return TRUE;
		}
		for (retries = LOCK_TRIES - 1; 0 < retries; retries--)	/* - 1 so do rel_quant 3 times first */
		{
			for (spins = maxspins; 0 < spins; spins--)
			{
				if (GET_SWAPLOCK(&upd_helper_ctl->pre_read_lock))
					break;
				if (retries & 0x3)
					/* On all but every 4th pass, do a simple rel_quant */
					rel_quant();	/* Release processor to holder of lock (hopefully) */
				else
				{
					if (NO_SHUTDOWN != helper_entry->helper_shutdown)
						return FALSE;
					/* On every 4th pass, we bide for awhile */
					wcs_sleep(LOCK_SLEEP);
					if (RETRY_CASLATCH_CUTOFF == retries)
						performCASLatchCheck(&upd_helper_ctl->pre_read_lock, LOOP_CNT_SEND_WAKEUP);
				}
			}
			if (0 < spins)
				break;
		}
		if (0 == retries)
		{
			gtm_putmsg(VARLSTCNT(9) ERR_DBCCERR, 2, LIT_AND_LEN("Pre-reader"), ERR_ERRCALL, 3, CALLFROM);
			return FALSE;
		}
#ifdef REPL_DEBUG
		write_wrap = recvpool_ctl->write_wrap;
		write = recvpool_ctl->write;
#endif
		if (pre_read_offset >= recvpool_ctl->write_wrap)
		{
			REPL_DPRINT4("Wrapped: pre_read_offset = %x write_wrap = %x write = %x\n",
				pre_read_offset, write_wrap, write);
			pre_read_offset = 0;
		}
		if (pre_read_offset == recvpool_ctl->write && (!recvpool_ctl->wrapped))
		{	/* Should not pre-read beyond what is written */
			RELEASE_SWAPLOCK(&upd_helper_ctl->pre_read_lock);
			return TRUE;
		}
		if (pre_read_offset < upd_proc_local->read && !recvpool_ctl->wrapped)
		{
			REPL_DPRINT1("Falling behind\n");
			pre_read_offset = upd_proc_local->read;
			upd_helper_ctl->first_done = FALSE;
		}
		if (!upd_helper_ctl->first_done)
		{	/* First Pre-reader after pre_read_offset changed or
			 * if pre-readers are falling behind, we will come here to skip some records
			 * to avoid any contention with update process. */
			for (reg = gd_header->regions, r_top = reg + gd_header->n_regions; reg < r_top; reg++)
			{
				if (reg->open)
				{
					csa = &FILE_INFO(reg)->s_addrs;
					csd = csa->hdr;
					csa->nl->n_pre_read = csd->n_bts * (100.0 - csd->reserved_for_upd) /
								csd->avg_blks_per_100gbl;
				}
			}
			for (cnt = 0; cnt < SKIP_REC; cnt++)
			{	/* Skip a few records to avoid contention with update process */
				readaddrs = recvpool.recvdata_base + pre_read_offset;
				limit_readaddrs = recvpool.recvdata_base +
					(recvpool_ctl->wrapped ? recvpool_ctl->write_wrap : recvpool_ctl->write);
				if (limit_readaddrs - MIN_JNLREC_SIZE > readaddrs)
				{
					rec = (jnl_record *)readaddrs;
					rec_len = rec->prefix.forwptr;
					if (IS_VALID_RECTYPE(rec)
					    && (ROUND_DOWN2(rec_len, JNL_REC_START_BNDRY) == rec_len)
					    && (readaddrs + rec_len <= limit_readaddrs)
					    && (rec_len == REC_LEN_FROM_SUFFIX(readaddrs, rec_len))
		    			    && (pre_read_offset + rec_len > upd_proc_local->read))
						pre_read_offset += rec_len;
					else
						break;
				} else
					break;
			}
			REPL_DPRINT3("First one::pre_read_offset = %x read %x\n", pre_read_offset, upd_proc_local->read);
			upd_helper_ctl->first_done = TRUE;
		} else
		{
			pre_read_offset = upd_helper_ctl->next_read_offset;
			REPL_DPRINT3("Non-first::pre_read_offset = %x read %x\n", pre_read_offset, upd_proc_local->read);
			if (NULL != csa)
			{
				REPL_DPRINT2("Enough read::csa->nl->n_pre_read is %x\n", csa->nl->n_pre_read);
				if (0 >= csa->nl->n_pre_read) /* Can be negative for concurrent decrement */
				{
					RELEASE_SWAPLOCK(&upd_helper_ctl->pre_read_lock);
					return TRUE;
				}
			}
		}
		good_record = TRUE;
		readaddrs = recvpool.recvdata_base + pre_read_offset;
		limit_readaddrs = recvpool.recvdata_base +
			(recvpool_ctl->wrapped ? recvpool_ctl->write_wrap : recvpool_ctl->write);
		if (limit_readaddrs - MIN_JNLREC_SIZE > readaddrs)
		{
			rec = (jnl_record *)readaddrs;
			rec_len = rec->prefix.forwptr;
			if (IS_VALID_RECTYPE(rec)
			    && (ROUND_DOWN2(rec_len, JNL_REC_START_BNDRY) == rec_len)
			    && (readaddrs + rec_len <= limit_readaddrs)
			    && (rec_len == REC_LEN_FROM_SUFFIX(readaddrs, rec_len))
			    && (pre_read_offset + rec_len > upd_proc_local->read))
			{
				upd_helper_ctl->next_read_offset = pre_read_offset + rec_len;
			}
			else
				good_record = FALSE;
		} else
			good_record = FALSE;
		RELEASE_SWAPLOCK(&upd_helper_ctl->pre_read_lock);
		if (good_record)
		{
			rectype = rec->prefix.jrec_type;
			if (IS_SET_KILL_ZKILL(rectype))
			{
				was_wrapped = recvpool_ctl->wrapped;
				if (IS_ZTP(rectype))
					keystr = (jnl_string *)&rec->jrec_fset.mumps_node;
				else
					keystr = (jnl_string *)&rec->jrec_set.mumps_node;
				key_len = keystr->length;	/* local copy of shared recvpool key */
				if ((MAX_KEY_SZ >= key_len && 0 < key_len && 0 == keystr->text[key_len - 1]) &&
					(upd_good_record == updproc_get_gblname(keystr->text, key_len, gv_mname, &mname)))
				{
					gv_bind_name(gd_header, &mname);	/* this sets gv_target and gv_cur_region */
					if (!gv_target->root)
						continue;
					csa = &FILE_INFO(gv_cur_region)->s_addrs;/* we modify n_pre_read for the region we read
										    on our last try. This is done for
										    performance reasons so that n_pre_read
										    doesn't have to be an atomic counter */
					memcpy(gv_currkey->base, keystr->text, key_len);
					if ((readaddrs + rec_len > recvpool.recvdata_base + upd_proc_local->read
						&& !(was_wrapped && !recvpool_ctl->wrapped))
						&& (0 != gv_currkey->base[0]
						&&  0 == gv_currkey->base[key_len - 1]
						&&  0 == gv_currkey->base[mname.len]))
					{
						gv_currkey->base[key_len] = 0; 	/* second null of a key terminator */
						gv_currkey->end = key_len;
						disk_blk_read = FALSE;
						status = gvcst_search(gv_currkey, NULL); /* ignore return status,
											because pre-readers do not care */
						if (disk_blk_read)
							csa->nl->n_pre_read--;
#ifdef REPL_DEBUG
						if (NULL == (end = format_targ_key(buff,
									MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
							end = &buff[MAX_ZWR_KEY_SZ - 1];
						util_out_print(
							"readaddrs = !XL pre_read_offset = !XL write_wrap = !XL write = !XL",
							FALSE, readaddrs, pre_read_offset,
							recvpool_ctl->write_wrap, recvpool_ctl->write);
						util_out_print(
							" Seqno = 0x!16@XJ Rectype = !SL gv_currkey = !AD status = !SL",
							TRUE, &recvpool.recvpool_ctl->jnl_seqno,
							rectype, end - buff, buff, status);
#endif
					} else
					{
						REPL_DPRINT1("Unexpected bad record\n");
						good_record = FALSE;
					}
				} else
					good_record = FALSE;
			}
		}
		if (!good_record)
		{
#ifdef REPL_DEBUG
			REPL_DPRINT5("Skipping record: pre_read_offset = %x read = %x write_wrap = %x write = %x\n",
				pre_read_offset, upd_proc_local->read, write_wrap, write);
			REPL_DPRINT3("New values: write_wrap = %x write = %x\n",
					recvpool_ctl->write_wrap, recvpool_ctl->write);
#endif
			return TRUE;
		}
	} /* end while */
	return TRUE;
}
