/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>		/* for offsetof macro */

#if defined(VMS)
#include <climsgdef.h>
#include <fab.h>
#include <rms.h>
#include <errno.h>
#include <nam.h>
#include <psldef.h>
#include <rmsdef.h>
#include <descrip.h>
#include <math.h> /* needed for handling of epoch_interval (EPOCH_SECOND2SECOND macro uses ceil) */
#endif
#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "jnl.h"
#include "mupip_set.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "util.h"
#include "gvcst_protos.h"	/* for gvcst_init prototype */
#include "mu_rndwn_file.h"
#include "dbfilop.h"
#include "gds_rundown.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "tp_change_reg.h"
#include "gtm_file_stat.h"
#include "min_max.h"		/* for MAX and JNL_MAX_RECLEN macro */
#include "gtm_rename.h"		/* for cre_jnl_file_intrpt_rename() prototype */
#include "send_msg.h"
#include "gtmio.h"
#include "is_file_identical.h"

#define	DB_OR_REG_SIZE	MAX(STR_LIT_LEN(FILE_STR), STR_LIT_LEN(REG_STR)) + 1 /* trailing null byte */

GBLREF	bool			region;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	mu_set_rlist		*grlist;
GBLREF	char			*before_image_lit[];
GBLREF	char			*jnl_state_lit[];
GBLREF	char			*repl_state_lit[];
GBLREF	jnl_gbls_t		jgbl;

error_def(ERR_BEFOREIMG);
error_def(ERR_DBFILERR);
error_def(ERR_DBOPNERR);
error_def(ERR_DBPRIVERR);
error_def(ERR_DBRDERR);
error_def(ERR_FILEEXISTS);
error_def(ERR_FILENAMETOOLONG);
error_def(ERR_FILEPARSE);
error_def(ERR_JNLALIGNTOOSM);
error_def(ERR_JNLALLOCGROW);
error_def(ERR_JNLBUFFDBUPD);
error_def(ERR_JNLBUFFREGUPD);
error_def(ERR_JNLCREATE);
error_def(ERR_JNLFNF);
error_def(ERR_JNLINVSWITCHLMT);
error_def(ERR_JNLNOCREATE);
error_def(ERR_JNLRDONLY);
error_def(ERR_JNLSTATE);
error_def(ERR_JNLSWITCHSZCHG);
error_def(ERR_JNLSWITCHTOOSM);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUSTANDALONE);
error_def(ERR_PREVJNLLINKCUT);
error_def(ERR_REPLSTATE);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

VMS_ONLY(static  const   unsigned short  zero_fid[3];)

uint4	mupip_set_journal(unsigned short db_fn_len, char *db_fn)
{
	jnl_create_info		jnl_info;
	GTMCRYPT_ONLY(
		jnl_create_info	*jnl_info_ptr;
	)
	GDS_INFO		*gds_info;
	file_control		*fc;
	int			new_stat_res;	/* gtm_file_stat() return value for new journal file name */
	int			curr_stat_res; 	/* gtm_file_stat() return value for current journal file name */
	mu_set_rlist		*rptr, dummy_rlist, *next_rptr;
	sgmnt_data_ptr_t	csd;
	uint4			status,	exit_status = EXIT_NRM, gds_rundown_status = EXIT_NRM;
	seq_num			max_reg_seqno;
	unsigned int		fn_len;
	unsigned char		tmp_full_jnl_fn[MAX_FN_LEN + 1], prev_jnl_fn[MAX_FN_LEN + 1];
	char			*db_reg_name, db_or_reg[DB_OR_REG_SIZE];
	int			db_reg_name_len, db_or_reg_len;
	set_jnl_options		jnl_options;
	boolean_t		curr_jnl_present,	/* for current state 2, is current journal present? */
				jnl_points_to_db, keep_prev_link, safe_to_switch, newjnlfiles, jnlname_same,
				this_iter_prevlinkcut_error, do_prevlinkcut_error;
	enum jnl_state_codes	jnl_curr_state;
	enum repl_state_codes	repl_curr_state;
	mstr 			jnlfile, jnldef, tmpjnlfile;
	uint4			align_autoswitch;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	jnl_tm_t		save_gbl_jrec_time;
#	ifdef UNIX
	int			jnl_fd;
	jnl_file_header		header;
	int4			status1;
	uint4			status2;
	boolean_t		header_is_usable = FALSE;
#	endif
	boolean_t		jnl_buffer_updated = FALSE, jnl_buffer_invalid = FALSE;
	int			jnl_buffer_size;
	char			s[JNLBUFFUPDAPNDX_SIZE];	/* JNLBUFFUPDAPNDX_SIZE is defined in jnl.h */

	assert(SGMNT_HDR_LEN == ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
	memset(&jnl_info, 0, SIZEOF(jnl_info));
	jnl_info.status = jnl_info.status2 = SS_NORMAL;
	max_reg_seqno = 1;
	if (!mupip_set_journal_parse(&jnl_options, &jnl_info))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_MUPCLIERR);
		error_condition = ERR_MUPCLIERR;
		return ERR_MUPCLIERR;
	}
	if (region && (NULL == grlist))
	{	/* region and grlist are set by mu_getlst() invoked from mupip_set() */
		assert(FALSE);
		util_out_print("Invalid region name specified on command line. Can't continue any further", TRUE);
		return (uint4)ERR_MUNOFINISH;
	}
	/* Now process the database file or region(s) */
	if (region)
	{ 	/* The command line specified one or more regions */
		if ((NULL != grlist->fPtr) && jnl_options.filename_specified)
		{
			util_out_print("!/Multiple database regions cannot be journalled in a single file", TRUE);
			return (uint4)ERR_MUNOFINISH;
		}
	} else
	{	/* The command line specified a single database file; force the following do-loop to be one-trip */
		dummy_rlist.fPtr = NULL;
		grlist = &dummy_rlist;
	}
	ESTABLISH_RET(mupip_set_jnl_ch, (uint4)ERR_MUNOFINISH);
	SET_GBL_JREC_TIME; /* set_jnl_file_close/cre_jnl_file/wcs_flu need gbl_jrec_time initialized */
	for (rptr = grlist; (EXIT_ERR != exit_status) && NULL != rptr; rptr = rptr->fPtr)
	{
		rptr->exclusive = FALSE;
		rptr->state = NONALLOCATED;
		rptr->sd = NULL;
		if (region)
		{
			if (dba_usr == rptr->reg->dyn.addr->acc_meth)
			{
				gtm_putmsg_csa(CSA_ARG(REG2CSA(rptr->reg)) VARLSTCNT(6) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Journaling is not supported for access method USR"));
				exit_status |= EXIT_WRN;
				continue;
			}
			if (!mupfndfil(rptr->reg, NULL))
			{
				exit_status |= EXIT_ERR;
				continue;
			}
			gv_cur_region = rptr->reg;
			if (NULL == gv_cur_region->dyn.addr->file_cntl)
			{
				gv_cur_region->dyn.addr->acc_meth = dba_bg;
				gv_cur_region->dyn.addr->file_cntl =
					(file_control *)malloc(SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
				memset(gv_cur_region->dyn.addr->file_cntl, 0, SIZEOF(*gv_cur_region->dyn.addr->file_cntl));
				gv_cur_region->dyn.addr->file_cntl->file_type = dba_bg;
				gds_info = FILE_INFO(gv_cur_region);
				gds_info = (GDS_INFO *)malloc(SIZEOF(GDS_INFO));
				memset(gds_info, 0, SIZEOF(GDS_INFO));
			}
		} else
		{
			mu_gv_cur_reg_init();
			rptr->reg = gv_cur_region;
			gv_cur_region->dyn.addr->fname_len = db_fn_len;
			memcpy(gv_cur_region->dyn.addr->fname, db_fn, db_fn_len);
		}
		fc = gv_cur_region->dyn.addr->file_cntl;
		/* open shared to see what's possible */
		gvcst_init(gv_cur_region);
		tp_change_reg();
		assert(!gv_cur_region->was_open);
		rptr->sd = csd = cs_data;
		rptr->state = ALLOCATED;		/* This means gvcst_init() was called for this region */
		assert(!gv_cur_region->was_open);	/* In case mupip_set_file opened it, they must have closed */
		if (gv_cur_region->read_only)
		{
			gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(4) ERR_DBPRIVERR, 2, DB_LEN_STR(gv_cur_region));
			exit_status |= EXIT_RDONLY;
			continue;
		}
		grab_crit(gv_cur_region);  /* corresponding rel_crit() is done in mupip_set_jnl_cleanup() */
		/* Now determine new journal state, replication state and before_image for this region.
		 * Information will be kept in "rptr", which is per region.
		 * Note that we have done grab_crit(), so we are safe on deciding state transitions */
		if (EXIT_NRM != (status = mupip_set_journal_newstate(&jnl_options, &jnl_info, rptr)))
		{
			exit_status |= status;
			UNIX_ONLY(gds_rundown_status =) gds_rundown();
			exit_status |= gds_rundown_status;
			rptr->sd = NULL;
			rptr->state = NONALLOCATED;	/* This means do not call gds_rundown() again for this region
							 * and do not process this region anymore. */
			continue;
		}
		jnl_curr_state = (enum jnl_state_codes)csd->jnl_state;
		repl_curr_state = (enum repl_state_codes)csd->repl_state;

		/* Following is the transition table for replication states:
		 *
		 *			repl_close	repl_was_open	repl_open
		 * --------------------------------------------------------
		 * repl_close		-		X		S
		 * repl_was_open 	S		-		O
		 * repl_open		S		J		-
		 *
		 * Where
		 * 	X ==> not allowed,
		 * 	S ==> Standalone access needed
		 *	O ==> Online, standalone access is not needed.
		 *	J ==> transition done by jnl_file_lost()
		 */
		if ((jnl_notallowed == jnl_curr_state && jnl_notallowed != rptr->jnl_new_state) ||
			(jnl_notallowed != jnl_curr_state && jnl_notallowed == rptr->jnl_new_state) ||
			(jnl_options.buffer_size_specified && (jnl_info.buffer != csd->jnl_buffer_size)) ||
			(repl_closed != repl_curr_state && repl_closed == rptr->repl_new_state) ||
			(repl_closed == repl_curr_state && repl_open == rptr->repl_new_state))
		{
			/* Since we did gvcst_init() and now will call mu_rndwn_file() */
			UNIX_ONLY(gds_rundown_status =) gds_rundown();
			exit_status |= gds_rundown_status;
			rptr->state = NONALLOCATED;
			rptr->sd = csd = NULL;
			/* WARNING: The remaining code uses gv_cur_region and others
			 * on the assumption that gds_rundown does not deallocate the space when it closes the file */
			assert(NULL != gv_cur_region);
			assert(NULL != gv_cur_region->dyn.addr);
			assert(NULL != gv_cur_region->dyn.addr->file_cntl);
			assert(NULL != gv_cur_region->dyn.addr->file_cntl->file_info);
			if (EXIT_NRM != gds_rundown_status)		/* skip mu_rndwn_file (STANDALONE) */
				continue;
			if (STANDALONE(gv_cur_region))
			{
				rptr->exclusive = TRUE;
				fc->op = FC_OPEN;
				fc->file_type = dba_bg;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
				{
					DBFILOP_FAIL_MSG(status, ERR_DBOPNERR);
					exit_status |= EXIT_ERR;
					continue;
				}
				/* Need to read file header again,
				 * because mu_rndwn_file does not have an interface to return fileheader */
				csd = (sgmnt_data_ptr_t)malloc(SGMNT_HDR_LEN);
				fc->op = FC_READ;
				fc->op_buff = (sm_uc_ptr_t)csd;
				fc->op_len = SGMNT_HDR_LEN;
				fc->op_pos = 1;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
				{
					DBFILOP_FAIL_MSG(status, ERR_DBRDERR);
					fc->op = FC_CLOSE;
					dbfilop(fc);
					exit_status |= EXIT_ERR;
					continue; /* Later mupip_set_jnl_cleanup() will do the cleanup */
				}
				cs_data = rptr->sd = csd;
				rptr->state = ALLOCATED;
			} else
			{
				gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(4) ERR_MUSTANDALONE, 2,
						DB_LEN_STR(gv_cur_region));
				exit_status |= EXIT_ERR;
				continue;
			}
		} else
		{
			/* Now that we have crit, check if this region is actively journaled and if gbl_jrec_time needs to be
			 * adjusted (to ensure time ordering of journal records within this region's journal file).
			 * This needs to be done BEFORE writing any journal records for this region. The value of
			 * jgbl.gbl_jrec_time at the end of this loop will be used to write journal records for ALL
			 * regions so all regions will have same eov/bov timestamps.
			 */
			if (JNL_ENABLED(cs_data)
				UNIX_ONLY( && (0 != cs_addrs->nl->jnl_file.u.inode))
				VMS_ONLY( && (0 != memcmp(cs_addrs->nl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid)))))
			{
				jpc = cs_addrs->jnl;
				jbp = jpc->jnl_buff;
				ADJUST_GBL_JREC_TIME(jgbl, jbp);
			}
		}
		if (max_reg_seqno < csd->reg_seqno)
			max_reg_seqno = csd->reg_seqno;
	}
	DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
	jgbl.dont_reset_gbl_jrec_time = TRUE;
	do_prevlinkcut_error = FALSE;
	for (rptr = grlist; (EXIT_ERR != exit_status) && NULL != rptr; rptr = next_rptr)
	{
		this_iter_prevlinkcut_error = do_prevlinkcut_error;
		do_prevlinkcut_error = FALSE;
		next_rptr = rptr->fPtr;
		gv_cur_region = rptr->reg;
		if (gv_cur_region->read_only)
			continue;
		tp_change_reg(); /* cs_data and cs_addrs are used in functions called from here */
		cs_data = csd = rptr->sd;
		assert(NULL != csd || NONALLOCATED == rptr->state);
		if (NULL == csd)	/* Just to be safe. May be this is not necessary. */
			continue;
		jnl_curr_state = (enum jnl_state_codes)csd->jnl_state;
		repl_curr_state = (enum repl_state_codes)csd->repl_state;
		jnl_info.csd = csd;
		jnl_info.csa = cs_addrs;
		jnl_info.before_images = rptr->before_images;
		jnl_info.repl_state = rptr->repl_new_state;
		jnl_info.jnl_state = csd->jnl_state;
		/* note that even replication on to off will create new journals */
		newjnlfiles = (jnl_open == rptr->jnl_new_state) ?  TRUE : FALSE;
		fc = gv_cur_region->dyn.addr->file_cntl;
		jnl_info.fn = gv_cur_region->dyn.addr->fname;
		jnl_info.fn_len = gv_cur_region->dyn.addr->fname_len;
		if (region)
		{
			strcpy(db_or_reg, REG_STR);
			db_or_reg_len = SIZEOF(REG_STR) - 1;
			db_reg_name = (char *)gv_cur_region->rname;
			db_reg_name_len = (gv_cur_region)->rname_len;
		} else
		{
			strcpy(db_or_reg, FILE_STR);
			db_or_reg_len = SIZEOF(FILE_STR) - 1;
			db_reg_name = (char *)jnl_info.fn;
			db_reg_name_len = jnl_info.fn_len;
		}
		if (jnl_notallowed != rptr->jnl_new_state)
		{ 	/* Fill in any unspecified information and process journal characteristics.
			 * If database contains journal characteristics (like allocation, alignsize etc.)
			 * which are smaller than the new journal minimums (increased in V54001), adjust
			 * them to ensure we honor the new minimums.
			 */
			if (!jnl_options.allocation_specified)
			{
				jnl_info.alloc = (0 == csd->jnl_alq) ? JNL_ALLOC_DEF : csd->jnl_alq;
				assert(JNL_ALLOC_DEF >= JNL_ALLOC_MIN);
				if (JNL_ALLOC_MIN > jnl_info.alloc)
					jnl_info.alloc = JNL_ALLOC_MIN;				/* Fix new journal settings. */
			}
			if (!jnl_options.alignsize_specified)
			{
				jnl_info.alignsize = (0 == csd->alignsize) ? (DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE) :
					csd->alignsize; /* In bytes */
				assert(JNL_DEF_ALIGNSIZE >= JNL_MIN_ALIGNSIZE);
				if ((DISK_BLOCK_SIZE * JNL_MIN_ALIGNSIZE) > jnl_info.alignsize)
					jnl_info.alignsize = (DISK_BLOCK_SIZE * JNL_MIN_ALIGNSIZE); /* Fix new journal settings. */
			}
			if (jnl_info.alignsize <= csd->blk_size)
			{
				if (region)
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(9) ERR_JNLALIGNTOOSM, 7,
							jnl_info.alignsize, csd->blk_size,
							LEN_AND_LIT("region"), REG_LEN_STR(gv_cur_region),
							(DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE));
				else
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(9) ERR_JNLALIGNTOOSM, 7,
							jnl_info.alignsize, csd->blk_size,
							LEN_AND_LIT("database file"), jnl_info.fn_len, jnl_info.fn,
							(DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE));

				jnl_info.alignsize = (DISK_BLOCK_SIZE * JNL_DEF_ALIGNSIZE);
				assert(jnl_info.alignsize > csd->blk_size);	/* to accommodate a PBLK journal record */
			}
			if (!jnl_options.autoswitchlimit_specified)
			{
				jnl_info.autoswitchlimit = (0 == csd->autoswitchlimit) ?
					JNL_AUTOSWITCHLIMIT_DEF : csd->autoswitchlimit;
				assert(JNL_AUTOSWITCHLIMIT_MIN <= jnl_info.autoswitchlimit);
			}
			if (!jnl_options.extension_specified)
			{
				jnl_info.extend = (0 == csd->jnl_deq) ? JNL_EXTEND_DEF : csd->jnl_deq;
				/* jnl_info.extend = (0 == csd->jnl_deq) ? jnl_info.alloc * JNL_EXTEND_DEF_PERC : csd->jnl_deq;
				 * Uncomment this section when code is ready to use extension = 10% of allocation */
			}
			if (!jnl_options.buffer_size_specified)
				jnl_info.buffer = (0 == csd->jnl_buffer_size) ? JNL_BUFFER_DEF : csd->jnl_buffer_size;
			ROUND_UP_JNL_BUFF_SIZE(jnl_buffer_size, jnl_info.buffer, csd);
			if (jnl_buffer_size < JNL_BUFF_PORT_MIN(csd))
			{
				jnl_buffer_invalid = TRUE;
				ROUND_UP_MIN_JNL_BUFF_SIZE(jnl_buffer_size, csd);
			} else if (jnl_buffer_size > JNL_BUFFER_MAX)
			{
				jnl_buffer_invalid = TRUE;
				ROUND_DOWN_MAX_JNL_BUFF_SIZE(jnl_buffer_size, csd);
			}
			if (jnl_buffer_size != jnl_info.buffer)
				jnl_buffer_updated = TRUE;
			/* ensure we have exclusive access in case csd->jnl_buffer_size is going to be changed */
			assert(!(jnl_options.buffer_size_specified && jnl_buffer_updated) || rptr->exclusive);
			if (!jnl_options.epoch_interval_specified)
				jnl_info.epoch_interval = (0 == csd->epoch_interval) ? DEFAULT_EPOCH_INTERVAL : csd->epoch_interval;
			JNL_MAX_RECLEN(&jnl_info, csd);
			jnl_info.reg_seqno = max_reg_seqno;
			jnl_info.prev_jnl = (char *)prev_jnl_fn;
			jnl_info.prev_jnl_len = 0;
			if (csd->jnl_file_len)
				cre_jnl_file_intrpt_rename(((int)csd->jnl_file_len), csd->jnl_file_name);
			assert(0 == csd->jnl_file_len || 0 == csd->jnl_file_name[csd->jnl_file_len]);
			csd->jnl_file_name[csd->jnl_file_len] = 0;
			if (!jnl_options.filename_specified)
				mupip_set_journal_fname(&jnl_info);
			/* the following autoswitchlimit check should be after the call to mupip_set_journal_fname()
			 * 	since it relies on jnl_info.jnl_len and jnl_info.jnl which is filled in by mupip_set_journal_fname()
			 */
			if (jnl_info.autoswitchlimit < jnl_info.alloc)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(7) ERR_JNLSWITCHTOOSM, 5,
						jnl_info.autoswitchlimit, jnl_info.alloc, DB_LEN_STR(gv_cur_region));
				if (newjnlfiles)
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
							jnl_info.jnl_len, jnl_info.jnl);
				exit_status |= EXIT_ERR;
				break;
#ifdef UNIX
			} else if (jnl_info.alloc + jnl_info.extend > jnl_info.autoswitchlimit
					&& jnl_info.alloc != jnl_info.autoswitchlimit)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_JNLALLOCGROW, 6, jnl_info.alloc,
						jnl_info.autoswitchlimit, "database file", DB_LEN_STR(gv_cur_region));
				jnl_info.alloc = jnl_info.autoswitchlimit;
#endif
			} else
			{
				align_autoswitch = ALIGNED_ROUND_DOWN(jnl_info.autoswitchlimit, jnl_info.alloc, jnl_info.extend);
				if (align_autoswitch != jnl_info.autoswitchlimit)
				{	/* round down specified autoswitch to be aligned at a journal extension boundary.
					 * t_end/tp_tend earlier used to round up their transaction's journal space requirements
					 * 	to the nearest extension boundary to compare against the autoswitchlimit later.
					 * but now with autoswitchlimit being aligned at an extension boundary, they can
					 * 	compare their journal requirements directly against the autoswitchlimit.
					 */
					assert(align_autoswitch < jnl_info.autoswitchlimit || !jnl_info.extend);
					if (jnl_options.autoswitchlimit_specified || jnl_options.extension_specified
											|| jnl_options.allocation_specified)
					{	/* print rounding down of autoswitchlimit only if journal options were specified */
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_JNLSWITCHSZCHG, 6,
								jnl_info.autoswitchlimit, align_autoswitch,
								jnl_info.alloc, jnl_info.extend, DB_LEN_STR(gv_cur_region));
					}
					jnl_info.autoswitchlimit = align_autoswitch;
					if (JNL_AUTOSWITCHLIMIT_MIN > jnl_info.autoswitchlimit)
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_JNLINVSWITCHLMT, 3,
								jnl_info.autoswitchlimit, JNL_AUTOSWITCHLIMIT_MIN, JNL_ALLOC_MAX);
						exit_status |= EXIT_ERR;
						break;
					}
				}
			}
			if (!jnl_options.yield_limit_specified)
				jnl_options.yield_limit = csd->yield_lmt;
			tmpjnlfile.addr = (char *)tmp_full_jnl_fn;
			tmpjnlfile.len = SIZEOF(tmp_full_jnl_fn);
			jnlfile.addr = (char *)jnl_info.jnl;
			jnlfile.len = jnl_info.jnl_len;
			jnldef.addr = JNL_EXT_DEF;
			jnldef.len = SIZEOF(JNL_EXT_DEF) - 1;
			if (FILE_STAT_ERROR == (new_stat_res = gtm_file_stat(&jnlfile, &jnldef, &tmpjnlfile, TRUE, &status)))
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_FILEPARSE, 2, jnlfile.len,
						jnlfile.addr, status);
				if (newjnlfiles)
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
							jnlfile.len, jnlfile.addr);
				exit_status |= EXIT_ERR;
				break;
			}
			memcpy(jnl_info.jnl, tmpjnlfile.addr, tmpjnlfile.len);
			jnl_info.jnl_len = tmpjnlfile.len;
			jnl_info.jnl[jnl_info.jnl_len] = 0;
			/* Note: At this point jnlfile should have expanded journal name with extension */
			if (MAX_FN_LEN + 1 < jnl_info.jnl_len)
			{
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(1) ERR_FILENAMETOOLONG);
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
						jnl_info.jnl_len, jnl_info.jnl);
				exit_status |= EXIT_ERR;
				break;
			}
			jnlname_same = ((jnl_info.jnl_len == csd->jnl_file_len)
				&& (0 == memcmp(jnl_info.jnl, csd->jnl_file_name, jnl_info.jnl_len))) ? TRUE : FALSE;
			jnlfile.addr = (char *)csd->jnl_file_name;
			jnlfile.len = csd->jnl_file_len;
			if (!jnlname_same)
			{
				if (FILE_STAT_ERROR == (curr_stat_res = gtm_file_stat(&jnlfile, NULL, NULL, TRUE, &status)))
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(5) ERR_FILEPARSE, 2,
							jnlfile.len, jnlfile.addr, status);
					if (newjnlfiles)
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
								jnl_info.jnl_len, jnl_info.jnl);
					exit_status |= EXIT_ERR;
					break;
				}
			} else
				curr_stat_res = new_stat_res; /* new_stat_res is already set */
#			ifdef UNIX
			if (newjnlfiles)
			{
				jnl_points_to_db = FALSE;
				if (FILE_PRESENT & curr_stat_res)
				{	/* Check if the journal file (if any) pointed to by the db file exists and points back to
					 * this database file.
					 */
					assert('\0' == jnlfile.addr[jnlfile.len]);
					jnlfile.addr[jnlfile.len] = '\0';	/* just in case above assert is FALSE */
					OPENFILE(jnlfile.addr, O_RDONLY, jnl_fd);
				} else if (FILE_PRESENT & new_stat_res)
				{	/* Check if the new journal file (that we know exists) points back to this database file.
					 * If not, the journal file prev links should be cut in the new journal file.
					 */
					assert('\0' == jnl_info.jnl[jnl_info.jnl_len]);
					jnl_info.jnl[jnl_info.jnl_len] = '\0';	/* just in case above assert is FALSE */
					OPENFILE((char *)jnl_info.jnl, O_RDONLY, jnl_fd);
				} else
					jnl_fd = FD_INVALID;
				if (0 <= jnl_fd)
				{
					DO_FILE_READ(jnl_fd, 0, &header, SIZEOF(header), status1, status2);
					if (SS_NORMAL == status1)
					{
						CHECK_JNL_FILE_IS_USABLE(&header, status1, FALSE, 0, NULL);
							/* FALSE => NO gtm_putmsg even if errors */
						if ((SS_NORMAL == status1)
							&& ARRAYSIZE(header.data_file_name) > header.data_file_name_length)
						{
							assert('\0' == header.data_file_name[header.data_file_name_length]);
							header.data_file_name[header.data_file_name_length] = '\0';
							assert('\0' == jnl_info.fn[jnl_info.fn_len]);
							jnl_info.fn[jnl_info.fn_len] = '\0';
							if (is_file_identical((char *)header.data_file_name,
												(char *)jnl_info.fn))
								jnl_points_to_db = TRUE;
							UNIX_ONLY(header_is_usable = TRUE;)
						}
					}
				}
				/* If journal file we are about to create exists, allow the switch only it is safe to do so.
				 * This way we prevent multiple environments from interfering with each other through a
				 * common journal file name. Also this way we disallow switching to a user-specified new
				 * journal file that already exists (say the database file itself due to a command line typo).
				 */
				if (FILE_PRESENT & curr_stat_res)
				{
					keep_prev_link = jnl_points_to_db;
					safe_to_switch = (jnlname_same && keep_prev_link);
				} else if (FILE_PRESENT & new_stat_res)
				{
					keep_prev_link = FALSE;
					/* In this case, the current jnl file does not exist. And so the prevlinks are
					 * going to be cleared in the new jnl file. Therefore it is safe to rename the
					 * existing new jnl file (in order to create the jnl file we want) as long as
					 * it points back to this db.
					 */
					safe_to_switch = jnl_points_to_db;
				} else
				{
					keep_prev_link = FALSE;	/* since current and new jnl file both dont exist */
					safe_to_switch = TRUE;	/* since current and new jnl file both dont exist */
				}
				if ((FILE_PRESENT & new_stat_res) && !safe_to_switch)
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_FILEEXISTS, 2,
							jnl_info.jnl_len, jnl_info.jnl);
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
							jnl_info.jnl_len, jnl_info.jnl);
					exit_status |= EXIT_ERR;
					break;
				}
			}
#			else
			keep_prev_link = TRUE;
#			endif
			if (jnl_open != jnl_curr_state)
				curr_jnl_present = FALSE;
			else
			{	/* We expect that a journal file for this region is present when
				 * current journal state is jnl_open ("2"). See if it is really present.
				 */
				curr_jnl_present = (FILE_PRESENT & curr_stat_res) ? TRUE : FALSE;
				if (curr_jnl_present)
				{
					if (FILE_READONLY & curr_stat_res)
					{
						gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLRDONLY, 2,
								JNL_LEN_STR(csd));
						if (newjnlfiles)
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4)
									ERR_JNLNOCREATE, 2, jnl_info.jnl_len, jnl_info.jnl);
						exit_status |= EXIT_RDONLY;
						continue;
					}
				} else
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLFNF, 2,
							JNL_LEN_STR(csd));
			}
			if (!rptr->exclusive)
			{
				if (jnl_open == jnl_curr_state)
				{
					assert(NULL != cs_addrs->nl);
					jpc = cs_addrs->jnl;
					UNIX_ONLY(if (cs_addrs->nl->jnl_file.u.inode))
					VMS_ONLY(if (memcmp(cs_addrs->nl->jnl_file.jnl_file_id.fid, zero_fid, SIZEOF(zero_fid))))
					{
						if (SS_NORMAL != (status = set_jnl_file_close(SET_JNL_FILE_CLOSE_SETJNL)))
						{	/* Invoke jnl_file_lost to turn off journaling and retry journal creation
							 * to create fresh journal files.
							 */
							jnl_file_lost(jpc, status);
							do_prevlinkcut_error = TRUE; /* In the next iteration issue PREVJNLLINKCUT
										      * information message */
							next_rptr = rptr;
							continue;
						}
						UNIX_ONLY(header.crash = FALSE;)	/* Even if the journal was crashed, that
											 * should be fixed now */
					} else
					{	/* Ideally, no other process should have a journal file for this database open.
						 * But, As part of C9I03-002965, we realized it is possible for processes accessing
						 * the older journal file to continue to write to it even though
						 * csa->nl->jnl_file.u.inode field is 0. The only way to signal other proceses, that
						 * have the jnl file open, of a concurrent journal file switch, is by incrementing
						 * the "jpc->jnl_buff->cycle" field. Therefore be safe and increment this just in
						 * case some other process has the older generation journal file still open.
						 */
						assert(NULL != jpc);
						jpc->jnl_buff->cycle++;
					}
#					ifdef UNIX
					/* Cut the link if the journal is crashed and there is no shared memory around */
					if (header_is_usable && header.crash)
						keep_prev_link = FALSE;
#					endif
					/* For MM, set_jnl_file_close() can call wcs_flu() which can remap the file.
					 * So reset csd and rptr->sd since their value may have changed.
					 */
					assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
					rptr->sd = csd = cs_data;
				} else if ((jnl_closed == jnl_curr_state) && (jnl_open == rptr->jnl_new_state))
				{	/* sync db for closed->open transition. for VMS WCSFLU_FSYNC_DB is ignored */
					wcs_flu(WCSFLU_FSYNC_DB | WCSFLU_FLUSH_HDR);
					/* In case this is MM and wcs_flu() remapped an extended database, reset csd and rptr->sd */
					assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
					rptr->sd = csd = cs_data;
				}
			}
			if (newjnlfiles)
			{
				jnl_info.no_prev_link = TRUE;
				jnl_info.no_rename = !(FILE_PRESENT & new_stat_res);
				if (curr_jnl_present)
				{
					if (!(jnl_info.repl_state == repl_open && repl_open != repl_curr_state))
					{ /* record the back link */
#						ifdef VMS
						if (!jnlname_same && (FILE_PRESENT & new_stat_res))
						{
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4)
									ERR_FILEEXISTS, 2, jnl_info.jnl_len, jnl_info.jnl);
							gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4)
									ERR_JNLNOCREATE, 2, jnl_info.jnl_len, jnl_info.jnl);
							exit_status |= EXIT_ERR;
							break;
						}
#						endif
						if (keep_prev_link)
						{	/* Save journal link */
							jnl_info.prev_jnl_len = csd->jnl_file_len;
							memcpy(prev_jnl_fn, csd->jnl_file_name, jnl_info.prev_jnl_len);
							prev_jnl_fn[jnl_info.prev_jnl_len] = '\0';
							jnl_info.no_prev_link = FALSE;
						}
					}
				}
				if ((jnl_closed == jnl_curr_state) && (NULL != cs_addrs->nl))
				{ /* Cleanup the jnl file info in shared memory before switching journal file.
				     This case occurs if mupip set -journal is run after jnl_file_lost() closes
				     journaling on a region */
					NULLIFY_JNL_FILE_ID(cs_addrs);
				}
				jnl_info.blks_to_upgrd = csd->blks_to_upgrd;
				jnl_info.free_blocks   = csd->trans_hist.free_blocks;
				jnl_info.total_blks    = csd->trans_hist.total_blks;
				GTMCRYPT_ONLY(
					jnl_info_ptr = &jnl_info;
					GTMCRYPT_COPY_HASH(csd, jnl_info_ptr);
				)
                                if (EXIT_NRM != (status = cre_jnl_file(&jnl_info)))
				{	/* There was an error attempting to create the journal file */
					exit_status |= status;
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_JNLNOCREATE, 2,
							jnl_info.jnl_len, jnl_info.jnl);
					continue;
				}
				csd->jnl_checksum = jnl_info.checksum;
				csd->jnl_eovtn = csd->trans_hist.curr_tn;
				gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(10) ERR_JNLCREATE, 8, jnl_info.jnl_len, jnl_info.jnl,
					db_or_reg_len, db_or_reg, db_reg_name_len, db_reg_name,
					LEN_AND_STR(before_image_lit[(jnl_info.before_images ? 1 : 0)]));
				if ((!curr_jnl_present && (jnl_open == jnl_curr_state))
					|| (curr_jnl_present && jnl_info.no_prev_link) || this_iter_prevlinkcut_error)
				{
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_PREVJNLLINKCUT, 4,
						jnl_info.jnl_len, jnl_info.jnl, DB_LEN_STR(gv_cur_region));
					send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_PREVJNLLINKCUT, 4,
						jnl_info.jnl_len, jnl_info.jnl, DB_LEN_STR(gv_cur_region));
				}
                        }
			/* Following jnl_before_image, jnl_state, repl_state are unique charecteristics per region */
			csd->jnl_before_image = jnl_info.before_images;
			csd->jnl_state = rptr->jnl_new_state;
			csd->repl_state = jnl_info.repl_state;
			/* All fields below are same for all regions */
			csd->jnl_alq = jnl_info.alloc;
			csd->alignsize = jnl_info.alignsize;
			csd->autoswitchlimit = jnl_info.autoswitchlimit;
			csd->jnl_buffer_size = jnl_buffer_size;
			if (jnl_buffer_updated)
				if (jnl_buffer_invalid)
				{
					SNPRINTF(s, JNLBUFFUPDAPNDX_SIZE, JNLBUFFUPDAPNDX, JNL_BUFF_PORT_MIN(csd), JNL_BUFFER_MAX);
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(10)
						(region ? ERR_JNLBUFFREGUPD : ERR_JNLBUFFDBUPD), 4,
						(region ? gv_cur_region->rname_len : jnl_info.fn_len),
						(region ? gv_cur_region->rname : jnl_info.fn),
						jnl_info.buffer, jnl_buffer_size, ERR_TEXT, 2, LEN_AND_STR(s));
				} else
					gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6)
						(region ? ERR_JNLBUFFREGUPD : ERR_JNLBUFFDBUPD), 4,
						(region ? gv_cur_region->rname_len : jnl_info.fn_len),
						(region ? gv_cur_region->rname : jnl_info.fn),
						jnl_info.buffer, jnl_buffer_size);
			csd->epoch_interval = jnl_info.epoch_interval;
			csd->jnl_deq = jnl_info.extend;
			memcpy(csd->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len);
			csd->jnl_file_len = jnl_info.jnl_len;
			csd->jnl_file_name[jnl_info.jnl_len] = 0;
			csd->reg_seqno = jnl_info.reg_seqno;
			if (jnl_options.sync_io_specified)
				csd->jnl_sync_io = jnl_options.sync_io;
			UNIX_ONLY(csd->yield_lmt = jnl_options.yield_limit;)
		} else
		{	/* Journaling is to be disabled for this region. Reset all fields */
			csd->jnl_before_image = FALSE;
			csd->jnl_state = jnl_notallowed;
			csd->repl_state = repl_closed;
			csd->jnl_alq = 0;
			csd->alignsize = 0;
			csd->autoswitchlimit = 0;
			csd->jnl_buffer_size = 0;
			csd->epoch_interval = 0;
			csd->jnl_deq = 0;
			csd->jnl_file_len = 0;
			csd->jnl_sync_io = FALSE;
			UNIX_ONLY(csd->yield_lmt = DEFAULT_YIELD_LIMIT;)
		}
		if (CLI_ABSENT != jnl_options.cli_journal || CLI_ABSENT != jnl_options.cli_replic_on)
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_JNLSTATE, 6, db_or_reg_len, db_or_reg, db_reg_name_len,
					db_reg_name, LEN_AND_STR(jnl_state_lit[rptr->jnl_new_state]));
		if (CLI_ABSENT != jnl_options.cli_replic_on)
			gtm_putmsg_csa(CSA_ARG(cs_addrs) VARLSTCNT(8) ERR_REPLSTATE, 6, db_or_reg_len, db_or_reg, db_reg_name_len,
					db_reg_name, LEN_AND_STR(repl_state_lit[jnl_info.repl_state]));
		/* Write the updated information back to the database file */
		fc->op = FC_WRITE;
		fc->op_buff = (sm_uc_ptr_t)csd;
		fc->op_len = SGMNT_HDR_LEN;
		fc->op_pos = 1;
		status = dbfilop(fc);
		if (SS_NORMAL != status)
		{
			DBFILOP_FAIL_MSG(status, ERR_DBFILERR);
			exit_status |= EXIT_ERR;
			break;
		}
	}
	jgbl.dont_reset_gbl_jrec_time = FALSE;
	/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions */
	assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
	REVERT;
	mupip_set_jnl_cleanup();
	if (EXIT_NRM == exit_status)
		return (uint4)SS_NORMAL;
	if (exit_status & EXIT_WRN)
		return (uint4)MAKE_MSG_WARNING(ERR_MUNOFINISH);
	return (uint4)ERR_MUNOFINISH;
}
