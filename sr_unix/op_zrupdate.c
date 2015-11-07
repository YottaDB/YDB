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

#include "gtmio.h"
#include "io.h"
#include "iosp.h"
#include <rtnhdr.h>
#include "relinkctl.h"
#include "parse_file.h"
#include "eintr_wrappers.h"
#include "error.h"
#include "min_max.h"
#include "op.h"
#include "op_fnzsearch.h"
#include "interlock.h"
#include "toktyp.h"
#include "valid_mname.h"
#ifdef DEBUG
# include "toktyp.h"		/* Needed for "valid_mname.h" */
#endif

#define DOTOBJEXT	".o"
#define OBJEXT 		'o'
#define WILDCARD	'*'

LITREF	mval	literal_null;

error_def(ERR_FILEPARSE);
error_def(ERR_PARNORMAL);
error_def(ERR_TEXT);

#ifndef AUTORELINK_SUPPORTED
/* Stub routine for unsupported platforms */
void op_zrupdate(int argcnt, ...)
{
	return;
}
#else
/* The ZRUPDATE command drives this routine once through for each argument (object file path and object file - potentially
 * containing wildcards). Each file specified, or found in a wildcard search, is separated into its path and its routine
 * name, the path is looked up and the appropriate relinkctl file opened where we lookup the routine name and bump its cycle.
 *
 * Although this routine is setup to handle a variable argument list, more than 1 argument is not currently supported. The
 * ZRUPDATE command itself does support a commented list of filespecs but the compiler turns each argument into a separate
 * call to this routine. The purpose of the variable argument list is to support a future proposed enhancement which would
 * allow a ZRUPDATE argument to be a parenthesized list of filespecs with the intention all of them are simultaneously
 * updated. Such a list, when supported would be passed as a list of files to this routine - hence the multi-arg support.
 *
 * To get a consistent view of each directory, the directory is run through realpath() to resolve soft links and normalize the
 * directory name consistently.
 *
 * Parameters:
 *   argcnt        - currently always 1 (see note above).
 *   objfilespec   - mval address holding string containing filespec to process.
 *
 * No return value.
 */

void op_zrupdate(int argcnt, ...)
{
	mval			*objfilespec;
	va_list			var;
	mval			objpath;
	char			tranbuf[MAX_FBUFF + 1], *chptr, chr;
	open_relinkctl_sgm 	*linkctl;
	relinkrec_t		*rec;
	plength			plen;
	int			status, fextlen, fnamlen, fcnt;
	parse_blk		pblk;
	struct stat		outbuf;
        int			stat_res;
	boolean_t		seenfext, fileexists;
	mstr			objdir, rtnname;
	uint4			hash, prev_hash_index;

	/* Currently only expecting one value per invocation right now. That will change in phase 2 hence the stdarg setup */
	va_start(var, argcnt);
	assert(1 == argcnt);
	objfilespec = va_arg(var, mval *);
	va_end(var);
	MV_FORCE_STR(objfilespec);
	/* First some pre-processing to determine if an explicit file name or type was specified. If so, it must be ".o" for
	 * this phase of implementation. Later phases may allow ".m" to be specified but not initially. Use parse_file() to
	 * parse everything out and isolate any extention.
	 */
	memset(&pblk, 0, SIZEOF(pblk));
        pblk.buffer = tranbuf;
	pblk.buff_size = (unsigned char)(MAX_FBUFF);	/* Pass size of buffer - 1 (standard protocol for parse_file) */
	pblk.def1_buf = DOTOBJEXT;			/* Default .o file type if not specified */
	pblk.def1_size = SIZEOF(DOTOBJEXT) - 1;
	pblk.fop = F_SYNTAXO;				/* Syntax check only - bypass directory existence check */
	status = parse_file(&objfilespec->str, &pblk);
	if (ERR_PARNORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, objfilespec->str.len, objfilespec->str.addr, status);
	tranbuf[pblk.b_esl] = '\0';			/* Needed for subsequent STAT_FILE */
	seenfext = FALSE;
	if (0 != pblk.b_ext)
	{	/* If a file extension was specified - get the extension sans any potential wildcard character */
		for (chptr = pblk.l_ext + 1, fextlen = pblk.b_ext - 1; 0 < fextlen; chptr++, fextlen--)
		{	/* Check each character in the extension except first which is the dot if ext exists at all */
			if (WILDCARD != *chptr)
			{	/* We see a char that isn't a wildcard character. If we've already seen our "o" file extension,
				 * this char makes our requirement filetype impossible so raise an error.
				 */
				if (seenfext || (OBJEXT != *chptr))
					/* No return from this error */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_FILEPARSE,
						      2, objfilespec->str.len, objfilespec->str.addr,
						      ERR_TEXT, 2, RTS_ERROR_TEXT("Unsupported filetype specified"));
				seenfext = TRUE;
			}
		}
	}
	/* Do a simlar check for the file type */
	if (0 != pblk.b_name)
	{	/* A file name was specified (if not, tiz probably hard to find the file but that can be dealt with later).
		 * Like in the above, the name must be comprised of valid chars for routine names.
		 */
		for (chptr = pblk.l_name, fnamlen = pblk.b_name; 0 < fnamlen; chptr++, fnamlen--)
		{
			if (WILDCARD != *chptr)
			{	/* Substitute '%' for '_'. While this substitution is really only valid on the first char, only the
				 * first char check allows "%" so a 2nd or later char check would fail the '%' substitution anyway.
				 */
				chr = ('_' == *chptr) ? '%' : *chptr;
				/* We see a char that isn't a wildcard character. If this is the first character, it can be
				 * alpha or percent. If the second or later character, it can be alphanumeric.
				 */
				if (((fnamlen != pblk.b_name) && !VALID_MNAME_NFCHAR(chr))	/* 2nd char or later check */
				    || ((fnamlen == pblk.b_name) && !VALID_MNAME_FCHAR(chr)))	/* 1st char check */
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_FILEPARSE,
						      2, objfilespec->str.len, objfilespec->str.addr,
						      ERR_TEXT, 2, RTS_ERROR_TEXT("Filename is not a valid routine name"));
				}
			}
		}
	}
	/* When specifying a non-wildcarded object file, it is possible for the file to have been removed, in which case we still
	 * need to update its relinkctl entry (if exists) to notify other processes about the object's deletion.
	 */
	if (!(pblk.fnb & F_WILD) & seenfext)
	{	/* If no wildcards in path and saw the extension we want - no need to wash through zsearch() */
		objdir.addr = pblk.l_dir;
		objdir.len = pblk.b_dir;
		linkctl = relinkctl_attach(&objdir);		/* Create/attach/open relinkctl file */
		if (NULL == linkctl)				/* Non-existant path and no associated relinkctl file */
			/* Note this reference to errno depends on nothing in relinkctl_attach() doing anything to modify
			 * errno after the realpath() call. No return from this error.
			 */
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, objfilespec->str.len, objfilespec->str.addr,
				      errno);
		/* What we do at this point depends on the following conditions:
		 *
		 * 1) If the specified file exists, we can add it to relinkctl file and/or update its cycle.
		 * 2) If the file doesn't exist on disk but the routine is found in the relinkctl file, update cycle.
		 * 3) If no file and no entry, just ignore it and do nothing (info error removed by request).
		 */
		STAT_FILE(tranbuf, &outbuf, stat_res);
		if (-1 == stat_res)
		{
			if (ENOENT != errno)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, objfilespec->str.len,
					      objfilespec->str.addr, errno);
			fileexists = FALSE;
		} else
			fileexists = TRUE;
		rtnname.len = pblk.b_name;
		rtnname.addr = pblk.l_name;
		CONVERT_FILENAME_TO_RTNNAME(rtnname);	/* Set rtnname right before searching in relinkctl file */
		assert(valid_mname(&rtnname));
		COMPUTE_RELINKCTL_HASH(&rtnname, hash);
		rec = relinkctl_find_record(linkctl, &rtnname, hash, &prev_hash_index);
		if ((NULL == rec) && !fileexists)
			return;					/* No file and no entry - ignore */
		/* Either the file exists or the entry exists so add or update it */
		rec = relinkctl_insert_record(linkctl, &rtnname);
		RELINKCTL_CYCLE_INCR(rec, linkctl); 		/* Increment cycle indicating change to world */
		return;
	}
	/* If we have a wildcarded request or one without the object filetype, reprocess the string with $ZSEARCH using our
	 * defined stream to resolve wildcards. Then loop through processing each file returned. In this loop, we just ignore
	 * any file that doesn't have a ".o" extension.
	 */
	op_fnzsearch((mval *)&literal_null, STRM_ZRUPDATE, 0, &objpath);	/* Clear any existing cache */
	for (fcnt = 0; ; fcnt++)
	{
		plen.p.pint = op_fnzsearch(objfilespec, STRM_ZRUPDATE, 0, &objpath);
		if (0 == objpath.str.len)
		{	/* End of file list */
			if (0 == fcnt)
				/* Still looking to process our first file - give no objects found message as a "soft" message
				 * (INFO level message - supressed in other than direct mode)
				 */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) MAKE_MSG_INFO(ERR_FILEPARSE), 2, objfilespec->str.len,
					      objfilespec->str.addr, ERR_TEXT, 2, RTS_ERROR_TEXT("No object files found"));
			break;
		}
		/* The extension contains the extension-start character ('.') so we are looking for the extension '.o' hence
		 * the length must be 2 and the 2nd char must be OBJEXT.
		 */
		if ((2 == plen.p.pblk.b_ext) && (OBJEXT == *(objpath.str.addr + plen.p.pblk.b_dir + plen.p.pblk.b_name + 1)))
		{			/* This is (probably) an object file. Double check file is a file and not a directory */
			memcpy(tranbuf, objpath.str.addr, objpath.str.len);	/* Need null terminated version for STAT */
			tranbuf[objpath.str.len] = '\0';		/* Not guaranteed null termination from op_fnzsearch */
			STAT_FILE(tranbuf, &outbuf, stat_res);
			/* If either something happened to the file since op_fnzsearch() saw it or the file is not a file, then
			 * ignore it.
			 */
			if ((-1 == stat_res) || !S_ISREG(outbuf.st_mode))
			{
				fcnt--;					/* Don't count files not found for whatever reason */
				continue;
			}
			/* Before opening the relinkctl file, verify we actually do have a valid routine name */
			rtnname.len = plen.p.pblk.b_name;
			rtnname.addr = objpath.str.addr + plen.p.pblk.b_dir;
			CONVERT_FILENAME_TO_RTNNAME(rtnname);		/* Set rtnname right before searching in relinkctl file */
			if (!valid_mname(&rtnname))
			{
				fcnt--;
				continue;				/* Ignore files that are invalid wildcard matches */
			}
			objdir.addr = objpath.str.addr;
			objdir.len = plen.p.pblk.b_dir;
			linkctl = relinkctl_attach(&objdir);		/* Create/attach/open relinkctl file */
			if (NULL == linkctl)
			{
				fcnt--;					/* Path disappeared - don't count it */
				continue;
			}
			rec = relinkctl_insert_record(linkctl, &rtnname);
			RELINKCTL_CYCLE_INCR(rec, linkctl); 		/* Increment cycle indicating change to world */
		} else
			fcnt--;						/* Don't count ignored files */

	}
}
#endif /* AUTORELINK_SUPPORTED */
