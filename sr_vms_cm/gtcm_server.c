/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_limits.h"
#include "gtm_inet.h"

#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "gdscc.h"
#include "gt_timer.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gtcmlkdef.h"
#include "ladef.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "efn.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmimagename.h"
#include "desblk.h"		/* for desblk structure */
#include "gtcmtr_protos.h"
#include "trans_log_name.h"
#include "get_page_size.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdskill.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "init_secshr_addrs.h"
#include "gtm_threadgbl_init.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */

GBLDEF bool			cm_timeout = FALSE;
GBLDEF bool			cm_shutdown = FALSE;
GBLDEF unsigned short		procnum;
GBLDEF short			gtcm_ast_avail;
GBLDEF int			gtcm_users = 0;
GBLDEF int4			gtcm_exi_condition;
GBLDEF connection_struct 	*curr_entry;
GBLDEF relque ALIGN_QUAD 	action_que;
GBLDEF desblk			gtcm_exi_blk;
GBLDEF struct CLB       	*proc_to_clb[USHRT_MAX + 1];    /* for index 0 */
GBLDEF gd_region		*action_que_dummy_reg;

GBLREF short			astq_dyn_avail;
GBLREF bool			licensed;
GBLREF boolean_t		run_time;
GBLREF boolean_t		gtcm_connection;
GBLREF uint4			image_count;
GBLREF int4			lkid, lid;
GBLREF cm_lckblkreg		*blkdlist;
GBLREF gv_namehead		*gv_target;
GBLREF struct NTD		*ntd_root;
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF boolean_t		is_replicator;

LITREF char		cm_prd_name[];
LITREF char		cm_ver_name[];
LITREF int4		cm_prd_len;
LITREF int4		cm_ver_len;
OS_PAGE_SIZE_DECLARE

#define GTCM_AST_OVRHD 4	/* init ast, read/write overlap, mbx, safety */

error_def(CMERR_CMINTQUE);
error_def(CMERR_CMSYSSRV);
error_def(ERR_BADGTMNETMSG);
error_def(ERR_WILLEXPIRE);
error_def(LP_NOCNFDB);
error_def(LP_INVCSM);

gtcm_server()
{
	static readonly int4	reptim[2] = {-100000, -1};	/* 10ms */
       	static readonly int4	wait[2] =  {-1000000, -1};	/* 100ms */
	void		gtcm_ch(), gtcm_exi_handler(), gtcm_init_ast(), gtcm_int_unpack(), gtcm_mbxread_ast(),
			gtcm_neterr(), gtcm_read_ast(), gtcm_remove_from_action_queue(), gtcm_shutdown_ast(), gtcm_write_ast(),
			la_freedb();
	bool		gtcm_link_accept();
	bool		alid;
	char		buff[512];
	char		*h = NULL;
	char		*la_getdb();
	char		nbuff[256];
	char		*pak = NULL;
	char		reply;
	unsigned short	outlen;
	int4		closewait[2] = {0, -1};
	int4		inid = 0, mdl = 0, nid = 0, days = 0;
	int4		lic_status;
	int4		lic_x;
	int4		lm_mdl_nid();
	uint4		status;
	int		i, receive(), value;
	mstr		name1, name2;
	gd_addr		*get_next_gdr();
	struct NTD	*cmu_ntdroot();
	connection_struct *prev_curr_entry;
	struct	dsc$descriptor_s	dprd;
	struct	dsc$descriptor_s	dver;
	$DESCRIPTOR(node_name, nbuff);
	$DESCRIPTOR(proc_name, "GTCM_SERVER");
	$DESCRIPTOR(timout, buff);
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
        assert(0 == EMPTY_QUEUE);       /* check so dont need gdsfhead everywhere */
	gtm_imagetype_init(GTCM_GNP_SERVER_IMAGE); /* Side-effect: Sets skip_dbtriggers to TRUE for non-trigger platforms */
	gtm_env_init();	/* read in all environment variables */
	get_page_size();
	name1.addr = "GTCMSVRNAM";
	name1.len = SIZEOF("GTCMSVRNAM") - 1;
	status = trans_log_name(&name1, &name2, nbuff);
	if (SS$_NORMAL == status)
	{
		proc_name.dsc$a_pointer = nbuff;
		proc_name.dsc$w_length = node_name.dsc$w_length = name2.len;
	} else if (SS$_NOLOGNAM == status)
	{
		MEMCPY_LIT(nbuff, "GTCMSVR");
		node_name.dsc$w_length = SIZEOF("GTCMSVR") - 1;
	} else
		rts_error(VARLSTCNT(1) status);
	sys$setprn(&proc_name);
	status = lib$get_foreign(&timout, 0, &outlen, 0);
	if ((status & 1) && (6 > outlen))
	{
		for (i = 0;  i < outlen;  i++)
		{
			value = value * 10;
			if (buff[i] <= '9' && buff[i] >= '0')
				value += buff[i] - 48;
			else
				break;
		}
		if (outlen && (i == outlen))
		{
			cm_timeout = TRUE;
			closewait[0] = value * -10000000;
		}
	}
	dprd.dsc$w_length = cm_prd_len;
	dprd.dsc$b_dtype  = DSC$K_DTYPE_T;
	dprd.dsc$b_class  = DSC$K_CLASS_S;
	dprd.dsc$a_pointer= cm_prd_name;
	dver.dsc$w_length = cm_ver_len;
	dver.dsc$b_dtype  = DSC$K_DTYPE_T;
	dver.dsc$b_class  = DSC$K_CLASS_S;
	dver.dsc$a_pointer= cm_ver_name;
	getjobnum();
	ast_init();
	licensed = TRUE;
	lkid = 2;
#	ifdef NOLICENSE
	lid = 1;
#	else
	/* this code used to be scattered to discourage reverse engineering, but since it now disabled, that seems pointless */
	lic_status = ((NULL == (h = la_getdb(LMDB))) ? LP_NOCNFDB : SS$_NORMAL);
	lic_status = ((1 == (lic_status & 1)) ? lm_mdl_nid(&mdl, &nid, &inid) : lic_status);
	lic_status = ((1 == (lic_status & 1)) ? lp_licensed(h, &dprd, &dver, mdl, nid, &lid, &lic_x, &days, pak) : lic_status);
	if (LP_NOCNFDB != lic_status)
		la_freedb(h);
	if (1 == (lic_status & 1))
	{
		licensed = TRUE;
		if (days < 14)
			rts_error(VARLSTCNT(1) ERR_WILLEXPIRE);
	} else
	{
		licensed = FALSE;
		sys$exit(lic_status);
	}
#	endif
	gtcm_ast_avail = astq_dyn_avail - GTCM_AST_OVRHD;
	stp_init(STP_INITSIZE);
	rts_stringpool = stringpool;
	cache_init();
	procnum = 0;
	get_proc_info(0, TADR(login_time), &image_count);
        memset(proc_to_clb, 0, SIZEOF(proc_to_clb));
	status = cmi_init(&node_name, 0, 0, gtcm_init_ast, gtcm_link_accept);
	if (!(status & 1))
	{
		rts_error(VARLSTCNT(1) ((status ^ 3) | 4));
		sys$exit(status);
	}
	ntd_root = cmu_ntdroot();
	ntd_root->mbx_ast =  gtcm_mbxread_ast;
	ntd_root->err = gtcm_neterr;
	gtcm_connection = FALSE;
	lib$establish(gtcm_ch);
	gtcm_exi_blk.exit_hand = &gtcm_exi_handler;
	gtcm_exi_blk.arg_cnt = 1;
	gtcm_exi_blk.cond_val = &gtcm_exi_condition;
	sys$dclexh(&gtcm_exi_blk);
	INVOKE_INIT_SECSHR_ADDRS;
	initialize_pattern_table();
	assert(run_time); /* Should have been set by gtm_imagetype_init */
	while (!cm_shutdown)
	{
		if (blkdlist)
			gtcml_chkreg();

		assert(!lib$ast_in_prog());
		status = sys$dclast(&gtcm_remove_from_action_queue, 0, 0);
		if (SS$_NORMAL != status)
			rts_error(VARLSTCNT(4) CMERR_CMSYSSRV, 0, status, 0);
		if (INTERLOCK_FAIL == curr_entry)
			rts_error(VARLSTCNT(1) CMERR_CMINTQUE);
		if (EMPTY_QUEUE != curr_entry)
		{
			switch (*curr_entry->clb_ptr->mbf)
			{
				case CMMS_L_LKCANALL:
					reply = gtcmtr_lkcanall();
					break;
				case CMMS_L_LKCANCEL:
					reply = gtcmtr_lkcancel();
					break;
				case CMMS_L_LKREQIMMED:
					reply = gtcmtr_lkreqimmed();
					break;
				case CMMS_L_LKREQNODE:
					reply = gtcmtr_lkreqnode();
					break;
				case CMMS_L_LKREQUEST:
					reply = gtcmtr_lkrequest();
					break;
				case CMMS_L_LKRESUME:
					reply = gtcmtr_lkresume();
					break;
				case CMMS_L_LKACQUIRE:
					reply = gtcmtr_lkacquire();
					break;
				case CMMS_L_LKSUSPEND:
					reply = gtcmtr_lksuspend();
					break;
				case CMMS_L_LKDELETE:
					reply = gtcmtr_lkdelete();
					break;
				case CMMS_Q_DATA:
					reply = gtcmtr_data();
					break;
				case CMMS_Q_GET:
					reply = gtcmtr_get();
					break;
				case CMMS_Q_KILL:
					reply = gtcmtr_kill();
					break;
				case CMMS_Q_ORDER:
					reply = gtcmtr_order();
					break;
				case CMMS_Q_PREV:
					reply = gtcmtr_zprevious();
					break;
				case CMMS_Q_PUT:
					reply = gtcmtr_put();
					break;
				case CMMS_Q_QUERY:
					reply = gtcmtr_query();
					break;
				case CMMS_Q_ZWITHDRAW:
					reply = gtcmtr_zwithdraw();
					break;
				case CMMS_S_INITPROC:
					reply = gtcmtr_initproc();
					break;
				case CMMS_S_INITREG:
					reply = gtcmtr_initreg();
					break;
				case CMMS_S_TERMINATE:
					reply = gtcmtr_terminate(TRUE);
					break;
				case CMMS_E_TERMINATE:
					reply = gtcmtr_terminate(FALSE);
					break;
				case CMMS_U_LKEDELETE:
					reply = gtcmtr_lke_clearrep(curr_entry->clb_ptr, curr_entry->clb_ptr->mbf);
					break;
				case CMMS_U_LKESHOW:
					reply = gtcmtr_lke_showrep(curr_entry->clb_ptr, curr_entry->clb_ptr->mbf);
					break;
				case CMMS_B_BUFRESIZE:
					reply = CM_WRITE;
					value = *(unsigned short *)(curr_entry->clb_ptr->mbf + 1);
					if (value > curr_entry->clb_ptr->mbl)
					{
						free(curr_entry->clb_ptr->mbf);
						curr_entry->clb_ptr->mbf = malloc(value);
					}
					*curr_entry->clb_ptr->mbf = CMMS_C_BUFRESIZE;
					curr_entry->clb_ptr->mbl = value;
					curr_entry->clb_ptr->cbl = 1;
					break;
				case CMMS_B_BUFFLUSH:
					reply = gtcmtr_bufflush();
					break;
				case CMMS_Q_INCREMENT:
					reply = gtcmtr_increment();
					break;
				default:
					reply = FALSE;
					if (SS$_NORMAL == status)
                                                rts_error(VARLSTCNT(3) ERR_BADGTMNETMSG, 1, (int)*curr_entry->clb_ptr->mbf);
					break;
			}
			if (curr_entry)		/* curr_entry can be NULL if went through gtcmtr_terminate */
			{
				status = sys$gettim(&curr_entry->lastact[0]);
				if (SS$_NORMAL != status)
					rts_error(VARLSTCNT(1) status);
				/* curr_entry is used by gtcm_mbxread_ast to determine if it needs to defer the interrupt message */
				prev_curr_entry = curr_entry;
				if (CM_WRITE == reply)
				{	/* if ast == gtcm_write_ast, let it worry */
					curr_entry->clb_ptr->ast = gtcm_write_ast;
					curr_entry = EMPTY_QUEUE;
					cmi_write(prev_curr_entry->clb_ptr);
				} else
				{
					curr_entry = EMPTY_QUEUE;
					if (1 == (prev_curr_entry->int_cancel.laflag & 1))
					{  /* valid interrupt cancel msg, handle in gtcm_mbxread_ast */
						status = sys$dclast(gtcm_int_unpack, prev_curr_entry, 0);
						if (SS$_NORMAL != status)
							rts_error(VARLSTCNT(1) status);
					} else  if (CM_READ == reply)
					{
						prev_curr_entry->clb_ptr->ast = gtcm_read_ast;
						cmi_read(prev_curr_entry->clb_ptr);
					}
				}
			}
		} else  if (1 < astq_dyn_avail)
		{
#			ifdef GTCM_REPTIM
			/* if reptim is not needed - and smw doesn't know why it would be - remove this	*/
			status = sys$schdwk(0, 0, &wait[0], &reptim[0]);
#			else
			status = sys$schdwk(0, 0, &wait[0], 0);
#			endif
			sys$hiber();
			sys$canwak(0, 0);
		}
		if (cm_timeout && (0 == gtcm_users))
                        sys$setimr(efn_ignore, closewait, gtcm_shutdown_ast, &cm_shutdown, 0);
	}
}
