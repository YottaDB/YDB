/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtm_strings.h"

#ifdef UNIX
# include "gtmio.h"
# include "io.h"
# include "io_params.h"
# include "op.h"
# include "iosp.h"
# include "gtmmsg.h"
# include "gtm_rename.h"

# define	MUR_CLOSE_FILE(file_info)				\
{									\
	mval			val, pars;				\
	unsigned char		no_param;				\
									\
	no_param = (unsigned char)iop_eol;				\
	pars.mvtype = MV_STR;						\
	pars.str.len = SIZEOF(no_param);				\
	pars.str.addr = (char *)&no_param;				\
	val.mvtype = MV_STR;						\
	val.str.len = ((unix_file_info *)file_info)->fn_len;		\
	val.str.addr = (char *) (((unix_file_info *)(file_info))->fn);	\
	if (NULL == val.str.addr)					\
		continue;						\
	op_close(&val, &pars);						\
}
#elif defined(VMS)
# define	MUR_CLOSE_FILE(file_info)				\
{									\
		assert(NULL != file_info);				\
		sys$close(((vms_file_info*)(file_info))->fab);		\
}
#endif

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;

error_def(ERR_FILENOTCREATE);

void mur_close_file_extfmt()
{
	int		recstat;
	fi_type		*file_info;
	static readonly	char 	*ext_file_type[] = {STR_JNLEXTR, STR_BRKNEXTR, STR_LOSTEXTR};

	assert(0 == GOOD_TN);
	assert(1 == BROKEN_TN);
	assert(2 == LOST_TN);
	for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
	{
		if (NULL != murgbl.file_info[recstat])
		{
			assert(mur_options.extr_fn_len[recstat]);
			MUR_CLOSE_FILE(murgbl.file_info[recstat]);
			free(murgbl.file_info[recstat]);
			murgbl.file_info[recstat] = NULL;
		}
#ifdef UNIX
		else if (mur_options.extr_fn[recstat] /* If STDOUT no file closing message. */
			   && (0 != STRNCASECMP(mur_options.extr_fn[recstat], JNL_STDO_EXTR, SIZEOF(JNL_STDO_EXTR))))
			gtm_putmsg(VARLSTCNT(6) ERR_FILENOTCREATE, 4, LEN_AND_STR(ext_file_type[recstat]),
				   mur_options.extr_fn_len[recstat], mur_options.extr_fn[recstat]);
#else
		else if (mur_options.extr_fn[recstat])
			gtm_putmsg(VARLSTCNT(6) ERR_FILENOTCREATE, 4, LEN_AND_STR(ext_file_type[recstat]),
				   mur_options.extr_fn_len[recstat], mur_options.extr_fn[recstat]);
#endif
		if (0 != mur_options.extr_fn_len[recstat])
		{
			free(mur_options.extr_fn[recstat]);
			mur_options.extr_fn[recstat] = NULL;
			mur_options.extr_fn_len[recstat] = 0;
		}
	}
}
