/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
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
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <termios.h>

#include <gcrypt.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "gtmxc_types.h"
#include "gtmcrypt_util.h"

#include "gtmcrypt_interface.h"

/* Define the global error string here. Since this module gets linked into libgtmcryptutil.so, the error string global gets
 * into the shared library automatically and is shared across all different libraries that comprise the encryption reference
 * implementation.
 */
GBLDEF	char	gtmcrypt_err_string[MAX_GTMCRYPT_ERR_STRLEN];
#ifndef USE_SYSLIB_FUNCS
GBLDEF gtm_malloc_fnptr_t		gtm_malloc_fnptr;
GBLDEF gtm_free_fnptr_t			gtm_free_fnptr;
GBLDEF gtm_filename_to_id_fnptr_t	gtm_filename_to_id_fnptr;
GBLDEF gtm_is_file_identical_fnptr_t	gtm_is_file_identical_fnptr;
GBLDEF gtm_xcfileid_free_fnptr_t	gtm_xcfileid_free_fnptr;
#endif

#define SIGPROCMASK(FUNC, NEWSET, OLDSET, RC)										\
{															\
        do														\
        {														\
          RC = sigprocmask(FUNC, NEWSET, OLDSET);									\
        } while (-1 == RC && EINTR == errno);										\
}

#define Tcsetattr(FDESC, WHEN, TERMPTR, RC, ERRNO)									\
{															\
	sigset_t block_ttinout;												\
	sigset_t oldset;												\
	int rc;														\
															\
	sigemptyset(&block_ttinout);											\
	sigaddset(&block_ttinout, SIGTTIN);										\
	sigaddset(&block_ttinout, SIGTTOU);										\
	SIGPROCMASK(SIG_BLOCK, &block_ttinout, &oldset, rc);								\
	do														\
	{														\
	   RC = tcsetattr(FDESC, WHEN, TERMPTR);									\
	} while(-1 == RC && EINTR == errno);										\
	ERRNO = errno;													\
	SIGPROCMASK(SIG_SETMASK, &oldset, NULL, rc);									\
}

#ifdef USE_OPENSSL
#define SHA512(INBUF, INBUF_LEN, OUTBUF, RC)										\
{															\
	RC = EVP_Digest(INBUF, INBUF_LEN, (unsigned char *)OUTBUF, NULL, EVP_sha512(), NULL);				\
	if (0 >= RC)													\
	{														\
		RC = -1;												\
		GC_APPEND_OPENSSL_ERROR("OpenSSL function `EVP_sha512' failed.");					\
	} else														\
		RC = 0;													\
}
#else
#define SHA512(INBUF, INBUF_LEN, OUTBUF, RC)										\
{															\
	/* First initialize the libgcrypt library */									\
	RC = 0;														\
	if (NULL == gcry_check_version(GCRYPT_VERSION))									\
	{														\
		UPDATE_ERROR_STRING("libgcrypt version mismatch. %s or higher is required", GCRYPT_VERSION);		\
		RC = -1;												\
	} else														\
		gcry_md_hash_buffer(GCRY_MD_SHA512, OUTBUF, INBUF, INBUF_LEN);						\
}
#endif

int gc_load_gtmshr_symbols()
{
#	ifndef USE_SYSLIB_FUNCS
	gtm_malloc_fnptr = &gtm_malloc;
	gtm_free_fnptr = &gtm_free;
	gtm_is_file_identical_fnptr = &gtm_is_file_identical;
	gtm_filename_to_id_fnptr = &gtm_filename_to_id;
	gtm_xcfileid_free_fnptr = &gtm_xcfileid_free;
#	endif
	return 0;
}

/* Libgcrypt doesn't do a good job of handling error messages and simply dumps them onto the console if
 * no handler is set. One such example is when libgcrypt is doing a select on /dev/random and when it
 * is interrupted due to a SIGALRM, libgcrypt dumps an error message onto the console. To avoid this,
 * setup a dummy handler and swallow libgcrypt messages as long as they are not FATAL or BUG.
 */
void gtm_gcry_log_handler(void *opaque, int level, const char *fmt, va_list arg_ptr)
{
	assert((GCRY_LOG_FATAL != level) && (GCRY_LOG_BUG != level));
	return;
}

int gc_read_passwd(char *prompt, char *buf, int maxlen)
{
	struct termios		new_tty, old_tty;
	int			fd, status, save_errno, istty, i, rv;
	char			c;

	/* Display the prompt */
	printf("\n%s", prompt);
	fflush(stdout);			/* BYPASSOK -- cannot use FFLUSH */
	/* Determine if the process has a terminal device associated with it */
	fd = fileno(stdin);
	if (FALSE != (istty = isatty(fd)))
	{	/* Turn off terminal echo. */
		if (0 != (status = tcgetattr(fd, &old_tty)))
		{
			UPDATE_ERROR_STRING("Failed to obtain passphrase from terminal. %s", strerror(errno));
			return -1;
		}
		new_tty = old_tty;
		new_tty.c_lflag &= ~ECHO;
		/* GT.M terminal settings has ICANON and ICRNL turned-off. This causes the terminal to be treated
		 * in non-canonical mode and carriage-return to not take effect. Re-enable them so that input can be
		 * read from the user.
		 */
		new_tty.c_lflag |= ICANON;
		new_tty.c_iflag |= ICRNL;
		Tcsetattr(fd, TCSAFLUSH, &new_tty, status, save_errno);
		if (-1 == status)
		{
			UPDATE_ERROR_STRING("Failed to obtain passphrase from terminal. %s", strerror(save_errno));
			return -1;
		}
	}
	/* Read the password. Note: we cannot use fgets() here as that would cause mixing of streams (buffered vs non-buffered). */
	i = rv = 0;
	do
	{
		while ((-1 == (status = read(fd, &c, 1))) && (EINTR == errno))
			;
		if (-1 == status)
		{
			save_errno = errno;
			UPDATE_ERROR_STRING("Failed to obtain passphrase. %s", strerror(save_errno));
			rv = -1;
			break;
		} else if (0 == status)
		{
			UPDATE_ERROR_STRING("Failed to obtain passphrase. Encountered premature EOF while reading from terminal.");
			rv = -1;
			break;
		}
		buf[i++] = c;
	} while (('\n' != c) && (i < maxlen));
	if (i == maxlen)
	{
		UPDATE_ERROR_STRING("Password too long. Maximum allowed password length is %d characters.", maxlen);
		rv = -1;
	}
	if (-1 != rv)
	{
		assert('\n' == buf[i - 1]);
		buf[i - 1] = '\0';	/* strip off the trailing \n */
	}
	if (istty)
	{	/* Reset terminal settings to how it was at the function entry. */
		Tcsetattr(fd, TCSAFLUSH, &old_tty, status, save_errno);
		if (-1 == status)
		{
			UPDATE_ERROR_STRING("Failed to obtain passphrase from terminal. %s", strerror(save_errno));
			return -1;
		}
	}
	return rv;
}

/* Given a stream of characters representing the obfuscated/unobfuscated password, convert to the other form. The process requires
 * an intermediate XOR_MASK.
 *
 * XOR MASK:
 * --------
 * If the gtm_obfuscation_key exists and points to a file that has readable contents, the XOR mask is the SHA-512 hash of the
 * contents of that file.
 * Otherwise, within a pre-zero'ed buffer (of size equal to the length of the password), the value of $USER (from the environment)
 * is left-justified and the decimal representation of the inode of the mumps executable is right-justified. The XOR mask is the
 * SHA-512 hash of the contents of this buffer.
 * 	<PASSWORDLEN>
 *	USER0000INODE => SHA-512 => XOR mask
 *
 * MASKING/UNMASKING:
 * ------------------
 * The input character pointer (in->address) is XOR'ed with the above XOR mask and copied into out->address.
 *
 * The `nparm' value is unused when called from C and is there only so that this function can be used as an external call entry-
 * point for M.
 */
int gc_mask_unmask_passwd(int nparm, gtm_string_t *in, gtm_string_t *out)
{
	char		tmp[GTM_PASSPHRASE_MAX], mumps_exe[GTM_PATH_MAX], hash_in[GTM_PASSPHRASE_MAX], hash[GTMCRYPT_HASH_LEN];
	char 		*ptr, *mmap_addrs;
	int		passwd_len, len, i, save_errno, fd, have_hash, status;
	struct stat	stat_info;

	have_hash = FALSE;
	passwd_len = in->length < GTM_PASSPHRASE_MAX ? in->length : GTM_PASSPHRASE_MAX;

	if (NULL != (ptr = getenv(GTM_OBFUSCATION_KEY)))
	{
		if (-1 != (fd = open(ptr, O_RDONLY)))
		{
			if ((-1 != fstat(fd, &stat_info)) && S_ISREG(stat_info.st_mode))
			{	/* File pointed by $gtm_obfuscation_key exists and is a regular file */
				mmap_addrs = mmap(0, stat_info.st_size, PROT_READ, MAP_SHARED, fd, 0);
				if (MAP_FAILED != mmap_addrs)
				{
					SHA512(mmap_addrs, stat_info.st_size, hash, status);
					if (0 != status)
						return -1;
					have_hash = TRUE;
					munmap(mmap_addrs, stat_info.st_size);
				}
			}
			close(fd);
		}
	}
	if (!have_hash)
	{
		if (!(ptr = getenv(USER_ENV)))
		{
			UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, USER_ENV);
			return -1;
		}
		strncpy(hash_in, ptr, passwd_len);
		if (!(ptr = getenv(GTM_DIST_ENV)))
		{
			UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, GTM_DIST_ENV);
			return -1;
		}
		SNPRINTF(mumps_exe, GTM_PATH_MAX, "%s/%s", ptr, "mumps");
		if (0 == stat(mumps_exe, &stat_info))
		{
			SNPRINTF(tmp, GTM_PASSPHRASE_MAX, "%ld", (long) stat_info.st_ino);
			len = (int)STRLEN(tmp);
			if (len < passwd_len)
				strncpy(hash_in + (passwd_len - len), tmp, len);
			else
				strncpy(hash_in, tmp, passwd_len);
		} else
		{
			save_errno = errno;
			UPDATE_ERROR_STRING("Cannot find MUMPS executable in %s - %s", ptr, strerror(save_errno));
			return -1;
		}
		SHA512(hash_in, passwd_len, hash, status);
		if (0 != status)
			return -1;
		have_hash = TRUE;
	}
	assert(have_hash);
	for (i = 0; i < passwd_len; i++)
		out->address[i] = in->address[i] ^ hash[i % GTMCRYPT_HASH_LEN];
	out->length = passwd_len;
	return 0;
}

void gc_freeup_pwent(passwd_entry_t *pwent)
{
	assert(NULL != pwent);
	memset(pwent->passwd, 0, pwent->passwd_len);
	FREE(pwent->passwd);
	FREE(pwent->env_value);
	FREE(pwent);
}

int gc_update_passwd(char *name, passwd_entry_t **ppwent, char *prompt, int interactive)
{
	char		*env_name, *env_value, *passwd, *lpasswd, tmp_passwd[GTM_PASSPHRASE_MAX];
	int		len, status;
	gtm_string_t	passwd_str, tmp_passwd_str;
	passwd_entry_t	*pwent;

	if (!(lpasswd = getenv(name)))
	{
		UPDATE_ERROR_STRING(ENV_UNDEF_ERROR, name);
		return -1;
	}
	pwent = *ppwent;
	if ((NULL != pwent) && (0 == strcmp(pwent->env_value, lpasswd)))
		return 0;	/* No change in the environment value. Nothing more to do. */
	len = STRLEN(lpasswd);
	assert((0 == len % 2) && (GTM_PASSPHRASE_MAX * 2 > len));
	if (NULL != pwent)
		gc_freeup_pwent(pwent);
	pwent = MALLOC(SIZEOF(passwd_entry_t));
	pwent->env_value = MALLOC(len ? len + 1 : GTM_PASSPHRASE_MAX * 2 + 1);
	pwent->passwd_len = len ? len / 2 + 1 : GTM_PASSPHRASE_MAX + 1;
	pwent->passwd = MALLOC(pwent->passwd_len);
	env_name = pwent->env_name;
	env_value = pwent->env_value;
	passwd = pwent->passwd;
	strncpy(env_name, name, SIZEOF(pwent->env_name));
	env_name[SIZEOF(pwent->env_name) - 1] = '\0';
	if (0 < len)
	{
		/* First, convert from hexadecimal representation to regular representation */
		GC_UNHEX(lpasswd, passwd, len);
		/* Now, unobfuscate to get the real password */
		passwd_str.address = passwd;
		passwd_str.length = len / 2;
		if (0 == (status = gc_mask_unmask_passwd(2, &passwd_str, &passwd_str)))
		{
			 strcpy(env_value, lpasswd);	/* Store the hexadecimal representation in environment */
			 passwd[len / 2] = '\0';	/* null-terminate the password string */
			*ppwent = pwent;
		} else
			gc_freeup_pwent(pwent);
		return status;
	}
	/* Environment variable is set to an empty string. Prompt for password. But, first check if we are running in interactive
	 * mode. If not, return with an error.
	 */
	if (!interactive)
	{
		UPDATE_ERROR_STRING("Environment variable %s set to empty string. "
					"Cannot prompt for password in this mode of operation.", env_name);
		gc_freeup_pwent(pwent);
		return -1;
	}
	if (-1 == gc_read_passwd(prompt, passwd, GTM_PASSPHRASE_MAX))
	{
		gc_freeup_pwent(pwent);
		return -1;
	}
	/* Obfuscate the read password and set it in the environment. */
	passwd_str.address = passwd;
	passwd_str.length = (int)STRLEN(passwd);
	tmp_passwd_str.address = &tmp_passwd[0];
	if (0 != gc_mask_unmask_passwd(2, &passwd_str, &tmp_passwd_str))
	{
		gc_freeup_pwent(pwent);
		return -1;
	}
	/* Since the obfuscated password might contain un-printable characters, represent them as hexadecimal digits. */
	GC_HEX(tmp_passwd, env_value, tmp_passwd_str.length * 2);
	setenv(env_name, env_value, TRUE);
	*ppwent = pwent;
	return 0;
}
