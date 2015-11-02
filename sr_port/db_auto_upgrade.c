/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "io.h"
#include "jnl.h"
#include "mutex.h"
#include "gvcst_protos.h"	/* for gvcst_init_sysops prototype */

void db_auto_upgrade(gd_region *reg)
{
	/* detect unitialized file header fields for this version of GT.M and do a mini auto-upgrade, initializing such fields
	 * to default values in the new GT.M version
	 */

	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;

	assert(NULL != reg);
	if (NULL == reg)
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	if (NULL == csd)
		return;

	if (0 == csd->mutex_spin_parms.mutex_hard_spin_count)
		csd->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	if (0 == csd->mutex_spin_parms.mutex_sleep_spin_count)
		csd->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	/* zero is a legitimate value for csd->mutex_spin_parms.mutex_spin_sleep_mask; so can't detect if need re-initialization */

	/* Auto upgrade based on minor database version number. This code currently only does auto upgrade and does not
	   do auto downgrade although that certainly is possible to implement if necessary. For now, if the current version
	   is at a lower level than the minor db version, we do nothing.

	   Note the purpose of the minor_dbver field is so that some part of gtm (either runtime, or conversion utility) some
	   time and several versions down the road from now knows by looking at this field what fields in the fileheader are
	   valid so it is important that the minor db version be updated each time the fileheader is updated and this routine
	   correspondingly updated. SE 5/2006.
	*/
	if (csd->minor_dbver < GDSMVCURR)
	{	/* In general, the method for adding new versions is:
		   1) If the top case is for the most previous version (and has a break in it), the break is no longer appropriate
		      since at minimum, the minor_dbver value needs updating.
		   2) Update (or add) a case for the previous version to update any necessary fields.
		*/
		switch(csd->minor_dbver)
		{
			case GDSMV51000:		/* Multi-site replication available */
			case GDSMV52000:		/* Unicode */
			case GDSMV53000:		/* M-Itanium release.*/
				break;			/* Nothing to do for this version */
			default:
				csd->minor_dbver = GDSMVCURR;
		}
	}
	return;
}
