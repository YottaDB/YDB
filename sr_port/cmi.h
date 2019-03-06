/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CMI_INCLUDED
#define CMI__INCLUDED

cmi_status_t cmi_read(struct CLB *lnk);
cmi_status_t cmi_write(struct CLB *lnk);

#define CM_ERRBUFF_SIZE		90 + 1

#endif /* CMI_INCLUDED */
