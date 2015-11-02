/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
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
#include <termios.h>

#define MAX_LEN			512
#define	FSTR_LEN		7		/* %2048s */
#define GTM_PATH_MAX  		1024
#define GTM_DIST		"gtm_dist"

struct termios 			old_tty, no_echo_tty;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define HEX(a, b, len)						\
{								\
	int i;							\
	for (i = 0; i < len; i+=2)				\
		sprintf(b + i, "%02X", (unsigned char)a[i/2]);	\
}

static void maskpass(char passwd[], char inode[], char user[], size_t max)
{
	size_t	i;
	for (i = 0; i < max; i++)
		passwd[i] = passwd[i] ^ inode[i] ^ user[i];
}

static int echo_off()
{
	int	fd, status;

	fd = fileno(stdin);
	/* Save current TTY settings */
	status = tcgetattr(fd, &old_tty);
	if (0 != status)
		return 1;
	no_echo_tty = old_tty;
	no_echo_tty.c_lflag &= ~ECHO; /* Turn off echo */
	status = tcsetattr(fd, TCSAFLUSH, &no_echo_tty);
	return status;
}

static int echo_on()
{
	int	fd, status;

	fd = fileno(stdin);
	status = tcsetattr(fd, TCSAFLUSH, &old_tty);
	return status;
}

static void prompt_passwd(char passwd[])
{
	char		fstr[FSTR_LEN];
	int		echo_off_status;

	sprintf(fstr, "%%%ds", MAX_LEN); /* Create the format string "%2048s" */
	printf("Enter Password: ");
	echo_off_status = echo_off();
	scanf(fstr, passwd);
	/* Since echo_on depends on whether echo_off succeeded or not, do echo_on only if echo_off went fine */
	if (0 == echo_off_status)
		echo_on();
}

int main()
{
	char		tmp[MAX_LEN], passwd[MAX_LEN], inode[MAX_LEN], user[MAX_LEN], out[MAX_LEN * 2];
	char		mumps_ex[GTM_PATH_MAX], save_user_env[MAX_LEN], *user_ptr, *dist_ptr;
	int		i;
	size_t		passwd_len, ilen;
	struct stat	stat_info;

	memset(passwd, 0, MAX_LEN);
	memset(inode, 0, MAX_LEN);
	memset(user, 0, MAX_LEN);
	memset(out, 0, MAX_LEN * 2);
	memset(mumps_ex, 0, GTM_PATH_MAX);
	/* We need $USER and $gtm_dist to be defined to do the proper masking */
	if (NULL == (user_ptr = (char *)getenv("USER")))
	{
		printf("Environment variable USER not defined.\n");
		exit(1);
	}
	strcpy(save_user_env, user_ptr);
	if (NULL == (dist_ptr = (char *)getenv(GTM_DIST)))
	{
		printf("Enivronment variable gtm_dist not defined.\n");
		exit(1);
	}
	snprintf(mumps_ex, GTM_PATH_MAX, "%s/%s", dist_ptr, "mumps");
	if (0 != stat(mumps_ex, &stat_info))
	{
		printf("Cannot stat %s\n", mumps_ex);
		exit(1);
	}
	prompt_passwd(passwd);
	passwd_len = strlen(passwd);
	strncpy(user, save_user_env, MIN(passwd_len, MAX_LEN));
	snprintf(tmp, MAX_LEN, "%ld", stat_info.st_ino);
	ilen = strlen(tmp);
	if (ilen < passwd_len)
	      strncpy(inode + (passwd_len - ilen), tmp, ilen);
	else
	      strncpy(inode, tmp, passwd_len);
	maskpass(passwd, inode, user, passwd_len);
	HEX(passwd, out, passwd_len * 2);
	printf("%s\n", out);
	return 0;
}
