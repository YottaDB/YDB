/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_ENV_XLATE_INIT_H
#define GTM_ENV_XLATE_INIT_H

#define GTM_ENV_XLATE_ROUTINE_NAME "gtm_env_xlate"

void 	gtm_env_xlate_init(void);
mval* 	gtm_env_translate(mval* val1, mval* val2, mval* val_xlated);

#endif	/* GTM_ENV_XLATE_INIT_H */
