/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define trigger subscript types/order. Used to define enum trig_subs_t in trigger.h
 * and trigger_subs in mtables.c.
 */

TRIGGER_SUBDEF(TRIGNAME),
TRIGGER_SUBDEF(GVSUBS),
TRIGGER_SUBDEF(CMD),
TRIGGER_SUBDEF(OPTIONS),
TRIGGER_SUBDEF(DELIM),
TRIGGER_SUBDEF(ZDELIM),
TRIGGER_SUBDEF(PIECES),
TRIGGER_SUBDEF(XECUTE),
TRIGGER_SUBDEF(CHSET),
TRIGGER_SUBDEF(LHASH),
TRIGGER_SUBDEF(BHASH)
