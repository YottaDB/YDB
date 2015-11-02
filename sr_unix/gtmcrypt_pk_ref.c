/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc 	*
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
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <gpgme.h>			/* gpgme functions */
#include <gpg-error.h>			/* gcry*_err_t */
#include <dlfcn.h>
#include <sys/mman.h>
#include "gtmxc_types.h"		/* xc_string, xc_status_t and other callin interfaces xc_fileid */
#include "gtmcrypt_interface.h"		/* Function prototypes for gtmcrypt*.* functions */
#include "gtmcrypt_ref.h"
#include "gtmcrypt_pk_ref.h"
#include "gtmcrypt_sym_ref.h"

static char	*gtm_passwd;
static char	*gtm_passwd_env;
int		can_prompt_passwd;
gpgme_ctx_t	pk_crypt_ctx;

extern char	err_string[ERR_STRLEN];

#ifdef __sparc /* for some reason sun does not provide a prototype */
int setenv(const char* name, const char *value, int overwrite);
#endif

/* Take a masked/unmasked password and convert it to the other form by doing an XOR operation via an XOR mask.
 * XOR MASK:
 * If the gtm_obfuscation_key exists and points to a file that has readable contents, the XOR mask is the
 * SHA-512 hash of the contents of that file.
 * Otherwise, within a pre-zero'ed buffer the length of the password the contents of the USER environment
 * variable is left-justified and the decimal representation of the inode of the mumps executable is
 * right-justified. The XOR mask is the SHA-512 hash of the contents of that buffer.
 * 	<PASSWORDLEN>
 *	USER0000INODE ---SHA-512--> XOR mask
 * MASKING:
 * The original password value is XOR'ed with the XOR mask, converted to it's hex representation (for easy
 * viewing) and set into the gtm_passwd environment variable. This can then be used by job'ed off child
 * processes.
 * UNMASKING:
 * The contents of the gtm_passwd environment variable converted into its binary representation (from its
 * hex representation). This gtm_passwd value is then XOR'ed with the XOR mask.
 */

int gc_pk_mask_unmask_passwd(char *in, char *out, int len)
{
	char		*ptr;
	char		tmp[GTM_PASSPHRASE_MAX], mumps_ex[GTM_PATH_MAX], tobehashed[GTM_PASSPHRASE_MAX], hash[GTMCRYPT_HASH_LEN];
	int		passwd_len, ilen, status, i;
	struct stat	stat_info;
	int 		fd;
	char 		*p;
	int 		have_hash;

	have_hash = FALSE;
	passwd_len = len < GTM_PASSPHRASE_MAX ? len : GTM_PASSPHRASE_MAX;

	GC_GETENV(ptr, "gtm_obfuscation_key", status);
	if (GC_SUCCESS == status)
	{
		fd = open(ptr, O_RDONLY);
		if (fd != -1)
			if (fstat(fd, &stat_info) != -1)
				if (S_ISREG(stat_info.st_mode))
				{
					p = mmap(0,stat_info.st_size, PROT_READ, MAP_SHARED, fd, 0);
					if (p != MAP_FAILED)
					{
#						ifdef USE_OPENSSL
						EVP_Digest(p, stat_info.st_size, (unsigned char *)hash, NULL, EVP_sha512(), NULL);
#						elif defined USE_GCRYPT
						GC_SYM_INIT;
						gcry_md_hash_buffer(GCRY_MD_SHA512, hash, p, stat_info.st_size );
#						endif
						have_hash = TRUE;
						munmap(p, stat_info.st_size);
					}
				}
		close(fd);
	}

	if (!have_hash)
	{
		memset(tobehashed, 0, passwd_len);
		memset(mumps_ex, 0, GTM_PATH_MAX);
		GC_GETENV(ptr, "USER", status);
		if (GC_SUCCESS != status)
		{
			GC_ENV_UNSET_ERROR("USER");
			return GC_FAILURE;
		}
		else
		{
			strncpy(tobehashed, ptr, passwd_len);
			GC_GETENV(ptr, "gtm_dist", status);
			if (GC_SUCCESS != status)
			{
				GC_ENV_UNSET_ERROR("gtm_dist");
				return GC_FAILURE;
			}
			else
			{
				sprintf(mumps_ex, "%s/%s", ptr, "mumps");
				if (0 == stat(mumps_ex, &stat_info))
				{
					sprintf(tmp, "%ld", (long) stat_info.st_ino);
					ilen = (int)strlen(tmp);
					if (ilen < passwd_len)
						strncpy(tobehashed + (passwd_len - ilen), tmp, ilen);
					else
						strncpy(tobehashed, tmp, passwd_len);
				} else
				{
					sprintf(err_string, "Cannot find MUMPS executable in %s", ptr);
					return GC_FAILURE;
				}
#				ifdef USE_OPENSSL
				EVP_Digest(tobehashed, passwd_len, (unsigned char *)hash, NULL, EVP_sha512(), NULL);
#				elif defined USE_GCRYPT
				GC_SYM_INIT;
				gcry_md_hash_buffer(GCRY_MD_SHA512, hash, tobehashed, passwd_len );
#				endif
				have_hash = TRUE;
			}
		}
	}
	if (have_hash)
	{
		for (i = 0; i < passwd_len; i++)
			out[i] = in[i] ^ hash[i % GTMCRYPT_HASH_LEN];
		return GC_SUCCESS;
	}

	return GC_FAILURE;
}

int gc_pk_mask_unmask_passwd_interlude(int nparm, gtm_string_t *in, gtm_string_t *out, int len)
{
	out->length=len;
	return gc_pk_mask_unmask_passwd(in->address, out->address, len);
}

void gc_pk_scrub_passwd()
{
	/* Nullify the key strings, so that any generated cores will not contain the unencrypted keys */
	memset(gtm_passwd, 0, strlen(gtm_passwd));
	/* Free gtm_passwd and gtm_passwd_env variables */
	if (NULL != gtm_passwd)
		GC_FREE(gtm_passwd);
	if (NULL != gtm_passwd_env)
		GC_FREE(gtm_passwd_env);
	/* Finally release the gpgme context */
	if (NULL != pk_crypt_ctx)
		gpgme_release(pk_crypt_ctx);
}

/* Loads the GTMCI variable with the path of the gtmcrypt.tab which will be placed in gtm_dist folder at build time.
 * Here we assume that the tab file be in $gtm_dist/plugin/gtmcrypt
 */

void gc_pk_crypt_load_gtmci_env()
{
	const char	*gtm_dist_value;
	const char	*gtmcrypt_tab_file = "gtmcrypt.tab"; /* Name of the tab file */
	static char	gtmcrypt_tab_path[TAB_NAME_MAX];  /* Needs to be in scope always */

	gtm_dist_value = getenv("gtm_dist");
	assert(NULL != gtm_dist_value);
	assert(0 != strlen(gtm_dist_value));

	sprintf(gtmcrypt_tab_path, "%s/%s/%s", gtm_dist_value, "plugin/gtmcrypt", gtmcrypt_tab_file);
	setenv(GTMCI, gtmcrypt_tab_path, TRUE);
}

/* The following function checks if gtm_passwd is already set. If gtm_passwd is not set in the env, it's a serious
 * error condition. We return back immediately. If it's set to empty string, we prompt for passwd immediately. The
 * current implementation of password prompting is done via a mumps call-in to %GETPASS.
 */

xc_status_t gc_pk_crypt_prompt_passwd_if_needed(int prompt_passwd)
{
			/* Name of the mumps password routine that will be called. */
	const char	*password_routine = "getpass";
			/* Points to the value that was held in GTMCI prior to modification. */
	char		*save_gtmci, tgtm_passwd[GTM_PASSPHRASE_MAX];
	char		*lgtm_passwd;
	int		status, len;
	gtm_int_t	pass_len = GTM_PASSPHRASE_MAX;

	can_prompt_passwd = prompt_passwd;
	GC_GETENV(lgtm_passwd, GTM_PASSWD, status);
	/* This is an error condition. We have hit a encrypted database but the env doesn't have gtm_passwd set. */
	if (0 != status)
	{
		GC_ENV_UNSET_ERROR(GTM_PASSWD);
		return GC_FAILURE;
	}

	/* If the masked password in the environment is same as we have in memory then it means that the password
	 * has not been changed and so the actual value in the gtm_passwd is still good to use. */
	if (NULL != gtm_passwd_env && (0 == strcmp(gtm_passwd_env, lgtm_passwd)))
		return GC_SUCCESS;
	/* If the password is set to an appropriate value, then we know for sure it's in it's masked form. So, we unmask it
	 * and set it in the global variable and return to the caller. */
	if (0 < (len = (int)strlen(lgtm_passwd)))
	{
		if (gtm_passwd)
			GC_FREE(gtm_passwd);
		GC_MALLOC(gtm_passwd, len / 2 + 1, char);
		memset(gtm_passwd, 0, len / 2 + 1);
		GC_UNHEX(lgtm_passwd, gtm_passwd, len);
		status = gc_pk_mask_unmask_passwd(gtm_passwd, gtm_passwd, len / 2);
		if (GC_SUCCESS == status)
		{
			/* Now that we have unmasked the gtm_passwd in the environment
			 * store the masked version in gtm_passwd_env so that future
			 * calls to this function can make use of this and return early
			 * if we find no change between the one in the environment and
			 * the one in the memory */
			if (NULL != gtm_passwd_env)
				GC_FREE(gtm_passwd_env);
			GC_MALLOC(gtm_passwd_env, strlen(lgtm_passwd) + 1, char);
			strcpy(gtm_passwd_env, lgtm_passwd);
		}
		return status;
	} else if (!prompt_passwd)
	{
		/* If we are here, it means that the caller of the plugin library was not MUMPS (may be MUPIP, DSE and LKE).
		 * For the utility programs, we expect the password to be set in the environment to an appropriate masked
		 * form. If not, it's an error and we return the appropriate error message. */
		strcpy(err_string, PASSWD_EMPTY);
		return GC_FAILURE;
	}

	/* Only if the gtm_passwd is set to empty string, we prompt the user for password */
	GC_MALLOC(gtm_passwd, GTM_PASSPHRASE_MAX, char);
	memset(gtm_passwd, 0, GTM_PASSPHRASE_MAX);
	save_gtmci = getenv(GTMCI);
	gc_pk_crypt_load_gtmci_env();
	status = gtm_ci_fptr(password_routine, gtm_passwd, pass_len);
	if (0 != status)
	{
		gtm_zstatus_fptr(err_string, ERR_STRLEN);
		return GC_FAILURE;
	}
	/* Restore the GTMCI variable */
	if (NULL != save_gtmci) /* To make sure we don't set an environment variable as NULL */
		setenv(GTMCI, save_gtmci, 1);

	/* After applying a minimal encryption, we set it to the environment variable */
	GC_MALLOC(lgtm_passwd, strlen(gtm_passwd) * 2 + 1, char);
	gc_pk_mask_unmask_passwd(gtm_passwd, tgtm_passwd, (int)strlen(gtm_passwd));
	GC_HEX(tgtm_passwd, lgtm_passwd, strlen(gtm_passwd) * 2);
	setenv("gtm_passwd", lgtm_passwd, TRUE); /* Note that we currently do not free 'gtm_passwd', even if it was
					       * allocated above, as it needs to be in the env buffer
					       */
	return GC_SUCCESS;
}

/* This function is called whenever gpg needs the passphrase with which the secret key is encrypted. In this case, the passphrase
 * is obtained from the ENVIRONMENT VARIABLE - $gtm_passwd or by invoking the mumps engine during the "gtmcrypt_init()".
 * In either ways, it's guaranteed that when this function is called, the passphrase is already set in the global variable.
 * In either ways, it's guaranteed that when this function is called, the passphrase is already set in the global variable.
 */
int gc_pk_crypt_passphrase_callback(void *opaque, const char *uid_hint,
			const char *passphrase_info, int last_was_bad,
			int fd)
{
	int write_ret;
	assert(0 != fd);
	assert(NULL != gtm_passwd);
	/* This is just being cautious. We would have thrown the appropriate error message
	 * if gtm_passwd have been zero length'ed one.
	 */
	assert(0 != strlen(gtm_passwd));
	write_ret = write(fd, gtm_passwd, strlen(gtm_passwd));
	if (strlen(gtm_passwd) == write_ret)
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

int gc_pk_crypt_retrieve_plain_text(gpgme_data_t plain_data, char *plain_text)
{
	int	ret;

	assert(NULL != plain_text);

	/* Clear the temporary buffer */
	memset(plain_text, 0, GTM_KEY_MAX);

	gpgme_data_seek(plain_data, 0, SEEK_SET);
	ret = (int)gpgme_data_read(plain_data, plain_text, GTM_KEY_MAX);
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
gpgme_error_t gc_pk_get_decrypted_key(const char *cipher_file, char *plain_text, int *plain_text_length)
{
	gpgme_error_t	err;
	gpgme_data_t	cipher_data = NULL, plain_data = NULL;
	xc_status_t	ret_status;
	gpg_err_code_t	ecode;
	char		null_buffer[GTM_KEY_MAX];

	assert(NULL != cipher_file);
	assert(NULL != plain_text);
	assert(NULL != pk_crypt_ctx);
	assert(0 != strlen(cipher_file));

	/* Convert the cipher content in the cipher file into
	 * in-memory content. This in-memory content is stored
	 * in gpgme_data_t structure. */
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
		switch(ecode)
		{
			case GPG_ERR_BAD_PASSPHRASE:
				snprintf(err_string, ERR_STRLEN, "%s", "Incorrect password");
				break;
			case GPG_ERR_ENOENT:
				snprintf(err_string, ERR_STRLEN, "encryption key file %s not found", cipher_file);
				break;
			default:
				snprintf(err_string, ERR_STRLEN, "%s", gpgme_strerror(err));
				break;
		}
	}
	if (NULL != plain_data)
	{	/* scrub plaintext data before releasing it */
		assert(GTM_KEY_MAX == SIZEOF(null_buffer));
		memset(null_buffer, 0, GTM_KEY_MAX);
		gpgme_data_write(plain_data, null_buffer, GTM_KEY_MAX);
		gpgme_data_release(plain_data);
	}
	if (NULL != cipher_data)
		gpgme_data_release(cipher_data);
	return ecode;
}

int gc_pk_gpghome_has_permissions()
{
	char	filename[GTM_PATH_MAX], *tmp_ptr = NULL;
	int	gnupghome_set, status, fd;

	/* See if GNUPGHOME is set in the environment */
	GC_GETENV(tmp_ptr, GNUPGHOME, status);
	if (GC_SUCCESS != status)
	{
		gnupghome_set = FALSE;
		GC_GETENV(tmp_ptr, "HOME", status);
		if (GC_SUCCESS != status)
		{
			GC_ENV_UNSET_ERROR("HOME");
			return GC_FAILURE;
		}
		/* If GNUPGHOME is not set, we choose the filename as $HOME/.gnupg */
		snprintf(filename, GTM_PATH_MAX, "%s/%s", tmp_ptr, DOT_GNUPG);
	} else
	{
		gnupghome_set = TRUE;
		/* If GNUPGHOME is set, then we choose the path pointed by GNUPGHOME as the
		 * directory containing the public keys and private keys whose permissions we are
		 * interested in. */
		strcpy(filename, tmp_ptr);
	}
	/* At this point, we are sure that the filename is pointing to the appropriate directory containing the public/private
	 * keys. If not, then we had encountered an error and would have returned back to the caller. */
	if (-1 != (fd = open(filename, O_RDONLY)))
	{
		close(fd);
		return GC_SUCCESS;
	}
	/* If we don't have appropriate read permissions then we report the error accordingly. */
	if (EACCES == errno)
	{
		if (gnupghome_set)
			snprintf(err_string, ERR_STRLEN, "%s", "No read permissions on $GNUPGHOME");
		else
			snprintf(err_string, ERR_STRLEN, "%s", "No read permissions on $HOME/.gnupg");
	}
	close(fd);
	return GC_FAILURE;
}
