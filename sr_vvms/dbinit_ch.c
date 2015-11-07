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
#include <fab.h>
#include <iodef.h>
#include <lckdef.h>
#include <lkidef.h>
#include <psldef.h>
#include <prvdef.h>
#include <secdef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "vmsdtype.h"
#include "error.h"
#include "locks.h"
#include "del_sec.h"
#include "mem_list.h"
#include "repl_sp.h"		/* for F_CLOSE (used by JNL_FD_CLOSE) */
#include "iosp.h"		/* for SS_NORMAL used by JNL_FD_CLOSE */
#include "shmpool.h"		/* needed for shmpool structures */
#include "have_crit.h"

error_def(ERR_REQRUNDOWN);

GBLREF	gd_region	*db_init_region;

CONDITION_HANDLER(dbinit_ch)
{
	char		name_buff[GLO_NAME_MAXLEN];
	short		iosb[4];
	uint4		last_one_status, outaddrs[2], status, lk_status;
	int4		lock_addrs_end;
	vms_gds_info	*gds_info;
	sgmnt_addrs	*csa;
	boolean_t	read_write, is_bg;
	vms_lock_sb	*file_lksb;
	struct
        {
                int4 length;
                char value[3];
        } state;
	item_list_3	ilist[2];
	uint4 		prvprv1[2], prvprv2[2], prvadr[2];
       	$DESCRIPTOR(desc, name_buff);

	START_CH;
	if (SUCCESS == SEVERITY || INFO == SEVERITY)
	{
		PRN_ERROR;
		CONTINUE;
	}
	last_one_status = 0;
	lock_addrs_end = 0;
	gds_info = NULL;
	if (NULL != db_init_region->dyn.addr->file_cntl)
		gds_info = FILE_INFO(db_init_region);
	if (NULL != gds_info)
	{
		gds_info = FILE_INFO(db_init_region);
		file_lksb = &gds_info->file_cntl_lsb;
		csa = &gds_info->s_addrs;
		read_write = (FALSE == db_init_region->read_only);
		is_bg = (dba_bg == db_init_region->dyn.addr->acc_meth);

		if ((NULL != csa->hdr) && (0 != csa->hdr->label[0]) && (JNL_ENABLED(csa->hdr)) && (NULL != csa->jnl))
		{
			if (NOJNL != csa->jnl->channel)
				JNL_FD_CLOSE(csa->jnl->channel, status);	/* sets csa->jnl->channel to NOJNL */
			if ((NULL != csa->jnl->jnllsb) && (0 != csa->jnl->jnllsb->lockid))
			{
				status = gtm_deq(csa->jnl->jnllsb->lockid, NULL, PSL$C_USER, 0);
				assert(SS$_NORMAL == status);
				csa->jnl->jnllsb->lockid = 0;
			}
		}

		if (0 != file_lksb->lockid)
		{
			/* Emulate rundown code - see if we are the last one before deleting the section */
			/* examine which lock state we are in at first */
			prvadr[1] = 0;
			prvadr[0] = PRV$M_SYSLCK;
			status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv1[0]);
			if (SS$_NORMAL == status)
			{
				prvadr[0] = PRV$M_WORLD;
				status = sys$setprv(TRUE, &prvadr[0], FALSE, &prvprv2[0]);
			}
			if (SS$_NORMAL == status)
			{
				memset(&ilist, 0, SIZEOF(ilist));
				ilist[0].item_code = LKI$_STATE;
				ilist[0].buffer_length = SIZEOF(state.value);
				ilist[0].buffer_address = &(state.value[0]);
				ilist[0].return_length_address = &(state.length);
				status = sys$getlkiw(EFN$C_ENF, &(file_lksb->lockid), &ilist, 0, 0, 0, 0);
			}
			if (SS$_NORMAL != status)
			{
				state.value[1] = LCK$K_NLMODE;
				status = SS$_NORMAL;
			}
			if (0 == (prvprv2[0] & PRV$M_WORLD))
				sys$setprv(FALSE, &prvadr[0], FALSE, 0);
			prvadr[0] = PRV$M_SYSLCK;
			if (0 == (prvprv1[0] & PRV$M_SYSLCK))
				sys$setprv(FALSE, &prvadr[0], FALSE, 0);
			switch (state.value[1])
			{
				case LCK$K_NLMODE:
				case LCK$K_CRMODE:
				case LCK$K_CWMODE:
				case LCK$K_PRMODE:
					status = gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, file_lksb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
						NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
					if (SS$_NORMAL == status)
						status = file_lksb->cond;
				case LCK$K_PWMODE:
					if (SS$_NORMAL == status)
					{
						last_one_status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, file_lksb,
							LCK$M_CONVERT | LCK$M_NOQUEUE | LCK$M_NODLCKWT,
							NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
						if (SS$_NORMAL == last_one_status)
							status =
							last_one_status = file_lksb->cond;
					}
			}
			if (csa->nl)
			{
				if ((1 == csa->nl->ref_cnt) && (ERR_REQRUNDOWN != SIGNAL) && (csa->read_write)
					&& (NULL != csa->hdr) && (FALSE == csa->hdr->clustered))
				{
					/* last writer and applicable to clean up the stamp in database */
					csa->hdr->owner_node = 0;
					memset(csa->hdr->now_running, 0, SIZEOF(csa->hdr->now_running));
					if (0 != csa->hdr->label[0])
						sys$qiow(EFN$C_ENF, gds_info->fab->fab$l_stv, IO$_WRITEVBLK, iosb,
							NULL, 0, csa->hdr, SIZEOF(sgmnt_data), 1, 0, 0, 0);
				}
				if (!is_bg)
				{
					assert(csa->db_addrs[0]);
					if (!csa->hdr)
						csa->hdr = csa->db_addrs[0];
					lock_addrs_end = (sm_uc_ptr_t)(csa->nl) + ROUND_UP(LOCK_SPACE_SIZE(csa->hdr)
								+ NODE_LOCAL_SPACE(csa->hdr) + JNL_SHARE_SIZE(csa->hdr)
								+ SHMPOOL_BUFFER_SIZE, OS_PAGE_SIZE) - 1;
				}
				if (SS$_NORMAL == last_one_status)
				{	/* Do not remove shared memory if we did not create it. */
					if (TREF(new_dbinit_ipc))
					{
						global_name("GT$S", &gds_info->file_id, name_buff);
						desc.dsc$a_pointer = &name_buff[1];
						desc.dsc$w_length = name_buff[0];
						desc.dsc$b_dtype = DSC$K_DTYPE_T;
						desc.dsc$b_class = DSC$K_CLASS_S;
						del_sec(SEC$M_SYSGBL, &desc, NULL);
						if (!is_bg)
						{
							name_buff[4] = 'L';
							del_sec(SEC$M_SYSGBL, &desc, NULL);
						}
					}
					gds_info->file_cntl_lsb.valblk[0] = 0;
					gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, &gds_info->file_cntl_lsb, LCK$M_CONVERT | LCK$M_VALBLK,
						NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
				} else if (csa->ref_cnt)
				{	/* we incremented csa->nl->ref_cnt; so decrement it now.
					 * decrement private ref_cnt before shared ref_cnt decrement. currently journaling
					 * logic in gds_rundown() in VMS relies on this order to detect last writer
					 */
					csa->ref_cnt--;
					assert(!csa->ref_cnt);
					adawi(-1, &csa->nl->ref_cnt);
				}
			}
		}
		if (NULL != csa->db_addrs[0])
		{	/* Unmap the section used by the database */
			outaddrs[0] = csa->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
			outaddrs[1] = csa->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
			gtm_deltva(outaddrs, NULL, PSL$C_USER);
			if ((!is_bg) && (csa->nl))
			{
				csa->lock_addrs[0] = (sm_uc_ptr_t)(csa->nl);
				csa->lock_addrs[1] = lock_addrs_end;
				assert(csa->lock_addrs[1] > csa->lock_addrs[0]);
				gtm_deltva(csa->lock_addrs, NULL, PSL$C_USER);
			}
		}
		if (0 != gds_info->file_cntl_lsb.lockid)
		{
			if (0 != gds_info->cx_cntl_lsb.lockid)
			{	/* if they have been granted, release the sub-locks too */
				status = gtm_deq(gds_info->cx_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
				assert(SS$_NORMAL == status);
				gds_info->cx_cntl_lsb.lockid = 0;
			}
			status = gtm_deq(gds_info->file_cntl_lsb.lockid, NULL, PSL$C_USER, 0);
			assert(SS$_NORMAL == status);
			gds_info->file_cntl_lsb.lockid = 0;
		}
		csa->hdr = NULL;
		csa->nl = NULL;
		if (NULL != csa->jnl)
		{
			free(csa->jnl);
			csa->jnl = NULL;
		}
		sys$dassgn(gds_info->fab->fab$l_stv);
	}
	/* Reset intrpt_ok_state to OK_TO_INTERRUPT in case we got called (due to an rts_error) with intrpt_ok_state
	 * being set to INTRPT_IN_GVCST_INIT.
	 * We should actually be calling RESTORE_INTRPT_OK_STATE macro but since we don't have access to local variable
	 * save_intrpt_ok_state, set intrpt_ok_state directly.
	 */
	assert((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) || (INTRPT_IN_GVCST_INIT == intrpt_ok_state));
	intrpt_ok_state = INTRPT_OK_TO_INTERRUPT;
	NEXTCH;
}
