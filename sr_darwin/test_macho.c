/* Mach-O stuff. See man 5 Mach-O */
#include <mach-o/loader.h> /* mach_header_64 */
#include <mach-o/nlist.h>  /* nlist_64 -- symbol table */
#include <mach-o/stab.h>   /* helper for that */

#include <stdlib.h>        /* malloc */
#include <stdio.h>         /* fopen fclose perror etc */
#include <string.h>        /* strerror */

int main()
{
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

    strtab[0].n_un.n_strx = 0;
    strtab[0].n_type = N_TYPE|N_UNDF;
    strtab[0].n_sect = NO_SECT;
    strtab[0].n_desc = N_LSYM;
    strtab[0].n_value = 0LL;

    strtab[1].n_un.n_strx = 1;
    strtab[1].n_type = N_TYPE|N_SECT;
    strtab[1].n_sect = 2; /* string table */
    strtab[1].n_desc = N_GSYM;
    strtab[1].n_value = 1LL;

    int errno = 0;
    struct mach_header_64 machoHeader = {};

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
