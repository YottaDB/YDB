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

#include <efndef.h>
#include <descrip.h>
#include <fab.h>
#include <iodef.h>
#include <lckdef.h>
#include <nam.h>
#include <psldef.h>
#include <rmsdef.h>
#include <secdef.h>
#include <ssdef.h>
#include <syidef.h>

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "efn.h"
#include "error.h"
#include "jnl.h"
#include "timedef.h"
#include "vmsdtype.h"
#include "sleep_cnt.h"
#include "locks.h"
#include "mlk_shr_init.h"
#include "mu_rndwn_file.h"
#include "dbfilop.h"
#include "gvcst_protos.h"	/* for gvcst_init_sysops prototype */
#include "del_sec.h"
#include "mem_list.h"
#include "gds_rundown.h"
#include "init_sec.h"
#include "gtmmsg.h"
#include "wcs_sleep.h"
#include "wcs_flu.h"
#include "shmpool.h"	/* Needed for the shmpool structures */

#define DEF_NODE	0xFFFF

GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data	*cs_data;
GBLREF	gd_region	*gv_cur_region;
GBLREF	jnl_gbls_t	jgbl;
#ifdef DEBUG
GBLREF	boolean_t	in_mu_rndwn_file;
#endif

LITREF	char		gtm_release_name[];
LITREF	int4		gtm_release_name_len;

error_def(ERR_BADDBVER);
error_def(ERR_BADGBLSECVER);
error_def(ERR_CLSTCONFLICT);
error_def(ERR_DBFILERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_FILEIDGBLSEC);
error_def(ERR_GBLSECNOTGDS);
error_def(ERR_TEXT);
error_def(ERR_VERMISMATCH);

OS_PAGE_SIZE_DECLARE

int	mu_rndwn_file(bool standalone)			/* operates on gv_cur_region */
{
	sgmnt_data		*temp_cs_data;
	jnl_private_control	*jpc;
	vms_gds_info		*gds_info;
	vms_lock_sb		*file_lksb;
	file_control		*fc;
	struct dsc$descriptor_s	section;
	uint4			flags, lcnt, size, status, dbfop_status, owner_node, node, init_status, outaddrs[2];
	boolean_t		clustered, read_write, is_bg;
	char			name_buff[GLO_NAME_MAXLEN], now_running[MAX_REL_NAME], node_buff[9];
	typedef struct
        {
                item_list_3     ilist;
                int4            terminator;
        } syistruct;
	syistruct		syi_list;
	unsigned short		retlen, iosb[4];
	boolean_t		mu_rndwn_status;
	node_local_ptr_t	cnl;
	gtm_uint64_t		sec_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_rndwn_status = FALSE;
	node = 0;
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->op = FC_OPEN;
	fc->file_type = dba_bg;	/* note that the file_type here does not imply the access method of the database (csd->acc_meth).
				 *	instead this is just an indication that database file I/O is done through sys$qiow() calls
				 *	and not sys$updsec() calls to dbfilop() which in turn require a fc->file_type of dba_bg
				 * later if gvcst_init() is attempted on the same database, this gets reset appropriately.
				 */
	dbfop_status = dbfilop(fc);
	if (SS$_NORMAL != dbfop_status)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
		gtm_putmsg(VARLSTCNT(1) dbfop_status);
		return mu_rndwn_status;
	}
	gds_info = FILE_INFO(gv_cur_region);
	read_write = (FALSE == gv_cur_region->read_only);
	syi_list.ilist.item_code = SYI$_NODE_CSID;
        syi_list.ilist.buffer_address = &node;
        syi_list.ilist.buffer_length = SIZEOF(node);
        syi_list.ilist.return_length_address = &retlen;
        syi_list.terminator = 0;
        status = sys$getsyiw(EFN$C_ENF, NULL, NULL, &syi_list, iosb, NULL, 0);
	if (SS$_NORMAL == status)
                status = iosb[0];
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(1) status);
		sys$dassgn(gds_info->fab->fab$l_stv);
		return mu_rndwn_status;
	}
	if (0 == node)
		node = DEF_NODE;
	gv_cur_region->node = node;		/* Leave it so that it goes into the value block */
	cs_addrs = &gds_info->s_addrs;
	cs_addrs->hdr = NULL;
        cs_addrs->nl = NULL;
        cs_addrs->jnl = NULL;
	cs_addrs->db_addrs[0] = cs_addrs->db_addrs[1] = NULL;
	cs_addrs->lock_addrs[0] = cs_addrs->lock_addrs[1] = NULL;
	ESTABLISH_RET(gds_rundown_ch, FALSE);
	global_name("GT$S", &gds_info->file_id, name_buff);
	section.dsc$a_pointer = &name_buff[1];
	section.dsc$w_length = name_buff[0];
	section.dsc$b_dtype = DSC$K_DTYPE_T;
	section.dsc$b_class = DSC$K_CLASS_S;
	file_lksb = &gds_info->file_cntl_lsb;
	file_lksb->valblk[0] = gv_cur_region->node;
	/* These locks must be taken out before mapping the file to a section, and released after unmapping the section */
	/* Note:  Rather than simply taking out this lock at PW mode, we take it out at NL mode and then convert to EX.
	 * Lock requests in the conversion queue are serviced before locks in the waiting queue;  heavy GT.CX activity
	 * on a given database can potentially keep the conversion queue busy enough to keep new lock requests (especially
	 * at higher lock modes like EX) bottled up on the waiting queue indefinitely.  Since NL mode lock requests are
	 * compatible with all other lock modes, they don't go on the waiting queue;  they are always granted.  Then the
	 * compatible with all other lock modes, and since they don't go to the waiting queue if LCK$M_EXPEDITE is specified;
	 * they are always granted. Then the subsequent conversion request will rapidly move to the head of the conversion
	 * queue and ultimately be granted.
	 */
	status = gtm_enqw(EFN$C_ENF, LCK$K_NLMODE, file_lksb, LCK$M_SYSTEM | LCK$M_EXPEDITE,
							&section, 0, NULL, 0, NULL, PSL$C_USER, 0);
	if (SS$_NORMAL == status)
		status = file_lksb->cond;
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
		gtm_putmsg(VARLSTCNT(1) status);
		sys$dassgn(gds_info->fab->fab$l_stv);
		return mu_rndwn_status;
	}
	for (lcnt = 1; lcnt <= MAX_LCK_TRIES; lcnt++)
	{	/* until the following lock is available, there's a transition going on */
		status = gtm_enq(efn_immed_wait, LCK$K_PWMODE, file_lksb, LCK$M_CONVERT | LCK$M_NOQUEUE | LCK$M_NODLCKWT,
					NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = file_lksb->cond;
		if (SS$_NOTQUEUED != status)
			break;
		wcs_sleep(lcnt);
	}
	assert(MAX_LCK_TRIES > lcnt);
	if (SS$_NORMAL == status)
	{
		status = gtm_enqw(EFN$C_ENF, LCK$K_EXMODE, file_lksb,
				LCK$M_CONVERT | LCK$M_NOQUEUE | LCK$M_NODLCKWT, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = file_lksb->cond;
	}
	if (SS$_NORMAL == status)
	{	/* convert the lock from EX to PW in order to update the value of the lsb */
		status = gtm_enqw(EFN$C_ENF, LCK$K_PWMODE, file_lksb, LCK$M_VALBLK | LCK$M_CONVERT | LCK$M_NODLCKBLK,
				NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		if (SS$_NORMAL == status)
			status = file_lksb->cond;
	}
	if (SS$_NORMAL != status)
	{
		if (SS$_NOTQUEUED == status)
			status = RMS$_FLK;
		gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
		gtm_putmsg(VARLSTCNT(1) status);
		status = gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		file_lksb->lockid = 0;
		sys$dassgn(gds_info->fab->fab$l_stv);
		return mu_rndwn_status;
	}
	/* -------------- From this point on, I should have standalone access db_init() might be pending ----------------- */
	/* Allocate temporary storage for the database file header and read it in */
	/* We only need to read SIZEOF(sgmnt_data) here */
	temp_cs_data = malloc(ROUND_UP(SIZEOF(sgmnt_data), DISK_BLOCK_SIZE));
	fc->op = FC_READ;
	fc->op_buff = temp_cs_data;
	fc->op_len = SGMNT_HDR_LEN;
	fc->op_pos = 1;
	dbfop_status = dbfilop(fc);
	if (SS$_NORMAL != dbfop_status)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
		gtm_putmsg(VARLSTCNT(1) dbfop_status);
		status = gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
		assert(SS$_NORMAL == status);
		file_lksb->lockid = 0;
		sys$dassgn(gds_info->fab->fab$l_stv);
		return mu_rndwn_status;
	}
	if ((gv_cur_region->dyn.addr->acc_meth != temp_cs_data->acc_meth)
		&& ((dba_mm == temp_cs_data->acc_meth) || (dba_bg == temp_cs_data->acc_meth)))
	{	/* Note that it is possible that temp_cs_data->acc_meth is not MM or BG and yet is a valid global section.
		 * This is possible if that global section was created by a version of GT.M with a different database format.
		 * We will issue BADDBVER error for such sections later but until then let us work with BG access method.
		 */
		assert(dba_cm != gv_cur_region->dyn.addr->acc_meth);
		gv_cur_region->dyn.addr->acc_meth = temp_cs_data->acc_meth;
	}
	dbsecspc(gv_cur_region, temp_cs_data, &sec_size);
	flags = SEC$M_GBL | SEC$M_SYSGBL;
        if (is_bg = (dba_bg == temp_cs_data->acc_meth))
                flags |= SEC$M_WRT | SEC$M_PAGFIL | SEC$M_PERM;
        else if (read_write)
                flags |= SEC$M_WRT;
	status = init_status = init_sec(cs_addrs->db_addrs, &section, gds_info->fab->fab$l_stv, sec_size, flags);
	if ((SS$_NORMAL == init_status) || (SS$_CREATED == init_status))
	{
		if (!gv_cur_region->dyn.addr->fname_len)
		{	/* coming in from MUPIP RUNDOWN with no arguments. fill in filename from the global section */
			assert(SS$_NORMAL == init_status);
			cnl = cs_addrs->db_addrs[0];
			assert(SIZEOF(cnl->fname) <= SIZEOF(gv_cur_region->dyn.addr->fname));
			memcpy(gv_cur_region->dyn.addr->fname, cnl->fname, SIZEOF(cnl->fname));
			gv_cur_region->dyn.addr->fname[SIZEOF(cnl->fname) - 1] = '\0';
			gv_cur_region->dyn.addr->fname_len = strlen(gv_cur_region->dyn.addr->fname);
		}
		if (memcmp(temp_cs_data->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			status = ERR_DBNOTGDS;
		else if (MEMCMP_LIT(temp_cs_data->label, GDS_LABEL))
			status = ERR_BADDBVER;
		/*	the following conditions should also be reported
		 * 	else if (temp_cs_data->createinprogress)
		 * 	else if (temp_cs_data->trans_hist.curr_tn > cs_data->trans_hist.curr_tn)
		 */
		if (SS$_NORMAL == status)
		{
			memcpy(now_running, temp_cs_data->now_running, MAX_REL_NAME);
			/* issue VERMISMATCH error if now_running in node_local does not match that of the file-header.
			 * there is one exception and that is to see if now_running in the file-header is the NULL string.
			 * 	(this is currently possible if the last process to detach from global section had read-only
			 * 	access to the database and was abnormally terminated leaving the global section orphaned).
			 * in this case, we do not want to issue a VERMISMATCH error.
			 */
			if (memcmp(now_running, gtm_release_name, gtm_release_name_len + 1) && (now_running[0]))
				status = ERR_VERMISMATCH;
		}
		/* similar to the VERMISMATCH error exception above, we need to except the case owner_node is ZERO.
		 * this needs to be reworked in a much better way for V4.3-001D --- nars -- 2002/09/11
		 */
		if ((init_status == status) && (owner_node = temp_cs_data->owner_node))
		{
			if ((SS$_NORMAL == status) && (node != owner_node))
			{
				status = ERR_CLSTCONFLICT;
				i2hex(owner_node, node_buff, 8);
			} else if ((SS$_CREATED == status) && read_write)
			{
				memset(temp_cs_data->machine_name, 0, MAX_MCNAMELEN);
				temp_cs_data->owner_node = 0;
				temp_cs_data->freeze = 0;
				fc->op = FC_WRITE;
				fc->op_len = SGMNT_HDR_LEN;
				fc->op_pos = 1;
				dbfop_status = dbfilop(fc);
				if (SS$_NORMAL != dbfop_status)
				{
					gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
					gtm_putmsg(VARLSTCNT(1) dbfop_status);
					status = gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
					assert(SS$_NORMAL == status);
					file_lksb->lockid = 0;
					sys$dassgn(gds_info->fab->fab$l_stv);
					free(temp_cs_data);
					return mu_rndwn_status;
				}
			}
		}
	}
	if (SS$_NORMAL != status)
	{	/* Note:  this includes the successful condition SS$_CREATED */
		REVERT;
		outaddrs[0] = cs_addrs->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
		outaddrs[1] = cs_addrs->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
		if (FALSE == is_va_free(outaddrs[0]))
			gtm_deltva(outaddrs, NULL, PSL$C_USER);
		/* Don't delete the global section if VERMISMATCH/CLSTCONFLICT/DBNOTGDS/BADDBVER error on an existing section */
		if (ERR_CLSTCONFLICT != status && ERR_VERMISMATCH != status && ERR_DBNOTGDS != status && ERR_BADDBVER != status)
			del_sec(SEC$M_SYSGBL, &section, NULL);
		free(temp_cs_data);
		if ((FALSE == standalone) || (SS$_CREATED != status))
		{
			gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
			file_lksb->lockid = 0;
		}
		if (SS$_CREATED == status)
		{
			mu_rndwn_status = TRUE;
			file_lksb->valblk[0] = 0;	/* reset to 0 since section has been deleted */
			sys$dassgn(gds_info->fab->fab$l_stv);
			return mu_rndwn_status;
		}
		if (ERR_VERMISMATCH == status)
			gtm_putmsg(VARLSTCNT(8) ERR_VERMISMATCH, 6, DB_LEN_STR(gv_cur_region),
				gtm_release_name_len, gtm_release_name, LEN_AND_STR(now_running));
		else if (ERR_CLSTCONFLICT == status)
			gtm_putmsg(VARLSTCNT(6) ERR_CLSTCONFLICT, 4, DB_LEN_STR(gv_cur_region), SIZEOF(node_buff), node_buff);
		else if ((ERR_DBNOTGDS == status) || (ERR_BADDBVER == status))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
			gtm_putmsg(VARLSTCNT(4) status, 2, GDS_LABEL_SZ - 1, GDS_LABEL);
		} else
		{
			gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
			gtm_putmsg(VARLSTCNT(1) status);
		}
		sys$dassgn(gds_info->fab->fab$l_stv);
		return mu_rndwn_status;
	}
	/* The database file is valid and up-to-date with respect to the file header;
	 * map global sections and establish pointers to shared memory
	 */
        if (is_bg)
                cs_addrs->nl = cs_addrs->db_addrs[0];
        else
        {
		name_buff[4] = 'L';
                size = ROUND_UP(LOCK_SPACE_SIZE(temp_cs_data) + NODE_LOCAL_SPACE(temp_cs_data) + JNL_SHARE_SIZE(temp_cs_data)
				+ SHMPOOL_BUFFER_SIZE, OS_PAGE_SIZE) / OS_PAGELET_SIZE;
                status = gtm_expreg(size, cs_addrs->lock_addrs, PSL$C_USER, 0);
		assert(cs_addrs->lock_addrs[0] + size * OS_PAGELET_SIZE - 1 == cs_addrs->lock_addrs[1]);
		if (SS$_NORMAL == status)
			status = init_sec(cs_addrs->lock_addrs, &section, 0, size,
						SEC$M_PAGFIL | SEC$M_GBL | SEC$M_WRT | SEC$M_SYSGBL);
		if ((SS$_NORMAL != status) && (SS$_CREATED != status))
                {
                        cs_addrs->lock_addrs[0] = NULL;
			gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
			file_lksb->lockid = 0;
                        gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
			gtm_putmsg(VARLSTCNT(1) status);
			sys$dassgn(gds_info->fab->fab$l_stv);
			free(temp_cs_data);
			return mu_rndwn_status;
                }
                cs_addrs->nl = cs_addrs->lock_addrs[0];
        }
	/* The handling of shared memory rundown differs between Unix and VMS in the following areas. The VMS checks above
	 * for GDS_LABEL and now_running are using the file header, whereas Unix uses shared memory. In VMS, running down
	 * an older version's partially initialized shared memory will not issue a VERMISMATCH error since now_running[0]
	 * would be 0 -- it is treated the same as the case where the last process to detach had read-only access to the
	 * database. In both cases, the Unix approach seems better.
	 */
	if (cs_addrs->nl->glob_sec_init)
	{
		cs_addrs->critical = (sm_uc_ptr_t)(cs_addrs->nl) + NODE_LOCAL_SIZE;
		/* Note: Here we check jnl_sate from database file and
		 * its value cannot change without standalone access.
		 * In other words it is not necessary to read shared memory for the test (jnl_state != jnl_notallowed)
		 * The jnl_buff buffer should be initialized irrespective of read/write process */
		JNL_INIT(cs_addrs, gv_cur_region, temp_cs_data);
		cs_addrs->shmpool_buffer = (shmpool_buff_hdr_ptr_t)((sm_uc_ptr_t)(cs_addrs->nl) + NODE_LOCAL_SPACE(temp_cs_data)
								    + JNL_SHARE_SIZE(temp_cs_data));
		cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)(cs_addrs->shmpool_buffer) + SHMPOOL_BUFFER_SIZE;
		cs_addrs->lock_addrs[1] = cs_addrs->lock_addrs[0] + LOCK_SPACE_SIZE(temp_cs_data) - 1;
		cs_data = cs_addrs->hdr = is_bg ? (cs_addrs->lock_addrs[1] + 1 + CACHE_CONTROL_SIZE(temp_cs_data))
						: cs_addrs->db_addrs[0];
		assert(cs_data->acc_meth == temp_cs_data->acc_meth);
		assert((-(SIZEOF(uint4) * 2) & (uint4)cs_addrs->critical) == (uint4)cs_addrs->critical);
		assert(0 == ((OS_PAGE_SIZE - 1) & (int)cs_addrs->critical));
		assert(0 == ((OS_PAGE_SIZE - 1) & (int)cs_addrs->nl));
		assert((!(JNL_ALLOWED(cs_data)))
		       || (0 == ((OS_PAGE_SIZE - 1) & (int)((char_ptr_t)cs_addrs->jnl->jnl_buff - JNL_NAME_EXP_SIZE))));
		assert(0 == ((OS_PAGE_SIZE - 1) & (int)cs_addrs->shmpool_buffer));
		assert(0 == ((OS_PAGE_SIZE - 1) & (int)cs_addrs->lock_addrs[0]));
		assert(0 == ((OS_PAGE_SIZE - 1) & (int)(cs_addrs->lock_addrs[1] + 1)));
		assert(0 == ((OS_PAGE_SIZE - 1) & (int)cs_addrs->hdr));
		/* -------- verify pointers from our calculation vs. the copy in shared memory ---------- */
		assert((sm_off_t)((sm_uc_ptr_t)cs_addrs->critical - (sm_uc_ptr_t)cs_addrs->nl) == cs_addrs->nl->critical);
		assert((!(JNL_ALLOWED(cs_data))) ||
		       ((sm_off_t)((sm_uc_ptr_t)cs_addrs->jnl->jnl_buff - (sm_uc_ptr_t)cs_addrs->nl)) == cs_addrs->nl->jnl_buff);
		assert((sm_off_t)((sm_uc_ptr_t)cs_addrs->shmpool_buffer - (sm_uc_ptr_t)cs_addrs->nl)
			== cs_addrs->nl->shmpool_buffer);
		assert(!is_bg || (sm_off_t)((sm_uc_ptr_t)cs_addrs->hdr - (sm_uc_ptr_t)cs_addrs->nl) == cs_addrs->nl->hdr);
		assert((sm_off_t)((sm_uc_ptr_t)cs_addrs->lock_addrs[0] - (sm_uc_ptr_t)cs_addrs->nl) == cs_addrs->nl->lock_addrs);
		status = SS$_NORMAL;
		if (memcmp(cs_addrs->nl->label, GDS_LABEL, GDS_LABEL_SZ - 1))
		{
			name_buff[4] = 'S';
			if (memcmp(cs_addrs->nl->label, GDS_LABEL, GDS_LABEL_SZ - 3))
				status = ERR_GBLSECNOTGDS;
			else
				status = ERR_BADGBLSECVER;
		}
		if (SS$_NORMAL == status)
		{	/* missing the file_id.did since it came from fid_from_sec, and it's not needed for uniqueness anyway */
			memcpy(gds_info->file_id.did, &cs_addrs->nl->unique_id.file_id[SIZEOF(gds_info->file_id.dvi)],
			       SIZEOF(gds_info->file_id.did));
			if (memcmp(&cs_addrs->nl->unique_id.file_id[0], (char *)(&(gds_info->file_id)), SIZEOF(gds_file_id)))
				status = ERR_FILEIDGBLSEC;
		}
		if (SS$_NORMAL != status)
		{
			cs_addrs->lock_addrs[0] = NULL;
			gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
			file_lksb->lockid = 0;
			if (ERR_FILEIDGBLSEC == status)
				gtm_putmsg(VARLSTCNT(4) ERR_FILEIDGBLSEC, 2, DB_LEN_STR(gv_cur_region));
			else
				gtm_putmsg(VARLSTCNT(4) status, 2, name_buff[0], &name_buff[1]);
			sys$dassgn(gds_info->fab->fab$l_stv);
			free(temp_cs_data);
			return mu_rndwn_status;
		}
		/* Check to see that the fileheader in the shared segment is valid, so we won't endup flushing garbage to db file */
		if (memcmp(cs_data->label, GDS_LABEL, GDS_LABEL_SZ - 1))
		{
			cs_addrs->lock_addrs[0] = NULL;
			gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
			file_lksb->lockid = 0;
			if (memcmp(cs_data->label, GDS_LABEL, GDS_LABEL_SZ - 3))
			{
				status = ERR_DBNOTGDS;
				outaddrs[0] = cs_addrs->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
				outaddrs[1] = cs_addrs->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
				if (FALSE == is_va_free(outaddrs[0]))
					gtm_deltva(outaddrs, NULL, PSL$C_USER);
				del_sec(SEC$M_SYSGBL, &section, NULL);
			} else
				status = ERR_BADDBVER;
			gtm_putmsg(VARLSTCNT(8) status, 2, DB_LEN_STR(gv_cur_region),
				   ERR_TEXT, 2, RTS_ERROR_LITERAL("File header in the shared segment seems corrupt"));
			sys$dassgn(gds_info->fab->fab$l_stv);
			free(temp_cs_data);
			return mu_rndwn_status;
		}
		/* ------------------------- shared memory is OK --------------------------------- */
		assert(JNL_ALLOWED(cs_data) == JNL_ALLOWED(temp_cs_data));
		free(temp_cs_data);
		if (is_bg)
			db_csh_ini(cs_addrs);
		else
			SET_MM_BASE_ADDR(cs_addrs, cs_data);
		cs_addrs->nl->in_crit = 0;
		clustered = cs_data->clustered;
		cs_data->clustered = FALSE;
		db_common_init(gv_cur_region, cs_addrs, cs_data);  /* do initialization common to db_init() and mu_rndwn_file() */
		mlk_shr_init(cs_addrs->lock_addrs[0], cs_data->lock_space_size, cs_addrs, read_write);
		mutex_init(cs_addrs->critical, NUM_CRIT_ENTRY(cs_data), FALSE);
		gv_cur_region->open = TRUE;
		DEBUG_ONLY(in_mu_rndwn_file = TRUE);
		TREF(donot_write_inctn_in_wcs_recover) = TRUE;
		/* If csa->nl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and therefore we should
		 * 	not flush shared memory contents to disk as they might be in an inconsistent state.
		 * In this case, we will go ahead and remove shared memory (without flushing the contents) in this routine.
		 * A reissue of the recover/rollback command will restore the database to a consistent state.
		 */
		if (!cs_addrs->nl->donotflush_dbjnl)
		{
			/* At this point we are holding standalone access and are about to invoke wcs_flu/wcs_recover. If
			 * one or more GT.M processes were at the midst of phase 2 commit, wcs_recover/wcs_flu invokes
			 * wcs_phase2_commit_wait to wait for the processes to complete the phase 2 commit. But, if we have
			 * standalone access, there is NO point waiting for the phase 2 commits to complete as the processes
			 * might have been killed. So, set wcs_phase2_commit_pidcnt to 0 so wcs_recover/wcs_flu skips
			 * invoking wcs_phase2_commit_wait
			 */
			cs_addrs->nl->wcs_phase2_commit_pidcnt = 0;
			if (is_bg)
			{	/* No WCSFLU_*_EPOCH is passed here, as we aren't sure of the state, so no EPOCHs are written.
				 * If we write an EPOCH record, recover may get confused
				 * Note that for journaling we do not call jnl_file_close() with TRUE for second parameter.
				 * As a result journal file might not have an EOF record.
				 * So, a new process will switch the journal file and cut the journal file link,
				 * though it might be a good journal without an EOF
				 */
				wcs_flu(read_write ? WCSFLU_FLUSH_HDR : WCSFLU_NONE);
			}
			jpc = cs_addrs->jnl;
			if (NULL != jpc)
			{
				grab_crit(gv_cur_region);
				if (NOJNL != jpc->channel)
					jnl_file_close(gv_cur_region, FALSE, FALSE);
				/* release the journal file lock if we have a non-zero jnllsb->lockid */
				if (0 != jpc->jnllsb->lockid)
				{
					status = gtm_deq(jpc->jnllsb->lockid, NULL, PSL$C_USER, 0);
					assert(SS$_NORMAL == status);
					jpc->jnllsb->lockid = 0;
				}
				if (NULL != jpc->jnllsb)
					free(jpc->jnllsb);
				free(jpc);
				cs_addrs->jnl = NULL;
				rel_crit(gv_cur_region);
			}
		}
		DEBUG_ONLY(in_mu_rndwn_file = FALSE);
		TREF(donot_write_inctn_in_wcs_recover) = FALSE;
		gv_cur_region->open = FALSE;
		if (read_write)
		{
			memset(cs_data->now_running, 0, SIZEOF(cs_data->now_running));
			cs_data->owner_node = 0;
			cs_data->freeze = 0;
			fc->op = FC_WRITE;
			fc->op_buff = cs_data;
			fc->op_len = SIZEOF_FILE_HDR(cs_data);  /* include master map */
			fc->op_pos = 1;
			dbfop_status = dbfilop(fc);
			if (SS$_NORMAL != dbfop_status)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
				gtm_putmsg(VARLSTCNT(1) dbfop_status);
				status = gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
				assert(SS$_NORMAL == status);
				file_lksb->lockid = 0;
				sys$dassgn(gds_info->fab->fab$l_stv);
				return mu_rndwn_status;
			}
		}
		if (cs_data->clustered = clustered)		/* Note embedded assignment */
		{
			fc->op = FC_WRITE;
			fc->op_buff = cs_addrs->lock_addrs[0];
			fc->op_len = cs_data->lock_space_size;
			fc->op_pos = LOCK_BLOCK(cs_data) + 1;
			dbfop_status = dbfilop(fc);
			if (SS$_NORMAL != dbfop_status)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
				gtm_putmsg(VARLSTCNT(1) dbfop_status);
				status = gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
				assert(SS$_NORMAL == status);
				file_lksb->lockid = 0;
				sys$dassgn(gds_info->fab->fab$l_stv);
				return mu_rndwn_status;
			}
		}
	} else
		free(temp_cs_data);

	REVERT;
	if (!is_bg)
	{
		cs_addrs->lock_addrs[0] = (sm_uc_ptr_t)cs_addrs->nl;
		cs_addrs->lock_addrs[1] = cs_addrs->lock_addrs[0] + ROUND_UP(LOCK_SPACE_SIZE(cs_data) + NODE_LOCAL_SPACE(cs_data)
						+ JNL_SHARE_SIZE(cs_data) + SHMPOOL_BUFFER_SIZE, OS_PAGE_SIZE) - 1;
		gtm_deltva(cs_addrs->lock_addrs, NULL, PSL$C_USER);
		name_buff[4] = 'L';
		status = del_sec(SEC$M_SYSGBL, &section, NULL);
	}
	if (SS$_NORMAL == status)
	{
		outaddrs[0] = cs_addrs->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
		outaddrs[1] = cs_addrs->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
		if (FALSE == is_va_free(outaddrs[0]))
			gtm_deltva(outaddrs, NULL, PSL$C_USER);
		name_buff[4] = 'S';
		status = del_sec(SEC$M_SYSGBL, &section, NULL);
	}
	if (SS$_NORMAL != status)
	{
		gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
		file_lksb->lockid = 0;
		gtm_putmsg(VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region));
		gtm_putmsg(VARLSTCNT(1) status);
		sys$dassgn(gds_info->fab->fab$l_stv);
		return mu_rndwn_status;
	}
	if (FALSE == standalone)
	{
		gtm_deq(file_lksb->lockid, NULL, PSL$C_USER, 0);
		file_lksb->lockid = 0;
	} else
		file_lksb->valblk[0] = 0;	/* reset to 0 since section has been deleted */
	mu_rndwn_status = TRUE;
	sys$dassgn(gds_info->fab->fab$l_stv);
	return mu_rndwn_status;
}
