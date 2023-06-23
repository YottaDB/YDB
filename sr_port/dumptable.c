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

#include "gtm_string.h"

#include "compiler.h"
#include "list_file.h"
#include "dumptable.h"

static readonly char argtype[][6] =
{
	"","TVAR ","TVAL ","TINT ","TVAD "
	,"TCAD ","VREG ","MLIT ","MVAR "
	,"TRIP ","TNXT ","TJMP ","INDR "
	,"MLAB ","ILIT ","CDLT ","TEMP "
};
static readonly char	start8[] = "  stored at r8 + ";
static readonly char	start9[] = "  stored at r9 + ";

GBLREF int4		sa_temps[];
GBLREF int4		sa_temps_offset[];

void dumptable(void)
{
	char	outbuf[256];
	int i;
	unsigned char *c;

	for (i=1; i <= TCAD_REF ; i++)
	{	c = (unsigned char *)&outbuf[0];
		memcpy(c,argtype[i],5);
		c += 5;
		*c++ = ' ';
		c = i2asc(c,sa_temps[i]);

		if (i == TVAR_REF)
		{	memcpy(c, &start8[0], SIZEOF(start8) - 1);
			c += SIZEOF(start8) - 1;
		}
		else
		{	memcpy(c, &start9[0], SIZEOF(start9) - 1);
			c += SIZEOF(start9) - 1;
		}

		c = i2asc(c,sa_temps_offset[i]);
		*c++ = 0;
		list_tab();
		list_line(outbuf);
	}
}
