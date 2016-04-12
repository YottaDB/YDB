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

#include <signal.h>
#ifndef GTMCI_SIGNALS_H
#define GTMCI_SIGNALS_H

void 	sig_save_ext(struct sigaction* act);
void 	sig_save_gtm(void);
void	sig_switch_gtm(void);
void	sig_switch_ext(void);
void 	gtmci_exit_handler(void);

#endif
