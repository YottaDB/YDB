/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef ZSOCKET_H
#define ZSOCKET_H
enum zsocket_levels {
	level_device,		/* io_desc_struct */
	level_socdev,		/* d_socket_struct */
	level_socket		/* socket_struct */
};
#define ZSOCKETITEM(A,B,C,D) B
enum zsocket_code {
#include "zsockettab.h"
};
#undef ZSOCKETITEM
#endif
