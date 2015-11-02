/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

error_def(ERR_ERRWZTRAP);
error_def(ERR_ERRWETRAP);
error_def(ERR_ERRWZBRK);
error_def(ERR_ERRWIOEXC);
error_def(ERR_ERRWEXC);
error_def(ERR_ERRWZINTR);

#define SET_ERR_CODE(fp, err)							\
{										\
	switch (fp->type)							\
	{									\
	case SFT_ZBRK_ACT:							\
		err = (int)ERR_ERRWZBRK;					\
		break;								\
	case SFT_DEV_ACT:							\
		err = (int)ERR_ERRWIOEXC;					\
		break;								\
	case SFT_ZTRAP:								\
		if (err_act == &dollar_etrap.str)				\
			err = (int)ERR_ERRWETRAP;				\
		else if (err_act == &dollar_ztrap.str)				\
			err = (int)ERR_ERRWZTRAP;				\
		else								\
			GTMASSERT;						\
		break;								\
	case SFT_ZSTEP_ACT:							\
		err = (int)ERR_ERRWEXC;					\
		break;								\
	case (SFT_ZINTR | SFT_COUNT):						\
		err = (int)ERR_ERRWZINTR;					\
		break;								\
	default:								\
		GTMASSERT;							\
	}									\
}

#endif
