/****************************************************************
 *								*
 *	Copyright 2005, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSDBVER_H_INCLUDED
#define GDSDBVER_H_INCLUDED
/* Database version related definitions */

/* Values for bytes 10 and 11 of the GDS Label (first field in the database file header) */
#ifdef VMS
#  define GDS_V20	"02"
#  define GDS_X21	"03"
#  define GDS_V23	"04"
#  define GDS_V24	"05"
#  define GDS_V25	"06"
#  define GDS_V254	"07"
#  define GDS_V255	"08"
#  define GDS_V30	"09"
#  define GDS_V40	"10"
#  define GDS_V50	"11"
#else
#  define GDS_V40	"02"
#  define GDS_V50	"03"
#endif
#define GDS_CURR	GDS_V50

/* Database major version as an enum quantity. Used to index the dbversion table in mtables.c */
enum db_ver
{
	GDSNOVER = -1,
	GDSV4 = 0,
	GDSV5 = 1,
	GDSV6 = 1, /*GDSV5 and GDSV6 have same value because block format is same for these two version*/
	GDSVLAST
};
#define GDSVCURR ((enum db_ver)(GDSVLAST - 1))

/* Database minor version as an enum quantity. This is an ever increasing number that may skip actual
 * releases as it is only added to when a file-header field is added or changed or if there is a
 * significant API difference in the version. This number can be incremented only by adding an entry
 * at the end, just before GDSMVLAST. Note these entries need corresponding updates in
 * db_auto_upgrade.c.
 */
enum mdb_ver
{
	GDSMV4,		/* Applies to all V4 versions (no minor versions defined) */
	GDSMV50000,
	GDSMV51000,	/* Multi-site available (for databases created by V51000 - see V51000ALT */
	GDSMV51000ALT,	/* Upgrade from a previous version upgraded to this value for V51000 due to bug */
	GDSMV52000,	/* Unicode .. no real header changes but db contents could be unusable by previous versions */
	GDSMV53000,	/* M-Itanium release. secshr_ops_array and index is been copied from sgmnt_data to node_local. */
	GDSMV53003,	/* ZSHOW "G" release: Db Statistics rearranged in file header */
	GDSMV53004,	/* New fields(is_encrypted, encryption_hash) for encryption */
	GDSMV54000,	/* New fields(db_trigger_cycle) for triggers */
	GDSMV54002,	/* New statistical counter field for ZTRIGGER command */
	GDSMV54002B,	/* New fields(turn_around_point, jnl_eovtn) for backward recovery */
	GDSMV55000,	/* New fields(strm_reg_seqno, save_strm_reg_seqno, intrpt_recov_resync_strm_seqno)
			 * 	for supplementary instances.
			 * New fields(before_trunc_total_blks, after_trunc_total_blks, before_trunc_free_blocks
			 *	before_trunc_file_size) for fixing interrupted MUPIP REORG -TRUNCATE.
			 */
	GDSMV60000,	/* New freeze_on_fail field for anticipatory freeze; the wc_blocked field moved to shared memory */
	GDSMV60001,
	GDSMV60002,	/* New field mutex_spin_parms.mutex_que_entry_space_size for configurable mutex queue size */
	GDSMVLAST
};
#define GDSMVCURR ((enum mdb_ver)(GDSMVLAST - 1))

#endif
