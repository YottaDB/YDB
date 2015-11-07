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

#include <fab.h>
#include <rab.h>
#include <nam.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <psldef.h>
#include <secdef.h>
#include <syidef.h>
#include <efndef.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "efn.h"
#include "sleep_cnt.h"
#include "util.h"
#include "send_msg.h"
#include "del_sec.h"
#include "mem_list.h"
#include "disk_block_available.h"
#include "init_sec.h"
#include "mucregini.h"
#include "mu_cre_file.h"
#include "mu_cre_vms_structs.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "iosb_disk.h"
#include "iosp.h"
#include "iormdef.h"
#include "shmpool.h"	/* Needed for the shmpool structures */

GBLREF gd_region 	*gv_cur_region;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF sgmnt_addrs 	*cs_addrs;

#define MAX_GBL_NAME_LEN 15
#define BLK_SIZE (((gd_segment*)gv_cur_region->dyn.addr)->blk_size)

error_def(ERR_BADACCMTHD);
error_def(ERR_DBFILERR);
error_def(ERR_MUNOSTRMBKUP);
error_def(ERR_LOWSPACECRE);

unsigned char mu_cre_file(void)
{
	unsigned char		*inadr[2], *c, exit_stat;
	enum db_acc_method	temp_acc_meth;
	uint4			lcnt, retadr[2];
	int4			blk_init_size, initial_alq, free_blocks;
	gtm_uint64_t		free_blocks_ll, blocks_for_extension;
	char			buff[GLO_NAME_MAXLEN], fn_buff[MAX_FN_LEN];
	unsigned int		status;
	int			free_space;
	struct FAB		*fcb;
	struct NAM		nam;
	gds_file_id		new_id;
	io_status_block_disk	iosb;
	char			node[16];
	short			len;
	struct {
		short	blen;
		short	code;
		char	*buf;
		short	*len;
		int4	terminator;
	} item = {15, SYI$_NODENAME, &node, &len, 0};
	$DESCRIPTOR(desc, buff);

	exit_stat = EXIT_NRM;
/* The following calculations should duplicate the BT_SIZE macro from GDSBT and the LOCK_BLOCK macro from GDSFHEAD.H,
 * but without using a sgmnt_data which is not yet set up at this point
 */

#ifdef GT_CX_DEF
	/* This section needs serious chnages for the fileheader changes in V5 if it is ever resurrected */
	over_head = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_DFLT
			+ (WC_MAX_BUFFS + getprime(WC_MAX_BUFFS) + 1) * SIZEOF(bt_rec), DISK_BLOCK_SIZE);
	if (gv_cur_region->dyn.addr->acc_meth == dba_bg)
	{
		free_space = over_head - DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_DFLT
			+ (gv_cur_region->dyn.addr->global_buffers + getprime(gv_cur_region->dyn.addr->global_buffers) + 1)
				* SIZEOF(bt_rec), DISK_BLOCK_SIZE);
		over_head += gv_cur_region->dyn.addr->lock_space ? gv_cur_region->dyn.addr->lock_space
								 : DEF_LOCK_SIZE / OS_PAGELET_SIZE;
	} else if (gv_cur_region->dyn.addr->acc_meth == dba_mm)
	{
		free_space = over_head - DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_DFLT, DISK_BLOCK_SIZE);
		if (gv_cur_region->dyn.addr->lock_space)
		{
			over_head += gv_cur_region->dyn.addr->lock_space;
			free_space += gv_cur_region->dyn.addr->lock_space;
		} else
		{
			over_head += DEF_LOCK_SIZE / OS_PAGELET_SIZE;
			free_space += DEF_LOCK_SIZE / OS_PAGELET_SIZE;
		}
	}
	free_space *= DISK_BLOCK_SIZE;
#else
	assert(START_VBN_CURRENT > DIVIDE_ROUND_UP(SIZEOF_FILE_HDR_DFLT, DISK_BLOCK_SIZE));
	free_space = ((START_VBN_CURRENT - 1) * DISK_BLOCK_SIZE) - SIZEOF_FILE_HDR_DFLT;
#endif
	switch (gv_cur_region->dyn.addr->acc_meth)
	{
		case dba_bg:
		case dba_mm:
			mu_cre_vms_structs(gv_cur_region);
			fcb = ((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->fab;
			cs_addrs = &((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->s_addrs;

			fcb->fab$b_shr &= FAB$M_NIL;	/* No access to this file while it is created */
			fcb->fab$l_nam = &nam;
			nam = cc$rms_nam;
			/* There are (bplmap - 1) non-bitmap blocks per bitmap, so add (bplmap - 2) to number of non-bitmap blocks
			 * and divide by (bplmap - 1) to get total number of bitmaps for expanded database. (must round up in this
			 * manner as every non-bitmap block must have an associated bitmap)
			*/
			fcb->fab$l_alq += DIVIDE_ROUND_UP(fcb->fab$l_alq, BLKS_PER_LMAP - 1);	/* Bitmaps */
			blk_init_size = fcb->fab$l_alq;
			fcb->fab$l_alq *= BLK_SIZE / DISK_BLOCK_SIZE;
			fcb->fab$l_alq += START_VBN_CURRENT - 1;
			initial_alq = fcb->fab$l_alq;
			fcb->fab$w_mrs = 512;				/* no longer a relevent field to us */
			break;
		case dba_usr:
			util_out_print("Database file for region !AD not created; access method is not GDS.", TRUE,
				REG_LEN_STR(gv_cur_region));
			return EXIT_WRN;
		default:
			gtm_putmsg(VARLSTCNT(1) ERR_BADACCMTHD);
			return EXIT_ERR;
	}
	nam.nam$b_ess = SIZEOF(fn_buff);
	nam.nam$l_esa = fn_buff;
	nam.nam$b_nop |= NAM$M_SYNCHK;
	status = sys$parse(fcb, 0, 0);
	if (RMS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, fcb->fab$b_fns, fcb->fab$l_fna, status, 0, fcb->fab$l_stv, 0);
		return EXIT_ERR;
	}
	if (nam.nam$b_node != 0)
	{
		status = sys$getsyiw(EFN$C_ENF, 0, 0, &item, &iosb, 0, 0);
		if (SS$_NORMAL == status)
			status = iosb.cond;
		if (SS$_NORMAL == status)
		{
			if (len == nam.nam$b_node-2 && !memcmp(nam.nam$l_esa, node, len))
			{
				fcb->fab$l_fna = nam.nam$l_esa + nam.nam$b_node;
				fcb->fab$b_fns = nam.nam$b_esl - nam.nam$b_node;
			}
		} else
		{
			util_out_print("Could not get node for !AD.", TRUE, REG_LEN_STR(gv_cur_region));
			exit_stat = EXIT_WRN;
		}
	}
	assert(gv_cur_region->dyn.addr->acc_meth == dba_bg || gv_cur_region->dyn.addr->acc_meth == dba_mm);
	nam.nam$l_esa = NULL;
	nam.nam$b_esl = 0;
	status = sys$create(fcb);
	if (status != RMS$_CREATED && status != RMS$_FILEPURGED)
	{
		switch(status)
		{
			case RMS$_FLK:
		 		util_out_print("Database file for region !AD not created; currently locked by another user.", TRUE,
					REG_LEN_STR(gv_cur_region));
				exit_stat = EXIT_INF;
				break;
			case RMS$_NORMAL:
		 		util_out_print("Database file for region !AD not created; already exists.", TRUE,
					REG_LEN_STR(gv_cur_region));
				exit_stat = EXIT_INF;
				break;
			case RMS$_SUPPORT:
				util_out_print("Database file for region !AD not created; cannot create across network.", TRUE,
					REG_LEN_STR(gv_cur_region));
				exit_stat = EXIT_WRN;
				break;
			case RMS$_FUL:
				send_msg(VARLSTCNT(8) ERR_DBFILERR, 2, fcb->fab$b_fns, fcb->fab$l_fna,
					status, 0, fcb->fab$l_stv, 0);
				/* intentionally falling through */
			default:
				gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, fcb->fab$b_fns, fcb->fab$l_fna,
					status, 0, fcb->fab$l_stv, 0);
				exit_stat = EXIT_ERR;
		}
		sys$dassgn(fcb->fab$l_stv);
		return exit_stat;
	}

	memcpy(new_id.dvi, nam.nam$t_dvi, SIZEOF(nam.nam$t_dvi));
	memcpy(new_id.did, nam.nam$w_did, SIZEOF(nam.nam$w_did));
	memcpy(new_id.fid, nam.nam$w_fid, SIZEOF(nam.nam$w_fid));
	global_name("GT$S", &new_id, buff);		/* 2nd parm is actually a gds_file_id * in global_name */
	desc.dsc$w_length = buff[0];			/* By definition, a gds_file_id is dvi,fid,did from nam */
	desc.dsc$a_pointer = &buff[1];
	cs_addrs->db_addrs[0] = cs_addrs->db_addrs[1] = inadr[0] = inadr[1] = inadr;	/* used to determine p0 or p1 allocation */
	status = init_sec(cs_addrs->db_addrs, &desc, fcb->fab$l_stv, (START_VBN_CURRENT - 1),
			  SEC$M_DZRO|SEC$M_GBL|SEC$M_WRT|SEC$M_EXPREG);
	if ((SS$_CREATED != status) && (SS$_NORMAL != status))
	{
		gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, fcb->fab$b_fns, fcb->fab$l_fna, status, 0, fcb->fab$l_stv, 0);
		sys$dassgn(fcb->fab$l_stv);
		return EXIT_ERR;
	}
	cs_data = (sgmnt_data *)cs_addrs->db_addrs[0];
	memset(cs_data, 0, SIZEOF_FILE_HDR_DFLT);
	cs_data->createinprogress = TRUE;
	cs_data->trans_hist.total_blks = (initial_alq - (START_VBN_CURRENT - 1)) / (BLK_SIZE / DISK_BLOCK_SIZE);
	/* assert that total_blks stored in file-header = non-bitmap blocks (initial allocation) + bitmap blocks */
	assert(cs_data->trans_hist.total_blks == gv_cur_region->dyn.addr->allocation +
				DIVIDE_ROUND_UP(gv_cur_region->dyn.addr->allocation, BLKS_PER_LMAP - 1));
	cs_data->start_vbn = START_VBN_CURRENT;
	temp_acc_meth = gv_cur_region->dyn.addr->acc_meth;
	cs_data->acc_meth = gv_cur_region->dyn.addr->acc_meth = dba_bg;
	cs_data->extension_size = gv_cur_region->dyn.addr->ext_blk_count;
	mucregini(blk_init_size);
	cs_addrs->hdr->free_space = free_space;
#ifndef GT_CX_DEF
	cs_addrs->hdr->unbacked_cache = TRUE;
#endif
	cs_data->acc_meth = gv_cur_region->dyn.addr->acc_meth = temp_acc_meth;
	cs_data->createinprogress = FALSE;
	if (SS$_NORMAL == (status = disk_block_available(fcb->fab$l_stv, &free_blocks)))
	{
		blocks_for_extension = (cs_data->blk_size / DISK_BLOCK_SIZE *
				  (DIVIDE_ROUND_UP(EXTEND_WARNING_FACTOR * (gtm_uint64_t)cs_data->extension_size, BLKS_PER_LMAP - 1)
					 + EXTEND_WARNING_FACTOR * (gtm_uint64_t)cs_data->extension_size));
		if ((gtm_uint64_t)free_blocks < blocks_for_extension)
		{
			free_blocks_ll = (gtm_uint64_t)free_blocks;
			gtm_putmsg(VARLSTCNT(8) ERR_LOWSPACECRE, 6, fcb->fab$b_fns, fcb->fab$l_fna, EXTEND_WARNING_FACTOR,
					&blocks_for_extension, DISK_BLOCK_SIZE, &free_blocks_ll);
			send_msg(VARLSTCNT(8) ERR_LOWSPACECRE, 6, fcb->fab$b_fns, fcb->fab$l_fna, EXTEND_WARNING_FACTOR,
					&blocks_for_extension, DISK_BLOCK_SIZE, &free_blocks_ll);
		}
	}
	if (SS$_NORMAL == (status = sys$updsec(((vms_gds_info *)(gv_cur_region->dyn.addr->file_cntl->file_info))->s_addrs.db_addrs,
			NULL, PSL$C_USER, 0, efn_immed_wait, &iosb, NULL, 0)))
	{
		status = sys$synch(efn_immed_wait, &iosb);
		if (SS$_NORMAL == status)
			status = iosb.cond;
	} else  if (SS$_NOTMODIFIED == status)
		status = SS$_NORMAL;
	if (SS$_NORMAL == status)
		status = del_sec(SEC$M_GBL, &desc, 0);
	if (SS$_NORMAL == status)
		status = sys$deltva(cs_addrs->db_addrs, retadr, PSL$C_USER);
	if (SS$_NORMAL == status)
		status = sys$dassgn(fcb->fab$l_stv);
	if (SS$_NORMAL == status)
	{
	 	util_out_print("Database file for region !AD created.", TRUE, REG_LEN_STR(gv_cur_region));
		/* the open and close are an attempt to ensure that the file is available, not under the control of an ACP,
		 * before MUPIP exits */
		fcb->fab$b_shr = FAB$M_SHRPUT | FAB$M_SHRGET | FAB$M_UPI;
		fcb->fab$l_fop = 0;
		for (lcnt = 1;  (60 * MAX_OPEN_RETRY) >= lcnt;  lcnt++)
		{	/* per VMS engineering a delay is expected.  We will wait up to an hour as a
			 * Delete Global Section operation is essentially and inherently asynchronous in nature
			 * and could take an arbitrary amount of time.
			 */
			if (RMS$_FLK != (status = sys$open(fcb, NULL, NULL)))
				break;
			wcs_sleep(lcnt);
		}
		assert(RMS$_NORMAL == status);
		if (RMS$_NORMAL == status)
		{
			status = sys$close(fcb);
			assert(RMS$_NORMAL == status);
		}
		if (RMS$_NORMAL != status)
			exit_stat = EXIT_WRN;
	} else
		exit_stat = EXIT_ERR;
	if (RMS$_NORMAL != status)
		gtm_putmsg(VARLSTCNT(8) ERR_DBFILERR, 2, fcb->fab$b_fns, fcb->fab$l_fna, status, 0, fcb->fab$l_stv, 0);
	if ((MAX_RMS_RECORDSIZE - SIZEOF(shmpool_blk_hdr)) < cs_data->blk_size)
		gtm_putmsg(VARLSTCNT(5) ERR_MUNOSTRMBKUP, 3, fcb->fab$b_fns, fcb->fab$l_fna, 32 * 1024 - DISK_BLOCK_SIZE);
	return exit_stat;
}
