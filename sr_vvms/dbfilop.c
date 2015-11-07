/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "efn.h"
#include "vmsdtype.h"
#include "sleep_cnt.h"
#include "dbfilop.h"
#include "wcs_sleep.h"
#include "gtm_file_stat.h"
#include "gtm_malloc.h"		/* for CHECK_CHANNEL_STATUS macro */
#include "iosb_disk.h"
#include "iosp.h"

#define DEFDBEXT		".dat"
#define MAX_NODE_NAME_LEN	16

GBLREF	gd_region	*gv_cur_region;

uint4 dbfilop(file_control *fc)
{
	uint4			addrs[2], lcnt, node_area, node_number, status;
	unsigned short		retlen[4];
	io_status_block_disk	iosb;
	char			node_name[MAX_NODE_NAME_LEN], dnetid[MAX_NODE_NAME_LEN];
	struct {
		item_list_3 ilist[3];
		int4 terminator;
	}			syi_list;
	vms_gds_info		*gds_info;
	uint4			channel_id;

	$DESCRIPTOR	(faodsc, dnetid);
	static readonly $DESCRIPTOR	(ctrstr, "!UL.!UL");

	error_def(ERR_DBFILOPERR);
	error_def(ERR_DBNOTGDS);
	error_def(ERR_NETDBOPNERR);
	error_def(ERR_SYSCALL);

	assert((dba_mm == fc->file_type) || (dba_bg == fc->file_type));
	gds_info = fc->file_info;
	switch(fc->op)
	{
		case	FC_READ:
			assert(fc->op_pos > 0);		/* gt.m uses the vms convention of numbering the blocks from 1 */
			channel_id = gds_info->fab->fab$l_stv;
			status = sys$qiow(EFN$C_ENF, channel_id, IO$_READVBLK, &iosb, NULL, 0,
						fc->op_buff, fc->op_len, fc->op_pos, 0, 0, 0);
			if (SYSCALL_SUCCESS(status))
				status = iosb.cond;
			if (SYSCALL_ERROR(status))
			{
				CHECK_CHANNEL_STATUS(status, channel_id);
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, gds_info->fab->fab$b_fns, gds_info->fab->fab$l_fna,
					status);
			}
			if ((1 == fc->op_pos) && (0 != memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 3)))
				rts_error(VARLSTCNT(4) ERR_DBNOTGDS, 2, gds_info->fab->fab$b_fns, gds_info->fab->fab$l_fna);
			break;
		case	FC_WRITE:
			if ((1 == fc->op_pos) && (0 != memcmp(fc->op_buff, GDS_LABEL, GDS_LABEL_SZ - 1)
					|| (0 == ((sgmnt_data_ptr_t)fc->op_buff)->acc_meth)))
				GTMASSERT;
			assert((1 != fc->op_pos) || fc->op_len <= SIZEOF_FILE_HDR(fc->op_buff));
			switch(fc->file_type)
			{
				case dba_bg:
					channel_id = gds_info->fab->fab$l_stv;
					status = sys$qiow(EFN$C_ENF, channel_id, IO$_WRITEVBLK,
							&iosb, NULL, 0, fc->op_buff, fc->op_len, fc->op_pos, 0, 0, 0);
					if (SYSCALL_SUCCESS(status))
						status = iosb.cond;
					else
						CHECK_CHANNEL_STATUS(status, channel_id);
					break;
				case dba_mm:
					addrs[0] = fc->op_buff;
					addrs[1] = addrs[0] + fc->op_len - 1;
					status = sys$updsec(addrs, NULL, PSL$C_USER, 0, efn_immed_wait, &iosb, NULL, 0);
					if (SYSCALL_SUCCESS(status))
					{
						status = sys$synch(efn_immed_wait, &iosb);
						if (SS$_NORMAL == status)
							status = iosb.cond;
					} else  if (SS$_NOTMODIFIED == status)
						status = SS$_NORMAL;
					break;
				default:
					GTMASSERT;
			}
			if (SYSCALL_ERROR(status))
				rts_error(VARLSTCNT(5) ERR_DBFILOPERR, 2, gds_info->fab->fab$b_fns,
					gds_info->fab->fab$l_fna, status);
			break;
		case	FC_OPEN:
			if (NULL == gds_info->fab)
				gds_info->fab = malloc(SIZEOF(*gds_info->fab));

			*gds_info->fab = cc$rms_fab;
			gds_info->fab->fab$l_fna = gv_cur_region->dyn.addr->fname;
			gds_info->fab->fab$b_fns = gv_cur_region->dyn.addr->fname_len;
			gds_info->fab->fab$b_rtv = WINDOW_ALL;
			gds_info->fab->fab$b_fac = FAB$M_BIO | FAB$M_GET | FAB$M_PUT;
			gds_info->fab->fab$l_fop = FAB$M_UFO;
			gds_info->fab->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
			gds_info->fab->fab$l_dna = DEFDBEXT;
			gds_info->fab->fab$b_dns = SIZEOF(DEFDBEXT) - 1;
			if (NULL == gds_info->nam)
			{
				gds_info->nam = malloc(SIZEOF(*gds_info->nam));
				*gds_info->nam = cc$rms_nam;
				gds_info->nam->nam$l_esa = malloc(MAX_FN_LEN);
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

			gv_cur_region->read_only = FALSE;	/* maintain csa->read_write simultaneously */
			gds_info->s_addrs.read_write = TRUE;	/* maintain reg->read_only simultaneously */
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
					return status;
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
						return status;
					if (((gds_info->nam->nam$b_node - 2 != retlen[0])
							|| (0 != memcmp(gds_info->nam->nam$l_esa, node_name, retlen[0])))
						&& ((gds_info->nam->nam$b_node - 2 != retlen[3])
							|| (0 != memcmp(gds_info->nam->nam$l_esa, dnetid, retlen[3]))))
						return ERR_NETDBOPNERR;

					gds_info->fab->fab$l_fna = gds_info->nam->nam$l_esa + gds_info->nam->nam$b_node;
					gds_info->fab->fab$b_fns = gds_info->nam->nam$b_esl - gds_info->nam->nam$b_node;
				}
			}
			for (lcnt = 1;  MAX_OPEN_RETRY >= lcnt;  lcnt++)
			{
				if (RMS$_FLK != (status = sys$open(gds_info->fab, NULL, NULL)))
					break;
				wcs_sleep(lcnt);
			}
			if (SYSCALL_ERROR(status))
			{
				if (RMS$_PRV == status)
				{
					gds_info->fab->fab$b_fac = FAB$M_BIO | FAB$M_GET;
					gv_cur_region->read_only = TRUE;	/* maintain csa->read_write simultaneously */
					gds_info->s_addrs.read_write = FALSE;	/* maintain reg->read_only simultaneously */
					gds_info->fab->fab$l_fna = gds_info->nam->nam$l_esa;
					gds_info->fab->fab$b_fns = gds_info->nam->nam$b_esl;
					status = sys$open(gds_info->fab);
				}
				if (RMS$_NORMAL != status)
				{
					if (RMS$_ACC == status)
						status = gds_info->fab->fab$l_stv;
					return status;
				}
			}
			memcpy(gds_info->file_id.dvi, gds_info->nam->nam$t_dvi, SIZEOF(gds_info->nam->nam$t_dvi));
			memcpy(gds_info->file_id.did, gds_info->nam->nam$w_did, SIZEOF(gds_info->nam->nam$w_did));
			memcpy(gds_info->file_id.fid, gds_info->nam->nam$w_fid, SIZEOF(gds_info->nam->nam$w_fid));
			/* Copy after removing the version number from file name */
			fncpy_nover(gds_info->nam->nam$l_esa, gds_info->nam->nam$b_esl,
				gv_cur_region->dyn.addr->fname, gv_cur_region->dyn.addr->fname_len);
			gds_info->nam->nam$b_esl = gds_info->fab->fab$b_fns = gv_cur_region->dyn.addr->fname_len;
			memcpy(gds_info->fab->fab$l_fna, gv_cur_region->dyn.addr->fname, gds_info->fab->fab$b_fns);
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
	return SS$_NORMAL;
}
