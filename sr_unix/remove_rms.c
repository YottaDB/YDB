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

#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "io.h"
#include "iormdef.h"
#include "gtmio.h"

GBLREF	io_log_name	*io_root_log_name;

void remove_rms (io_desc *ciod)
{
	io_log_name	**lpp, *lp;	/* logical name pointers */
	d_rm_struct     *rm_ptr;
	int		rc, fclose_res;
	assert (ciod->type == rm);
	assert (ciod->state == dev_closed || ciod->state == dev_never_opened);
	rm_ptr = (d_rm_struct *) ciod->dev_sp;

	/* This routine will now be called if there is an open error to remove the partially created device
	 from the list of devices in io_root_log_name.  This means rm_ptr may be zero so don't use it if it is*/

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
	if (ciod->newly_created && rm_ptr && !rm_ptr->pipe)
	{
		UNLINK(ciod->trans_name->dollar_io);
	}
	for (lpp = &io_root_log_name, lp = *lpp; lp; lp = *lpp)
	{
		if ((lp->iod->pair.in == ciod) ZOS_ONLY(|| (rm_ptr && rm_ptr->fifo && (lp->iod->pair.out == ciod))))
		{
			assert (lp->iod == ciod);
#ifndef __MVS__
			assert (lp->iod->pair.out == ciod);
#else
			if (rm_ptr && rm_ptr->fifo)
			{
				if (ciod == lp->iod->pair.out)
					free(lp->iod->pair.in);
				else if (ciod == lp->iod->pair.in)
					free(lp->iod->pair.out);
			}
#endif
			/*
			 * The only device that may be "split" is the
			 * principal device.  Since that device is
			 * permanently open, it will never get here.
			 */
			*lpp = (*lpp)->next;
			free (lp);
		}
		else
			lpp = &lp->next;
	}
	if (rm_ptr)
	{
		if (rm_ptr->pipe)
		{
			int i;
			/* free up dev_param_pairs if defined */
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
	free (ciod);
}
