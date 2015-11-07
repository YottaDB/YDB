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

#include "mdef.h"
#include <jpidef.h>
#include <ssdef.h>
#include "op.h"
#include "mvalconv.h"

static uint4 last_pid = 0;
static uint4 new_pid;
static bool pid_used = FALSE;
static int4 jpi_code = JPI$_PID;

void op_fnzpid(mint boolexpr,mval *ret)
{
	error_def(ERR_ZPIDBADARG);
	uint4 status;
	int4 pid = -1;

	if(!pid_used && boolexpr)
	{	rts_error(VARLSTCNT(1) ERR_ZPIDBADARG);
	}
	if ( !boolexpr )
	{	do
		{	status = lib$getjpi(&jpi_code
						,&pid
						,0
						,&new_pid
						,0 ,0);
		}while (status == SS$_NOPRIV || status == SS$_SUSPENDED);
		if (status == SS$_NORMAL)
		{
			i2mval(ret,new_pid) ;
			pid_used = TRUE;
			last_pid = new_pid;
			return;
		}
		else
		{	rts_error(VARLSTCNT(1) status);
		}
	}
	else
	{	do
		{	do
			{	status = lib$getjpi(	 &jpi_code
							,&pid
							,0
							,&new_pid
							,0 ,0 );
			}while (status == SS$_NOPRIV || status == SS$_SUSPENDED);
		}while (new_pid != last_pid && status == SS$_NORMAL);
		switch (status )
		{
		case SS$_NOMOREPROC:
			ret->str.len = 0;
			ret->mvtype = MV_STR;
			pid_used = FALSE;
			return;
		case SS$_NORMAL:
			do
			{	status = lib$getjpi(	 &jpi_code
							,&pid
							,0
							,&new_pid
							,0 ,0 );
			}while (status == SS$_NOPRIV || status == SS$_SUSPENDED);
			if (status == SS$_NORMAL)
			{
				i2mval(ret,new_pid) ;
				pid_used = TRUE;
				last_pid = new_pid;
				return;
			}
			else if (status == SS$_NOMOREPROC)
			{	ret->str.len = 0;
				ret->mvtype = MV_STR;
				pid_used = FALSE;
				return;
			}
			else
			{	rts_error(VARLSTCNT(1)  status );
			}
		}
	}
}
