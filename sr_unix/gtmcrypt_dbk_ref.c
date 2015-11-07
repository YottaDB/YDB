/****************************************************************
 *								*
 *	Copyright 2009, 2014 Fidelity Information Services, Inc *
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

STATICDEF int					n_keys;					/* Count of how many keys were loaded. */
STATICDEF char					gc_config_filename[GTM_PATH_MAX];	/* Path to the configuration file. */
STATICDEF gtm_keystore_hash_link_t		*keystore_by_hash_head;			/* Root of the binary search tree to look
											 * keys up by hash. */
STATICDEF gtm_keystore_keyname_link_t		*keystore_by_keyname_head;		/* Root of the binary search tree to look
											 * keys up by name. */
STATICDEF gtm_keystore_unres_keyname_link_t	*keystore_by_unres_keyname_head;	/* Head of the linked list holding keys of
											 * DBs with presently unresolved paths. */
STATICDEF config_t				gtmcrypt_cfg;				/* Encryption configuration. */
STATICDEF char					db_real_path[GTM_PATH_MAX];		/* Array for temporary storage of DBs' real
											 * path information. */
STATICDEF unsigned char				key_hash[GTMCRYPT_HASH_LEN];		/* Array for temporary storage of keys'
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
 * 		nulled		Flag indicating whether keyname is null-terminated.
 *
 * Returns:	0 if the key with the specified name is found; -1 otherwise.
 */
int gtmcrypt_getkey_by_keyname(char *keyname, int length, gtm_keystore_t **entry, int database, int nulled)
{
	int new_db_keynames, new_dev_keynames, new_hashes, new_keynames, error;

	if (NULL == (*entry = keystore_lookup_by_keyname(keyname, length, nulled)))
	{	/* Lookup still failed. Verify if we have right permissions on GNUPGHOME or $HOME/.gnupg (if GNUPGHOME is unset).
		 * If not, then the below function will store the appropriate error message in err_string and so return -1.
		 */
		if (0 != gc_pk_gpghome_has_permissions())
			return -1;
		/* Hashes are irrelevant, so using the same variable for number of both device and DB hashes. */
		if (0 != keystore_refresh(&new_db_keynames, &new_hashes, &new_dev_keynames, &new_hashes))
			return -1;
		error = 0;
		new_keynames = database ? new_db_keynames : new_dev_keynames;
		if ((0 >= new_keynames) || ((0 < new_keynames)
				&& (NULL == (*entry = keystore_lookup_by_keyname(keyname, length, nulled)))))
		{	/* If either no keynames have been loaded, or the key is not found among those that were loaded, try the
			 * unresolved keys list.
			 */
			if (NULL == (*entry = keystore_lookup_by_unres_keyname(keyname, &error)))
			{
				if (!error)
				{
					UPDATE_ERROR_STRING("%s " STR_ARG " missing in configuration file or does not exist",
						(database ? "Database file" : "Keyname"), ELLIPSIZE(keyname));
				}
				return -1;
			}
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
	int		err_caused_by_gpg;
	char		save_err[MAX_GTMCRYPT_ERR_STRLEN], hex_buff[GTMCRYPT_HASH_HEX_LEN + 1];
	char		*alert_msg;
	int		new_db_hashes, new_dev_hashes, new_keynames, new_hashes;

	if (NULL == (*entry = keystore_lookup_by_hash(hash)))
	{	/* Lookup still failed. Verify if we have right permissions on GNUPGHOME or $HOME/.gnupg (if GNUPGHOME is unset).
		 * If not, then the below function will store the appropriate error message in err_string and so return -1.
		 */
		if (0 != gc_pk_gpghome_has_permissions())
			return -1;
		/* Keynames are irrelevant, so using the same variable for number of both device and DB keynames. */
		if (0 != keystore_refresh(&new_keynames, &new_db_hashes, &new_keynames, &new_dev_hashes))
			return -1;
		new_hashes = new_db_hashes + new_dev_hashes;
		if ((0 >= new_hashes) || ((0 < new_hashes) && (NULL == (*entry = keystore_lookup_by_hash(hash)))))
		{	/* If either no hashes have been loaded, or the key is not found among those that were loaded, try the
			 * unresolved keys list.
			 */
			if (NULL == (*entry = keystore_lookup_by_unres_keyname_hash(hash)))
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
				return -1;
			}
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
 * 		nulled		Indicates whether keyname is null-terminated.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_keyname(char *keyname, int length, int nulled)
{
	LOOKUP_KEY(keystore_by_keyname_head, gtm_keystore_keyname_link_t, key_name, keyname, length, !nulled);
}

/*
 * Helper function to perform a linear search of the key by its name in the unresolved keys list. It attempts to resolve the real
 * path of every node's keyname, assuming that it corresponds to a previously unresolved database name. If the path is resolved, the
 * node's entry is used to create (as needed) new key node as well as hash- and keyname-based links to it, and the unresolved entry
 * is removed from the list.
 *
 * Arguments:	keyname		Name of the key.
 * 		error		Address where to set the flag indicating whether an error was encountered.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_unres_keyname(char *keyname, int *error)
{
	gtm_keystore_unres_keyname_link_t	*curr, *prev, *next;
	gtm_keystore_t				*node, *result;
	int					length;

	result = NULL;
	prev = NULL;
	curr = keystore_by_unres_keyname_head;
	while (curr)
	{
		next = curr->next;
		if (NULL != realpath(curr->key_name, db_real_path))
		{	/* It is possible that a newly resolved realpath points to a previously seen database file, in which case we
			 * should first check whether that database has already been inserted into the tree to avoid inserting a
			 * duplicate.
			 */
			length = strlen(db_real_path);
			assert(length < GTM_PATH_MAX);
			if (NULL != (node = keystore_lookup_by_keyname(db_real_path, length + 1, TRUE)))
			{	/* If we have already loaded a different key for this database, issue an error. */
				if (memcmp(node->key_hash, curr->key_hash, GTMCRYPT_HASH_LEN))
				{
					*error = TRUE;
					UPDATE_ERROR_STRING("Database file " STR_ARG " resolves to a previously seen file, but "
						"specifies a different key", ELLIPSIZE(curr->key_name));
					return NULL;
				}
			} else
			{	/* It is possible that while no key has been specified for this database, the same key has already
				 * been loaded for a different database or device, so do a lookup first.
				 */
				if (NULL == (node = keystore_lookup_by_hash(curr->key_hash)))
				{	/* If we have not seen this hash before, create new entries. */
					GC_ALLOCATE_KEYSTORE_ENTRY(node);
					memcpy(node->key, curr->key, SYMMETRIC_KEY_MAX);
					/* This should take care of assigning key_hash to the node itself. */
					INSERT_KEY_LINK(keystore_by_hash_head, node, gtm_keystore_hash_link_t, link->key_hash,
							curr->key_hash, GTMCRYPT_HASH_LEN, GTMCRYPT_HASH_LEN);
				}
				INSERT_KEY_LINK(keystore_by_keyname_head, node, gtm_keystore_keyname_link_t,
						key_name, db_real_path, length + 1, GTM_PATH_MAX);
			}
			/* If we have not found a suitable node before, and this path matches, set the result to that. Do not break
			 * the loop, though, as we want to resolve as many previously unresolved paths as possible. And if it
			 * happens that some other real path matches the one we already found, we will already have it in the
			 * resolved entries' tree, so it will simply get removed from the unresolved list.
			 */
			if ((NULL == result) && (!strcmp(keyname, db_real_path)))
				result = node;
			if (NULL != prev)
				prev->next = next;
			if (curr == keystore_by_unres_keyname_head)
				keystore_by_unres_keyname_head = next;
			FREE(curr);
		} else
		{
			if (ENOENT == errno)
			{	/* If we still could not resolve the path, move on to the next element. */
				prev = curr;
			} else if (ENAMETOOLONG == errno)
			{
				UPDATE_ERROR_STRING("Real path, or a component of the path, of the database " STR_ARG
					" is too long", ELLIPSIZE(curr->key_name));
				return NULL;
			} else
			{
				UPDATE_ERROR_STRING("Could not obtain the real path of the database " STR_ARG,
					ELLIPSIZE(curr->key_name));
				return NULL;
			}
		}
		curr = next;
	}
	return result;
}

/*
 * Helper function to perform a linear search of the key by its hash in the unresolved keys list.
 *
 * Arguments:	hash	Hash of the key.
 *
 * Returns:	Pointer to the key, if found; NULL otherwise.
 */
STATICFNDEF gtm_keystore_t *keystore_lookup_by_unres_keyname_hash(unsigned char *hash)
{
	gtm_keystore_unres_keyname_link_t	*curr;
	gtm_keystore_t				*node, *result;

	result = NULL;
	/* Unlike with unresolved key lookups by the keyname, we will not attempt to resolve the realpath of the keyname (database
	 * file) in question. All we are interested in is a matching hash, so do a linear search thereof, and, if found, do not
	 * delete the entry, but simply create a corresponding keystore entry and a pointer from the hash-based lookup tree.
	 */
	curr = keystore_by_unres_keyname_head;
	while (curr)
	{
		if (!memcmp(curr->key_hash, hash, GTMCRYPT_HASH_LEN))
		{	/* Assumption is that a lookup by hash has already been done and not yielded any result. */
			GC_ALLOCATE_KEYSTORE_ENTRY(node);
			memcpy(node->key, curr->key, SYMMETRIC_KEY_MAX);
			/* This should take care of assigning key_hash to the node itself. */
			INSERT_KEY_LINK(keystore_by_hash_head, node, gtm_keystore_hash_link_t, link->key_hash, curr->key_hash,
					GTMCRYPT_HASH_LEN, GTMCRYPT_HASH_LEN);
			result = node;
			break;
		}
		curr = curr->next;
	}
	return result;
}

/*
 * Re-read the configuration file, if necessary, and store any previously unseen keys in memory. If the configuration file has not
 * been modified since the last successful read, then it is not processed, and the counts for newly loaded DB and device keyname and
 * hash entries are set to -1.
 *
 * Arguments:	new_db_keynames		Address where to place the number of added DB keyname entries.
 * 		new_db_hashes		Address where to place the number of added DB hash entries.
 * 		new_dev_keynames	Address where to place the number of added device keyname entries.
 * 		new_dev_hashes		Address where to place the number of added device hash entries.
 *
 * Returns:	0 if succeeded re-reading the configuration file; -1 otherwise.
 */
STATICFNDEF int keystore_refresh(int *new_db_keynames, int *new_db_hashes, int *new_dev_keynames, int *new_dev_hashes)
{
	int			n_mappings, just_read;
	char			*config_env;
	struct stat		stat_info;
	static long		last_modified_s, last_modified_ns;
	gtm_keystore_t		*node;

	just_read = FALSE;
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
		*new_db_keynames = *new_db_hashes = *new_dev_keynames = *new_dev_hashes = -1;
		return 0;
	}
	/* File has been modified, so re-read it. */
	if (!config_read_file(&gtmcrypt_cfg, gc_config_filename))
	{
		UPDATE_ERROR_STRING("Cannot read config file " STR_ARG ". At line# %d - %s", ELLIPSIZE(gc_config_filename),
					config_error_line(&gtmcrypt_cfg), config_error_text(&gtmcrypt_cfg))
		return -1;
	}
	/* Check and update the value of gtm_passwd if it has changed since we last checked. This way, if the user had originally
	 * entered a wrong password, but later changed the value (possible in MUMPS using external call), we read the up-to-date
	 * value instead of issuing an error.
	 */
	if (0 != gc_update_passwd(GTM_PASSWD_ENV, &gtmcrypt_pwent, GTMCRYPT_DEFAULT_PASSWD_PROMPT,
					GTMCRYPT_OP_INTERACTIVE_MODE & gtmcrypt_init_flags))
	{
		return -1;
	}
	/* Clear the entire unresolved keys list because it will be rebuilt. */
	REMOVE_UNRESOLVED_KEY_LINKS;
	/* The configuration file has two sections that we are interested in. The "database" section which contains a mapping of
	 * the database filenames and their corresponding key files and the "files" section which contains a mapping of regular
	 * files (read and written by GT.M) and their corresponding key files. Read both the sections to create the key and
	 * encryption / decryption structures as outlined in gtmcrypt_dbk_ref.h.
	 */
	*new_db_keynames = *new_db_hashes = *new_dev_keynames = *new_dev_hashes = 0;
	if (-1 == read_database_section(&gtmcrypt_cfg, &n_mappings, new_db_keynames, new_db_hashes))
		return -1;
	n_keys += n_mappings;
	if (-1 == read_files_section(&gtmcrypt_cfg, &n_mappings, new_dev_keynames, new_dev_hashes))
		return -1;
	n_keys += n_mappings;
	/* If we update the modified date before we go through the configuration and validate everything in it, any error might
	 * cause the unresolved list to not be built and configuration file to not subsequently be reread.
	 */
	last_modified_s = (long)stat_info.st_mtime;
	last_modified_ns = (long)stat_info.st_nmtime;
	assert((0 == n_keys) ||
		((NULL != keystore_by_hash_head) && (NULL != keystore_by_keyname_head)) ||
		(NULL != keystore_by_unres_keyname_head));
	if (0 == n_keys)
	{
		UPDATE_ERROR_STRING("Configuration file " STR_ARG " contains neither 'database.keys' section nor 'files' section, "
			"or both sections are empty.", ELLIPSIZE(gc_config_filename));
		return -1;
	}
	return 0;
}

/*
 * Process the 'files' section of the configuration file, storing any previously unseen key.
 *
 * Arguments:	cfgp		Pointer to the configuration object as populated by libconfig.
 * 		n_mappings	Pointer to a variable where to place the number of key entries found in the file.
 * 		new_keynames	Pointer to a variable where to place the number of key references by keyname added to storage.
 * 		new_hashes	Pointer to a variable where to place the number of key references by hash added to storage.
 *
 * Returns:	0 if successfully processed the 'files' section; -1 otherwise.
 */
STATICFNDEF int read_files_section(config_t *cfgp, int *n_mappings, int *new_keynames, int *new_hashes)
{
	int			i, length, lcl_n_maps, raw_key_length;
	config_setting_t	*setting, *elem;
	gtm_keystore_t		*node;
	char			*name, *key;
	unsigned char		raw_key[SYMMETRIC_KEY_MAX];

	*new_keynames = *new_hashes = 0;
	if (NULL == (setting = config_lookup(cfgp, "files")))
	{
		*n_mappings = 0;
		return 0;
	}
	lcl_n_maps = config_setting_length(setting);
	for (i = 0; i < lcl_n_maps; i++)
	{
		elem = config_setting_get_elem(setting, i);
		assert(NULL != elem);
		assert(CONFIG_TYPE_STRING == config_setting_type(elem));
		if (NULL == (name = config_setting_name(elem)))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG
				", entry #%d corresponding to 'files' does not have a key attribute",
				ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		/* Length should be under GTM_PATH_MAX because that is the size of the array where the name of a key is stored. */
		length = strlen(name);
		if (GTM_PATH_MAX <= length)
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d's field length exceeds %d",
						ELLIPSIZE(gc_config_filename), i + 1, GTM_PATH_MAX - 1);
			return -1;
		}
		if (NULL == (key = (char *)config_setting_get_string(elem)))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", cannot find the value corresponding to 'files.%s'",
						ELLIPSIZE(gc_config_filename), name);
			return -1;
		}
		/* Now that we have the name of the symmetric key file, try to decrypt it. If gc_pk_get_decrypted_key returns a
		 * non-zero status, it should have already populated the error string.
		 */
		if (0 != gc_pk_get_decrypted_key(key, raw_key, &raw_key_length))
			return -1;
		if (0 == raw_key_length)
		{
			UPDATE_ERROR_STRING("Symmetric key " STR_ARG " found to be empty", ELLIPSIZE(key));
			return -1;
		}
		/* We expect a symmetric key within a certain length. */
		assert(SYMMETRIC_KEY_MAX >= raw_key_length);
		/* For most key operations we need a hash, so compute it now. */
		GC_PK_COMPUTE_HASH(key_hash, raw_key);
		/* Make sure we have not previously specified a different key for the same device. */
		if (NULL != (node = keystore_lookup_by_keyname(name, length + 1, TRUE)))
		{
			if (memcmp(node->key_hash, key_hash, GTMCRYPT_HASH_LEN))
			{
				UPDATE_ERROR_STRING("In config file " STR_ARG ", the key 'files." STR_ARG
					"' has already been seen, but specifies a different key",
					ELLIPSIZE(gc_config_filename), ELLIPSIZE(name));
				return -1;
			} else
				continue;
		}
		/* It is possible that while no key has been specified under this name, the same key has already been loaded
		 * for a different database or device, so look up the key by hash to avoid duplicates.
		 */
		if (NULL == (node = keystore_lookup_by_hash(key_hash)))
		{
			GC_ALLOCATE_KEYSTORE_ENTRY(node);
			/* WARNING: Not doing a memset here because raw_key comes padded with NULLs from gc_pk_get_decrypted_key. */
			memcpy(node->key, raw_key, SYMMETRIC_KEY_MAX);
			/* This should take care of assigning key_hash to the node itself. */
			INSERT_KEY_LINK(keystore_by_hash_head, node, gtm_keystore_hash_link_t, link->key_hash, key_hash,
					GTMCRYPT_HASH_LEN, GTMCRYPT_HASH_LEN);
			(*new_hashes)++;
		}
		INSERT_KEY_LINK(keystore_by_keyname_head, node, gtm_keystore_keyname_link_t,
				key_name, name, length + 1, GTM_PATH_MAX);
		(*new_keynames)++;
	}
	*n_mappings = lcl_n_maps;
	return 0;
}

/*
 * Process the 'database' section of the configuration file, storing any previously unseen key.
 *
 * Arguments:	cfgp		Pointer to the configuration object as populated by libconfig.
 * 		n_mappings	Pointer to a variable where to place the number of key entries found in the file.
 * 		new_keynames	Pointer to a variable where to place the number of key references by keyname added to storage.
 * 		new_hashes	Pointer to a variable where to place the number of key references by hash added to storage.
 *
 * Returns:	0 if successfully processed the 'database' section; -1 otherwise.
 */
STATICFNDEF int read_database_section(config_t *cfgp, int *n_mappings, int *new_keynames, int *new_hashes)
{
	int			i, length, lcl_n_maps, raw_key_length;
	int			defer, key_found_by_filename;
	config_setting_t	*setting, *elem;
	gtm_keystore_t		*node;
	char			*name, *key;
	unsigned char		raw_key[SYMMETRIC_KEY_MAX];

	*new_keynames = *new_hashes = 0;
	if (NULL == (setting = config_lookup(cfgp, "database.keys")))
	{
		*n_mappings = 0;
		return 0;
	}
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
		if (!config_setting_lookup_string(elem, "dat", (const char **)&name))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d corresponding to "
				"database.keys does not have a 'dat' item", ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		/* Length should be under GTM_PATH_MAX because that is the size of the array where the name of a key is stored. */
		if (GTM_PATH_MAX <= strlen(name))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d's database file name exceeds %d",
						ELLIPSIZE(gc_config_filename), i + 1, GTM_PATH_MAX - 1);
			return -1;
		}
		/* Database might not exist yet, in which case set the flag to place the entry in unresolved databases list and not
		 * error out.
		 */
		if (NULL == realpath(name, db_real_path))
		{
			if (ENOENT == errno)
				defer = 1;
			else if (ENAMETOOLONG == errno)
			{
				UPDATE_ERROR_STRING("In config file " STR_ARG ", the real path, or a component of the path, of the "
					"database in entry #%d of database.keys section is too long",
					ELLIPSIZE(gc_config_filename), i + 1);
				return -1;
			} else
			{
				UPDATE_ERROR_STRING("In config file " STR_ARG ", could not obtain the real path of the database in "
					"entry #%d of database.keys section", ELLIPSIZE(gc_config_filename), i + 1);
				return -1;
			}
		} else
			defer = 0;
		if (!config_setting_lookup_string(elem, "key", (const char **)&key))
		{
			UPDATE_ERROR_STRING("In config file " STR_ARG ", entry #%d corresponding to "
				"database.keys does not have a 'key' item", ELLIPSIZE(gc_config_filename), i + 1);
			return -1;
		}
		/* Now that we have the name of the symmetric key file, try to decrypt it. If gc_pk_get_decrypted_key returns a
		 * non-zero status, it should have already populated the error string.
		 */
		if (0 != gc_pk_get_decrypted_key(key, raw_key, &raw_key_length))
			return -1;
		if (0 == raw_key_length)
		{
			UPDATE_ERROR_STRING("Symmetric key " STR_ARG " found to be empty", ELLIPSIZE(key));
			return -1;
		}
		/* We expect a symmetric key within a certain length. */
		assert(SYMMETRIC_KEY_MAX >= raw_key_length);
		/* For most key operations we need a hash, so compute it now. */
		GC_PK_COMPUTE_HASH(key_hash, raw_key);
		/* Depending on whether we are deferring the key storage and error processing, either verify that the key is unique
		 * and create proper references or place it in the unresolved keys list.
		 */
		if (!defer)
		{
			length = strlen(db_real_path);
			assert(length < GTM_PATH_MAX);
			/* Make sure we have not previously specified a different key for the same database. */
			if (NULL != (node = keystore_lookup_by_keyname(db_real_path, length + 1, TRUE)))
			{
				if (memcmp(node->key_hash, key_hash, GTMCRYPT_HASH_LEN))
				{
					UPDATE_ERROR_STRING("In config file " STR_ARG ", the database file in entry #%d resolves "
						"to a previously seen file, but specifies a different key",
						ELLIPSIZE(gc_config_filename), i + 1);
					return -1;
				} else
					continue;
			}
			/* It is possible that while no key has been specified for this database, the same key has already been
			 * loaded for a different database or device, so look up the key by hash to avoid duplicates.
			 */
			if (NULL == (node = keystore_lookup_by_hash(key_hash)))
			{
				GC_ALLOCATE_KEYSTORE_ENTRY(node);
				/* WARNING: Not doing a memset here because raw_key comes padded with NULLs from
				 * gc_pk_get_decrypted_key.
				 */
				memcpy(node->key, raw_key, SYMMETRIC_KEY_MAX);
				/* This should take care of assigning key_hash to the node itself. */
				INSERT_KEY_LINK(keystore_by_hash_head, node, gtm_keystore_hash_link_t, link->key_hash, key_hash,
						GTMCRYPT_HASH_LEN, GTMCRYPT_HASH_LEN);
				(*new_hashes)++;
			}
			INSERT_KEY_LINK(keystore_by_keyname_head, node, gtm_keystore_keyname_link_t, key_name, db_real_path,
					length + 1, GTM_PATH_MAX);
			(*new_keynames)++;
		} else
		{	/* WARNING: Not passing the raw key length here because raw_key comes padded with NULLs from
			 * gc_pk_get_decrypted_key and there is no need to know its length.
			 */
			INSERT_UNRESOLVED_KEY_LINK(raw_key, key_hash, name);
		}
	}
	*n_mappings = lcl_n_maps;
	return 0;
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
	if (NULL != keystore_by_unres_keyname_head)
	{
		gtm_keystore_cleanup_unres_keyname_list(keystore_by_unres_keyname_head);
		keystore_by_unres_keyname_head = NULL;
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
STATICFNDEF void gtm_keystore_cleanup_unres_keyname_list(gtm_keystore_unres_keyname_link_t *entry)
{
	gtm_keystore_unres_keyname_link_t *curr;

	while (NULL != entry)
	{
		curr = entry;
		memset(entry->key, 0, SYMMETRIC_KEY_MAX);
		memset(entry->key_hash, 0, GTMCRYPT_HASH_LEN);
		entry = entry->next;
		FREE(curr);
	}
}
