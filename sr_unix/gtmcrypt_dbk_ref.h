/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCRYPT_DBK_REF_H
#define GTMCRYPT_DBK_REF_H

#define DATABASE_LINE_INFO			0
#define SYMMETRIC_KEY_LINE_INFO			1
#define DATABASE_LINE_INDICATOR			"dat "
#define SYMMETRIC_KEY_LINE_INDICATOR		"key "
#define DATABASE_LINE_INDICATOR_SIZE		(SIZEOF(DATABASE_LINE_INDICATOR) - 1)
#define SYMMETRIC_KEY_LINE_INDICATOR_SIZE	(SIZEOF(SYMMETRIC_KEY_LINE_INDICATOR) - 1)
#ifdef LINE_MAX
#undef LINE_MAX
#endif
#define LINE_MAX				(GTM_PATH_MAX + DATABASE_LINE_INDICATOR_SIZE + 2) /* 2 is just for safety */


typedef struct gtm_dbkeys_tbl_struct
{
	struct gtm_dbkeys_tbl_struct 	*next;
	int				fileid_dirty;
	int				symmetric_key_dirty;
	int				index;
	int				database_fn_len;
	char				database_fn[GTM_PATH_MAX + 1];
	char				symmetric_key_fn[GTM_PATH_MAX + 1];
	unsigned char			symmetric_key[SYMMETRIC_KEY_MAX + 1];
	unsigned char			symmetric_key_hash[GTMCRYPT_HASH_LEN + 1];
	xc_fileid_ptr_t			fileid;
	crypt_key_t			encr_key_handle;
	crypt_key_t			decr_key_handle;
} gtm_dbkeys_tbl;



void					gc_dbk_scrub_entries(void);
gtm_dbkeys_tbl*				gc_dbk_get_entry_by_fileid(xc_fileid_ptr_t fileid);
gtm_dbkeys_tbl*				gc_dbk_get_entry_by_hash(xc_string_t *hash);
xc_status_t				gc_dbk_fill_gtm_dbkeys_fname(char *fname);
xc_status_t				gc_dbk_load_entries_from_file(void);
xc_status_t				gc_dbk_fill_sym_key_and_hash(xc_fileid_ptr_t req_fileid, char *req_hash);
void					gc_dbk_get_hash(gtm_dbkeys_tbl *entry,  xc_string_t *hash);


#define GC_FREE_TBL_ENTRY(X)													\
{																\
	gtm_xcfileid_free_fptr((X)->fileid);											\
	memset((X)->symmetric_key, 0, SYMMETRIC_KEY_MAX);									\
	memset((X)->symmetric_key_hash, 0, GTMCRYPT_HASH_LEN);									\
	GC_FREE(X);														\
}

#define GC_ALLOCATE_TBL_ENTRY(X)												\
{																\
	GC_MALLOC((X), SIZEOF(gtm_dbkeys_tbl), gtm_dbkeys_tbl);									\
	(X)->fileid_dirty = TRUE;												\
	(X)->symmetric_key_dirty = TRUE;											\
	(X)->fileid = NULL;													\
	(X)->index = 0;														\
}

/* After the preliminary search, if we haven't found our entry in the in-memory linked list for the given hash/fileid, we try
 * reloading the db key file (just in case it has been changed since last time) and re-organize our in-memory linked list
 */
#define GC_DBK_RELOAD_IF_NEEDED(entry, RC, fileid, req_hash)									\
{																\
	if (NULL == entry)													\
	{															\
		if (0 != gc_dbk_load_entries_from_file())									\
			return GC_FAILURE;											\
		RC = gc_dbk_fill_sym_key_and_hash(fileid, req_hash);								\
	}															\
}

#define GC_DBK_GET_ENTRY_FROM_HANDLE(handle, entry, ret)									\
{																\
	GBLREF int			num_entries;										\
	GBLREF gtm_dbkeys_tbl	**fast_lookup_entry;										\
																\
	int				idx;											\
																\
	idx = (int)handle;													\
	if (idx < 0 || (idx > num_entries))											\
	{															\
		UPDATE_ERROR_STRING("Encryption handle corrupted.");								\
		entry = NULL;													\
		return ret;													\
	} else															\
		entry = (gtm_dbkeys_tbl *)fast_lookup_entry[idx];								\
}

#define GC_DBK_FILENAME_TO_ID(filename, fileid)											\
{																\
	if (TRUE != gtm_filename_to_id_fptr(filename, &fileid))									\
	{															\
		UPDATE_ERROR_STRING("Database file %s not found", filename->address);			 			\
		return GC_FAILURE;												\
	}															\
}

#endif /* GTMCRYPT_DBK_REF_H */
