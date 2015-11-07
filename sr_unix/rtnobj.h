/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef RTNOBJ_H_INCLUDED
#define RTNOBJ_H_INCLUDED

#ifdef AUTORELINK_SUPPORTED

#include <rtnhdr.h>

#define	LATCH_GRABBED_FALSE	FALSE
#define	LATCH_GRABBED_TRUE	TRUE

#ifdef DEBUG
void	rtnobj_verify_freelist_fl_bl(rtnobjshm_hdr_t *rtnobj_shm_hdr, sm_uc_ptr_t shm_base);
void	rtnobj_verify_min_max_free_index(rtnobjshm_hdr_t *rtnobj_shm_hdr);
#endif
void		insqt_rtnobj(que_ent_ptr_t new, que_ent_ptr_t que_base, sm_uc_ptr_t shm_base);
rtnobj_hdr_t	*remqh_rtnobj(que_ent_ptr_t base, sm_uc_ptr_t shm_base);
void		remq_rtnobj_specific(que_ent_ptr_t que_base, sm_uc_ptr_t shm_base, rtnobj_hdr_t *rtnobj);
sm_uc_ptr_t	rtnobj_shm_malloc(zro_hist *zhist, int fd, off_t obj_size, gtm_uint64_t objhash);
void		rtnobj_shm_free(rhdtyp *rhead, boolean_t latch_grabbed);

#endif

#endif /* RTNOBJ_H_INCLUDED */
