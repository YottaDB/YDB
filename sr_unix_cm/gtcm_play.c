/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright (c) 2017-2018 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
||||||| parent of 07a11a26bb (GT.M V7.1-004)
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
=======
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 07a11a26bb (GT.M V7.1-004)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtcm.h"
#include "cli.h"

int main(int argc, char **argv, char **envp)
{
<<<<<<< HEAD
	return dlopen_libyottadb(argc, argv, envp, "gtcm_play_main");
||||||| parent of 07a11a26bb (GT.M V7.1-004)
	omi_conn	*cptr, conn;
	int		i;
	char		buff[OMI_BUFSIZ];
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(GTCM_SERVER_IMAGE);
	/*  Open the packet log file for playback */
	if (1 == argc)
		conn.fd = fileno(stdin);
	else if (2 == argc)
	{
		if (INV_FD_P((conn.fd = open(argv[argc - 1], O_RDONLY))))
		{
			PRINTF("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
			       STRERROR(errno));
			EXIT(-1);
		}
	} else
	{
		PRINTF("%s: bad command line arguments\n\t%s [ filename ]\n",
		       argv[0], argv[0]);
		EXIT(-1);
	}
	/*  Initialize everything but the network */
	err_init(gtcm_exit_ch);
	omi_errno = OMI_ER_NO_ERROR;
	ESTABLISH_RET(omi_dbms_ch, -1);	/* any return value to signify error return */
	gtcm_init(argc, argv);
#	ifdef GTCM_RC
	rc_create_cpt();
#	endif
	REVERT;
	if (omi_errno != OMI_ER_NO_ERROR)
		EXIT(omi_errno);
	/*  Initialize the connection structure */
	conn.next   = (omi_conn *)0;
	conn.bsiz   = OMI_BUFSIZ;
	conn.bptr   = conn.buff   = buff;
	conn.xptr   = (char *)0;
	conn.blen   = 0;
	conn.exts   = 0;
	conn.state  = OMI_ST_DISC;
	conn.ga     = (ga_struct *)0;	/* struct gd_addr_struct */
	conn.of     = (oof_struct *)0;	/* struct rc_oflow */
	conn.pklog  = FD_INVALID;
	/*  Initialize the statistics */
	conn.stats.bytes_recv = 0;
	conn.stats.bytes_send = 0;
	conn.stats.start      = time((time_t *)0);
	for (i = 0; i < OMI_OP_MAX; i++)
		conn.stats.xact[i] = 0;
	for (i = 0; i < OMI_ER_MAX; i++)
		conn.stats.errs[i] = 0;

	for (;;)
		if (omi_srvc_xact(&conn) < 0)
			break;
	PRINTF("%ld seconds connect time\n", time((time_t)0) - conn.stats.start);
	PRINTF("%d OMI transactions\n", omi_nxact);
	PRINTF("%d OMI errors\n", omi_nerrs);
#	ifdef GTCM_RC
	PRINTF("%d RC transactions\n", rc_nxact);
	PRINTF("%d RC errors\n", rc_nerrs);
#	endif /* defined(GTCM_RC) */
	PRINTF("%d bytes recv'd\n", conn.stats.bytes_recv);
	PRINTF("%d bytes sent\n", conn.stats.bytes_send);
	gtcm_exit();
	return 0;
=======
	omi_conn	*cptr, conn;
	int		i;
	char		buff[OMI_BUFSIZ];
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(GTCM_SERVER_IMAGE);
	/*  Open the packet log file for playback */
	if (1 == argc)
		conn.fd = fileno(stdin);
	else if (2 == argc)
	{
		if (INV_FD_P((conn.fd = open(argv[argc - 1], O_RDONLY))))
		{
			PRINTF("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
			       STRERROR(errno));
			EXIT(-1);
		}
	} else
	{
		PRINTF("%s: bad command line arguments\n\t%s [ filename ]\n",
		       argv[0], argv[0]);
		EXIT(-1);
	}
	/*  Initialize everything but the network */
	err_init(gtcm_exit_ch);
	omi_errno = OMI_ER_NO_ERROR;
	ESTABLISH_RET(omi_dbms_ch, -1);	/* any return value to signify error return */
	gtcm_init(argc, argv);
#	ifdef GTCM_RC
	rc_create_cpt();
#	endif
	REVERT;
	if (omi_errno != OMI_ER_NO_ERROR)
		EXIT(omi_errno);
	/*  Initialize the connection structure */
	conn.next   = (omi_conn *)0;
	conn.bsiz   = OMI_BUFSIZ;
	conn.bptr   = conn.buff   = buff;
	conn.xptr   = (char *)0;
	conn.blen   = 0;
	conn.exts   = 0;
	conn.state  = OMI_ST_DISC;
	conn.ga     = (ga_struct *)0;	/* struct gd_addr_struct */
	conn.of     = (oof_struct *)0;	/* struct rc_oflow */
	conn.pklog  = FD_INVALID;
	/*  Initialize the statistics */
	conn.stats.bytes_recv = 0;
	conn.stats.bytes_send = 0;
	conn.stats.start      = time((time_t *)0);
	for (i = 0; i < OMI_OP_MAX; i++)
		conn.stats.xact[i] = 0;
	for (i = 0; i < OMI_ER_MAX; i++)
		conn.stats.errs[i] = 0;

	for (;;)
		if (omi_srvc_xact(&conn) < 0)
			break;
	PRINTF("%ld seconds connect time\n", time((time_t *)0) - conn.stats.start);
	PRINTF("%d OMI transactions\n", omi_nxact);
	PRINTF("%d OMI errors\n", omi_nerrs);
#	ifdef GTCM_RC
	PRINTF("%d RC transactions\n", rc_nxact);
	PRINTF("%d RC errors\n", rc_nerrs);
#	endif /* defined(GTCM_RC) */
	PRINTF("%d bytes recv'd\n", conn.stats.bytes_recv);
	PRINTF("%d bytes sent\n", conn.stats.bytes_send);
	gtcm_exit();
	return 0;
>>>>>>> 07a11a26bb (GT.M V7.1-004)
}
