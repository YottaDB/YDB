/****************************************************************
 *								*
 * Copyright (c) 2022 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/*	name		symbol		device	where used		defer		argtype */
/*	If argtype is zero, the item is either enabled or disabled */
DEVOPTIONITEM("KEEPALIVE",	devopt_keepalive,	gtmsocket,	IOP_OPEN_OK|IOP_USE_OK,	TRUE,	IOP_SRC_INT),
DEVOPTIONITEM("KEEPCNT",	devopt_keepcnt,		gtmsocket,	IOP_OPEN_OK|IOP_USE_OK,	TRUE,	IOP_SRC_INT),
DEVOPTIONITEM("KEEPIDLE",	devopt_keepidle,	gtmsocket,	IOP_OPEN_OK|IOP_USE_OK,	TRUE,	IOP_SRC_INT),
DEVOPTIONITEM("KEEPINTVL",	devopt_keepintvl,	gtmsocket,	IOP_OPEN_OK|IOP_USE_OK,	TRUE,	IOP_SRC_INT),
DEVOPTIONITEM("SNDBUF",		devopt_sndbuf,		gtmsocket,	IOP_OPEN_OK|IOP_USE_OK,	TRUE,	IOP_SRC_INT)
