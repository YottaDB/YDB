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

#include "gtm_string.h"
#include <stdarg.h>

#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include <descrip.h>
#include "util.h"
#include "cmi.h"

#include "fao_parm.h"
#include "gvcmz.h"

#define PROPER(X,status) if ((status & 1)==0) { ((link_info *)(lnk->usr))->neterr = TRUE ; gvcmz_error(X,status) ;}

#define	NOFLUSH	0
#define FLUSH	1
#define RESET	2

static char outbuff[OUT_BUFF_SIZE];
static char *outptr;

void util_cm_print(clb_struct *lnk, int code, char *message, int flush, ...)
{
	va_list		var;


	int4		status;
	short		faolen;
	struct dsc$descriptor	desc;
	struct dsc$descriptor	out;

	int4		cnt, faolist[MAX_FAO_PARMS + 1];
	int 		i;

	VAR_START(var, flush);
	va_count(cnt);
	memset(faolist, 0, SIZEOF(faolist));
	for(i = 0; i < (cnt - 4); i++)        /* already 4 args */
	  {
	    faolist[i] = va_arg(var, int4);
	  }
	va_end(var);

	if (outptr==outbuff)
	{
		outbuff[0] = code ; outptr++ ;
	}
	if (message)
	{	desc.dsc$a_pointer = message;
		desc.dsc$b_dtype = DSC$K_DTYPE_T;
		desc.dsc$b_class = DSC$K_CLASS_S;
		desc.dsc$w_length = strlen(message);
		out.dsc$b_dtype = DSC$K_DTYPE_T;
		out.dsc$b_class = DSC$K_CLASS_S;
		out.dsc$a_pointer = outptr;
		out.dsc$w_length = OUT_BUFF_SIZE - (outptr - outbuff);
		faolen = 0;
		status = sys$faol(&desc,&faolen,&out,faolist);
		if (!(status & 1))
		{	lib$signal(status);
		}
		outptr += faolen;
	}
	switch (flush)
	{
	case NOFLUSH:	break;
	case FLUSH  :	*outptr++ = 0 ; lnk->mbf = outbuff ; lnk->cbl = outptr - outbuff ; lnk->ast = 0 ;
			status = cmi_write(lnk) ;
			PROPER (code,status) ;
			outptr = outbuff ;
			break;
	case RESET  :	outptr = outbuff ;
			break;
	default	    :	break ;
	}
	return;
}
