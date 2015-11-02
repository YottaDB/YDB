/****************************************************************
 *                                                              *
 *      Copyright 2009 Fidelity Information Services, Inc *
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/


#ifndef MAIN_PRAGMA_included
#define MAIN_PRAGMA_included

#ifdef __MVS__
#pragma runopts(ENVAR(_BPXK_AUTOCVT=ON))
#pragma runopts(FILETAG(AUTOCVT,AUTOTAG))
#endif

#endif /* MAIN_PRAGMA_included */
