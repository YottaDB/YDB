/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>			/* BYPASSOK -- see above */
#include <sys/types.h>
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include <libconfig.h>

#include "gtmxc_types.h"		/* xc_string, xc_status_t and other callin interfaces xc_fileid */

#include "gtmcrypt_util.h"
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */

#include "gtmcrypt_ref.h"
#include "gtmcrypt_dbk_ref.h"
#include "gtmcrypt_sym_ref.h"
#include "gtmcrypt_pk_ref.h"

GBLDEF	passwd_entry_t	*gtmcrypt_pwent;
GBLDEF	gpgme_ctx_t	pk_crypt_ctx;

void gc_pk_scrub_passwd()
{
	/* Nullify the key strings, so that any generated cores will not contain the unencrypted keys */
	gc_freeup_pwent(gtmcrypt_pwent);
	/* Finally release the gpgme context */
	if (NULL != pk_crypt_ctx)
		gpgme_release(pk_crypt_ctx);
}

/* This function is called whenever gpg needs the passphrase with which the secret key is encrypted. In this case, the passphrase
 * is obtained from the ENVIRONMENT VARIABLE - $gtm_passwd or by invoking the mumps engine during the "gtmcrypt_init()".
 * In either ways, it's guaranteed that when this function is called, the passphrase is already set in the global variable.
 */
int gc_pk_crypt_passphrase_callback(void *opaque, const char *uid_hint, const char *passphrase_info, int last_was_bad, int fd)
{
	int 	write_ret, len;

	assert(0 != fd);
	assert(NULL != gtmcrypt_pwent->passwd);
	len = STRLEN(gtmcrypt_pwent->passwd);
	write_ret = write(fd, gtmcrypt_pwent->passwd, len);
	if (len == write_ret)
	{
		write_ret = write(fd, "\n", 1);
		if (1 == write_ret)
			return 0;
	}
	/* Problem with one of the writes so let gpgme know */
	return -1;
}

/* Given the structure that holds the plain data, this function reads through the structure and retrieves the plain text. We
 * also return the number of bytes actually read from the structure.
 */
int gc_pk_crypt_retrieve_plain_text(gpgme_data_t plain_data, unsigned char *plain_text)
{
	int	ret;

	assert(NULL != plain_text);

	/* Clear the temporary buffer */
	memset(plain_text, 0, SYMMETRIC_KEY_MAX);
	gpgme_data_seek(plain_data, 0, SEEK_SET);
	ret = (int)gpgme_data_read(plain_data, plain_text, SYMMETRIC_KEY_MAX);
	assert(ret || (NULL != getenv("gtm_white_box_test_case_enable"))); /* || needed for "encryption/key_file_enc" subtest */
	return ret;
}

/* This is currently necessary to work around what seems to be a gpgme issue in not clearing the plaintext keys
 * from the C stack (shows up in a core dump otherwise). When gpgme is fixed, this code can be removed.
 * The size of lclarray (8K) is determined purely from experimentation on all platforms.
 */
int gc_pk_scrub_plaintext_keys_from_c_stack()
{
	char lclarray[8192];

	memset(lclarray, 0, SIZEOF(lclarray));
	return 0;
}

/* This function tries to decrypt the cipher file (the file containing the symmetric key with which the database is encrypted).
 * It's assumed that the context is initialized and is set with the appropriate passphrase callback. The cipher_file
 * should contain the fully qualified path of the encrypted database key file. Also, plain text is supposed to be allocated with
 * sufficient space to hold the decrypted text.
 */
gpgme_error_t gc_pk_get_decrypted_key(const char *cipher_file, unsigned char *plain_text, int *plain_text_length)
{
	gpgme_error_t	err;
	gpgme_data_t	cipher_data = NULL, plain_data = NULL;
	gpg_err_code_t	ecode;
	char		null_buffer[SYMMETRIC_KEY_MAX];

	assert(NULL != cipher_file);
	assert(NULL != plain_text);
	assert(NULL != pk_crypt_ctx);
	assert(0 != STRLEN(cipher_file));

	/* Convert the cipher content in the cipher file into in-memory content.
	 * This in-memory content is stored in gpgme_data_t structure.
	 */
	err = gpgme_data_new_from_file(&cipher_data, cipher_file, 1);
	if (!err)
	{
		err = gpgme_data_new(&plain_data);
		if (!err)
		{	/* Try decrypting the cipher content with the context.
			 * The decrypted content will also be stored in gpgme_data_t structure.
			 */
			err = gpgme_op_decrypt(pk_crypt_ctx, cipher_data, plain_data);
			if (!err)	/* Once decrypted, the plain text has to be obtained from the plain_data structure. */
				*plain_text_length = gc_pk_crypt_retrieve_plain_text(plain_data, plain_text);
			gc_pk_scrub_plaintext_keys_from_c_stack();
		}
	}
	ecode = gpgme_err_code(err);
	if (0 != ecode)
	{
		switch (ecode)
		{
			case GPG_ERR_BAD_PASSPHRASE:
				UPDATE_ERROR_STRING("Incorrect password or error while obtaining password");
				break;
			case GPG_ERR_ENOENT:
				UPDATE_ERROR_STRING("Encryption key file " STR_ARG " not found", ELLIPSIZE(cipher_file));
				break;
			case GPG_ERR_EACCES:
				UPDATE_ERROR_STRING("Encryption key file " STR_ARG " not accessible", ELLIPSIZE(cipher_file));
				break;
			case GPG_ERR_ENAMETOOLONG:
				UPDATE_ERROR_STRING("Path, or a component of the path, to encryption key file " STR_ARG
					" is too long", ELLIPSIZE(cipher_file));
				break;
			default:
				UPDATE_ERROR_STRING("Error while accessing key file " STR_ARG ": %s", ELLIPSIZE(cipher_file),
					gpgme_strerror(err));
				break;
		}
	} else
	{
		assert((0 == plain_text_length) || (NULL != plain_data));
	}
	if (NULL != plain_data)
	{	/* Scrub plaintext data before releasing it */
		assert(SYMMETRIC_KEY_MAX == SIZEOF(null_buffer));
		memset(null_buffer, 0, SYMMETRIC_KEY_MAX);
		assert((0 != ecode) || (0 != plain_text_length) || memcmp(plain_text, null_buffer, SYMMETRIC_KEY_MAX));
		gpgme_data_write(plain_data, null_buffer, SYMMETRIC_KEY_MAX);
		gpgme_data_release(plain_data);
	}
	if (NULL != cipher_data)
		gpgme_data_release(cipher_data);
	return ecode;
}

int gc_pk_gpghome_has_permissions()
{
	char		pathname[GTM_PATH_MAX], *ptr;
	int		gnupghome_set, perms, pathlen;

	/* See if GNUPGHOME is set in the environment */
	if (!(ptr = getenv(GNUPGHOME)))
	{	/* $GNUPGHOME is not set, use $HOME/.gnupg as the GPG home directory */
		gnupghome_set = FALSE;
		if (!(ptr = getenv(HOME)))
		{
			UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, HOME);
			return -1;
		}
		SNPRINTF(pathname, GTM_PATH_MAX, "%s/%s", ptr, DOT_GNUPG);
	} else
	{
		pathlen = strlen(ptr);
		if (0 == pathlen)
		{
			UPDATE_ERROR_STRING(ENV_EMPTY_ERROR, GNUPGHOME);
			return -1;
		}
		if (GTM_PATH_MAX <= pathlen)
		{
			UPDATE_ERROR_STRING("$GNUPGHOME is too long -" STR_ARG, ELLIPSIZE(pathname));
			return -1;
		}
		gnupghome_set = TRUE;
		strncpy(pathname, ptr, pathlen);
		pathname[pathlen] = '\0';
	}
	if (-1 != (perms = access(pathname, R_OK | X_OK)))
		return 0;
	else if (EACCES == errno)
	{
		if (gnupghome_set)
		{
			UPDATE_ERROR_STRING("No read permissions on $%s", GNUPGHOME);
		} else
			UPDATE_ERROR_STRING("No read permissions on $%s/%s", HOME, DOT_GNUPG);
	} else	/* Some other error */
		UPDATE_ERROR_STRING("Cannot stat on " STR_ARG " - %d", ELLIPSIZE(pathname), errno);
	return -1;
}
