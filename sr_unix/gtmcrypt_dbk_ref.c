/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc 	*
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
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include <libconfig.h>

#include "gtmxc_types.h"

#include "gtmcrypt_util.h"
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"

#define NEWLINE			0x0A

#define PARSE_ERROR_PREFIX	"Error parsing database key file"
#define LOOKING_FOR_DB		0x1
#define LOOKING_FOR_KEY		0x2

#define PARSE_COMPLETED		0x1
#define KEEP_GOING		0x2

STATICDEF int			n_dbkeys;
STATICDEF char			gc_dbk_filename[GTM_PATH_MAX];
STATICDEF gtm_dbkeys_tbl	*tbl_head;
STATICDEF config_t		gtmcrypt_cfg;
GBLDEF gtm_dbkeys_tbl		**fast_lookup_entry;
GBLDEF int			gc_dbk_file_format;

GBLREF	passwd_entry_t		*gtmcrypt_pwent;
GBLREF	int			gtmcrypt_init_flags;

/* Free up the linked list of database-symmetric key association AFTER scrubbing off the contents of the raw symmetric key and
 * its corresponding hash
 */
void gc_dbk_scrub_entries()
{
	gtm_dbkeys_tbl	*cur, *temp;

	cur = tbl_head;
	while (NULL != cur)
	{
#		ifdef USE_GCRYPT
		if (!cur->symmetric_key_dirty)
		{
			if (cur->encr_key_handle)
				gcry_cipher_close(cur->encr_key_handle);
			if (cur->decr_key_handle)
				gcry_cipher_close(cur->decr_key_handle);
		}
#		endif
		temp = cur->next;
		GTM_XCFILEID_FREE(cur->fileid);
		memset(cur->symmetric_key, 0, SYMMETRIC_KEY_MAX);
		memset(cur->symmetric_key_hash, 0, GTMCRYPT_HASH_LEN);
		FREE(cur);
		cur = temp;
	}
	if (NULL != fast_lookup_entry)
		FREE(fast_lookup_entry);
	n_dbkeys = 0;
}

/* Given a fileid, containing a unique description of the dat file, the function searches for it's
 * entry in the linked list. On unsuccessful search, returns NULL.
 */
gtm_dbkeys_tbl* gc_dbk_get_entry_by_fileid(gtm_fileid_ptr_t fileid)
{
	gtm_dbkeys_tbl *cur = tbl_head;

	while (NULL != cur)
	{
		if (!cur->fileid_dirty && (!cur->symmetric_key_dirty) && (gtm_is_file_identical(fileid, cur->fileid)))
			break;
		cur = (gtm_dbkeys_tbl *)cur->next;
	}
	return cur;
}

/* Given a hash, the function returns the entry in the linked list that matches with the given hash. Otherwise, NULL is returned */
gtm_dbkeys_tbl* gc_dbk_get_entry_by_hash(gtm_string_t *hash)
{
	gtm_dbkeys_tbl *cur = tbl_head;

	assert(hash && (hash->length == GTMCRYPT_HASH_LEN));
	while (NULL != cur)
	{
		if ((hash->length == GTMCRYPT_HASH_LEN) && (0 == memcmp(hash->address, cur->symmetric_key_hash, GTMCRYPT_HASH_LEN)))
			break;
		cur = cur->next;
	}
	return cur;
}

STATICFNDEF int	gc_dbk_get_dbkeys_fname(char *fname, int *stat_success)
{
	char			*ptr;
	struct stat		stat_buf;

	if (NULL != (ptr = getenv(GTM_DBKEYS)))
	{
		if (0 == STRLEN(ptr))
		{
			UPDATE_ERROR_STRING(ENV_EMPTY_ERROR, GTM_DBKEYS);
			return GC_FAILURE;
		} else if (0 == stat(ptr, &stat_buf)) /* See if the environment variable points to a proper path */
		{
			if (S_ISDIR(stat_buf.st_mode)) /* if directory */
			{
				SNPRINTF(fname, GTM_PATH_MAX, "%s/%s", ptr, DOT_GTM_DBKEYS);
			} else if (S_ISREG(stat_buf.st_mode)) /* if file */
			{
				SNPRINTF(fname, GTM_PATH_MAX, "%s", ptr);
			} else
			{
				assert(FALSE);
				UPDATE_ERROR_STRING("%s is neither a directory nor a regular file", ptr);
				return GC_FAILURE;
			}
		} else if (ENOENT == errno)
		{	/* File doesn't exist */
			UPDATE_ERROR_STRING("Cannot find DB keys file - %s", ptr);
			return GC_FAILURE;
		} else
		{	/* Some other error */
			UPDATE_ERROR_STRING("Cannot find DB keys file - %s. %s", ptr, strerror(errno));
			return GC_FAILURE;
		}
	} else if (NULL != (ptr = getenv(HOME)))
	{
		SNPRINTF(fname, GTM_PATH_MAX, "%s/%s", ptr, DOT_GTM_DBKEYS);
	} else
	{
		UPDATE_ERROR_STRING("Neither $"GTM_DBKEYS "nor $"HOME " is defined");
		return GC_FAILURE;
	}
	if (0 != stat(fname, &stat_buf))
	{
		*stat_success = FALSE;
		if (ENOENT == errno)
		{
			UPDATE_ERROR_STRING("Cannot find DB keys file - %s", fname);
		} else
		{
			UPDATE_ERROR_STRING("Cannot find DB keys file - %s. %s", fname, strerror(errno));
		}
		return GC_FAILURE;
	}
	*stat_success = TRUE;
	return GC_SUCCESS;
}

STATICFNDEF int gc_dbk_get_single_entry(void *handle, char **db, char **key, int n)
{
	FILE			*fp;
	int			current_state, space_cnt, line_type, filename_len, buflen;
	char			buf[LINE_MAX];
	static int		line_no;
	config_setting_t	*db_setting, *elem;

	/* The caller makes sure that the file format at this point is either the old $gtm_dbkeys flat file format or the new
	 * libconfig file format. Assert accordingly. In PRO, if the file format is neither, we fall-through anyways and attempt
	 * to parse the flat-file provides an appropriate error message to the user.
	 */
	assert((LIBCONFIG_FILE_FORMAT == gc_dbk_file_format) || (DBKEYS_FILE_FORMAT == gc_dbk_file_format));
	if (LIBCONFIG_FILE_FORMAT == gc_dbk_file_format)
	{
		db_setting = (config_setting_t *)handle;
		if (n + 1 == config_setting_length(db_setting))
			return PARSE_COMPLETED;
		if (NULL != (elem = config_setting_get_elem(db_setting, n)))
		{
			if (!config_setting_lookup_string(elem, "dat", (const char **)db))
			{
				UPDATE_ERROR_STRING("In config file %s, entry# %d corresponding to database.keys "
							"does not have a `dat' item", gc_dbk_filename, n + 1);
				return -1;
			}

			if (!config_setting_lookup_string(elem, "key", (const char **)key))
			{
				UPDATE_ERROR_STRING("In config file %s, entry# %d corresponding to database.keys "
							"does not have a `key' item", gc_dbk_filename, n + 1);
				return -1;
			}
			return KEEP_GOING;
		}
		assert(FALSE);	/* We should never reach here as that would mean we couldn't find the nth entry in database.keys. */
	}
	/* Fall-through: Old $gtm_dbkeys flat file format. Read a single pair of database-key entry. */
	fp = (FILE *)handle;
	current_state = LOOKING_FOR_DB;
	while (!feof(fp))
	{
		if (NULL == fgets(buf, LINE_MAX, fp))
			break;
		line_no++;
		buflen = STRLEN(buf);
		if (NEWLINE != buf[buflen - 1])
		{	/* Last character in the buffer is not a newline implying that the line contains more than LINE_MAX
			 * characters.
			 */
			UPDATE_ERROR_STRING("%s. Entry at line: %d longer than %ld characters", PARSE_ERROR_PREFIX, line_no,
							LINE_MAX);
			return -1;
		}
		buf[--buflen] = '\0'; /* strip off the newline at the end */
		for (space_cnt = 0; isspace(buf[space_cnt]); space_cnt++)	/* BYPASSOK -- cannot use ISSPACE */
			;
		assert(space_cnt <= buflen);
		if ((0 == space_cnt) && ('\0' != buf[0]))
		{
			if (0 == memcmp(buf, DATABASE_LINE_INDICATOR, DATABASE_LINE_INDICATOR_SIZE))
			{
				filename_len = buflen - DATABASE_LINE_INDICATOR_SIZE;
				line_type = DATABASE_LINE_INFO;
			} else if (0 == memcmp(buf, SYMMETRIC_KEY_LINE_INDICATOR, SYMMETRIC_KEY_LINE_INDICATOR_SIZE))
			{
				filename_len = buflen - SYMMETRIC_KEY_LINE_INDICATOR_SIZE;
				line_type = SYMMETRIC_KEY_LINE_INFO;
			} else
				line_type = -1;
		} else if (space_cnt < buflen)
			line_type = -1;		/* line doesn't consist entirely of spaces (but only has leading spaces) */
		else
			continue;	/* skip this line as it consists entirely of spaces -- blank line */
		switch (line_type)
		{
			case DATABASE_LINE_INFO:
				if (LOOKING_FOR_KEY == current_state)
				{
					UPDATE_ERROR_STRING("%s. At line %d: Found DAT entry, expecting KEY entry",
								PARSE_ERROR_PREFIX, line_no);
					return -1;
				}
				memcpy(*db, &buf[DATABASE_LINE_INDICATOR_SIZE], filename_len + 1);
				assert('\0' == *(*db + filename_len));
				current_state = LOOKING_FOR_KEY;
				break;

			case SYMMETRIC_KEY_LINE_INFO:
				if (LOOKING_FOR_DB == current_state)
				{
					UPDATE_ERROR_STRING("%s. At line %d: Found KEY entry, expecting DAT entry",
								PARSE_ERROR_PREFIX, line_no);
					return -1;
				}
				memcpy(*key, &buf[SYMMETRIC_KEY_LINE_INDICATOR_SIZE], filename_len + 1);
				assert('\0' == *(*key + filename_len));
				current_state = LOOKING_FOR_DB;
				return KEEP_GOING;	/* Done reading a single entry. */

			default:
				UPDATE_ERROR_STRING("%s. At line %d: %s does not start with 'dat '/'key '", PARSE_ERROR_PREFIX,
							line_no, buf);
				return -1;
		}
	}
	if (!feof(fp))
	{
		UPDATE_ERROR_STRING("Error while reading from database key file. %s", strerror(errno));
		return -1;
	} else if (0 == line_no)
	{
		UPDATE_ERROR_STRING("%s. No entries found in DB keys file.", PARSE_ERROR_PREFIX);
		return -1;
	} else if (LOOKING_FOR_KEY == current_state)
	{
		UPDATE_ERROR_STRING("%s. No matching KEY entry found for DAT entry at line: %d", PARSE_ERROR_PREFIX, line_no);
		return -1;
	}
	return PARSE_COMPLETED;
}

int gc_dbk_init_dbkeys_tbl()
{
	FILE			*fp;
	void			*handle;
	int			count, status, ret, stat_success;
	char			err[MAX_GTMCRYPT_ERR_STRLEN], *db, *key, db_fname[GTM_PATH_MAX], key_fname[GTM_PATH_MAX];
	char			*config_env;
	struct stat		stat_info;
	static time_t		last_modified;
	config_setting_t	*setting;
	gtm_dbkeys_tbl		*node;

	if (INVALID_FILE_FORMAT == gc_dbk_file_format)
	{	/* Decide which file format to use: Old format ($gtm_dbkeys) or the new format ($gtmcrypt_config). */
		if ((GC_SUCCESS != gc_dbk_get_dbkeys_fname(gc_dbk_filename, &stat_success)) || !stat_success)
		{
			if (NULL != (config_env = getenv("gtmcrypt_config")))
			{
				strncpy(err, gtmcrypt_err_string, MAX_GTMCRYPT_ERR_STRLEN);
				if (0 != stat(config_env, &stat_info))
				{
					UPDATE_ERROR_STRING("Failed to open $gtm_dbkeys. Reason: %s; attempt to read alternate "
								"config file %s failed as well. Reason: %s", err, config_env,
								strerror(errno));
					return GC_FAILURE;
				}
			} else
				return GC_FAILURE;	/* Error string is already updated in gc_dbk_get_dbkeys_fname. */
			strncpy(gc_dbk_filename, config_env, GTM_PATH_MAX);
			gc_dbk_file_format = LIBCONFIG_FILE_FORMAT;
		} else
			gc_dbk_file_format = DBKEYS_FILE_FORMAT;
	}
	assert('\0' != gc_dbk_filename[0]);
	if (0 != stat(gc_dbk_filename, &stat_info))
	{
		assert(FALSE);
		UPDATE_ERROR_STRING("Cannot stat %s file %s. %s", DBKEYS_FILE_FORMAT == gc_dbk_file_format ? "DB keys" : "config",
					gc_dbk_filename, strerror(errno));
		return GC_FAILURE;
	}
	if (last_modified == stat_info.st_mtime)
		return GC_SUCCESS;	/* Nothing changed since we last read it. So, return success. */
	handle = NULL;
	if (DBKEYS_FILE_FORMAT == gc_dbk_file_format)
	{
		if (NULL == (fp = fopen(gc_dbk_filename, "r")))
		{
			UPDATE_ERROR_STRING("Cannot open DB keys file - %s. %s", gc_dbk_filename, strerror(errno));
			return GC_FAILURE;
		}
		handle = fp;
	} else
	{
		if (!config_read_file(&gtmcrypt_cfg, gc_dbk_filename))
		{
			UPDATE_ERROR_STRING("Cannot read config file %s. At line# %d - %s", gc_dbk_filename,
						config_error_line(&gtmcrypt_cfg), config_error_text(&gtmcrypt_cfg))
			return GC_FAILURE;
		}
		if (NULL == (setting = config_lookup(&gtmcrypt_cfg, "database.keys")))
		{
			UPDATE_ERROR_STRING("Failed to lookup database.keys in config file %s", gc_dbk_filename);
			return GC_FAILURE;
		}
		handle = setting;
	}
	assert(NULL != handle);
	if (tbl_head)
	{
		gc_dbk_scrub_entries();	/* free up the existing linked list as we are about to create a fresh one */
		tbl_head = NULL;
	}
	db = &db_fname[0];
	key = &key_fname[0];
	while (KEEP_GOING == (status = gc_dbk_get_single_entry(handle, &db, &key, n_dbkeys)))
	{
		assert('\0' == db[strlen(db)]);
		assert('\0' == key[strlen(key)]);
		GC_ALLOCATE_TBL_ENTRY(node);
		strncpy(node->database_fn, db, GTM_PATH_MAX);
		strncpy(node->symmetric_key_fn, key, GTM_PATH_MAX);
		node->next = tbl_head;
		tbl_head = node;
		n_dbkeys++;
	}
	if (PARSE_COMPLETED == status)
	{
		fast_lookup_entry = (gtm_dbkeys_tbl **)MALLOC((SIZEOF(fast_lookup_entry) * n_dbkeys));
		node = tbl_head;
		for (count = 0, node = tbl_head; NULL != node; node = node->next, count++)
		{
			node->index = count;
			fast_lookup_entry[count] = node;
		}
		assert(count == n_dbkeys);
		ret = 0;
	} else
		ret = -1;
	if (DBKEYS_FILE_FORMAT == gc_dbk_file_format)
		fclose(fp);
	else
		config_destroy(&gtmcrypt_cfg);
	return ret;
}

int gc_dbk_fill_symkey_hash(gtm_fileid_ptr_t req_fileid, char *req_hash)
{
	gtm_dbkeys_tbl		*cur;
	int			status, concerns_current_file, skip_entry, plain_text_length;
	gtm_fileid_ptr_t	db_fileid;
	gtm_string_t		filename;

	cur = tbl_head;
	while (NULL != cur)
	{
		db_fileid = NULL;
		if (cur->fileid_dirty)
		{
			filename.length = cur->database_fn_len;
			filename.address = cur->database_fn;
			if (TRUE == GTM_FILENAME_TO_ID(&filename, &db_fileid))
			{
				cur->fileid_dirty = FALSE;
				cur->fileid = db_fileid;
			}
		}
		if (cur->symmetric_key_dirty) /* Need to fill sym key value */
		{
			skip_entry = FALSE;
			/* Check & update the value of $gtm_passwd if it changed since we last checked. This way, if the user
			 * had originally entered a wrong password, but later changed the value (possible in MUMPS using external
			 * call), we read the up-to-date value instead of issuing an error.
			 */
			if (0 != gc_update_passwd(GTM_PASSWD_ENV, &gtmcrypt_pwent, GTMCRYPT_DEFAULT_PASSWD_PROMPT,
							GTMCRYPT_OP_INTERACTIVE_MODE & gtmcrypt_init_flags))
			{
				return GC_FAILURE;
			}
			status = gc_pk_get_decrypted_key(cur->symmetric_key_fn, cur->symmetric_key, &plain_text_length);
			concerns_current_file = (NULL != req_fileid && (GTM_IS_FILE_IDENTICAL(cur->fileid, req_fileid)));
			if (0 != status)
			{
				/* If we failed because of wrong we password OR we are processing an entry that concerns the file
				 * for which we are called for, don't continue any further
				 */
				if ((GPG_ERR_BAD_PASSPHRASE == status) || concerns_current_file)
					return GC_FAILURE;
				skip_entry = TRUE;
			} else if (0 == plain_text_length)
			{	/* It's possible that the decryption didn't encounter error but the plain text length is 0 */
				if (concerns_current_file)
				{
					UPDATE_ERROR_STRING("Symmetric key %s found to be empty", cur->symmetric_key_fn);
					return GC_FAILURE;
				}
				skip_entry = TRUE;
			}
			if (!skip_entry)
			{ 	/* Everything is fine, compute the hash for the key */
				GC_PK_COMPUTE_HASH(cur->symmetric_key_hash, cur->symmetric_key);
				if (0 != gc_sym_create_key_handles(cur))
					return -1;
				cur->symmetric_key_dirty = FALSE;
				if (concerns_current_file
				    || (NULL != req_hash && (0 == memcmp(cur->symmetric_key_hash, req_hash, GTMCRYPT_HASH_LEN))))
				{	/* Processed the entry for which the function was called or found a matching hash. Return */
					return GC_SUCCESS;
				}
			}
		}
		cur = cur->next;
	}
	return GC_SUCCESS;
}

void	 gc_dbk_get_hash(gtm_dbkeys_tbl *entry,  gtm_string_t *hash)
{
	assert(hash->address);
	assert(NULL != entry);
	memcpy(hash->address, entry->symmetric_key_hash, GTMCRYPT_HASH_LEN);
	hash->length = GTMCRYPT_HASH_LEN;
}
