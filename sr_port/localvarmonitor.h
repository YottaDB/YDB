/****************************************************************
 *								*
 * Copyright (c) 2017 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _LOCALVARMONITOR_H_DEFINED_
#define _LOCALVARMONITOR_H_DEFINED_

/* Note there are two forms of local variable monitoring in GTM - both initiated by VIEW commands. The first is the lv monitoring
 * potentially done by aliases when #ifdef(DEBUG_ALIAS) is set during a build followed by the appropriate
 *    VIEW LVMONSTART/LVMONSTOP/LVMONOUT
 * command.
 *
 * The second method is this one. There is one lvmon_var struct (in an array anchored by TREF(lvmon_vars_anchor)) for each
 * base variable named as a variable in. Monitoring is started with the
 *    VIEW "LVMON":<var1<:var2<...>>>
 * command. A lack of arguments turns off monitoring and releases the trace data structures.
 *
 * There are two routines of interest:
 *    1. lvmon_pull_values(idx)
 *    2. lvmon_compare_value_slots(idx1, idx2)
 *
 * lvmon_pull_values(idx)
 *    - Pulls the current value of all variables being monitored storing it in the index given.
 *
 * lvmon_compare_value_slots(idx1, idx2)
 *    - Compares the values in the two given slots giving an error message and generating a core if they differ but otherwise
 * 	allowing the process to continue.
 */

/* Uncomment following statement to debug LVMONITOR */
/* #define DEBUG_LVMONITOR */
#ifdef DEBUG_LVMONITOR
# define DBGLVMON(X) DBGFPF(X)
#else
# define DBGLVMON(X)
#endif

#define MAX_LVMON_VALUES 2				/* Enough for 1 comparison set for now - should be an even number */
#define IS_LVMON_ACTIVE (TREF(lvmon_active))

/* Structure to hold a single saved value */
typedef struct
{
	lv_val	varlvval;				/* Copy of the lv_val holding value (type and numeric value ) */
	mstr	varvalue;				/* If varlvval is string, points to malloc'd space holding value */
	uint4	alloclen;				/* Allocation length of var_value */
	GTM64_ONLY(int filler;)
} lvmon_value_ent;
/* Structure used to monitor variables defined in a table made up of lvmon_var structures */
typedef struct
{
	mname_entry	lvmv;
	lv_val		*varlvadr;			/* Only need to fetch it once unless curr_symtab_cycle has changed */
	gtm_int8	curr_symval_cycle;		/* Copy of curr_symval_cycle when copied lv_val addr */
	lvmon_value_ent	values[MAX_LVMON_VALUES];
} lvmon_var;

void	lvmon_pull_values(int lvmon_ary_idx);
void	lvmon_compare_value_slots(int lvmon_idx1, int lvmon_idx2);

#endif /* ifndef _LOCALVARMONITOR_H_DEFINED_ */
