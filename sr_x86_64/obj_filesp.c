/****************************************************************
 *								*
 *	Copyright 2007, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * The native object (ELF) wrapper has the following format:
 *
 *	+-------------------------------+
 *	| ELF header			|
 *	+-------------------------------+
 *	| .text section (GTM object)	|
 *	+-------------------------------+
 *	| .strtab section(string table)	|
 *	+-------------------------------+
 *	| .symtab section(sym table)	|
 *	+-------------------------------+
 *	| ELF Section header table	|
 *	+-------------------------------+
 *
 * The GT.M object layout (in the .text section) is described in obj_code.c
 *
 */

#include "mdef.h"

#include "gtm_string.h"
#include <errno.h>
#include <libelf.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"

#include "compiler.h"
#include <rtnhdr.h>
#include "obj_gen.h"
#include "cgp.h"
#include "mdq.h"
#include "cmd_qlf.h"
#include "objlabel.h"	/* needed for masscomp.h */
#include "stringpool.h"
#include "parse_file.h"
#include "gtmio.h"
#include "mmemory.h"
#include "obj_file.h"
#include <obj_filesp.h>
#include "release_name.h"
#include "min_max.h"
/* The following definitions are reqquired for the new(for ELF files) create/close_obj_file.c functions */

#ifdef __linux__
#define ELF64_LINKER_FLAG       0x10
#else
#define ELF64_LINKER_FLAG       0x18
#endif /* __linux__ */


/* Platform specific action instructions when routine called from foreign language */
/* Currently just a return to caller on AIX */

#define MIN_LINK_PSECT_SIZE     0
LITDEF mach_inst jsb_action[JSB_ACTION_N_INS] = {0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc3};


GBLREF command_qualifier cmd_qlf;
GBLREF char		object_file_name[];
GBLREF int		object_file_des;
GBLREF short		object_name_len;
GBLREF mident		module_name;
GBLREF boolean_t	run_time;
GBLREF int4		gtm_object_size;
DEBUG_ONLY(GBLREF int   obj_bytes_written;)

#define GTM_LANG        "MUMPS"
static char static_string_tbl[] = {
        /* Offset 0 */  '\0',
        /* Offset 1 */  '.', 't', 'e', 'x', 't', '\0',
        /* Offset 7 */  '.', 's', 't', 'r', 't', 'a', 'b', '\0',
        /* Offset 15 */ '.', 's', 'y', 'm', 't', 'a', 'b', '\0'
};

#define SPACE_STRING_ALLOC_LEN  (SIZEOF(static_string_tbl) +    \
                                 SIZEOF(GTM_LANG) + 1 +         \
                                 SIZEOF(GTM_PRODUCT) + 1 +      \
                                 SIZEOF(GTM_RELEASE_NAME) + 1 + \
                                 SIZEOF(mident_fixed))

/* Following constants has to be in sync with above static string array(static_string_tbl) */
#define STR_SEC_TEXT_OFFSET 1
#define STR_SEC_STRTAB_OFFSET 7
#define STR_SEC_SYMTAB_OFFSET 15

#define SEC_TEXT_INDX 1
#define SEC_STRTAB_INDX 2
#define SEC_SYMTAB_INDX 3

LITREF char gtm_release_name[];
LITREF int4 gtm_release_name_len;

GBLREF mliteral 	literal_chain;
GBLREF char 		source_file_name[];
GBLREF unsigned short 	source_name_len;
GBLREF mident		routine_name;
GBLREF mident		module_name;
GBLREF int4		mlmax, mvmax;
GBLREF int4		code_size, lit_addrs, lits_size;
GBLREF int4		psect_use_tab[];	/* bytes of each psect in this module */

error_def(ERR_OBJFILERR);

/* Open the object file and write out the gtm object. Actual ELF creation happens at later stage during close_object_file */
void create_object_file(rhdtyp *rhead)
{
        int             status, rout_len;
        char            obj_name[SIZEOF(mident_fixed) + 5];
        mstr            fstr;
        parse_blk       pblk;

        error_def(ERR_FILEPARSE);

        assert(!run_time);

        DEBUG_ONLY(obj_bytes_written = 0);
        memset(&pblk, 0, SIZEOF(pblk));
        pblk.buffer = object_file_name;
        pblk.buff_size = MAX_FBUFF;

        /* create the object file */
        fstr.len = (MV_DEFINED(&cmd_qlf.object_file) ? cmd_qlf.object_file.str.len : 0);
        fstr.addr = cmd_qlf.object_file.str.addr;
        rout_len = (int)module_name.len;
        memcpy(&obj_name[0], module_name.addr, rout_len);
        memcpy(&obj_name[rout_len], DOTOBJ, SIZEOF(DOTOBJ));    /* includes null terminator */
        pblk.def1_size = rout_len + SIZEOF(DOTOBJ) - 1;         /* Length does not include null terminator */
        pblk.def1_buf = obj_name;
        status = parse_file(&fstr, &pblk);
        if (0 == (status & 1))
                rts_error(VARLSTCNT(5) ERR_FILEPARSE, 2, fstr.len, fstr.addr, status);

        object_name_len = pblk.b_esl;
        object_file_name[object_name_len] = 0;

        OPEN_OBJECT_FILE(object_file_name, O_CREAT | O_RDWR, object_file_des);
        if (FD_INVALID == object_file_des)
                rts_error(VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, errno);

/* Action instructions and marker are not kept in the same array since the type of the elements of
 * the former (uint4) may be different from the type of the elements of the latter (char).
 * 'tiz cleaner this way rather than converting one to the other type in order to be accommodated
 * in an array
 * */
        assert(JSB_ACTION_N_INS * SIZEOF(jsb_action[0]) == SIZEOF(jsb_action)); /* JSB_ACTION_N_INS maintained? */
        assert(SIZEOF(jsb_action) <= SIZEOF(rhead->jsb));                       /* overflow check */

  	memcpy(rhead->jsb, (char *)jsb_action, SIZEOF(jsb_action)); /* action instructions */
        memcpy(&rhead->jsb[SIZEOF(jsb_action)], JSB_MARKER,                     /* followed by GTM_CODE marker */
               MIN(STR_LIT_LEN(JSB_MARKER), SIZEOF(rhead->jsb) - SIZEOF(jsb_action)));

        emit_immed((char *)rhead, SIZEOF(*rhead));
}


/* At this point, we know only gtm_object has been written onto the file.
 * Read that gtm_object and wrap it up in .text section, add remaining sections to native object(ELF)
 * Update the ELF, write it out to the object file and close the object file */
void close_object_file(void)
{
        int		i, status;
        size_t		bufSize;
        ssize_t		actualSize;
        char		*gtm_obj_code, *string_tbl;
        int		symIndex, strEntrySize;
        Elf		*elf;
        Elf64_Ehdr	*ehdr;
        Elf64_Shdr	*shdr, *text_shdr, *symtab_shdr, *strtab_shdr;
        Elf_Scn		*text_scn, *symtab_scn, *strtab_scn;
        Elf_Data	*text_data, *symtab_data, *strtab_data;
        Elf64_Sym	symEntries[3];

        buff_flush();
        bufSize = gtm_object_size;
        actualSize = 0;
        string_tbl = malloc(SPACE_STRING_ALLOC_LEN);
        symIndex = 0;

        strEntrySize = SIZEOF(static_string_tbl);
        memcpy((string_tbl + symIndex), static_string_tbl, strEntrySize);
        symIndex += strEntrySize;

        strEntrySize = SIZEOF(GTM_LANG);
        memcpy((string_tbl + symIndex), GTM_LANG, strEntrySize);
        symIndex += strEntrySize;

        strEntrySize = SIZEOF(GTM_PRODUCT);
        memcpy((string_tbl + symIndex), GTM_PRODUCT, strEntrySize);
        symIndex += strEntrySize;

        strEntrySize = SIZEOF(GTM_RELEASE_NAME);
        memcpy((string_tbl + symIndex), GTM_RELEASE_NAME, strEntrySize);
        symIndex += strEntrySize;

        gtm_obj_code = (char *)malloc(bufSize);
        /* At this point, we have only the GTM object written onto the file.
         * We need to read it back and wrap inside the ELF object and
         * write a native ELF object file. */
        lseek(object_file_des, 0, SEEK_SET);
        DOREADRL(object_file_des, gtm_obj_code, bufSize, actualSize);
        /* Reset the pointer back for writing an ELF object. */
        lseek(object_file_des, 0, SEEK_SET);

        /* Generate ELF64 header */
        if (elf_version(EV_CURRENT) == EV_NONE )
        {
		FPRINTF(stderr, "Elf library out of date!n");
		GTMASSERT;
        }
        if ((elf = elf_begin(object_file_des, ELF_C_WRITE, NULL)) == 0)
        {
		FPRINTF(stderr, "elf_begin failed!\n");
		GTMASSERT;
        }
        if ( (ehdr = elf64_newehdr(elf)) == NULL )
        {
		FPRINTF(stderr, "elf64_newehdr() failed!\n");
		GTMASSERT;
        }

        ehdr->e_ident[EI_MAG0] = ELFMAG0;
        ehdr->e_ident[EI_MAG1] = ELFMAG1;
        ehdr->e_ident[EI_MAG2] = ELFMAG2;
        ehdr->e_ident[EI_MAG3] = ELFMAG3;
        ehdr->e_ident[EI_CLASS] = ELFCLASS64;
        ehdr->e_ident[EI_VERSION] = EV_CURRENT;
#ifdef __hpux
        ehdr->e_ident[EI_DATA] = ELFDATA2MSB;
        ehdr->e_ident[EI_OSABI] = ELFOSABI_HPUX;
#else
        ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
        ehdr->e_ident[EI_OSABI] = ELFOSABI_LINUX;
#endif /* __hpux */
        ehdr->e_ident[EI_ABIVERSION] = EV_CURRENT;
        ehdr->e_machine = EM_X86_64;
        ehdr->e_type = ET_REL;
        ehdr->e_version = EV_CURRENT;
        ehdr->e_shoff = SIZEOF(Elf64_Ehdr);
        ehdr->e_flags = ELF64_LINKER_FLAG;

        if ((text_scn = elf_newscn(elf)) == NULL)
        {
                FPRINTF(stderr, "elf_newscn() failed for text section!\n");
                GTMASSERT;
        }

        if ((text_data = elf_newdata(text_scn)) == NULL)
        {
                FPRINTF(stderr, "elf_newdata() failed for text section!\n");
                GTMASSERT;
        }

        text_data->d_align = SECTION_ALIGN_BOUNDARY;
        text_data->d_off  = 0LL;
        text_data->d_buf  = gtm_obj_code;
        text_data->d_type = ELF_T_REL;
        text_data->d_size = gtm_object_size;
        text_data->d_version = EV_CURRENT;

        if ((text_shdr = elf64_getshdr(text_scn)) == NULL)
        {
                FPRINTF(stderr, "elf64_getshdr() failed for text section\n");
                GTMASSERT;
        }

        text_shdr->sh_name = STR_SEC_TEXT_OFFSET;
        text_shdr->sh_type = SHT_PROGBITS;
        text_shdr->sh_flags = SHF_ALLOC | SHF_EXECINSTR;
        text_shdr->sh_entsize = gtm_object_size;

        memcpy((string_tbl +  symIndex), module_name.addr, module_name.len);
        string_tbl[symIndex + module_name.len] = '\0';

        if ((strtab_scn = elf_newscn(elf)) == NULL)
        {
                FPRINTF(stderr, "elf_newscn() failed for strtab section\n");
                GTMASSERT;
        }

        if ((strtab_data = elf_newdata(strtab_scn)) == NULL)
        {
                FPRINTF(stderr, "elf_newdata() failed for strtab section!\n");
                GTMASSERT;
        }

        strtab_data->d_align = NATIVE_WSIZE;
        strtab_data->d_buf = string_tbl;
        strtab_data->d_off = 0LL;
        strtab_data->d_size = SPACE_STRING_ALLOC_LEN;
        strtab_data->d_type = ELF_T_BYTE;
        strtab_data->d_version = EV_CURRENT;

        if ((strtab_shdr = elf64_getshdr(strtab_scn)) == NULL)
        {
                FPRINTF(stderr, "elf_getshdr() failed for strtab section!\n");
                GTMASSERT;
        }

        strtab_shdr->sh_name = STR_SEC_STRTAB_OFFSET;
        strtab_shdr->sh_type = SHT_STRTAB;
        strtab_shdr->sh_entsize = 0;
        ehdr->e_shstrndx = elf_ndxscn(strtab_scn);

        /* Creating .symbtab section */
        i = 0;

        /* NULL symbol */
        symEntries[i].st_name = 0;
        symEntries[i].st_info = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
        symEntries[i].st_other = STV_DEFAULT;
        symEntries[i].st_shndx = 0;
        symEntries[i].st_size = 0;
        symEntries[i].st_value = 0;
        i++;

        /* Module symbol */
        symEntries[i].st_name = symIndex;
        symEntries[i].st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
        symEntries[i].st_other = STV_DEFAULT;
        symEntries[i].st_shndx = SEC_TEXT_INDX;
        symEntries[i].st_size = gtm_object_size;
        symEntries[i].st_value = 0;
        i++;

        /* symbol for .text section */
        symEntries[i].st_name = STR_SEC_TEXT_OFFSET;
        symEntries[i].st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
        symEntries[i].st_other = STV_DEFAULT;
        symEntries[i].st_shndx = SEC_TEXT_INDX; /* index of the .text */
        symEntries[i].st_size = 0;
        symEntries[i].st_value = 0;
        i++;

        if ((symtab_scn = elf_newscn(elf)) == NULL)
        {
                FPRINTF(stderr, "elf_newscn() failed for symtab section!\n");
                GTMASSERT;
        }

        if ((symtab_data = elf_newdata(symtab_scn)) == NULL)
        {
                FPRINTF(stderr, "elf_newdata() failed for symtab section!\n");
                GTMASSERT;
        }

        symtab_data->d_align = NATIVE_WSIZE;
        symtab_data->d_off  = 0LL;
        symtab_data->d_buf  = symEntries;
        symtab_data->d_type = ELF_T_REL;
        symtab_data->d_size = SIZEOF(Elf64_Sym) * i;
        symtab_data->d_version = EV_CURRENT;

        if ((symtab_shdr = elf64_getshdr(symtab_scn)) == NULL)
        {
                FPRINTF(stderr, "elf_getshdr() failed for symtab section!\n");
                GTMASSERT;
        }

        symtab_shdr->sh_name = STR_SEC_SYMTAB_OFFSET;
        symtab_shdr->sh_type = SHT_SYMTAB;
        symtab_shdr->sh_entsize = SIZEOF(Elf64_Sym) ;
        symtab_shdr->sh_link = SEC_STRTAB_INDX;

        elf_flagehdr(elf, ELF_C_SET, ELF_F_DIRTY);
        if (elf_update(elf, ELF_C_WRITE) < 0)
        {
                FPRINTF(stderr, "elf_update() failed!\n");
                GTMASSERT;
        }

        elf_end(elf);

        /* Free the memory malloc'ed above */
        free(string_tbl);
        free(gtm_obj_code);

	if ((off_t)-1 == lseek(object_file_des, (off_t)0, SEEK_SET))
		rts_error(VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, errno);
}

