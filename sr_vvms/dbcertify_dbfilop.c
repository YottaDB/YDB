/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, LLC.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include <descrip.h>
#include <iodef.h>
#include <psldef.h>
#include <rms.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <syidef.h>
#include <efndef.h>


#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblk.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "efn.h"
#include "vmsdtype.h"
#include "sleep_cnt.h"
#include "error.h"
#include "wcs_sleep.h"
#include "gtm_file_stat.h"
#include "iosb_disk.h"
#include "iosp.h"
#include "is_file_identical.h"
#include "dbcertify.h"

#define DEFDBEXT		".dat"
#define MAX_NODE_NAME_LEN	16

static	int msgcodes[2] = {1 , 0};			/* allows for 1 parm for sys$putmsg() */

void dbcertify_dbfilop(phase_static_area *psa)
{
	uint4		addrs[2], lcnt, node_area, node_number, status, hold_esa;
	unsigned short	retlen[4];
	io_status_block_disk	iosb;
	char		node_name[MAX_NODE_NAME_LEN], dnetid[MAX_NODE_NAME_LEN];
	struct
	{
		item_list_3	ilist[3];
		int4		terminator;
	} syi_list;
	vms_gds_info	*gds_info;
	$DESCRIPTOR	(faodsc, dnetid);
	static readonly $DESCRIPTOR	(ctrstr, "!UL.!UL");

	error_def(ERR_DBFILOPERR);
	error_def(ERR_MUSTANDALONE);
	error_def(ERR_NETDBOPNERR);
	error_def(ERR_DBOPNERR);
	error_def(ERR_SYSCALL);

	gds_info = psa->fc->file_info;
	switch(psa->fc->op)
	{
		case	FC_READ:
			assert(psa->fc->op_pos > 0);		/* gt.m uses the vms convention of numbering the blocks from 1 */
			status = sys$qiow(EFN$C_ENF, gds_info->fab->fab$l_stv, IO$_READVBLK, &iosb, NULL, 0,
					  psa->fc->op_buff, psa->fc->op_len, psa->fc->op_pos, 0, 0, 0);
			if (SYSCALL_SUCCESS(status))
				status = iosb.cond;
			if (SYSCALL_ERROR(status))
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, gds_info->fab->fab$b_fns, gds_info->fab->fab$l_fna,
					status);
			break;
		case	FC_WRITE:
			status = sys$qiow(EFN$C_ENF, gds_info->fab->fab$l_stv, IO$_WRITEVBLK,
					  &iosb, NULL, 0, psa->fc->op_buff, psa->fc->op_len, psa->fc->op_pos, 0, 0, 0);
			if (SYSCALL_SUCCESS(status))
				status = iosb.cond;
			if (SYSCALL_ERROR(status))
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, gds_info->fab->fab$b_fns,
					gds_info->fab->fab$l_fna, status);
			break;
		case	FC_OPEN:
			if (NULL == gds_info->fab)
				gds_info->fab = malloc(SIZEOF(*gds_info->fab));

			*gds_info->fab = cc$rms_fab;
			gds_info->fab->fab$l_fna = psa->dbc_gv_cur_region->dyn.addr->fname;
			gds_info->fab->fab$b_fns = psa->dbc_gv_cur_region->dyn.addr->fname_len;
			gds_info->fab->fab$b_fac = FAB$M_BIO | FAB$M_GET | FAB$M_PUT;
			gds_info->fab->fab$l_fop = FAB$M_UFO;
			if (psa->phase_one)
			{	/* We need shared access for phase-1 but not phase-2 which must run standalone */
				gds_info->fab->fab$b_rtv = WINDOW_ALL;
				gds_info->fab->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
			} else
				gds_info->fab->fab$b_shr = FAB$M_NIL;
			gds_info->fab->fab$l_dna = DEFDBEXT;
			gds_info->fab->fab$b_dns = SIZEOF(DEFDBEXT) - 1;
			if (NULL == gds_info->nam)
			{
				gds_info->nam = malloc(SIZEOF(*gds_info->nam));
				*gds_info->nam = cc$rms_nam;
				gds_info->nam->nam$l_esa = malloc(MAX_FN_LEN + 1);
			}
			gds_info->nam->nam$b_ess = MAX_FN_LEN;
			gds_info->fab->fab$l_nam = gds_info->nam;
			if (NULL == gds_info->xabfhc)
				gds_info->xabfhc = malloc(SIZEOF(*gds_info->xabfhc));
			*gds_info->xabfhc = cc$rms_xabfhc;
			gds_info->fab->fab$l_xab = gds_info->xabfhc;
			if (NULL == gds_info->xabpro)
				gds_info->xabpro = malloc(SIZEOF(*gds_info->xabpro));
			*gds_info->xabpro = cc$rms_xabpro;
			gds_info->xabfhc->xab$l_nxt = gds_info->xabpro;

			psa->dbc_gv_cur_region->read_only = FALSE;	/* maintain csa->read_write simultaneously */
			gds_info->s_addrs.read_write = TRUE;		/* maintain reg->read_only simultaneously */
			if (0 == gds_info->fab->fab$b_fns)
			{
				memcpy(gds_info->nam->nam$t_dvi, gds_info->file_id.dvi, SIZEOF(gds_info->nam->nam$t_dvi));
				memcpy(gds_info->nam->nam$w_did, gds_info->file_id.did, SIZEOF(gds_info->nam->nam$w_did));
				memcpy(gds_info->nam->nam$w_fid, gds_info->file_id.fid, SIZEOF(gds_info->nam->nam$w_fid));
				gds_info->fab->fab$l_fop |= FAB$M_NAM;
			} else
			{
				gds_info->nam->nam$b_nop = NAM$M_NOCONCEAL;
				if (0 == (1 & (status = sys$parse(gds_info->fab, NULL, NULL))))
					rts_error(VARLSTCNT(9) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("sys$parse"),
						  CALLFROM, status, gds_info->fab->fab$l_stv);
				if (gds_info->nam->nam$b_node)
				{
					syi_list.ilist[0].item_code = SYI$_NODENAME;
					syi_list.ilist[0].buffer_address = &node_name;
					syi_list.ilist[0].buffer_length = SIZEOF(node_name);
					syi_list.ilist[0].return_length_address = &retlen[0];
					syi_list.ilist[1].item_code = SYI$_NODE_AREA;
					syi_list.ilist[1].buffer_address = &node_area;
					syi_list.ilist[1].buffer_length = SIZEOF(node_area);
					syi_list.ilist[1].return_length_address = &retlen[1];
					syi_list.ilist[2].item_code = SYI$_NODE_NUMBER;
					syi_list.ilist[2].buffer_address = &node_number;
					syi_list.ilist[2].buffer_length = SIZEOF(node_number);
					syi_list.ilist[2].return_length_address = &retlen[2];
					syi_list.terminator = 0;
					status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &syi_list, &iosb, NULL, 0);
					if (SYSCALL_SUCCESS(status))
						status = sys$fao(&ctrstr, &retlen[3], &faodsc, node_area, node_number);
					if (SYSCALL_ERROR(status))
						rts_error(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("sys$getsyiw()"),
							  CALLFROM, status);
					if (((gds_info->nam->nam$b_node - 2 != retlen[0])
					     || (0 != memcmp(gds_info->nam->nam$l_esa, node_name, retlen[0])))
					    && ((gds_info->nam->nam$b_node - 2 != retlen[3])
						|| (0 != memcmp(gds_info->nam->nam$l_esa, dnetid, retlen[3]))))
					{
						rts_error(VARLSTCNT(1) ERR_NETDBOPNERR);
					}
					gds_info->fab->fab$l_fna = gds_info->nam->nam$l_esa + gds_info->nam->nam$b_node;
					gds_info->fab->fab$b_fns = gds_info->nam->nam$b_esl - gds_info->nam->nam$b_node;
				}
			}
			for (lcnt = 1;  15 >= lcnt;  lcnt++)
			{	/* Try for 15 seconds */
				if (RMS$_FLK != (status = sys$open(gds_info->fab, NULL, NULL)))
					break;
				sleep(1);
			}
			if (SYSCALL_ERROR(status))
			{
				if (RMS$_PRV == status)
				{
					gds_info->fab->fab$b_fac = FAB$M_BIO | FAB$M_GET;
					psa->dbc_gv_cur_region->read_only = TRUE; /* maintain csa->read_write simultaneously */
					gds_info->s_addrs.read_write = FALSE;	  /* maintain reg->read_only simultaneously */
					gds_info->fab->fab$l_fna = gds_info->nam->nam$l_esa;
					gds_info->fab->fab$b_fns = gds_info->nam->nam$b_esl;
					status = sys$open(gds_info->fab);
				}
				if (RMS$_NORMAL != status)
				{
					if (RMS$_FLK == status)
						rts_error(VARLSTCNT(4) MAKE_MSG_TYPE(ERR_MUSTANDALONE, ERROR),
							  2, gds_info->fab->fab$b_fns, gds_info->fab->fab$l_fna);
					else
						rts_error(VARLSTCNT(6) ERR_DBOPNERR, 2, gds_info->fab->fab$b_fns,
							  gds_info->fab->fab$l_fna, status, gds_info->fab->fab$l_stv);
				}
			}
			memcpy(gds_info->file_id.dvi, gds_info->nam->nam$t_dvi, SIZEOF(gds_info->nam->nam$t_dvi));
			memcpy(gds_info->file_id.did, gds_info->nam->nam$w_did, SIZEOF(gds_info->nam->nam$w_did));
			memcpy(gds_info->file_id.fid, gds_info->nam->nam$w_fid, SIZEOF(gds_info->nam->nam$w_fid));
			/* Copy after removing the version number from file name */
			fncpy_nover(gds_info->nam->nam$l_esa, gds_info->nam->nam$b_esl,
				    psa->dbc_gv_cur_region->dyn.addr->fname, psa->dbc_gv_cur_region->dyn.addr->fname_len);
			gds_info->nam->nam$b_esl = gds_info->fab->fab$b_fns = psa->dbc_gv_cur_region->dyn.addr->fname_len;
			strcpy(gds_info->fab->fab$l_fna, psa->dbc_gv_cur_region->dyn.addr->fname);
			gds_info->fab->fab$l_xab = NULL;
			gds_info->fab->fab$l_nam = NULL;
			gds_info->xabfhc->xab$l_nxt = NULL;
			break;
		case	FC_CLOSE:
			status = sys$dassgn(gds_info->fab->fab$l_stv);
			if (!(status & 1))
				gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("sys$dassgn"), CALLFROM, status);
			break;
		default:
			GTMASSERT;
	}
}
