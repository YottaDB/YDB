/****************************************************************
 *								*
 *	Copyright 2011, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define trigger subscript types/order. Used to define enum trig_subs_t in trigger.h
 * and trigger_subs in mtables.c.
 *
 * Note : The order of lines below matters to a great extent. For example TRIGNAME needs to be before CMD
 * which in turn needs to be before XECUTE as that otherwise affects the output of MUPIP TRIGGER -SELECT.
 * There are other requirements like this in MUPIP TRIGGER -SELECT output format.
 * In addition, BHASH and LHASH need to be at the end of this list. The #define of NUM_SUBS relies on this.
 */

/*
TRIGGER_SUBSDEF (trigsubstype, subsname,   litmvalname,      trigfilequal, partofhash                       )
*/
TRIGGER_SUBSDEF (TRIGNAME_SUB, "TRIGNAME", literal_trigname, "-name=",     TRSBS_IN_NONE                    )
TRIGGER_SUBSDEF (GVSUBS_SUB,   "GVSUBS",   literal_gvsubs,   "",           (TRSBS_IN_LHASH | TRSBS_IN_BHASH) )
TRIGGER_SUBSDEF (CMD_SUB,      "CMD",      literal_cmd,      "-commands=", TRSBS_IN_NONE                    )
TRIGGER_SUBSDEF (OPTIONS_SUB,  "OPTIONS",  literal_options,  "-options=",  TRSBS_IN_NONE                    )
TRIGGER_SUBSDEF (DELIM_SUB,    "DELIM",    literal_delim,    "-delim=",    TRSBS_IN_BHASH                   )
TRIGGER_SUBSDEF (ZDELIM_SUB,   "ZDELIM",   literal_zdelim,   "-zdelim=",   TRSBS_IN_BHASH                   )
TRIGGER_SUBSDEF (PIECES_SUB,   "PIECES",   literal_pieces,   "-pieces=",   TRSBS_IN_BHASH                   )
TRIGGER_SUBSDEF (XECUTE_SUB,   "XECUTE",   literal_xecute,   "-xecute=",   (TRSBS_IN_LHASH | TRSBS_IN_BHASH) )
TRIGGER_SUBSDEF (CHSET_SUB,    "CHSET",    literal_chset,    "",           TRSBS_IN_NONE                    )
TRIGGER_SUBSDEF (BHASH_SUB,    "BHASH",    literal_bhash,    "",           TRSBS_IN_NONE                    )
TRIGGER_SUBSDEF (LHASH_SUB,    "LHASH",    literal_lhash,    "",           TRSBS_IN_NONE                    )
