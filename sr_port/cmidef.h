/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef CMIPORT_H_INCLUDED
#define CMIPORT_H_INCLUDED

#define CM_MSG_BUF_SIZE 	512		/* Message buffer size */
#define	CM_MAX_BUF_LEN		((unsigned short)0xFFFF) /* lnk->cbl is of type unsigned short, hence 64K-1 is the max currently */

/*
 * Connection States
 */
#define CM_CLB_IDLE		0
#define CM_CLB_READ		1
#define CM_CLB_WRITE		2
#define CM_CLB_CONNECT		3
#define CM_CLB_DISCONNECT	4
#define CM_CLB_WRITE_URG	5
#define CM_CLB_READ_URG		6

/* get platform specific stuff */
#include "cmidefsp.h"

cmi_status_t cmi_read(struct CLB *c);
cmi_status_t cmi_write(struct CLB *c);
cmi_status_t cmi_open(struct CLB *c);
cmi_status_t cmi_close(struct CLB *c);
struct CLB *cmu_getclb(cmi_descriptor *node, cmi_descriptor *task);
struct NTD *cmu_ntdroot(void);

#ifndef RELQUE2PTR
#define RELQUE2PTR(X) (((unsigned char *) &(X)) + ((int4) (X)))
#endif
#define PTR2RELQUE(DESTINATION,TARGET) (DESTINATION = (((unsigned char *) &(TARGET)) - ((unsigned char *) &(DESTINATION))))
#define QUEENT2CLB(QP, QH) (struct CLB *)((char *)(QP) - (char *)&(((struct CLB *)(0))->QH))

#endif /* CMI_INCLUDED */
