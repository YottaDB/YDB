/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <descrip.h>

#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "iosp.h"
#include "mlkdef.h"
#include "lke.h"

/*
 * -----------------------------------------------
 * Read terminal input, displaying a prompt string
 *
 * Return:
 *	TRUE - the answer was 'Y'
 *	FALSE - answer is 'N'
 * -----------------------------------------------
 */
bool lke_get_answ(char *prompt)
{
	char		res[8] ;
	$DESCRIPTOR	(dres,res) ;
	short unsigned int len;
	struct	dsc$descriptor_s dprm;

	dprm.dsc$b_dtype = DSC$K_DTYPE_T;
	dprm.dsc$b_class = DSC$K_CLASS_S;
	dprm.dsc$a_pointer = prompt;
	for (dprm.dsc$w_length = 0 ; *prompt++ ; dprm.dsc$w_length++)
		;
	lib$get_input(&dres,&dprm,&len);
	if (len < 1)
		return FALSE;
	else
		return ((res[0]=='y' || res[0]=='Y'));
}

