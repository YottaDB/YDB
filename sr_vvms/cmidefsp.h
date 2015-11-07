/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*** The following typedef's will have to be changed if this is ported to UNIX (for DUX) at least */

#ifndef CMIDEFSP_H_INCLUDED
#define CMIDEFSP_H_INCLUDED

#ifndef MDEFSP_included
typedef 	long	int4;	/* 4-byte signed integer */
typedef	unsigned long	uint4;	/* 4 byte unsigned integer */
#endif

#define ALIGN_QUAD _align(quadword)

#define CMM_MIN_PEER_LEVEL	"010"

enum nsn_type
{	connect_op, discon_op, abort_op, exit_op, pathlost, protocol,
	thirdparty, timeout, netshut, intmsg, reject, confirm,
	nsn_type_count
}; /* connect, discon, abort, exit were renamed connect_op, discon_op, abort_op, exit_op respectively to prevent name conflicts
    * with system library routines */

typedef struct
{
	unsigned short msg;
	unsigned short unit;
	unsigned char netnum;
	unsigned char netnam[3];
	unsigned char len;
	unsigned char text[3];
} cm_mbx;

typedef struct
{
	unsigned short status;
	unsigned short xfer_count;
	uint4 dev_info;
} qio_iosb;

typedef struct
{
	unsigned short dsc$w_length;
	unsigned char dsc$b_dtype;
	unsigned char dsc$b_class;
	char *dsc$a_pointer;
} cmi_descriptor;

typedef struct
{
	int4 fl;
	int4 bl;
} relque;

struct NTD
{
	relque cqh;
	qio_iosb mst;
	cmi_descriptor mnm;
	cmi_descriptor mbx;
	unsigned short dch;
	unsigned short mch;
	int (*crq)();
	void (*err)();
	void (*sht)();
	void (*dcn)();
	void (*mbx_ast)();
	bool (*acc)();
	short unsigned stt[nsn_type_count];
};

typedef struct clb_stat_struct
{
	struct
	{
		uint4 msgs,errors,bytes,last_error;
	} read,write;
} clb_stat;

struct CLB
{
	relque cqe;
	cmi_descriptor nod;
	cmi_descriptor tnd;
	struct NTD *ntd;
	unsigned short dch;
	unsigned short mun;
	uint4 usr;
	void (*err)();
	qio_iosb ios;
	unsigned short cbl;
	unsigned short mbl;
	unsigned char *mbf;
	void (*tra)();
	unsigned char sta;
	unsigned char unused1;
	unsigned short tmo;
	void (*ast)();
	struct clb_stat_struct stt;
};

typedef struct CLB clb_struct;

#include <ssdef.h>

#define CMI_MUTEX_DECL		long int was_setast
#define CMI_MUTEX_BLOCK	{ \
				was_setast = DISABLE_AST; /* save previous state */ \
				assert(SS$_WASSET == was_setast); \
			}
#define CMI_MUTEX_RESTORE { \
				if (SS$_WASSET == was_setast) /* don't enable if was disabled coming in */ \
	  				ENABLE_AST; \
			  }

#define ALIGN_QUAD	_align(quadword)

#define CM_URGDATA_OFFSET	0
#define CM_URGDATA_LEN		6

#define CMI_ERROR(s)		(0 == ((s) & 1))
#define CMI_CLB_IOSTATUS(c)	((c)->ios.status)
#define CMI_CLB_ERROR(c)	(CMI_ERROR(CMI_CLB_IOSTATUS(c)))
#define CMI_MAKE_STATUS(s)	(!CMI_ERROR(s) ? SS$_NORMAL : (s))

typedef uint4 cmi_status_t;
typedef uint4 cmi_reason_t;
typedef int cmi_unit_t;

#include <msgdef.h>

#define CMI_REASON_INTMSG	MSG$_INTMSG
#define CMI_REASON_DISCON	MSG$_DISCON
#define CMI_REASON_ABORT	MSG$_ABORT
#define CMI_REASON_EXIT		MSG$_EXIT
#define CMI_REASON_PATHLOST	MSG$_PATHLOST
#define CMI_REASON_PROTOCOL	MSG$_PROTOCOL
#define CMI_REASON_THIRDPARTY	MSG$_THIRDPARTY
#define CMI_REASON_TIMEOUT	MSG$_TIMEOUT
#define CMI_REASON_NETSHUT	MSG$_NETSHUT
#define CMI_REASON_REJECT	MSG$_REJECT
#define CMI_REASON_CONFIRM	MSG$_CONFIRM

#define CMI_IDLE(milliseconds)	hiber_start_wait_any((milliseconds))

uint4 cmi_init(cmi_descriptor *tnd, unsigned char tnr, void (*err)(), void (*crq)(), bool (*acc)());

#define cmi_realloc_mbf(clb, newsize)				\
	{							\
		if (clb->mbf)					\
		{						\
			clb->mbl = 0;				\
			free(clb->mbf);				\
		}						\
		clb->mbf = (unsigned char *)malloc(newsize); 	\
		clb->mbl = newsize;				\
	}

#endif /* CMIDEFSP_H_INCLUDED */
