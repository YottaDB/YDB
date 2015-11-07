/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "main_pragma.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#ifdef USE_OPENSSL
# include <openssl/sha.h>
# include <openssl/evp.h>
#elif defined USE_GCRYPT
# include <gcrypt.h>
#else
# error "Unsupported encryption library. Reference implementation currently supports openssl and gcrypt"
#endif
#include <termios.h>

#define MAX_LEN			512
#define	FSTR_LEN		7		/* %2048s */
#define GTM_PATH_MAX  		1024
#define GTM_DIST		"gtm_dist"
#define HASH_LENGTH		64	/* 512 bits = 64 bytes */

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define HEX(a, b, len)						\
{								\
	int i;							\
	for (i = 0; i < len; i+=2)				\
		sprintf(b + i, "%02X", (unsigned char)a[i/2]);	\
}

#define SIGPROCMASK(FUNC, NEWSET, OLDSET, RC)			\
{								\
        do							\
        {							\
          RC = sigprocmask(FUNC, NEWSET, OLDSET);		\
        } while (-1 == RC && EINTR == errno);			\
}

#define Tcsetattr(FDESC, WHEN, TERMPTR, RC, ERRNO)		\
{								\
	sigset_t block_ttinout;					\
	sigset_t oldset;					\
	int rc;							\
	sigemptyset(&block_ttinout);				\
	sigaddset(&block_ttinout, SIGTTIN);			\
	sigaddset(&block_ttinout, SIGTTOU);			\
	SIGPROCMASK(SIG_BLOCK, &block_ttinout, &oldset, rc);	\
	do							\
	{							\
	   RC = tcsetattr(FDESC, WHEN, TERMPTR);		\
	} while(-1 == RC && EINTR == errno);			\
	ERRNO = errno;						\
	SIGPROCMASK(SIG_SETMASK, &oldset, NULL, rc);		\
}

struct termios 			old_tty, no_echo_tty;

static void maskpass(char passwd[], size_t password_len, char hash[], size_t hash_length)
{
	size_t	i;
	for (i = 0; i < password_len; i++)
		passwd[i] = passwd[i] ^ hash[i % hash_length];
}

static int echo_off()
{
	int	fd, status, save_errno;

	fd = fileno(stdin);
	/* Save current TTY settings */
	status = tcgetattr(fd, &old_tty);
	if (0 != status)
		return 1;
	no_echo_tty = old_tty;
	no_echo_tty.c_lflag &= ~ECHO; /* Turn off echo */
	Tcsetattr(fd, TCSAFLUSH, &no_echo_tty, status, save_errno);
	return status;
}

static int echo_on()
{
	int	fd, status, save_errno;

	fd = fileno(stdin);
	Tcsetattr(fd, TCSAFLUSH, &old_tty, status, save_errno);
	return status;
}

static void prompt_passwd(char passwd[])
{
	char		fstr[FSTR_LEN];
	char		tmp[MAX_LEN + 1]; /* +1 for \n from fgets */
	char		*fgets_ret;
	int			echo_off_status;

	memset(tmp, 0, MAX_LEN + 1);
	printf("Enter Password: ");
	echo_off_status = echo_off();
	fgets_ret = fgets(tmp, MAX_LEN + 1, stdin);
	tmp[strlen(tmp) - 1] = '\0'; /* remove the /n that fgets gives */
	strncpy(passwd, tmp, MAX_LEN);
	/* Since echo_on depends on whether echo_off succeeded or not, do echo_on only if echo_off went fine */
	if (0 == echo_off_status)
		echo_on();
}

static int get_hash_via_env_var(char *hash)
{
	int 		fd;
	char 		*ob_key;
	struct stat 	stat_info;
	char 		*p;

	if (NULL == (ob_key = (char *) getenv("gtm_obfuscation_key")))
		return 1;

	fd = open(ob_key, O_RDONLY);
	if (fd == -1)
		return 1;

	if (fstat(fd, &stat_info) == -1)
		return 1;

	if (!S_ISREG(stat_info.st_mode))
		return 1;

	p = mmap(0, stat_info.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (MAP_FAILED == p)
		return 1;

	if (-1 == close(fd))
		return 1;

#	ifdef USE_OPENSSL
	EVP_Digest(p, stat_info.st_size, (unsigned char *)hash, NULL, EVP_sha512(), NULL);
#	elif defined USE_GCRYPT
	gcry_md_hash_buffer(GCRY_MD_SHA512, hash, p, stat_info.st_size );
#	endif

	/* Since we have what we want no need to check the status of the munmap */
	munmap(p, stat_info.st_size);

	return 0;
}

static int get_hash_via_username_and_inode(char *hash, char *passwd, size_t *passwd_len)
{
	char		tmp[MAX_LEN], tobehashed[MAX_LEN];
	char		mumps_ex[GTM_PATH_MAX], *user_ptr, *dist_ptr;
	size_t		ilen;
	struct stat	stat_info;

	memset(tobehashed, 0, MAX_LEN);
	memset(mumps_ex, 0, GTM_PATH_MAX);
	/* We need $USER and $gtm_dist to be defined to do the proper masking */
	if (NULL == (user_ptr = (char *)getenv("USER")))
	{
		printf("Environment variable USER not defined.\n");
		return 1;
	}
	if (NULL == (dist_ptr = (char *)getenv(GTM_DIST)))
	{
		printf("Enivronment variable gtm_dist not defined.\n");
		return 1;
	}
	snprintf(mumps_ex, GTM_PATH_MAX, "%s/%s", dist_ptr, "mumps");
	if (0 != stat(mumps_ex, &stat_info))
	{
		printf("Cannot stat %s\n", mumps_ex);
		return 1;
	}
	prompt_passwd(passwd);
	*passwd_len = strlen(passwd);
	strncpy(tobehashed, user_ptr, MIN(*passwd_len, MAX_LEN));
	snprintf(tmp, MAX_LEN, "%ld", stat_info.st_ino);
	ilen = strlen(tmp);
	/* a potential simplification is to just concatenate the userid and inode */
	if (ilen < *passwd_len)
	      strncpy(tobehashed + (*passwd_len - ilen), tmp, ilen);
	else
	      strncpy(tobehashed, tmp, *passwd_len);

#	ifdef USE_OPENSSL
	EVP_Digest(tobehashed, *passwd_len, (unsigned char *)hash, NULL, EVP_sha512(), NULL);
#	elif defined USE_GCRYPT
	gcry_md_hash_buffer(GCRY_MD_SHA512, hash, tobehashed, *passwd_len );
#	endif

	return 0;

}

int main()
{
	char		passwd[MAX_LEN], hash[HASH_LENGTH], out[MAX_LEN * 2];
	size_t		passwd_len;


#	ifdef USE_GCRYPT
	gcry_error_t err;

	/* Initialize libgcrypt so it does not put warning messages in the syslog. */
	if (!gcry_check_version(GCRYPT_VERSION))
	{
		printf("libgcrypt version mismatch. %s or higher is required.\n",
				GCRYPT_VERSION);
				exit(EXIT_FAILURE);
	}
	/* Since we will just be hashing, secure memory is not needed. */
	if (!(err = gcry_control(GCRYCTL_DISABLE_SECMEM,0)))
			err = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
	if (GPG_ERR_NO_ERROR != err)
	{
		printf("Libgcrypt error: %s\n", gcry_strerror(err));
		exit(EXIT_FAILURE);
	}
#	endif

	passwd_len = (size_t)-1;
	memset(passwd, 0, MAX_LEN);
	memset(out, 0, MAX_LEN * 2);

	if (get_hash_via_env_var(hash))
		if (get_hash_via_username_and_inode(hash, passwd, &passwd_len))
			exit(EXIT_FAILURE);
	if ((size_t)-1 == passwd_len)
	{
		prompt_passwd(passwd);
		passwd_len = strlen(passwd);
	}

	maskpass(passwd, passwd_len, hash, HASH_LENGTH);
	HEX(passwd, out, passwd_len * 2);
	printf("%s\n", out);
	return 0;
}
