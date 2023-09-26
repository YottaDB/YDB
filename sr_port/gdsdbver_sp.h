/****************************************************************
 *								*
 * Copyright (c) 2015-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
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
				 *      for supplementary instances.
				 *      New fields(before_trunc_total_blks, after_trunc_total_blks, before_trunc_free_blocks
				 *      before_trunc_file_size) for fixing interrupted MUPIP REORG -TRUNCATE.
				 */
ENUM_ENTRY(GDSMV60000),		/* 12 - new freeze_on_fail field for anticipatory freeze; the wc_blocked field moved to
				 *      shared memory
				 */
ENUM_ENTRY(GDSMV60001),		/* 13 */
ENUM_ENTRY(GDSMV60002),		/* 14 - new field mutex_spin_parms.mutex_que_entry_space_size for configurable mutex queue size */
ENUM_ENTRY(GDSMV62001),		/* 15 - new field hasht_upgrade_needed for ^#t upgrade */
ENUM_ENTRY(GDSMV62002),		/* 16 - new field defer_allocate needed for database file preallocation and ^#t upgrade */
ENUM_ENTRY(GDSMV63000),		/* 17 - new field non_null_iv to indicate IV mode for encrypted blocks */
ENUM_ENTRY(GDSMV63000A),	/* 18 - move fields ftok_counter_halted and access_counter_halted from fileheader to nodelocal */
ENUM_ENTRY(GDSMV63001),		/* 19 - new "asyncio" option; New reservedDBFlags field */
ENUM_ENTRY(GDSMV63003),		/* 20 - new field "read_only" to indicate a read-only database */
ENUM_ENTRY(GDSMV63007),		/* 21 - reuse abandoned field for use controlled stable flush_trigger_top.
				 *      Can also correspond to ENUM_ENTRY(GDSMR122) since this enum value was used by both
				 *      YottaDB r1.22 and GT.M V6.3-007. In YottaDB r1.24, no db format changes happened.
				 *      And in YottaDB r1.26, filler space was introduced (GDSMVFILLER1 etc.) so this use
				 *      by GT.M and YottaDB will not happen. Treat GDSMR122 as equal to GDSMV63007 because
				 *      below is what happened in GDSMR122.
				 *      New field "reorg_sleep_nsec" to slow down reorg update rate (e.g. reduce restarts) by user
				 */
ENUM_ENTRY(GDSMV63012),		/* 22 - new fullblklwrt option */
ENUM_ENTRY(GDSMV63014),		/* 23 - GTM-8863 stats added to file header: GVSTATS moved, upsized */
ENUM_ENTRY(GDSMV63015),		/* 24 - safety entry in case GT.M needs to release another V6.3 version */
ENUM_ENTRY(GDSMV70000),		/* 25 - Changed GT.M to use 64-bit block numbers, required significant changes to the header */
ENUM_ENTRY(GDSMV70001),		/* 26 - GTM-9131 new statsdb_allocation option & GTM-8681 Backup Timestamp in file header */
ENUM_ENTRY(GDSMV70002),		/* 27 - GTM-9426 - Automatically split database blocks based upon restarts... */
ENUM_ENTRY(GDSMV71001),		/* 28 - Change default proactive block split threshhold */
<<<<<<< HEAD
ENUM_ENTRY(GDSMVFILLER29),	/* 29 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER30),	/* 30 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER31),	/* 31 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER32),	/* 32 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER33),	/* 33 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER34),	/* 34 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER35),	/* 35 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER36),	/* 36 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER37),	/* 37 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMR126),		/* 38 - Includes GDSMV63007 */
ENUM_ENTRY(GDSMR130),		/* 39 */
ENUM_ENTRY(GDSMR134),		/* 40 - New field "max_procs" records max concurrent processes accessing database */
ENUM_ENTRY(GDSMR136),		/* 41 - Includes GDSMV63012 and GDSMV63014.
				 *      Note: As part of V6.3-014 merge, the following fields in the file header had to be moved
				 *      because GT.M changes encroached on a filler section where those YottaDB fields used to be.
				 *      a) max_procs
				 *      b) reorg_sleep_nsec
				 */
ENUM_ENTRY(GDSMR200_V70000),	/* 42 - YottaDB r2.00 which includes GDSMV70000 */
ENUM_ENTRY(GDSMR200_V70001),	/* 43 - YottaDB r2.00 which includes GDSMV70001 */
ENUM_ENTRY(GDSMR202_V70002),	/* 44 - YottaDB r2.02 which includes GDSMV70002 */
ENUM_ENTRY(GDSMVFILLER45),	/* 45 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER46),	/* 46 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER47),	/* 47 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER48),	/* 48 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER49),	/* 49 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER50),	/* 50 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER51),	/* 51 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER52),	/* 52 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER53),	/* 53 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER54),	/* 54 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER55),	/* 55 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER56),	/* 56 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER57),	/* 57 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER58),	/* 58 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER59),	/* 59 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER60),	/* 60 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER61),	/* 61 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER62),	/* 62 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER63),	/* 63 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER64),	/* 64 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER65),	/* 65 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER66),	/* 66 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER67),	/* 67 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER68),	/* 68 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER69),	/* 69 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER70),	/* 70 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER71),	/* 71 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER72),	/* 72 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER73),	/* 73 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER74),	/* 74 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER75),	/* 75 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER76),	/* 76 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER77),	/* 77 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER78),	/* 78 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER79),	/* 79 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER80),	/* 80 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER81),	/* 81 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER82),	/* 82 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER83),	/* 83 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER84),	/* 84 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER85),	/* 85 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER86),	/* 86 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER87),	/* 87 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER88),	/* 88 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER89),	/* 89 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER90),	/* 90 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER91),	/* 91 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER92),	/* 92 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER93),	/* 93 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER94),	/* 94 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER95),	/* 95 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER96),	/* 96 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER97),	/* 97 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER98),	/* 98 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER99),	/* 99 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER100),	/* 100 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER101),	/* 101 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER102),	/* 102 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER103),	/* 103 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER104),	/* 104 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER105),	/* 105 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER106),	/* 106 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER107),	/* 107 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER108),	/* 108 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER109),	/* 109 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER110),	/* 110 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER111),	/* 111 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER112),	/* 112 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER113),	/* 113 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER114),	/* 114 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER115),	/* 115 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER116),	/* 116 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER117),	/* 117 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER118),	/* 118 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER119),	/* 119 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER120),	/* 120 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER121),	/* 121 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER122),	/* 122 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER123),	/* 123 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER124),	/* 124 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER125),	/* 125 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER126),	/* 126 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER127),	/* 127 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER128),	/* 128 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER129),	/* 129 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER130),	/* 130 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER131),	/* 131 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER132),	/* 132 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER133),	/* 133 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER134),	/* 134 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER135),	/* 135 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER136),	/* 136 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER137),	/* 137 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER138),	/* 138 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER139),	/* 139 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER140),	/* 140 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER141),	/* 141 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER142),	/* 142 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER143),	/* 143 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER144),	/* 144 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER145),	/* 145 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER146),	/* 146 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER147),	/* 147 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER148),	/* 148 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER149),	/* 149 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER150),	/* 150 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER151),	/* 151 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER152),	/* 152 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER153),	/* 153 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER154),	/* 154 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER155),	/* 155 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER156),	/* 156 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER157),	/* 157 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER158),	/* 158 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER159),	/* 159 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER160),	/* 160 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER161),	/* 161 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER162),	/* 162 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER163),	/* 163 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER164),	/* 164 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER165),	/* 165 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER166),	/* 166 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER167),	/* 167 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER168),	/* 168 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER169),	/* 169 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER170),	/* 170 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER171),	/* 171 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER172),	/* 172 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER173),	/* 173 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER174),	/* 174 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER175),	/* 175 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER176),	/* 176 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER177),	/* 177 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER178),	/* 178 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER179),	/* 179 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER180),	/* 180 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER181),	/* 181 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER182),	/* 182 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER183),	/* 183 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER184),	/* 184 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER185),	/* 185 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER186),	/* 186 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER187),	/* 187 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER188),	/* 188 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER189),	/* 189 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER190),	/* 190 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER191),	/* 191 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER192),	/* 192 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER193),	/* 193 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER194),	/* 194 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER195),	/* 195 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER196),	/* 196 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER197),	/* 197 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER198),	/* 198 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER199),	/* 199 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER200),	/* 200 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER201),	/* 201 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER202),	/* 202 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER203),	/* 203 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER204),	/* 204 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER205),	/* 205 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER206),	/* 206 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER207),	/* 207 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER208),	/* 208 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER209),	/* 209 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER210),	/* 210 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER211),	/* 211 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER212),	/* 212 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER213),	/* 213 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER214),	/* 214 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER215),	/* 215 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER216),	/* 216 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER217),	/* 217 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER218),	/* 218 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER219),	/* 219 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER220),	/* 220 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER221),	/* 221 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER222),	/* 222 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER223),	/* 223 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER224),	/* 224 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER225),	/* 225 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER226),	/* 226 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER227),	/* 227 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER228),	/* 228 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER229),	/* 229 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER230),	/* 230 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER231),	/* 231 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER232),	/* 232 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER233),	/* 233 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER234),	/* 234 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER235),	/* 235 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER236),	/* 236 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER237),	/* 237 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER238),	/* 238 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER239),	/* 239 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER240),	/* 240 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER241),	/* 241 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER242),	/* 242 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER243),	/* 243 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER244),	/* 244 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER245),	/* 245 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER246),	/* 246 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER247),	/* 247 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER248),	/* 248 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER249),	/* 249 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER250),	/* 250 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER251),	/* 251 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER252),	/* 252 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER253),	/* 253 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER254),	/* 254 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMVFILLER255),	/* 255 - Space reserved for GT.M changes to minor db format */
ENUM_ENTRY(GDSMR204_V71001),	/* 256 - YottaDB r2.04 which includes GDSMV71001 */
=======
ENUM_ENTRY(GDSMV71002),		/* 29 - Make full use of the index reserved bytes field */
>>>>>>> fdfdea1e (GT.M V7.1-002)
ENUM_ENTRY(GDSMVLAST)
