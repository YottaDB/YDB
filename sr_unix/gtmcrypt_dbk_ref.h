/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMCRYPT_DBK_REF_H
#define GTMCRYPT_DBK_REF_H

typedef struct
{
	xc_string_t		db_name, key_filename;	/* name of the database and corresponding key found in the db key file */
	xc_string_t		key_string, hash; /* plain text key and it's hash */
	xc_fileid_ptr_t		fileid;		  /* if valid, unique file id representation of the database path */
	int			fileid_dirty, sym_key_dirty; /* indicates if the db and the key file are valid accessible path */
	int			index; 		  /* A positive integer (initialized to -1) indicating the ith entry in the db key
						   * file. This value is returned to the caller and subsequently passed to the
						   * plugin to get the key for the corresponding database. */
	struct db_key_map 	*next;		  /* Pointer to the next entry in the linked list */
	crypt_key_t		encr_key_handle, decr_key_handle; /* Pointer to the actual key handles typedef'ed to the underlying
							       * encryption library. */
}db_key_map;


void 				gc_dbk_scrub_entries(void);
xc_status_t 			gc_dbk_is_db_key_file_modified(void);
db_key_map* 			gc_dbk_get_entry_by_fileid(xc_fileid_ptr_t fileid);
db_key_map* 			gc_dbk_get_entry_by_hash(xc_string_t *hash);
dbkeyfile_line_type 		gc_dbk_get_line_info (char *buf, char *data);
xc_status_t 			gc_dbk_load_gtm_dbkeys(FILE **gtm_dbkeys);
xc_status_t 			gc_dbk_load_entries_from_file(void);
xc_status_t 			gc_dbk_fill_sym_key_and_hash(xc_fileid_ptr_t req_fileid, char *req_hash);
void	 			gc_dbk_get_hash(db_key_map *entry,  xc_string_t *hash);


#define GC_FREE_DB_KEY_MAP(X)				\
{							\
	GC_FREE((X)->db_name.address);			\
	GC_FREE((X)->key_filename.address);		\
	memset((X)->key_string.address, 0, GTM_KEY_MAX);\
	GC_FREE((X)->key_string.address);		\
	GC_FREE((X)->hash.address);			\
	gtm_xcfileid_free_fptr((X)->fileid);		\
	GC_FREE(X);					\
}

#define GC_NEW_DB_KEYMAP(X)						\
{									\
	GC_MALLOC(X, SIZEOF(db_key_map), db_key_map);			\
	memset(X, 0, SIZEOF(db_key_map));				\
	GC_MALLOC(X->db_name.address, GTM_PATH_MAX, char);		\
	memset((X)->db_name.address, 0, GTM_PATH_MAX);			\
	GC_MALLOC(X->key_filename.address, GTM_PATH_MAX, char);		\
	memset((X)->key_filename.address, 0, GTM_PATH_MAX);		\
	GC_MALLOC(X->key_string.address, GTM_PATH_MAX, char);		\
	memset((X)->key_string.address, 0, GTM_KEY_MAX);		\
	GC_MALLOC(X->hash.address, GTMCRYPT_HASH_LEN, char);		\
	memset((X)->hash.address, 0, GTMCRYPT_HASH_LEN);		\
	(X)->fileid_dirty = TRUE;					\
	(X)->sym_key_dirty = TRUE;					\
	(X)->fileid = NULL;						\
	(X)->index = 0;							\
}

#define GC_DBK_LOAD_KEY_FILE				\
{							\
	if (0 != gc_dbk_load_entries_from_file())	\
		return GC_FAILURE;			\
}

/* After the preliminary search, if we haven't found our entry in the in-memory linked list for the
 * given hash/fileid, we try reloading the db key file(if it has been changed since last time) and then
 * we re-organize our in-memory linked list and try to search again.
 */
#define GC_DBK_RELOAD_IF_NEEDED(entry, RC, fileid, req_hash)		\
{									\
	if (NULL == entry)						\
	{								\
		if (TRUE == gc_dbk_is_db_key_file_modified())		\
			GC_DBK_LOAD_KEY_FILE;				\
		RC = gc_dbk_fill_sym_key_and_hash(fileid, req_hash);	\
	}								\
}

#define GC_DBK_GET_ENTRY_FROM_HANDLE(handle, entry, ret)				\
{											\
	int	idx;									\
											\
	idx = (int)handle;								\
	if (idx < 0 || (idx > num_entries))						\
	{										\
		snprintf(err_string, ERR_STRLEN, "%s", "Encryption handle corrupted.");	\
		entry = NULL;								\
		return ret;								\
	} else										\
		entry = (db_key_map *)fast_lookup_entry[idx];				\
}

#define GC_DBK_FILENAME_TO_ID(filename, fileid)							\
{												\
	if (TRUE != gtm_filename_to_id_fptr(filename, &fileid))					\
	{											\
		snprintf(err_string, ERR_STRLEN, "database file %s not found", filename->address); 	\
		return GC_FAILURE;								\
	}											\
}

#define GC_DBK_SET_FIRST_ENTRY(cur)	db_map_root = (db_key_map *)cur
#define GC_DBK_GET_FIRST_ENTRY()	db_map_root
#define GC_DBK_GET_NEXT_ENTRY(cur)	(db_key_map *) cur->next

#endif /* GTMCRYPT_DBK_REF_H */
