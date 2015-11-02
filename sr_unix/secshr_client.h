/****************************************************************
 *                                                              *
 *      Copyright 2011 Fidelity Information Services, Inc	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#ifndef SECSHR_CLIENT_HANDLER_INCLUDED
#define SECSHR_CLIENT_HANDLER_INCLUDED

void client_timer_handler(void);
int send_mesg2gtmsecshr(unsigned int, unsigned int, char *, int);
int create_server(void);

#endif /* SECSHR_HANDLER_INCLUDED */
