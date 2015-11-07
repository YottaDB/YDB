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

/*
 * mu_getlst.c
 *
 * Description:	constructs the list that is specified by "name" from cli, size of each item
 *		of the list is "size", if anything wrong happened, set error_mupip.
 *		If we are in backup, we also parse the file spec for the list.
 *
 * Input:	char *name		-- specifies cli value to get
 *		int4 size		-- specifies the size of a list item
 *		bool in_backup		-- specifies whether we need to parse file specs for the list.
 * Output:	tp_region *grlist	-- head of the list constructed
 *		error_mupip		-- set, if something wrong happened
 */
#include "mdef.h"

#include "gtm_string.h"
#include "gtm_limits.h"

#include <descrip.h>
#include <climsgdef.h>
#include <strdef.h>
#include <rms.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mupipbckup.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "mu_getlst.h"
#include "gtmmsg.h"

GBLREF  bool 		error_mupip;
GBLREF  bool		in_backup;
GBLREF  bool		is_directory;
GBLREF	tp_region	*grlist;
GBLREF	gd_addr		*gd_header;
GBLREF	boolean_t	mu_star_specified;

void mu_getlst(char *name, int4 size)
{
	unsigned char	regspec_buffer[GTM_PATH_MAX], filspec_buffer[GTM_PATH_MAX];
	unsigned short	ret_len, ct;
	gd_region	*reg;
	tp_region	*list;
	uint4		status;
	boolean_t	matched;

	$DESCRIPTOR(regspec, regspec_buffer);
	$DESCRIPTOR(filspec, filspec_buffer);
	$DESCRIPTOR(cand_str, "");
	$DESCRIPTOR(fili,"DIRECTORY");
	$DESCRIPTOR(regi, "");
	error_def(ERR_FILEPARSE);
	error_def(ERR_TEXT);

	regi.dsc$a_pointer = name;
	regi.dsc$w_length = strlen(name);

	assert(size > 0);
	mu_star_specified = FALSE;

	is_directory = FALSE;
	for (; CLI$_ABSENT != CLI$GET_VALUE(&regi, &regspec, &ret_len); regspec.dsc$w_length = MAX_FN_LEN + 1)
	{
		if ((1 == ret_len) && ('*' == *regspec.dsc$a_pointer))
			mu_star_specified = TRUE;
		regspec.dsc$w_length = ret_len;
		reg = (gd_region *)gd_header->regions;
		for (matched = FALSE, ct = 0 ; ct < gd_header->n_regions ; reg++, ct++)
		{
			cand_str.dsc$a_pointer = &(reg->rname[0]);
			cand_str.dsc$w_length = strlen(reg->rname);
			if(STR$_MATCH == str$match_wild(&cand_str, &regspec))
			{
				matched = TRUE;
				if (NULL == (list = insert_region(reg, &(grlist), NULL, size)))
				{
                               		error_mupip = TRUE;
					rts_error(VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_STRING("Region not found"));
					continue;
				}
				if ((FALSE == in_backup) || (0 != ((backup_reg_list *)list)->backup_file.len))
					continue;
				if (TRUE == is_directory)
				{
					assert(NULL != grlist->fPtr);
					mubexpfilnam((backup_reg_list *)list);
					if (error_mupip)
						return;
				}
				else
				{
					/* get a file spec for this reg spec */
					status = CLI$GET_VALUE(&fili, &filspec, &ret_len);
					if ((SS$_NORMAL != status) && (CLI$_COMMA != status))
					{
						gtm_putmsg(VARLSTCNT(1) status);
						error_mupip = TRUE;
					        return;
					}
					if (FALSE == mubgetfil((backup_reg_list *)list,
								filspec.dsc$a_pointer,
								ret_len))
					{
						gtm_putmsg(VARLSTCNT(4) ERR_FILEPARSE, 2,
							ret_len, filspec.dsc$a_pointer);
						error_mupip = TRUE;
					        return;
					}
					if ((FALSE == is_directory) && (SS$_NORMAL == status))
						break;
				}
       	        	}
		} /* foreach region in gd_header */
		if (FALSE == matched)
		{
			util_out_print("Region !AD not found.", TRUE, regspec.dsc$w_length, regspec.dsc$a_pointer);
			error_mupip = TRUE;
			return;
		}
	} /* foreach reg spec */

	return;
}
