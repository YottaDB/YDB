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
#ifdef VMS
#include <climsgdef.h>
#include <fab.h>
#include <rms.h>
#include <errno.h>
#include <nam.h>
#include <psldef.h>
#include <rmsdef.h>
#include <descrip.h>
#endif

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "jnl.h"

#define DOT		'.'
#define UNDERSCORE	'_'
#ifdef VMS
GBLREF	gd_region		*gv_cur_region;
#endif
GBLREF	sgmnt_data_ptr_t	cs_data;

void	mupip_set_journal_fname(jnl_create_info *jnl_info)
{
	char			ext_name[MAX_FN_LEN], *ctop;
	sgmnt_data_ptr_t	csd;
	int			ext_name_len = 0;
	char			*ptr1, *ptr2;
	UNIX_ONLY(boolean_t	first_time = TRUE;)
	VMS_ONLY(GDS_INFO	*gds_info;)

	csd = cs_data;
	if (0 == csd->jnl_file_len)
	{
#if defined(UNIX)
		memcpy(jnl_info->jnl, jnl_info->fn, jnl_info->fn_len);
		jnl_info->jnl[jnl_info->fn_len] = 0;
		for (ptr1 = (char *)jnl_info->jnl, ptr2 = ctop = ptr1 + jnl_info->fn_len; (ptr1 < ctop) && ('/' != *ctop); --ctop)
		{
			if (first_time)
			{
				if (DOT == *ctop)
				{
					first_time = FALSE;
					ext_name_len = (int)(ptr2 - ctop - 1);
					assert(0 <= ext_name_len);
					memcpy(ext_name, ctop + 1, ext_name_len);
					ptr2 = ctop;
				}
			} else if (DOT == *ctop)
				*ctop = UNDERSCORE;
		}
		jnl_info->jnl_len = ptr2 - ptr1;
#elif defined (VMS)
		gds_info = FILE_INFO(gv_cur_region);
		jnl_info->jnl_len = gds_info->nam->nam$b_esl - gds_info->nam->nam$b_type;
		memcpy(jnl_info->jnl, gds_info->nam->nam$l_esa, jnl_info->jnl_len);
		ext_name_len = gds_info->nam->nam$b_type - 1;
		memcpy(ext_name, gds_info->nam->nam$l_esa + jnl_info->jnl_len + 1, ext_name_len);
#endif
		if (0 == ext_name_len) /* file name ended with DOT */
			jnl_info->jnl[jnl_info->jnl_len++] = UNDERSCORE;
		else if (0 != memcmp(ext_name, DEF_DB_EXT_NAME, ext_name_len))
		{
			jnl_info->jnl[jnl_info->jnl_len++] = UNDERSCORE;
			memcpy(jnl_info->jnl + jnl_info->jnl_len, ext_name, ext_name_len);
			jnl_info->jnl_len += ext_name_len;
		}
		memcpy(jnl_info->jnl + jnl_info->jnl_len, DEF_JNL_EXT_NAME, SIZEOF(DEF_JNL_EXT_NAME) - 1);
		jnl_info->jnl_len = jnl_info->jnl_len + SIZEOF(DEF_JNL_EXT_NAME) - 1;
	} else
	{
		memcpy(jnl_info->jnl, csd->jnl_file_name, csd->jnl_file_len);
		jnl_info->jnl_len = csd->jnl_file_len;
	}
	jnl_info->jnl[jnl_info->jnl_len] = '\0';
}
