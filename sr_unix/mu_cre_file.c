/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <errno.h>
#include "gtm_common_defs.h"
#include "mdef.h"

#include "getzposition.h"
#include "gdsdbver.h"
#include "gdsfhead.h"
#include "gdsroot.h"
#include "gtm_fcntl.h"
#include "gtm_permissions.h"
#include "gtm_reservedDB.h"
#include "gtm_sizeof.h"
#include "gtm_threadgbl.h"
#include "gtmcrypt.h"
#include "gtmio.h"
#include "gtmmsg.h"
#include "parse_file.h"
#include "filestruct.h"
#include "is_raw_dev.h"
#include "mu_cre_file.h"
#include "jnl.h"
#include "io.h"

error_def(ERR_DBOPNERR);
error_def(ERR_FNTRANSERROR);
error_def(ERR_MUCREFILERR);
error_def(ERR_NOCRENETFILE);
error_def(ERR_PARNORMAL);
error_def(ERR_RAWDEVUNSUP);

GBLREF uint4 process_id;

unsigned char mu_cre_file(gd_region *reg)
{
	char		path[MAX_FN_LEN + 1];
	int		retcode;
	int4		save_errno;
	mstr		file;
	parse_blk	pblk = { 0 };
	int		mu_cre_file_fd = FD_INVALID;
	gd_segment	*seg;
	unix_db_info	*udi;
	uint4		gtmcrypt_errno;
	ZOS_ONLY(int	realfiletag;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((-(SIZEOF(uint4) * 2) & SIZEOF_FILE_HDR_DFLT) == SIZEOF_FILE_HDR_DFLT);
	seg = reg->dyn.addr;
	FILE_CNTL_INIT_IF_NULL(seg);
	udi = FILE_INFO(reg);
	pblk.fop = (F_SYNTAXO | F_PARNODE);
	pblk.buffer = path;
	pblk.buff_size = MAX_FN_LEN;
	file.addr = (char*)reg->dyn.addr->fname;
	file.len = reg->dyn.addr->fname_len;
	strncpy(path, file.addr, file.len);
	*(path + file.len) = '\0';
	if (is_raw_dev(path))
	{
		PUTMSG_MSG_ROUTER_CSA(NULL, reg, 4, ERR_RAWDEVUNSUP, 2, REG_LEN_STR(reg));
		return EXIT_ERR;
	}
	pblk.def1_buf = DEF_DBEXT;
	pblk.def1_size = SIZEOF(DEF_DBEXT) - 1;
	if (ERR_PARNORMAL != (retcode = parse_file(&file, &pblk)))	/* Note assignment */
	{
		PUTMSG_MSG_ROUTER_CSA(NULL, reg, 4, ERR_FNTRANSERROR, 2, REG_LEN_STR(reg));
		return EXIT_ERR;
	}
	path[pblk.b_esl] = 0;
	if (pblk.fnb & F_HAS_NODE)
	{	/* Remote node specification given */
		assert(pblk.b_node);
		PUTMSG_MSG_ROUTER_CSA(NULL, reg, 4, ERR_NOCRENETFILE, 2, LEN_AND_STR(path));
		return EXIT_WRN;
	}
	assert(!pblk.b_node);
	memcpy(seg->fname, pblk.buffer, pblk.b_esl);
	seg->fname[pblk.b_esl] = 0;
	seg->fname_len = pblk.b_esl;
	udi->fn = (char *)seg->fname;
	assert(!udi->raw);
	if (IS_ENCRYPTED(reg->dyn.addr->is_encrypted))
	{
		assert(!TO_BE_ENCRYPTED(reg->dyn.addr->is_encrypted));
		INIT_PROC_ENCRYPTION(gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			return EXIT_ERR;
		}
	}
	do
	{
		mu_cre_file_fd = OPEN3(pblk.l_dir, O_CREAT | O_EXCL | O_RDWR, 0600);
	} while ((FD_INVALID == mu_cre_file_fd) && (EINTR == errno));
	if (FD_INVALID == mu_cre_file_fd)
	{	/* Avoid error message if file already exists (another process created it) for AUTODBs.
		 */
		save_errno = errno;
		TREF(mu_cre_file_openrc) = save_errno;		/* Save for gvcst_init() */
		/* If this is an AUTODB and the file already exists, this is not an error (some other
		 * process created the file). This is ok so return as if we created it.
		 */
		if (!IS_AUTODB_REG(reg) || (EEXIST != save_errno))
			PUTMSG_MSG_ROUTER_CSA(NULL, reg, 5, ERR_DBOPNERR, 2, LEN_AND_STR(path), save_errno);
		return EXIT_ERR;
	}
	CLOSEFILE_RESET(mu_cre_file_fd, retcode); /* Close file after creation; will open again on init */
	DBGRDB((stderr, "%s:%d:%s: process id %d successfully finished mu_cre_file of file %s for region %s\n", __FILE__, __LINE__,
				__func__, process_id, reg->dyn.addr->fname, reg->rname));
	return EXIT_NRM;
}
