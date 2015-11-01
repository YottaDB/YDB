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

#ifndef lint
static char rcsid[] = "$Header: /cvsroot/sanchez-gtm/gtm/sr_unix_cm/gtcm_pkdisp.c,v 1.1.1.1 2001/05/16 14:01:54 marcinim Exp $";
#endif

#include "gtm_stdio.h"
#include <fcntl.h>

#include "mdef.h"
#include "omi.h"

GBLREF char	*omi_oprlist[];

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
int main(int argc, char_ptr_t argv[])
{
	extern int	  errno;

#ifndef __linux__

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

	extern char	 *sys_errlist[];

#ifdef __osf__
#pragma pointer_size (restore)
#endif

#endif

	omi_fd	  	fd;
	char		buff[OMI_BUFSIZ], *bptr, *xptr, *end, *chr;
	int		cc, blen, bunches, i, n, len, buf[5], j, rdmr;
	omi_vi		mlen, xlen;
	omi_li		nx;
	omi_si		hlen;
	omi_req_hdr	rh;

	bunches = 0;
	if (argc == 3)
	{
		if (argv[1][0] != '-' || argv[1][1] != 'b' || argv[1][2] != '\0')
		{
			printf("%s: bad command line arguments\n\t%s [ -b ] filename\n",
				argv[0], argv[0]);
			exit(-1);
		} else if (INV_FD_P((fd = open(argv[argc - 1], O_RDONLY))))
		{
			printf("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
				sys_errlist[errno]);
			exit(-1);
		}
	} else if (argc == 2)
	{
		if (argv[1][0] == '-' && argv[1][1] == 'b' && argv[1][2] == '\0')
			fd = fileno(stdin);
		else if (INV_FD_P((fd = open(argv[argc - 1], O_RDONLY))))
		{
			printf("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
				sys_errlist[errno]);
			exit(-1);
		}
	}
	else if (argc == 1)
		fd = fileno(stdin);
	else
	{
		printf("%s: bad command line arguments\n\t%s [ -b ] [ filename ]\n", argv[0], argv[0]);
		exit(-1);
	}
	for (blen = 0, bptr = buff, n = 1, rdmr = 1; ; )
	{
		if (rdmr)
		{
			cc = &buff[sizeof(buff)] - &bptr[blen];
			if ((cc = read(fd, &bptr[blen], cc)) < 0)
			{
				printf("%s: read(): %s", argv[0], sys_errlist[errno]);
				exit(-1);
			} else if (cc == 0)
				break;
			blen += cc;
			if (blen < OMI_VI_SIZ)
			{
				if (bptr != buff)
				{
					memmove(buff, bptr, blen);
					bptr = buff;
				}
				continue;
			}
		}
		xptr = bptr;
		OMI_VI_READ(&mlen, xptr);
		if (blen < mlen.value + 4)
		{
			if (bptr != buff)
			{
				memmove(buff, bptr, blen);
				bptr = buff;
			}
			rdmr = 1;
			continue;
		}
		rdmr = 0;
		printf("Message %d, %d bytes", n, mlen.value);
		if (argc == 3 && bunches)
		{
			OMI_LI_READ(&nx, xptr);
			printf(", %d transactions in bunch", nx.value);
			bptr += OMI_VI_SIZ + OMI_LI_SIZ;
			blen -= OMI_VI_SIZ + OMI_LI_SIZ;
		} else
		{
			nx.value = 1;
			xlen     = mlen;
		}
		puts("");
		for (i = 1; i <= nx.value; i++)
		{
			if (argc == 3 && bunches)
			{
				OMI_VI_READ(&xlen, xptr);
			}
			end = xptr + xlen.value;
			OMI_SI_READ(&hlen, xptr);
			OMI_LI_READ(&rh.op_class, xptr);
			OMI_SI_READ(&rh.op_type, xptr);
			OMI_LI_READ(&rh.user, xptr);
			OMI_LI_READ(&rh.group, xptr);
			OMI_LI_READ(&rh.seq, xptr);
			OMI_LI_READ(&rh.ref, xptr);
			if (rh.op_class.value == 1)
			{
				printf("    %s (%d bytes)", (omi_oprlist[rh.op_type.value])
				       ? omi_oprlist[rh.op_type.value] : "unknown",xlen.value);
				if (argc == 3 && bunches)
				    printf(", transaction #%d in bunch", i);
				puts("");
			} else
				printf("    (%d bytes)\n", xlen.value);
			chr  = (char *)buf;
			while (xptr < end)
			{
				fputc('\t', stdout);
				if ((len = end - xptr) > 20)
					len = 20;
				memcpy(chr, xptr, len);
				xptr += len;
				for (j = len; j < 20; j++)
					chr[j] = '\0';
				for (j = 0; j < 5; j++)
					printf("%08x ", buf[j]);
				for (j = 0; j < 20; j++)
				{
					if (j >= len)
						chr[j] = ' ';
					else if (chr[j] < 32 || chr[j] > 126)
						chr[j] = '.';
				}
				printf("%20s\n", chr);
			}
			bptr += xlen.value + 4;
			blen -= xlen.value + 4;
		}
		if (argc == 3)
			bunches = 1;
		n++;
	}
	return 0;
}
