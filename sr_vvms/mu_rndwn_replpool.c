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

#include <ssdef.h>
#include <prtdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h" /* Required for gtmsource.h */
#include <stddef.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "repl_sem.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "gtm_logicals.h"
#include "jnl.h"
#include "repl_shm.h"
#include "mu_rndwn_replpool.h"
#include "gtmmsg.h"

LITREF char	gtm_release_name[];
LITREF int4	gtm_release_name_len;

error_def(ERR_VERMISMATCH);
error_def(ERR_MUREPLPOOL);
error_def(ERR_TEXT);

#define MU_RNDWN_REPLPOOL_RETURN(RETVAL)	\
{						\
	detach_shm(shm_range);			\
	signoff_from_gsec(shm_lockid);		\
	return RETVAL;				\
}

/* runsdown the shared segment identified by replpool_id */

boolean_t mu_rndwn_replpool(replpool_identifier *replpool_id, boolean_t rndwn_all, boolean_t *segment_found)
{
	int			which_pool;
	int4		        status;
	int4                    shm_lockid;
	sm_uc_ptr_t		shm_range[2];
	replpool_id_ptr_t	rp_id_ptr;
	struct dsc$descriptor_s name_dsc;

	/* name_dsc holds the resource name */
	*segment_found = FALSE;
	name_dsc.dsc$a_pointer = replpool_id->repl_pool_key;
        name_dsc.dsc$w_length = strlen(replpool_id->repl_pool_key);
        name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
        name_dsc.dsc$b_class = DSC$K_CLASS_S;
	name_dsc.dsc$a_pointer[name_dsc.dsc$w_length] = '\0';

	assert(JNLPOOL_SEGMENT == replpool_id->pool_type || RECVPOOL_SEGMENT == replpool_id->pool_type);
	which_pool = (JNLPOOL_SEGMENT == replpool_id->pool_type)? SOURCE : RECV;

	if (!shm_exists(which_pool, &name_dsc))
		return TRUE;
	*segment_found = TRUE;
	if (SS$_NORMAL != (status = register_with_gsec(&name_dsc, &shm_lockid)))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Failed to register with replpool"), status);
		return FALSE;
	}
	status = map_shm(which_pool, &name_dsc, shm_range);
	if (SS$_NORMAL != status)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Failed to map replpool segment"), status);
		signoff_from_gsec(shm_lockid);
		return FALSE;
	}
	/* assert that the replpool identifier is at the top of replpool control structure */
	assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
	assert(0 == offsetof(recvpool_ctl_struct, recvpool_id));

	rp_id_ptr = (replpool_identifier *)shm_range[0];
	if (memcmp(rp_id_ptr->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 1))
	{
		if (!memcmp(rp_id_ptr->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 3))
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Incorrect version for the replpool segment."));
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Incorrect replpool format for the segment."));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (memcmp(rp_id_ptr->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
			ERR_VERMISMATCH, 6, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
			gtm_release_name_len, gtm_release_name, LEN_AND_STR(rp_id_ptr->now_running));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (rndwn_all)
		memcpy(replpool_id->gtmgbldir, rp_id_ptr->gtmgbldir, MAX_FN_LEN + 1);
	else if	(strcmp(replpool_id->gtmgbldir, rp_id_ptr->gtmgbldir))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Global directory name does not match that in the replpool segment."));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (SS$_NORMAL != (status = lastuser_of_gsec(shm_lockid)))
	{
		if (SS$_NOTQUEUED == status)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Replpool segment is in use by another process."));
		} else
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Failed to get last_user status for replpool segment."), status);
		}
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (SS$_NORMAL != (status = delete_shm(&name_dsc)))
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_MUREPLPOOL, 2, name_dsc.dsc$w_length, name_dsc.dsc$a_pointer,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Failed to delete replpool segment."), status);
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	return TRUE;
}
