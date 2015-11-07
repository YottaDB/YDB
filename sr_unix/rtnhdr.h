/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef RTNHDR_H_INCLUDED
#define RTNHDR_H_INCLUDED

/* rtnhdr.h - routine header for shared binary Unix platforms */

/* There are several references to this structure from assembly language; these include:
 *
 * From Unix:	g_msf.si
 *
 * Any changes to the routine header must be reflected in those files as well.
 *
 * Warning: the list above may not be complete.
 */

/* Variable table entry */
typedef mname_entry var_tabent; /* the actual variable name is stored in the literal text pool */

/* Linenumber table entry */
typedef int4 lnr_tabent;

/* Label table entry */
typedef struct
{
	mident			lab_name;	/* The name of the label */
	lnr_tabent		*lnr_adr;	/* Pointer to lnrtab entry offset into code for this label */
	boolean_t		has_parms;	/* Flag to indicate whether the callee has a formallist */
	GTM64_ONLY(int4		filler;)
} lab_tabent;

/* Label table entry proxy for run-time linking */
typedef struct
{
	lnr_tabent		*lnr_adr;	/* Pointer to lnrtab entry offset into code for this label */
	boolean_t		has_parms;	/* Flag to indicate whether the callee has a formallist */
} lab_tabent_proxy;

/* Linkage table entry */
typedef struct
{
	char_ptr_t	ext_ref;	/* Address (quadword on alpha) this linkage entry resolves to or NULL */
} lnk_tabent;

/* rhead_struct is the routine header; it occurs at the beginning of the
 * object code part of each module. Since this structure may be resident in
 * a shared library, this structure is considered inviolate. Therefore there is
 * a process-private version of this header that is modified as necessary and
 * always points to the current version.
 *
 * The routine header is initialized when a module is first linked into
 * an executable. The fields marked with "(#)" are updated when the routine
 * is replaced by a newer version via explicit zlink.
 */
typedef struct	rhead_struct
{
	char			jsb[RHEAD_JSB_SIZE];	/* GTM_CODE object marker */
	void_ptr_t		shlib_handle;		/* Null if header not for shared object. Non-zero means header is
							 * describing shared library resident routine and this is its handle.
							 * Note this is an 8 byte field on Tru64 (hence its position near top).
							 */
	mstr			src_full_name;		/* (#) fully qualified path of routine source code */
	uint4			compiler_qlf;		/* bit flags of compiler qualifiers used (see cmd_qlf.h) */
	uint4			objlabel;		/* Object code level/label (see objlable.h).
							 * Note: this field must be the 10th word (11th on Tru64) on 32-bit
							 * environments so that incr_link() can deference object label from old
							 * (pre-V5 32-bit) objects as well. In 64-bit environments though, this
							 * situation wouldn't occur since dlopen() would/should have failed
							 * when a 32-bit shared library is loaded
							 */
	mident			routine_name;		/* external routine name */
	var_tabent		*vartab_adr;		/* (#) address of variable table (offset in original rtnhdr) */
	int4			vartab_len;		/* (#) number of variable table entries */
	lab_tabent		*labtab_adr;		/* address of label table (offset in original rtnhdr) */
	int4			labtab_len;		/* number of label table entries */
	lnr_tabent		*lnrtab_adr;		/* address of linenumber table (offset in original rtnhdr) */
	int4			lnrtab_len;		/* number of linenumber table entries */
	unsigned char		*literal_text_adr;	/* address of literal text pool (offset in original rtnhdr) */
	int4			literal_text_len;	/* length of literal text pool */
	mval			*literal_adr;		/* (#) address of literal mvals (offset in original rtnhdr) */
	int4			literal_len;		/* number of literal mvals */
	lnk_tabent		*linkage_adr;		/* (#) address of linkage Psect (offset in original rtnhdr) */
	int4			linkage_len;		/* number of linkage entries */
	int4			rel_table_off;		/* offset to relocation table (not kept) */
	int4			sym_table_off;		/* offset to symbol table (not kept) */
	unsigned char		*shared_ptext_adr;	/* If set, ptext_adr points to local copy, this points to old shared copy */
	unsigned char		*ptext_adr;		/* (#) address of start of instructions (offset in original rtnhdr) */
	unsigned char		*ptext_end_adr;		/* (#) address of end of instructions + 1 (offset in original rtnhdr) */
	int4			checksum;		/* verification value */
	int4			temp_mvals;		/* (#) temp_mvals value of current module version */
	int4			temp_size;		/* (#) temp_size value of current module version */
	struct rhead_struct	*current_rhead_adr;	/* (#) address of routine header of current module version */
	struct rhead_struct	*old_rhead_adr;		/* (#) chain of replaced routine headers */
#	ifdef GTM_TRIGGER
	void_ptr_t		trigr_handle;		/* Type is void to avoid needing gv_trigger.h for gv_trigger_t type addr */
#	endif
} rhdtyp;

/* Routine table entry */
typedef struct
{
	mident		rt_name;	/* The name of the routine (in the literal text pool) */
	rhdtyp		*rt_adr;	/* Pointer to its routine header */
} rtn_tabent;

/* byte offset of the routine_name field in the routine headers of pre-V5 releases */
#define PRE_V5_RTNHDR_RTNOFF		24

/* byte offset of the routine_name mstr (len,addr) in V50 and V51 - only used in Tru64/HPUX-HPPA */
#if defined(__osf__) || defined(__hppa)
#  define V50V51_RTNHDR_RTNMSTR_OFFSET	24
#  ifdef __osf__
#    define V50V51_FTNHDR_LITBASE_OFFSET	68
#  elif defined(__hppa)
#    define V50V51_FTNHDR_LITBASE_OFFSET	64
#  else
#    error "Unsupported platform"
#  endif
typedef struct
{
	unsigned int	len;		/* Byte length */
	int4		*addr;		/* Offset at this stage */
} v50v51_mstr;
#endif
#
/* Macros for accessing routine header fields in a portable way */
#define VARTAB_ADR(rtnhdr) ((rtnhdr)->vartab_adr)
#define LABTAB_ADR(rtnhdr) ((rtnhdr)->labtab_adr)
#define LNRTAB_ADR(rtnhdr) ((rtnhdr)->lnrtab_adr)
#define LITERAL_ADR(rtnhdr) ((rtnhdr)->literal_adr)
#define LINKAGE_ADR(rtnhdr) ((rtnhdr)->linkage_adr)
#define PTEXT_ADR(rtnhdr) ((rtnhdr)->ptext_adr)
#define PTEXT_END_ADR(rtnhdr) ((rtnhdr)->ptext_end_adr)
#define CURRENT_RHEAD_ADR(rtnhdr) ((rtnhdr)->current_rhead_adr)
#define OLD_RHEAD_ADR(rtnhdr) ((rtnhdr)->old_rhead_adr)
#define LINE_NUMBER_ADDR(rtnhdr, lnr_tabent_adr) ((rtnhdr)->ptext_adr + *(lnr_tabent_adr))
#define LABENT_LNR_ENTRY(rtnhdr, lab_tabent_adr) ((lab_tabent_adr)->lnr_adr)
#define LABEL_ADDR(rtnhdr, lab_tabent_adr) (CODE_BASE_ADDR(rtnhdr) + *(LABENT_LNR_ENTRY(rtnhdr, lab_tabent_adr)))
#define CODE_BASE_ADDR(rtnhdr) ((rtnhdr)->ptext_adr)
#define CODE_OFFSET(rtnhdr, addr) ((char *)(addr) - (char *)(rtnhdr->ptext_adr))

#define DYNAMIC_LITERALS_ENABLED(rtnhdr) ((rtnhdr)->compiler_qlf & CQ_DYNAMIC_LITERALS)
#define RW_REL_START_ADR(rtnhdr) (((DYNAMIC_LITERALS_ENABLED(rtnhdr)) ? (char *)VARTAB_ADR(rtnhdr) : (char *)LITERAL_ADR(rtnhdr)))

/* Macro to determine if given address is inside code segment. Note that even though
 * the PTEXT_END_ADR macro is the address of end_of_code + 1, we still want a <= check
 * here because in many cases, the address being tested is the RETURN address from a
 * call that was done as the last instruction in the code segment. Sometimes this call
 * is to an error or it could be the implicit quit. On HPUX, the delay slot for the
 * implicit quit call at the end of the module can also cause the problem. Without
 * the "=" check also being there, the test will fail when it should succeed.
 */
#define ADDR_IN_CODE(caddr, rtnhdr) (PTEXT_ADR((rtnhdr)) <= (caddr) && (caddr) <= PTEXT_END_ADR((rtnhdr)))

/* Types that are different depending on shared/unshared unix binaries */
#define LABENT_LNR_OFFSET lnr_adr

/* Format of a relocation datum. */
struct	relocation_info
{
		 int	r_address;	/* address which is relocated */
	unsigned int	r_symbolnum;	/* local symbol ordinal */
};

struct	rel_table
{
	struct rel_table	*next,
				*resolve;
	struct relocation_info	r;
};

/* Format of a symbol table entry; this file is included by <a.out.h>
 * and should be used if you aren't interested the a.out header
 * or relocation information.
 */
struct	nlist
{
	int4		n_type;		/* type flag, i.e. N_TEXT etc; see below */
	uint4		n_value;	/* value of this symbol (or sdb offset) */
};

struct	sym_table
{
	struct sym_table	*next;
	struct nlist		n;
	struct rel_table	*resolve;
	int4			linkage_offset;
	unsigned short		name_len;
	unsigned char		name[1];
};

/* Simple values for n_type. */
#define	N_TEXT	0x04		/* text */
#define	N_EXT	0x01		/* external bit, or'ed in */

/* Flag values for get_src_line call */
#define VERIFY		TRUE
#define NOVERIFY	FALSE

/* Prototypes */
int get_src_line(mval *routine, mval *label, int offset, mstr **srcret, boolean_t verifytrig);
unsigned char *find_line_start(unsigned char *in_addr, rhdtyp *routine);
int4 *find_line_addr(rhdtyp *routine, mstr *label, int4 offset, mident **lent_name);
rhdtyp *find_rtn_hdr(mstr *name);
bool zlput_rname(rhdtyp *hdr);
void zlmov_lnames(rhdtyp *hdr);
rhdtyp *make_dmode(void);
void comp_lits(rhdtyp *rhead);
rhdtyp  *op_rhdaddr(mval *name, rhdtyp *rhd);
lnr_tabent **op_labaddr(rhdtyp *routine, mval *label, int4 offset);
void urx_resolve(rhdtyp *rtn, lab_tabent *lbl_tab, lab_tabent *lbl_top);
char *rtnlaboff2entryref(char *entryref_buff, mident *rtn, mident *lab, int offset);

#endif /* RTNHDR_H_INCLUDED */
