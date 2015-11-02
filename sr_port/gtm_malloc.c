/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_malloc -- the default version

   The bulk of the GTM storage manager code now sits in gtm_malloc_src.h and
   is included twice: once in gtm_malloc.c and again in gtm_malloc_dbg.c.
   The reason for this is that the production modules built for distribution
   in the field can have both forms of storage mgmt available in the event
   that it becomes necessary to chase a corruption issue. It can now be done
   without resorting to a "debug" version. Several different levels of
   debugging will also be made available to catch various problems.

   If the DEBUG flag is not defined (indicating a pro build), the gtm_malloc
   module will expand without all the asserts and special checking making for
   a compact and efficient storage manager. The gtm_malloc_dbg module will
   expand AS IF DEBUG had been specified supplying an alternate assert filled
   version of storage mgmnt with several different levels of storage validation
   available.

   If the DEBUG flag is defined (debug or beta build), the gtm_malloc module
   will expand with all debugging information intact and the gtm_malloc_dbg
   module will expand as call backs to the gtm_malloc module since it makes
   little sense to expand the identical module twice.
*/

#include "caller_id.h"
#include "gtm_malloc_src.h"
