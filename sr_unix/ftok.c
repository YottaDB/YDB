/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	ftok.c - display the IPC Key value for a file
 *
 *	Usage:  ftok <dbfile1> <dbfile2> ... <dbfilen>
 */

#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include <sys/types.h>
#include "gtm_ipc.h"
#include <errno.h>
#include <string.h>

#define DEFAULT_ID	43
#define ID_PREFIX	"-id="

#define PrintUsage \
	{ \
		FPRINTF(stderr, "\nUsage:\n"); \
		FPRINTF(stderr, "\t%s [%s<number>] <file1> <file2> ... <filen>\n\n", argv[0], ID_PREFIX); \
		FPRINTF(stderr, "Reports IPC Key(s) (using id 1, or <number>) of <file1> <file2> ... <filen>\n\n"); \
		exit(1); \
	}

int main (int argc, char *argv[])
{
	int	i;
	int	id = DEFAULT_ID;

#ifdef __MVS__
	__argvtoascii_a(argc, argv);
#endif
	if (argc == 1)
		PrintUsage;

	if (*argv[1] == '-')
	{
		if (strncmp(argv[1], ID_PREFIX, sizeof(ID_PREFIX) - 1) != 0 || argv[1][sizeof(ID_PREFIX) - 1] == '\0')
			PrintUsage;

		errno = 0;
		if (((id = ATOI(argv[1] + sizeof(ID_PREFIX) - 1)) == 0 && errno != 0) || id <= 0)
		{
			FPRINTF(stderr, "Invalid id %s specified, using default id %d\n", argv[1] + sizeof(ID_PREFIX) - 1, DEFAULT_ID);
			id = DEFAULT_ID;
		}
		i = 2;
	} else
		i = 1;

	PRINTF("\n");

	for ( ; i < argc; i++)
	{
		PRINTF("%20s  ::  %d  [ 0x%x ]\n", argv[i], FTOK(argv[i], id), FTOK(argv[i], id));
	}

	PRINTF("\n");
}
