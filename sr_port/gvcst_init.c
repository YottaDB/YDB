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

#include <stddef.h>		/* for offsetof macro */

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "cryptdef.h"
#include "mlkdef.h"
#include "error.h"
#include "gt_timer.h"
#include "gtmimagename.h"
#include "gvcst_init.h"
#include "dbfilop.h"
#include "gvcst_init_sysops.h"
#include "set_num_additional_processors.h"

GBLREF gd_region		*gv_cur_region, *db_init_region;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF boolean_t		gtcm_connection;
GBLREF bool			licensed;
GBLREF int4			lkid;
GBLREF char			*update_array, *update_array_ptr;
GBLREF int			update_array_size;
GBLREF int			cumul_update_array_size;
GBLREF ua_list			*first_ua, *curr_ua;
GBLREF short			crash_count, dollar_tlevel;
GBLREF jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF jnl_process_vector	*prc_vec;
GBLREF unsigned char            *non_tp_jfb_buff_ptr;
GBLREF boolean_t               	mupip_jnl_recover;
GBLREF buddy_list		*global_tlvl_info_list;
GBLREF enum gtmImageTypes	image_type;

LITREF char			gtm_release_name[];
LITREF int4			gtm_release_name_len;
LITREF int			jnl_fixed_size[];

void	assert_jrec_member_offsets(void);

/* The following function hs been moved from jnl_write_logical.c to make sure that offsets are correct */

void	assert_jrec_member_offsets(void)
{
	jrec_union	jnl_record;

	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_fkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_gkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_tkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_ukill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_set.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_fset.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_gset.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_tset.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_uset.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_zkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_fzkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_gzkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_tzkill.pini_addr);
	assert(&jnl_record.jrec_kill.pini_addr == &jnl_record.jrec_uzkill.pini_addr);

	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_fkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_gkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_tkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_ukill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_set.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_fset.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_gset.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_tset.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_uset.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_zkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_fzkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_gzkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_tzkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_uzkill.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_pblk.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_epoch.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_tcom.tc_short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_ztcom.tc_short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_null.short_time);
	assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_inctn.short_time);
        assert(&jnl_record.jrec_kill.short_time == &jnl_record.jrec_aimg.short_time);

	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_fkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_gkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_tkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_ukill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_set.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_fset.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_gset.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_tset.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_uset.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_zkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_fzkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_gzkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_tzkill.tn);
	assert(&jnl_record.jrec_kill.tn	== &jnl_record.jrec_uzkill.tn);
	assert(&jnl_record.jrec_kill.tn == &jnl_record.jrec_inctn.tn);
        assert(&jnl_record.jrec_kill.tn == &jnl_record.jrec_aimg.tn);


	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_fkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_gkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_tkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_ukill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_set.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_fset.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_gset.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_tset.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_uset.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_zkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_fzkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_gzkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_tzkill.rec_seqno);
	assert(&jnl_record.jrec_kill.rec_seqno == &jnl_record.jrec_uzkill.rec_seqno);

	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_fkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_gkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_tkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_ukill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_set.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_fset.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_gset.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_tset.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_uset.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_zkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_fzkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_gzkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_tzkill.jnl_seqno);
	assert(&jnl_record.jrec_kill.jnl_seqno == &jnl_record.jrec_uzkill.jnl_seqno);

	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_gkill.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_tkill.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_ukill.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_fset.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_gset.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_tset.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_uset.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_fzkill.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_gzkill.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_tzkill.token);
	assert(&jnl_record.jrec_fkill.token == &jnl_record.jrec_uzkill.token);
	assert(&jnl_record.jrec_pini.process_vector == &jnl_record.jrec_pfin.process_vector);
	assert(&jnl_record.jrec_pini.process_vector == &jnl_record.jrec_eof.process_vector);
}

void gvcst_init (gd_region *greg)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd, temp_cs_data;
	char			cs_data_buff[ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE)];
	unsigned int		segment_update_array_size;
	file_control		*fc;
	gd_region		*prev_reg;
	sgm_info		*si;
#ifdef DEBUG
	cache_rec_ptr_t		cr;
	bt_rec_ptr_t		bt;
	blk_ident		tmp_blk;
#endif

	error_def (ERR_DBCRPT);
	error_def (ERR_DBCREIMC);
	error_def (ERR_DBNOTGDS);
	error_def (ERR_BADDBVER);
	error_def (ERR_VERMISMATCH);

	/* check the header design assumptions */
	assert(sizeof(th_rec) == (sizeof(bt_rec) - sizeof(bt->blkque)));
	assert(sizeof(cache_rec) == (sizeof(cache_state_rec) + sizeof(cr->blkque)));
	assert_jrec_member_offsets();
        set_num_additional_processors();

	DEBUG_ONLY(
		/* Note that the "block" member in the blk_ident structure in gdskill.h has 26 bits.
		 * Currently, the maximum number of blocks is 2**26. If ever this increases, something
		 * has to be correspondingly done to the "block" member to increase its capacity.
		 * The following assert checks that we always have space in the "block" member
		 * to represent a GDS block number.
		 */
		tmp_blk.block = -1;
		assert(MAXTOTALBLKS - 1 <= tmp_blk.block);
	)
	if ((prev_reg = dbfilopn(greg)) != greg)
	{
		if (greg->dyn.addr->acc_meth == dba_cm)
			return;
		greg->dyn.addr->file_cntl = prev_reg->dyn.addr->file_cntl;
		memcpy(greg->dyn.addr->fname, prev_reg->dyn.addr->fname, prev_reg->dyn.addr->fname_len);
		greg->dyn.addr->fname_len = prev_reg->dyn.addr->fname_len;
		csa = (sgmnt_addrs *)&FILE_INFO(greg)->s_addrs;
		csd = csa->hdr;
		greg->max_rec_size = csd->max_rec_size;
		greg->max_key_size = csd->max_key_size;
	 	greg->null_subs = csd->null_subs;
		greg->jnl_state = csd->jnl_state;
		greg->jnl_file_len = csd->jnl_file_len;		/* journal file name length */
		memcpy(greg->jnl_file_name, csd->jnl_file_name, greg->jnl_file_len);	/* journal file name */
		greg->jnl_alq = csd->jnl_alq;
		greg->jnl_deq = csd->jnl_deq;
		greg->jnl_buffer_size = csd->jnl_buffer_size;
		greg->jnl_before_image = csd->jnl_before_image;
		greg->open = TRUE;
		greg->opening = FALSE;
		greg->was_open = TRUE;
		return;
	}
	greg->was_open = FALSE;
	csa = (sgmnt_addrs *)&FILE_INFO(greg)->s_addrs;

#ifdef	NOLICENSE
	licensed= TRUE ;
#else
	CRYPT_CHKSYSTEM ;
#endif
	db_init_region = greg;	/* initialized for dbinit_ch */
	csa->hdr = NULL;
        csa->nl = NULL;
        csa->jnl = NULL;
	csa->persistent_freeze = FALSE;	/* want secshr_db_clnup() to clear an incomplete freeze/unfreeze codepath */
	UNIX_ONLY(
		FILE_INFO(greg)->semid = -1;
		FILE_INFO(greg)->shmid = -1;
		FILE_INFO(greg)->sem_ctime = 0;
		FILE_INFO(greg)->shm_ctime = 0;
		FILE_INFO(greg)->ftok_semid = -1;
	)
	VMS_ONLY(
		csa->db_addrs[0] = csa->db_addrs[1] = NULL;
		csa->lock_addrs[0] = csa->lock_addrs[1] = NULL;
	)
	UNSUPPORTED_PLATFORM_CHECK;
        ESTABLISH(dbinit_ch);

	temp_cs_data = (sgmnt_data_ptr_t)cs_data_buff;
	fc = greg->dyn.addr->file_cntl;
	fc->file_type = greg->dyn.addr->acc_meth;
	fc->op = FC_READ;
	fc->op_buff = (sm_uc_ptr_t)temp_cs_data;
	fc->op_len = sizeof(*temp_cs_data);
	fc->op_pos = 1;
	dbfilop(fc);
	if (memcmp(temp_cs_data->label, GDS_LABEL, GDS_LABEL_SZ - 1))
	{
		if (memcmp(temp_cs_data->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			rts_error(VARLSTCNT(4) ERR_DBNOTGDS, 2, DB_LEN_STR(greg));
		else
			rts_error(VARLSTCNT(4) ERR_BADDBVER, 2, DB_LEN_STR(greg));
	}
	/* Set the following values to sane values for recovery/rollback */
	if (mupip_jnl_recover)
	{
		temp_cs_data->createinprogress = FALSE;
		temp_cs_data->freeze = 0;
		temp_cs_data->image_count = 0;
		temp_cs_data->owner_node = 0;
	}
	if (temp_cs_data->createinprogress)
		rts_error(VARLSTCNT(4) ERR_DBCREIMC, 2, DB_LEN_STR(greg));
	if (temp_cs_data->file_corrupt && !mupip_jnl_recover)
		rts_error(VARLSTCNT(4) ERR_DBCRPT, 2, DB_LEN_STR(greg));
	assert(greg->dyn.addr->acc_meth != dba_cm);
	if (greg->dyn.addr->acc_meth != temp_cs_data->acc_meth)
		greg->dyn.addr->acc_meth = temp_cs_data->acc_meth;

/* Here's the shared memory layout:
 *
 * low address
 *
 * both
 *	segment_data
 *	(file_header)
 *	MM_BLOCK
 *	(master_map)
 *	TH_BLOCK
 * BG
 *	bt_header
 *	(bt_buckets * bt_rec)
 *	th_base (sizeof(que_ent) into an odd bt_rec)
 *	bt_base
 *	(n_bts * bt_rec)
 *	LOCK_BLOCK (lock_space)
 *	(lock_space_size)
 *	cs_addrs->acc_meth.bg.cache_state
 *	(cache_que_heads)
 *	(bt_buckets * cache_rec)
 *	(n_bts * cache_rec)
 *	critical
 *	(mutex_struct)
 *	nl
 *	(node_local)
 *	[jnl_name
 *	jnl_buffer]
 * MM
 *	file contents
 *	LOCK_BLOCK (lock_space)
 *	(lock_space_size)
 *	cs_addrs->acc_meth.mm.mmblk_state
 *	(mmblk_que_heads)
 *	(bt_buckets * mmblk_rec)
 *	(n_bts * mmblk_rec)
 *	critical
 *	(mutex_struct)
 *	nl
 *	(node_local)
 *	[jnl_name
 *	jnl_buffer]
 * high address
 */
 	/* Ensure first 3 members (upto now_running) of node_local are at the same offset for any version.
	 * This is so that the VERMISMATCH error can be successfully detected in db_init/mu_rndwn_file
	 *	and so that the db-file-name can be successfully obtained from orphaned shm by mu_rndwn_all.
	 */
 	assert(offsetof(node_local, label[0]) == 0);
	assert(offsetof(node_local, fname[0]) == GDS_LABEL_SZ);
	assert(offsetof(node_local, now_running[0]) == GDS_LABEL_SZ + MAX_FN_LEN + 1);
	assert(sizeof(csa->nl->now_running) == MAX_REL_NAME);
	db_init(greg, temp_cs_data);
	crash_count = csa->critical->crashcnt;

	csd = csa->hdr;
	if (memcmp(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1))
		rts_error(VARLSTCNT(8) ERR_VERMISMATCH & ~SEV_MSK | ((DSE_IMAGE != image_type) ? ERROR : INFO), 6,
			DB_LEN_STR(greg), gtm_release_name_len, gtm_release_name, LEN_AND_STR(csa->nl->now_running));
	/* set csd and fill in selected fields */
	switch (greg->dyn.addr->acc_meth)
	{
	case dba_mm:
		csa->acc_meth.mm.base_addr = (sm_uc_ptr_t)((sm_ulong_t)csd + (int)(csd->start_vbn - 1) * DISK_BLOCK_SIZE);
		break;
	case dba_bg:
		csa->clustered = csd->clustered;
		db_csh_ini(csa);
		break;
	default:
		GTMASSERT;
	}
	db_common_init(greg, csa, csd);	/* do initialization common to db_init() and mu_rndwn_file() */
	/* initialization of prc_vec is done unconditionally irrespective of whether journalling
	 * is allowed or not because mupip recover calls mur_recover_write_epoch_rec() which in turn
	 * calls jnl_put_jrt_pini() which needs an initialized prc_vec even though journalling
	 * had been disabled when recover did the call to gvcst_init() for that region
	 */
	if ((NULL == prc_vec)
		VMS_ONLY(&& (GTCM_SERVER_IMAGE != image_type)))
	{
		prc_vec = (jnl_process_vector *)malloc(sizeof(jnl_process_vector));
		jnl_prc_vector(prc_vec);
	}
	/* Compute the maximum journal space requirements for a PBLK (including possible ALIGN record).
	 * Use this constant in the TOTAL_TPJNL_REC_SIZE and TOTAL_NONTP_JNL_REC_SIZE macros instead of recomputing.
	 */
	csa->pblk_align_jrecsize = (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_PBLK] + JREC_SUFFIX_SIZE
					+ csd->blk_size
					+ JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE);
	segment_update_array_size = UA_SIZE(csd);

	if (first_ua == NULL)
	{	/* first open of first database - establish an update array system */
		assert(update_array == NULL);
		assert(update_array_ptr == NULL);
		assert(update_array_size == 0);
		first_ua = curr_ua
			 = (ua_list *)malloc(sizeof(ua_list));
		first_ua->next_ua = NULL;
		first_ua->update_array_size = update_array_size = cumul_update_array_size
					    = segment_update_array_size;
		first_ua->update_array = update_array
					= update_array_ptr
					= (char *)malloc(segment_update_array_size);
	} else
	{	/* there's already an update_array system in place */
		assert(update_array != NULL);
		assert(update_array_size != 0);
		if (!dollar_tlevel && segment_update_array_size > first_ua->update_array_size)
		{
			/* no transaction in progress and the current array is too small - replace it */
			assert(first_ua->update_array == update_array);
			assert(first_ua->update_array_size == update_array_size);
			assert(first_ua->next_ua == NULL);
			free(update_array);
			first_ua->update_array_size = update_array_size = cumul_update_array_size
							= segment_update_array_size;
			first_ua->update_array = update_array = update_array_ptr
						= (char *)malloc(segment_update_array_size);
		}
	}
	assert(global_tlvl_info_list || !csa->sgm_info_ptr);
	if (JNL_ALLOWED(csd))
	{
		if (NULL == non_tp_jfb_ptr)
		{
			non_tp_jfb_ptr = (jnl_format_buffer *)malloc(sizeof(jnl_format_buffer));
			non_tp_jfb_buff_ptr =  (unsigned char *)malloc(MAX_JNL_REC_SIZE);
			non_tp_jfb_ptr->buff = (char *) non_tp_jfb_buff_ptr;
			non_tp_jfb_ptr->record_size = 0;	/* initialize it to 0 since TOTAL_NONTPJNL_REC_SIZE macro uses it */
		}
		/* csa->min_total_tpjnl_rec_size represents the minimum journal buffer space needed for a TP
		 * 	transaction. It is a conservative estimate assuming an align record will be written for
		 *	every jnl record written and assuming a PINI will be written every TP transaction.
		 * csa->total_jnl_record_size is initialized/reinitialized  to this value here and in tp_clean_up().
		 * The purpose of this field is to avoid recomputation of a constant value in tp_clean_up().
		 * In addition to this, space requirements for whatever journal records get formatted as part of
		 *	jnl_format() need to be taken into account.
		 *	This is done in jnl_format() where si->total_jnl_record_size is appropriately incremented.
		 */
		csa->min_total_tpjnl_rec_size =
				(JREC_PREFIX_SIZE + jnl_fixed_size[JRT_PINI] + JREC_SUFFIX_SIZE)
					+ (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE)
					+ (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_TCOM] + JREC_SUFFIX_SIZE)
					+ (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE);
		/* Similarly csa->min_total_nontpjnl_rec_size represents the minimum journal buffer space needed
		 *	for a non-TP transaction. It is a conservative estimate assuming an align record will be
		 *	written for every jnl record written and also assumes a PINI will be written.
		 * The second ALIGN record accounted below corresponds to the logical jnl record in non_tp_jfb_ptr.
		 */
		 csa->min_total_nontpjnl_rec_size =
				(JREC_PREFIX_SIZE + jnl_fixed_size[JRT_PINI] + JREC_SUFFIX_SIZE)
					+ (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE)
					+ (JREC_PREFIX_SIZE + jnl_fixed_size[JRT_ALIGN] + JREC_SUFFIX_SIZE);
	}
	/* For the first open of this region we are guaranteed that csa->sgm_info_ptr is NULL.
	 * Only in this case, the one-time TP structure-initialization needs to be done.
	 * Note GT.CM and DAL-calls-to-gds_rundown can require opening/closing of the same region multiple times.
	 */
	if (GTCM_SERVER_IMAGE != image_type)	/* currently TP is not supported for GT.CM */
	{
		if (NULL == csa->sgm_info_ptr)
		{
			si = csa->sgm_info_ptr = (sgm_info *)malloc(sizeof(sgm_info));
			assert(32768 > sizeof(sgm_info));
			memset(si, 0, sizeof(sgm_info));
			si->tp_hist_size = TP_MAX_MM_TRANSIZE;
			si->cur_tp_hist_size = INIT_CUR_TP_HIST_SIZE;	/* should be very much less than si->tp_hist_size */
			assert(si->cur_tp_hist_size <= si->tp_hist_size);
			init_hashtab(&si->blks_in_use, BLKS_IN_USE_INIT_ELEMS);
			/* See comment in tp.h about cur_tp_hist_size for details */
			si->first_tp_hist = si->last_tp_hist =
							(srch_blk_status *)malloc(sizeof(srch_blk_status) * si->cur_tp_hist_size);
			si->cw_set_list = (buddy_list *)malloc(sizeof(buddy_list));
			initialize_list(si->cw_set_list, sizeof(cw_set_element), CW_SET_LIST_INIT_ALLOC);
			si->tlvl_cw_set_list = (buddy_list *)malloc(sizeof(buddy_list));
			initialize_list(si->tlvl_cw_set_list, sizeof(cw_set_element), TLVL_CW_SET_LIST_INIT_ALLOC);
			si->tlvl_info_list = (buddy_list *)malloc(sizeof(buddy_list));
			initialize_list(si->tlvl_info_list, sizeof(tlevel_info), TLVL_INFO_LIST_INIT_ALLOC);
			si->new_buff_list = (buddy_list *)malloc(sizeof(buddy_list));
			initialize_list(si->new_buff_list, sizeof(que_ent) + csa->hdr->blk_size, NEW_BUFF_LIST_INIT_ALLOC);
			si->recompute_list = (buddy_list *)malloc(sizeof(buddy_list));
			initialize_list(si->recompute_list, sizeof(key_cum_value), RECOMPUTE_LIST_INIT_ALLOC);
			/* The size of the si->cr_array can go up to TP_MAX_MM_TRANSIZE, but usually is quite less.
			 * Therefore, initially allocate a small array and expand as needed later.
			 */
			if (dba_bg == greg->dyn.addr->acc_meth)
			{
				si->cr_array_size = si->cur_tp_hist_size;
				si->cr_array = (cache_rec_ptr_ptr_t)malloc(sizeof(cache_rec_ptr_t) * si->cr_array_size);
			} else
			{
				si->cr_array_size = 0;
				si->cr_array = NULL;
			}
			si->fresh_start = TRUE;
		} else
			si = csa->sgm_info_ptr;
		si->gv_cur_region = greg;
		si->start_tn = csa->ti->curr_tn;
		if (JNL_ALLOWED(csd))
		{
			si->total_jnl_record_size = csa->min_total_tpjnl_rec_size;	/* Reinitialize total_jnl_record_size */
			/* Since the following jnl-mallocs are independent of any dynamically-changeable parameter of the
			 * database, we can as well use the existing malloced jnl structures if at all they exist.
			 */
			if (NULL == si->jnl_tail)
			{
				si->jnl_tail = &si->jnl_head;
				si->jnl_list = (buddy_list *)malloc(sizeof(buddy_list));
				initialize_list(si->jnl_list, sizeof(jnl_format_buffer), JNL_LIST_INIT_ALLOC);
				si->format_buff_list = (buddy_list *)malloc(sizeof(buddy_list));
				/* Minimum value of elemSize is 8 due to alignment requirements of the returned memory location.
				 * Therefore, we request an elemSize of 8 bytes for the format-buffer and will convert as much
				 * bytes as we need into as many 8-byte multiple segments (see code in jnl_format).
				 */
				initialize_list(si->format_buff_list, 8, DIVIDE_ROUND_UP(JNL_FORMAT_BUFF_INIT_ALLOC,8));
			}
		} else if (NULL != si->jnl_tail)
		{	/* Journalling is currently disallowed although it was allowed (non-zero si->jnl_tail)
			 * during the prior use of this region. Free up unnecessary region-specific structures now.
			 */
			FREEUP_BUDDY_LIST(si->jnl_list);
			FREEUP_BUDDY_LIST(si->format_buff_list);
			si->jnl_tail = NULL;
		}
	}
	if (!global_tlvl_info_list)
	{
		global_tlvl_info_list = (buddy_list *)malloc(sizeof(buddy_list));
		initialize_list(global_tlvl_info_list, sizeof(global_tlvl_info), GBL_TLVL_INFO_LIST_INIT_ALLOC);
	}
	greg->open = TRUE;
	greg->opening = FALSE;
	REVERT;
	return;
}
