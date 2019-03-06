/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef TRANS_CODE_CLEANUP_H_INCLUDED
#define TRANS_CODE_CLEANUP_H_INCLUDED

void trans_code_cleanup(void);

error_def(ERR_ERRWETRAP);
error_def(ERR_ERRWEXC);
error_def(ERR_ERRWIOEXC);
error_def(ERR_ERRWZBRK);
error_def(ERR_ERRWZINTR);
error_def(ERR_ERRWZTRAP);
error_def(ERR_ERRWZTIMEOUT);

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
			if (&((TREF(dollar_etrap)).str) == err_act)	\
				errmsg = ERR_ERRWETRAP;			\
			else if (&(TREF(dollar_ztrap)).str == err_act)	\
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
		case (SFT_ZTIMEOUT | SFT_COUNT):			\
			errmsg = ERR_ERRWZTIMEOUT;			\
		default:						\
			assertpro(FALSE && fp->type);			\
	}								\
}

#endif
