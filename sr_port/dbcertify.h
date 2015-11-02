/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DBCERTIFY_H_INCLUDED
#define DBCERTIFY_H_INCLUDED

#include "gtm_stdio.h"
#ifdef UNIX
#  include "mu_all_version_standalone.h"
#endif

#ifdef VMS
#  define OPTDELIM	"/"
#  define DEFAULT_OUTFILE_SUFFIX "_DBCERTSCAN"
#  define TMPCMDFILSFX	".com"
#  define SETDISTLOGENV	"$ DEFINE/NOLOG "GTM_DIST" "
#  define DSE_START	"$ RUN "GTM_DIST":DSE.EXE"
#  define DSE_FIND_REG_ALL "FIND /REG"
#  define MUPIP_START	"$ RUN "GTM_DIST":MUPIP.EXE"
#  define RUN_CMD	"@"
#  define RESULT_ASGN	"$ DEFINE SYS$OUTPUT "
#  define DUMPRSLTFILE	"TYPE "
#  define RMS_OPEN_BIN        ,"rfm=fix","mrs=512","ctx=bin"
#else
#  define SHELL_START	"#!/bin/sh"
#  define SETDISTLOGENV	GTM_DIST"="
#  define DSE_START_PIPE_RSLT1	"$"GTM_DIST"/dse << EOF > "
#  define DSE_START_PIPE_RSLT2	" 2>&1"
#  define OPTDELIM	"-"
#  define DEFAULT_OUTFILE_SUFFIX ".dbcertscan"
#  define TMPCMDFILSFX	".sh"
#  define DSE_FIND_REG_ALL	"find -reg"
#  define MUPIP_START	"$"GTM_DIST"/mupip << EOF"
#  define RUN_CMD	"./"
#  define DUMPRSLTFILE	"cat "
#  define RMS_OPEN_BIN
#endif
#define DSE_BFLUSH	"buffer_flush"
#define DSE_QUIT	"quit"
#define DSE_OPEN_RSLT	"OPEN "OPTDELIM"FILE="TMPRSLTFILE
#define DSE_CLOSE_RSLT	"CLOSE"
#define DSE_PROMPT	"DSE> "
#define DSE_REG_LIST_START UNIX_ONLY(DSE_PROMPT)"List of global directory:"
#define MUPIP_EXTEND	"EXTEND "
#define MUPIP_BLOCKS	" "OPTDELIM"BLOCKS="
#define TMPFILEPFX	"dbcmdx"
#define TMPRSLTFILSFX	".out"
#define MAX_IEB_CNT	10

/* The "maximum" key that we use to search for righthand children of gvtroot blocks
   is simply 0xFF, 0x00, 0x00. So we don't need much here..
*/
#define MAX_DBC_KEY_SZ 3

/* A restart is triggered when, in the processing of a given block, we discover that
   some other block in its path needs to be dealt with first. Since the splitting of
   the higher (tree) level block will take care of anything else higher in the tree
   (that call may restart but it does not matter how deeply we nest, each level has
   its own restart counter), we should never need more than 1 restart but it is set
   to 3 for "cushion". SE 5/2005.
*/
#define MAX_RESTART_CNT	3
/* The version should be updated anytime the format of the header or underlying records
   is changed..

   **NOTE ** Any change here require changes in v5cbsu.m.
*/
#define P1HDR_TAG	"GTMDBC01"

/* Worst case of max level of DT tree + max level of GVT tree plus if every block in a tree gets split plus
   each split block creates a block in a different bitmap.
*/
#define MAX_BLOCK_INFO_DEPTH (MAX_BT_DEPTH * 4)

#define PREMATURE_EOF "Premature EOF on temporary results file. Command may have failed"

enum gdsblk_usage	{gdsblk_read = 1, gdsblk_update, gdsblk_create};

/* Note gdsblk_gvtleaf is used in v5cbsu.m so changes here must be reflected there ! */
enum gdsblk_type	{gdsblk_gvtgeneric = 1, gdsblk_dtgeneric,
			 gdsblk_gvtroot, gdsblk_gvtindex, gdsblk_gvtleaf,
			 gdsblk_dtroot, gdsblk_dtindex, gdsblk_dtleaf,
			 gdsblk_bitmap};

/* Structure defining the header of the phase 1 output file. Note the char fields in this
   structure use absolute lengths. This is because of the requirements that this structure
   be able to be read by the M program vb5cbsu.

   **NOTE ** Any changes here require changes in v5cbsu.m.
 */
typedef struct
{
	unsigned char	p1hdr_tag[8];		/* We are what we are */
	v15_trans_num	tn;			/* TN at point we started reading blocks */
	int4		blk_count;		/* number of blocks recorded in this file (records) */
	int4		tot_blocks;		/* Total blocks we processed (used blocks) */
	int4		dt_leaf_cnt;		/* Block type counters */
	int4		dt_index_cnt;
	int4		gvt_leaf_cnt;
	int4		gvt_index_cnt;
	unsigned char	regname[V4_MAX_RN_LEN + 1];	/* Region name for DSE */
	unsigned char	dbfn[V4_MAX_FN_LEN + 1];	/* Database file name */
	int4		uid_len;		/* Length of following unique id field (varies across platforms, used by v5cbsu) */
	unique_file_id	unique_id;		/* Make sure this DB doesn't move  */
	char		fillx[32 - SIZEOF(unique_file_id)];	/* Fill out for variable size of unique_id */
	char		fill512[152];		/* Pad out to 512 for VMS oddities with fixed record IO and hdr rewrite */
} p1hdr;

/* Structure defining the header of each record in the phase 1 output file.

   **NOTE ** Any changes here require changes in v5cbsu.m.
*/
typedef struct
{
	v15_trans_num	tn;			/* TN of this block */
	block_id	blk_num;		/* block id (number) for this block */
	enum gdsblk_type blk_type;		/* Block type */
	int4		blk_levl;		/* block level for type */
	int4		akey_len;		/* ASCII key length */
} p1rec;


typedef struct
{
	unsigned int	top;		/* Offset to top of buffer allocated for the key */
	unsigned int	end;		/* End of the current key. Offset to the second null */
	unsigned int	gvn_len;	/* Length of part of key that makes up the GVN */
	unsigned char	base[1];	/* Base of the key */
} dbc_gv_key;

typedef struct
{
	dbc_gv_key	*ins_key;	/* Describes key part of record */
	block_id	blk_id;		/* Block id to use for value of key */
} dbc_inserted_rec;

/* Structure to hold block numbers where an integrity error caused a problem the first time around */
typedef struct integ_error_blk_list_struct
{
	struct integ_error_blk_list_struct *next; /* Chain of such structures if necessary */
	int		blk_cnt;		/* Count of blccks in structure */
	block_id	blk_list[MAX_IEB_CNT];	/* List of block ids with integ errors */
} integ_error_blk_list;

/* Structure we use for pseudo cw_set_element/cache entry - allocated in an array */
typedef struct block_info_struct
{
	v15_trans_num	tn;			/* transaction number for bit maps */
	block_id	blk_num;		/* Block number or a hint block number for creates */
	enum gdsblk_usage usage;		/* Read, Update, or Create */
	enum gdsblk_type  blk_type;		/* Type of block */
	blk_segment	*upd_addr;		/* Address of the block segment array containing update info
						 * for this block */
	boolean_t	found_in_cache;		/* This block was found in the cache instead of being read */
	uchar_ptr_t	old_buff;		/* Original buffer before changes */
	uchar_ptr_t	new_buff;		/* New buffer after changes/create */
	uchar_ptr_t	prev_rec;		/* Pointer to previous record in block (in old_buff) */
	unsigned int	prev_match;		/* Match of previous record to search key */
	uchar_ptr_t	curr_rec;		/* Current record being looked at in block (in old_buff) */
	unsigned int	curr_match;		/* Match of current record to search key */
	dbc_gv_key	*curr_blk_key;		/* Key in use to lookup records in this block (last key used) */
	dbc_gv_key	*prev_blk_key;		/* Key as known at previous record (copy of curr_blk_key before it is updated) */
	dbc_inserted_rec ins_rec;		/* Record to be inserted in this block (if .ins_key.end is not 0) */
	int		blk_len;		/* (old) size of this block */
	int		blk_levl;		/* block's level */
 	/* When a block splits a new block must be created and the parent must be updated to
	 * to have a record pointing to the new block.  The created block number will not be
	 * known until the last possible moment.  Thus it is not possible to completely modify
	 * the parent.  The following field is used in such a case. When non-zero (and it should only
	 * be non-zero for created blocks), it points to the referring block's ins_key.blk_id field. The
	 * location this field targets is inside the update array for that block. When a block is assigned
	 * at commit time, this field is used to "fixup" the update array so that when the block is
	 * (re)created during commit, it contains the proper block number.
	 */
	block_id	*ins_blk_id_p;
} block_info;

/* Single malloc'd structure to hold static fields while we shuffle between our processing routines */
typedef struct
{
	int		hint_lcl;		/* Hint offset in hint_blk's local bitmap */
	int		outfd;			/* File descriptor for input file */
	int		blks_processed;		/* Counter - blocks from phase-1 we ended up splitting */
	int		blks_bypassed;		/* Counter - blocks from phase-1 no longer needed splitting */
	int		blks_too_big;		/* Counter -- blocks needing to be split */
	int		blks_read;		/* Counter - blocks we read during processing */
	int		blks_cached;		/* Counter - block reads satisfied from cache */
	int		blks_updated;		/* Counter - blocks we updated during processing */
	int		blks_created;		/* Counter - blocks we created during processing */
	int		dtlvl0;			/* Counter - Directory tree level 0 blocks */
	int		dtlvln0;		/* Counter - Directory tree not level 0 blocks */
	int		gvtlvl0;		/* Counter - Global variable tree level 0 blocks */
	int		gvtlvln0;		/* Counter - Global variable tree not level 0 blocks */
	int		gvtrchildren;		/* Counter - Right sibling children of gvtroot processed */
	int		blk_process_errors;	/* Counter - Errors encountered which keep us from certifying DB */
	int		gvtroot_rchildren_cnt;	/* Count of the inhabitants of gvtroot_rchildren[] */
	int		local_bit_map_cnt;	/* Total local bit maps in database */
	uint4		blocks_to_process;	/* Number of blocks (records) phase two will process */
	int		tmpcmdfile_len;		/* Length of tmpcmdfile */
	int		tmprsltfile_len;	/* Length of tmprsltfile */
	unsigned int	max_blk_len;		/* Max block length (including blk header) */
	unsigned int	max_rec_len;		/* Maximum record length (including rec header) */
	boolean_t	report_only;		/* Copy of input report_only flag */
	boolean_t	detail;			/* Copy of input detail flag */
	boolean_t	bsu_keys;		/* Copy of input bsu_keys flag */
	boolean_t	final;			/* This is the final pass for error blocks */
	boolean_t	phase_one;		/* When we are in phase 1 (aka scan phase)... */
	boolean_t	dbc_debug;		/* The -debug flag was specified */
	boolean_t	tmp_file_names_gend;	/* Whether we have figured out what our temp file names are */
	boolean_t	keep_temp_files;	/* Leave temp files instead of delete on exit */
	UNIX_ONLY(sem_info sem_inf[FTOK_ID_CNT];)	/* Ftok keys for id 43 and 1 (both used by older versions) */
	volatile boolean_t dbc_critical;	/* Critical section indicator for condition/signal/exit handlers */
	volatile boolean_t dbc_fhdr_dirty;	/* If fileheader has been modified, flag it */
	uchar_ptr_t	curr_lbmap_buff;	/* Buffer holding current local bit map block */
	uchar_ptr_t	block_buff;		/* Buffer holding current database block */
	unsigned char	util_cmd_buff[256];	/* Buffer for DSE and/or MUPIP command creation */
	block_info	*blk_set;		/* Max number of blocks we need per query/update (array) */
	int		block_depth;		/* How many blocks we have ref'd/used this query */
	int		block_depth_hwm;	/* High water mark for block depth */
	FILE		*tcfp;			/* File pointer for temporary command file */
	FILE		*trfp;			/* File pointer for temporary result file */
	block_id	hint_blk;		/* Hint where to start search for next free block */
	p1hdr		ofhdr;			/* Phase-1 output file header (phase-2 input file) */
	p1rec		rhdr;			/* Record in output file */
	p1rec		gvtroot_rchildren[MAX_BT_DEPTH + 1];	/* List of right hand children for a GVT root block to process */
	gd_region	*dbc_gv_cur_region;	/* our version of gv_cur_region */
	v15_sgmnt_data_ptr_t	dbc_cs_data;	/* our version of cs_data */
	dbc_gv_key	*first_rec_key;		/* Statically allocazted key buffer used for initial search */
	file_control	*fc;
	integ_error_blk_list *iebl;		/* Chain of blocks describing integ errors we encountered in 1st pass */
	dbc_gv_key	*gvn_key;		/* Used to look up GVN in directory tree */
	dbc_gv_key      *max_key;		/* Maximum possible key value */
	unsigned char	outfn[MAX_FN_LEN + 1];		/* File name argument (may be db or outfile depending on phase) */
	unsigned char	regname[MAX_RN_LEN + 1]; 	/* Buffer for region name */
	unsigned char	rslt_buff[MAX_ZWR_KEY_SZ + 1];	/* Buffer for reading from result file */
	unsigned char	tmpcmdfile[MAX_FN_LEN + 1];	/* Temporary command file name */
	unsigned char	tmprsltfile[MAX_FN_LEN + 1];	/* Temporary command result file name */
	unsigned char	tmpfiledir[MAX_FN_LEN + 1];	/* Directory where temp files created (defaults to current dir */
} phase_static_area;

/* Debug macro. This macro is compiled into even the pro-build. It emits the enclosed message if a debug
   option has been specified.*/
#define DBC_DEBUG(x) if (psa->dbc_debug) {PRINTF x; FFLUSH(stdout);}

/* We need to redefine 2 macros from gdsblkops.h (BLK_INIT and BLK_FINI) because they contain references
   to the type blk_hdr which for V5 is a different size than the v15_blk_hdr type we are using for V4 databases.
*/
#ifndef BLK_INIT
# error gdsblkops.h must be included prior to dbcertify.h
#endif
#undef BLK_INIT
#undef BLK_FINI

/* ***************************************************************************
 *	BLK_INIT(BNUM, ARRAY) allocates:
 *		blk_segment ARRAY[BLK_SEG_ARRAY_SIZE]
 *	at the next octaword-aligned location in the update array and sets
 *		BNUM = &ARRAY[1]
 */
#define BLK_INIT(BNUM, ARRAY) 									\
{												\
	update_array_ptr = (char_ptr_t)ROUND_UP2((INTPTR_T)update_array_ptr, UPDATE_ELEMENT_ALIGN_SIZE);	\
	(ARRAY) = (blk_segment *)update_array_ptr; 						\
	update_array_ptr += BLK_SEG_ARRAY_SIZE * SIZEOF(blk_segment); 				\
	assert(((update_array + update_array_size) - update_array_ptr) >= 0); 			\
	(BNUM) = (ARRAY + 1); 									\
	blk_seg_cnt = SIZEOF(v15_blk_hdr);							\
}

/* ***************************************************************************
 *	BLK_FINI(BNUM,ARRAY) finishes the update array by
 *		BNUM->addr = 0
 *		BNUM--
 *	if the blk_seg_cnt is within range, then
 *		ARRAY[0].addr = BNUM		(address of last entry containing data)
 *		ARRAY[0].len  = blk_seg_cnt	(total size of all block segments)
 *		and it returns the value of blk_seg_cnt,
 *	otherwise, it returns zero and the caller should invoke t_retry
 */
#define BLK_FINI(BNUM,ARRAY) 								\
(											\
	(BNUM--)->addr = (uchar_ptr_t)0,						\
	(blk_seg_cnt <= blk_size  &&  blk_seg_cnt >= SIZEOF(v15_blk_hdr))		\
		? (ARRAY)[0].addr = (uchar_ptr_t)(BNUM), (ARRAY)[0].len = blk_seg_cnt	\
		: 0									\
)

/* Need to redefine the IS_BML macro in gdsblk.h to use the older header */
#ifndef IS_BML
# error gdsblk.h must be included prior to dbcertify.h
#endif
#undef IS_BML
#define IS_BML(b)	(BML_LEVL == ((v15_blk_hdr_ptr_t)(b))->levl)

CONDITION_HANDLER(dbcertify_base_ch);

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
void dbcertify_parse_and_dispatch(int argc, char **argv);
#ifdef __osf__
#pragma pointer_size (restore)
#endif

void dbcertify_scan_phase(void);
void dbcertify_certify_phase(void);
void dbcertify_dbfilop(phase_static_area *psa);
#ifdef UNIX
#include <signal.h>
void dbcertify_signal_handler(int sig, siginfo_t *info, void *context);
void dbcertify_deferred_signal_handler(void);
#else
void dbcertify_exit_handler(void);
#endif
/* Routines in dbcertify_funcs.c */
void dbc_gen_temp_file_names(phase_static_area *psa);
void dbc_open_command_file(phase_static_area *psa);
void dbc_write_command_file(phase_static_area *psa, char_ptr_t cmd);
void dbc_close_command_file(phase_static_area *psa);
void dbc_run_command_file(phase_static_area *psa, char_ptr_t cmdname, char_ptr_t cmdargs, boolean_t piped_result);
void dbc_remove_command_file(phase_static_area *psa);
void dbc_open_result_file(phase_static_area *psa);
void dbc_find_database_filename(phase_static_area *psa, uchar_ptr_t regname, uchar_ptr_t dbfn);
uchar_ptr_t dbc_read_result_file(phase_static_area *psa, int errmsg, uchar_ptr_t eofmsg);
void dbc_close_result_file(phase_static_area *psa);
void dbc_remove_result_file(phase_static_area *psa);
int dbc_syscmd(char_ptr_t cmd);
int dbc_read_dbblk(phase_static_area *psa, int blk_num, enum gdsblk_type blk_type);
void dbc_init_key(phase_static_area *psa, dbc_gv_key **key);
void dbc_find_key(phase_static_area *psa, dbc_gv_key *key, uchar_ptr_t rec_p, int blk_levl);
boolean_t dbc_match_key(dbc_gv_key *key1, int blk_levl1, dbc_gv_key *key2, unsigned int *matchc);
int dbc_find_dtblk(phase_static_area *psa, dbc_gv_key *key, int min_levl);
int dbc_find_record(phase_static_area *psa, dbc_gv_key *key, int blk_index, int min_levl, enum gdsblk_type newblk_type,
		    boolean_t fail_ok);
void dbc_init_blk(phase_static_area *psa, block_info *blk_set_p, int blk_num, enum gdsblk_usage blk_usage, int blk_len,
		  int blk_levl);
void dbc_init_db(phase_static_area *psa);
void dbc_close_db(phase_static_area *psa);
void dbc_scan_phase_cleanup(void);
void dbc_certify_phase_cleanup(void);
#ifdef UNIX
void dbc_aquire_standalone_access(phase_static_area *psa);
void dbc_release_standalone_access(phase_static_area *psa);
#endif

#endif
