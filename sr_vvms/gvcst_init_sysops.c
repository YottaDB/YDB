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

#include <descrip.h>
#include <dvidef.h>
#include <fab.h>
#include <iodef.h>
#include <lckdef.h>
#include <lkidef.h>
#include <nam.h>
#include <psldef.h>
#include <prvdef.h>
#include <rmsdef.h>
#include <secdef.h>
#include <ssdef.h>
#include <syidef.h>
#include <xabfhcdef.h>
#include <efndef.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "ccp.h"
#include "cryptdef.h"
#include "del_sec.h"
#include "efn.h"
#include "io.h"
#include "error.h"
#include "iottdef.h"
#include "jnl.h"
#include "locks.h"
#include "min_max.h"
#include "mlkdef.h"
#include "vmsdtype.h"
#include "gt_timer.h"
#include "util.h"
#include "mlk_shr_init.h"
#include "dbfilop.h"
#include "gvcst_protos.h"	/* for gvcst_init_sysops prototype */
#include "mem_list.h"
#include "gv_match.h"
#include "init_sec.h"
#include "gvcmy_open.h"
#include "semwt2long_handler.h"
#include "gtmmsg.h"
#include "shmpool.h"
#include "send_msg.h"
#include "gtmimagename.h"
#include "have_crit.h"

#define DEF_NODE	0xFFFF
#define MAX_SEM_WT	(1000 * 30)      /* 30 second wait before DSE errors out for access control lock */
#define PRT$C_NA	0

typedef struct {
	item_list_3 ilist;
	int4 terminator;
} syistruct;

OS_PAGE_SIZE_DECLARE

GBLREF  boolean_t       	dse_running;
GBLREF	gd_region		*gv_cur_region;
GBLREF	short			crash_count;
GBLREF	boolean_t		gtm_fullblockwrites;	/* Do full (not partial) database block writes T/F */
GBLREF	uint4			process_id;

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

/* The following set of macros works around the fact that older VMS compilers do not support
 * macros with variable number of parameters (a.k.a. variadic macros) that we use to
 * implement the UNIX counterpart of PRINT_CRASH_MESSAGE in gvcst_ini_sysops.c.
 */
#define PRINT_CRASH_MESSAGE_2_ARGS(ARG1, ARG2)						\
	PRINT_CRASH_MESSAGE(2, ARG1, ARG2, NULL, NULL, NULL, NULL, NULL, NULL)

#define PRINT_CRASH_MESSAGE_6_ARGS(ARG1, ARG2, ARG3, ARG4, ARG5, ARG6)			\
	PRINT_CRASH_MESSAGE(6, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, NULL, NULL)

#define PRINT_CRASH_MESSAGE_8_ARGS(ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)	\
	PRINT_CRASH_MESSAGE(8, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)

#define VARIADIC_RTS_ERROR(CNT, ERROR, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)	\
{											\
	if (2 == CNT)									\
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERROR, 4, DB_LEN_STR(reg),	\
			ARG1, ARG2);							\
	else if (6 == CNT)								\
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(10) ERROR, 4, DB_LEN_STR(reg),	\
			ARG1, ARG2, ARG3, ARG4, ARG5, ARG6);				\
	else if (8 == CNT)								\
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERROR, 4, DB_LEN_STR(reg),	\
			ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);		\
}

/* Depending on whether journaling and/or replication was enabled at the time of the crash,
 * print REQRUNDOWN, REQRECOV, or REQROLLBACK error message.
 */
#define PRINT_CRASH_MESSAGE(CNT, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8)	\
{											\
	if (JNL_ENABLED(tsd))								\
	{										\
		if (REPL_ENABLED(tsd) && tsd->jnl_before_image)				\
		{									\
			VARIADIC_RTS_ERROR(CNT, ERR_REQROLLBACK, 			\
				ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);	\
		} else									\
		{									\
			VARIADIC_RTS_ERROR(CNT, ERR_REQRECOV,				\
				ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);	\
		}									\
	} else										\
	{										\
		VARIADIC_RTS_ERROR(CNT, ERR_REQRUNDOWN, 				\
			ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7, ARG8);		\
	}										\
}

error_def(ERR_BADGBLSECVER);
error_def(ERR_CLSTCONFLICT);
error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBFILERR);
error_def(ERR_DBOPNERR);
error_def(ERR_FILEIDGBLSEC);
error_def(ERR_GBLSECNOTGDS);
error_def(ERR_JNLBUFFREGUPD);
error_def(ERR_MUPRECFLLCK);
error_def(ERR_NETDBOPNERR);
error_def(ERR_NLMISMATCHCALC);
error_def(ERR_REQRECOV);
error_def(ERR_REQROLLBACK);
error_def(ERR_REQRUNDOWN);
error_def(ERR_SEMWT2LONG);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

gd_region *dbfilopn(gd_region *reg)
{
	gd_region	*prev_reg, *sav_reg;
	gd_segment	*seg;
	file_control	*fc;
	uint4		status, sub_status;
	vms_gds_info	*gds_info;

	sav_reg = gv_cur_region;
	gv_cur_region = reg;
	seg = reg->dyn.addr;
	assert((dba_bg == seg->acc_meth) || (dba_mm == seg->acc_meth));
	FILE_CNTL_INIT_IF_NULL(seg);
	gds_info = seg->file_cntl->file_info;
	fc = seg->file_cntl;
	fc->file_type = seg->acc_meth;
	fc->op = FC_OPEN;
	if (ERR_NETDBOPNERR == (status = dbfilop(fc)))
	{
		gv_cur_region = sav_reg;
		gvcmy_open(reg, gds_info->nam);
		return -1;
	}
	if (SS$_NORMAL != status)
	{
		sub_status = gds_info->fab->fab$l_stv;
		free(gds_info->fab);
		free(gds_info->nam->nam$l_esa);
		free(gds_info->nam);
		free(gds_info->xabfhc);
		free(gds_info->xabpro);
		free(seg->file_cntl->file_info);
		free(seg->file_cntl);
		seg->file_cntl = NULL;
		gv_cur_region = sav_reg;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBOPNERR, 2, DB_LEN_STR(reg), status, sub_status);
		GTMASSERT;
	}
	if (NULL != (prev_reg = gv_match(reg)))
	{
		sys$dassgn(gds_info->fab->fab$l_stv);
		free(gds_info->fab);
		free(gds_info->nam->nam$l_esa);
		free(gds_info->nam);
		free(gds_info->xabfhc);
		free(gds_info->xabpro);
		free(seg->file_cntl->file_info);
		free(seg->file_cntl);
		seg->file_cntl = NULL;
		reg = prev_reg;
	}
	gv_cur_region = sav_reg;
	return reg;
}

void dbsecspc(gd_region *reg, sgmnt_data *sd, gtm_uint64_t *sec_size)
{
	int			section_size;
	int4			inadr[2], inadr1[2];
	uint4			status;
	vms_gds_info		*gds_info;

	gds_info = FILE_INFO(reg);
	switch (reg->dyn.addr->acc_meth)
	{
	case dba_mm:
		*sec_size = gds_info->xabfhc->xab$l_ebk;
		break;

	case dba_bg:
		*sec_size = DIVIDE_ROUND_UP((LOCK_BLOCK_SIZE(sd) * OS_PAGE_SIZE) + LOCK_SPACE_SIZE(sd) + CACHE_CONTROL_SIZE(sd)
			+ NODE_LOCAL_SPACE(sd) + JNL_SHARE_SIZE(sd) + SHMPOOL_BUFFER_SIZE, OS_PAGELET_SIZE);
		break;
	default:
		GTMASSERT;
	}
	/* sys$expreg's first argument is expressed in pages on VAX VMS (512 bytes each), but in "pagelets" on
	   Alpha VMS (also 512 bytes each).  Since the region will be expanded by an integral number of pages,
	   round up sec_size to a multiple of the page size, then add two full pages for protection. */
	section_size = ROUND_UP(*sec_size, OS_PAGE_SIZE / OS_PAGELET_SIZE);
	status = gtm_expreg(section_size + 2 * OS_PAGE_SIZE / OS_PAGELET_SIZE, inadr, PSL$C_USER, 0);
	if (SS$_NORMAL != status)
	{
		sys$dassgn(gds_info->fab->fab$l_stv);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2,
			gds_info->fab->fab$b_fns, gds_info->fab->fab$l_fna, status);
	}
	gds_info->s_addrs.db_addrs[0] = inadr[0] + OS_PAGE_SIZE;
	gds_info->s_addrs.db_addrs[1] = inadr[1] - OS_PAGE_SIZE;
	assert(gds_info->s_addrs.db_addrs[1] == gds_info->s_addrs.db_addrs[0] + section_size * OS_PAGELET_SIZE - 1);
	/* GUARD THE BEGINNING OF THE DATA BASE SEGMENT WITH ONE NOACCESS PAGE */
	inadr1[0] = inadr1[1]
		  = inadr[0];
	status = sys$setprt(inadr1, NULL, 0, PRT$C_NA, 0);
	if (SS$_NORMAL == status)
	{
		/* GUARD THE END OF THE DATA BASE SEGMENT WITH ONE NOACCESS PAGE */
		inadr1[0] = inadr1[1]
			  = gds_info->s_addrs.db_addrs[1] + 1;
		status = sys$setprt(inadr1, NULL, PSL$C_USER, PRT$C_NA, NULL);
	}
	if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2,
			gds_info->fab->fab$b_fns, gds_info->fab->fab$l_fna, status);
	return;
}

void db_init(gd_region *reg, sgmnt_data *tsd)
{
	boolean_t		clustered, cluster_member, mupip_jnl_recover, read_write, is_bg, lock_released = FALSE;
	char			name_buff[GLO_NAME_MAXLEN], node_buff[14], nodename_buff[16], local_nodename_buff[16];
	sgmnt_addrs		*csa;
	sgmnt_data		*nsd;
	file_control		*fc;
	struct dsc$descriptor_s name_dsc, node_dsc;
	syistruct		syi_list;
        int             	index, dblksize, fbwsize, item_code;
	uint4			efn_mask, flags, i, init_status, node, size, status;
        uint4           	lk_pid = 0;
	unsigned short		iosb[4], retlen, local_nodename_len;
	vms_gds_info		*gds_info;
	vms_lock_sb		*file_lksb;
	uint4			get_lkpid(struct dsc$descriptor_s *, int, uint4);
	struct
	{
		item_list_3	ilist[2];
		int4		terminator;
	} syi_list_2;
	struct
        {
                item_list_3     item[1];
                int4            terminator;
        } lk_pid_list;
	char			now_running[MAX_REL_NAME];
	gtm_uint64_t		sec_size;
	int			jnl_buffer_size;
	char			s[JNLBUFFUPDAPNDX_SIZE];	/* JNLBUFFUPDAPNDX_SIZE is defined in jnl.h */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
        ESTABLISH(dbinit_ch);
	assert(INTRPT_IN_GVCST_INIT == intrpt_ok_state); /* we better be called from gvcst_init */
	TREF(new_dbinit_ipc) = FALSE;	/* we did not create a new ipc resource */
	/* ------------------------------------- gather information --------------------------------------- */
	assert((dba_bg == tsd->acc_meth) || (dba_mm == tsd->acc_meth));
	is_bg = (dba_bg == tsd->acc_meth);
	clustered = (dba_bg == tsd->clustered && tsd->acc_meth);		/* ??? */
	read_write = (FALSE == reg->read_only);
	node = 0;
	syi_list_2.ilist[0].item_code = SYI$_NODE_CSID;
	syi_list_2.ilist[0].buffer_address = &node;
	syi_list_2.ilist[0].buffer_length = SIZEOF(node);
	syi_list_2.ilist[0].return_length_address = &retlen;
	syi_list_2.ilist[1].item_code = SYI$_NODENAME;
	syi_list_2.ilist[1].buffer_address = local_nodename_buff;
	syi_list_2.ilist[1].buffer_length = SIZEOF(local_nodename_buff) - 1;
	syi_list_2.ilist[1].return_length_address = &local_nodename_len;
	syi_list_2.terminator = 0;
	status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &syi_list_2, iosb, NULL, 0);
	if (SS$_NORMAL == status)
		status = iosb[0];
	if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_CLSTCONFLICT, 4, DB_LEN_STR(reg),
			0, local_nodename_buff, status);
	if (0 == node)
		node = DEF_NODE;
	if (-1 != reg->node)		/* -1 is a flag indicating that a MUPIP JOURNAL /RECOVER is in progress */
		reg->node = node;	/* Leave it so that it goes into the value block */

	/* --------------------------- grab the startup/rundown lock on file --------------------------- */
	gds_info = FILE_INFO(reg);
	file_lksb = &gds_info->file_cntl_lsb;
	global_name("GT$S", &gds_info->file_id, name_buff);
	name_dsc.dsc$a_pointer = &name_buff[1];
	name_dsc.dsc$w_length = name_buff[0];
	name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	name_dsc.dsc$b_class = DSC$K_CLASS_S;
	/* These locks must be taken out before mapping the file to a section, and released after unmapping the section
	 * Note:  Rather than simply taking out this lock at PW mode, we take it out at NL mode and then convert to PW.
	 * Lock requests in the conversion queue are serviced before locks in the waiting queue;  heavy GT.CX activity
	 * on a given database can potentially keep the conversion queue busy enough to keep new lock requests (especially
	 * at higher lock modes like PW) bottled up on the waiting queue indefinitely.	Since NL mode lock requests are
	 * compatible with all other lock modes, and since they don't go to the waiting queue if LCK$M_EXPEDITE is specified;
	 * they are always granted. Then the subsequent conversion request will rapidly move to the head of the conversion
	 * queue and ultimately be granted.
	 */
	assert((0 == file_lksb->lockid) || (!IS_GTM_IMAGE));
	if (0 == file_lksb->lockid)
	{
		mupip_jnl_recover = FALSE;
		/* NL mode lock is granted immediately only if LCK$M_EXPEDITE is specified. If there is a waiting queue
		 * for this lock and LCK$M_EXPEDITE is not specified, we might not be granted this NL mode lock immediately.
		 */
		status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, file_lksb, LCK$M_SYSTEM | LCK$M_EXPEDITE,
								&name_dsc, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
		{
			if (dse_running)
			{
				/* try to acquire the lock synchronously without waiting */
				status = gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, file_lksb, LCK$M_VALBLK | LCK$M_CONVERT |
						LCK$M_NODLCKBLK | LCK$M_NOQUEUE, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
				if (SS$_NORMAL == status)
					status = file_lksb->cond;
				else if (SS$_NOTQUEUED == status)
				{	/* someone else is holding the lock */
					assert(0 != file_lksb->lockid && -1 != file_lksb->lockid);
					status = get_lkpid(&name_dsc, LCK$K_PWMODE, &lk_pid);
					if (SS$_NORMAL != status)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_TEXT, 2,
							RTS_ERROR_LITERAL("Failed to get lock pid"),
						ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), status);
					} /* no 'else' needed, since the process wont come down after the rts_error */
					if (lk_pid)
						util_out_print("Access control lock for region !AD held by pid, !UL. "
								"An attempt will be made in the next 30 seconds to grab it.",
								TRUE, DB_LEN_STR(reg), lk_pid);
					TREF(semwait2long) = FALSE;
					start_timer(semwt2long_handler, MAX_SEM_WT, semwt2long_handler, 0, NULL);
					/* do an asynchronous enq and then wait using wflor() */
					status = gtm_enq(efn_immed_wait, LCK$K_PWMODE, file_lksb, LCK$M_VALBLK |
						LCK$M_CONVERT | LCK$M_NODLCKBLK, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
					if (SS$_NORMAL != status)
					{
						cancel_timer(semwt2long_handler);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_TEXT, 2,
							RTS_ERROR_LITERAL("Failed to register an asynchronous enq"),
							ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), status);
					}
					/* wait for the timer to expire or for the above enq() to finish */
					efn_mask = ((SHFT_MSK << efn_timer) | (SHFT_MSK << efn_immed_wait));
					status = sys$wflor(efn_immed_wait, efn_mask);
					if (SS$_NORMAL != status)
					{
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_TEXT, 2,
							RTS_ERROR_LITERAL("Wait-for-logical-or failed"), ERR_CRITSEMFAIL,
							2, DB_LEN_STR(reg), status);
					}
					if (SS$_NORMAL != file_lksb->cond)
					{
						if (TREF(semwait2long))
						{
							status = get_lkpid(&name_dsc, LCK$K_PWMODE, &lk_pid);
							if (SS$_NORMAL != status)
							{
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_TEXT, 2,
								RTS_ERROR_LITERAL("Failed to get lock pid"), ERR_CRITSEMFAIL, 2,
								DB_LEN_STR(reg), status);
							}
							if (0 != lk_pid)
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_SEMWT2LONG, 7,
									process_id, (MAX_SEM_WT / 1000),
									LEN_AND_LIT("access control"), DB_LEN_STR(reg), lk_pid);
							else
								rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4)
									ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
						} else
						{
							cancel_timer(semwt2long_handler);
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4)
								ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
						}
					} else
					{
						status = file_lksb->cond;
						if (!TREF(semwait2long))
							cancel_timer(semwt2long_handler);
					}
				}
			} else
			{
				status = gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, file_lksb,
					LCK$M_VALBLK | LCK$M_CONVERT | LCK$M_NODLCKBLK, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
				if (SS$_NORMAL == status)
					status = file_lksb->cond;
			}
		}
		if ((SS$_NORMAL != status) && (SS$_VALNOTVALID != status))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		if ((SS$_NORMAL == status) && (0 != file_lksb->valblk[0]) && (-1 != reg->node))
		{
			if (file_lksb->valblk[0] != node)
			{
				if (-1 == file_lksb->valblk[0])
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUPRECFLLCK, 2, DB_LEN_STR(reg));
				if (FALSE == clustered)
				{
					syi_list.ilist.item_code = SYI$_NODENAME;
					syi_list.ilist.buffer_address = nodename_buff;
					syi_list.ilist.buffer_length = SIZEOF(nodename_buff) - 1;
					syi_list.ilist.return_length_address = &retlen;
					syi_list.terminator = 0;
					status = sys$getsyiw(EFN$C_ENF, file_lksb->valblk, NULL, &syi_list, iosb, NULL, 0);
					if (SS$_NORMAL == status)
						status = iosb[0];
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_CLSTCONFLICT, 4, DB_LEN_STR(reg),
						(SS$_NORMAL == status) ? strlen(nodename_buff) : 0, nodename_buff);
				}
			}
		} else
			file_lksb->valblk[0] = 0;
	} else	if (IS_MUPIP_IMAGE)
		mupip_jnl_recover = TRUE;

	/* ------------------------------------ GT.CX: grab the lock on node --------------------------------------- */
	memcpy(node_buff, "GT$N_", 5);
	i2hex(node, &node_buff[5], 8);
	if (TRUE == clustered)
		node_dsc.dsc$w_length = 13;
	else
	{
		node_buff[13] = read_write ? 'W' : 'R';
		node_dsc.dsc$w_length = 14;
	}
	node_dsc.dsc$a_pointer = node_buff;
	node_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	node_dsc.dsc$b_class = DSC$K_CLASS_S;
	status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, &gds_info->cx_cntl_lsb, LCK$M_SYSTEM | LCK$M_EXPEDITE,
			&node_dsc, file_lksb->lockid, NULL, 0, NULL, PSL$C_USER, 0);
	if (SS$_NORMAL == status)
	{
		status = gtm_enqw(EFN$C_ENF, LCK$K_CRMODE, &gds_info->cx_cntl_lsb, LCK$M_CONVERT,
				&node_dsc, file_lksb->lockid, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = gds_info->cx_cntl_lsb.cond;
	}
	if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);

	/* --------------------------- Re-read the fileheader inside the lock -------------------------------- */
	fc = reg->dyn.addr->file_cntl;
	fc->file_type = reg->dyn.addr->acc_meth;
	fc->op = FC_READ;
	fc->op_buff = (sm_uc_ptr_t)tsd;
	fc->op_len = SIZEOF(*tsd);
	fc->op_pos = 1;
	status = dbfilop(fc);
	if (SS$_NORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBFILERR, 2, DB_LEN_STR(reg), status,
			gds_info->fab->fab$l_stv);

	/* ----------------------------- init sections and establish pointers in csa --------------------- */
	/* Since we are about to allocate new shared memory, if necessary, adjust the journal buffer size right now.
	 * Note that if the process setting up shared memory is a read-only process, then we might not flush updated
	 * jnl_buffer_size to the file header, which is fine because the value in shared memory is what all processes
	 * are looking at. If necessary, the next process to initialize shared memory will repeat the process of
	 * adjusting the jnl_buffer_size value.
	 */
	jnl_buffer_size = tsd->jnl_buffer_size;
	if ((0 != jnl_buffer_size) && (jnl_buffer_size < (tsd->blk_size / DISK_BLOCK_SIZE + 1)))
	{
		ROUND_UP_MIN_JNL_BUFF_SIZE(tsd->jnl_buffer_size, tsd);
		SNPRINTF(s, JNLBUFFUPDAPNDX_SIZE, JNLBUFFUPDAPNDX, JNL_BUFF_PORT_MIN(tsd), JNL_BUFFER_MAX);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_JNLBUFFREGUPD, 4, REG_LEN_STR(reg),
			jnl_buffer_size, tsd->jnl_buffer_size, ERR_TEXT, 2, LEN_AND_STR(s));
	}
	/* The layout of shared memory depends on the number of mutex queue entries specified in the file header. Thus in
	 * order to set, for example, csa->critical or csa->shmpool_buffer, we need to know this number. However, this
	 * number can be zero if we have not yet done db_auto_upgrade. So go ahead and upgrade to the value that will
	 * eventually be used, which is DEFAULT_NUM_CRIT_ENTRY.
	 */
	if (0 == NUM_CRIT_ENTRY(tsd))
		NUM_CRIT_ENTRY(tsd) = DEFAULT_NUM_CRIT_ENTRY;
	dbsecspc(reg, tsd, &sec_size);		/* calculate section size (filesize for MM) */
	csa = &gds_info->s_addrs;
	flags = SEC$M_GBL | SEC$M_SYSGBL;
	if (is_bg)
		flags |= SEC$M_WRT | SEC$M_PAGFIL | SEC$M_PERM;
	else if (read_write)
		flags |= SEC$M_WRT;
	init_status = init_sec(csa->db_addrs, &name_dsc, gds_info->fab->fab$l_stv, sec_size, flags);
	if ((SS$_NORMAL != init_status)
		&& ((SS$_CREATED != init_status) || ((0 != file_lksb->valblk[0]) && (FALSE == clustered))))
	{
		if (SS$_CREATED == init_status)
		{
			del_sec(SEC$M_SYSGBL, &name_dsc, NULL);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, LEN_AND_LIT("Check for 'delete pending' sections - stop all accessors"));
		} else
		{
			assert(FALSE);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, LEN_AND_LIT("Error initializing shared memory"), init_status);
		}
	}
	if (is_bg)
		csa->nl = csa->db_addrs[0];
	else
	{
		size = ROUND_UP(LOCK_SPACE_SIZE(tsd) + NODE_LOCAL_SPACE(tsd) + JNL_SHARE_SIZE(tsd) + SHMPOOL_BUFFER_SIZE,
					OS_PAGE_SIZE) / OS_PAGELET_SIZE;
		status = gtm_expreg(size, csa->lock_addrs, PSL$C_USER, 0);
		if (SS$_NORMAL != status)
		{
			csa->lock_addrs[0] = NULL;
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		}
		assert(csa->lock_addrs[0] + size * OS_PAGELET_SIZE - 1 == csa->lock_addrs[1]);
		name_buff[4] = 'L';
		status = init_sec(csa->lock_addrs, &name_dsc, 0, size, SEC$M_PAGFIL | SEC$M_GBL | SEC$M_WRT | SEC$M_SYSGBL);
		if ((SS$_NORMAL != status) && ((SS$_CREATED != status) || (SS$_NORMAL == init_status)))
		{
			if (SS$_CREATED == status)
			{
				del_sec(SEC$M_SYSGBL, &name_dsc, NULL);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Check for 'delete pending' sections - stop all accessors"));
			} else
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, LEN_AND_LIT("Error initializing MM non-file shared memory"), status);
		}
		csa->nl = csa->lock_addrs[0];
	}
	/* If shared memory is already initialized, do VERMISMATCH check BEFORE referencing any other fields in shared memory. */
	if (SS$_CREATED == init_status)
		TREF(new_dbinit_ipc) = TRUE;
	else if (memcmp(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1))
	{	/* Copy csa->nl->now_running into a local variable before passing to rts_error() due to the following issue.
		 * In VMS, a call to rts_error copies only the error message and its arguments (as pointers) and
		 *  transfers control to the topmost condition handler which is dbinit_ch() in this case. dbinit_ch()
		 *  does a PRN_ERROR only for SUCCESS/INFO (VERMISMATCH is neither of them) and in addition
		 *  nullifies csa->nl as part of its condition handling. It then transfers control to the next level condition
		 *  handler which does a PRN_ERROR but at that point in time, the parameter csa->nl->now_running is no longer
		 *  accessible and hence no parameter substitution occurs (i.e. the error message gets displayed with plain !ADs).
		 * In UNIX, this is not an issue since the first call to rts_error() does the error message
		 *  construction before handing control to the topmost condition handler. But it does not hurt to do the copy.
		 */
		assert(strlen(csa->nl->now_running) < SIZEOF(now_running));
		memcpy(now_running, csa->nl->now_running, SIZEOF(now_running));
		now_running[SIZEOF(now_running) - 1] = '\0';	/* protection against bad values of csa->nl->now_running */
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(reg), gtm_release_name_len, gtm_release_name,
			  LEN_AND_STR(now_running));
	}
	/* Neither the creator, nor the first-writer ==> Done with mod to shared memory ==> release the lock */
	if ((0 != file_lksb->valblk[0]) && ((!read_write) || (csa->nl->ref_cnt > 0)))
	{
		if (read_write)
		{
			adawi(1, &csa->nl->ref_cnt);
			assert(!csa->ref_cnt);	/* Increment shared ref_cnt before private ref_cnt increment. */
			csa->ref_cnt++;		/* Currently journaling logic in gds_rundown() in VMS relies
						 * 	on this order to detect last writer */
			assert(csa->read_write);
		}
		if (FALSE == clustered)
			status = gtm_enqw(EFN$C_ENF, LCK$K_CRMODE, file_lksb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
					NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		else
			status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, file_lksb, LCK$M_CONVERT,
					NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		lock_released = TRUE;
		if (SS$_NORMAL == status)
			status = file_lksb->cond;
		if (SS$_NORMAL != status)
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
	}
	csa->critical = (sm_uc_ptr_t)(csa->nl) + NODE_LOCAL_SIZE;
	crash_count = csa->critical->crashcnt;	/* done in gvcst_init(), but needed before that for call to grab_crit() below */
	/* Note: Here we check jnl_sate from database file and its value cannot change without standalone access.
	 * The jnl_buff buffer should be initialized irrespective of read/write process */
	JNL_INIT(csa, reg, tsd);
	csa->shmpool_buffer = (shmpool_buff_hdr_ptr_t)((sm_uc_ptr_t)(csa->nl) + NODE_LOCAL_SPACE(tsd) + JNL_SHARE_SIZE(tsd));
	csa->lock_addrs[0] = (sm_uc_ptr_t)(csa->shmpool_buffer) + SHMPOOL_BUFFER_SIZE;
	csa->lock_addrs[1] = csa->lock_addrs[0] + LOCK_SPACE_SIZE(tsd) - 1;
	if (is_bg)
	{
		nsd = csa->hdr = csa->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(tsd);
		if (SS$_CREATED == init_status)
		{
			memcpy((uchar_ptr_t)nsd, (uchar_ptr_t)tsd, SIZEOF(sgmnt_data));
			fc->file_type = dba_bg;
			fc->op = FC_READ;
			fc->op_buff = MM_ADDR(nsd);
			fc->op_len = MASTER_MAP_SIZE(nsd);
			fc->op_pos = MM_BLOCK;
			status = dbfilop(fc);
			if (SS$_NORMAL != status)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBFILERR, 2, DB_LEN_STR(reg), status,
					gds_info->fab->fab$l_stv);
			if (nsd->owner_node)
			{	/* Crash occurred. */
				PRINT_CRASH_MESSAGE_2_ARGS(strlen((char *)local_nodename_buff), local_nodename_buff);
			}
		}
	} else
	{
		nsd = csa->hdr = csa->db_addrs[0];
		SET_MM_BASE_ADDR(csa, nsd);
	}
	csa->total_blks = nsd->trans_hist.total_blks;

	/* ensure correct alignment */
	assert((-(SIZEOF(int4) * 2) & (int)csa->critical) == csa->critical);
	assert(0 == ((OS_PAGE_SIZE - 1) & (int)csa->critical));
	assert(0 == ((OS_PAGE_SIZE - 1) & (int)csa->nl));
	assert((!(JNL_ALLOWED(csa))) ||
			(0 == ((OS_PAGE_SIZE - 1) & (int)((sm_uc_ptr_t)(csa->jnl->jnl_buff) - JNL_NAME_EXP_SIZE))));
	assert(0 == ((OS_PAGE_SIZE - 1) & (int)csa->shmpool_buffer));
	assert(0 == ((OS_PAGE_SIZE - 1) & (int)csa->lock_addrs[0]));
	assert(0 == ((OS_PAGE_SIZE - 1) & (int)(csa->lock_addrs[1] + 1)));
	assert(0 == ((OS_PAGE_SIZE - 1) & (int)csa->hdr));

	/* ----------------------------- setup shared memory if needed -------------------------- */
	if (SS$_CREATED == init_status)
	{	/* initialize if new */
		if (!clustered)
			memset(csa->nl, 0, SIZEOF(*csa->nl));
		if (JNL_ALLOWED(csa))
		{	/* initialize jb->cycle to a value different from initial value of jpc->cycle (0). although this is not
			 * necessary right now, in the future, the plan is to change jnl_ensure_open() to only do a cycle mismatch
			 * check in order to determine whether to call jnl_file_open() or not. this is in preparation for that.
			 */
			csa->jnl->jnl_buff->cycle = 1;
		}
		memcpy(csa->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1);				/* Database label */
		assert(MAX_REL_NAME > gtm_release_name_len);
		memcpy(csa->nl->now_running, gtm_release_name, gtm_release_name_len + 1);	/* GT.M version */
		assert(MAX_FN_LEN > reg->dyn.addr->fname_len);
		memcpy(csa->nl->fname, reg->dyn.addr->fname, reg->dyn.addr->fname_len);		/* file name */
		memcpy(&csa->nl->unique_id.file_id[0], &(gds_info->file_id), SIZEOF(gds_file_id));		/* file id */
		assert(MAX_MCNAMELEN > local_nodename_len);
		memcpy(csa->nl->machine_name, local_nodename_buff, local_nodename_len);		/* node name */
		csa->nl->owner_node = node;							/* node id */
		csa->nl->wcs_staleness = -1;
		csa->nl->wcs_timers = -1;
		csa->nl->highest_lbm_blk_changed = -1;
		csa->nl->nbb = BACKUP_NOT_IN_PROGRESS;
		shmpool_buff_init(reg);
		csa->nl->sec_size = sec_size;							/* Set the shared memory size */

		/* save pointers in csa to access shared memory */
		csa->nl->critical = (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl);
		if (JNL_ALLOWED(csa))
			csa->nl->jnl_buff = (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl);
		csa->nl->shmpool_buffer = (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl);
		if (is_bg)
			/* In MM mode, it is possible that (eventually) shared mem and mapped mem could be > 4G apart */
			csa->nl->hdr = (sm_off_t)((sm_uc_ptr_t)csa->hdr - (sm_uc_ptr_t)csa->nl);
		csa->nl->lock_addrs = (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl);

		mlk_shr_init(csa->lock_addrs[0], nsd->lock_space_size, csa, read_write);

		/* At this point, nsd->owner_node may indicate improper shutdown;  see if it's safe to continue */
		if ((0 != nsd->owner_node) && (nsd->owner_node != node))
		{
			syi_list_2.ilist[0].item_code = SYI$_CLUSTER_MEMBER;
			syi_list_2.ilist[0].buffer_address = &cluster_member;
			syi_list_2.ilist[0].buffer_length = SIZEOF(cluster_member);
			syi_list_2.ilist[0].return_length_address = &i; /* dummy - not used */
			syi_list_2.ilist[1].item_code = SYI$_NODENAME;
			syi_list_2.ilist[1].buffer_address = nodename_buff;
			syi_list_2.ilist[1].buffer_length = SIZEOF(nodename_buff) - 1;
			syi_list_2.ilist[1].return_length_address = &retlen;
			syi_list_2.terminator = 0;
			status = sys$getsyiw(EFN$C_ENF, &nsd->owner_node, NULL, &syi_list_2, iosb, NULL, 0);
			if (SS$_NORMAL == status)
				status = iosb[0];
			if ((SS$_NORMAL != status) && (SS$_NOSUCHNODE != status))
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_CLSTCONFLICT, 4,
					DB_LEN_STR(reg), LEN_AND_STR(nodename_buff), status);
			/*** Temporarily add `&& !clustered' to the following condition,  ****
			**** until a more specific solution to this problem is made, probably in ccp_close1 ***/
			if ((SS$_NORMAL == status) && (TRUE == cluster_member) && (FALSE == clustered))
			{	/* DB was improperly shutdown on another node in the cluster,
				 * and that node is still running; force user to rundown.
				 */
				PRINT_CRASH_MESSAGE_2_ARGS(retlen, nodename_buff);
			}
			if (read_write)
			{
				CHECK_TN(csa, nsd, nsd->trans_hist.curr_tn + HEADER_UPDATE_COUNT); /* can issue TNTOOLARGE error */
				nsd->trans_hist.curr_tn += HEADER_UPDATE_COUNT;
			}
		}
		if (is_bg)
		{
			csa->nl->cache_off = -CACHE_CONTROL_SIZE(nsd);
			db_csh_ini(csa);
			bt_malloc(csa);
			db_csh_ref(csa, TRUE);
			if (0 == nsd->flush_trigger)
				nsd->flush_trigger = FLUSH_FACTOR(nsd->n_bts);
		}
		if (!read_write)
			csa->nl->remove_shm = TRUE;	/* gds_rundown can remove gblsec if first process has read-only access */
		csa->nl->glob_sec_init = TRUE;
		if (read_write || is_bg)
		{	/* add current db_csh counters into the cumulative counters and reset the current counters */
#			define TAB_DB_CSH_ACCT_REC(COUNTER, DUMMY1, DUMMY2)			\
				csa->hdr->COUNTER.cumul_count += csa->hdr->COUNTER.curr_count;	\
				csa->hdr->COUNTER.curr_count = 0;
#			include "tab_db_csh_acct_rec.h"
#			undef TAB_DB_CSH_ACCT_REC
		}
		gvstats_rec_csd2cnl(csa);	/* should be called before "db_auto_upgrade" */
		db_auto_upgrade(reg);		/* should be called before "gtm_mutex_init" to ensure NUM_CRIT_ENTRY is nonzero */
		mutex_init(csa->critical, NUM_CRIT_ENTRY(csa->hdr), FALSE);
	} else
	{
		if (memcmp(csa->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1))
		{
			name_buff[4] = 'S';
			if (memcmp(csa->nl->label, GDS_LABEL, GDS_LABEL_SZ - 3))
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_GBLSECNOTGDS, 2, name_buff[0], &name_buff[1]);
			else
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_BADGBLSECVER, 2, name_buff[0], &name_buff[1]);
		}
		if (memcmp(&csa->nl->unique_id.file_id[0], (char *)(&(gds_info->file_id)), SIZEOF(gds_file_id)))
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_FILEIDGBLSEC, 2, DB_LEN_STR(reg));
		if (csa->nl->donotflush_dbjnl)
		{
			assert(FALSE);
			PRINT_CRASH_MESSAGE_6_ARGS(strlen((char *)csa->nl->machine_name), csa->nl->machine_name, ERR_TEXT, 2,
				SIZEOF("mupip recover/rollback created shared memory. Needs MUPIP RUNDOWN") - 1,
				"mupip recover/rollback created shared memory. Needs MUPIP RUNDOWN");
		}
		/* verify pointers from our calculation vs. the copy in shared memory */
		if (csa->nl->critical != (sm_off_t)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl))
		{
			PRINT_CRASH_MESSAGE_8_ARGS(strlen((char *)csa->nl->machine_name), csa->nl->machine_name,
				ERR_NLMISMATCHCALC, 4, SIZEOF("critical") - 1, "critical",
				(uint4)((sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->critical);
		}
		if ((JNL_ALLOWED(csa)) &&
			(csa->nl->jnl_buff != (sm_off_t)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl)))
		{
			PRINT_CRASH_MESSAGE_8_ARGS(strlen((char *)csa->nl->machine_name), csa->nl->machine_name,
				ERR_NLMISMATCHCALC, 4, SIZEOF("journal buffer") - 1, "journal buffer",
				(uint4)((sm_uc_ptr_t)csa->jnl->jnl_buff - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->jnl_buff);
		}
		if (csa->nl->shmpool_buffer != (sm_off_t)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl))
		{
			PRINT_CRASH_MESSAGE_8_ARGS(strlen((char *)csa->nl->machine_name), csa->nl->machine_name,
				ERR_NLMISMATCHCALC, 4, SIZEOF("backup buffer") - 1, "backup buffer",
				(uint4)((sm_uc_ptr_t)csa->shmpool_buffer - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->shmpool_buffer);
		}
		if ((is_bg) && (csa->nl->hdr != (sm_off_t)((sm_uc_ptr_t)csa->hdr - (sm_uc_ptr_t)csa->nl)))
		{
			PRINT_CRASH_MESSAGE_8_ARGS(strlen((char *)csa->nl->machine_name), csa->nl->machine_name,
				ERR_NLMISMATCHCALC, 4, SIZEOF("file header") - 1, "file header",
				(uint4)((sm_uc_ptr_t)csa->hdr - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->hdr);
		}
		if (csa->nl->lock_addrs != (sm_off_t)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl))
		{
			PRINT_CRASH_MESSAGE_8_ARGS(strlen((char *)csa->nl->machine_name), csa->nl->machine_name,
				ERR_NLMISMATCHCALC, 4, SIZEOF("lock address") - 1, "lock address",
				(uint4)((sm_uc_ptr_t)csa->lock_addrs[0] - (sm_uc_ptr_t)csa->nl), (uint4)csa->nl->lock_addrs);
		}
		/* now_running is checked in gvcst_init, since they are portable */
		if (is_bg)
			db_csh_ini(csa);
	}
	if (gtm_fullblockwrites)
	{	/* We have been asked to do FULL BLOCK WRITES for this database. Unlike *NIX, on VMS, we can get the
		   underlying filsystem block/buffersize with a call to $GETDVI. This allows a full write of a block
		   without the OS having to fetch the old block for a read/update operation. We will round the IOs
		   (only from dsk_write() to the next filesystem blocksize if the following criteria are met:

		   1) Database blocksize must be a whole multiple of the filesystem blocksize for the above
		      mentioned reason.

		   2) Filesystem blocksize must be a factor of the location of the first data block
		      given by start_vbn.

		   The saved length (if the feature is enabled) will be the filesystem blocksize and will be the
		   length that a database IO is rounded up to prior to initiation of the IO.
		*/
		item_code = DVI$_DEVBUFSIZ;
		status = lib$getdvi(&item_code, &gds_info->fab->fab$l_stv, NULL, &fbwsize, NULL, NULL);
		if (SS$_NORMAL == status)
		{
			dblksize = csa->hdr->blk_size;
			if (0 != fbwsize && (0 == dblksize % fbwsize) &&
				(0 == ((csa->hdr->start_vbn - 1) * DISK_BLOCK_SIZE) % fbwsize))
				csa->do_fullblockwrites = TRUE;		/* This region is fullblockwrite enabled */
			/* Report this length in DSE even if not enabled */
			csa->fullblockwrite_len = fbwsize;		/* Length for rounding fullblockwrite */
		}	/* else if lib$getdvi fails.. do non-fullblockwrites */
		else
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("getdvi DEVBUFSIZ"), CALLFROM, status);
	}
	if (!lock_released)
	{
		if (read_write)
		{	/* ========= first writer =========== */
			/* since lock_released was set based on csa->nl->ref_cnt and since the latter is not an accurate
			 * indicator of whether we are the last writer or not, we need to be careful before modifying any
			 * shared memory fields below, hence the grab_crit */
			if (!csa->nl->wc_blocked)
			{	/* if wc_blocked is TRUE and if journaling is enabled, grab_crit below will end up doing
				 * wcs_recover which will try to open the journal file but will fail because cs_addrs
				 * will be NULL since we have not yet opened this region (reg->open is still FALSE).
				 * in that case we want to be safe and avoid doing the grab_crit().
				 * this will be fixed as part of C9E01-002490.
				 */
				bt_init(csa);	/* needed to initialize csa->ti, csa->bt_header, csa->bt_base, csa->th_base */
						/* used by grab_crit() and routines it invokes which are wcs_verify/wcs_recover */
				grab_crit(reg);
			}
			csa->nl->remove_shm = FALSE;
			assert(0 == csa->nl->ref_cnt);	/* ensure no other writer has incremented ref_cnt until now */
			adawi(1, &csa->nl->ref_cnt);
			assert(!csa->ref_cnt);	/* Increment shared ref_cnt before private ref_cnt increment. */
			csa->ref_cnt++;		/* Currently journaling logic in gds_rundown() in VMS relies
						 * 	on this order to detect last writer */
			assert(csa->read_write);
			memcpy(csa->hdr->now_running, gtm_release_name, gtm_release_name_len + 1);
			csa->hdr->owner_node = node;
			nsd->trans_hist.early_tn = nsd->trans_hist.curr_tn;
			nsd->max_update_array_size = nsd->max_non_bm_update_array_size
				= ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(nsd), UPDATE_ARRAY_ALIGN_SIZE);
			nsd->max_update_array_size += ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE, UPDATE_ARRAY_ALIGN_SIZE);
			assert(0 == memcmp(nsd, GDS_LABEL, GDS_LABEL_SZ - 1));
			status = sys$qiow(EFN$C_ENF, gds_info->fab->fab$l_stv, IO$_WRITEVBLK, iosb, NULL, 0,
				nsd, ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE), 1, 0, 0, 0);
			if (SS$_NORMAL == status)
				status = iosb[0];
			if (csa->now_crit)
				rel_crit(reg);
			if (SS$_NORMAL != status)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
		}
		/* releasing the startup/shutdown lock */
		file_lksb->valblk[0] = mupip_jnl_recover ? -1 : reg->node;
		if ((FALSE == clustered) || (TRUE == mupip_jnl_recover))
			status = gtm_enqw(EFN$C_ENF, mupip_jnl_recover ? LCK$K_EXMODE : LCK$K_CRMODE, file_lksb,
					LCK$M_VALBLK | LCK$M_CONVERT | LCK$M_NODLCKBLK, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		else
			status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, file_lksb, LCK$M_VALBLK | LCK$M_CONVERT,
					NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = file_lksb->cond;
		if (SS$_NORMAL != status)
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), status);
	}
	reg->node = node;	/* In case MUPIP JOURNAL /RECOVER */
	if ((TRUE == clustered) && (FALSE == CCP_SEGMENT_STATE(csa->nl, CCST_MASK_OPEN)))
	{
		ccp_sendmsg(CCTR_WRITEDB, &gds_info->file_id);
		ccp_userwait(reg, CCST_MASK_OPEN, &nsd->ccp_response_interval, csa->nl->ccp_cycle);
	}
	REVERT;
	return;
}
