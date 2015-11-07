/***************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include <nam.h>
#include <psldef.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <xab.h>
#include "gtm_inet.h"
#include <efndef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "probe.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "locks.h"
#include "gtmmsg.h"
#include "send_msg.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "gtmimagename.h"
#include "gtmrecv.h"
#include "repl_sp.h"		/* for F_CLOSE (used by JNL_FD_CLOSE) */

#include "wbox_test_init.h"

GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		pool_init;
GBLREF	boolean_t		is_src_server;
GBLREF	boolean_t		is_updproc;
GBLREF	boolean_t		is_updhelper;
GBLREF	upd_helper_entry_ptr_t	helper_entry;
GBLREF	jnl_process_vector	*prc_vec;
GBLREF	boolean_t		forw_phase_recovery;

error_def(ERR_CLSTCONFLICT);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLOPNERR);

static	const	unsigned short	zero_fid[3];

/* Called from the blocking AST routine entered to release the journal file, as a result of a MUPIP SET command */

void	jnl_oper_user_ast(gd_region *reg)
{
	jnl_private_control	*jpc;
	uint4			status;
	int			close_res;

	if (reg && reg->open)
	{
		jpc = FILE_INFO(reg)->s_addrs.jnl;
		if (SS_NORMAL != jpc->status)
		{
			assert(0 != jpc->jnllsb->lockid);
			status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
			assert(SS$_NORMAL == status);
			jnl_send_oper(jpc, status);

		}
		if ((FALSE == jpc->qio_active) && (NOJNL != jpc->old_channel))
		{
			JNL_FD_CLOSE(jpc->old_channel, close_res);	/* sets csa->jnl->channel to NOJNL */
			jpc->pini_addr = 0;
			jpc->jnllsb->lockid = 0;
		}
	}
}

/* NOTE:  Because the blocking AST routine is established via a call to gtm_enqw,
   it executes in KERNEL mode;  jnl_oper_user_ast, however, must execute in USER mode.
   This is accomplished by using sys$dclast, explicitly specifying USER mode. */

static	void	jnl_oper_krnl_ast(gd_region *reg)
{
	jnl_private_control	*jpc;

	if (!GTM_PROBE(SIZEOF(gd_region), reg, READ)  ||  !reg->open)
		return;
	if (!GTM_PROBE(SIZEOF(gd_segment), reg->dyn.addr, READ))
		return;
	if (dba_bg != reg->dyn.addr->acc_meth  &&  dba_mm != reg->dyn.addr->acc_meth)
		return;
	if (!GTM_PROBE(SIZEOF(file_control), reg->dyn.addr->file_cntl, READ))
		return;
	if (!GTM_PROBE(SIZEOF(vms_gd_info), reg->dyn.addr->file_cntl->file_info, READ))
		return;

	jpc = FILE_INFO(reg)->s_addrs.jnl;	/* since *cs_addrs is a part of vms_gds_info, no additional probe
							for sgmnt_addrs is needed */
	if (!GTM_PROBE(SIZEOF(jnl_private_control), jpc, WRITE))
		return;
	if ((FALSE == jpc->qio_active)  &&  (NOJNL != jpc->channel))
	{
		jpc->old_channel = jpc->channel;
		jpc->channel = NOJNL;
		if (!GTM_PROBE(SIZEOF(vms_lock_sb), jpc->jnllsb, WRITE))
			return;
		jpc->status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
		sys$dclast(jnl_oper_user_ast, reg, PSL$C_USER);		/* if it fails, jnl_ensure_open should cleanup */
	}
}

uint4	jnl_file_open(gd_region *reg, bool init, void	oper_ast())
{
	sgmnt_addrs		*csa;
	sgmnt_data		*csd;
	node_local_ptr_t	cnl;
	struct FAB		fab;	/* same name as other modules help to compare code */
	struct NAM		nam;
	struct XABFHC		xabfhc;
	struct XABITM		xabitm;
	struct xab_caching_options_flags	xabitemcacheopt;
	struct {
		short	len,
			cod;
		void	*address;
		int	*retlen;
	} xabitemlist[2];
	jnl_private_control	*jpc;
	jnl_buffer		*jb;
	jnl_file_header		header;
	uint4			status, sts;
	char			name_buffer[GLO_NAME_MAXLEN], es_buffer[MAX_FN_LEN];
	boolean_t		retry;
	$DESCRIPTOR(desc, name_buffer);

	if ((dba_bg != reg->dyn.addr->acc_meth) && (dba_mm != reg->dyn.addr->acc_meth))
		GTMASSERT;
	if (NULL == oper_ast)
		oper_ast = jnl_oper_krnl_ast;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	assert(NOJNL == jpc->channel);
	sts = 0;
	jpc->status = jpc->status2 = SS_NORMAL;	/* caution : SS_NORMAL not equal to 0 in VMS */
	xabfhc = cc$rms_xabfhc;
	nam = cc$rms_nam;
	fab = cc$rms_fab;
	fab.fab$l_fop = FILE_INFO(reg)->fab->fab$l_fop;
	fab.fab$b_shr = FILE_INFO(reg)->fab->fab$b_shr;
	fab.fab$b_fac = FILE_INFO(reg)->fab->fab$b_fac;
	fab.fab$l_xab = &xabfhc;
	if (csd->jnl_sync_io && (IS_GTM_IMAGE || is_updproc || (is_updhelper && helper_entry &&
								(UPD_HELPER_WRITER == helper_entry->helper_type))))
	{	/* setup xabitm and link in */
		xabitemcacheopt.xab$v_file_attributes = 0;
		xabitemcacheopt.xab$v_file_contents = XAB$K_NOCACHING;
		xabitemcacheopt.xab$v_flush_on_close = XAB$K_FLUSH;
		xabitemcacheopt.xab$v_cachectl_mbz = 0;
		memset(&xabitemlist, 0, SIZEOF(xabitemlist));
		xabitemlist[0].len = SIZEOF(struct xab_caching_options_flags);
		xabitemlist[0].cod = XAB$_CACHING_OPTIONS;
		xabitemlist[0].address = &xabitemcacheopt;
		memset(&xabitm, 0, SIZEOF(xabitm));
		xabitm.xab$b_bln = XAB$C_ITMLEN;
		xabitm.xab$b_cod = XAB$C_ITM;
		xabitm.xab$l_itemlist = xabitemlist;
		xabitm.xab$b_mode = XAB$K_SETMODE;
		xabfhc.xab$l_nxt = &xabitm;
	}
	fab.fab$l_nam = &nam;
	if (init)
	{
		cre_jnl_file_intrpt_rename(((int)csd->jnl_file_len), csd->jnl_file_name);
		nam.nam$l_esa = es_buffer;	/* Though conclealed name is not possible, we use it */
		nam.nam$b_ess = SIZEOF(es_buffer);
		fab.fab$l_fna = csd->jnl_file_name;
		fab.fab$b_fns = csd->jnl_file_len;
		/* although jnl_file_close() would have memset cnl->jnl_file.jnl_file_id to 0 and incremented cycle, it
		 * might have got shot in the middle of executing those instructions. we redo it here just to be safe.
		 */
		memset(&cnl->jnl_file.jnl_file_id, 0, SIZEOF(cnl->jnl_file.jnl_file_id));
		jb->cycle++;	/* increment shared cycle so all future callers of jnl_ensure_open recognize journal switch */
		for (retry = TRUE;  ;)
		{
			status = sys$open(&fab);
			if (0 == (status & 1))
			{
				jpc->status = status;
				jpc->status2 = fab.fab$l_stv;
				sts = ERR_JNLFILOPN;
				if (RMS$_PRV == status)
					break;
			} else
			{
				jpc->channel = fab.fab$l_stv;
				sts = jnl_file_open_common(reg, DISK_BLOCK_SIZE * (xabfhc.xab$l_ebk - 1));
			}
			if ((0 != sts) && retry)
			{
				assert(!is_src_server);	/* source server should only read journal files so must never reach
							 * a situation where it has to switch journal files.
							 */
				sts = jnl_file_open_switch(reg, sts);
				retry = FALSE;	/* Do not switch more than once, even if error occurs */
				if (0 == sts)
					continue;
			}
			break;
		}
	} else
	{
		ASSERT_JNLFILEID_NOT_NULL(csa);
		/* use the file id in node-local shared memory to open the journal file */
		memcpy(&nam.nam$t_dvi, &cnl->jnl_file.jnl_file_id.dvi, SIZEOF(cnl->jnl_file.jnl_file_id.dvi));
		memcpy(&nam.nam$w_did, &cnl->jnl_file.jnl_file_id.did, SIZEOF(cnl->jnl_file.jnl_file_id.did));
		memcpy(&nam.nam$w_fid, &cnl->jnl_file.jnl_file_id.fid, SIZEOF(cnl->jnl_file.jnl_file_id.fid));
		fab.fab$l_fop |= FAB$M_NAM;
		status = sys$open(&fab);
		if (!(status & 1))
		{
			jpc->status = status;
			jpc->status2 = fab.fab$l_stv;
			sts = ERR_JNLFILOPN;
		} else 	if (csd->clustered  &&  (0 == jb->size))
		{
			/* CCP -- not first to open */
			jb->size = csd->jnl_buffer_size * DISK_BLOCK_SIZE;
			jb->filesize = xabfhc.xab$l_ebk; /* Why not virtual file size ??? */
			jb->min_write_size = JNL_MIN_WRITE;
			jb->max_write_size = JNL_MAX_WRITE;
		}
	}
	/* Clear out any previous out-of-sync values that new_dskaddr/new_dsk might have (from previous journal files) */
	jpc->new_dskaddr = jpc->new_dsk = 0;
	/* Also clear out the field that controls whether the above two fields are used in secshr_db_clnup */
	jpc->dsk_update_inprog = FALSE;
	if (0 == sts)
	{
		if (!is_src_server)
		{
			global_name("GT$J", &FILE_INFO(reg)->file_id, name_buffer);
			desc.dsc$w_length = name_buffer[0];
			desc.dsc$a_pointer = &name_buffer[1];
			desc.dsc$b_dtype = DSC$K_DTYPE_T;
			desc.dsc$b_class = DSC$K_CLASS_S;
			memset(jpc->jnllsb, 0, SIZEOF(vms_lock_sb));
			status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, jpc->jnllsb, LCK$M_SYSTEM | LCK$M_EXPEDITE, &desc, 0, NULL,
					0, NULL, PSL$C_USER, 0);
			if (SS_NORMAL == status)
				status = jpc->jnllsb->cond;
			if (SS_NORMAL == status)
			{
				status = gtm_enqw(EFN$C_ENF, LCK$K_CRMODE, jpc->jnllsb, LCK$M_CONVERT | LCK$M_NODLCKBLK,
						NULL, 0, NULL, reg, oper_ast, PSL$C_USER, 0);
				if (SS_NORMAL == status)
					status = jpc->jnllsb->cond;
			}
			if (SS_NORMAL != status)
			{
				sys$dassgn(fab.fab$l_stv);
				jpc->status = status;
				sts = ERR_CLSTCONFLICT;
			}
		}
		if (is_src_server || SS_NORMAL == status)
		{
			if (init)
			{	/* deferred to ensure that the lock works - stash the file id in node-local for subsequent users */
				memcpy(&cnl->jnl_file.jnl_file_id.dvi, &nam.nam$t_dvi, SIZEOF(cnl->jnl_file.jnl_file_id.dvi));
				memcpy(&cnl->jnl_file.jnl_file_id.did, &nam.nam$w_did, SIZEOF(cnl->jnl_file.jnl_file_id.did));
				memcpy(&cnl->jnl_file.jnl_file_id.fid, &nam.nam$w_fid, SIZEOF(cnl->jnl_file.jnl_file_id.fid));
			}
			jpc->channel = fab.fab$l_stv;
			jpc->cycle = jb->cycle;	/* make private cycle and shared cycle in sync */
			jpc->sync_io = csd->jnl_sync_io;
		}
	}
	GTM_WHITE_BOX_TEST(WBTEST_JNL_FILE_OPEN_FAIL, sts, ERR_JNLFILOPN);
	if (0 != sts)
	{
		jpc->channel = NOJNL;
		jnl_send_oper(jpc, sts);
	}
	assert((0 != sts) || (NOJNL != jpc->channel));
	return sts;
}
