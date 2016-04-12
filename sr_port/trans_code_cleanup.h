/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __TRANS_CODE_CLEANUP_H__
#define __TRANS_CODE_CLEANUP_H__

void trans_code_cleanup(void);

error_def(ERR_ERRWETRAP);
error_def(ERR_ERRWEXC);
error_def(ERR_ERRWIOEXC);
error_def(ERR_ERRWZBRK);
error_def(ERR_ERRWZINTR);
error_def(ERR_ERRWZTRAP);

/* Note assertpro() checks have extra text in them to identify which assertpro tripped */
#define SET_ERR_CODE(fp, errmsg)					\
{									\
	switch (fp->type)						\
	{								\
		case SFT_ZBRK_ACT:					\
			errmsg = ERR_ERRWZBRK;				\
			break;						\
		case SFT_DEV_ACT:					\
			errmsg = ERR_ERRWIOEXC;				\
			break;						\
		case SFT_ZTRAP:						\
			if (err_act == &dollar_etrap.str)		\
				errmsg = ERR_ERRWETRAP;			\
			else if (err_act == &dollar_ztrap.str)		\
				errmsg = ERR_ERRWZTRAP;			\
			else						\
				assertpro(FALSE && err_act);		\
			break;						\
		case SFT_ZSTEP_ACT:					\
			errmsg = ERR_ERRWEXC;				\
			break;						\
		case (SFT_ZINTR | SFT_COUNT):				\
			errmsg = ERR_ERRWZINTR;				\
			break;						\
		default:						\
			assertpro(FALSE && fp->type);			\
	}								\
}

#endif
