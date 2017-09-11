/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_string.h"

#include "io.h"
#include "gtm_limits.h"
#include "iormdef.h"
#include "gtmio.h"
#include "gtmcrypt.h"

GBLREF	io_log_name	*io_root_log_name;
GBLREF	int		process_exiting;

error_def(ERR_CRYPTKEYRELEASEFAILED);

void remove_rms (io_desc *ciod)
{
	io_log_name	**lpp, *lp;	/* logical name pointers */
	d_rm_struct     *rm_ptr;
	io_desc		*iod;
	int		i, rc, fclose_res;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	/* The routine is also called now when there's an open error and ciod->type == n_io_dev_types. */
	assert((rm == ciod->type) || (n_io_dev_types == ciod->type) || (NULL == ciod->dev_sp));
	assert(ciod->state == dev_closed || ciod->state == dev_never_opened);
	/* When we get here after an open error with n_io_dev_types == ciod->type, the ciod->dev_sp should be NULL. Assert it */
	assert((n_io_dev_types != ciod->type) || (NULL == ciod->dev_sp));
	rm_ptr = (d_rm_struct *) ciod->dev_sp;
	/* This routine will now be called if there is an open error to remove the partially created device
	 * from the list of devices in io_root_log_name.  This means rm_ptr may be zero so don't use it if it is.
	 */
	/* if this is the stderr device being closed directly by user then let the close of the pipe handle it */
	if (rm_ptr && rm_ptr->stderr_parent)
		return;
	if (rm_ptr && (0 < rm_ptr->fildes))
		CLOSEFILE_RESET(rm_ptr->fildes, rc);	/* resets "rm_ptr->fildes" to FD_INVALID */
	if (rm_ptr && (NULL != rm_ptr->filstr))
		FCLOSE(rm_ptr->filstr, fclose_res);
	if (rm_ptr && (0 < rm_ptr->read_fildes))
		CLOSEFILE_RESET(rm_ptr->read_fildes, rc);	/* resets "rm_ptr->read_fildes" to FD_INVALID */
	if (rm_ptr && (rm_ptr->read_filstr != NULL))
		FCLOSE(rm_ptr->read_filstr, fclose_res);
	if (rm_ptr && rm_ptr->input_encrypted && (GTMCRYPT_INVALID_KEY_HANDLE != rm_ptr->input_cipher_handle))
	{
		GTMCRYPT_REMOVE_CIPHER_CONTEXT(rm_ptr->input_cipher_handle, rc);
		if (0 != rc)
			GTMCRYPT_REPORT_ERROR(rc, rts_error, ciod->trans_name->len, ciod->trans_name->dollar_io);
	}
	if (rm_ptr && rm_ptr->output_encrypted && (GTMCRYPT_INVALID_KEY_HANDLE != rm_ptr->output_cipher_handle))
	{
		GTMCRYPT_REMOVE_CIPHER_CONTEXT(rm_ptr->output_cipher_handle, rc);
		if (0 != rc)
			GTMCRYPT_REPORT_ERROR(rc, rts_error, ciod->trans_name->len, ciod->trans_name->dollar_io);
	}
	if (rm_ptr && (NULL != rm_ptr->fsblock_buffer))
		free(rm_ptr->fsblock_buffer);
	if ((n_io_dev_types != ciod->type) && ciod->newly_created)
	{
		assert((NULL == rm_ptr) || !rm_ptr->is_pipe);
		UNLINK(ciod->trans_name->dollar_io);
	}
	for (lpp = &io_root_log_name, lp = *lpp; lp; lp = *lpp)
	{
		if ((NULL != lp->iod) && (n_io_dev_types == lp->iod->type))
		{
			/* remove the uninitialized device */
			*lpp = (*lpp)->next;
			free(lp);
			continue;
		}
		iod = lp->iod;
		/* Handle case where iod can be NULL (e.g. if GTM-F-MEMORY occurred during device setup & we are creating
		 * zshow dump file).
		 */
		assert((NULL != iod) || (process_exiting && TREF(jobexam_counter)));
		if ((NULL != iod) && (iod->pair.in == ciod) ZOS_ONLY(|| (rm_ptr && rm_ptr->fifo && (iod->pair.out == ciod))))
		{
			assert (iod == ciod);
#			ifndef __MVS__
			assert (iod->pair.out == ciod);
#			else
			if (rm_ptr && rm_ptr->fifo)
			{
				if (ciod == iod->pair.out)
					free(iod->pair.in);
				else if (ciod == iod->pair.in)
					free(iod->pair.out);
			}
#			endif
			/* The only device that may be "split" is the principal device, other than a PIPE device which
			 * is handled above.  Since that device is permanently open, it will never get here.
			 */
			*lpp = (*lpp)->next;
			free(lp);
			continue;
		}
		lpp = &lp->next;
	}
	if (rm_ptr)
	{
		if (rm_ptr->is_pipe)
		{	/* free up dev_param_pairs if defined */
			for ( i = 0; i < rm_ptr->dev_param_pairs.num_pairs; i++ )
			{
				if (NULL != rm_ptr->dev_param_pairs.pairs[i].name)
					free(rm_ptr->dev_param_pairs.pairs[i].name);
				if (NULL != rm_ptr->dev_param_pairs.pairs[i].definition)
					free(rm_ptr->dev_param_pairs.pairs[i].definition);
			}
		}
		free (rm_ptr);
	}
	free(ciod);
}
