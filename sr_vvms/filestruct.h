/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* filestruct.h */

#include "gdsdbver.h"

#define GDS_LABEL_GENERIC 	"GDSDYNSEG"
#define GDS_LABEL 		GDS_LABEL_GENERIC GDS_CURR	/* This string must be of length GDS_LABEL_SZ */
#define GDS_RPL_LABEL 		"GDSRPLUNX03" 			/* This string must be of length GDS_LABEL_SZ */

typedef struct vms_gds_info_struct	/* BG and MM databases */
{
	struct FAB	*fab;
	struct NAM	*nam;
	struct XABFHC	*xabfhc;
	sgmnt_addrs	s_addrs;
	gds_file_id	file_id;
	vms_lock_sb	file_cntl_lsb;	/* replaces gd_region lsb */
	vms_lock_sb	cx_cntl_lsb;	/* replaces gd_region ref_lsb */
	struct XABPRO	*xabpro;
} vms_gds_info;

typedef struct vms_gd_info_struct	/* Global Directories */
{
	struct FAB	*fab;
	struct NAM	*nam;
} vms_gd_info;

typedef struct vms_file_info_struct	/* sequential files used by MUPIP */
{
	struct FAB	*fab;
	struct NAM	*nam;
	struct RAB	*rab;
} vms_file_info;

#define FILE_INFO(reg)	((vms_gds_info *)(reg)->dyn.addr->file_cntl->file_info)
#define FILE_ID(reg)	((vms_gds_info *)(reg)->dyn.addr->file_cntl->file_info)->file_id
#define GDS_INFO vms_gds_info
#define FI_FN(file_info)	((vms_file_info *)file_info)->fab->fab$b_fns
#define FI_FN_LEN(file_info)	((vms_file_info *)file_info)->fab->fab$l_fna

#define REG_EQUAL(fileinfo,reg) (memcmp(&(fileinfo)->file_id, &FILE_INFO(reg)->file_id, SIZEOF(gds_file_id)) == 0)
#define WINDOW_ALL 255
#define WRT_STRT_PNDNG (unsigned short)65534 /* the code assumes this is non-zero, even,
					and that VMS never uses its value for iosb.cond */
