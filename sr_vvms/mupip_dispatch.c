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

/*------------------------------------------------------------------------------
 *
 * The 3 files, GTMCOMMANDS.CLDX, MUPIP_CMD.CLD and MUPIP_DISPATCH.C, must
 * be maintained in parallel.  In order to add a MUPIP command, first update
 * GTMCOMMANDS.CLDX.  The new command must be added as a member of the
 * TYPE MUPIP_ACTIONS.  The new syntax MUST have the first parameter:
 *
 *	PARAMETER	P1
 *		LABEL =	MUPIP_ACTION
 *		VALUE	(REQUIRED)
 *
 * The actual routine which executes the new command must be added to
 * MUPIP_DISPATCH.C by including a new descriptor:
 *
 *	$DESCRIPTOR	(newcommand, "NEWCOMMAND");
 *
 * and by adding a new comparison:
 *
 *	if (!dsccmp (action, &newcommand))	mupip_newcommand ();
 *	else ...
 *
 * The new syntax should be converted to a verb definition and added to
 * MUPIP_CMD.CLD.  In order to convert a syntax definition in GTMCOMMANDS.CLDX
 * to a verb definition in MUPIP_CMD.CLD:
 *
 * 1.	Change the line DEFINE SYNTAX MUPIP_NEWCOMMAND to
 * 	DEFINE VERB NEWCOMMAND.
 * 2.	Add the routine clause ROUTINE mupip_newcommand, where mupip_newcommand
 *	is the name of the routine used in MUPIP_DISPATCH.C.
 * 3.	Subtract 1 from every parameter number, i.e., P8 becomes P7, P7
 *	becomes P6, etc.
 *
 *----------------------------------------------------------------------------*/


#include "mdef.h"
#include <descrip.h>
#include <ssdef.h>
#include <rms.h>
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

#include "mupip_exit.h"
#include "muextr.h"
#include "mupip_create.h"
#include "mupip_set.h"
#include "mupip_backup.h"
#include "mupip_cvtgbl.h"
#include "mupip_cvtpgm.h"
#include "mupip_help.h"
#include "mupip_integ.h"
#include "mupip_extend.h"
#include "mupip_recover.h"
#include "mupip_restore.h"
#include "mupip_rundown.h"
#include "mupip_stop.h"
#include "mupip_upgrade.h"
#include "buddy_list.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"

#define	dsccmp(a,b)	(memcmp ((a)->dsc$a_pointer, (b)->dsc$a_pointer, mupip_action_len (a)))

mupip_dispatch ( struct dsc$descriptor *action)
{
	int		status;

	$DESCRIPTOR	(backup, "BACKUP");
	$DESCRIPTOR	(convert, "CONVERT");
	$DESCRIPTOR	(create,"CREATE");
	$DESCRIPTOR	(exit, "EXIT");
	$DESCRIPTOR	(extend, "EXTEND");
	$DESCRIPTOR	(extract, "EXTRACT");
	$DESCRIPTOR	(integ, "INTEG");
	$DESCRIPTOR	(load, "LOAD");
	$DESCRIPTOR	(help, "HELP");
	$DESCRIPTOR	(quit, "QUIT");
	$DESCRIPTOR	(journal, "JOURNAL");
	$DESCRIPTOR	(restore, "RESTORE");
	$DESCRIPTOR	(rundown, "RUNDOWN");
	$DESCRIPTOR	(set, "SET");
	$DESCRIPTOR	(stop, "STOP");
	$DESCRIPTOR	(upgrade, "UPGRADE");

	if (!dsccmp (action, &backup))	mupip_backup ();
	else
	if (!dsccmp (action, &convert))	mupip_cvtpgm ();
	else
	if (!dsccmp (action, &create))	mupip_create ();
	else
	if (!dsccmp (action, &exit))	mupip_exit (SS$_NORMAL);
	else
	if (!dsccmp (action, &extend))	mupip_extend ();
	else
	if (!dsccmp (action, &extract))	mu_extract ();
	else
	if (!dsccmp (action, &integ))	mupip_integ ();
	else
	if (!dsccmp (action, &load))	mupip_cvtgbl ();
	else
	if (!dsccmp (action, &help))	mupip_help ();
	else
	if (!dsccmp (action, &quit))	mupip_exit (SS$_NORMAL);
	else
	if (!dsccmp (action, &journal))	mupip_recover ();
	else
	if (!dsccmp (action, &restore)) mupip_restore ();
	else
	if (!dsccmp (action, &rundown))	mupip_rundown ();
	else
	if (!dsccmp (action, &set))	mupip_set ();
	else
	if (!dsccmp (action, &stop))	mupip_stop ();
	else
	if (!dsccmp (action, &upgrade))	mupip_upgrade ();
	else
		GTMASSERT;
}

int mupip_action_len ( struct dsc$descriptor *d)
{
	unsigned char	*cp;

	for (cp = d->dsc$a_pointer;
		(char *) cp - d->dsc$a_pointer <= d->dsc$w_length &&
		*cp != SP && *cp && *cp != 9;
		cp++) ;
	return (char *) cp - d->dsc$a_pointer;
}

