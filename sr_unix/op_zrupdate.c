/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#define ASTERISK	'*'
#define QUESTION	'?'

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
 * containing wildcards). Each file specified, or found in a wildcard search, is separated into its path and its routine name;
 * the path is then looked up and the appropriate relinkctl file opened, where we find the routine name and bump its cycle.
 *
 * Although this routine is set up to handle a variable argument list, more than 1 argument is not currently supported. The
 * ZRUPDATE command itself does support a commented list of filespecs, but the compiler turns each argument into a separate
 * call to this routine. The purpose of the variable argument list is to support a future proposed enhancement, which would
 * allow a ZRUPDATE argument to be a parenthesized list of filespecs with the intention that all of them be simultaneously
 * updated. Such a list, when supported, would be passed as a list of files to this routine - hence the multi-arg support.
 *
 * Parameters:
 *   argcnt        - currently always 1 (see note above).
 *   objfilespec   - mval address holding string containing filespec to process.
 *
 * No return value.
 */
void op_zrupdate(int argcnt, ...)
{
	boolean_t		wildcarded, noresult, seenfext, invalid;
	char			pblkbuf[MAX_FBUFF + 1], statbuf[MAX_FBUFF + 1], namebuf[MAX_FBUFF + 1];
	char			*chptr, chr;
	int			status, fextlen, fnamlen, object_count;
	mstr			objdir, rtnname;
	mval			*objfilespec, objpath;
	open_relinkctl_sgm 	*linkctl;
	parse_blk		pblk;
	plength			plen;
	relinkrec_t		*rec;
	struct stat		outbuf;
	uint4			hash, prev_hash_index;
	va_list			var;

	/* Currently only expecting one value per invocation right now. That will change in phase 2, hence the stdarg setup. */
	va_start(var, argcnt);
	assert(1 == argcnt);
	objfilespec = va_arg(var, mval *);
	va_end(var);
	MV_FORCE_STR(objfilespec);
	/* Initialize pblk with information about the pattern in the argument to ZRUPDATE. */
	memset(&pblk, 0, SIZEOF(pblk));
        pblk.buffer = pblkbuf;
	pblk.buff_size = (unsigned char)(MAX_FBUFF);	/* Pass size of buffer - 1 (standard protocol for parse_file). */
	pblk.def1_buf = DOTOBJEXT;			/* Default .o file type if not specified. */
	pblk.def1_size = SIZEOF(DOTOBJEXT) - 1;
	pblk.fop = F_SYNTAXO;				/* Syntax check only - bypass directory / file existence check. */
	status = parse_file(&objfilespec->str, &pblk);
	if (ERR_PARNORMAL != status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, objfilespec->str.len, objfilespec->str.addr, status);
	wildcarded = (pblk.fnb & F_WILD);		/* Our error logic is different depending on the presence of wildcards. */
	invalid = FALSE;
	if (0 != pblk.b_name)
	{	/* A file name was specified (if not, it is probably hard to find the file name, but that can be dealt with later).
		 * Like above, the string must be comprised of valid chars for routine names.
		 */
		for (chptr = pblk.l_name, fnamlen = pblk.b_name; 0 < fnamlen; chptr++, fnamlen--)
		{
			if ((ASTERISK != *chptr) && (QUESTION != *chptr))
			{	/* Substitute '%' for '_'. While this substitution is valid just for the first char, only the first
				 * char can be '%', so a check of the second or later char would fail the '%' substitution anyway.
				 */
				chr = ('_' == *chptr) ? '%' : *chptr;
				/* We see a char that isn't a wildcard character. If this is the first character, it can be
				 * alpha or percent. If the second or later character, it can be alphanumeric.
				 */
				if (((fnamlen == pblk.b_name) && (!VALID_MNAME_FCHAR(chr) || ('%' == *chptr)))	/* If 1st char */
					|| ((fnamlen != pblk.b_name) && !VALID_MNAME_NFCHAR(chr)))		/* If 2nd+ char */
				{
					invalid = TRUE;
					break;
				}
			}
		}
	} else if (!wildcarded)
		invalid = TRUE;
	if (invalid)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_FILEPARSE, 2, objfilespec->str.len, objfilespec->str.addr,
			      ERR_TEXT, 2, RTS_ERROR_TEXT("Filename is not a valid routine name"));
	/* Do a simlar check for the file type */
	seenfext = FALSE;
	if (0 != pblk.b_ext)
	{	/* If a file extension was specified - get the extension sans any potential wildcard character. */
		for (chptr = pblk.l_ext + 1, fextlen = pblk.b_ext - 1; 0 < fextlen; chptr++, fextlen--)
		{	/* Check each character in the extension except the first, which is the dot if extension exists at all. */
			if (ASTERISK != *chptr)
			{	/* We see a char that is not a '*' wildcard character. If we have already seen our "o" file
				 * extension or a '?' wildcard character (which we assume is "o"), this char makes our requirement
				 * filetype impossible, so raise an error.
				 */
				if (seenfext || ((OBJEXT != *chptr) && (QUESTION != *chptr)))
				{
					invalid = TRUE;
					break;
				}
				seenfext = TRUE;
			}
		}
	} else if (!wildcarded)
		invalid = TRUE;
	if (invalid)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_FILEPARSE, 2, objfilespec->str.len, objfilespec->str.addr,
				ERR_TEXT, 2, RTS_ERROR_TEXT("Unsupported filetype specified"));
	zsrch_clr(STRM_ZRUPDATE);	/* Clear any existing search cache */
	object_count = 0;
	do
	{	/* The DO-WHILE form is to do one iteration even if not wildcarded. */
		plen.p.pint = op_fnzsearch(objfilespec, STRM_ZRUPDATE, 0, &objpath);
		if (TRUE == (noresult = (0 == objpath.str.len)))	/* Note: assignment! */
		{	/* No (more) matches. In wildcarded case we are simply done with this loop. */
			if (wildcarded)
				break;
			else
			{	/* In a non-wildcarded case we want to verify whether the user is referring to a previously existent
				 * file that got removed or the one that op_fnzsearch() silently skipped due to access issues. So,
				 * set the objpath to the user-provided string after processing by parse_file() and adjust the
				 * length fields accordingly.
				 */
				objpath.str.addr = pblk.buffer;
				objpath.str.len = pblk.b_esl;
				SET_LENGTHS(&plen, objpath.str.addr, objpath.str.len, FALSE);
			}
		}
		/* Verify the extension and filename on wildcarded patterns; the non-wildcarded ones have been checked earlier.
		 * Start with the extension.
		 */
		if (wildcarded && ((SIZEOF(DOTOBJEXT) - 1 != plen.p.pblk.b_ext)
				|| (OBJEXT != objpath.str.addr[plen.p.pblk.b_dir + plen.p.pblk.b_name + 1])))
			continue;
		/* Before opening the relinkctl file, verify that a valid routine name can be derived, thus almost definitely
		 * telling us that the object name is also correct. The only exception is when the object name starts with a '%',
		 * so we want to note down that fact. Note that we cannot operate on the objpath memory because we would be
		 * affecting the object name, so we have to make a copy first.
		 */
		if (wildcarded && ((0 == plen.p.pblk.b_name) || ('%' == objpath.str.addr[plen.p.pblk.b_dir])))
			continue;
		memcpy(namebuf, objpath.str.addr + plen.p.pblk.b_dir, plen.p.pblk.b_name);
		rtnname.len = plen.p.pblk.b_name;
		rtnname.addr = namebuf;
		CONVERT_FILENAME_TO_RTNNAME(rtnname);	/* Get rtnname before searching in relinkctl file */
		if (wildcarded && !valid_mname(&rtnname))
			continue;
		assert(!noresult || !wildcarded);	/* We should have left the loop early on no results with a wildcard. */
		/* The reasons for doing the below STAT depend on the situation. If we do have at least one result, we need to make
		 * sure it is legitimate. If we have no results, we do the STAT because op_fnzsearch() on non-wildcarded requests
		 * may cleanly return an empty list even in the face of access errors, whereas we want to notify the user about
		 * potential access issues on a single file.
		 */
		memcpy(statbuf, objpath.str.addr, objpath.str.len);
		statbuf[objpath.str.len] = '\0';
		LSTAT_FILE(statbuf, &outbuf, status);	/* We use lstat to detect and eliminate soft links. */
		if (-1 == status)
		{	/* In the wildcarded case we just skip missing files. Any access error (but not the case of a missing file)
			 * gets reported on non-wildcarded patterns. If we did not find this file initially, it does not matter if
			 * exists now. We simply want to determine whether the reason that we could not find it had to do with
			 * access errors.
			 */
			if (wildcarded)
				continue;
			else if (ENOENT != errno)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2,
						objfilespec->str.len, objfilespec->str.addr, errno);
		} else if (!S_ISREG(outbuf.st_mode))
		{	/* We are only interested in regular files. */
			continue;
		}
		/* Extraction of object directory is different depending on whether a match was found or not. If we have a match,
		 * then objpath already contains the full path to the object, including the directory. If not, then we cannot use
		 * objpath because it was populated with user's argument to ZRUPDATE, which might not have a directory name in it.
		 * So, in that case we derive the directory name from the original parsing results populated by parse_file().
		 */
		if (noresult)
		{
			objdir.addr = pblk.l_dir;
			objdir.len = pblk.b_dir;
		} else
		{
			objdir.addr = objpath.str.addr;
			objdir.len = plen.p.pblk.b_dir;
		}
		linkctl = relinkctl_attach(&objdir, &objpath.str, 0);	/* Create/attach/open relinkctl file. */
		if (NULL == linkctl)
		{
			if (wildcarded)
				continue;
			else
			{	/* Note that the below errno value should come from the realpath() call in relinkctl_attach()
				 * invoked above, so we need to make sure nothing gets called in between.
				 */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2,
						objfilespec->str.len, objfilespec->str.addr, errno);
			}
		}
		if (!wildcarded)
		{	/* In the non-wildcarded case we decide whether to proceed with the cycle bump thusly:
			 *  1. If the specified file exists, it may or may not be accounted for in the relinkctl file, meaning that
			 *     we need to either add it there or simply update its cycle.
			 *  2. If the file does not exist on disk, but the routine is found in the relinkctl file, update its cycle.
			 *  3. If there is no file and no entry for it in the relinkctl file, do nothing (info error removed by
			 *     request).
			 */
			COMPUTE_RELINKCTL_HASH(&rtnname, hash, linkctl->hdr->relinkctl_hash_buckets);
			rec = relinkctl_find_record(linkctl, &rtnname, hash, &prev_hash_index);
			if ((NULL == rec) && noresult)
				return;
		}
		rec = relinkctl_insert_record(linkctl, &rtnname);
		RELINKCTL_CYCLE_INCR(rec, linkctl); 		/* Increment cycle indicating change to world */
		object_count++;					/* Update the count of valid objects encountered. */
	} while (wildcarded);
	/* For a wildcarded request that did not return any suitable object files give a no objects found error as a "soft" INFO
	 * level message, which gets supressed in except in direct mode.
	 */
	if (wildcarded && (0 == object_count))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) MAKE_MSG_INFO(ERR_FILEPARSE), 2, objfilespec->str.len,
			      objfilespec->str.addr, ERR_TEXT, 2, RTS_ERROR_TEXT("No object files found"));
}
#endif /* AUTORELINK_SUPPORTED */
