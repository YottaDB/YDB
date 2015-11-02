/****************************************************************
 *								*
 *	Copyright 2009, 2012 Fidelity Information Services, Inc 	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define _FILE_OFFSET_BITS	64	/* Needed to compile gpgme client progs also with large file support */

#include <stdio.h>			/* BYPASSOK -- Plugin doesn't have access to gtm_* header files */
#include <string.h>			/* BYPASSOK -- see above */
#include <unistd.h>			/* BYPASSOK -- see above */
#include <stdlib.h>			/* BYPASSOK -- see above */
#include <sys/stat.h>			/* BYPASSOK -- see above */
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include "gtmxc_types.h"		/* xc_string, xc_status_t and other callin interfaces xc_fileid */
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */
#include "gtmcrypt_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_pk_ref.h"

#define NEWLINE		0x0A

GBLDEF int		num_entries;
GBLREF int		can_prompt_passwd;
GBLDEF gtm_dbkeys_tbl	*tbl_head;
GBLDEF gtm_dbkeys_tbl	**fast_lookup_entry;

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
		GC_FREE_TBL_ENTRY(cur); /* Note, this will memset the symmetric_key to 0 before free'ing */
		cur = temp;
	}
	if (NULL != fast_lookup_entry)
		GC_FREE(fast_lookup_entry);
	num_entries = 0;
}

/* Given a xc_fileid, containing a unique description of the dat file, the function searches for it's
 * entry in the linked list. On unsuccessful search, returns NULL.
 */
gtm_dbkeys_tbl* gc_dbk_get_entry_by_fileid(xc_fileid_ptr_t fileid)
{
	gtm_dbkeys_tbl *cur = tbl_head;

	while (NULL != cur)
	{
		if (!cur->fileid_dirty && (!cur->symmetric_key_dirty) && (gtm_is_file_identical_fptr(fileid, cur->fileid)))
			break;
		cur = (gtm_dbkeys_tbl *)cur->next;
	}
	return cur;
}

/* Given a hash, the function returns the entry in the linked list that matches with the given hash. Otherwise, NULL is returned */
gtm_dbkeys_tbl* gc_dbk_get_entry_by_hash(xc_string_t *hash)
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

xc_status_t	gc_dbk_fill_gtm_dbkeys_fname(char *fname)
{
	char			*ptr;
	int			status;
	struct stat		stat_buf;

	if (ptr = getenv(GTM_DBKEYS))
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
	} else if (ptr = getenv(HOME))
	{
		SNPRINTF(fname, GTM_PATH_MAX, "%s/%s", ptr, DOT_GTM_DBKEYS);
	} else
	{
		UPDATE_ERROR_STRING("Neither $"GTM_DBKEYS "nor $"HOME " is defined");
		return GC_FAILURE;
	}
	return GC_SUCCESS;
}
/* Initialize the linked list with minimal things. For each pair of entries in the db key file, load the
 * file names into the linked list and validate the format of the entries. Returns error if the format is
 * not the one that's expected. This is a fatal error and program will not continue on encountering this
 * error. Another fatal error is the 'gtm_dbkeys' env variable not set
 */
xc_status_t gc_dbk_load_entries_from_file()
{
	FILE			*gtm_dbkeys_fp;
	int			current_state, count, status, space_cnt, line_no = 0, line_type, filename_len;
	int			looking_for_dat_entry = 1, looking_for_key_entry = 2, buflen, save_errno;
	const char		*prefix = "Error parsing database key file";
	char			buf[LINE_MAX], gtm_dbkeys_fname[GTM_PATH_MAX];
	struct stat		stat_info;
	static time_t		last_modified = 0;
	gtm_dbkeys_tbl		*node = NULL;

	if (GC_SUCCESS != gc_dbk_fill_gtm_dbkeys_fname(&gtm_dbkeys_fname[0]))
		return GC_FAILURE;
	if (0 != stat(gtm_dbkeys_fname, &stat_info))
	{
		save_errno = errno;
		if (ENOENT == save_errno)
		{
			UPDATE_ERROR_STRING("Cannot find DB keys file - %s", gtm_dbkeys_fname);
		} else
			UPDATE_ERROR_STRING("Cannot find DB keys file - %s. %s", gtm_dbkeys_fname, strerror(save_errno));
		return GC_FAILURE;
	}
	if (last_modified == stat_info.st_mtime)
		return GC_SUCCESS;/* Nothing changed since we last read it. So, return success */
	last_modified = stat_info.st_mtime;
	if (NULL == (gtm_dbkeys_fp = fopen(gtm_dbkeys_fname, "r")))
	{
		save_errno = errno;
		UPDATE_ERROR_STRING("Cannot open DB keys file - %s. %s", gtm_dbkeys_fname, strerror(save_errno));
		return GC_FAILURE;
	}
	/* Read the file and parse the contents and fill a mapping table */
	/* Note the format of this dbkeys will be like this -
	 * 	dat <db file1 path>
	 *	key <key file1 name>
	 *	dat <db file2 path>
	 *	key <key file2 name>
	 */
	current_state = looking_for_dat_entry;	/* To start with we are looking for a database entry */
	if (tbl_head)
	{
		gc_dbk_scrub_entries();	/* free up the existing linked list as we are about to create a fresh one */
		tbl_head = NULL;
	}
	while (!feof(gtm_dbkeys_fp))
	{
		if (NULL == fgets(buf, LINE_MAX, gtm_dbkeys_fp))
			break;
		line_no++;
		buflen = STRLEN(buf);
		if (NEWLINE != buf[buflen - 1])
		{	/* last character in the read buffer is not a newline implying that the line contains more than
			 * LINE_MAX characters.
			 */
			 fclose(gtm_dbkeys_fp);
			 UPDATE_ERROR_STRING("%s. Entry at line: %d longer than %ld characters", prefix, line_no, LINE_MAX);
			 return GC_FAILURE;
		}
		buf[buflen - 1] = '\0'; /* strip off the newline at the end */
		space_cnt = 0;
		while (isspace(buf[space_cnt]))		/* BYPASSOK -- don't have access to gtm_ctype.h */
			space_cnt++;	/* go past any whitespace characters */
		assert(space_cnt <= (buflen - 1));
		if ((0 == space_cnt) && ('\0' != buf[0]))
		{
			if (0 == memcmp(buf, DATABASE_LINE_INDICATOR, DATABASE_LINE_INDICATOR_SIZE))
			{
				filename_len = buflen - DATABASE_LINE_INDICATOR_SIZE;
				line_type = DATABASE_LINE_INFO;
			}
			else if (0 == memcmp(buf, SYMMETRIC_KEY_LINE_INDICATOR, SYMMETRIC_KEY_LINE_INDICATOR_SIZE))
			{
				filename_len = buflen - SYMMETRIC_KEY_LINE_INDICATOR_SIZE;
				line_type = SYMMETRIC_KEY_LINE_INFO;
			}
			else
				line_type = -1;
		} else if (space_cnt < (buflen - 1))
			line_type = -1;		/* line doesn't consist entirely of spaces (but only has leading spaces) */
		else
			continue;	/* skip this line as it consists entirely of spaces -- blank line */
		switch(line_type)
		{
			case DATABASE_LINE_INFO:
				if (current_state == looking_for_key_entry)
				{
					fclose(gtm_dbkeys_fp);
					UPDATE_ERROR_STRING("%s. At line %d: Found DAT entry, expecting KEY entry", prefix,
								line_no);

					return GC_FAILURE;
				}
				GC_ALLOCATE_TBL_ENTRY(node);
				memcpy(node->database_fn, &buf[DATABASE_LINE_INDICATOR_SIZE], filename_len + 1);
				assert('\0' == node->database_fn[filename_len]);
				node->database_fn_len = filename_len;
				node->next = tbl_head;
				tbl_head = node;
				current_state = looking_for_key_entry;
				break;

			case SYMMETRIC_KEY_LINE_INFO:
				if (current_state == looking_for_dat_entry)
				{
					fclose(gtm_dbkeys_fp);
					UPDATE_ERROR_STRING("%s. At line %d: Found KEY entry, expecting DAT entry", prefix,
								line_no);
					return GC_FAILURE;
				}
				assert(NULL != node);
				memcpy(node->symmetric_key_fn, &buf[SYMMETRIC_KEY_LINE_INDICATOR_SIZE], filename_len + 1);
				assert('\0' == node->symmetric_key_fn[filename_len]);
				num_entries++;	/* one set of entries processed */
				current_state = looking_for_dat_entry;
				break;

			default:
				fclose(gtm_dbkeys_fp);
				UPDATE_ERROR_STRING("%s. At line %d: %s does not start with 'dat '/'key '", prefix, line_no, buf);
				return GC_FAILURE;
		}
	}
	if (!feof(gtm_dbkeys_fp))
	{
		save_errno = errno;
		UPDATE_ERROR_STRING("Error while reading from database key file. %s", strerror(save_errno));
		return GC_FAILURE;
	} else if (0 == line_no)
	{	/* EOF reached, but did not go past the first line -- no entries in database key file */
		fclose(gtm_dbkeys_fp);
		UPDATE_ERROR_STRING("%s. No entries found in DB keys file.", prefix);
		return GC_FAILURE;
	} else if (current_state == looking_for_key_entry)
	{	/* last database file entry has no matching symmetric key file entry */
		fclose(gtm_dbkeys_fp);
		UPDATE_ERROR_STRING("%s. No matching KEY entry found for DAT entry at line: %d", prefix, line_no);
		return GC_FAILURE;
	}
	GC_MALLOC(fast_lookup_entry, (SIZEOF(fast_lookup_entry) * num_entries), gtm_dbkeys_tbl*);
	node = tbl_head;
	count = 0;
	while (NULL != node)
	{
		node->index = count;
		fast_lookup_entry[count] = node;
		count++;
		node = node->next;
	}
	assert(count == num_entries);
	fclose(gtm_dbkeys_fp);
	return GC_SUCCESS;
}

xc_status_t gc_dbk_fill_sym_key_and_hash(xc_fileid_ptr_t req_fileid, char *req_hash)
{
	gtm_dbkeys_tbl	*cur;
	int		status, concerns_current_file, skip_entry, plain_text_length;
	xc_fileid_ptr_t	db_fileid;
	xc_string_t	filename;

	cur = tbl_head;
	while (NULL != cur)
	{
		db_fileid = NULL;
		if (cur->fileid_dirty)
		{
			filename.length = cur->database_fn_len;
			filename.address = cur->database_fn;
			if (TRUE == gtm_filename_to_id_fptr(&filename, &db_fileid))
			{
				cur->fileid_dirty = FALSE;
				cur->fileid = db_fileid;
			}
		}
		if (cur->symmetric_key_dirty) /* Need to fill sym key value */
		{
			skip_entry = FALSE;
			/* Before decrypting the key, let's see if the gtm_passwd in the environment has changed since
			 * the last time we read from the environment. This way if the user had originally entered a wrong
			 * password and if he/she is in MUMPS and changes the password through an external call then we should
			 * be using the new password rather than the old one which might still be hanging in the environment.
			 */
			gc_pk_crypt_prompt_passwd_if_needed(can_prompt_passwd);
			status = gc_pk_get_decrypted_key(cur->symmetric_key_fn, cur->symmetric_key, &plain_text_length);
			concerns_current_file = (NULL != req_fileid && (gtm_is_file_identical_fptr(cur->fileid, req_fileid)));
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
				GC_SYM_CREATE_HANDLES(cur);
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

void	 gc_dbk_get_hash(gtm_dbkeys_tbl *entry,  xc_string_t *hash)
{
	assert(hash->address);
	assert(NULL != entry);
	memcpy(hash->address, entry->symmetric_key_hash, GTMCRYPT_HASH_LEN);
	hash->length = GTMCRYPT_HASH_LEN;
}
