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

/*
 * This file wraps the curses tparm function to avoid a conflict with
 * the bool typedef in mdef.h and curses.h.  Since the only use
 * appears to be in sr_unix/iott_use.c, this is a reasonable detour.
 * The alternative would be to excise bool from all unix files, insuring
 * that persistent structures were correctly sized.
 */
#include "gtm_tparm.h"
#include <curses.h>
#include "gtm_term.h"
#include "gtm_sizeof.h"
#if defined(__MVS__) && __CHARSET_LIB==1	/* -qascii */
#include "ebc_xlat.h"
static char	gtm_tparm_buf[64];	/* static so it can be returned, room for expanded escape sequence */
#endif

char *gtm_tparm(char *c, int p1, int p2)
{
#if !(defined(__MVS__) && __CHARSET_LIB==1)	/* -qascii */
	return tparm(c, p1, p2, 0, 0, 0, 0, 0, 0, 0);
#else
	int	len;
	char	ebcdicbuf[32];	/* ESC_LEN in io.h is 16, allow extra */
	char	*retptr;

	len = strlen(c) + 1;
	if (SIZEOF(ebcdicbuf) <= len)
		len = SIZEOF(ebcdicbuf) - 1;
	asc_to_ebc((unsigned char *)ebcdicbuf, (unsigned char *)c, len);
	ebcdicbuf[len] = '\0';	/* ensure null terminated */
	retptr = tparm(ebcdicbuf, p1, p2, 0, 0, 0, 0, 0, 0, 0);
	len = strlen(retptr) + 1;
	if (SIZEOF(gtm_tparm_buf) <= len)
		len = SIZEOF(gtm_tparm_buf) - 1;
	ebc_to_asc((unsigned char *)gtm_tparm_buf, (unsigned char *)retptr, len);
	gtm_tparm_buf[len] = '\0';	/* ensure null terminated */
	return gtm_tparm_buf;
#endif
}

