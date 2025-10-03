/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "svnames.h"
#include "nametabtyp.h"
#include "funsvn.h"
#include "advancewindow.h"
#include "cmd.h"
#include "namelook.h"
#include "start_fetches.h"
#include "show_source_line.h"
#include "util.h"
#include "hashtab_str.h"
#include "buddy_list.h"
#include "duplicatenew_cleanup.h"

STATICFNDCL void duplicatenew_check(mstr new_var_name, boolean_t last_new, boolean_t exclusive);

GBLREF	boolean_t	run_time;

LITREF	nametabent	svn_names[];
LITREF	svn_data_type	svn_data[];
LITREF	unsigned char	svn_index[];

/*
 * Macro used by duplicatenew_check to indicate
 * that a variable has been NEWed both inside and outside of parentheses.
 */
#define NEWED_BOTH 2

error_def(ERR_INVSVN);
error_def(ERR_RPARENMISSING);
error_def(ERR_SVNEXPECTED);
error_def(ERR_SVNONEW);
error_def(ERR_VAREXPECTED);
error_def(ERR_DUPLICATENEW);

/* Function for compiler to call once per variable being NEWed.
 * @returns:
 *	TRUE	If successfully records a NEW statement.
 *	FALSE	If it encounters a syntax error.
*/
int m_new(void)
{
	boolean_t	parse_warn;
	int		count, n;
	mvar		*var;
	mstr		window_ident_val;
	oprtype		tmparg;
	triple		*fetch, *next, *org, *ref, *s, *tmp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(xecute_literal_parse))
		return FALSE;
	switch (TREF(window_token))
	{
	case TK_IDENT:
		var = get_mvaddr(&(TREF(window_ident)));
		/* Gives warning if var was already NEWed in the current command block. */
		duplicatenew_check(var->mvname, ((TREF(director_token)) != TK_COMMA), FALSE);
		if ((var->last_fetch != (TREF(fetch_control)).curr_fetch_trip) && !TREF(discard))
		{	/* This block is identical to one in put_mvar */
			fetch = newtriple(OC_PARAMETER);
			(TREF(fetch_control)).curr_fetch_opr->operand[1] = put_tref(fetch);
			fetch->operand[0] = put_ilit(var->mvidx);
			((TREF(fetch_control)).curr_fetch_count)++;
			(TREF(fetch_control)).curr_fetch_opr = fetch;
			var->last_fetch = (TREF(fetch_control)).curr_fetch_trip;
		}
		tmp = maketriple(OC_NEWVAR);
		tmp->operand[0] = put_ilit(var->mvidx);
		ins_triple(tmp);
		advancewindow();
		return TRUE;
	case TK_ATSIGN:
		/* In order for window_ident_val to point to the correct @var value here,
		 * we need it to point to the current line. (TREF(lexical_ptr)) points one char
		 * after 2 tokens down on the current line, and @var is 2 tokens so subtracting
		 * the length of the @var gives the correct pointer.
		 */
		window_ident_val.len = (TREF(director_ident)).len + 1;
		window_ident_val.addr = (TREF(lexical_ptr)) - window_ident_val.len;
		if (!indirection(&tmparg))
		{
			duplicatenew_cleanup();
			return FALSE;
		}
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit((mint) indir_new);
		/* Gives warning if var was already NEWed in the current command block. */
		duplicatenew_check(window_ident_val, (TREF(window_token) != TK_COMMA), FALSE);
		ins_triple(ref);
		MID_LINE_REFETCH;
		return TRUE;
	case TK_DOLLAR:
		/* In order for window_ident_val to point to the correct $var value here,
		 * we need it to point to the current line. (TREF(lexical_ptr)) points one char
		 * after 2 tokens down on the current line, and $var is 2 tokens so subtracting
		 * the length of the $var gives the correct pointer.
		 */
		window_ident_val.len = (TREF(director_ident)).len + 1;
		window_ident_val.addr = (TREF(lexical_ptr)) - window_ident_val.len;
		advancewindow();
		if (TK_IDENT == TREF(window_token))
		{
			parse_warn = FALSE;
			if ((0 <= (n = namelook(svn_index, svn_names, (TREF(window_ident)).addr, (TREF(window_ident)).len))))
			{	/* NOTE assignment above */

				/* Gives warning if var was already NEWed in the current command block. */
				duplicatenew_check(window_ident_val, (TREF(director_token) != TK_COMMA), FALSE);
				switch (svn_data[n].opcode)
				{
				case SV_ZTRAP:
				case SV_ETRAP:
				case SV_ESTACK:
				case SV_ZYERROR:
				case SV_ZGBLDIR:
				case SV_TEST:
				case SV_ZCMDLINE:
				GTMTRIG_ONLY(case SV_ZTWORMHOLE:)
					tmp = maketriple(OC_NEWINTRINSIC);
					tmp->operand[0] = put_ilit(svn_data[n].opcode);
					break;
				default:
					STX_ERROR_WARN(ERR_SVNONEW);	/* sets "parse_warn" to TRUE */
					tmp = NULL;
				}
			} else
			{
				STX_ERROR_WARN(ERR_INVSVN);	/* sets "parse_warn" to TRUE */
				tmp = NULL;
			}
			advancewindow();
			if (!parse_warn)
			{
				assert(tmp);
				ins_triple(tmp);
			} else if (run_time)
			{	/* OC_RTERROR triple would have been inserted in curtchain by ins_errtriple
				 * (invoked by stx_error). No need to do anything else in that case. But that
				 * happens only if "run_time" is TRUE and not if it is FALSE (e.g. $ZYCOMPILE).
				 * Hence the "else if (run_time)" check above.
				 */
				assert(ALREADY_RTERROR);
			}
			return TRUE;
		}
		duplicatenew_cleanup();
		stx_error(ERR_SVNEXPECTED);
		return FALSE;
	case TK_EOL:
	case TK_SPACE:
		/* This actually a NEW all, but an XNEW with no arguments does the job */
		tmp = maketriple(OC_XNEW);
		tmp->operand[0] = put_ilit((mint) 0);
		ins_triple(tmp);
		/* start a new fetch for whatever follows on the line */
		MID_LINE_REFETCH;
		return TRUE;
	case TK_LPAREN:
		ref = org = maketriple(OC_XNEW);
		count = 0;
		do
		{
			advancewindow();
			next = maketriple(OC_PARAMETER);
			ref->operand[1] = put_tref(next);
			switch (TREF(window_token))
			{
			case TK_IDENT:
				next->operand[0] = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
				/* Gives warning if var was already NEWed in the current command block. */
				duplicatenew_check(TREF(window_ident),
					(TK_RPAREN == (TREF(director_token)) && (',' != *(TREF(lexical_ptr)))),
					TRUE);
				advancewindow();
				break;
			case TK_ATSIGN:
				/* In order for window_ident_val to point to the correct @var value here,
				 * we need it to point to the current line. (TREF(lexical_ptr)) points one char
				 * after 2 tokens down on the current line, and @var is 2 tokens so subtracting
				 * the length of the @var gives the correct pointer.
				*/
				window_ident_val.len = (TREF(director_ident)).len + 1;
				window_ident_val.addr = (TREF(lexical_ptr)) - window_ident_val.len;
				if (!indirection(&tmparg))
				{
					duplicatenew_cleanup();
					return FALSE;
				}
				s = newtriple(OC_INDLVARG);
				s->operand[0] = tmparg;
				next->operand[0] = put_tref(s);
				/* Gives warning if var was already NEWed in the current command block. */
				duplicatenew_check(window_ident_val,
					((TK_RPAREN == (TREF(director_token))) && (',' != *(TREF(lexical_ptr)))),
					TRUE);
				break;
			default:
				/* After false is returned the compiler will not call this function
				 * for the rest of this line so call duplicatenew_check to clear buffer.
				 */
				duplicatenew_cleanup();
				stx_error(ERR_VAREXPECTED);
				return FALSE;
			}
			ins_triple(next);
			ref = next;
			count++;
		} while (TK_COMMA == TREF(window_token));
		if (TK_RPAREN != TREF(window_token))
		{
			duplicatenew_cleanup();
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
		advancewindow();
		org->operand[0] = put_ilit((mint) count);
		ins_triple(org);
		/* start a new fetch for whatever follows on the line */
		MID_LINE_REFETCH;
		return TRUE;
	default:
		/* After false is returned the compiler will not call this function
		 * for the rest of this line so call duplicatenew_cleanup.
		 */
		duplicatenew_cleanup();
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
}

/*
 * Checks if new_var_name has already been NEWed in the current block.
 * if it has and the program is in runtime issue a DUPLICATENEW warning.
 * keeps track of variables newed in each line internally, so this must be called
 * for every variable being newed in a new command including the first.
 * @param new_var_name, The name of the new variable as an mstr (note that mident is defined as an mstr).
 * @param last_new, TRUE if this is the last variable getting NEWed in the current block.
 * @param exclusive, 1 if variable is newed inside parentheses, otherwise 0.
 */
void duplicatenew_check(mstr new_var_name, boolean_t last_new, boolean_t exclusive)
{
	/* Hashtable of the mstr representing the names of the variables NEWed in the current command block. */
	static hash_table_str	*newed_on_line = NULL;
	static buddy_list	*to_free = NULL;
	void			**buddy_list_add_pointer;
	ht_ent_str		*table_entry, *ent_mname;
	char			*elem_to_free, *temp;
	int			table_value, *value_PTR;
	stringkey		*new_hash_key;
	boolean_t		duplicate_found, first_new;
	int4			i;

	/* This function checks if you need to give a compile time warning DUPLICATENEW that indicates NEWing
	 * a variable multiple times. This warning is not given at runtime as there are potential
	 * reasons to new a variable multiple times during runtime. For example you could write code that
	 * adds a variable to the list of ones being NEWed in an XECUTE command if it is needed
	 * for any reason during runtime, adding it twice if it is needed for 2 reasons.
	 * More details https://gitlab.com/YottaDB/DB/YDB/-/merge_requests/1768#note_2963274774
	 */
	if (run_time)
		return;
	first_new = (NULL == newed_on_line);
	if (first_new)
	{
		if (last_new)
			return; /* new_var_name is the first and last var so no repeat is possible. */
		newed_on_line = malloc(SIZEOF(hash_table_str));
		to_free = malloc(SIZEOF(buddy_list));
		initialize_list(to_free, SIZEOF(void *), 16);
		init_hashtab_str(newed_on_line, 16, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	}
	new_hash_key = malloc(SIZEOF(stringkey));
	buddy_list_add_pointer = (void **)get_new_element(to_free, 1);
	*buddy_list_add_pointer = new_hash_key;
	new_hash_key->str = new_var_name;
	COMPUTE_HASH_STR(new_hash_key);
	if (!first_new) /* No need to query the hash table if you are on the first variable. */
	{
		table_entry = lookup_hashtab_str(newed_on_line, new_hash_key);
		if (NULL == table_entry)
			duplicate_found = FALSE;
		else
		{
			table_value = *((int *)(table_entry->value));	/* Table_value is 0 for seen not exclusive,
									 * 1 for seen exclusive, and NEWED_BOTH for seen both.
									 */
			if ((NEWED_BOTH == table_value) || (exclusive == table_value)) /* return true if you have seen this value before */
				duplicate_found = TRUE;
			else
			{
				duplicate_found = FALSE;
				*((int *)table_entry->value) = NEWED_BOTH; /* Otherwise mark that you have seen both values. */
			}
		}
	} else
		duplicate_found = FALSE;
	if (last_new) /* This is the last variable in this command. Clean up memory. */
	{
		for (i = 0; i < to_free->nElems; i++)
		{
			elem_to_free = find_element(to_free, i);
			free(*((void **)elem_to_free));
		}
		cleanup_list(to_free);
		free(to_free);
		free_hashtab_str(newed_on_line);
		free(newed_on_line);
		newed_on_line = NULL;
		to_free = NULL;
	} else if (first_new || (NULL == table_entry)) /* new_var_name not yet in table, so add it. */
	{
		value_PTR = malloc(SIZEOF(int));
		assert(1 == exclusive || 0 == exclusive); /* Assumes that if exclusive is TRUE, it is equal to 1. */
		*value_PTR = exclusive;
		buddy_list_add_pointer = (void **)get_new_element(to_free, 1);
		*buddy_list_add_pointer = value_PTR;
		add_hashtab_str(newed_on_line, new_hash_key, value_PTR, &ent_mname);
	}
	if (duplicate_found)
	{
		show_source_line(TRUE);
		dec_err(VARLSTCNT(4) ERR_DUPLICATENEW, 2, new_var_name.len, new_var_name.addr);
	}
}

/*
 * Clears the duplicatenew_check buffer.
 * used to call duplicatenew_check when the new command ends with an error.
 * if the duplicatenew_check buffer is already empty it is still safe to call this function.
 */
void duplicatenew_cleanup()
{
	mstr	dummy_ident;
	char	dummy_name;

	dummy_ident.len = 1;
	dummy_name = '\0';
	dummy_ident.addr = &dummy_name;
	duplicatenew_check(dummy_ident, TRUE, FALSE);
}
