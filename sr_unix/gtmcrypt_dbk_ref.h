/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc *
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
#define INVALID_FILE_FORMAT			0x0
#define LIBCONFIG_FILE_FORMAT			0x1
#define DBKEYS_FILE_FORMAT			0x2
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
	gtm_fileid_ptr_t		fileid;
	crypt_key_t			encr_key_handle;
	crypt_key_t			decr_key_handle;
} gtm_dbkeys_tbl;

STATICFNDCL gtm_status_t	gc_dbk_get_dbkeys_fname(char *fname, int *stat_success);
STATICFNDCL int			gc_dbk_get_single_entry(void *handle, char **db, char **key, int n);
void				gc_dbk_scrub_entries(void);
void				gc_dbk_get_hash(gtm_dbkeys_tbl *entry,  gtm_string_t *hash);
gtm_dbkeys_tbl*			gc_dbk_get_entry_by_fileid(gtm_fileid_ptr_t fileid);
gtm_dbkeys_tbl*			gc_dbk_get_entry_by_hash(gtm_string_t *hash);
gtm_status_t			gc_dbk_init_dbkeys_tbl(void);
gtm_status_t			gc_dbk_fill_symkey_hash(gtm_fileid_ptr_t req_fileid, char *req_hash);

GBLREF	int		n_dbkeys;
GBLREF	gtm_dbkeys_tbl	**fast_lookup_entry;

#define GC_ALLOCATE_TBL_ENTRY(X)												\
{																\
	X = MALLOC(SIZEOF(gtm_dbkeys_tbl));											\
	(X)->fileid_dirty = TRUE;												\
	(X)->symmetric_key_dirty = TRUE;											\
	(X)->fileid = NULL;													\
	(X)->index = 0;														\
}

#define RETURN_IF_INVALID_HANDLE(HANDLE)											\
{																\
	if (0 > (int)HANDLE || ((int)HANDLE > n_dbkeys))									\
	{															\
		assert(FALSE);													\
		UPDATE_ERROR_STRING("Encryption handle corrupted.");								\
		return GC_FAILURE;												\
	}															\
}

#endif /* GTMCRYPT_DBK_REF_H */
