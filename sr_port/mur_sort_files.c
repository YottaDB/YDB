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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "util.h"

GBLREF	mur_opt_struct		mur_options;
GBLREF	char			*log_rollback;

/*  Notes on the following logic:
 *
 *  The list of journal files specified on the command line is in an arbitrary and
 *  essentially random order.  This may be the result of wildcard processing and/or
 *  operator whim.  More than one of these journal files may apply to the same database
 *  file, however.  When this is the case, the list of files must be sorted into an
 *  order such that all files that apply to the same database file form a logically
 *  (and chronologically) continuous sequence.
 *
 *  Continuity of journal files is determined by examining the BOV (beginning of volume)
 *  and EOV (end of volume) timestamps contained in the journal file header.  Two
 *  journal files are continuous if and only if the EOV timestamp in the first file is
 *  identical to the BOV timestamp in the second file (and is non-zero). It may so happen
 *  that the BOV and EOV timestamp in the second file are the same (in very fast machines
 *  one might switch journals within a second which is the granularity in Unix). In that
 *  case we use the bov_tn and eov_tn fields to find out the order. If it turns out that
 *  they too are the same, then the order of the journals doesn't matter and we are fine.
 *
 *  It is an error if two or more journal files referring to the same database file
 *  cannot be sorted into a single continuous sequence.  (It is NOT an error if such
 *  a sequence begins and/or ends with unmatched timestamps;  in fact, this would be
 *  the norm for journal files produced by continuously running applications.)
 *
 *  Of course, journal files that apply to different database files are logically
 *  independent, and it doesn't matter what order they're processed in.
 */
bool	mur_sort_files(ctl_list **jnl_file_list_ptr)
{
	ctl_list	*curr, *prev, *next, *curr_sorted, *first_sorted, *last_sorted;
	jnl_file_header	*header, *header_sorted;
	bool		matched;
	int4		db_id;

	db_id = 0;
	first_sorted = last_sorted = NULL;
	for (curr = *jnl_file_list_ptr;  curr != NULL;  curr = next)
	{
		next = curr->next;
		header = mur_get_file_header(curr->rab);
		COPY_BOV_FIELDS_FROM_JNLHDR_TO_CTL(header, curr);
		if (FALSE == mur_jnlhdr_bov_check(header, curr->jnl_fn_len, curr->jnl_fn))
			return FALSE;
		matched = FALSE;
		for (curr_sorted = first_sorted;  curr_sorted != NULL;  curr_sorted = curr_sorted->next)
		{
			header_sorted = mur_get_file_header(curr_sorted->rab);
			if (header->data_file_name_length == header_sorted->data_file_name_length  &&
			    memcmp(header->data_file_name, header_sorted->data_file_name, header->data_file_name_length) == 0)
			{	/* Both journal files apply to the same database file */
				/* return FALSE if the half-open {time or tn}-intervals of the two journal files intersect */
				if (FALSE == mur_jnlhdr_multi_bov_check(header_sorted, curr_sorted->jnl_fn_len, curr_sorted->jnl_fn,
										header, curr->jnl_fn_len, curr->jnl_fn, FALSE))
					return FALSE;
				matched = TRUE;
				curr->db_id = curr_sorted->db_id;
				if (curr->eov_timestamp < curr_sorted->bov_timestamp
					|| (curr->eov_timestamp == curr_sorted->bov_timestamp
						&& curr->eov_tn <= curr_sorted->bov_tn))
					break;
			} else if (matched) /* This file applied to the same database file as the previous sorted file
					   	(but not the current one), so it must be inserted before the current one */
				break;
		}
		/* Insert this file before the current entry in the sorted list, if any */
		if (curr_sorted == NULL)
		{	/* The sorted list is empty, or the file is to be inserted at the end of the list */
			curr->next = NULL;
			curr->prev = last_sorted;	/* NULL if empty */
			if (first_sorted == NULL)	/* The list is empty */
				first_sorted = curr;
			else	/* Insert it at the end */
				last_sorted->next = curr;
			last_sorted = curr;
			if (!matched)
				curr->db_id = ++db_id;
		} else
		{
			curr->next = curr_sorted;
			curr->prev = curr_sorted->prev;
			if (curr_sorted == first_sorted)	/* Insert it at the beginning of the list */
				first_sorted = curr;
			else
				curr->prev->next = curr;
			curr_sorted->prev = curr;
		}
	}
	/* Reset the original pointer to point to the sorted list */
	*jnl_file_list_ptr = first_sorted;
	/* Run through the sorted list looking for mismatches, and set the concat_* flags as appropriate */
	for (prev = *jnl_file_list_ptr, curr = prev->next;  curr != NULL;  prev = curr, curr = curr->next)
	{
		if (prev->db_id == curr->db_id)
		{
			if (mur_get_file_header(prev->rab)->before_images != mur_get_file_header(curr->rab)->before_images)
			{
				util_out_print("Journal files !AD and !AD", TRUE, prev->jnl_fn_len, prev->jnl_fn,
											curr->jnl_fn_len, curr->jnl_fn);
				util_out_print("  have different settings of BEFORE_IMAGES for the same database", TRUE);
				return FALSE;
			}
			prev->concat_next = curr->concat_prev = TRUE;
		}
	}
	return TRUE;
}
