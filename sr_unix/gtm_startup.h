/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_STARTUP_INCLUDED
#define GTM_STARTUP_INCLUDED

void 	gtm_startup(struct startup_vector *svec);
void	init_gtm(void);
void 	gtm_init_env(rhdtyp* base_addr, unsigned char* transfer_addr);
void 	lref_parse(unsigned char* lref, mstr* rtn, mstr* lab, int* offset);
void	gtm_utf8_init(void);

#endif /* GTM_STARTUP_INCLUDED */
