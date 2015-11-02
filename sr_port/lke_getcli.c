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

/*
   lke_getcli.c : obtains CLI qualifiers, validates their values
   used in      : lke_clear.c, lke_show.c
*/

#include "mdef.h"
#include "gtm_string.h"
#include "cli.h"
#include "lke_getcli.h"
#include "util.h"

/*
 * ------------------------------------------------------
 * Get command line parameters
 * ------------------------------------------------------
 */
int4 lke_getcli(bool *all,
		bool *wait,
		bool *inta,
		int4 *pid,
		mstr *region,
		mstr *node,
		mstr *one_lock,
		bool *memory,
		bool *nocrit,
		boolean_t *exact)
{
	int4		status;
	unsigned short	len;
	int		keylen;
	char		one_lockbuf[MAX_ZWR_KEY_SZ + 1];

	status = TRUE;
/*
 * -----------------------------------------------------------
 * 		-INTERACTIVE overrides any defaults
		-NOINTERACTIVE overrides any defaults

 * otherwise:   default is nointeractive when -ALL is set
	        default is original value of inta without -ALL
 * -----------------------------------------------------------
 */


	*all  = (*all  && cli_present("ALL") == CLI_PRESENT);

	*inta = *inta && (cli_present("INTERACTIVE") != CLI_NEGATED);

	*wait = (*wait && cli_present("WAIT") == CLI_PRESENT);

	*memory = (*memory && cli_present("MEMORY") == CLI_PRESENT);
	*nocrit = (*nocrit && cli_present("CRIT") == CLI_NEGATED);
	*exact  = (*exact  && cli_present("EXACT") == CLI_PRESENT);

	if (cli_present("PID") == CLI_PRESENT)
	{
#ifdef HEXPID
		if (!cli_get_hex("PID", pid))
#else
		assert(SIZEOF(*pid) == SIZEOF(int));
		if (!cli_get_int("PID", (int4 *)pid))
#endif
		{
			*pid = 0;
			status = FALSE;
		}
	}
	else
		*pid  = 0 ;

	if (cli_present("REGION") == CLI_PRESENT)
	{
		len = region->len;
	 	if (!cli_get_str("REGION", region->addr, &len))
		{	util_out_print("Error getting REGION parameter",TRUE);
			region->len = 0;
			status = FALSE;
		} else
			region->len = len;
	}
	else
		region->len = 0;

	if (cli_present("LOCK") == CLI_PRESENT)
	{
		len = one_lock->len;
		if (!cli_get_str("LOCK", one_lock->addr, &len) || -1 == (keylen = lke_getki(one_lock->addr, len, one_lockbuf)))
		{
			util_out_print("Error getting LOCK parameter",TRUE);
			one_lock->len = 0;
			status = FALSE;
		} else
		{
			one_lock->len = keylen;
			memcpy(one_lock->addr, one_lockbuf, keylen);
		}
	}
	else
	{
		one_lock->len = 0;
		one_lock->addr = 0;
	}

	if (cli_present("NODE") == CLI_PRESENT)
	{
		len = node->len;
	 	if (!cli_get_str("NODE", node->addr, &len))
		{	util_out_print("Error getting NODE parameter",TRUE);
			node->len = 0;
			status = FALSE;
		} else
			node->len = len;
	}
	else
		node->len = 0;

	return status ;
}

