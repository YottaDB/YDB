/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#define INIT_GBL_ROOT() { curr_gbl_root.str.addr = (char *)malloc(SIZEOF(mident_fixed)); }

#define CREATE_DUMMY_GBLDIR(header, saved, region, map, map_top) { \
		mval	v; \
		gd_binding *map_temp; \
\
		saved = header; \
		header = 0; \
		v.mvtype = MV_STR; \
		header = (gd_addr *)create_dummy_gbldir(); \
		map = header->maps;	\
		map_top = map + header->n_maps; \
		map_temp = map + 1; \
		map_temp->reg.addr = region; \
		map_temp = map_temp + 1;\
		map_temp->reg.addr = region; \
	}

#define GET_SAVED_GDADDR(header, saved, map, region) {\
	header = saved;	\
	map = header->maps; \
	map  = map + 1; /* get past local locks */ \
	map->reg.addr = region;  \
	map  = map + 1; \
	map->reg.addr = region;  \
}

#define RESTORE_ORIGINAL_GDADDR (header, saved) { header = saved; }


#define RETRIEVE_ROOT_VAL(gbl_name, root_val, temp_gbl_name, temp_root_val, root_val_len)	\
{												\
		temp_gbl_name = gbl_name; temp_root_val = root_val;				\
		for (root_val_len = 0; (*temp_root_val++ = *temp_gbl_name++); root_val_len++);	\
		*temp_root_val = '\0';								\
}

#define INIT_ROOT_GVT(curr_gbl_root, root_val_len, curr_gbl_root_mval)							\
{															\
	unsigned char		*key;											\
	gv_namehead		*hasht_tree;										\
	mname_entry		gvent;											\
															\
	GTMTRIG_ONLY(if ((HASHT_GBLNAME_LEN != root_val_len) || (0 != memcmp(HASHT_GBLNAME, curr_gbl_root, root_val_len))))\
	{														\
		curr_gbl_root_mval.str.len = MIN(root_val_len, MAX_MIDENT_LEN);						\
		memcpy(curr_gbl_root_mval.str.addr, curr_gbl_root, curr_gbl_root_mval.str.len);				\
        	op_gvname(VARLSTCNT(1) &curr_gbl_root_mval);								\
	} GTMTRIG_ONLY(else												\
		{													\
			SETUP_TRIGGER_GLOBAL;										\
			key = &gv_currkey->base[0];									\
			memcpy(key, HASHT_GBLNAME, HASHT_GBLNAME_FULL_LEN);						\
			key += HASHT_GBLNAME_FULL_LEN;									\
			*key++ = '\0';											\
			gv_currkey->end = HASHT_GBLNAME_FULL_LEN;							\
		}													\
		)													\
}
