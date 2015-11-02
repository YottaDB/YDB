/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define _FILE_OFFSET_BITS	64	/* Needed to compile gpgme client progs also with large file support */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include "gtmxc_types.h"		/* xc_string, xc_status_t and other callin interfaces xc_fileid */
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */
#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_pk_ref.h"
#include "gtmcrypt_sym_ref.h"

int		num_entries;
db_key_map	*db_map_root;
db_key_map	**fast_lookup_entry;
extern char	err_string[ERR_STRLEN];
extern int	can_prompt_passwd;

/* Cleanup the db key entries and also remove the plain text passwd stored there */
void gc_dbk_scrub_entries()
{
	db_key_map *temp, *temp1;

	temp = GC_DBK_GET_FIRST_ENTRY();
	/* Walk through the linked list and free each member of the structure.*/
	while (NULL != temp)
	{
#		ifdef USE_GCRYPT
		if (temp->encr_key_handle)
			gcry_cipher_close(temp->encr_key_handle);
		if (temp->decr_key_handle)
			gcry_cipher_close(temp->decr_key_handle);
#		endif
		temp1 = GC_DBK_GET_NEXT_ENTRY(temp);
		GC_FREE_DB_KEY_MAP(temp); /* Note, this will memset the key_string to 0 before free'ing */
		temp = temp1;
	}
	if (NULL != fast_lookup_entry)
		GC_FREE(fast_lookup_entry);
}

/* Find out whether the db key file is modified since last time */
xc_status_t gc_dbk_is_db_key_file_modified()
{
	struct stat	stat_info;
	char		*gtm_dbkeys;
	int		status;
	static time_t	last_modified = 0;

	GC_GETENV(gtm_dbkeys, GTM_DBKEYS, status);
	if (GC_FAILURE == status)
	{
		GC_ENV_UNSET_ERROR(GTM_DBKEYS);
		return GC_FAILURE;
	}
	if (0 != stat(gtm_dbkeys, &stat_info) || (last_modified != stat_info.st_mtime))
	{
		last_modified = stat_info.st_mtime;
		return TRUE;
	}
	return FALSE;
}

/* Given a xc_fileid, containing a unique description of the dat file, the function searches for it's
 * entry in the linked list. On unsuccessful search, returns NULL.
 */
db_key_map* gc_dbk_get_entry_by_fileid(xc_fileid_ptr_t fileid)
{
	db_key_map *cur = db_map_root;

	while (NULL != cur)
	{
		if (!cur->fileid_dirty && (!cur->sym_key_dirty) && (gtm_is_file_identical_fptr(fileid, (cur->fileid))))
			break;
		cur = (db_key_map *)cur->next;
	}
	return cur;
}

/* Given a hash of the symmetric key, the function searches for the entry in the linked list that matches with the
 * given hash
 */
db_key_map* gc_dbk_get_entry_by_hash(xc_string_t *hash)
{
	db_key_map *cur = db_map_root;

	assert(hash);
	assert(hash->length);

	while (NULL != cur)
	{
		if (hash->length == cur->hash.length && (0 == memcmp(hash->address, cur->hash.address, hash->length)))
			break;
		cur = (db_key_map *)cur->next;
	}
	return cur;
}

dbkeyfile_line_type gc_dbk_get_line_info (char *buf, char *data)
{
	dbkeyfile_line_type	line_type = ERROR_LINE_INFO;

	if (!memcmp(buf, DAT_LINE_INDICATOR, DAT_LINE_INDICATOR_SIZE))
	{
		strcpy(data, &buf[DAT_LINE_INDICATOR_SIZE]); /* The rest of the line is a file name */
		if ('\n' == data[strlen(data) - 1]) data[strlen(data) - 1] = '\0';
		line_type = DAT_LINE_INFO;
	} else if (!memcmp(buf, KEY_LINE_INDICATOR, KEY_LINE_INDICATOR_SIZE))
	{
		strcpy(data, &buf[KEY_LINE_INDICATOR_SIZE]); /* The rest of the line is a file name */
		if ('\n' == data[strlen(data) - 1]) data[strlen(data) - 1] = '\0';
		line_type = KEY_LINE_INFO;
	}
	return line_type;
}

xc_status_t gc_dbk_load_gtm_dbkeys(FILE **gtm_dbkeys)
{
	char		*ptr, dbkeys_filename[GTM_PATH_MAX];
	int		status;
	FILE		*dbkeys_fp;
	struct stat	stat_buf;

	GC_GETENV(ptr, GTM_DBKEYS, status);
	if (GC_SUCCESS == status)
	{
		if (0 == strlen(ptr))
		{
			snprintf(err_string, ERR_STRLEN, "%s", "Environment variable gtm_dbkeys set to empty string");
			return GC_FAILURE;
		}
		if (0 == stat(ptr, &stat_buf)) /* See if the environment variable points to a proper path */
		{
			if (S_ISDIR(stat_buf.st_mode)) /* if directory */
				snprintf(dbkeys_filename, GTM_PATH_MAX, "%s/%s", ptr, DOT_GTM_DBKEYS);
			else if (S_ISREG(stat_buf.st_mode)) /* if file */
				snprintf(dbkeys_filename, GTM_PATH_MAX, "%s", ptr);
			else
			{
				snprintf(err_string, ERR_STRLEN, "Unknown file type : %s", ptr);
				return GC_FAILURE;
			}
		} else /* error if env variable present but couldn't stat */
		{
			snprintf(err_string, ERR_STRLEN, "Cannot find DB keys file - %s", ptr);
			return GC_FAILURE;
		}
	} else /* if env variable is undefined, then look for $HOME/.gtm_dbkeys */
	{
		GC_GETENV(ptr, "HOME", status);
		snprintf(dbkeys_filename, GTM_PATH_MAX, "%s/%s", ptr, DOT_GTM_DBKEYS);
		if (0 != stat(dbkeys_filename, &stat_buf))
		{
			snprintf(err_string,
				 ERR_STRLEN,
				 "Environment variable gtm_dbkeys undefined. Cannot find %s/.gtm_dbkeys",
				 ptr);
			return GC_FAILURE;
		}
	}
	/* At this point we would have at least one form of the gtm_dbkeys in dbkeys_filename */
	status = GC_SUCCESS;
	if (NULL != (dbkeys_fp = fopen(dbkeys_filename, "r")))
		*gtm_dbkeys = dbkeys_fp;
	else
	{
		snprintf(err_string, ERR_STRLEN, "Cannot open DB keys file - %s", dbkeys_filename);
		status = GC_FAILURE;
	}
	return status;
}
/* Initialize the linked list with minimal things. For each pair of entries in the db key file, load the
 * file names into the linked list and validate the format of the entries. Returns error if the format is
 * not the one that's expected. This is a fatal error and program will not continue on encountering this
 * error. Another fatal error is the 'gtm_dbkeys' env variable not set
 */
xc_status_t gc_dbk_load_entries_from_file()
{
	FILE			*dbkeys_fp = NULL;
	db_key_map		*node = NULL;
	int			current_state;
	int			start = TRUE, count = 0, status, all_done = FALSE;
	int			looking_for_dat_entry = 1, looking_for_key_entry = 2;
	int			line_no = 0;
	char			*prefix = "Error parsing database key file";
	char			buf[GTM_PATH_MAX], data[GTM_PATH_MAX];
	dbkeyfile_line_type	line_type;
	/* Check for $gtm_dbkeys */
	if (0 != gc_dbk_load_gtm_dbkeys(&dbkeys_fp))
		return GC_FAILURE;

	/* Read the file and parse the contents and fill a mapping table */
	/* Note the format of this dbkeys will be like this -
		dat <db file1 path>
		key <key file1 name>
		dat <db file2 path>
		key <key file2 name>
	*/
	/* To start with we are looking for a dat entry */
	current_state = looking_for_dat_entry;
	GC_DBK_SET_FIRST_ENTRY(NULL);
	while (!feof(dbkeys_fp))
	{

		memset(buf, 0, GTM_PATH_MAX);
		memset(data, 0, GTM_PATH_MAX);

		/* Skip past empty lines */
		while (1)
		{
			if (!fgets(buf, GTM_PATH_MAX, dbkeys_fp))
			{
				/* If EOF is reached but the line_no din't move beyond 0, it means we have no entries
				 * in the db key file. */
				if (0 == line_no)
				{
					fclose(dbkeys_fp);
					snprintf(err_string,
						ERR_STRLEN,
						"%s. %s",
						prefix,
						"No entries found in DB keys file.");
					return GC_FAILURE;
				}
				/* At the end if we are looking for a key entry, then the last dat entry is unmatched*/
				if (current_state == looking_for_key_entry)
				{
					fclose(dbkeys_fp);
					snprintf(err_string,
						ERR_STRLEN,
						"%s. No matching KEY entry found for DAT entry at line: %d",
						prefix,
						line_no);
					return GC_FAILURE;
				}
				all_done = TRUE;
				break;
			}
			if (buf[0] != '\0' && (buf[0] != '\n'))  /* Non-Empty line */
			{
				buf[strlen(buf) - 1] = '\0';
				break;
			}
			else
				line_no++;
		}
		if (all_done) break;
		/* Figure out what kind of line are we going to deal with. */
		line_type = gc_dbk_get_line_info(buf, data);
		switch(line_type)
		{
			case DAT_LINE_INFO:
				line_no++;
				/* We should have seen a key before seeing the next dat file */
				if (current_state == looking_for_key_entry && (FALSE == start))
				{
					fclose(dbkeys_fp);
					snprintf(err_string,
						 ERR_STRLEN,
						 "%s. At line %d: No matching KEY entry found for %s",
						 prefix,
						 line_no,
						 buf);
					return GC_FAILURE;
				}
				/* Now that we have seen a dat file, we will now be looking for a key entry */
				current_state = looking_for_key_entry;
				start = FALSE;
				GC_NEW_DB_KEYMAP(node);
				GC_COPY_TO_XC_STRING(&node->db_name, data, strlen(data));
				node->next = (struct db_key_map*) db_map_root;
				GC_DBK_SET_FIRST_ENTRY(node);
				break;

			case KEY_LINE_INFO:
				line_no++;
				/* We should have seen a dat file before seeing a key file */
				if (!node && (current_state == looking_for_dat_entry))
				{
					fclose(dbkeys_fp);
					snprintf(err_string,
						 ERR_STRLEN,
						 "%s. At line %d: No matching DAT entry found for %s",
						 prefix,
						 line_no,
						 buf);
					return GC_FAILURE;
				}
				/* Now that we have seen a key file, we will now be looking for a dat entry */
				current_state = looking_for_dat_entry;
				num_entries++;
				GC_COPY_TO_XC_STRING(&node->key_filename, data, strlen(data));
				break;

			default:
				line_no++;
				fclose(dbkeys_fp);
				snprintf(err_string,
					 ERR_STRLEN,
					 "%s. At line %d: %s does not start with 'dat '/'key '",
					 prefix,
					 line_no,
					 buf);
				return GC_FAILURE;
		}
	}
	GC_MALLOC(fast_lookup_entry, (SIZEOF(fast_lookup_entry) * num_entries), db_key_map*);
	node = GC_DBK_GET_FIRST_ENTRY();
	while (NULL != node)
	{
		node->index = count;
		fast_lookup_entry[count] = node;
		count++;
		node = GC_DBK_GET_NEXT_ENTRY(node);
	}
	assert(count == num_entries);
	fclose(dbkeys_fp);
	return GC_SUCCESS;
}

xc_status_t gc_dbk_fill_sym_key_and_hash(xc_fileid_ptr_t req_fileid, char *req_hash)
{
	db_key_map		*cur;
	int			status, concerns_current_file;
	xc_fileid_ptr_t		db_fileid;

	cur = GC_DBK_GET_FIRST_ENTRY();
	while (NULL != cur)
	{
		db_fileid = NULL;
		if (TRUE == cur->fileid_dirty)
		{
			if (TRUE == gtm_filename_to_id_fptr(&(cur->db_name), &db_fileid))
			{
				cur->fileid_dirty = FALSE;
				cur->fileid = db_fileid;
			}
		}
		if (TRUE == cur->sym_key_dirty) /* Need to fill sym key value */
		{
			/* Before decrypting the key, let's see if the gtm_passwd in the environment has changed since
			 * the last time we read from the environment. This way if the user had originally entered a wrong
			 * password and if he is in MUMPS and changes the password through a external call then we should
			 * be using the new password rather than the old one which might still be hanging in the environment. */
			gc_pk_crypt_prompt_passwd_if_needed(can_prompt_passwd);
			GC_PK_GET_DECRYPTED_KEY(cur->key_string, status);

			/* If we failed because of a gtm_passwd being wrong we wouldn't want to continue any further although it
			 * might not concern for the current file. */
			if (GPG_ERR_BAD_PASSPHRASE == status)
				return GC_FAILURE;

			concerns_current_file = (NULL != req_fileid && (gtm_is_file_identical_fptr(cur->fileid, req_fileid)));
			/* Eventhough we may have an encountered error in the above decryption, we report only when it is concerned
			 * with the current dat file being used. For other files, we silently ignore the error, with a hope that by
			 * the time the database file is accessed, db key file would have been updated appropriately by the user.
			 */
			if (0 != status && concerns_current_file)
				return GC_FAILURE;

			/* It could be possible that the decryption din't return any error but plain_text_length happens to
			 * be zero. So, we verify it and return error in case the length is zero. Again we make sure that we return
			 * the error only when it concerned with the current dat file.
			 */
			if (0 == cur->key_string.length && concerns_current_file)
			{
				snprintf(err_string, ERR_STRLEN, "Symmetric key %s found to be empty", cur->key_filename.address);
				return GC_FAILURE;
			}

			/* If we fall through here, it means that we have encountered an error for a database which is not of
			 * concern at this moment. So, we continue with the next database. */
			if (0 != status)
			{
				cur = GC_DBK_GET_NEXT_ENTRY(cur);
				continue;
			}

			/* If everything is fine, compute the hash for the key */
			GC_PK_COMPUTE_HASH(cur->hash, cur->key_string);
			GC_SYM_CREATE_HANDLES(cur);
			cur->sym_key_dirty = FALSE;

			/* If we have found a matching entry for the hash/fileid that we requested for, return immediately with
			 * GC_SUCCESS */
			if (concerns_current_file
			    || (NULL != req_hash && (0 == memcmp(cur->hash.address, req_hash, GTMCRYPT_HASH_LEN))))
			    	return GC_SUCCESS;
		}
		cur = GC_DBK_GET_NEXT_ENTRY(cur);
	}
	return GC_SUCCESS;
}

void	 gc_dbk_get_hash(db_key_map *entry,  xc_string_t *hash)
{
	/*Make sure the reference block that is being passed is already allocated */
	assert(hash->address);
	assert(NULL != entry);
	memcpy(hash->address, entry->hash.address, GTMCRYPT_HASH_LEN);
	hash->length = GTMCRYPT_HASH_LEN;
}
