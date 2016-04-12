/****************************************************************
 *								*
 *	Copyright 2003, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef GTM_AIO_H
#define GTM_AIO_H

#ifndef __CYGWIN__
#include <aio.h>
#endif

#if defined(__CYGWIN__) && !defined(AIO_CANCELED)

/* minimal just to satisfy mur_init.c and mur_read_file.h.
 * More would be needed if MUR_USE_AIO were defined */

struct aiocb {
    int			aio_fildes;
    volatile void	*aio_buf;
    size_t		aio_nbytes;
    off_t		aio_offset;
    size_t		aio_bytesread;
    int			aio_errno;
};

#endif /* __CYGWIN__ empty aio.h */

#define	AIO_POLL_SLEEP_TIME	10	/* 10 msec */

#if defined(_AIX)

#define AIO_READ(FD, AIOCBP, STATUS1, STATUS2) 		\
{                                               	\
	STATUS2 = SS_NORMAL;				\
        (AIOCBP)->aio_whence = SEEK_SET;          	\
        do                                     	 	\
        {						\
                STATUS1 = aio_read(FD, AIOCBP);		\
        } while(-1 == STATUS1 && EAGAIN == errno);   	\
	if (-1 == STATUS1)				\
		STATUS1 = errno;			\
}

#define AIO_ERROR(AIOCBP, STATUS)						\
{										\
	while ((STATUS = aio_error((AIOCBP)->aio_handle)) == EINPROGRESS)	\
		SHORT_SLEEP(AIO_POLL_SLEEP_TIME);				\
}

#define AIO_RETURN(AIOCBP, STATUS)			\
{							\
	STATUS = aio_return((AIOCBP)->aio_handle);	\
}

#else
/* Non-AIX */
#define AIO_READ(FD, AIOCBP, STATUS1, STATUS2) 		\
{							\
	STATUS2 = SS_NORMAL;				\
        AIOCBP->aio_fildes = FD;       			\
        do                                      	\
        {                                       	\
                STATUS1 = aio_read(AIOCBP);          	\
        } while(-1 == STATUS1 && EAGAIN == errno);    	\
	if (-1 == STATUS1)				\
		STATUS1 = errno;			\
}

#define AIO_ERROR(AIOCBP, STATUS)					\
{									\
	while ((STATUS = aio_error(AIOCBP)) == EINPROGRESS)		\
		SHORT_SLEEP(AIO_POLL_SLEEP_TIME);			\
}

#define AIO_RETURN(AIOCBP, STATUS)			\
{							\
	STATUS = aio_return(AIOCBP);			\
}

#endif

#define AIO_CANCEL(FD, AIOCBP, STATUS)			\
{							\
	STATUS = aio_cancel(FD, AIOCBP);		\
}

#endif
