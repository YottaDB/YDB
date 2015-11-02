/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#define BYPASS_MEMCPY_OVERRIDE  /* Signals gtm_string.h to not override memcpy(). The assert in the called routine ends
                                 * up pulling in the world in various executables so bypass for this routine.
                                 */
#include "gtm_string.h"
#undef UNIX		/* Cause non-GTM-runtime routines to be used since this is a standalone module */
#include "gtm_stdio.h"
#define UNIX
#include "gtm_stdlib.h"		/* for exit() */

/* The use of "open" below involves pulling in gtm_open() (from gtm_fd_trace.c) which in turn pulls in caller_id and the works.
 * To avoid all those from bloating this library, we skip the file-descriptor trace in this module.
 */
#undef GTM_FD_TRACE

#include "gtm_unistd.h"		/* for read() */
#include "gtm_fcntl.h"
#include <errno.h>
#include "gtm_threadgbl_init.h"
#include "omi.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF char	*omi_oprlist[];

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
int main(int argc, char_ptr_t argv[])
{
	omi_fd	  	fd;
	char		buff[OMI_BUFSIZ], *bptr, *xptr, *end, *chr;
	int		cc, blen, bunches, i, n, len, buf[5], j, rdmr;
	omi_vi		mlen, xlen;
	omi_li		nx;
	omi_si		hlen;
	omi_req_hdr	rh;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	bunches = 0;
	if (argc == 3)
	{
		if (argv[1][0] != '-' || argv[1][1] != 'b' || argv[1][2] != '\0')
		{
			PRINTF("%s: bad command line arguments\n\t%s [ -b ] filename\n",
				argv[0], argv[0]);
			exit(-1);
		} else if (INV_FD_P((fd = open(argv[argc - 1], O_RDONLY))))
		{
			PRINTF("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
				STRERROR(errno));
			exit(-1);
		}
	} else if (argc == 2)
	{
		if (argv[1][0] == '-' && argv[1][1] == 'b' && argv[1][2] == '\0')
			fd = fileno(stdin);
		else if (INV_FD_P((fd = open(argv[argc - 1], O_RDONLY))))
		{
			PRINTF("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
				STRERROR(errno));
			exit(-1);
		}
	}
	else if (argc == 1)
		fd = fileno(stdin);
	else
	{
		PRINTF("%s: bad command line arguments\n\t%s [ -b ] [ filename ]\n", argv[0], argv[0]);
		exit(-1);
	}
	for (blen = 0, bptr = buff, n = 1, rdmr = 1; ; )
	{
		if (rdmr)
		{
			cc = (int)(ARRAYTOP(buff) - &bptr[blen]);
			if ((cc = (int)(read(fd, &bptr[blen], cc))) < 0)
			{
				PRINTF("%s: read(): %s", argv[0], STRERROR(errno));
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
		PRINTF("Message %d, %ld bytes", n, (long)mlen.value);
		if (argc == 3 && bunches)
		{
			OMI_LI_READ(&nx, xptr);
			PRINTF(", %d transactions in bunch", nx.value);
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
				PRINTF("    %s (%ld bytes)", (omi_oprlist[rh.op_type.value])
				       ? omi_oprlist[rh.op_type.value] : "unknown",(long)xlen.value);
				if (argc == 3 && bunches)
				    PRINTF(", transaction #%d in bunch", i);
				puts("");
			} else
				PRINTF("    (%ld bytes)\n", (long)xlen.value);
			chr  = (char *)buf;
			while (xptr < end)
			{
				fputc('\t', stdout);
				if ((len = (int)(end - xptr)) > 20)
					len = 20;
				memcpy(chr, xptr, len);
				xptr += len;
				for (j = len; j < 20; j++)
					chr[j] = '\0';
				for (j = 0; j < 5; j++)
					PRINTF("%08x ", buf[j]);
				for (j = 0; j < 20; j++)
				{
					if (j >= len)
						chr[j] = ' ';
					else if (chr[j] < 32 || chr[j] > 126)
						chr[j] = '.';
				}
				PRINTF("%20s\n", chr);
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
