/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Header file for things relating to reserved databases (e.g. statsDB) that are created when they are opened and
 * are deleted when the last user closes it.
 */
#ifndef GTM_RESERVEDDB_H
#define GTM_RESERVEDDB_H

/* ReservedDB flags applicable to gd_region->reservedDBFlags */
enum
{
	RDBF_AUTODB  = 0x01,			/* This DB is auto-created */
	RDBF_NOSTATS = 0x02,			/* This DB does not collect global stats so has no statsDB */
	RDBF_STATSDB = 0x04			/* This is a statsDB (must have AUTODB and NOSTATS also) */
};
#define RDBF_STATSDB_MASK (RDBF_AUTODB | RDBF_NOSTATS | RDBF_STATSDB)	/* This is a statsDB */

/* Possible errors we encounter that prevent us from setting baseDBnl->statsdb_fnname[.len] */
enum
{
	FNERR_NOERR = 0,			/* No error recorded */
	FNERR_NOSTATS,				/* BaseDB has NOSTATS set - (should never happen) */
	FNERR_STATSDIR_TRNFAIL,			/* Unable to translate $gtm_statsdir (should never happen) */
	FNERR_STATSDIR_TRN2LONG,		/* Translation of $gtm_statsdir too long (should never happen) */
	FNERR_INV_BASEDBFN,			/* BaseDBfn had no '/' making parse fail (should never happen) */
	FNERR_FTOK_FAIL,			/* The STAT() in gtm_ftok() failed (no rc) (should never hapen) */
	FNERR_FNAMEBUF_OVERFLOW			/* Not enough space to add the statsdb fname to fname buffer */
};
#define FNERR_NOSTATS_TEXT 		"The base database has NOSTATS set - should not be trying to open statistics database"
#define FNERR_STATSDIR_TRNFAIL_TEXT	"Error attempting to translate $gtm_statsdir"
#define FNERR_STATSDIR_TRN2LONG_TEXT	"Translation of $gtm_statsdir was too long to fit in buffer"
#define FNERR_INV_BASEDBFN_TEXT		"Unable to parse the base database filename"
#define FNERR_FTOK_FAIL_TEXT		"Failure generating FTOK value for $gtm_statsdir"
#define FNERR_FNAMEBUF_OVERFLOW_TEXT	"Buffer overflow detected adding statistics database filename (hash.<dbname>.gst) to " \
					"filename buffer"

/* Global name used for shared global stats in statsDB MM database */
#define STATSDB_GBLNAME		"%YGS"
#define STATSDB_GBLNAME_LEN	(SIZEOF(STATSDB_GBLNAME) - 1)
#define	STATSDB_FNAME_SUFFIX	".gst"
#define RESERVED_NAMESPACE	"%Y"
#define RESERVED_NAMESPACE_LEN	(SIZEOF(RESERVED_NAMESPACE) - 1)

/* The maximum size of $gtm_statsdir is MAX_FN_LEN minus a minimal statsDB fname */
#define MAX_STATSDIR_LEN	(MAX_FN_LEN - 8 /* hash */ - STRLEN(".a.dat") - STRLEN(STATSDB_FNAME_SUFFIX))

/* Size of the minimum additional record that can fit in a statsDB block after a record has been added to it. Use this
 * value to pad the first record in the block to prevent another record from being added. The '3' being added in is for
 * the single byte difference in the key that must exist plus the two NULL bytes that end the key following it.
 */
#define MIN_STATSDB_REC (SIZEOF(rec_hdr) + 3 + SIZEOF(gvstats_rec_t))

/* Macroize statsDB identification so we can change how it is defined in the future */
#define IS_STATSDB_REG(REG) (RDBF_STATSDB_MASK == (REG)->reservedDBFlags)
#define IS_STATSDB_CSA(CSA) (RDBF_STATSDB_MASK == (CSA)->reservedDBFlags)
#define	IS_RDBF_STATSDB(X)  (RDBF_STATSDB & (X)->reservedDBFlags)		/* X can be REG or CSA or CSD */
/* Macroize autoDB identification */
#define IS_AUTODB_REG(REG)  (RDBF_AUTODB & (REG)->reservedDBFlags)

/* Identify if a region name is lower-case (and thus for a statsDB) by just checking the first character of the region.
 * No need to check the entire region name as a region name is always all upper or all lower.
 */
#define IS_STATSDB_REGNAME(REGPTR) (ISLOWER_ASCII((REGPTR)->rname[0]))
#define IS_BASEDB_REGNAME(REGPTR)  (ISUPPER_ASCII((REGPTR)->rname[0]))

/* Macros to "find" the statsDBreg from a baseDBreg or vice versa. We use two different names for the two directions
 * but they are the same code because of how the indexes are set up. The name difference is just to make clear what
 * is happening where the macros are being used.
 */
#define STATSDBREG_TO_BASEDBREG(STATSDBREG, BASEDBREG)						\
MBSTART {											\
	int	dbindx;										\
	gd_addr	*owningGd;									\
												\
	dbindx = (STATSDBREG)->statsDB_reg_index;						\
	owningGd = (STATSDBREG)->owning_gd;							\
	assert(NULL != owningGd);								\
	assert(INVALID_STATSDB_REG_INDEX != dbindx);						\
	/* If TREF(ok_to_see_statsdb_regs) is FALSE, "gd_load" would have squished "n_regions"	\
	 * into half of the original (by removing the stats regions) so account for that below.	\
	 */											\
	assert(dbindx < (2 * owningGd->n_regions));						\
	BASEDBREG = &(STATSDBREG)->owning_gd->regions[dbindx];					\
} MBEND
#define BASEDBREG_TO_STATSDBREG(BASEDBREG, STATSDBREG) STATSDBREG_TO_BASEDBREG(BASEDBREG, STATSDBREG)

/* If an attempt to initialize a statsDB occurs inside a transaction (dollar_tlevel is non-zero), allocate one of these
 * blocks and queue it to defer initialization until the transaction (outer-most level) is committed or abandoned.
 */
typedef struct statsDB_diqe				/* Deferred init queue element */
{
	struct statsDB_diqe	*next;			/* Next of these blocks on queue */
	struct gd_region_struct	*baseDBreg;		/* base region ptr */
	struct gd_region_struct	*statsDBreg;		/* statsDB region ptr */
} statsDB_deferred_init_que_elem;

/* To enable debugging macro (output to console) uncomment the following #define */
/* #define DEBUG_RESERVEDDB */
#ifdef DEBUG_RESERVEDDB
# define DBGRDB(x) DBGFPF(x)
# define DBGRDB_ONLY(x) x
#else
# define DBGRDB(x)
# define DBGRDB_ONLY(x)
#endif
/* Debugging macro that supresses unlink/recreate of statsDB file if fail to create it (orphaned file). To use this,
 * uncomment define below.
 */
/* #define BYPASS_UNLINK_RECREATE_STATSDB */

#endif
