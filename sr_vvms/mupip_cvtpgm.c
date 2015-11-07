/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>
#include <rmsdef.h>
#include <ssdef.h>

#include "io.h"
#include "cli.h"
#include "mupip_exit.h"
#include "mupip_cvtpgm.h"
#include "trans_log_name.h"

static struct FAB	infab, outfab;
static struct RAB	inrab, outrab;
static unsigned char	buffer[512];
static int		buflen;
static char		dirname[512];
static short		full_name_len = 0;
static unsigned short	prog_converted = 0;

void mupip_cvtpgm(void)
{
	char		infilename[256];
	unsigned char	buf[MAX_TRANS_NAME_LEN];
	unsigned short	dir_len = 255;
	unsigned short	in_len = 255;
	uint4		stat;
	int		status;
	mstr		transed_dir, transed_file, untransed_dir, untransed_file;

	status = cli_get_str("FILE", infilename, &in_len);
	assert(TRUE == status);
	status = cli_get_str("DIR", dirname, &dir_len);
	assert(TRUE == status);
	infab = cc$rms_fab;
	inrab = cc$rms_rab;
	inrab.rab$l_fab = &infab;
	infab.fab$b_fac = FAB$M_GET;
	untransed_file.addr = &infilename;
	untransed_file.len = in_len;
	switch(stat = trans_log_name(&untransed_file, &transed_file, buf))
	{
		case SS$_NORMAL:
			infab.fab$l_fna = transed_file.addr;
			infab.fab$b_fns = transed_file.len;
			break;
		case SS$_NOLOGNAM:
			infab.fab$l_fna = infilename;
			infab.fab$b_fns = in_len;
			break;
		default:
			rts_error(VARLSTCNT(1) stat);
	}
	infab.fab$l_fop = FAB$M_SQO;
	infab.fab$w_mrs = 511;
	inrab.rab$l_ubf = buffer;
	inrab.rab$w_usz = SIZEOF(buffer) - 1;
	status = sys$open(&infab);
	if (RMS$_NORMAL != status)
		rts_error(status);
	status = sys$connect(&inrab);
	if (RMS$_NORMAL != status)
		rts_error(status);
	status = sys$get(&inrab);
	if (RMS$_NORMAL != status)
		rts_error(status);
	status = sys$get(&inrab);
	if (RMS$_NORMAL != status)
		rts_error(status);
	for (;;)
	{
		if (RMS$_EOF == status)
			break;
		/* get program name */
		status = sys$get(&inrab);
		if (RMS$_EOF == status)
			break;
		if (RMS$_NORMAL != status)
			rts_error(status);
		fixup();
		if (buflen < 1)
			continue;
		if (*buffer == '%')
			*buffer = '_';
		untransed_dir.addr = &dirname;
		untransed_dir.len = dir_len;
		switch(stat = trans_log_name(&untransed_dir, &transed_dir, buf))
			{
			case SS$_NORMAL:
				dir_len	= transed_dir.len;
				memcpy(&dirname, transed_dir.addr, dir_len);
				break;
			case SS$_NOLOGNAM:
				break;
			default:
				rts_error(VARLSTCNT(1) stat);
			}
		memcpy(&dirname[dir_len], buffer, buflen);
		full_name_len = dir_len + buflen;
		openoutfile(dirname, full_name_len);
		for (;;)
		{
			status = sys$get(&inrab);
			if (RMS$_EOF == status)
				break;
			if (RMS$_NORMAL != status)
				rts_error(status);
			fixup();
			if (!buflen)
			{
				closeoutfile();
				break;
			}
			putdata(buffer, buflen);
		}
		prog_converted++;
	}
	status = sys$close(&infab);
	if (RMS$_NORMAL != status)
		rts_error(status);
	mupip_exit(status);
}

openoutfile(char *fa, int fn)
{
	int	status;
	char	*ch;

	outfab = cc$rms_fab;
	outrab = cc$rms_rab;
	outrab.rab$l_fab = &outfab;
	outfab.fab$l_dna = DOTM;
	outfab.fab$b_dns = SIZEOF(DOTM);
	outfab.fab$b_fac = FAB$M_PUT;
	outfab.fab$l_fna = fa;
	outfab.fab$b_fns = fn;
	outfab.fab$l_fop = FAB$M_SQO | FAB$M_MXV;
	outfab.fab$w_mrs = 511;
	outfab.fab$b_rat = FAB$M_CR;
	outrab.rab$l_ubf = buffer;
	outrab.rab$w_usz = SIZEOF(buffer) - 1;
	status = sys$create(&outfab);
	switch (status )
	{
		case RMS$_NORMAL:
		case RMS$_CREATED:
		case RMS$_SUPERSEDE:
		case RMS$_FILEPURGED:
			break;
		default:
			rts_error(status);
	}
	status = sys$connect(&outrab);
	if (RMS$_NORMAL != status)
		rts_error(status);
}

closeoutfile()
{
	int	status;

	status = sys$close(&outfab);
	if (RMS$_NORMAL != status)
		rts_error(status);
}

putdata(unsigned char *buff, int len)
{
	char		*tempbuf;
	unsigned char	*inpt, *cp, *ctop;
	int		n;
	int		status;

	cp = tempbuf = malloc(512);
	inpt = buff;
	ctop = inpt + len;
	/* copy label */
	while (inpt < ctop && *inpt != ' ' && *inpt != '\t')
		*cp++ = *inpt++;
	/* use one tab as line separator*/
	*cp++ = '\t';
	/* get rid of spaces and tabs in input stream */
	while (inpt < ctop && ((' ' == *inpt) || ('\t' == *inpt)))
		inpt++;
	if ((n = ctop - inpt) > 0)
	{	memcpy(cp, inpt, n);
		cp += n;
	}
	outrab.rab$l_rbf = tempbuf;
	outrab.rab$w_rsz = cp - (unsigned char *) tempbuf;
	status = sys$put(&outrab);
	free(tempbuf);
	if (RMS$_NORMAL != status)
		rts_error(status);
	return;
}

fixup()
{
	buflen = inrab.rab$w_rsz;
	while (buflen > 0 &&
		(('\n' == buffer[buflen - 1]) || ('\r' == buffer[buflen - 1])))
		buflen--;
}
