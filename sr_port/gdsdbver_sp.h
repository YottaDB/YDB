/****************************************************************
 *								*
 * Copyright (c) 2015-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
ENUM_ENTRY(GDSMV4),		/* 0  - Applies to all V4 versions (no minor versions defined) */
ENUM_ENTRY(GDSMV50000),		/* 1  - minor versions introduced */
ENUM_ENTRY(GDSMV51000),		/* 2  - multi-site available (for databases created by V51000 - see V51000ALT */
ENUM_ENTRY(GDSMV51000ALT),	/* 3  - upgrade from a previous version upgraded to this value for V51000 due to bug */
ENUM_ENTRY(GDSMV52000),		/* 4  - UTF8 .. no real header changes but db contents could be unusable by previous versions */
ENUM_ENTRY(GDSMV53000),		/* 5  - M-Itanium release. secshr_ops_array and index copied from sgmnt_data to node_local. */
ENUM_ENTRY(GDSMV53003),		/* 6  - ZSHOW "G" release: Db Statistics rearranged in file header */
ENUM_ENTRY(GDSMV53004),		/* 7  - new fields(is_encrypted, encryption_hash) for encryption */
ENUM_ENTRY(GDSMV54000),		/* 8  - ew fields(db_trigger_cycle) for triggers */
ENUM_ENTRY(GDSMV54002),		/* 9  - new statistical counter field for ZTRIGGER command */
ENUM_ENTRY(GDSMV54002B),	/* 10 - new fields(turn_around_point, jnl_eovtn) for backward recovery */
ENUM_ENTRY(GDSMV55000),		/* 11 - new fields(strm_reg_seqno, save_strm_reg_seqno, intrpt_recov_resync_strm_seqno)
				 * for supplementary instances.
				 * New fields(before_trunc_total_blks, after_trunc_total_blks, before_trunc_free_blocks
				 * before_trunc_file_size) for fixing interrupted MUPIP REORG -TRUNCATE.
				 */
<<<<<<< HEAD
ENUM_ENTRY(GDSMV60000),		/* New freeze_on_fail field for anticipatory freeze; the wc_blocked field moved to shared memory */
ENUM_ENTRY(GDSMV60001),
ENUM_ENTRY(GDSMV60002),		/* New field mutex_spin_parms.mutex_que_entry_space_size for configurable mutex queue size */
ENUM_ENTRY(GDSMV62001),		/* New field hasht_upgrade_needed for ^#t upgrade */
ENUM_ENTRY(GDSMV62002),		/* New field defer_allocate needed for database file preallocation and ^#t upgrade */
ENUM_ENTRY(GDSMV63000),		/* New field non_null_iv to indicate IV mode for encrypted blocks */
ENUM_ENTRY(GDSMV63000A),	/* Move fields ftok_counter_halted and access_counter_halted from fileheader to nodelocal */
ENUM_ENTRY(GDSMV63001),		/* New "asyncio" option; New reservedDBFlags field */
ENUM_ENTRY(GDSMV63003),		/* New field "read_only" to indicate a read-only database */
ENUM_ENTRY(GDSMV63007),		/* Reuse abandoned field for use controlled stable flush_trigger_top.
				 * Can also correspond to ENUM_ENTRY(GDSMR122) since this enum value was used by both
				 * YottaDB r1.22 and GT.M V6.3-007. In YottaDB r1.24, no db format changes happened.
				 * And in YottaDB r1.26, filler space was introduced (GDSMVFILLER1 etc.) so this use
				 * by GT.M and YottaDB will not happen. Treat GDSMR122 as a special case == GDSMV63007.
				 * In GDSMR122, this is what happened.
				 *	New field "reorg_sleep_nsec" to slow down reorg update rate (e.g. reduce restarts) by user
				 */
ENUM_ENTRY(GDSMV63012),		/* New fullblklwrt option */
ENUM_ENTRY(GDSMV63014),		/* GTM-8863 stats added to file header: GVSTATS moved, upsized */
ENUM_ENTRY(GDSMV70000),		/* Changed GT.M to use 64-bit block numbers, required significant changes to the header*/
ENUM_ENTRY(GDSMVFILLER4),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER5),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER6),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER7),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER8),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER9),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER10),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER11),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER12),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER13),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER14),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER15),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER16),	/* Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMR126),		/* Includes GDSMV63007 */
ENUM_ENTRY(GDSMR130),
ENUM_ENTRY(GDSMR134),		/* New field "max_procs" records max concurrent processes accessing database */
ENUM_ENTRY(GDSMR136),		/* Includes GDSMV63012 and GDSMV63014.
				 * Note: As part of V6.3-014 merge, the following fields in the file header had to be moved
				 * because GT.M changes encroached on a filler section where those YottaDB fields used to be.
				 *  a) max_procs
				 *  b) reorg_sleep_nsec
				 */
ENUM_ENTRY(GDSMR200),		/* Includes GDSMV70000 */
=======
ENUM_ENTRY(GDSMV60000),		/* 12 - new freeze_on_fail field for anticipatory freeze; wc_blocked field moved to shared memory */
ENUM_ENTRY(GDSMV60001),		/* 13 */
ENUM_ENTRY(GDSMV60002),		/* 14 - new field mutex_spin_parms.mutex_que_entry_space_size for configurable mutex queue size */
ENUM_ENTRY(GDSMV62001),		/* 15 - New field hasht_upgrade_needed for ^#t upgrade */
ENUM_ENTRY(GDSMV62002),		/* 16 - new field defer_allocate needed for database file preallocation and ^#t upgrade */
ENUM_ENTRY(GDSMV63000),		/* 17 - new field non_null_iv to indicate IV mode for encrypted blocks */
ENUM_ENTRY(GDSMV63000A),	/* 18 - move fields ftok_counter_halted and access_counter_halted from fileheader to nodelocal */
ENUM_ENTRY(GDSMV63001),		/* 19 - new "asyncio" option; New reservedDBFlags field */
ENUM_ENTRY(GDSMV63003),		/* 20 - new field read_only to indicate a read-only database */
ENUM_ENTRY(GDSMV63007),		/* 21 - reuse abandoned field for use controlled stable flush_trigger_top */
ENUM_ENTRY(GDSMV63012),		/* 22 - new fullblklwrt option */
ENUM_ENTRY(GDSMV63014),		/* 23 - GTM-8863 stats added to file header: GVSTATS moved, upsized */
ENUM_ENTRY(GDSMV63015),		/* 24 - safety entry in case we are forced to release another V6.3 version */
ENUM_ENTRY(GDSMV70000),		/* 25 - Changed GT.M to use 64-bit block numbers, required significant changes to the header*/
ENUM_ENTRY(GDSMV70001),		/* 26 - GTM-9131 new statsdb_allocation option & GTM-8681 Backup Timestamp in file header */
>>>>>>> 52a92dfd (GT.M V7.0-001)
ENUM_ENTRY(GDSMVLAST)
