/****************************************************************
 *								*
 * Copyright (c) 2009-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <gpgme.h>				/* gpgme functions */
#include <gpg-error.h>				/* gcry*_err_t */
#include <libconfig.h>

#include "gtmxc_types.h"

#include "gtmcrypt_util.h"
#include "gtmcrypt_interface.h"			/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"

#define	UNRES_KEY_FILE				0	/* Key is for device encryption. */
#define	UNRES_KEY_UNRES_DB			1	/* Key is for a database that does not yet exist. */
#define	UNRES_KEY_RES_DB			2	/* Key is for a database that already exists. */

#define CONFIG_FILE_UNREAD			('\0' == gc_config_filename[0])
#define GPG_MESSAGE				"Verify encrypted key file and your GNUPGHOME settings"
#define NON_GPG_MESSAGE				"Verify encryption key in configuration file pointed to by $gtmcrypt_config"

/* On certain platforms the st_mtime field of the stat structure got replaced by a timespec st_mtim field, which in turn has tv_sec
 * and tv_nsec fields. For compatibility reasons, those platforms define an st_mtime macro which points to st_mtim.tv_sec. Whenever
 * we detect such a situation, we define a nanosecond flavor of that macro to point to st_mtim.tv_nsec. On HPUX Itanium and older
 * AIX boxes the stat structure simply has additional fields with the nanoseconds value, yet the names of those field are different
 * on those two architectures, so we choose our mapping accordingly.
 */
#if defined st_mtime
#  define st_nmtime				st_mtim.tv_nsec
#elif defined(_AIX)
#  define st_nmtime				st_mtime_n
#elif defined(__hpux) && defined(__ia64)
#  define st_nmtime				st_nmtime
#endif

/* Allocate a gtm_keystore_t element. */
#define GC_ALLOCATE_KEYSTORE_ENTRY(X)						\
{										\
	X = MALLOC(SIZEOF(gtm_keystore_t));					\
	(X)->cipher_head = NULL;						\
	(X)->db_cipher_entry = NULL;						\
}

/* Allocate a gtm_keystore_xxx_link_t element. */
#define GC_ALLOCATE_KEYSTORE_LINK(X, TYPE)					\
{										\
	X = (TYPE *)MALLOC(SIZEOF(TYPE));					\
	(X)->left = (X)->right = NULL;						\
}

/* Insert a new gtm_keystore_xxx_link_t element in a respective tree. It assumes
 * (and asserts) that there is no existing matching node.
 */
#define INSERT_KEY_LINK(ROOT, LINK, TYPE, FIELD, VALUE, LENGTH, FILL_LEN)	\
{										\
	int	diff;								\
	TYPE	*cur_node, **target_node;					\
										\
	target_node = &ROOT;							\
	while (cur_node = *target_node)	/* NOTE: Assignment!!! */		\
	{									\
		diff = memcmp(cur_node->FIELD, VALUE, LENGTH);			\
		assert(0 != diff);						\
		if (diff < 0)							\
			target_node = &cur_node->left;				\
		else								\
			target_node = &cur_node->right;				\
	}									\
	GC_ALLOCATE_KEYSTORE_LINK(*target_node, TYPE);				\
	(*target_node)->link = LINK;						\
	memset((*target_node)->FIELD, 0, FILL_LEN);				\
	memcpy((*target_node)->FIELD, VALUE, LENGTH);				\
}

/* Find a particular key based on a binary tree with a specific search criterion, such
 * as the key's name or hash. The macro causes the caller to return the found node.
 */
#define LOOKUP_KEY(ROOT, TYPE, FIELD, VALUE, LENGTH, CHECK_NULL)		\
{										\
	int	diff;								\
	TYPE	*cur_node;							\
										\
	cur_node = ROOT;							\
	while (cur_node)							\
	{									\
		diff = memcmp(cur_node->FIELD, VALUE, LENGTH);			\
		if (0 < diff)							\
			cur_node = cur_node->right;				\
		else if ((0 == diff) &&						\
			(CHECK_NULL						\
			 ? '\0' == *(((char *)cur_node->FIELD) + LENGTH)	\
			 : TRUE))						\
			return cur_node->link;					\
		else								\
			cur_node = cur_node->left;				\
	}									\
	return NULL;								\
}

/* Insert a new gtm_keystore_unres_key_link_t element in the unresolved keys list. */
#define INSERT_UNRESOLVED_KEY_LINK(KEYNAME, KEYPATH, INDEX, STATUS)				\
{												\
	gtm_keystore_unres_key_link_t *node;							\
												\
	node = (gtm_keystore_unres_key_link_t *)MALLOC(	SIZEOF(gtm_keystore_unres_key_link_t));	\
	memset(node->key_name, 0, GTM_PATH_MAX);						\
	strncpy(node->key_name, KEYNAME, GTM_PATH_MAX);						\
	memset(node->key_path, 0, GTM_PATH_MAX);						\
	strncpy(node->key_path, KEYPATH, GTM_PATH_MAX);						\
	node->next = keystore_by_unres_key_head;						\
	node->index = INDEX;									\
	node->status = STATUS;									\
	keystore_by_unres_key_head = node;							\
}

/* Remove all elements from the unresolved keys tree. */
#define REMOVE_UNRESOLVED_KEY_LINKS						\
{										\
	gtm_keystore_unres_key_link_t *curr, *temp;				\
										\
	curr = keystore_by_unres_key_head;					\
	while (curr)								\
	{									\
		temp = curr->next;						\
		FREE(curr);							\
		curr = temp;							\
	}									\
	keystore_by_unres_key_head = NULL;					\
}

STATICDEF int					n_keys;					/* Count of how many keys were loaded. */
STATICDEF char					gc_config_filename[GTM_PATH_MAX];	/* Path to the configuration file. */
STATICDEF gtm_keystore_hash_link_t		*keystore_by_hash_head = NULL;		/* Root of the binary search tree to look
											 * keys up by hash. */
STATICDEF gtm_keystore_keyname_link_t		*keystore_by_keyname_head = NULL;	/* Root of the binary search tree to look
											 * keys up by name. */
STATICDEF gtm_keystore_keypath_link_t		*keystore_by_keypath_head = NULL;	/* Root of the binary search tree to look
											 * keys up by path. */
STATICDEF gtm_keystore_unres_key_link_t		*keystore_by_unres_key_head = NULL;	/* Head of the linked list holding keys of
											 * DBs with presently unresolved paths. */
STATICDEF config_t				gtmcrypt_cfg;				/* Encryption configuration. */
STATICDEF char					key_name_array[GTM_PATH_MAX];		/* Array for temporary storage of DBs' real
											 * path information. */
STATICDEF unsigned char				key_hash_array[GTMCRYPT_HASH_LEN];	/* Array for temporary storage of keys'
											 * hashes. */

GBLREF	passwd_entry_t				*gtmcrypt_pwent;
GBLREF	int					gtmcrypt_init_flags;

/*
 * Find the key based on its name.
 *
 * Arguments:	keyname		Name of the key.
 * 		length		Length of the key name.
 * 		entry		Address where to place the pointer to the found key.
 * 		database	Flag indicating whether a database (or device) key is being searched.
 *
 * Returns:	0 if the key with the specified name is found; -1 otherwise.
 */
int gtmcrypt_getkey_by_keyname(char *keyname, int length, gtm_keystore_t **entry, int database)
{
	int error;

	if (NULL == (*entry = keystore_lookup_by_keyname(keyname, length)))
	{	/* No matches in the binary tree; trying the unresolved key list. */
		if (0 != keystore_refresh())
			return -1;
		error = 0;
		if (NULL == (*entry = keystore_lookup_by_unres_key(keyname, length, FALSE, database, &error)))
		{
			if (!error)
			{
				UPDATE_ERROR_STRING("%s " STR_ARG " missing in configuration file or does not exist",
					(database ? "Database file" : "Keyname"), ELLIPSIZE(keyname));
			}
			return -1;
		}
	}
	assert(NULL != *entry);
	return 0;
}

/*
 * Find the key based on its hash.
 *
 * Arguments:	hash	Hash of the key.
 * 		entry	Address where to place the pointer to the found key.
 *
 * Returns:	0 if the key with the specified name is found; -1 otherwise.
 */
int gtmcrypt_getkey_by_hash(unsigned char *hash, gtm_keystore_t **entry)
{
	int		err_caused_by_gpg, error;
	char		save_err[MAX_GTMCRYPT_ERR_STRLEN], hex_buff[GTMCRYPT_HASH_HEX_LEN + 1];
	char		*alert_msg;

	if (NULL == (*entry = keystore_lookup_by_hash(hash)))
	{	/* No matches in the binary tree; trying the unresolved key list. */
		if (0 != keystore_refresh())
			return -1;
		error = 0;
		if (NULL == (*entry = keystore_lookup_by_unres_key((char *)hash, GTMCRYPT_HASH_LEN, TRUE, TRUE, &error)))
		{
			if (!error)
			{	/* Be specific in the error as to what hash we were trying to find. */
				err_caused_by_gpg = ('\0' != gtmcrypt_err_string[0]);
				alert_msg = err_caused_by_gpg ? GPG_MESSAGE : NON_GPG_MESSAGE;
				GC_HEX(hash, hex_buff, GTMCRYPT_HASH_LEN);
				if (err_caused_by_gpg)
				{
					strncpy(save_err, gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN);
					UPDATE_ERROR_STRING("Expected hash - " STR_ARG " - %s. %s",
						ELLIPSIZE(hex_buff), save_err, alert_msg);
				} else
					UPDATE_ERROR_STRING("Expected hash - " STR_ARG ". %s", ELLIPSIZE(hex_buff), alert_msg);
			}
			return -1;
		}
	}
	assert(NULL != *entry);
	return 0;
}

/*
 * Helper function to perform the actual binary search of the key by its hash.
 *
 * Arguments:	hash	Hash of the key.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_hash(unsigned char *hash)
{
	LOOKUP_KEY(keystore_by_hash_head, gtm_keystore_hash_link_t, link->key_hash, hash, GTMCRYPT_HASH_LEN, FALSE);
}

/*
 * Helper function to perform the actual binary search of the key by its name.
 *
 * Arguments:	keyname		Name of the key.
 * 		length		Length of the key.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_keyname(char *keyname, int length)
{
	LOOKUP_KEY(keystore_by_keyname_head, gtm_keystore_keyname_link_t, key_name, keyname, length, TRUE);
}

/*
 * Helper function to perform the actual binary search of the key by its path.
 *
 * Arguments:	keypath		Path to the key.
 * 		length		Length of the path.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_keypath(char *keypath, int length)
{
	LOOKUP_KEY(keystore_by_keypath_head, gtm_keystore_keypath_link_t, link->key_path, keypath, length, TRUE);
}

/*
 * Helper function to perform a linear search of the key by its name or hash in the unresolved keys list. It attempts to resolve the
 * real path of a keyname in case it corresponds to a previously unresolved database name. If the path is resolved, the node's entry
 * is used to create (as needed) new key node as well as hash-, keyname-, and keypath-based links to it, and the unresolved entry is
 * removed from the list.
 *
 * Arguments:	search_field	Either name or hash of the key to find.
 *		search_len	Length of the search field.
 *		hash		Flag indicating whether to search by hash or keyname.
 *		database	Flag indicating whether the search is for a database or device encryption key.
 * 		error		Address where to set the flag indicating whether an error was encountered.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_unres_key(char *search_field, int search_len, int hash, int database, int *error)
{
	gtm_keystore_unres_key_link_t	*curr, *prev, *next;
	gtm_keystore_t			*node;
	int				name_length, path_length;

	prev = NULL;
	curr = keystore_by_unres_key_head;
	while (curr)
	{	/* Skip entries whose type does not match the one we are searching for. */
		if ((database && (UNRES_KEY_FILE == curr->status)) || (!database && (UNRES_KEY_FILE != curr->status)))
		{
			prev = curr;
			curr = curr->next;
			continue;
		}
		/* If the database file has not been resolved yet, try resolving it. */
		if (UNRES_KEY_UNRES_DB == curr->status)
		{
			if (NULL == realpath(curr->key_name, key_name_array))
			{
				if (ENAMETOOLONG == errno)
				{
					*error = TRUE;
					UPDATE_ERROR_STRING("Real path, or a component of the path, of the database " STR_ARG
						" is too long", ELLIPSIZE(curr->key_name));
					return NULL;
				} else if (ENOENT != errno)
				{
					*error = TRUE;
					UPDATE_ERROR_STRING("Could not obtain the real path of the database " STR_ARG,
						ELLIPSIZE(curr->key_name));
					return NULL;
				}
				if (!hash)
				{	/* If we are looking by a keyname, and the database is missing, skip the entry. Otherwise,
					 * give a chance to find the key by hash.
					 */
					prev = curr;
					curr = curr->next;
					continue;
				}
			} else
			{	/* Once the path has been resolved, save it to avoid future realpath()s. */
				strncpy(curr->key_name, key_name_array, GTM_PATH_MAX);
				curr->status = UNRES_KEY_RES_DB;
			}
		}
		path_length = strlen(curr->key_path);
		if (UNRES_KEY_UNRES_DB != curr->status)
		{	/* It is possible that a newly resolved realpath points to a previously seen database file, in which case we
			 * should first check whether that database has already been inserted into the tree to avoid inserting a
			 * duplicate.
			 */
			name_length = strlen(curr->key_name);
			assert(name_length < GTM_PATH_MAX);
			if (NULL != (node = keystore_lookup_by_keyname(curr->key_name, name_length)))
			{	/* +1 is to avoid matches of names with common prefixes. */
				if (strncmp(node->key_path, curr->key_path, path_length + 1))
				{
					*error = TRUE;
					if (database)
					{
						UPDATE_ERROR_STRING("In config file " STR_ARG ", database file in entry #%d "
							"corresponding to 'database.keys' resolves to a previously seen file but "
							"specifies a different key", ELLIPSIZE(gc_config_filename), curr->index);
					} else
					{
						UPDATE_ERROR_STRING("In config file " STR_ARG ", keyname in entry #%d "
							"corresponding to 'files' has already been seen but specifies "
							"a different key", ELLIPSIZE(gc_config_filename), curr->index);
					}
					return NULL;
				} else
				{	/* This was already found in our search trees, so remove from the unresolved list. */
					next = curr->next;
					if (NULL != prev)
						prev->next = next;
					else
					{
						assert(curr == keystore_by_unres_key_head);
						keystore_by_unres_key_head = next;
					}
					FREE(curr);
					curr = next;
					continue;
				}
			}
		} else
		{	/* Name is unresolved; we better be searching by hash. */
			assert(hash);
			name_length = -1;
		}
		if (hash || ((name_length == search_len) && !strncmp(search_field, curr->key_name, name_length)))
		{	/* If either we have a name match or we are searching by hash, go ahead and decrypt the key. */
			if (NULL == (node = gtmcrypt_decrypt_key(curr->key_path, path_length, curr->key_name, name_length)))
			{
				*error = TRUE;
				return NULL;
			} else
			{
				next = curr->next;
				if (NULL != prev)
					prev->next = next;
				else
				{
					assert(curr == keystore_by_unres_key_head);
					keystore_by_unres_key_head = next;
				}
				FREE(curr);
				curr = next;
				/* If the key name or hash (depending on the type of search) matches, return the key. */
				if (!hash || (!memcmp(node->key_hash, search_field, GTMCRYPT_HASH_LEN)))
					return node;
				continue;
			}
		}
		prev = curr;
		curr = curr->next;
	}
	return NULL;
}

/*
 * Helper function to decrypt a symmetric key, produce its hash, and store it for future discovery by key name, path, or hash.
 *
 * Arguments:	key_path	Path to the key.
 *		path_length	Length of the keypath.
 *		key_name	Name of the key.
 *		name_length	Length of the keyname.
 *
 * Returns:	Pointer to the key, if created; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *gtmcrypt_decrypt_key(char *key_path, int path_length, char *key_name, int name_length)
{
	gtm_keystore_t		*node;
	unsigned char		raw_key[SYMMETRIC_KEY_MAX];
	int			raw_key_length;

	/* If we have seen a key with the same path, do not re-read it. */
	if (NULL == (node = keystore_lookup_by_keypath(key_path, path_length)))
	{	/* Now that we have the name of the symmetric key file, try to decrypt it. If gc_pk_get_decrypted_key returns a
		 * non-zero status, it should have already populated the error string.
		 */
		if (0 != gc_pk_get_decrypted_key(key_path, raw_key, &raw_key_length))
			return NULL;
		if (0 == raw_key_length)
		{
			UPDATE_ERROR_STRING("Symmetric key " STR_ARG " found to be empty", ELLIPSIZE(key_path));
			return NULL;
		}
		/* We expect a symmetric key within a certain length. */
		assert(SYMMETRIC_KEY_MAX >= raw_key_length);
		GC_PK_COMPUTE_HASH(key_hash_array, raw_key);
		/* It is possible that while no key has been specified under this name, the same key has already been loaded
		 * for a different database or device, so look up the key by hash to avoid duplicates.
		 */
		if (NULL == (node = keystore_lookup_by_hash(key_hash_array)))
		{
			GC_ALLOCATE_KEYSTORE_ENTRY(node);
			/* WARNING: Not doing a memset here because raw_key comes padded with NULLs from gc_pk_get_decrypted_key. */
			memcpy(node->key, raw_key, SYMMETRIC_KEY_MAX);
			/* This should take care of assigning key_hash to the node itself. */
			INSERT_KEY_LINK(keystore_by_hash_head, node, gtm_keystore_hash_link_t, link->key_hash, key_hash_array,
					GTMCRYPT_HASH_LEN, GTMCRYPT_HASH_LEN);
		}
		INSERT_KEY_LINK(keystore_by_keypath_head, node, gtm_keystore_keypath_link_t,
				link->key_path, key_path, path_length + 1, GTM_PATH_MAX);
	}
	if (-1 != name_length)
	{	/* Only inserting a keyname-based link if the keyname was passed. */
		INSERT_KEY_LINK(keystore_by_keyname_head, node, gtm_keystore_keyname_link_t,
				key_name, key_name, name_length + 1, GTM_PATH_MAX);
	}
	return node;
}

/*
 * Re-read the configuration file, if necessary, and store it in memory.
 *
 * Returns: 0 if succeeded re-reading the configuration file; -1 otherwise.
 */
STATICFNDEF int keystore_refresh()
{
	int			n_mappings, status, just_read;
	char			*config_env;
	struct stat		stat_info;
	static long		last_modified_s, last_modified_ns;

	just_read = FALSE;
	/* Check and update the value of gtm_passwd if it has changed since we last checked. This way, if the user had originally
	 * entered a wrong password, but later changed the value (possible in MUMPS using external call), we read the up-to-date
	 * value instead of issuing an error.
	 */
	if (0 != gc_update_passwd(GTM_PASSWD_ENV, &gtmcrypt_pwent, GTMCRYPT_DEFAULT_PASSWD_PROMPT,
					GTMCRYPT_OP_INTERACTIVE_MODE & gtmcrypt_init_flags))
	{
		return -1;
	}
	if (CONFIG_FILE_UNREAD)
	{	/* First, make sure we have a proper environment varible and a regular configuration file. */
		if (NULL != (config_env = getenv("gtmcrypt_config")))
		{
			if (0 == strlen(config_env))
			{
				UPDATE_ERROR_STRING(ENV_EMPTY_ERROR, "gtmcrypt_config");
				return -1;
			}
			if (0 != stat(config_env, &stat_info))
			{
				UPDATE_ERROR_STRING("Cannot stat configuration file: " STR_ARG ". %s", ELLIPSIZE(config_env),
					strerror(errno));
				return -1;
			}
			if (!S_ISREG(stat_info.st_mode))
			{
				UPDATE_ERROR_STRING("Configuration file " STR_ARG " is not a regular file", ELLIPSIZE(config_env));
				return -1;
			}
		} else
		{
			UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, "gtmcrypt_config");
			return -1;
		}
		/* The gtmcrypt_config variable is defined and accessible. Copy it to a global for future references. */
		strncpy(gc_config_filename, config_env, GTM_PATH_MAX);
		just_read = TRUE;
	}
	assert(!CONFIG_FILE_UNREAD);
	/* Stat the file if not done already, so that we can get the last modified date. */
	if ((!just_read) && (0 != stat(gc_config_filename, &stat_info)))
	{
		UPDATE_ERROR_STRING("Cannot stat configuration file " STR_ARG ". %s", ELLIPSIZE(gc_config_filename),
			strerror(errno));
		return -1;
	}
	/* If the config file has not been modified since the last time we checked, return right away. */
	if ((last_modified_s > (long)stat_info.st_mtime)
		|| ((last_modified_s == (long)stat_info.st_mtime)
			&& (last_modified_ns >= (long)stat_info.st_nmtime)))
	{
		return 0;
	}
	/* File has been modified, so re-read it. */
	if (!config_read_file(&gtmcrypt_cfg, gc_config_filename))
	{
		UPDATE_ERROR_STRING("Cannot read config file " STR_ARG ". At line# %d - %s", ELLIPSIZE(gc_config_filename),
					config_error_line(&gtmcrypt_cfg), config_error_text(&gtmcrypt_cfg))
		return -1;
	}
	/* Clear the entire unresolved keys list because it will be rebuilt. */
	REMOVE_UNRESOLVED_KEY_LINKS;
	n_keys = 0;
	if (-1 == (status = read_database_section(&gtmcrypt_cfg)))
		return -1;
	n_keys += status;
	if (-1 == (status = read_files_section(&gtmcrypt_cfg)))
		return -1;
	n_keys += status;
	last_modified_s = (long)stat_info.st_mtime;
	last_modified_ns = (long)stat_info.st_nmtime;
	if (0 == n_keys)
	{
		UPDATE_ERROR_STRING("Configuration file " STR_ARG " contains neither 'database.keys' section nor 'files' section, "
			"or both sections are empty.", ELLIPSIZE(gc_config_filename));
		return -1;
	}
	return 0;
}

/*
 * Read the 'files' section of the configuration file, storing any previously unseen key in the unresolved list.
 *
 * Arguments:	cfgp		Pointer to the configuration object as populated by libconfig.
 *
 * Returns:	0 if successfully processed the 'files' section; -1 otherwise.
 */
STATICFNDEF int read_files_section(config_t *cfgp)
{
	int			i, name_length, path_length, lcl_n_maps;
	config_setting_t	*setting, *elem;
	gtm_keystore_t		*node;
	char			*key_name, *key_path;

	if (NULL == (setting = config_lookup(cfgp, "files")))
		return 0;
	lcl_n_maps = config_setting_length(setting);
	for (i = 0; i < lcl_n_maps; i++)
	{
		elem = config_setting_get_elem(setting, i);
		assert(NULL != elem);
		if (CONFIG_TYPE_STRING != config_setting_type(elem))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d corresponding to 'files' is not a string",
					ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		if (NULL == (key_name = config_setting_name(elem)))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d corresponding to 'files' does not have a "
					"key attribute", ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		/* Length should be under GTM_PATH_MAX because that is the size of the array where the name of a key is stored. */
		name_length = strlen(key_name);
		if (GTM_PATH_MAX <= name_length)
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", 'files' entry #%d's field length exceeds %d",
					ELLIPSIZE(gc_config_filename), i + 1, GTM_PATH_MAX - 1);
			return -1;
		}
		if (NULL == (key_path = (char *)config_setting_get_string(elem)))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", cannot find the value corresponding to 'files.%s'",
						ELLIPSIZE(gc_config_filename), key_name);
			return -1;
		}
		path_length = strlen(key_path);
		if (GTM_PATH_MAX <= path_length)
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", 'files' entry #%d's field length exceeds %d",
						ELLIPSIZE(gc_config_filename), i + 1, GTM_PATH_MAX - 1);
			return -1;
		}
		if (NULL != (node = keystore_lookup_by_keyname(key_name, name_length)))
		{	/* +1 is to match the NULL character. */
			if (strncmp(node->key_path, key_path, path_length + 1))
			{
				UPDATE_ERROR_STRING("In config file " STR_ARG ", keyname in entry #%d corresponding to 'files' "
					"has already been seen but specifies a different key",
					ELLIPSIZE(gc_config_filename), i + 1);
				return -1;
			} else
				continue;
		}
		INSERT_UNRESOLVED_KEY_LINK(key_name, key_path, i + 1, UNRES_KEY_FILE);
	}
	return lcl_n_maps;
}

/*
 * Process the 'database' section of the configuration file, storing any previously unseen key in the unresolved list.
 *
 * Arguments:	cfgp		Pointer to the configuration object as populated by libconfig.
 *
 * Returns:	0 if successfully processed the 'database' section; -1 otherwise.
 */
STATICFNDEF int read_database_section(config_t *cfgp)
{
	int			i, name_length, path_length, lcl_n_maps;
	config_setting_t	*setting, *elem;
	gtm_keystore_t		*node;
	char			*key_name, *key_path;

	if (NULL == (setting = config_lookup(cfgp, "database.keys")))
		return 0;
	lcl_n_maps = config_setting_length(setting);
	/* The following code makes sure that having an empty last entry in the database section is not required and does not cause
	 * errors, as GTM-7948's original implementation would have it.
	 */
	if (lcl_n_maps > 1)
	{
		elem = config_setting_get_elem(setting, lcl_n_maps - 1);
		if (0 == config_setting_length(elem))
		{
			config_setting_remove_elem(setting, lcl_n_maps - 1);
			lcl_n_maps--;
		}
	}
	for (i = 0; i < lcl_n_maps; i++)
	{
		elem = config_setting_get_elem(setting, i);
		assert(NULL != elem);
		if (!config_setting_lookup_string(elem, "dat", (const char **)&key_name))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d corresponding to "
				"'database.keys' does not have a 'dat' item", ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		/* Length should be under GTM_PATH_MAX because that is the size of the array where the name of a key is stored. */
		name_length = strlen(key_name);
		if (GTM_PATH_MAX <= name_length)
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", in entry #%d corresponding to 'database.keys' "
					"file name exceeds %d", ELLIPSIZE(gc_config_filename), i + 1, GTM_PATH_MAX - 1);
			return -1;
		}
		if (!config_setting_lookup_string(elem, "key", (const char **)&key_path))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d corresponding to "
				"'database.keys' does not have a 'key' item", ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		path_length = strlen(key_path);
		if (GTM_PATH_MAX <= path_length)
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", 'database.keys' entry #%d's field length exceeds %d",
						ELLIPSIZE(gc_config_filename), i + 1, GTM_PATH_MAX - 1);
			return -1;
		}
		if (NULL != (node = keystore_lookup_by_keyname(key_name, name_length)))
		{	/* +1 is to match the NULL character. */
			if (strncmp(node->key_path, key_path, path_length + 1))
			{
				UPDATE_ERROR_STRING("In config file " STR_ARG ", database file in entry #%d corresponding to "
					"'database.keys' resolves to a previously seen file but specifies a different key",
					ELLIPSIZE(gc_config_filename), i + 1);
				return -1;
			} else
				continue;
		}
		INSERT_UNRESOLVED_KEY_LINK(key_name, key_path, i + 1, UNRES_KEY_UNRES_DB);
	}
	return lcl_n_maps;
}

/*
 * Create new encryption / decryption state object with the specified IV.
 *
 * Arguments:	entry	Pointer to the key structure to which the encryption / decryption state object will be assigned.
 * 		iv	Initialization vector to use.
 * 		length	Length of the initialization vector.
 * 		action	1 for encryption, 0 for decryption.
 *
 * Returns:	0 if successfully created a new encryption / decryption state object; -1 otherwise.
 */
int keystore_new_cipher_ctx(gtm_keystore_t *entry, char *iv, int length, int action)
{
	int			rv;
	crypt_key_t		handle;
	gtm_cipher_ctx_t	*ctx;
	unsigned char		iv_array[GTMCRYPT_IV_LEN];

	memset(iv_array, 0, GTMCRYPT_IV_LEN);
	memcpy(iv_array, iv, length);
	if (0 != (rv = gc_sym_create_cipher_handle(entry->key, iv_array, &handle, action)))
		return rv;
	ctx = MALLOC(SIZEOF(gtm_cipher_ctx_t));
	ctx->store = entry;
	ctx->handle = handle;
	memcpy(ctx->iv, iv_array, GTMCRYPT_IV_LEN);
	if (!entry->cipher_head)
	{
		ctx->next = ctx->prev = NULL;
	} else
	{
		ctx->next = entry->cipher_head;
		ctx->prev = NULL;
		entry->cipher_head->prev = ctx;
	}
	entry->cipher_head = ctx;
	return 0;
}

/*
 * Remove an encryption / decryption state object.
 *
 * Arguments:	ctx	Pointer to the encryption / decryption state object to remove.
 */
void keystore_remove_cipher_ctx(gtm_cipher_ctx_t *ctx)
{
	gtm_cipher_ctx_t *next, *prev;

	assert(NULL != ctx);
	gc_sym_destroy_cipher_handle(ctx->handle);
	next = ctx->next;
	prev = ctx->prev;
	if (NULL != prev)
		prev->next = next;
	if (NULL != next)
		next->prev = prev;
	if (ctx->store->cipher_head == ctx)
		ctx->store->cipher_head = next;
	if (ctx->store->db_cipher_entry == ctx)
		ctx->store->db_cipher_entry = NULL;
	FREE(ctx);
}

/*
 * Clean up all key and encryption / decryption state contexts.
 */
void gtm_keystore_cleanup_all()
{
	if (NULL != keystore_by_hash_head)
	{
		gtm_keystore_cleanup_hash_tree(keystore_by_hash_head);
		keystore_by_hash_head = NULL;
	}
	if (NULL != keystore_by_keyname_head)
	{
		gtm_keystore_cleanup_keyname_tree(keystore_by_keyname_head);
		keystore_by_keyname_head = NULL;
	}
	if (NULL != keystore_by_unres_key_head)
	{
		gtm_keystore_cleanup_unres_key_list(keystore_by_unres_key_head);
		keystore_by_unres_key_head = NULL;
	}
}

/*
 * Clean up a particular key object and all its encryption / decryption state objects.
 *
 * Arguments:	node	Key object to clean.
 */
STATICFNDEF void gtm_keystore_cleanup_node(gtm_keystore_t *node)
{
	gtm_cipher_ctx_t *curr, *temp;

	curr = node->cipher_head;
	while (NULL != curr)
	{
		temp = curr->next;
		gc_sym_destroy_cipher_handle(curr->handle);
		FREE(curr);
		curr = temp;
	}
	memset(node->key, 0, SYMMETRIC_KEY_MAX);
	memset(node->key_hash, 0, GTMCRYPT_HASH_LEN);
	FREE(node);
}

/*
 * Clean up (recursively) a binary search tree for looking up keys by their hashes.
 *
 * Arguments:	entry	Pointer to the node from which to descend for cleaning.
 */
STATICFNDEF void gtm_keystore_cleanup_hash_tree(gtm_keystore_hash_link_t *entry)
{
	gtm_keystore_hash_link_t *curr;

	while (TRUE)
	{
		if (NULL != entry->left)
			gtm_keystore_cleanup_hash_tree(entry->left);
		gtm_keystore_cleanup_node(entry->link);
		curr = entry;
		if (NULL != entry->right)
			entry = entry->right;
		else
			break;
		FREE(curr);
	}
}

/*
 * Clean up (recursively) a binary search tree for looking up keys by their names.
 *
 * Arguments:	entry	Pointer to the node from which to descend for cleaning.
 */
STATICFNDEF void gtm_keystore_cleanup_keyname_tree(gtm_keystore_keyname_link_t *entry)
{
	gtm_keystore_keyname_link_t *curr;

	while (TRUE)
	{
		if (NULL != entry->left)
			gtm_keystore_cleanup_keyname_tree(entry->left);
		curr = entry;
		if (NULL != entry->right)
			entry = entry->right;
		else
			break;
		FREE(curr);
	}
}

/*
 * Clean up (linearly) an unresolved keys list.
 *
 * Arguments:	entry	Pointer to the node from which to continue cleaning.
 */
STATICFNDEF void gtm_keystore_cleanup_unres_key_list(gtm_keystore_unres_key_link_t *entry)
{
	gtm_keystore_unres_key_link_t *curr;

	while (NULL != entry)
	{
		curr = entry;
		entry = entry->next;
		FREE(curr);
	}
}
