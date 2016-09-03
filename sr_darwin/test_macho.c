/* Mach-O stuff. See man 5 Mach-O */
#include <mach-o/loader.h> /* mach_header_64 */
#include <mach-o/nlist.h>  /* nlist_64 -- symbol table */
#include <mach-o/stab.h>   /* helper for that */

#include <stdlib.h>        /* malloc */
#include <stdio.h>         /* fopen fclose perror etc */
#include <string.h>        /* strerror */

int main()
{
    /* NB: Mach-O objects need to be built in reverse order because we need
     * the sizes of all the stuff at the bottom to put the offsets at the top. */

    /* Sample __Text Data */
    unsigned char __text[] = 
    {
        0x55, 0x48, 0x89, 0xE5, 0x48, 0x83
    };

    /* Sample string table */
    char __strtab[] = "\0XUS\0";

    /* Sample Symbol table */
    /* from readelf:
     *    Num:    Value          Size Type    Bind   Vis      Ndx Name
     *    0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
     *    1: 0000000000000000 30048 FUNC    GLOBAL DEFAULT    1 XUS
    */
    struct nlist_64 strtab[2];
    memset(&strtab, 0, sizeof(strtab));

    strtab[0].n_un.n_strx = 0; /* Index into str tab */
    strtab[0].n_type = N_TYPE|N_UNDF; /* type flag */
    strtab[0].n_sect = NO_SECT; /* Section Number */
    strtab[0].n_desc = N_LSYM; /* Local/Global */
    strtab[0].n_value = 0LL; /* value or stab offset */

    strtab[1].n_un.n_strx = 1;
    strtab[1].n_type = N_TYPE|N_SECT;
    strtab[1].n_sect = 2; /* string table */
    strtab[1].n_desc = N_GSYM;
    strtab[1].n_value = 1LL;

    struct section_64 secText = {0};
    memcpy(&secText.sectname, "__text", 6);
    memcpy(&secText.segname, "__TEXT", 6); /* segment this section goes in */
    secText.addr = 0LL;          /* memory address of this section */
    secText.size = sizeof(__text); /* size in bytes of this section */
    secText.offset = sizeof(struct mach_header_64) + sizeof(struct segment_command_64) 
        + 3 * sizeof(struct section_64) + sizeof(struct symtab_command);
    secText.align = 16;
    secText.reloff = 0;
    secText.nreloc = 0;
    secText.flags = S_REGULAR;

    struct segment_command_64 segs = {0};
    {
    uint32_t    cmd;        /* LC_SEGMENT_64 */
    uint32_t    cmdsize;    /* includes sizeof section_64 structs */
    char        segname[16];    /* segment name */
    uint64_t    vmaddr;     /* memory address of this segment */
    uint64_t    vmsize;     /* memory size of this segment */
    uint64_t    fileoff;    /* file offset of this segment */
    uint64_t    filesize;   /* amount to map from the file */
    vm_prot_t   maxprot;    /* maximum VM protection */
    vm_prot_t   initprot;   /* initial VM protection */
    uint32_t    nsects;     /* number of sections in segment */
    uint32_t    flags;      /* flags */
    };


    int errno = 0;
    struct mach_header_64 machoHeader = {0};

    machoHeader.magic = MH_MAGIC_64;       /* mach magic number identifier */
    machoHeader.cputype = CPU_TYPE_X86_64;  /* cpu specifier */
    machoHeader.cpusubtype = CPU_SUBTYPE_X86_ALL;   /* machine specifier */
    machoHeader.filetype = MH_OBJECT;   /* type of file */
    machoHeader.ncmds = 0;  /* number of load commands:TODO */
    machoHeader.sizeofcmds = 0; /* the size of all the load commands:TODO */
    machoHeader.flags = 0;      /* flags:TODO */
    machoHeader.reserved = 0;   /* reserved */

    FILE * f = fopen("test_macho.obj","w+");

    /* Error! */
    if(!f){
        perror(strerror(errno));
        return errno;
    }

    fwrite(&machoHeader, sizeof(machoHeader), 1, f);
    fclose(f);
}
