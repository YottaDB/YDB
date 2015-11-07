/****************************************************************
 *								*
 * Copyright (c) 2014-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* Any changes here must be reflected in op_fnzsocket.c especially zsocket_indextab */
ZSOCKETITEM("CURRENTINDEX", zsocket_currindex, MV_NM, level_socdev),
ZSOCKETITEM("DELIMITER", zsocket_delimiter, MV_STR|MV_NM, level_socket),
ZSOCKETITEM("DESCRIPTOR", zsocket_descriptor, MV_NM, level_socket),
ZSOCKETITEM("HOWCREATED", zsocket_howcreated, MV_STR, level_socket),
ZSOCKETITEM("INDEX", zsocket_index, MV_NM, level_socket),
ZSOCKETITEM("IOERROR", zsocket_ioerror, MV_STR, level_socket),
ZSOCKETITEM("LOCALADDRESS", zsocket_localaddress, MV_STR, level_socket),
ZSOCKETITEM("LOCALPORT", zsocket_localport, MV_NM, level_socket),
ZSOCKETITEM("MOREREADTIME", zsocket_morereadtime, MV_NM, level_socket),
ZSOCKETITEM("NUMBER", zsocket_number, MV_NM, level_socdev),
ZSOCKETITEM("PARENT", zsocket_parent, MV_STR, level_socket),
ZSOCKETITEM("PROTOCOL", zsocket_protocol, MV_STR, level_socket),
ZSOCKETITEM("REMOTEADDRESS", zsocket_remoteaddress, MV_STR, level_socket),
ZSOCKETITEM("REMOTEPORT", zsocket_remoteport, MV_NM, level_socket),
ZSOCKETITEM("SOCKETHANDLE", zsocket_sockethandle, MV_STR, level_socket),
ZSOCKETITEM("STATE", zsocket_state, MV_STR, level_socket),
ZSOCKETITEM("TLS", zsocket_tls, MV_STR, level_socket),
ZSOCKETITEM("ZBFSIZE", zsocket_zbfsize, MV_NM, level_socket),
ZSOCKETITEM("ZDELAY", zsocket_zdelay, MV_STR, level_socket),
ZSOCKETITEM("ZFF", zsocket_zff, MV_STR, level_socket),
ZSOCKETITEM("ZIBFSIZE", zsocket_zibfsize, MV_NM, level_socket)
