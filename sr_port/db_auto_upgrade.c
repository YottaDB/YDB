/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2025 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "io.h"
#include "jnl.h"
#include "mutex.h"
#include "wcs_phase2_commit_wait.h"
#include "gvcst_protos.h"	/* for gvcst_init_sysops prototype */
#include "db_write_eof_block.h"

#define	MOVE_GVSTATS_REC_FROM_OLD_HDR_TO_NODE_LOCAL(CSA, CSD)						\
{													\
	/* Copy the 62 pre GTM-8863 stats from the old header location to node_local.			\
	 * They will be copied back to the new header location at database rundown time.		\
	 */												\
	node_local_ptr_t	cnl;									\
	gtm_uint64_t		*old_stats, *new_stats;							\
													\
	cnl = CSA->nl;											\
	old_stats = (gtm_uint64_t *) &CSD->gvstats_rec_old_now_filler[0];				\
	new_stats = (gtm_uint64_t *) &cnl->gvstats_rec;							\
	for (i = 0; i < SIZEOF(CSD->gvstats_rec_old_now_filler)/SIZEOF(gtm_uint64_t); i++)		\
		new_stats[i] = old_stats[i];								\
}

GBLREF  boolean_t       dse_running;

error_def(ERR_DBBADUPGRDSTATE);

void db_auto_upgrade(gd_region *reg)
{
	/* detect uninitialized file header fields for this version of GT.M and do a mini auto-upgrade, initializing such fields
	 * to default values in the most recent GT.M version
	 */
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	off_t			new_eof;
	unix_db_info		*udi;
#	ifdef DEBUG
	gtm_uint64_t		file_size;
#	endif
	int 			i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != reg);
	if (NULL == reg)
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	if (NULL == csd)
		return;
	assert(0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1)); /* This function is only for V7 DBs */
	if (0 > csd->mutex_spin_parms.mutex_hard_spin_count)
		csd->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	if (0 > csd->mutex_spin_parms.mutex_sleep_spin_count)
		csd->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	/* zero is a legitimate value for csd->mutex_spin_parms.mutex_spin_sleep_mask; so can't detect if need re-initialization */
	INIT_NUM_CRIT_ENTRY_IF_NEEDED(csd);

	/* Auto upgrade based on minor database version number. This code currently only does auto upgrade and does not
	 * do auto downgrade although that certainly is possible to implement if necessary. For now, if the current version
	 * is at a lower level than the minor db version, we do nothing.
	 *
	 * Note the purpose of the minor_dbver field is so that some part of GT.M (either runtime, or conversion utility) some
	 * time and several versions down the road from now knows by looking at this field what fields in the fileheader are
	 * valid so it is important that the minor db version is incremented each time the fileheader is updated and that
	 * this routine is correspondingly updated to initialize the new fields in prior versions of the header. SE 5/2006.
	 */
	if (GDSMVCURR > csd->minor_dbver)
	{	/* In general, the method for adding new versions is:
		 * 1) If there are no automatic updates for this version, it is optional to add the version to the switch
		 *    statement below. Those there are more for example at this time (through V53000).
		 * 2) Update (or add) a case for the previous version to update any necessary fields.
		 */
		if (!csd->opened_by_gtmv53 && !csd->db_got_to_v5_once)
		{
			csd->opened_by_gtmv53 = TRUE;
			/* This is a case of a database that has been used by a pre-V53 version of GT.M that did not contain
			 * the fix (C9H07-002873). At this point, the database might contain RECYCLED blocks that are a mix of
			 *	a) Those blocks that were RECYCLED at the time of the MUPIP UPGRADE from V4 to V5.
			 *	b) Those blocks that became RECYCLED due to M-kills in V5.
			 * It is only (a) that we have to mark as FREE as it might contain too-full v4 format blocks. But there
			 * is no way to distinguish the two. So we mark both (a) and (b) as FREE. This will mean no PBLKs written
			 * for (b) and hence no backward journal recovery possible to a point before the start of the REORG UPGRADE.
			 * We force a MUPIP REORG UPGRADE rerun (to mark RECYCLED blocks FREE) by setting fully_upgraded to FALSE.
			 * Note that this does not need to be done for databases created by a V5 version (C9I05-002987).
			 */
			if (MASTER_MAP_SIZE_V4 == csd->master_map_len)
			{
				csd->fully_upgraded = FALSE;
				csd->reorg_upgrd_dwngrd_restart_block = 0;	/* reorg upgrade should restart from block 0 */
				/* Ensure reorg_db_fmt_start_tn and desired_db_format_tn are set to different
				 * values so fresh reorg upgrade can set fully_upgraded to TRUE once it is done.
				 */
				csd->reorg_db_fmt_start_tn = 0;
				csd->desired_db_format_tn = 1;
			} else
				csd->db_got_to_v5_once = TRUE;	/* db was created by V5 so safe to set this */
		}
		/* When adding a new minor version, the following template should be maintained
		 * a) Remove the penultimate 'break'
		 * b) Remove the assert(FALSE) in the last case (most recent minor version)
		 * c) If there are any file header fields added in the new minor version, initialize the fields to default values
		 *    in the last case
		 * d) Add a new case with the new minor version
		 * e) Add assert(FALSE) and break (like it was before)
		 */
		switch (csd->minor_dbver)
		{	/* Note that handling for any fields introduced in a version will not go in the "switch-case" block
			 * of code introduced for the new version but will go in the PREVIOUS "switch-case" block.
			 */
			case GDSMV63015: /* because we added this in case, after its number had already been used by V70000 */
			case GDSMV70000:
			case GDSMR200_V70000:
				/* Do GT.M V7.0-001 related auto upgrade operations */
				csd->creation_db_ver = GDSVCURR;     /* required due to renumbering of db_vers due to V6 upgrades */
				csd->creation_mdb_ver = GDSMV70000;  /* required due to adding protection for possible V63015 */
				csd->desired_db_format = GDSVCURR;   /* required due to renumbering of db_vers due to V6 upgrades */
				csd->minor_dbver = GDSMV70000;       /* required due to adding protection for possible V63015 */
				csd->statsdb_allocation = STATSDB_ALLOCATION;
				csd->offset = 0;
				csd->max_rec = 0;
				csd->last_start_backup = (gtm_timet) 0; /* GTM-8681, but this is default value anyway */
				/*the next four lines are because gvcst_init_sysops might have used an outdated assumption */
				csd->max_update_array_size = csd->max_non_bm_update_array_size
					= (int4)(ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE));
				csd->max_update_array_size
					+= (int4)(ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE));
			case GDSMV70001:
			case GDSMV70002:
<<<<<<< HEAD
				/* GT.M V70002 added proactive block split option.
				 * GT.M V71001 changed proactive block split default.
				 * So just one line below to incorporate both those changes in one shot.
				 */
				csd->problksplit = DEFAULT_PROBLKSPLIT;
			case GDSMV71001:
				if (GDSMR200_V70000 != csd->minor_dbver)
				{
					/* Do YottaDB r2.00 related auto upgrade operations.
					 * This is a GT.M V7.0-000 database. Apply all file header upgrade changes that happened
					 * in YottaDB r1.2* and r1.3* releases (i.e. all releases less than YottaDB r2.0 release).
					 */
					/* Handle GDSMR122 (aka GDSMV63007) case */
					/* ----------------------------------- */
					/* YottaDB r1.22 introduced "reorg_sleep_nsec" to slow down reorg update rate by user */
					csd->reorg_sleep_nsec = 0;
					/* ----------------------------------- */
					/* YottaDB r1.30 changed "flush_time" from milliseconds to nanoseconds to support
					 * nanosecond timers. In GT.M, "flush_time" is an array of 2 4-byte quantities but only
					 * the first 4-byte is used. Whereas in YottaDB, "flush_time" is an 8-byte quantity.
					 * Therefore, nullify the second 4-byte before doing the below multiplication to
					 * arrive at the 8-byte value.
					 */
					csd->flush_time = (csd->flush_time & (((uint8)1 << 32) - 1)) * NANOSECS_IN_MSEC;
					/* Note: This is a little-endian solution and will need to be modified if we
					 * ever support big-endian.
					 */
					if (0 == csd->flush_trigger_top)
						csd->flush_trigger_top = FLUSH_FACTOR(csd->n_bts);
					/* Note: This ensures that the csd->flush_trigger_top field added in GT.M V6.3-007 is
					 * set if the old db_minorver is GDSMR122.
					 */
					/* ----------------------------------- */
					/* YottaDB r1.34 introduced "max_procs" which records the maximum number of
					 * processes accessing the database to measure system load.
					 */
					csd->max_procs.cnt = 0;
					csd->max_procs.time = 0;
				}
			case GDSMR200_V70001:
			case GDSMR202_V70002:
				/* GT.M V70002 added proactive block split option.
				 * GT.M V71001 changed proactive block split default.
				 * So just one line below to incorporate both those changes in one shot.
				 */
				csd->problksplit = DEFAULT_PROBLKSPLIT;
				break;		/* so a new "case" needs to be added BEFORE the assert. */
			case GDSMR204_V71001:
				/* When adding a new minor version, the following template should be maintained
				 * 1) Remove the penultimate 'break' (i.e. "break" in the PREVIOUS "case" block.
				 * 2) If there are any file header fields added in the new minor version, initialize the fields
				 *    to default values in THIS/CURRENT case (i.e. above this comment block). Do add a
				 *    "break" for THIS/CURRENT "case" block.
				 * 3) Then, add a NEW "case" statement with the new minor version. This comment block and
				 *    the below 3 lines become part of that "case".
				 * 4) For every GT.M minor ver added since the previous YottaDB release, duplicate the relevant
				 *    auto upgrade code in this case block. This is needed as the GT.M GDSMV* (e.g. GDSMV63012)
				 *    values will be way less than the older YottaDB GDSMVCURR value (e.g. in case of YottaDB
				 *    r1.32) and so those GT.M switch/case code paths above will not be reached for upgrades
				 *    from an older YottaDB release to a newer YottaDB release.
				 */
				/* Nothing to do for this version since it is GDSMVCURR for now. */
				assert(FALSE);		/* When this assert fails, it means a new GDSMV* was created, */
				break;			/* 	so a new "case" needs to be added BEFORE the assert. */
			/* Remove the below cases one by one as later GT.M versions use up these minor db version enum values. */
			case GDSMVFILLER29:
			case GDSMVFILLER30:
			case GDSMVFILLER31:
			case GDSMVFILLER32:
			case GDSMVFILLER33:
			case GDSMVFILLER34:
			case GDSMVFILLER35:
			case GDSMVFILLER36:
			case GDSMVFILLER37:
			case GDSMVFILLER45:
			case GDSMVFILLER46:
			case GDSMVFILLER47:
			case GDSMVFILLER48:
			case GDSMVFILLER49:
			case GDSMVFILLER50:
			case GDSMVFILLER51:
			case GDSMVFILLER52:
			case GDSMVFILLER53:
			case GDSMVFILLER54:
			case GDSMVFILLER55:
			case GDSMVFILLER56:
			case GDSMVFILLER57:
			case GDSMVFILLER58:
			case GDSMVFILLER59:
			case GDSMVFILLER60:
			case GDSMVFILLER61:
			case GDSMVFILLER62:
			case GDSMVFILLER63:
			case GDSMVFILLER64:
			case GDSMVFILLER65:
			case GDSMVFILLER66:
			case GDSMVFILLER67:
			case GDSMVFILLER68:
			case GDSMVFILLER69:
			case GDSMVFILLER70:
			case GDSMVFILLER71:
			case GDSMVFILLER72:
			case GDSMVFILLER73:
			case GDSMVFILLER74:
			case GDSMVFILLER75:
			case GDSMVFILLER76:
			case GDSMVFILLER77:
			case GDSMVFILLER78:
			case GDSMVFILLER79:
			case GDSMVFILLER80:
			case GDSMVFILLER81:
			case GDSMVFILLER82:
			case GDSMVFILLER83:
			case GDSMVFILLER84:
			case GDSMVFILLER85:
			case GDSMVFILLER86:
			case GDSMVFILLER87:
			case GDSMVFILLER88:
			case GDSMVFILLER89:
			case GDSMVFILLER90:
			case GDSMVFILLER91:
			case GDSMVFILLER92:
			case GDSMVFILLER93:
			case GDSMVFILLER94:
			case GDSMVFILLER95:
			case GDSMVFILLER96:
			case GDSMVFILLER97:
			case GDSMVFILLER98:
			case GDSMVFILLER99:
			case GDSMVFILLER100:
			case GDSMVFILLER101:
			case GDSMVFILLER102:
			case GDSMVFILLER103:
			case GDSMVFILLER104:
			case GDSMVFILLER105:
			case GDSMVFILLER106:
			case GDSMVFILLER107:
			case GDSMVFILLER108:
			case GDSMVFILLER109:
			case GDSMVFILLER110:
			case GDSMVFILLER111:
			case GDSMVFILLER112:
			case GDSMVFILLER113:
			case GDSMVFILLER114:
			case GDSMVFILLER115:
			case GDSMVFILLER116:
			case GDSMVFILLER117:
			case GDSMVFILLER118:
			case GDSMVFILLER119:
			case GDSMVFILLER120:
			case GDSMVFILLER121:
			case GDSMVFILLER122:
			case GDSMVFILLER123:
			case GDSMVFILLER124:
			case GDSMVFILLER125:
			case GDSMVFILLER126:
			case GDSMVFILLER127:
			case GDSMVFILLER128:
			case GDSMVFILLER129:
			case GDSMVFILLER130:
			case GDSMVFILLER131:
			case GDSMVFILLER132:
			case GDSMVFILLER133:
			case GDSMVFILLER134:
			case GDSMVFILLER135:
			case GDSMVFILLER136:
			case GDSMVFILLER137:
			case GDSMVFILLER138:
			case GDSMVFILLER139:
			case GDSMVFILLER140:
			case GDSMVFILLER141:
			case GDSMVFILLER142:
			case GDSMVFILLER143:
			case GDSMVFILLER144:
			case GDSMVFILLER145:
			case GDSMVFILLER146:
			case GDSMVFILLER147:
			case GDSMVFILLER148:
			case GDSMVFILLER149:
			case GDSMVFILLER150:
			case GDSMVFILLER151:
			case GDSMVFILLER152:
			case GDSMVFILLER153:
			case GDSMVFILLER154:
			case GDSMVFILLER155:
			case GDSMVFILLER156:
			case GDSMVFILLER157:
			case GDSMVFILLER158:
			case GDSMVFILLER159:
			case GDSMVFILLER160:
			case GDSMVFILLER161:
			case GDSMVFILLER162:
			case GDSMVFILLER163:
			case GDSMVFILLER164:
			case GDSMVFILLER165:
			case GDSMVFILLER166:
			case GDSMVFILLER167:
			case GDSMVFILLER168:
			case GDSMVFILLER169:
			case GDSMVFILLER170:
			case GDSMVFILLER171:
			case GDSMVFILLER172:
			case GDSMVFILLER173:
			case GDSMVFILLER174:
			case GDSMVFILLER175:
			case GDSMVFILLER176:
			case GDSMVFILLER177:
			case GDSMVFILLER178:
			case GDSMVFILLER179:
			case GDSMVFILLER180:
			case GDSMVFILLER181:
			case GDSMVFILLER182:
			case GDSMVFILLER183:
			case GDSMVFILLER184:
			case GDSMVFILLER185:
			case GDSMVFILLER186:
			case GDSMVFILLER187:
			case GDSMVFILLER188:
			case GDSMVFILLER189:
			case GDSMVFILLER190:
			case GDSMVFILLER191:
			case GDSMVFILLER192:
			case GDSMVFILLER193:
			case GDSMVFILLER194:
			case GDSMVFILLER195:
			case GDSMVFILLER196:
			case GDSMVFILLER197:
			case GDSMVFILLER198:
			case GDSMVFILLER199:
			case GDSMVFILLER200:
			case GDSMVFILLER201:
			case GDSMVFILLER202:
			case GDSMVFILLER203:
			case GDSMVFILLER204:
			case GDSMVFILLER205:
			case GDSMVFILLER206:
			case GDSMVFILLER207:
			case GDSMVFILLER208:
			case GDSMVFILLER209:
			case GDSMVFILLER210:
			case GDSMVFILLER211:
			case GDSMVFILLER212:
			case GDSMVFILLER213:
			case GDSMVFILLER214:
			case GDSMVFILLER215:
			case GDSMVFILLER216:
			case GDSMVFILLER217:
			case GDSMVFILLER218:
			case GDSMVFILLER219:
			case GDSMVFILLER220:
			case GDSMVFILLER221:
			case GDSMVFILLER222:
			case GDSMVFILLER223:
			case GDSMVFILLER224:
			case GDSMVFILLER225:
			case GDSMVFILLER226:
			case GDSMVFILLER227:
			case GDSMVFILLER228:
			case GDSMVFILLER229:
			case GDSMVFILLER230:
			case GDSMVFILLER231:
			case GDSMVFILLER232:
			case GDSMVFILLER233:
			case GDSMVFILLER234:
			case GDSMVFILLER235:
			case GDSMVFILLER236:
			case GDSMVFILLER237:
			case GDSMVFILLER238:
			case GDSMVFILLER239:
			case GDSMVFILLER240:
			case GDSMVFILLER241:
			case GDSMVFILLER242:
			case GDSMVFILLER243:
			case GDSMVFILLER244:
			case GDSMVFILLER245:
			case GDSMVFILLER246:
			case GDSMVFILLER247:
			case GDSMVFILLER248:
			case GDSMVFILLER249:
			case GDSMVFILLER250:
			case GDSMVFILLER251:
			case GDSMVFILLER252:
			case GDSMVFILLER253:
			case GDSMVFILLER254:
			case GDSMVFILLER255:
=======
				/* GT.M V70002 added proactive block split option */
				/* GT.M V71001 changed proactive block split default */
				csd->problksplit = DEFAULT_PROBLKSPLIT;
			case GDSMV71001:
				/* GT.M V7.1-002 implemented index reserved bytes and changed default to zero */
				csd->i_reserved_bytes = 0;
				break; /* so a new "case" needs to be added BEFORE the assert. */
			case GDSMV71002:
				assert(FALSE);
				break;
>>>>>>> fdfdea1e (GT.M V7.1-002)
			default:
				/* Unrecognized version in the header */
				assertpro(FALSE && csd->minor_dbver);
		}
		csd->minor_dbver = GDSMVCURR;
	}
	csd->last_mdb_ver = GDSMVCURR;
	if (csd->fully_upgraded && !csd->db_got_to_v5_once)
	{	/* Database is fully upgraded but the db_got_to_v5_once field says different.
		 * Don't know how that could happen, except with DSE which can change both the database file header fields
		 */
		assert(!dse_running);
		csd->db_got_to_v5_once = TRUE; /* fix it in PRO */
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBBADUPGRDSTATE, 4, REG_LEN_STR(reg), DB_LEN_STR(reg));
	}
	return;
}

void v6_db_auto_upgrade(gd_region *reg)
{
	/* detect uninitialized file header fields for this version of GT.M and do a mini auto-upgrade, initializing such fields
	 * to default values in the last GT.M V6 version
	 */
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	off_t			new_eof;
	unix_db_info		*udi;
#	ifdef DEBUG
	gtm_uint64_t		file_size;
#	endif
	int			i;
	gtm_uint64_t		*old_stats, *new_stats;
	node_local_ptr_t	cnl;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != reg);
	if (NULL == reg)
		return;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	/* Auto upgrade previously done by current ver, skip it. WARNING: Fields auto-upgraded to non-zero
	 * values, like problksplit, need special handling to enabled switch V7 versions working with a V6
	 * database. There is no such special casing at the moment. Future versions will need to address it
	 */
	if ((NULL == csd) || (GDSMVCURR == csd->last_mdb_ver))
		return;
	assert(0 == memcmp(csd->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1)); /* This function is only for V6 DBs */
	if (0 > csd->mutex_spin_parms.mutex_hard_spin_count)
		csd->mutex_spin_parms.mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
	if (0 > csd->mutex_spin_parms.mutex_sleep_spin_count)
		csd->mutex_spin_parms.mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
	/* zero is a legitimate value for csd->mutex_spin_parms.mutex_spin_sleep_mask; so can't detect if need re-initialization */
	INIT_NUM_CRIT_ENTRY_IF_NEEDED(csd);

	/* Auto upgrade based on minor database version number. This code currently only does auto upgrade and does not
	 * do auto downgrade although that certainly is possible to implement if necessary. For now, if the current version
	 * is at a lower level than the minor db version, we do nothing.
	 *
	 * Note the purpose of the minor_dbver field is so that some part of GT.M (either runtime, or conversion utility) some
	 * time and several versions down the road from now knows by looking at this field what fields in the fileheader are
	 * valid so it is important that the minor db version is incremented each time the fileheader is updated and that
	 * this routine is correspondingly updated to initialize the new fields in prior versions of the header. SE 5/2006.
	 */
	if (csd->minor_dbver <= BLK_ID_32_MVER)
	{	/* In general, the method for adding new versions is:
		 * 1) If there are no automatic updates for this version, it is optional to add the version to the switch
		 *    statement below. Those there are more for example at this time (through V53000).
		 * 2) Update (or add) a case for the previous version to update any necessary fields.
		 */
		if (!csd->opened_by_gtmv53 && !csd->db_got_to_v5_once)
		{
			csd->opened_by_gtmv53 = TRUE;
			/* This is a case of a database that has been used by a pre-V53 version of GT.M that did not contain
			 * the fix (C9H07-002873). At this point, the database might contain RECYCLED blocks that are a mix of
			 *	a) Those blocks that were RECYCLED at the time of the MUPIP UPGRADE from V4 to V5.
			 *	b) Those blocks that became RECYCLED due to M-kills in V5.
			 * It is only (a) that we have to mark as FREE as it might contain too-full v4 format blocks. But there
			 * is no way to distinguish the two. So we mark both (a) and (b) as FREE. This will mean no PBLKs written
			 * for (b) and hence no backward journal recovery possible to a point before the start of the REORG UPGRADE.
			 * We force a MUPIP REORG UPGRADE rerun (to mark RECYCLED blocks FREE) by setting fully_upgraded to FALSE.
			 * Note that this does not need to be done for databases created by a V5 version (C9I05-002987).
			 */
			if (MASTER_MAP_SIZE_V4 == csd->master_map_len)
			{
				csd->fully_upgraded = FALSE;
				csd->reorg_upgrd_dwngrd_restart_block = 0;	/* reorg upgrade should restart from block 0 */
				/* Ensure reorg_db_fmt_start_tn and desired_db_format_tn are set to different
				 * values so fresh reorg upgrade can set fully_upgraded to TRUE once it is done.
				 */
				csd->reorg_db_fmt_start_tn = 0;
				csd->desired_db_format_tn = 1;
			} else
				csd->db_got_to_v5_once = TRUE;	/* db was created by V5 so safe to set this */
		}
		/* When adding a new minor version, the following template should be maintained
		 * a) Remove the penultimate 'break'
		 * b) Remove the assert(FALSE) in the last case (most recent minor version)
		 * c) If there are any file header fields added in the new minor version, initialize the fields to default values
		 *    in the last case
		 * d) Add a new case with the new minor version
		 * e) Add assert(FALSE) and break (like it was before)
		 */
		switch (csd->minor_dbver)
		{	/* Note that handling for any fields introduced in a version will not go in the "switch-case" block
			 * of code introduced for the new version but will go in the PREVIOUS "switch-case" block.
			 */
			case GDSMV50000:
			case GDSMV51000ALT:
			case GDSMV51000:		/* Multi-site replication available */
			case GDSMV52000:		/* UTF8 */
			case GDSMV53000:		/* M-Itanium release */
				gvstats_rec_upgrade(csa); /* Move GVSTATS information to new place in file header */
				/* V53002 introduced wcs_phase2_commit_wait_spincnt */
				if (0 == csd->wcs_phase2_commit_wait_spincnt)
					csd->wcs_phase2_commit_wait_spincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;
			case GDSMV53003:		/* ZSHOW "G" release */
				csd->is_encrypted = FALSE;
				memset(csd->encryption_hash, 0, GTMCRYPT_RESERVED_HASH_LEN);
			case GDSMV53004:		/* New encryption fields */
				csd->db_trigger_cycle = 0;
			case GDSMV54000:		/* First trigger version */
			case GDSMV54002:
				/* GT.M V54002B introduced jnl_eov_tn for backward recovery */
				csd->jnl_eovtn = csd->trans_hist.curr_tn;
			case GDSMV54002B:
				/* GT.M V55000 introduced strm_reg_seqno, save_strm_reg_seqno, intrpt_recov_resync_strm_seqno
				 * AND obsoleted dualsite_resync_seqno. For new fields, we are guaranteed they are
				 * zero (in formerly unused sections of the file header) so no need for any initialization.
				 * For obsoleted fields, it would be good to clear them here so we don't run into issues later.
				 */
				csd->filler_seqno = 0;	/* was "dualsite_resync_seqno" in pre-V55000 versions */
				/* In addition, V55000 introduced before_trunc_total_blks for MUPIP REORG -TRUNCATE.
				 * Since it is a new field no initialization necessary.
				 */
			case GDSMV55000:
				csd->freeze_on_fail = FALSE;
				csd->span_node_absent = TRUE;
				csd->maxkeysz_assured = FALSE;
			case GDSMV60000:
			case GDSMV60001:
				/* GT.M V60002 introduced mutex_spin_parms.mutex_que_entry_space_size */
				NUM_CRIT_ENTRY(csd) = DEFAULT_NUM_CRIT_ENTRY;
			case GDSMV60002:
				/* GT.M V62001 introduced ^#t upgrade. Record this pending event in filehdr. */
				csd->hasht_upgrade_needed = TRUE;
			case GDSMV62001:
				/* GT.M V62002 introduced database file preallocation. */
				csd->defer_allocate = TRUE;
				/* GT.M V62002 incremented ^#t label. Record this pending event in filehdr. */
				csd->hasht_upgrade_needed = TRUE;
				/* GT.M V62002 introduced epoch taper */
				csd->epoch_taper = TRUE;
				csd->epoch_taper_time_pct = EPOCH_TAPER_TIME_PCT_DEFAULT;
				csd->epoch_taper_jnl_pct = EPOCH_TAPER_JNL_PCT_DEFAULT;
			case GDSMV62002:
				/* GT.M V63000 introduced non-null IV encryption and encryption on-the-fly. */
				csd->non_null_iv = FALSE;
				csd->encryption_hash_cutoff = UNSTARTED;
				csd->encryption_hash2_start_tn = 0;
				memset(csd->encryption_hash2, 0, GTMCRYPT_RESERVED_HASH_LEN);
				SPIN_SLEEP_MASK(csd) = 0;	/* previously unused, but was 7FF and it should now default to 0 */
			case GDSMV63000:
				/* GT.M V63000A moved ftok_counter_halted and access_counter_halted from filehdr to node_local */
				csd->filler_ftok_counter_halted = FALSE;
				csd->filler_access_counter_halted = FALSE;
			case GDSMV63000A:
				/* GT.M V63001 introduced asyncio but csd->asyncio could be set to TRUE by a MUPIP SET -ASYNCIO
				 * command which did not come through here (because it needs standalone access). Therefore
				 * do not set csd->asyncio to FALSE like is normally done for any newly introduced field.
				 *	csd->asyncio = FALSE;
				 */
				 /* The database file would have a 512-byte EOF block. Enlarge it to be a GDS-block instead. */
				udi = FILE_INFO(reg);
				new_eof = (off_t)BLK_ZERO_OFF(csd->start_vbn) + (off_t)csd->trans_hist.total_blks * csd->blk_size;
				DEBUG_ONLY(file_size = gds_file_size(reg->dyn.addr->file_cntl);)
				/* Note that the last process to access this database in V63000A could have been in the middle of
				 * a database file extension after having extended the database file size but before updating
				 * the total_blks field in the file header etc. In that case, the "new_eof" value would be less
				 * than "file_size". Hence the lack of "==" in the assert check below. In this situation though,
				 * if the process going through "db_auto_upgrade" is a forward or backward MUPIP JOURNAL RECOVER
				 * command, it would eventually invoke "mur_block_count_correct" which would fix this discrepancy
				 * and ensure a later mupip integ is clean (no DBTOTBLK error).
				 *
				 * Also, the below assert does not work with block sizes under 4096 which became the default in
				 * GT.M V6.3-001. Tests that randomize the block size below 4096 will fail the assert. Therefore
				 * modify the assert to account for that.
				 */
				assert(((file_size * DISK_BLOCK_SIZE) >= (new_eof + DISK_BLOCK_SIZE))
						|| ((DISK_BLOCK_SIZE << 3) > csd->blk_size));
				db_write_eof_block(udi, udi->fd, csd->blk_size, new_eof, &TREF(dio_buff));
				/* GT.M V63001 introduced reservedDBFlags */
				csd->reservedDBFlags = 0; /* RDBF_AUTODB = FALSE, RDBF_NOSTATS = FALSE, RDBF_STATSDB = FALSE */
			case GDSMV63001:
				/* GT.M V63003 introduced read-only databases */
				csd->read_only = 0;
			case GDSMV63003:
				/*  GT.M v63007 added stable user control of flush_trigger using flush_trigger_top */
				csd->flush_trigger_top = FLUSH_FACTOR(csd->n_bts);	/* more predictable than flush_trigger */
				/* Note: This also needs to handle the GDSMR122 case (see comment in gdsdbver_sp.h) hence
				 * the r122 related code block below.
				 */
				/* YottaDB r122 introduced "reorg_sleep_nsec" to slow down reorg update rate by user */
				csd->reorg_sleep_nsec = 0;
			case GDSMV63007:	/* Note: This is also the case for GDSMR122 */
				/* GT.M V63012 added fullblkwrt option */
				csd->write_fullblk = 0;
			case GDSMV63012:
				/* GT.M V63014 moved the gvstats section from one location in the file header to another */
				MOVE_GVSTATS_REC_FROM_OLD_HDR_TO_NODE_LOCAL(csa, csd);
			case GDSMV63014:
			case GDSMR126:
				/* YottaDB r130 changed "flush_time" from milliseconds to nanoseconds to support nanosecond
				 * timers. In GT.M, "flush_time" is an array of 2 4-byte quantities but only the first 4-byte
				 * is used. Whereas in YottaDB, "flush_time" is an 8-byte quantity. Therefore, nullify the
				 * second 4-byte before doing the below multiplication to arrive at the 8-byte value.
				 */
				csd->flush_time = (csd->flush_time & (((uint8)1 << 32) - 1)) * NANOSECS_IN_MSEC;
				/* Note: This is a little-endian solution and will need to be modified if we
				 * ever support big-endian.
				 */
				if (0 == csd->flush_trigger_top)
					csd->flush_trigger_top = FLUSH_FACTOR(csd->n_bts);
				/* Note: This ensures that the csd->flush_trigger_top field added in GT.M v63007 is set if
				 * the old db_minorver is GDSMR122.
				 */
			case GDSMR130:
				/* YottaDB r134 introduced "max_procs" which records the maximum number of
				 * processes accessing the database to measure system load.
				 */
				csd->max_procs.cnt = 0;
				csd->max_procs.time = 0;
			case GDSMR134:
				if (GDSMR126 <= csd->minor_dbver)
				{	/* Upgrading from a prior YottaDB release */
					/* GT.M V63012 (merged after YottaDB r1.34) added fullblkwrt option */
					csd->write_fullblk = 0;
					/* GT.M V63014 (merged after YottaDB r1.34) moved gvstats around */
					MOVE_GVSTATS_REC_FROM_OLD_HDR_TO_NODE_LOCAL(csa, csd);
					/* GT.M V63014 code changes caused a couple of YottaDB fields to move around after r1.34.
					 * Move them from their old location to their new location.
					 */
					csd->max_procs =
						*(max_procs_t *)((sm_uc_ptr_t)csd + SGMNT_DATA_OFFSET_R134_max_procs);
					csd->reorg_sleep_nsec =
						*(uint4 *)((sm_uc_ptr_t)csd + SGMNT_DATA_OFFSET_R134_reorg_sleep_nsec);
				}
				/* else: Upgrading from a GT.M release. All auto upgrade activity would have already
				 * happened as appropriate in the previous "case" blocks.
				 */
			case GDSMR136:
				/* Below are GT.M V7 related auto upgrade actions. We do this here so all GT.M prior releases
				 * and YottaDB prior releases fall through to this section. Else we would have to duplicate
				 * this code in various places.
				 */
				csd->desired_db_format = GDSV6;	/* because it takes an action to move toward V7 */
				if (0 == csd->statsdb_allocation)
					csd->statsdb_allocation = STATSDB_ALLOCATION;	/* Initialize with statsdb default value */
				/* we do not initialize new post GDSMV63014 field csd->last_start_backup here because if
				   it has never been set the default (all zeroes) is OK, and if it has, we do not want to
				   overwrite it on our way up */
				csd->offset = 0;
				csd->max_rec = 0;
<<<<<<< HEAD
				csd->i_reserved_bytes = 0;
				/* the next four lines are because gvcst_init_sysops might have used an outdated assumption */
=======
				/*the next four lines are because gvcst_init_sysops might have used an outdated assumption */
>>>>>>> fdfdea1e (GT.M V7.1-002)
				csd->max_update_array_size = csd->max_non_bm_update_array_size
					= (int4)(ROUND_UP2(MAX_NON_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE));
				csd->max_update_array_size
					+= (int4)(ROUND_UP2(MAX_BITMAP_UPDATE_ARRAY_SIZE(csd), UPDATE_ARRAY_ALIGN_SIZE));
				/* GT.M V70002 added proactive block split option */
				if (csd->last_mdb_ver < GDSMV71001)
					csd->problksplit = DEFAULT_PROBLKSPLIT;
				if (csd->last_mdb_ver < GDSMV71002)
					csd->i_reserved_bytes = 0;
				break;
			case GDSMV63015:
				assert(FALSE);	/* if this should come to pass, add appropriate code above the assert */
			default:
				/* Unrecognized version in the header */
				assertpro(FALSE && csd->minor_dbver);
				break;
		}
		csd->minor_dbver = BLK_ID_32_MVER;	/* applies to all pre-V7 versions and expected here by workcheck */
	}
	csd->last_mdb_ver = GDSMVCURR;
	if (csd->fully_upgraded && !csd->db_got_to_v5_once)
	{	/* Database is fully upgraded but the db_got_to_v5_once field says different.
		 * Don't know how that could happen, except with DSE which can change both the database file header fields
		 */
		assert(!dse_running);
		csd->db_got_to_v5_once = TRUE; /* fix it in PRO */
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBBADUPGRDSTATE, 4, REG_LEN_STR(reg), DB_LEN_STR(reg));
	}
	return;
}
