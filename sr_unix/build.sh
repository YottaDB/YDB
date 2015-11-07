#!/bin/sh
#################################################################
#								#
#	Copyright 2009, 2013 Fidelity Information Services, Inc #
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if [ $# -lt 2 ]; then
	echo "Usage: $0 ENCRYPTION_LIB BUILD_TYPE [ALGORITHM]"
	echo "ENCRYPTION_LIB is either gcrypt or openssl"
	echo "	gcrypt : Build the plugin with libgcrypt"
	echo "	openssl: Build the plugin with OpenSSL"
	echo "BUILD_TYPE is either [p|P] or [d|D]"
	echo "	d|D : Build the plugin with debug information"
	echo "	p|P : Build the plugin without debug information"
	echo "ALGORITHM is either AES256CFB (default) or BLOWFISHCFB"
	echo "  AES256CFB   : Build the plugin with AES (CFB mode) with 256-bit encryption"
	echo "  BLOWFISHCFB : Build the plugin with BLOWFISH (CFB mode) with 256-bit encryption"
	exit 1
fi

if [ -z "$gtm_dist" ]; then
	echo "Environment variable gtm_dist undefined. Exiting"
	exit 1
fi

encryption_lib=$1
build_type=$2
if [ $# -eq 3 ]; then
	algorithm="$3"
else
	algorithm="AES256CFB"	# default
fi
algorithm_option="-DUSE_$algorithm"
# Clean up existing artifacts
\rm -f *.o 2>/dev/null

hostos=`uname -s`
machtype=`uname -m`

# There are two cases where we might be running 32 bit GT.M
# (a) On Linux i686
# (b) On Linux x86_64, but running 32 bit GT.M
# All the non-Linux platforms that supports encryption runs 64 bit GT.M
if [ "Linux" = "$hostos" ] ; then
	is64bit_gtm=`file $gtm_dist/mumps | grep "64" | wc -l`
else
	is64bit_gtm=1
fi

builddir=`pwd`
bld_status=0
ext=".so"

# Set debug/optimization options if needed.
if [ "d" = $build_type -o "D" = $build_type ]; then
	dbg_enable="1"
	options_optimize="-DDEBUG -g"
else
	options_optimize="-O"
fi

cc="cc"
ld="$cc"

# Set library and include search path for compiler and linker to find the dependencies based on whether the
# platform is running 32 bit GT.M.
if [ $is64bit_gtm -eq 1 ] ; then
	options_libpath="-L /usr/local/lib64 -L /usr/local/lib -L /usr/lib64 -L /usr/lib -L /lib64 -L /lib"
else
	options_libpath="-L /usr/local/lib32 -L /usr/local/lib -L /usr/lib32 -L /usr/lib -L /lib32 -L /lib"
fi
# -I $gtm_dist needed for main_pragma.h, gtmxc_types.h and gtm_common_defs.h
options_incpath="-I /usr/local/include/ -I /usr/include -I $builddir -I $gtm_dist"

base_libname="libgtmcrypt"
# Common CC options for various platforms
if [ "AIX" = "$hostos" ] ; then
	cc_common="-c -qchars=signed -qsrcmsg -qmaxmem=8192 -D_BSD=43 -D_LARGE_FILES -D_TPARM_COMPAT -D_AIO_AIX_SOURCE"
	cc_common="$cc_common -qro -qroconst -D_USE_IRS -q64"
	ld_common="-q64 -brtl"
	aix_loadmap_option="-bcalls:$builddir/$base_libname.so.map -bmap:$builddir/$base_libname.so.map"
	aix_loadmap_option="$aix_loadmap_option -bxref:$builddir/$base_libname.so.map"
	ld_shl_options="-q64 -Wl,-G -bexpall -bnoentry -bh:4 $aix_loadmap_option"
	# Reference implementation on AIX requires OpenSSL. Adjust library and include paths accordingly.
	options_libpath="-L /usr/local/ssl/lib $options_libpath"
	options_incpath="-I /usr/local/ssl/include $options_incpath"
elif [ "HP-UX" = "$hostos" -a "ia64" = "$machtype" ] ; then
	cc_common="+Z -DGTM_PIC -c -D__STDC_EXT__ -D_FILE_OFFSET_BITS=64 -D_XOPEN_SOURCE_EXTENDED  -Ae  +DD64"
	cc_common="$cc_common -D_LARGEFILE64_SOURCE +W2550"
	ld_common="-Wl,-m +DD64"
	ld_shl_options="+DD64 -Wl,-b,-B,symbolic"
	options_libpath="-L /opt/openssl/lib/hpux64 $options_libpath"
	options_incpath="-L /opt/openssl/include $options_incpath"
	# Additional DEBUG-ONLY options on HP-UX
	if  [ ! -z "$dbg_enable" ] ; then options_optimize="$options_optimize +ESdbgasm" ; fi
elif [ "SunOS" = "$hostos" ] ; then
	cc_common="-KPIC -c -D_LARGEFILE64_SOURCE=1 -D_FILE_OFFSET_BITS=64 -DSUNOS -DSHADOWPW -m64"
	ld_common="-Wl,-m,-64 -m64 -xarch=generic"
	ld_shl_options="-m64 -xarch=generic -G -z combreloc"
	options_libpath="-L /usr/lib/sparcv9 -L /usr/local/ssl/lib $options_libpath"
	options_incpath="-I /usr/local/ssl/include $options_incpath"
elif [ "Linux" = "$hostos" ] ; then
	cc_common="-c -ansi  -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64  -D_XOPEN_SOURCE=600 -fsigned-char -fPIC"
	ld_common="-Wl,-M"
	if [ "ia64" != "$machtype" ] ; then
		cc_common="$cc_common -Wmissing-prototypes -D_LARGEFILE64_SOURCE"
		# PRO options on Linux x86_64 and i686
		if  [ -z "$dbg_enable" ] ; then
			options_optimize="-O2 -fno-defer-pop -fno-strict-aliasing -ffloat-store"
			if [ "i686" = "$machtype" ] ; then options_optimize="$options_optimize -march=i686" ; fi
		fi
	fi
	if [ "x86_64" = "$machtype" -a "32" = "$OBJECT_MODE" ] ; then
		cc_common="$cc_common -m32"
		ld_common="$ld_common -m32"
	fi
	ld_shl_options="-shared"
elif [ "OS/390" = "$hostos" ] ; then
	cc="xlc"
	ld="xlc"
	cc_common="-c -q64 -qWARN64 -qchars=signed -qenum=int -qascii -D_ENHANCED_ASCII_EXT=0xFFFFFFFF -D_VARARG_EXT_"
	cc_common="$cc_common -D_XOPEN_SOURCE_EXTENDED=1 -D_ALL_SOURCE_NO_THREADS -D_ISOC99_SOURCE -D_UNIX03_SOURCE"
	cc_common="$cc_common -D_IEEEV1_COMPATIBILITY -D_POSIX_C_SOURCE=200112L"
	cc_common="$cc_common -W c,DLL,XPLINK,EXPORTALL,RENT,NOANSIALIAS,LANGLVL(EXTENDED),ARCH(7)"
	cc_common="$cc_common -W l,DLL,XPLINK"
	ld_common="-q64 -W l,DLL,XPLINK,MAP,XREF,REUS=RENT"
	ld_shl_options="-q64 -W l,DLL,XPLINK"
	ext=".dll"
fi
generic_libname=$base_libname$ext
specific_libname=${base_libname}_${encryption_lib}_${algorithm}${ext}
if [ -f $builddir/$specific_libname ]; then \rm -f $specific_libname ; fi
# Needed shared libraries for building encryption plugin
crypto_libs="-lgpgme -lgpg-error"
if [ "openssl" = "$encryption_lib" ] ; then
	crypto_libs="$crypto_libs -lcrypto"
	cc_options="$cc_common $options_optimize $options_incpath -DUSE_OPENSSL $algorithm_option"
elif [ "gcrypt" = "$encryption_lib" ] ; then
	crypto_libs="$crypto_libs -lgcrypt"
	cc_options="$cc_common $options_optimize $options_incpath -DUSE_GCRYPT $algorithm_option"
else
	echo "Unsupported encryption library : $encryption_lib"
	exit 1
fi

$cc $cc_options $builddir/gtmcrypt_ref.c
bld_status=`expr $bld_status + $?`
$cc $cc_options $builddir/gtmcrypt_pk_ref.c
bld_status=`expr $bld_status + $?`
$cc $cc_options $builddir/gtmcrypt_dbk_ref.c
bld_status=`expr $bld_status + $?`

$ld $ld_common $ld_shl_options -o $builddir/$specific_libname $builddir/gtmcrypt_ref.o $builddir/gtmcrypt_pk_ref.o \
	$builddir/gtmcrypt_dbk_ref.o $options_libpath $crypto_libs > $builddir/$generic_libname.map
bld_status=`expr $bld_status + $?`

# Compile maskpass.c
$cc $cc_options $builddir/maskpass.c
$ld $ld_common -o $builddir/maskpass $builddir/maskpass.o $options_libpath $crypto_libs > $builddir/maskpass.map
bld_status=`expr $bld_status + $?`

# Remove *.o files left over from the compilations above
\rm -f *.o
if [ 0 = $bld_status ] ; then
	echo "Encryption plugin built successfully."
else
	echo "Encryption plugin build failed. Please use verbose option for more details."
fi

exit $bld_status
