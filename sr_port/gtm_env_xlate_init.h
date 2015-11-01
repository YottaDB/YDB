/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_ENV_XLATE_INIT_H
#define GTM_ENV_XLATE_INIT_H

void gtm_env_xlate_init(void);

GBLREF mstr		env_gtm_env_xlate;
GBLREF int		(*gtm_env_xlate_entry)();
GBLREF mval		dollar_zdir;

#define GTM_ENV_XLATE_ROUTINE_NAME "gtm_env_xlate"

error_def(ERR_XTRNTRANSERR);
error_def(ERR_XTRNTRANSDLL);
error_def(ERR_XTRNRETVAL);
error_def(ERR_XTRNRETSTR);
error_def(ERR_TEXT);

/* UNIX_ONLY (Any file that uses GTM_ENV_TRANSLATE macro should also include <limits.h>
 * which defines PATH_MAX macro) */

#define GTM_ENV_TRANSLATE(VAL1, VAL2) {									\
	if (0 != env_gtm_env_xlate.len)									\
	{												\
		MSTR_CONST(routine_name, GTM_ENV_XLATE_ROUTINE_NAME);					\
		int             ret_gtm_env_xlate;							\
		UNIX_ONLY(										\
			char		pakname[PATH_MAX + 1];						\
			void_ptr_t	pakhandle;							\
		)											\
		VMS_ONLY(										\
			int4                    status;							\
			struct dsc$descriptor   filename;						\
			struct dsc$descriptor   entry_point;						\
		)											\
		MV_FORCE_STR(VAL2);									\
		if (NULL == gtm_env_xlate_entry) 							\
		{											\
		UNIX_ONLY(										\
			memcpy(pakname, env_gtm_env_xlate.addr, env_gtm_env_xlate.len);			\
			pakname[env_gtm_env_xlate.len]='\0';						\
			pakhandle = fgn_getpak(pakname, ERROR);						\
			gtm_env_xlate_entry = (int (*)())fgn_getrtn(pakhandle, &routine_name, ERROR);	\
		)											\
		VMS_ONLY(										\
			entry_point.dsc$b_dtype = DSC$K_DTYPE_T;					\
			entry_point.dsc$b_class = DSC$K_CLASS_S;					\
			entry_point.dsc$w_length = routine_name.len;					\
			entry_point.dsc$a_pointer = routine_name.addr;					\
													\
			filename.dsc$b_dtype = DSC$K_DTYPE_T;						\
			filename.dsc$b_class = DSC$K_CLASS_S;						\
			filename.dsc$w_length = env_gtm_env_xlate.len;					\
			filename.dsc$a_pointer = env_gtm_env_xlate.addr;				\
													\
			ESTABLISH(gtm_env_xlate_ch);							\
			status = lib$find_image_symbol(&filename,&entry_point,&gtm_env_xlate_entry, 0);	\
			REVERT;										\
			if (0 == (status & 1))								\
				rts_error(VARLSTCNT(1) ERR_XTRNTRANSDLL);				\
		)												\
		}												\
		val_xlated.str.addr = NULL;									\
		ret_gtm_env_xlate = (*gtm_env_xlate_entry)(&(VAL1)->str, &(VAL2)->str, &(dollar_zdir.str), &(val_xlated.str));	\
		if (MAX_DBSTRLEN < val_xlated.str.len)							\
			rts_error(VARLSTCNT(4) ERR_XTRNRETVAL, 2, val_xlated.str.len, MAX_DBSTRLEN);				\
		if (0 != ret_gtm_env_xlate)									\
		{												\
			if ((val_xlated.str.len) && (val_xlated.str.addr))					\
				rts_error(VARLSTCNT(6) ERR_XTRNTRANSERR, 0, ERR_TEXT,  2, val_xlated.str.len, 	\
					val_xlated.str.addr);							\
			else											\
				rts_error(VARLSTCNT(1) ERR_XTRNTRANSERR);					\
		}												\
		if ((NULL == val_xlated.str.addr) && (0 != val_xlated.str.len))					\
			rts_error(VARLSTCNT(1)ERR_XTRNRETSTR);							\
		val_xlated.mvtype = MV_STR;									\
		(VAL1) = &val_xlated;										\
	}													\
}

#endif	/*GTM_ENV_XLATE_INIT_H*/
