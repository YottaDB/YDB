/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <netinet/in.h>
#ifdef VMS
#include <ssdef.h>
#include <psldef.h>
#include <descrip.h>
#endif

#include "gtm_inet.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "gdsbml.h"
#include "gdskill.h"
#include "copy.h"
#include "hashdef.h"
#include "interlock.h"
#include "jnl.h"
#include "probe.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "io.h"
#include "gtmsecshr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "is_proc_alive.h"
#ifdef UNIX
#include "aswp.h"
#endif
#include "util.h"
#include "compswap.h"
#ifdef UNIX
#include "mutex.h"
#endif
#include "sec_shr_blk_build.h"
#include "sec_shr_map_build.h"
#include "add_inter.h"

#define FLUSH 1

/* SECSHR_ACCOUNTING macro assumes csd is dereferencible and uses "csa", "csd" and "is_bg" */
#define		SECSHR_ACCOUNTING(value)						\
{											\
	if (csa->read_write || is_bg)							\
	{										\
		if (csd->secshr_ops_index < sizeof(csd->secshr_ops_array))		\
			csd->secshr_ops_array[csd->secshr_ops_index] = (uint4)(value);	\
		csd->secshr_ops_index++;						\
	}										\
}

/* IMPORTANT : SECSHR_PROBE_REGION sets csa */
#define	SECSHR_PROBE_REGION(reg)									\
	if (!GTM_PROBE(sizeof(gd_region), (reg), READ))							\
		continue; /* would be nice to notify the world of a problem but where and how?? */	\
	if (!reg->open || reg->was_open)								\
		continue;										\
	if (!GTM_PROBE(sizeof(gd_segment), (reg)->dyn.addr, READ))					\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	if ((dba_bg != (reg)->dyn.addr->acc_meth) && (dba_mm != (reg)->dyn.addr->acc_meth))		\
		continue;										\
	if (!GTM_PROBE(sizeof(file_control), (reg)->dyn.addr->file_cntl, READ))				\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	if (!GTM_PROBE(sizeof(GDS_INFO), (reg)->dyn.addr->file_cntl->file_info, READ))			\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	csa = &(FILE_INFO((reg)))->s_addrs;								\
	if (!GTM_PROBE(sizeof(sgmnt_addrs), csa, WRITE))						\
		continue; /* would be nice to notify the world of a problem but where and how? */	\
	assert(reg->read_only && !csa->read_write || !reg->read_only && csa->read_write);

#ifdef UNIX
#  ifdef DEBUG_CHECK_LATCH
#   define CHECK_LATCH(X) {								\
	                          uint4 pid;						\
				  if ((pid = (X)->latch_pid) == process_id)		\
				  {							\
					  SET_LATCH_GLOBAL(X, LOCK_AVAILABLE);		\
					  util_out_print("Latch cleaned up", FLUSH);	\
				  } else if (0 != pid && FALSE == is_proc_alive(pid, 0))\
                                  {							\
                                          util_out_print("Orphaned latch cleaned up", TRUE); \
					  compswap((X), pid, LOCK_AVAILABLE);		\
                                  }							\
                          }
#else
#   define CHECK_LATCH(X) {								\
	                          uint4 pid;						\
				  if ((pid = (X)->latch_pid) == process_id)		\
				  {							\
					  SET_LATCH_GLOBAL(X, LOCK_AVAILABLE);		\
				  }							\
				  else if (0 != pid && FALSE == is_proc_alive(pid, 0))	\
					  compswap((X), pid, LOCK_AVAILABLE);		\
                          }
#endif
GBLREF uint4		process_id;
#else
#   define CHECK_LATCH(X)
#endif

GBLDEF gd_addr		*(*get_next_gdr_addrs)();
GBLDEF cw_set_element	*cw_set_addrs;
GBLDEF sgm_info		**first_sgm_info_addrs;
GBLDEF unsigned char	*cw_depth_addrs;
GBLDEF uint4		rundown_process_id;
GBLDEF int4		rundown_os_page_size;
GBLDEF gd_region	**jnlpool_reg_addrs;

#ifdef UNIX
GBLREF short		crash_count;
GBLREF node_local_ptr_t	locknl;
#endif

bool secshr_tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1);

void secshr_db_clnup(boolean_t termination_mode)
{
	unsigned char		*chain_ptr;
	boolean_t		is_bg, jnlpool_reg, tp_update_underway;
	int			max_bts;
	unsigned int		lcnt;
	cache_rec_ptr_t		clru, cr, cr_top, start_cr;
	cw_set_element		*cs, *cs_ptr, *cs_top, *first_cw_set, *nxt, *orig_cs;
	gd_addr			*gd_header;
	gd_region		*reg, *reg_top;
	jnl_buffer_ptr_t	jbp;
	off_chain		chain;
	sgm_info		*si;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		blk_ptr;

	if (NULL == get_next_gdr_addrs)
		return;
	tp_update_underway = FALSE;
	if (GTM_PROBE(sizeof(first_sgm_info_addrs), first_sgm_info_addrs, READ))
	{
		for (si = *first_sgm_info_addrs;  NULL != si;  si = si->next_sgm_info)
		{
			if (GTM_PROBE(sizeof(sgm_info), si, READ))
			{
				if (GTM_PROBE(sizeof(cw_set_element), si->first_cw_set, READ))
				{	/* Note that SECSHR_PROBE_REGION does a "continue" if any probes fail. */
					SECSHR_PROBE_REGION(si->gv_cur_region);	/* sets csa */
					if (csa->t_commit_crit || gds_t_committed == si->first_cw_set->mode)
						tp_update_underway = TRUE;
					break;
				}
			} else
				break;
		}
	}
	for (gd_header = (*get_next_gdr_addrs)(NULL);  NULL != gd_header;  gd_header = (*get_next_gdr_addrs)(gd_header))
	{
		if (GTM_PROBE(sizeof(gd_addr), gd_header, READ))
		{
			for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions;  reg < reg_top;  reg++)
			{
				SECSHR_PROBE_REGION(reg);	/* SECSHR_PROBE_REGION sets csa */
				csd = csa->hdr;
				if (!GTM_PROBE(sizeof(sgmnt_data), csd, WRITE))
					continue; /* would be nice to notify the world of a problem but where and how? */
				is_bg = (csd->acc_meth == dba_bg);
				if (csa->read_write || is_bg)		/* cannot update csd if MM and read-only */
					csd->secshr_ops_index = 0;	/* start accounting otherwise */
				SECSHR_ACCOUNTING(3);	/* 3 is the number of arguments following including self */
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING(rundown_process_id);
				if (csa->ti != &csd->trans_hist)
				{
					SECSHR_ACCOUNTING(4);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(csa->ti);
					SECSHR_ACCOUNTING(&csd->trans_hist);
					csa->ti = &csd->trans_hist;	/* better to correct and proceed than to stop */
				}
				SECSHR_ACCOUNTING(3);	/* 3 is the number of arguments following including self */
				SECSHR_ACCOUNTING(__LINE__);
				SECSHR_ACCOUNTING(csa->ti->curr_tn);
				CHECK_LATCH(&csa->backup_buffer->backup_ioinprog_latch);
                                VMS_ONLY(
                                        if (csa->backup_buffer->backup_ioinprog_latch.latch_pid == rundown_process_id)
                                                bci(&csa->backup_buffer->backup_ioinprog_latch.latch_word);
                                )
				if (GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE))
				{
#ifdef UNIX
					/* If we hold any latches in the node_local area, release them. Note we do not check
					   db_latch here because it is never used by the compare and swap logic but rather
					   the aswp logic. Since it is only used for the 3 state cache record lock and
					   separate recovery exists for it, we do not do anything with it here.
					*/
					CHECK_LATCH(&csa->nl->wc_var_lock);
#endif
					if (ABNORMAL_TERMINATION == termination_mode)
					{
						if (csa->timer)
						{
							if (-1 < csa->nl->wcs_timers) /* private flag is optimistic: dont overdo */
								CAREFUL_DECR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);
							csa->timer = FALSE;
						}
						if (csa->read_write && csa->ref_cnt)
						{
							assert(0 < csa->nl->ref_cnt);
							csa->ref_cnt--;
							assert(!csa->ref_cnt);
							CAREFUL_DECR_CNT(&csa->nl->ref_cnt, &csa->nl->wc_var_lock);
						}
					}
					if ((csa->in_wtstart) && (0 < csa->nl->in_wtstart))
						CAREFUL_DECR_CNT(&csa->nl->in_wtstart, &csa->nl->wc_var_lock);
					csa->in_wtstart = FALSE;	/* Let wcs_wtstart run for exit processing */
					if (csa->nl->wcsflu_pid == rundown_process_id)
						csa->nl->wcsflu_pid = 0;
				}
				if (is_bg)
				{
					if ((0 == reg->sec_size) || !GTM_PROBE(reg->sec_size, csa->nl, WRITE))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(reg->sec_size);
						continue;
					}
					CHECK_LATCH(&csa->acc_meth.bg.cache_state->cacheq_active.latch);
					start_cr = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
					max_bts = csd->n_bts;
					if (!GTM_PROBE(max_bts * sizeof(cache_rec), start_cr, WRITE))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(start_cr);
						continue;
					}
					cr_top = start_cr + max_bts;
					for (cr = start_cr;  cr < cr_top;  cr++)
					{	/* walk write cache looking for incomplete writes and reads issued by self */
						VMS_ONLY(
							if ((0 == cr->iosb[0]) && (cr->epid == rundown_process_id))
								cr->wip_stopped = TRUE;
						)
						CHECK_LATCH(&cr->rip_latch);
						assert(rundown_process_id);
						if ((cr->r_epid == rundown_process_id) && (0 == cr->dirty)
								&& (FALSE == cr->in_cw_set))
						{
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							cr->blk = CR_BLKEMPTY;
							/* don't mess with ownership the I/O may not yet be cancelled; ownership
							 * will be cleared by whoever gets stuck waiting for the buffer */
						}
					}
				}
				first_cw_set = cs = NULL;
				if (tp_update_underway)
				{	/* this is contructed to deal with the issue of reg != si->gv_cur_region
					 * due to the possibility of multiple global directories pointing to regions
					 * that resolve to the same physical file; was_open prevents processing the segment
					 * more than once, so this code matches on the file rather than the region to make sure
					 * that it gets processed at least once */
					for (si = *first_sgm_info_addrs;  NULL != si;  si = si->next_sgm_info)
					{
						if (!GTM_PROBE(sizeof(sgm_info), si, READ))
						{
							SECSHR_ACCOUNTING(3);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(si);
							break;
						} else if (!GTM_PROBE(sizeof(gd_region), si->gv_cur_region, READ))
						{
							SECSHR_ACCOUNTING(3);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(si->gv_cur_region);
							continue;
						} else if (!GTM_PROBE(sizeof(gd_segment), si->gv_cur_region->dyn.addr, READ))
						{
							SECSHR_ACCOUNTING(3);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(si->gv_cur_region->dyn.addr);
							continue;
						} else if (si->gv_cur_region->dyn.addr->file_cntl == reg->dyn.addr->file_cntl)
						{
							cs = si->first_cw_set;
							if (cs && GTM_PROBE(sizeof(cw_set_element), cs, READ))
							{
								while (cs->high_tlevel)
								{
									if (GTM_PROBE(sizeof(cw_set_element),
												cs->high_tlevel, READ))
										cs = cs->high_tlevel;
									else
									{
										SECSHR_ACCOUNTING(3);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(cs->high_tlevel);
										first_cw_set = cs = NULL;
										break;
									}
								}
							}
							first_cw_set = cs;
							break;
						}
					}
					if (NULL == si)
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
					}
				} else if (csa->t_commit_crit)
				{
					if (!GTM_PROBE(sizeof(unsigned char), cw_depth_addrs, READ))
					{
						SECSHR_ACCOUNTING(3);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(cw_depth_addrs);
					} else
					{
						first_cw_set = cs = cw_set_addrs;
						cs_top = cs + *cw_depth_addrs;
					}
				}
				if (NULL != first_cw_set)
				{
					if (is_bg)
					{
						clru = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, csa->nl->cur_lru_cache_rec_off);
						lcnt = 0;
					}
					if (csa->t_commit_crit)
						csd->trans_hist.free_blocks = csa->prev_free_blks;
					SECSHR_ACCOUNTING(tp_update_underway ? 5 : 6);
					SECSHR_ACCOUNTING(__LINE__);
					SECSHR_ACCOUNTING(first_cw_set);
					SECSHR_ACCOUNTING(tp_update_underway);
					if (!tp_update_underway)
					{
						SECSHR_ACCOUNTING(cs_top);
						SECSHR_ACCOUNTING(*cw_depth_addrs);
					} else
						SECSHR_ACCOUNTING(si->cw_set_depth);
					for (; (tp_update_underway  &&  NULL != cs) || (!tp_update_underway  &&  cs < cs_top);
						cs = tp_update_underway ? orig_cs->next_cw_set : (cs + 1))
					{
						if (tp_update_underway)
						{
							orig_cs = cs;
							if (cs && GTM_PROBE(sizeof(cw_set_element), cs, READ))
							{
								while (cs->high_tlevel)
								{
									if (GTM_PROBE(sizeof(cw_set_element),
												cs->high_tlevel, READ))
										cs = cs->high_tlevel;
									else
									{
										SECSHR_ACCOUNTING(3);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(cs->high_tlevel);
										cs = NULL;
										break;
									}
								}
							}
						}
						if (!GTM_PROBE(sizeof(cw_set_element), cs, READ))
						{
							SECSHR_ACCOUNTING(3);
							SECSHR_ACCOUNTING(__LINE__);
							SECSHR_ACCOUNTING(cs);
							break;
						}
						if (csa->t_commit_crit && 0 != cs->reference_cnt)
							csd->trans_hist.free_blocks -= cs->reference_cnt;
						if ((gds_t_committed == cs->mode) || (gds_t_write_root == cs->mode))
							continue;	/* already processed */
						if (is_bg)
						{
							for (; lcnt++ < max_bts;)
							{	/* find any available cr */
								if (clru++ >= cr_top)
									clru = start_cr;
								if (((0 == clru->dirty) && (FALSE == clru->in_cw_set))
									&& (-1 == clru->read_in_progress)
									&& (GTM_PROBE(csd->blk_size,
									GDS_ANY_REL2ABS(csa, clru->buffaddr), WRITE)))
										break;
							}
							if (lcnt >= max_bts)
							{
								SECSHR_ACCOUNTING(9);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING(cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(cs->tn);
								SECSHR_ACCOUNTING(cs->level);
								SECSHR_ACCOUNTING(cs->done);
								SECSHR_ACCOUNTING(cs->forward_process);
								SECSHR_ACCOUNTING(cs->first_copy);
								continue;
							}
							cr = clru++;
							cr->cycle++;	/* increment cycle for blk number changes (for tp_hist) */
							cr->blk = cs->blk;
							cr->jnl_addr = cs->jnl_freeaddr;
							cr->stopped = TRUE;
						        blk_ptr = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr);
						} else
						{
							blk_ptr = (sm_uc_ptr_t)csa->acc_meth.mm.base_addr + csd->blk_size * cs->blk;
							if (!GTM_PROBE(csd->blk_size, blk_ptr, WRITE))
							{
								SECSHR_ACCOUNTING(7);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING(cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(blk_ptr);
								SECSHR_ACCOUNTING(csd->blk_size);
								SECSHR_ACCOUNTING(csa->acc_meth.mm.base_addr);
								continue;
							}
						}
						if (cs->mode == gds_t_writemap)
						{
							if (!GTM_PROBE(csd->blk_size, cs->old_block, READ))
							{
								SECSHR_ACCOUNTING(11);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING(cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(cs->tn);
								SECSHR_ACCOUNTING(cs->level);
								SECSHR_ACCOUNTING(cs->done);
								SECSHR_ACCOUNTING(cs->forward_process);
								SECSHR_ACCOUNTING(cs->first_copy);
								SECSHR_ACCOUNTING(cs->old_block);
								SECSHR_ACCOUNTING(csd->blk_size);
								continue;
							}
							memmove(blk_ptr, cs->old_block, csd->blk_size);
							if (FALSE == sec_shr_map_build((uint4*)cs->upd_addr, blk_ptr, cs,
								csa->ti->curr_tn, BM_SIZE(csd->bplmap)))
							{
								SECSHR_ACCOUNTING(11);
								SECSHR_ACCOUNTING(__LINE__);
								SECSHR_ACCOUNTING(cs);
								SECSHR_ACCOUNTING(cs->blk);
								SECSHR_ACCOUNTING(cs->tn);
								SECSHR_ACCOUNTING(cs->level);
								SECSHR_ACCOUNTING(cs->done);
								SECSHR_ACCOUNTING(cs->forward_process);
								SECSHR_ACCOUNTING(cs->first_copy);
								SECSHR_ACCOUNTING(cs->upd_addr);
								SECSHR_ACCOUNTING(blk_ptr);
							}
						} else
						{
							if (!tp_update_underway)
							{
								if (FALSE == sec_shr_blk_build(cs, blk_ptr, csa->ti->curr_tn))
								{
									SECSHR_ACCOUNTING(10);
									SECSHR_ACCOUNTING(__LINE__);
									SECSHR_ACCOUNTING(cs);
									SECSHR_ACCOUNTING(cs->blk);
									SECSHR_ACCOUNTING(cs->level);
									SECSHR_ACCOUNTING(cs->done);
									SECSHR_ACCOUNTING(cs->forward_process);
									SECSHR_ACCOUNTING(cs->first_copy);
									SECSHR_ACCOUNTING(cs->upd_addr);
									SECSHR_ACCOUNTING(blk_ptr);
									continue;
								} else if (cs->ins_off)
								{
									if ((cs->ins_off >
										((blk_hdr *)blk_ptr)->bsiz - sizeof(block_id))
										|| (cs->ins_off < (sizeof(blk_hdr)
											+ sizeof(rec_hdr)))
										|| (0 > (short)cs->index)
										|| ((cs - cw_set_addrs) <= cs->index))
									{
										SECSHR_ACCOUNTING(7);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->index);
										SECSHR_ACCOUNTING(cs->ins_off);
										SECSHR_ACCOUNTING(((blk_hdr *)blk_ptr)->bsiz);
										continue;
									}
									PUT_LONG((blk_ptr + cs->ins_off),
									 ((cw_set_element *)(cw_set_addrs + cs->index))->blk);
									if (((nxt = cs + 1) < cs_top)
										&& (gds_t_write_root == nxt->mode))
									{
										if ((nxt->ins_off >
										     ((blk_hdr *)blk_ptr)->bsiz - sizeof(block_id))
											|| (nxt->ins_off < (sizeof(blk_hdr)
												 + sizeof(rec_hdr)))
											|| (0 > (short)nxt->index)
											|| ((cs - cw_set_addrs) <= nxt->index))
										{
											SECSHR_ACCOUNTING(7);
											SECSHR_ACCOUNTING(__LINE__);
											SECSHR_ACCOUNTING(nxt);
											SECSHR_ACCOUNTING(cs->blk);
											SECSHR_ACCOUNTING(nxt->index);
											SECSHR_ACCOUNTING(nxt->ins_off);
											SECSHR_ACCOUNTING(
												((blk_hdr *)blk_ptr)->bsiz);
											continue;
										}
										PUT_LONG((blk_ptr + nxt->ins_off),
									                 ((cw_set_element *)
											 (cw_set_addrs + nxt->index))->blk);
									}
								}
							} else
							{	/* TP */
								if (cs->done == 0)
								{
									if (FALSE == sec_shr_blk_build(cs, blk_ptr,
													csa->ti->curr_tn))
									{
										SECSHR_ACCOUNTING(10);
										SECSHR_ACCOUNTING(__LINE__);
										SECSHR_ACCOUNTING(cs);
										SECSHR_ACCOUNTING(cs->blk);
										SECSHR_ACCOUNTING(cs->level);
										SECSHR_ACCOUNTING(cs->done);
										SECSHR_ACCOUNTING(cs->forward_process);
										SECSHR_ACCOUNTING(cs->first_copy);
										SECSHR_ACCOUNTING(cs->upd_addr);
										SECSHR_ACCOUNTING(blk_ptr);
										continue;
									}
									if (cs->ins_off != 0)
									{
										if ((cs->ins_off
											> ((blk_hdr *)blk_ptr)->bsiz
												- sizeof(block_id))
											|| (cs->ins_off
											 < (sizeof(blk_hdr) + sizeof(rec_hdr))))
										{
											SECSHR_ACCOUNTING(7);
											SECSHR_ACCOUNTING(__LINE__);
											SECSHR_ACCOUNTING(cs);
											SECSHR_ACCOUNTING(cs->blk);
											SECSHR_ACCOUNTING(cs->index);
											SECSHR_ACCOUNTING(cs->ins_off);
											SECSHR_ACCOUNTING(
												((blk_hdr *)blk_ptr)->bsiz);
											continue;
										}
										if (cs->first_off == 0)
											cs->first_off = cs->ins_off;
										chain_ptr = blk_ptr + cs->ins_off;
										chain.flag = 1;
										chain.cw_index = cs->index;
										/* note: currently no verification of cs->index */
										chain.next_off = cs->next_off;
										GET_LONGP(chain_ptr, &chain);
										cs->ins_off = cs->next_off = 0;
									}
								} else
								{
									memmove(blk_ptr, cs->new_buff,
										((blk_hdr *)cs->new_buff)->bsiz);
									((blk_hdr *)blk_ptr)->tn = csa->ti->curr_tn;
								}
								if (cs->first_off)
								{
									for (chain_ptr = blk_ptr + cs->first_off; ;
										chain_ptr += chain.next_off)
									{
										GET_LONGP(&chain, chain_ptr);
										if ((1 == chain.flag)
										   && ((chain_ptr - blk_ptr + sizeof(block_id))
										          <= ((blk_hdr *)blk_ptr)->bsiz)
										   && (chain.cw_index < si->cw_set_depth)
										   && (TRUE == secshr_tp_get_cw(
										      first_cw_set, chain.cw_index, &cs_ptr)))
										{
											PUT_LONG(chain_ptr, cs_ptr->blk);
											if (0 == chain.next_off)
												break;
										} else
										{
											SECSHR_ACCOUNTING(11);
											SECSHR_ACCOUNTING(__LINE__);
											SECSHR_ACCOUNTING(cs);
											SECSHR_ACCOUNTING(cs->blk);
											SECSHR_ACCOUNTING(cs->index);
											SECSHR_ACCOUNTING(blk_ptr);
											SECSHR_ACCOUNTING(chain_ptr);
											SECSHR_ACCOUNTING(chain.next_off);
											SECSHR_ACCOUNTING(chain.cw_index);
											SECSHR_ACCOUNTING(si->cw_set_depth);
											SECSHR_ACCOUNTING(
												((blk_hdr *)blk_ptr)->bsiz);
											break;
										}
									}
								}
							}	/* TP */
						}	/* non-map processing */
						cs->mode = gds_t_committed;
					}	/* for all cw_set entries */
				}	/* if (NULL != first_cw_set) */
				if (JNL_ENABLED(csd))
				{
					if (GTM_PROBE(sizeof(jnl_private_control), csa->jnl, WRITE))
					{
						jbp = csa->jnl->jnl_buff;
						if (GTM_PROBE(sizeof(jnl_buffer), jbp, WRITE))
						{
							CHECK_LATCH(&jbp->fsync_in_prog_latch);
							if (VMS_ONLY(csa->jnl->qio_active)
								UNIX_ONLY(jbp->io_in_prog_latch.latch_pid == process_id))
							{
								if (csa->jnl->dsk_update_inprog)
								{
									jbp->dsk = csa->jnl->new_dsk;
									jbp->dskaddr = csa->jnl->new_dskaddr;
								}
								if (jbp->blocked == rundown_process_id)
									jbp->blocked = 0;
								VMS_ONLY(
									bci(&jbp->io_in_prog);
									csa->jnl->qio_active = FALSE;
								)
								UNIX_ONLY(RELEASE_SWAPLOCK(&jbp->io_in_prog_latch));
							}

							if (csa->jnl->free_update_inprog)
							{
								jbp->free = csa->jnl->temp_free;
								jbp->freeaddr = csa->jnl->new_freeaddr;
							}
						}
					} else
					{
					 	SECSHR_ACCOUNTING(4);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(csa->jnl);
						SECSHR_ACCOUNTING(sizeof(jnl_private_control));
					}
				}
				if (csa->freeze && csd->freeze == rundown_process_id && !csa->persistent_freeze)
				{
					csd->image_count = 0;
					csd->freeze = 0;
				}
				if (csa->now_crit)
				{
					if (is_bg)
						csd->wc_blocked = TRUE;
					if (csa->ti->curr_tn == csa->ti->early_tn - 1)
					{
						if (csa->t_commit_crit)
							csa->ti->curr_tn++;
						else
							csa->ti->early_tn = csa->ti->curr_tn;
					}
					csa->t_commit_crit = FALSE;	/* ensure we don't process this region again */
					if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE)) &&
                                                (GTM_PROBE(CRIT_SPACE, csa->critical, WRITE)))
					{
						if (csa->nl->in_crit == rundown_process_id)
							csa->nl->in_crit = 0;
						UNIX_ONLY(
							DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
							mutex_unlockw(reg, crash_count);
							DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
						)
						VMS_ONLY(
							mutex_stoprelw(csa->critical);
							csa->now_crit = FALSE;
						)
						UNSUPPORTED_PLATFORM_CHECK;
					} else
					{
						SECSHR_ACCOUNTING(6);
                                                SECSHR_ACCOUNTING(__LINE__);
                                                SECSHR_ACCOUNTING(csa->nl);
                                                SECSHR_ACCOUNTING(NODE_LOCAL_SIZE_DBS);
                                                SECSHR_ACCOUNTING(csa->critical);
                                                SECSHR_ACCOUNTING(CRIT_SPACE);
					}
				} else  if (csa->read_lock)
				{
					if (GTM_PROBE(CRIT_SPACE, csa->critical, WRITE))
					{
						VMS_ONLY(mutex_stoprelr(csa->critical);)
						csa->read_lock = FALSE;
					} else
					{
						SECSHR_ACCOUNTING(4);
						SECSHR_ACCOUNTING(__LINE__);
						SECSHR_ACCOUNTING(csa->critical);
						SECSHR_ACCOUNTING(CRIT_SPACE);
					}
				}
#ifdef UNIX
				/* All releases done now. Double check latch is really cleared */
				if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE)) &&
				    (GTM_PROBE(CRIT_SPACE, csa->critical, WRITE)))
				{
					CHECK_LATCH(&csa->critical->semaphore);
					CHECK_LATCH(&csa->critical->crashcnt_latch);
					CHECK_LATCH(&csa->critical->prochead.latch);
					CHECK_LATCH(&csa->critical->freehead.latch);
				}
#endif
			}	/* For all regions */
		} else		/* if gd_header is accessible */
			break;
	}	/* For all glds */
	if (jnlpool_reg_addrs && (GTM_PROBE(sizeof(jnlpool_reg_addrs), jnlpool_reg_addrs, READ)))
	{
		for (reg = *jnlpool_reg_addrs, jnlpool_reg = TRUE; jnlpool_reg && reg; jnlpool_reg = FALSE) /* only jnlpool reg */
		{
			SECSHR_PROBE_REGION(reg);	/* SECSHR_PROBE_REGION sets csa */
			if (csa->now_crit)
			{
				if ((GTM_PROBE(NODE_LOCAL_SIZE_DBS, csa->nl, WRITE)) &&
					(GTM_PROBE(CRIT_SPACE, csa->critical, WRITE)))
				{
					if (csa->nl->in_crit == rundown_process_id)
						csa->nl->in_crit = 0;
					UNIX_ONLY(
						DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
						mutex_unlockw(reg, 0);
						DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
					)
					VMS_ONLY(
						mutex_stoprelw(csa->critical);
						csa->now_crit = FALSE;
					)
				}
			}
		}
	}
	return;
}

bool secshr_tp_get_cw(cw_set_element *cs, int depth, cw_set_element **cs1)
{
	int	iter;

	*cs1 = cs;
	for (iter = 0; iter < depth; iter++)
	{
		if (!(GTM_PROBE(sizeof(cw_set_element), *cs1, READ)))
		{
			*cs1 = NULL;
			return FALSE;
		}
		*cs1 = (*cs1)->next_cw_set;
	}
	if (*cs1 && GTM_PROBE(sizeof(cw_set_element), *cs1, READ))
	{
		while ((*cs1)->high_tlevel)
		{
			if (GTM_PROBE(sizeof(cw_set_element), (*cs1)->high_tlevel, READ))
				*cs1 = (*cs1)->high_tlevel;
			else
			{
				*cs1 = NULL;
				return FALSE;
			}
		}
	}
	return TRUE;
}
