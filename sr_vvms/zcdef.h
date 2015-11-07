/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ZCDEF.H -- coincides with module ZCDEF in GTMZCALL.MAX, GTMZCALL.MLB */

#define	ZC$RETC_		0		; Return classes
#define	ZC$RETC_STATUS		1
#define	ZC$RETC_VALUE		2
#define	ZC$RETC_IGNORED		3

#define	ZC$MECH_		0		; Argument-passing mechanisms
#define	ZC$MECH_VALUE		1
#define	ZC$MECH_REFERENCE	2
#define	ZC$MECH_DESCRIPTOR	3
#define	ZC$MECH_DESCRIPTOR64	4

#define	ZC$DTYPE_		0		; Native data types
#define	ZC$DTYPE_STRING		DSC$K_DTYPE_T
#define ZC$DTYPE_BYTE		DSC$K_DTYPE_B
#define ZC$DTYPE_BYTEU		DSC$K_DTYPE_BU
#define ZC$DTYPE_WORD		DSC$K_DTYPE_W
#define ZC$DTYPE_WORDU		DSC$K_DTYPE_WU
#define	ZC$DTYPE_LONG		DSC$K_DTYPE_L
#define	ZC$DTYPE_LONGU		DSC$K_DTYPE_LU
#define	ZC$DTYPE_QUAD		DSC$K_DTYPE_Q
#define	ZC$DTYPE_FLOATING	DSC$K_DTYPE_F
#define	ZC$DTYPE_DOUBLE		DSC$K_DTYPE_G
#define	ZC$DTYPE_G_FLOATING	DSC$K_DTYPE_G
#define	ZC$DTYPE_H_FLOATING	DSC$K_DTYPE_H

#define	ZC$IQUAL_		0		; Input argument qualifiers
#define	ZC$IQUAL_CONSTANT	1
#define	ZC$IQUAL_OPTIONAL	2
#define	ZC$IQUAL_OPTIONAL_0	3
#define	ZC$IQUAL_DEFAULT	4
#define	ZC$IQUAL_REQUIRED	5
#define	ZC$IQUAL_BLOCK		6

#define	ZC$OQUAL_		0		; Output argument qualifiers
#define	ZC$OQUAL_REQUIRED	1
#define	ZC$OQUAL_DUMMY		2
#define	ZC$OQUAL_PREALLOCATE	3

