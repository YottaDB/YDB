#!/bin/sh
#################################################################
#								#
#	Copyright 2009 Fidelity Information Services, Inc 	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

if [ $# -lt 1 ]; then
	echo "Usage: $0 <encryption_library> <build_type>"
	echo "encryption_library:"
	echo "gcrypt: Build the plugin with libgcrypt"
	echo "openssl: Build the plugin with OpenSSL"
	echo "build_type:"
	echo "p|P: Build the plugin with debug information"
	echo "d|D: Build the plugin without debug information"
	exit 1
fi

platform_name=`uname -s`

src_path=`pwd`
build_path=$src_path
inc_path=$src_path
NL="echo "
build_status=0

if [ $1 = openssl ]; then
	gpg_lib="-lgpgme -lcrypto -lgpg-error"
	use_crypt_library="-DUSE_OPENSSL"
elif [ $1 = gcrypt ]; then
	gpg_lib="-lgpgme -lgcrypt -lgpg-error"
	use_crypt_library="-DUSE_GCRYPT"
else
	echo "Unsupported encryption library: $1"
	exit 1
fi

debug_flag=""
if [ "d" = $2 -o "D" = $2 ]; then
	debug_flag="-DDEBUG -g"
fi

cc_compiler="cc"
ld_compiler="$cc_compiler"
ld_search_path="-L /usr/lib -L /usr/lib64 -L /usr/local/lib64 -L /usr/lib/sparcv9 -L /usr/local/ssl/lib -L /usr/local/lib"
inc_search_path="-I /usr/local/include/ -I /usr/local/ssl/include -I /usr/include"
cc_build_type=""
ld_build_type=""
cc_common="-c $debug_flag $inc_search_path"
cc_options="$cc_common -fPIC"
ld_options="-shared"
lib_name="libgtmcrypt.so"
mach_type=`uname -m`
if [ "Linux" = $platform_name -a "x86_64" = $mach_type -a "32" = "$OBJECT_MODE" ] ; then
	cc_build_type="-m32"
	ld_build_type="-m32"
fi

if [ "OS/390" = $platform_name ]; then
	# need to set _C89_CCMODE=1 on other machines
	cc_compiler="xlc"
	ld_compiler="$cc_compiler"

	cc_options="-c $debug_flag"
	cc_options="$cc_options -W c,DLL,XPLINK,EXPORTALL,RENT,NOANSIALIAS"
	cc_options="$cc_options -W l,DLL,XPLINK"
	cc_options="$cc_options -I/usr/local/include"

	cc_build_type="-q64 -qWARN64 -qchars=signed -qenum=int"
	cc_build_type="$cc_build_type -qascii -D_ENHANCED_ASCII_EXT=0xFFFFFFFF"
	cc_build_type="$cc_build_type -DFULLBLOCKWRITES -D_VARARG_EXT_"
	cc_build_type="$cc_build_type -D_XOPEN_SOURCE_EXTENDED=1 -D_ALL_SOURCE_NO_THREADS -D_ISOC99_SOURCE"
	cc_build_type="$cc_build_type -D_UNIX03_SOURCE -D_IEEEV1_COMPATIBILITY"
	lib_name="libgtmcrypt.dll"
	ld_options="-W l,DLL,XPLINK,MAP,XREF,REUS=RENT"
	ld_build_type="-q64"
	ld_search_path=""
	if [ $1 = gcrypt ]; then
		gpg_lib="/usr/local/lib/libgcrypt.x /usr/local/lib/libgpg-error.x /usr/local/lib/libgpgme.x"
	else
		gpg_lib="/usr/local/lib/libcrypto.x /usr/local/lib/libgpg-error.x /usr/local/lib/libgpgme.x"
	fi
fi
if [ "AIX" = $platform_name ]; then
	ld_build_type="-q64"
	cc_build_type="-q64"
	ld_options="-brtl -Wl,-G -bexpall -bnoentry -bh:4"
fi

if [ "HP-UX" = $platform_name ]; then
	cc_build_type="+DD64"
	ld_build_type="+DD64"
	cc_options="-Ae $cc_common -w"
	ld_options="+Z -b -Wl,-b,-B,symbolic"
fi

if [ "SunOS" = $platform_name ]; then
	cc_build_type="-m64"
	ld_build_type="-m64"
	cc_options="$cc_common -KPIC"
fi

echo "########## Compiling encryption source files ###########"
$NL
compile_common="$cc_compiler $cc_build_type $cc_options $use_crypt_library"
#compile the plugin source code
compile_verbose="$compile_common $build_path/gtmcrypt_ref.c -o $build_path/gtmcrypt_ref.o"
echo $compile_verbose
$compile_verbose
build_status=`expr $build_status + $?`
$NL
compile_verbose="$compile_common $build_path/gtmcrypt_pk_ref.c -o $build_path/gtmcrypt_pk_ref.o"
echo $compile_verbose
$compile_verbose
build_status=`expr $build_status + $?`
$NL
compile_verbose="$compile_common $build_path/gtmcrypt_dbk_ref.c -o $build_path/gtmcrypt_dbk_ref.o"
echo $compile_verbose
$compile_verbose
build_status=`expr $build_status + $?`
$NL
echo "########## Building encryption shared library ###########"
$NL
ld_common="$ld_compiler $ld_options $ld_build_type"
ld_verbose="$ld_common -o $build_path/$lib_name"
ld_verbose="$ld_verbose $build_path/gtmcrypt_ref.o $build_path/gtmcrypt_pk_ref.o $build_path/gtmcrypt_dbk_ref.o "
ld_verbose="$ld_verbose $ld_search_path $gpg_lib"
echo $ld_verbose
$ld_verbose > ld_verbose.out
build_status=`expr $build_status + $?`
$NL
cat >> $build_path/gtmcrypt.tab << tabfile
getpass:char* getpass^GETPASS(I:gtm_int_t)
tabfile

#compile maskpass.c
echo "########## Building maskpass, ascii2hex ###########"
$NL
compile_verbose="$cc_compiler $cc_build_type -I $gtm_dist $build_path/maskpass.c -o $build_path/maskpass"
echo $compile_verbose
$compile_verbose
build_status=`expr $build_status + $?`
$NL
#compile ascii2hex.c
compile_verbose="$cc_compiler $cc_build_type -I $gtm_dist $build_path/ascii2hex.c -o $build_path/ascii2hex"
echo $compile_verbose
$compile_verbose
build_status=`expr $build_status + $?`
$NL
file $gtm_dist/mumps | grep "64" > /dev/null
if [ $? -eq 0 ]; then
	libpath="/usr/local/lib64:/usr/lib64:/usr/local/lib:/usr/lib"
else
	libpath="/usr/lib32:/emul/ia32-linux/usr/lib:/usr/local/lib:/usr/lib"
fi

rm -rf $build_path/*.o

if [ "" = "$gtm_dist" ]; then
	echo "Environment variable gtm_dist undefined. Cannot compile GETPASS.m"
	exit 1
fi

#compile GETPASS in non-UTF8 mode
LC_CTYPE=C
gtm_chset=M
export LC_CTYPE
export gtm_chset
$gtm_dist/mumps $build_path/GETPASS.m
build_status=`expr $build_status + $?`

#compile utf8 version of GETPASS
if [ -d $gtm_dist/utf8 ]; then
	if [ -d $build_path/utf8 ]; then
		rm -rf $build_path/utf8
	fi
	mkdir -p $build_path/utf8
	cp $build_path/GETPASS.m $build_path/utf8
	if [ "OS/390" = $platform_name ] ; then
		utflocale=`locale -a | grep En_US.UTF-8.lp64 | sed 's/.lp64$//'`
		gtm_chset_locale=$utflocale
		export gtm_chset_locale
	else
		utflocale=`locale -a | grep -i en_us | grep -i utf | grep '8$'`
	fi
	LC_CTYPE=$utflocale
	export LC_CTYPE
	gtm_chset=UTF-8
	export gtm_chset
	unset LC_ALL
	LIBPATH=$libpath
	LD_LIBRARY_PATH=$libpath
	export LIBPATH
	export LD_LIBRARY_PATH
	cd utf8
	$gtm_dist/mumps GETPASS.m
	build_status=`expr $build_status + $?`
fi

exit $build_status
