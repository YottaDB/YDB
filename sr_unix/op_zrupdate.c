/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <stdarg.h>
#include "gtm_limits.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdlib.h"

#include "io.h"
#include "iosp.h"
#include <rtnhdr.h>
#include "relinkctl.h"
#include "zhist.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "min_max.h"
#include "op.h"
#include "op_fnzsearch.h"

#define OBJEXT 		'o'
#define WILDCARD	'*'


#ifndef USHBIN_SUPPORTED
/* Stub routine for unsupported platforms */
void op_zrupdate(int argcnt, ...)
{
	return;
}
#else
/************************************************************/

LITREF	mval	literal_null;

error_def(ERR_FILEPARSE);
error_def(ERR_PARNORMAL);
error_def(ERR_TEXT);

/*
 * TODO: Add description of syntax/operation
 */

void op_zrupdate(int argcnt, ...)
{
	mval			*objfilespec;
	va_list			var;
	mval			objpath;
	char			tranbuf[MAX_FBUFF + 1], *chptr;
	open_relinkctl_sgm 	*linkctl;
	relinkrec_loc_t		rec;
	plength			plen;
	int			status, fextlen;
	parse_blk		pblk;
	struct stat		outbuf;
        int			stat_res;
	boolean_t		seenfext, seenwildcard;
	mstr			objdir, rtnname;

	/* Currently only expecting one value per invocation right now. That will change in phase 2 hence the stdarg setup */
	va_start(var, argcnt);
	assert(1 == argcnt);
	objfilespec = va_arg(var, mval *);
	va_end(var);
	MV_FORCE_STR(objfilespec);
	/* First some pre-processing to determine if an explicit file type was specified. If so, it must be ".o" for this
	 * phase of implementation. Later phases may allow ".m" to be specified but not initially. Use parse_file() to
	 * parse everything out and isolate any extention.
	 */
	memset(&pblk, 0, SIZEOF(pblk));
        pblk.buffer = tranbuf;
	pblk.buff_size = (unsigned char)(MAX_FBUFF);
	pblk.fnb = 0;
	status = parse_file(&objfilespec->str, &pblk);
	if (ERR_PARNORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, objfilespec->str.len, objfilespec->str.addr, status);
	if (0 != pblk.b_ext)
	{	/* If a file extension was specified - get the extension sans any potential wildcard character */
		seenfext = seenwildcard = FALSE;
		for (chptr = pblk.l_ext + 1, fextlen = pblk.b_ext - 1; 0 < fextlen; chptr++, fextlen--)
		{	/* Check each character in the extension except first which is the dot if ext exists at all */
			if (WILDCARD != *chptr)
			{	/* We see a char that isn't a wildcard character. If we've already seen our "o" file extension,
				 * this char makes our requirement filetype impossible so raise an error.
				 * TODO - more appropriate error
				 */
				if (seenfext || (OBJEXT != *chptr))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FILEPARSE, 2, objfilespec->str.len,
						      objfilespec->str.addr);
				seenfext = TRUE;	/* No return from rts_error() above so this is our object file type */
			} else
				seenwildcard = TRUE;
		}
	}
	/* When specifying a non-wildcarded object file, it is possible for the file to have been removed, in which case we still
	 * need to update its relinkctl entry (if exists) to notify other processes about the object's deletion.
	 * TODO: should this see if given object exists in relinkctl file? If not, should it error or just ignore?
	 */
	if (!seenwildcard & seenfext)
	{
		objdir.addr = pblk.l_dir;
		objdir.len = pblk.b_dir;
		linkctl = relinkctl_attach(&objdir);		/* Create/attach/open relinkctl file */
		if (NULL == linkctl)
			return;					/* Path doesn't exist - ignore */
		rtnname.len = MIN(MAX_MIDENT_LEN, pblk.b_name); /* Avoid overflow */
		rtnname.addr = pblk.l_name;
		rec = relinkctl_insert_record(linkctl, &rtnname);
		RELINKCTL_CYCLE_INCR(linkctl, rec); 		/* Increment cycle indicating change to world */
		return;
	}
	/* If we have a wildcarded request or one without the object filetype, reprocess the string with $ZSEARCH using our
	 * defined stream to resolve wildcards. Then loop through processing each file returned. In this loop, we just ignore
	 * any file that doesn't have a ".o" extension.
	 */
	op_fnzsearch((mval *)&literal_null, STRM_ZRUPDATE, 0, &objpath);	/* Clear any existing cache */
	while(TRUE)
	{
		plen.p.pint = op_fnzsearch(objfilespec, STRM_ZRUPDATE, 0, &objpath);
		if (0 == objpath.str.len)
			/* End of file list */
			break;
		/* The extension contains the extension-start character ('.') so we are looking for the extension '.o' hence
		 * the length must be 2 and the 2nd char must be OBJEXT.
		 */
		if ((2 == plen.p.pblk.b_ext) && (OBJEXT == *(objpath.str.addr + plen.p.pblk.b_dir + plen.p.pblk.b_name + 1)))
		{	/* This is (probably) an object file. Double check file is a file and not a directory */
			memcpy(tranbuf, objpath.str.addr, objpath.str.len);	/* Need null terminated version for STAT */
			tranbuf[objpath.str.len] = '\0';
			STAT_FILE(tranbuf, &outbuf, stat_res);
			/* If either something happened to the file since op_fnzsearch() saw it or the file is not a file, then
			 * ignore it.
			 */
			if ((-1 == stat_res) || !S_ISREG(outbuf.st_mode))
				continue;
			objdir.addr = objpath.str.addr;
			objdir.len = plen.p.pblk.b_dir;
			linkctl = relinkctl_attach(&objdir);		/* Create/attach/open relinkctl file */
			if (NULL == linkctl)
				continue;				/* Path disappeared - ignore */
			rtnname.len = MIN(MAX_MIDENT_LEN, plen.p.pblk.b_name); 	/* Avoid overflow */
			rtnname.addr = objpath.str.addr + plen.p.pblk.b_dir;
			rec = relinkctl_insert_record(linkctl, &rtnname);
			RELINKCTL_CYCLE_INCR(linkctl, rec); 		/* Increment cycle indicating change to world */
		}
	}
}
#endif /* USHBIN_SUPPORTED */
