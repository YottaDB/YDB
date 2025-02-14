/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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

#include "mdef.h"

#include "gtm_string.h"
#ifdef DEBUG
#include "gtm_ctype.h"
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gbldirnam.h"
#include "hashtab_mname.h"
#include "dpgbldir.h"
#include "filestruct.h"
#include "aio_shim.h"
#include "gtmio.h"
#include "dpgbldir_sysops.h"
#include "targ_alloc.h"
#include "ydb_logicals.h"
#include "zshow.h"
#ifdef DEBUG
#include "gtm_caseconv.h"
#endif
#include "trans_log_name.h"
#include "gtm_env_xlate_init.h"
#include "gtm_threadgbl.h"
#include "ydb_getenv.h"
#include "op.h"
#include "gdskill.h"
#include "mu_upgrade_bmm.h"

#define	NOISOLATION_LITERAL	"NOISOLATION"

GBLREF	mstr		env_ydb_gbldir_xlate;
GBLREF	gd_addr		*gd_header;
GBLREF	gv_namehead	*gv_target_list;
GBLREF	gdr_name	*gdr_name_head;
GBLREF	gd_addr		*gd_addr_head;
GBLREF	uint4		mu_upgrade_in_prog;

LITREF	char		gde_labels[GDE_LABEL_NUM][GDE_LABEL_SIZE];

error_def(ERR_GDINVALID);

/*+
Function:       ZGBLDIR

		This function searches the list of global directory names for
		the specified names.  If not found, it adds the new name to the
		list and calls GD_LOAD.  A pointer to the global directory
		structure is returned, and the name entry is pointed at it.
		The global directory pointer is then returned to the caller.

Syntax:         gd_addr *zgbldir(mval *v)
                gd_addr *zgbldir_opt(mval *v, boolean_t env_translated, boolean_t force_load)

Prototype:      ?

Return:         *gd_addr -- a pointer to the global directory structure

Arguments:      mval *v	-- an mval that contains the name of the global
				directory to be accessed.
		boolean_t env_translated -- indicates whether the value
				is a result of any previous env-translation so that any secondary
				translation can be eventually skipped (if the env and gbldir
				translation is the same).

Side Effects:   NONE

Notes:          NONE
-*/
gd_addr *zgbldir(mval *v)
{
	return zgbldir_opt(v, FALSE, FALSE);
}

gd_addr *zgbldir_opt(mval *v, boolean_t env_translated, boolean_t force_load)
{
	gd_addr		*gd_ptr;
	mval		trans_val, temp_val;
	gdr_name	*name;
	mstr		gbldirenv_mstr, temp_mstr, trnlnm_mstr, *tran_mstr, *tran_name;
	char		temp_buff[MAX_FN_LEN + 1];
	uint4		status;
	boolean_t	need_gbldir_translate, skip_gbldir_translate;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If "force_load" is TRUE, we should not be looking at our cache of already loaded gld files.
	 * Note that the reload is going to create a NEW entry in the "gdr_name_head" linked list.
	 * If the gld is already loaded, the reload is going to create a duplicate entry but more importantly
	 * ahead of the pre-existing entry in the linked list. This way a later call to "zgbldir()" will
	 * ensure the newly added entry gets picked up since it is ahead in the linked list. We do not get rid
	 * of the pre-existing entry because a lot of structures could have noted it down. For example,
	 * "udi->owning_gd", "reg->owning_gd", "csa->gd_ptr". In addition, "aio_shim_thread_init()" creates
	 * an AIO thread based on the "owning_gd". All of those would get confused if we free up the memory
	 * corresponding to the pre-existing "gd_ptr" entry. This means the "force_load" call, if done repeatedly
	 * would end up allocating duplicate memory structures indefinitely (i.e. there is no limit to memory usage).
	 * So the application has to be careful when invoking VIEW "GBLDIRLOAD" which is what does the "force_load" call.
	 */
	if (!force_load)
	{
		for (name = gdr_name_head;  name;  name = (gdr_name *)name->link)
		{
			if (v->str.len == name->name.len)
			{
				if (!v->str.len || !memcmp(v->str.addr, name->name.addr, v->str.len))
					return name->gd_ptr;
			}
		}
	}

	/* Skip gbldir translation if env and gbldir translation is handled in the same way */
	skip_gbldir_translate = env_translated
				&& (NULL != RFPTR(ydb_gbldir_xlate_entry))
				&& (RFPTR(gtm_env_xlate_entry) == RFPTR(ydb_gbldir_xlate_entry));
	need_gbldir_translate = (env_ydb_gbldir_xlate.len && !skip_gbldir_translate);

	/* v is empty so we want to get the value from environment variable */
	if (!v->str.len)
	{
		gbldirenv_mstr.addr = (char *)YDB_GBLDIR;
		gbldirenv_mstr.len = STRLEN(YDB_GBLDIR);
		if (!need_gbldir_translate)
		{	/* Do not need gbldir translation for sure. Use env var name as is. */
			temp_mstr.addr = gbldirenv_mstr.addr;
			temp_mstr.len  = gbldirenv_mstr.len;
			/* If the env var is undefined OR if it is defined and set to "",
			 * treat both as the same scenario and issue a ZGBLDIRUNDEF error.
			 */
			status = trans_log_name(&temp_mstr, &trnlnm_mstr, temp_buff, MAX_FN_LEN + 1, dont_sendmsg_on_log2long);
			if ((SS_NOLOGNAM == status) || ((SS_NORMAL == status) && !trnlnm_mstr.len))
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_ZGBLDIRUNDEF);
		} else
		{	/* Translate the logical name (environment variable) ydb_gbldir/gtmgbldir which would be done in
			 * "parse_file" (invoked inside "get_name" below) but we need to know the its value so that we can
			 * attempt to do global directory translation.
			 */
			status = trans_log_name(&gbldirenv_mstr, &trnlnm_mstr, temp_buff, MAX_FN_LEN + 1, dont_sendmsg_on_log2long);
			if (SS_LOG2LONG == status)
			{	/* Note: Env var value was too long. Don't issue error yet.
				 * Let later call to "get_name" handle it. But skip gbldir translation part now that
				 * we know input for gbldir translation is not available (too long to fit in buffer).
				 */
				need_gbldir_translate = FALSE;
				temp_mstr.addr = gbldirenv_mstr.addr;
				temp_mstr.len  = gbldirenv_mstr.len;
			} else
			{
				assert((SS_NOLOGNAM == status) || (SS_NORMAL == status));
				/* In either of the cases in the above assert, "trnlnm_mstr" is valid and usable */
				temp_mstr.addr = trnlnm_mstr.addr;
				temp_mstr.len  = trnlnm_mstr.len;
			}
		}
	} else
	{
		temp_mstr.addr = v->str.addr;
		temp_mstr.len  = v->str.len;
	}
	temp_val.str = temp_mstr;
	temp_val.mvtype = MV_STR;
	trans_val.str.len = 0;
	tran_mstr = need_gbldir_translate
				? (&ydb_gbldir_translate(&temp_val, &trans_val)->str)
				: &temp_mstr;
	tran_name = get_name(tran_mstr);
	gd_ptr = gd_load(tran_name, force_load);
	name = (gdr_name *)malloc(SIZEOF(gdr_name));
	if (name->name.len = v->str.len)	/* Note embedded assignment */
	{
		name->name.addr = (char *)malloc(v->str.len);
		memcpy(name->name.addr, v->str.addr, v->str.len);
	}
	/* Store translated global directory name as well */
	assert(tran_name->len);
	name->exp_name = *tran_name;
	/* free up memory allocated for mstr field in get_name.
	 * memory allocated for addr field of the mstr is needed as it has been copied over to "name->exp_name" */
	free(tran_name);

	if (NULL != gdr_name_head)
		name->link = (gdr_name *)gdr_name_head;
	else
		name->link = NULL;
	gdr_name_head = name;
	gdr_name_head->gd_ptr = gd_ptr;
	if ((NULL == name->link) && (MUPIP_UPGRADE_IN_PROGRESS != mu_upgrade_in_prog))
	{	/* Now that the FIRST global directory has been opened by this process, check if "ydb_app_ensures_isolation"
		 * env var was specified. If so do needed initialization for that here. Cannot do this initialization before
		 * here as it can cause STATSDB related issues (YDB#1047).
		 *
		 * Also if "mupip upgrade" is in progress, it overrides all names to be in the current region and resets
		 * "gvt->gd_csa = cs_addrs" for all gvts in "gv_target_list" which includes all gvts in "gvt_pending_list"
		 * too. This can cause assert failures in "process_gvt_pending_list()" due to the "gvt->gd_csa" overwrite.
		 * In that case, ignore the "ydb_app_ensures_isolation" env var as this process is operating with a tampered
		 * global directory.
		 */
		char	*ptr;
		mval	noiso_lit, gbllist;

		if (NULL != (ptr = ydb_getenv(YDBENVINDX_APP_ENSURES_ISOLATION, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH)))
		{	/* Call the VIEW "NOISOLATION" command with the contents of the <ydb_app_ensures_isolation> env var */
			noiso_lit.mvtype = MV_STR;
			noiso_lit.str.len = STR_LIT_LEN(NOISOLATION_LITERAL);
			noiso_lit.str.addr = NOISOLATION_LITERAL;
			gbllist.mvtype = MV_STR;
			gbllist.str.len = STRLEN(ptr);
			gbllist.str.addr = ptr;
			op_view(VARLSTCNT(2) &noiso_lit, &gbllist);
		}
	}
	return gd_ptr;
}

/*+
Function:       ZGBLDIR_NAME_LOOKUP_ONLY

		This function searches the list of global directory names for
		the specified names.  If not found, it retruns NULL.

Syntax:         gdr_name *zgbldir_name_lookup_only(mval *v)

Prototype:      ?

Return:         *gdr_name -- a pointer to the global directory name structure

Arguments:      mval *v	-- an mval that contains the name of the global
		directory to be accessed.  The name may require translation.

Side Effects:   NONE

Notes:          NONE
-*/
gdr_name	*zgbldir_name_lookup_only(mval *v)
{
	gd_addr		*gd_ptr;
	gdr_name	*name;
	mstr		temp_mstr, *tran_name;

	for (name = gdr_name_head;  name;  name = (gdr_name *)name->link)
		if (v->str.len == name->name.len && !memcmp(v->str.addr, name->name.addr, v->str.len))
			return name;
	return NULL;
}

/*+
Function:       GD_LOAD

Syntax:		gd_addr *gd_load(mstr *gd_name, boolean_t force_load)

		Open a global directory file and verify that it is a valid GD.
		Determine if it has already been opened.  If not, setup and
		initialize the GT.M structures used to access the GD based on
		the information in the file, enter in the linked list of global
		directories and return a pointer to it.  If already opened, return
		a pointer to it.

Prototype:      ?

Return:         gd_addr *	(all errors are signalled)

Arguments:	gd_name is the name of the file to be opened

Side Effects:   None
Notes:          A) While checking may be done earlier for duplicate names,
		unique identification of files can require OS specific
		operations useable only after the file is open, so checks
		must be done within this function for duplicate files.
-*/
gd_addr *gd_load(mstr *v, boolean_t force_load)
{
	void			*file_ptr; /* is a temporary structure as the file open and manipulations are currently stubs */
	header_struct		*header, temp_head, disp_head;
	gd_addr			*table, *gd_addr_ptr;
	gd_binding		*map, *map_top, *next_stats_map;
	gd_region		*reg, *reg_top, *first_stats_reg;
	uint4			t_offset, size;
	gd_gblname		*gnam, *gnam_top;
	int			i, n_regions, arraysize, disp_len;
	trans_num		*array;
#	ifdef DEBUG
	boolean_t		prevMapIsSpanning, currMapIsSpanning, gdHasSpanGbls;
	boolean_t		isSpannedReg[512];	/* allow up to 256 (* 2 for implicit statsdb) regions in dbg logic */
	gd_region		*base_reg, *stats_reg;
	uint4			reg_index;
	unsigned char		regname[MAX_MIDENT_LEN + 1];
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	file_ptr = open_gd_file(v);
	/* If "force_load" is TRUE, we should not be looking at our cache of already loaded gld files */
	if (!force_load)
	{
		for (gd_addr_ptr = gd_addr_head;  gd_addr_ptr;  gd_addr_ptr = gd_addr_ptr->link)
		{	/* if already open then return old structure */
			if (comp_gd_addr(gd_addr_ptr, file_ptr))
			{
				close_gd_file(file_ptr);
				return gd_addr_ptr;
			}
		}
	}
	file_read(file_ptr, SIZEOF(header_struct), (uchar_ptr_t)&temp_head, 1);		/* Read in header and verify is valid GD */
	for (i = 0;  i < GDE_LABEL_NUM;  i++)
	{
		if (!memcmp(temp_head.label, gde_labels[i], GDE_LABEL_SIZE - 1))
			break;
	}
	if (GDE_LABEL_NUM == i)
	{
		close_gd_file(file_ptr);
		disp_len = SIZEOF(disp_head.label);
		format2disp(temp_head.label, SIZEOF(temp_head.label), disp_head.label, &disp_len);
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_GDINVALID, 6, v->len, v->addr, LEN_AND_LIT(GDE_LABEL_LITERAL),
			disp_len, disp_head.label);
	}
	size = temp_head.filesize;
	header = (header_struct *)malloc(size + SIZEOF(gd_runtime_t));
	file_read(file_ptr, size, (uchar_ptr_t)header, 1);			/* Read in body of file */
	table = (gd_addr *)((char *)header + SIZEOF(header_struct));
        table->local_locks = (struct gd_region_struct *)((UINTPTR_T)table->local_locks + (UINTPTR_T)table);
	assert(table->var_maps_len == ((UINTPTR_T)table->regions - (UINTPTR_T)table->maps) - (table->n_maps * SIZEOF(gd_binding)));
	table->maps = (struct gd_binding_struct *)((UINTPTR_T)table->maps + (UINTPTR_T)table);
	table->regions = (struct gd_region_struct *)((UINTPTR_T)table->regions + (UINTPTR_T)table);
	table->segments = (struct gd_segment_struct *)((UINTPTR_T)table->segments + (UINTPTR_T)table);
	table->gblnames = (struct gd_gblname_struct *)((UINTPTR_T)table->gblnames + (UINTPTR_T)table);
	table->instinfo = (struct gd_inst_info_struct *)((UINTPTR_T)table->instinfo + (UINTPTR_T)table);
	if (table == (gd_addr *)table->instinfo)
		table->instinfo = NULL;
	table->end = (table->end + (UINTPTR_T)table);
	n_regions = table->n_regions;
	for (reg = table->regions, reg_top = reg + n_regions; reg < reg_top; reg++)
	{
		t_offset = reg->dyn.offset;
		reg->dyn.addr = (gd_segment *)((char *)table + t_offset);
#		ifdef DEBUG
		assert((reg - table->regions) <= ARRAYSIZE(isSpannedReg));
		isSpannedReg[reg - table->regions] = FALSE;
#		endif
		reg->owning_gd = table; /* set backpointer from region to owning gbldir */
	}
	table->gd_runtime = (gd_runtime_t *)((char *)header + size);
	assert((UINTPTR_T)table->gd_runtime >= (UINTPTR_T)table->end);
	memset(table->gd_runtime, 0, SIZEOF(gd_runtime_t));
#	ifdef DEBUG
	prevMapIsSpanning = FALSE;
	currMapIsSpanning = FALSE;
	gdHasSpanGbls = FALSE;
#	endif
	for (map = table->maps, map_top = map + table->n_maps;  map < map_top;  map++)
	{
		t_offset = map->reg.offset;
		map->reg.addr = (gd_region *)((char *)table + t_offset);
#		ifdef DEBUG
		currMapIsSpanning = ((map->gvname_len + 1) != map->gvkey_len);
		if (currMapIsSpanning)
			gdHasSpanGbls = TRUE;
		if (currMapIsSpanning || prevMapIsSpanning)
			isSpannedReg[map->reg.addr - table->regions] = TRUE;
		prevMapIsSpanning = currMapIsSpanning;
#		endif
		t_offset = map->gvkey.offset;
		map->gvkey.addr = (char *)table + t_offset;
		assert('\0' == map->gvkey.addr[map->gvname_len]);
		assert('\0' == map->gvkey.addr[map->gvkey_len]);
		assert('\0' == map->gvkey.addr[map->gvkey_len - 1]);
		assert((map->gvname_len + 1) <= map->gvkey_len);
	}
#	ifdef DEBUG
	assert(table->has_span_gbls == gdHasSpanGbls);
	for (reg = table->regions, reg_top = reg + n_regions; reg < reg_top; reg++)
	{
		assert(reg->is_spanned == isSpannedReg[reg - table->regions]);
		/* Validate "reg->statsDB_reg_index" */
		reg_index = reg->statsDB_reg_index;
		assert((INVALID_STATSDB_REG_INDEX != reg_index) && (reg_index < n_regions));
		if (IS_STATSDB_REGNAME(reg))
		{	/* is a statsDB reg */
			stats_reg = reg;
			base_reg = &table->regions[reg_index];
		} else
		{	/* is a base reg */
			base_reg = reg;
			stats_reg = &table->regions[reg_index];
		}
		assert(IS_BASEDB_REGNAME(base_reg));
		assert(IS_STATSDB_REGNAME(stats_reg));
		assert(base_reg->statsDB_reg_index == (stats_reg - table->regions));
		assert(stats_reg->statsDB_reg_index == (base_reg - table->regions));
		assert(base_reg->rname_len == stats_reg->rname_len);
		assert(ARRAYSIZE(regname) > base_reg->rname_len);
		upper_to_lower(regname, REG_STR_LEN(base_reg));
		assert(!memcmp(regname, (stats_reg)->rname, (stats_reg)->rname_len));	/* BYPASSOK */
		/* Since a statsdb region points to an MM database, setting defer_time=0 in that segment
		 * automatically disables flush timers (wcs_stale) from being set up. GDE should have ensured this.
		 */
		assert(0 == stats_reg->dyn.addr->defer_time);
		/* The below assert is relied upon by "gvcst_init". If it fails, it most likely means SIZEOF("gvstats_rec_t")
		 * has increased enough to cause GDE to calculate the statsdb blk size as 1536 instead. Fix macro accordingly.
		 */
		assert(STATSDB_BLK_SIZE == stats_reg->dyn.addr->blk_size);
		/* Similar asserts for a few other critical fields that are initialized in GDE & GT.M ("gvcst_init") */
		assert(STATSDB_MAX_KEY_SIZE == stats_reg->max_key_size);
		assert(STATSDB_MAX_REC_SIZE == stats_reg->max_rec_size);
		assert(stats_reg->mumps_can_bypass);
	}
	for (gnam = table->gblnames, gnam_top = gnam + table->n_gblnames; gnam < gnam_top; gnam++)
	{
		assert(SIZEOF(gnam->gblname) == (MAX_MIDENT_LEN + 1));
		assert('\0' == gnam->gblname[MAX_MIDENT_LEN]);
	}
#	endif
	table->link = gd_addr_head;
	table->is_dummy_gbldir = FALSE;
	gd_addr_head = table;
	fill_gd_addr_id(gd_addr_head, file_ptr);
	close_gd_file(file_ptr);
	table->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
	init_hashtab_mname(table->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE);
	/* For most MUPIP commands (except those that can do logical database updates ("is_replicator" == TRUE)
	 * or MUPIP RUNDOWN or MUPIP CREATE, hide the statsdb regions so the commands do not even know about them
	 * let alone operate on them. All of them would have set TREF(ok_to_see_statsdb_regs) appropriately.
	 */
	if (!TREF(ok_to_see_statsdb_regs))
	{
		first_stats_reg = NULL;
		/* Coalesce the regions first given that at least one stats region will always exist (GDE ensures this) */
		for (reg = table->regions, reg_top = reg + n_regions; reg < reg_top; reg++)
		{
			reg->reservedDBFlags |= RDBF_NOSTATS;
			if ((NULL == first_stats_reg) && IS_STATSDB_REGNAME(reg))
			{
				first_stats_reg = reg;
				break;
			}
		}
		if (NULL != first_stats_reg)
		{
#			ifdef DEBUG
			/* Once a stats reg has been seen, all following regions should be stats regions. Assert that */
			for (reg = first_stats_reg; reg < reg_top; reg++)
				assert(IS_STATSDB_REGNAME(reg));
#			endif
			n_regions = table->n_regions = first_stats_reg - table->regions;
			/* Coalesce the maps next. Move the non-statsdb-maps into one contiguous array */
			map = table->maps;
			map_top = map + table->n_maps;
			assert(IS_BASEDB_REGNAME(map->reg.addr));		/* first map entry should be a nonstats region */
			assert(IS_BASEDB_REGNAME(map_top[-1].reg.addr));	/* last map entry should be a nonstats region */
			next_stats_map = NULL;
			for ( ;  map < map_top; map++)
			{
				reg = map->reg.addr;
				if (IS_BASEDB_REGNAME(reg))
				{
					if (NULL != next_stats_map)
						*next_stats_map++ = *map;
				} else if (NULL == next_stats_map)
					next_stats_map = map;
			}
			assert(NULL != next_stats_map);
			assert(next_stats_map < map_top);
			table->n_maps = next_stats_map - table->maps;
		}
	}
	if (table->has_span_gbls && (TREF(gd_targ_reg_array_size) < n_regions))
	{
		array = TREF(gd_targ_reg_array);
		if (NULL != array)
			free(array);
		arraysize = n_regions * SIZEOF(*array);
		array = malloc(arraysize);
		memset(array, 0, arraysize);
		TREF(gd_targ_reg_array) = array;
		TREF(gd_targ_reg_array_size) = n_regions;
	}
	/* Assert that all runtime-only fields are null-initialized by GDE */
	assert(!table->ygs_map_entry_changed);
	return table;
}

/* Maintain list of regions for GTCM_SERVER */
void cm_add_gdr_ptr(gd_region *greg)
{
	gd_addr	*ga;

	ga = (gd_addr *)malloc(SIZEOF(gd_addr));
	ga->end = 0;	/* signifies a GT.CM gd_addr */
	ga->regions = greg;
	ga->n_regions = 1;
	ga->link = gd_addr_head;
	gd_addr_head = ga;
	return;
}

void cm_del_gdr_ptr(gd_region *greg)
{
	gd_addr	*ga1, *ga2;

	for (ga1 = ga2 = gd_addr_head;  ga1;  ga1 = ga1->link)
	{
		if (ga1->regions == greg)
		{
			if (ga1 == gd_addr_head)
				gd_addr_head = ga1->link;
			else
				ga2->link = ga1->link;
			free(ga1);
			break;
		}
		ga2 = ga1;
	}
	return;
}

boolean_t get_first_gdr_name(gd_addr *current_gd_header, mstr *log_nam)
{
	gdr_name	*name;

	for (name = gdr_name_head;  name;  name = (gdr_name *)name->link)
	{
		if (name->gd_ptr == current_gd_header)
		{
			*log_nam = name->exp_name;
			return (TRUE);
		}
	}
	return FALSE;
}

void gd_rundown(void)		/* Wipe out the global directory structures */
{
	gd_addr		*gda_cur, *gda_next;
	gdr_name	*gdn_cur, *gdn_next;

	for (gda_cur = gd_addr_head;  NULL != gda_cur; gda_cur = gda_next)
	{
		gda_next = gda_cur->link;
		if (gda_cur->end)
		{
			gd_ht_kill(gda_cur->tab_ptr, TRUE);
			free(gda_cur->tab_ptr);		/* free up hashtable malloced in gd_load() */
			free(gda_cur->id);		/* free up gd_id malloced in gd_load()/fill_gd_addr_id() */
			free((char *)gda_cur - SIZEOF(header_struct));	/* free up global directory itself */
		} else
			free(gda_cur);	/* GT.CM gd_addr and hence header_struct wasn't malloced in cm_add_gdr_ptr */
	}
	assert(NULL == gv_target_list);
	gd_header = gd_addr_head = (gd_addr *)NULL;
	for (gdn_cur = gdr_name_head; NULL != gdn_cur; gdn_cur = gdn_next)
	{
		gdn_next = (gdr_name *)gdn_cur->link;
		if (gdn_cur->name.len)
			free(gdn_cur->name.addr);
		free(gdn_cur);
	}
	gdr_name_head = (gdr_name *)NULL;
}

void gd_ht_kill(hash_table_mname *table, boolean_t contents)	/* wipe out the hash table corresponding to a gld */
{
	ht_ent_mname	*tabent, *topent;
	gvnh_reg_t	*gvnh_reg;
	gv_namehead	*gvt;
	gvnh_spanreg_t	*gvspan;
	int		i;

	if (contents)
	{
		for (tabent = table->base, topent = tabent + table->size; tabent < topent; tabent++)
		{
			if (HTENT_VALID_MNAME(tabent, gvnh_reg_t, gvnh_reg))
			{
				gvspan = gvnh_reg->gvspan;
				if (NULL == gvspan)
				{
					gvt = gvnh_reg->gvt;
					DEBUG_ONLY(gvnh_reg->gvt = NULL;)	/* or else targ_free() might assert fail */
					TARG_FREE_IF_NEEDED(gvt);
				} else
				{	/* this global spans more than one region. free up gvts corresponding to those regions */
					for (i = 0; i < (gvspan->max_reg_index - gvspan->min_reg_index + 1); i++)
					{
						gvt = GET_REAL_GVT(gvspan->gvt_array[i]);
						if (NULL != gvt)
						{
#							ifdef DEBUG
							/* below is needed to ensure "targ_free" does not assert fail */
							gvspan->gvt_array[i] = NULL;
							if (gvt == gvnh_reg->gvt)
								gvnh_reg->gvt = NULL;
#							endif
							TARG_FREE_IF_NEEDED(gvt);
						}
					}
					free(gvspan);
				}
				free(gvnh_reg);
			}
		}
	}
	free_hashtab_mname(table);
	/* We don't do a free(table) in this generic routine because it is called both by GT.M and GT.CM
	 * and GT.CM retains the table for reuse while GT.M doesn't. GT.M fgncal_rundown() takes care of
	 * this by freeing it up explicitly (after a call to ht_kill) in gd_rundown() [dpgbldir.c]
	 */
	return;
}
