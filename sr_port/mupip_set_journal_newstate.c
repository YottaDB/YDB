/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "mupip_set.h"
#include "cli.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	char		*jnl_state_lit[];
GBLREF	char		*repl_state_lit[];

uint4 mupip_set_journal_newstate(set_jnl_options *jnl_options, jnl_create_info *jnl_info, mu_set_rlist *rptr)
{
	enum jnl_state_codes	jnl_curr_state;
	enum repl_state_codes	repl_curr_state;
	boolean_t 		current_image;

	error_def(ERR_REPLNOBEFORE);
	error_def(ERR_REPLJNLCNFLCT);
	error_def(ERR_JNLDISABLE);

	jnl_curr_state = rptr->sd->jnl_state;
	repl_curr_state = rptr->sd->repl_state;
	current_image = rptr->sd->jnl_before_image;
	if (CLI_ABSENT == jnl_options->cli_journal)
		rptr->jnl_new_state = jnl_curr_state;
	else if ((CLI_NEGATED == jnl_options->cli_journal) || (CLI_NEGATED == jnl_options->cli_enable))
		rptr->jnl_new_state = jnl_notallowed; /* DISABLE specified */
	else if ((jnl_notallowed != jnl_curr_state) || (CLI_PRESENT == jnl_options->cli_enable))
	{	/* journaling is already ENABLED or ENABLE is explicitly specified */
		if (CLI_NEGATED == jnl_options->cli_on)	/* OFF specified */
			rptr->jnl_new_state = jnl_closed;
		else if (repl_curr_state == repl_was_open && CLI_PRESENT != jnl_options->cli_replic_on)
		{ /* Journaling was turned OFF by jnl_file_lost(). Do not allow turning journaling ON without also
		     turning replication ON */
			gtm_putmsg(VARLSTCNT(8) ERR_REPLJNLCNFLCT, 6, LEN_AND_STR(jnl_state_lit[jnl_open]),
					DB_LEN_STR(gv_cur_region),
					LEN_AND_STR(repl_state_lit[repl_closed]));
			return EXIT_WRN;
		}
		else /* ON explicitly specified or present by default */
			rptr->jnl_new_state = jnl_open;
	} else	/* jnl_notallowed == jnl_curr_state && CLI_ABSENT == jnl_options->cli_enable */
	{
		if (CLI_PRESENT != jnl_options->cli_replic_on)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_JNLDISABLE, 2, DB_LEN_STR(gv_cur_region));
			return EXIT_WRN;
		} else
			rptr->jnl_new_state = jnl_open;	/* turn journaling on for REPLICATION=ON */
	}
	rptr->before_images = (jnl_options->image_type_specified ?  jnl_info->before_images : current_image);
	if (CLI_PRESENT == jnl_options->cli_replic_on) /* replic="ON" */
	{
		assert((CLI_ABSENT == jnl_options->cli_journal)
			|| (!((CLI_NEGATED == jnl_options->cli_enable) || (CLI_NEGATED == jnl_options->cli_on))
			|| (jnl_open == rptr->jnl_new_state)));
		assert((CLI_ABSENT == jnl_options->cli_journal) || (!jnl_options->image_type_specified || rptr->before_images));
		rptr->repl_new_state = repl_open;
		rptr->jnl_new_state = jnl_open;
		rptr->before_images = TRUE;
	} else if (CLI_NEGATED == jnl_options->cli_replic_on) /* replic="OFF" */
		rptr->repl_new_state = repl_closed;
	else
	{
		if (repl_open == repl_curr_state)
		{
			if (CLI_ABSENT != jnl_options->cli_journal)
			{
				if (jnl_options->image_type_specified && !rptr->before_images)
				{
					gtm_putmsg(VARLSTCNT(4) ERR_REPLNOBEFORE, 2, DB_LEN_STR(gv_cur_region));
					return EXIT_WRN;
				}
				if (jnl_open != rptr->jnl_new_state)
				{
					gtm_putmsg(VARLSTCNT(8) ERR_REPLJNLCNFLCT, 6,
							LEN_AND_LIT("OFF/DISABLED"),
							DB_LEN_STR(gv_cur_region),
							LEN_AND_STR(repl_state_lit[repl_open]));
					return EXIT_WRN;
				}
			}
		}
		rptr->repl_new_state = repl_curr_state;
	}
	return EXIT_NRM;
}
