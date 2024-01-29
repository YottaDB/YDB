#################################################################
#								#
# Copyright (c) 2013-2023 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

# Default configuration parameters. These values can be overridden by passing different values to build different build targets. For
# the list of available build targets, see below.
image = DEBUG
thirdparty = gcrypt
algo = AES256CFB
# Add options for code scanning service
scan = FALSE
# If the machine has a libgcrypt version <= 1.4.1, then FIPS mode cannot be turned on.
gcrypt_nofips = 0

# Verify that $gtm_dist is defined
ifndef gtm_dist
$(error $$gtm_dist not defined!)
endif

DISTDIR = $(gtm_dist)
PLUGINDIR = $(DISTDIR)/plugin
GTMCRYPTDIR = $(PLUGINDIR)/gtmcrypt
CURDIR = `pwd`

# Find out whether we are already in $gtm_dist/plugin/gtmcrypt directory.
NOT_IN_GTMCRYPTDIR = $(shell [ "$(CURDIR)" = "$(GTMCRYPTDIR)" ] ; echo $$?)

# Users may install GT.M without utf8 support
HAVE_UTF8 = $(shell [ -d "$(DISTDIR)/utf8" ] ; echo $$?)

# Determine machine and OS type.
UNAMESTR = $(shell uname -a)
MACHTYPE = $(shell uname -m)

ifneq (,$(findstring Linux,$(UNAMESTR)))
	FILEFLAG = -L
endif
# 64 bit version of GT.M? 0 for yes!
BIT64 = $(shell file $(FILEFLAG) $(DISTDIR)/mumps | grep -q -E '64-bit|ELF-64'; echo $$?)

# Determine if GPG 2.1+ is installed
HAVE_GPG21   = 0
HAVE_GPGCONF = $(shell /bin/sh -c "command -v gpgconf")
ifneq ($(HAVE_GPGCONF),)
	GPGBIN     = $(shell gpgconf | grep 'gpg:' | cut -d: -f3)
	GPGVER     = $(shell $(GPGBIN) --version | head -n1 | cut -d' ' -f3)
	HAVE_GPG21 = $(shell expr $(GPGVER) \>= 2.1.12)
endif
ifeq ($(HAVE_GPG21),1)
	USE_LOOPBACK = "-DUSE_GPGME_PINENTRY_MODE_LOOPBACK"
endif

# Determine if libgcrypt is installed.
HAVE_GPGCRYPT = $(shell /bin/sh -c "command -v libgcrypt-config")

# Default installation target. This allows for the build system to randomize `thirdparty' and `algo' thereby changing the default
# gtmcrypt install link.
install_targ = libgtmcrypt_$(thirdparty)_$(algo).so

# Setup build type -- debug or production.
ifneq ($(image),DEBUG)
	debug_flag =
	optimize = -O2
else
	ifneq ($(scan),TRUE)
		debug_flag = -g -DDEBUG
	else
		debug_flag = -g3 -gdwarf-2 -O0 -fno-builtin -DDEBUG -DBYPASS_MEMCPY_OVERRIDE -DSTATIC_ANALYSIS
	endif
	optimize =
endif

CC = cc

# Setup common compiler flags
CFLAGS = $(debug_flag) $(optimize) -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_LARGEFILE64_SOURCE=1 $(USE_LOOPBACK)

ifneq ($(gcrypt_nofips),0)
	gcrypt_nofips_flag = -DGCRYPT_NO_FIPS
else
	gcrypt_nofips_flag =
endif

# Set default extra CFLAGS and LDFLAGS for building maskpass and libgtmcryptutil.so. Both of these use SHA512 for password
# obfuscation which must come from either OpenSSL or Libgcrypt. Historically, on AIX we have always made OpenSSL the default
# library and on non-AIX platforms, it has been libgcrypt.
default_thirdparty_CFLAGS = -DUSE_GCRYPT
default_thirdparty_LDFLAGS = -lgcrypt -lgpg-error

# Platform specific compiler and linker flags
LIBFLAGS =
IFLAGS =
# Linux
ifneq (,$(findstring Linux,$(UNAMESTR)))
	# -fPIC for Position Independent Code.
	CFLAGS += -fPIC -std=c99 -D_GNU_SOURCE -D_XOPEN_SOURCE=600
	LDFLAGS =
	# So that dependent libraries are loaded from the parent library's load path at runtime
	RPATHFLAGS = -Wl,-rpath,'$$ORIGIN'
	# So that we can build shared library.
	LDSHR = -shared
	IFLAGS += -I /usr/local/ssl/include
	ifeq ($(BIT64),0)
		LIBFLAGS += -L /usr/local/ssl/lib -L /usr/lib/x86_64-linux-gnu
	else
		LIBFLAGS += -L /usr/local/ssl/lib -L /usr/lib/x86-linux-gnu
	endif
	icuver_msg = "gtm_icu_version is set to (${gtm_icu_version}). "
	icuver_msg := $(icuver_msg)"UTF-8 mode library installation may fail if gtm_icu_version is not set"

endif

# AIX
ifneq (,$(findstring AIX,$(UNAMESTR)))
	# -qchars=signed forces `char' type to be treated as signed chars.
	# -qsrcmsg enhances error reporting.
	# -qmaxmem=-1 does not limit the amount of memory used for local tables of specific,
	#	memory-intensive operations (in kilobytes).
	CFLAGS += -qchars=signed -qsrcmsg -qmaxmem=-1 -D_TPARM_COMPAT

	# -qro places string literals in read-only storage.
	# -qroconst places constants in read-only storage.
	# -q64 forces 64-bit object generation
	CFLAGS += -qro -qroconst -q64 -qlanglvl=extc99
	# -q64 for 64-bit object generation
	# -brtl for allowing both '.a' and '.so' to be searched at runtime.
	# -bhalt:5 is to disable warnings about duplicate symbols that come from
	#  libgtmcryptutil.so and other .so that need pulling the same object
	#  file (/lib/crt0_64.o)
	LDFLAGS = -q64 -brtl -bhalt:5
	RPATHFLAGS =
	# -G so that we can build shared library
	# -bexpall exports all symbols from the shared library.
	# -bnoentry to tell the linker that shared library has no entry point.
	LDSHR = -Wl,-G -bexpall -bnoentry -Wl,-L$(gtm_dist)/plugin
	# On AIX, build maskpass and libgtmcryptutil.so with OpenSSL's libcrypto instead of libgcrypt.
	default_thirdparty_CFLAGS = -DUSE_OPENSSL -DOPENSSL_API_COMPAT=0x10000000L
	default_thirdparty_LDFLAGS = -lcrypto
	# Set the default library
	thirdparty = openssl
	install_targ = libgtmcrypt_$(thirdparty)_$(algo).so
endif

# Common header and library paths
IFLAGS += -I /opt/freeware/include -I /usr/local/include -I /usr/include -I $(gtm_dist) -I $(CURDIR)
ifeq ($(BIT64),0)
	LIBFLAGS += -L /opt/freeware/lib64 -L /opt/freeware/lib
	LIBFLAGS += -L /usr/local/lib64
	LIBFLAGS += -L /usr/local/lib -L /usr/lib64 -L /usr/lib -L /lib64 -L /lib -L `pwd`
else
	LIBFLAGS += -L /opt/freeware/lib32 -L /opt/freeware/lib
	LIBFLAGS += -L /usr/local/lib32
	LIBFLAGS += -L /usr/local/lib -L /usr/lib32 -L /usr/lib -L /lib32 -L /lib -L `pwd`
endif

CFLAGS += $(IFLAGS)
LDFLAGS += $(LIBFLAGS) -o

COMMON_LIBS = -lgtmcryptutil -lconfig

# Lists of all files needed for building the encryption plugin.
crypt_util_srcfiles = gtmcrypt_util.c
crypt_util_hdrfiles = gtmcrypt_util.h gtmcrypt_interface.h
crypt_srcfiles = gtmcrypt_ref.c gtmcrypt_pk_ref.c gtmcrypt_dbk_ref.c gtmcrypt_sym_ref.c
crypt_hrdfiles = gtmcrypt_ref.h gtmcrypt_pk_ref.h gtmcrypt_dbk_ref.h gtmcrypt_sym_ref.h gtmcrypt_interface.h
tls_srcfiles = gtm_tls_impl.c
gen_tls_hdrfiles = gen_tls_options.h gen_tls_verify_options.h
tls_hdrfiles = gtm_tls_impl.h gtm_tls_interface.h ${gen_tls_hdrfiles}
# Intentionally clean up these files
cleanupfiles =  gen_tls_options.tmp gen_tls_verify_options.tmp whichsslh.[chi] whichsslhver

all: libgtmcryptutil.so maskpass gcrypt openssl libgtmtls.so

# Use the compiler to find the ssl.h header being used. Copy it into PWD and extract the macro definitions
# Explanation of the AWK
# - /ssl\.h/ match the line with the "ssl.h" header; assumption openssl/ssl.h is the only ssl.h include
# - /^[^"]*"/ match everything from the beginning of the line that isn't a quotation mark until it encounters
#             a quotation mark and substitute that with the null str
# - /".*$/ match everything from a quotation mark to the end of the line and substitute that with the null str
# - (possibly problematic) With full path to "ssl.h", use AWK's system func to copy the file into PWD
# - exit the script so that this action is done only once
whichsslh: Makefile
	printf '#include <openssl/ssl.h>\n#include <stdio.h>\nint main(int argc, char *argv[])' > whichsslh.c
	printf '{\nprintf("/* %%s */\\n", OPENSSL_VERSION_TEXT);return 0;}' >> whichsslh.c
	$(CC) $(IFLAGS) $(default_thirdparty_CFLAGS) -E whichsslh.c > whichsslh.i
	awk '/ssl\.h/{sub(/^[^"]*"/,"");sub(/".*/,"");print;system("cp -f "$$0" whichsslh.h");exit}' whichsslh.i
	$(CC) $(IFLAGS) whichsslh.c -o whichsslhver

# Generate the two header files using an intermediate file step. Commands are always executed ensuring that the plugins
# are recompiled only when a material change occurs
# NOTE: select between field 2 vs 3 because OpenSSL headers after 1.0.2 do "#<space>define" instead of "#define"
gen_tls_options.h: whichsslh
	awk '/define[ \t]+SSL_OP_[A-Z_v0-9]+[ \t]+/{fld=2;if($$2 ~ /define/)fld=3;print "DEFINE_SSL_OP(" $$(fld) "),"}'       \
		whichsslh.h > gen_tls_options.tmp
	./whichsslhver >> gen_tls_options.tmp
	test -e $@ && (cmp -s gen_tls_options.tmp $@ || mv gen_tls_options.tmp $@) || mv gen_tls_options.tmp $@

gen_tls_verify_options.h: whichsslh
	awk '/define[ \t]+SSL_VERIFY_[A-Z_v0-9]+[ \t]+/{fld=2;if($$2 ~ /define/)fld=3;print "DEFINE_SSL_OP(" $$(fld) "),"}'   \
		whichsslh.h > gen_tls_verify_options.tmp
	./whichsslhver >> gen_tls_verify_options.tmp
	test -e $@ && (cmp -s gen_tls_verify_options.tmp $@ || mv gen_tls_verify_options.tmp $@) || mv gen_tls_verify_options.tmp $@

# Reference Encryption Plugin Libraries
libgtmcryptutil.so: $(crypt_util_srcfiles) $(crypt_util_hdrfiles)
	@echo ; echo "Compiling $@..."
	$(CC) $(CFLAGS) $(default_thirdparty_CFLAGS) $(crypt_util_srcfiles) $(LDSHR) $(LDFLAGS) $@ $(default_thirdparty_LDFLAGS)

# Since maskpass is a standalone utility, link it (implicitly) with gtmcrypt_util.o instead of libgtmcryptutil.so. This allows
# maskpass to be run without setting LD_LIBRARY_PATH/LIBPATH to load libgtmcryptutil.so. As a standalone utility maskpass should
# not depend on functions like `gtm_malloc' and `gtm_free', so we are compiling the executable with -DUSE_SYSLIB_FUNCS.
maskpass: maskpass.c $(crypt_util_srcfiles) $(crypt_util_hdrfiles)
	@echo ; echo "Compiling $@..."
	$(CC) $(CFLAGS) -DUSE_SYSLIB_FUNCS $(default_thirdparty_CFLAGS) maskpass.c $(crypt_util_srcfiles)	\
		$(LDFLAGS) $@ $(default_thirdparty_LDFLAGS)

gcrypt: libgtmcrypt_gcrypt_AES256CFB.so

libgtmcrypt_gcrypt_AES256CFB.so: $(crypt_srcfiles) $(crypt_hdrfiles) libgtmcryptutil.so
	@echo ; echo "Compiling $@..."
	$(CC) $(CFLAGS) -DUSE_GCRYPT -DUSE_AES256CFB $(gcrypt_nofips_flag) $(crypt_srcfiles) $(LDSHR)		\
		$(RPATHFLAGS) $(LDFLAGS) $@ -lgcrypt -lgpgme -lgpg-error $(COMMON_LIBS)

openssl: libgtmcrypt_openssl_AES256CFB.so

libgtmcrypt_openssl_AES256CFB.so: $(crypt_srcfiles) $(crypt_hdrfiles) libgtmcryptutil.so
	@echo ; echo "Compiling $@..."
	$(CC) $(CFLAGS) -DUSE_OPENSSL -DOPENSSL_API_COMPAT=0x10000000L -DUSE_AES256CFB	\
		$(crypt_srcfiles) $(LDSHR) $(RPATHFLAGS) $(LDFLAGS)	\
		$@ -lcrypto -lgpgme -lgpg-error $(COMMON_LIBS)

libgtmtls.so: $(tls_srcfiles) $(tls_hdrfiles) libgtmcryptutil.so
	@echo ; echo "Compiling $@..."
	$(CC) $(CFLAGS) -DOPENSSL_API_COMPAT=0x10000000L $(tls_srcfiles) $(LDSHR) $(RPATHFLAGS) $(LDFLAGS) $@ -lssl $(COMMON_LIBS)

install: all
	@echo ; echo "Installing shared libraries to $(PLUGINDIR) and maskpass to $(PLUGINDIR)/gtmcrypt..."
	mkdir -p $(PLUGINDIR)/o/utf8 $(PLUGINDIR)/r
	cp -f *.so $(PLUGINDIR)
	sed "s;@GTM_TOP@;$(DISTDIR);" gpgagent.tab.in		> $(PLUGINDIR)/gpgagent.tab
	sed "s;@GTM_TOP@;$(DISTDIR);" gtmtlsfuncs.tab.in	> $(PLUGINDIR)/gtmtlsfuncs.tab
	ln -fs ./$(install_targ) $(PLUGINDIR)/libgtmcrypt.so
	cp -pf pinentry.m $(PLUGINDIR)/r
	(cd $(PLUGINDIR)/o      && env gtm_chset=M     ${gtm_dist}/mumps $(PLUGINDIR)/r/pinentry.m)
ifeq ($(NOT_IN_GTMCRYPTDIR),1)
	cp -pf *.sh *.m $(GTMCRYPTDIR)/
	cp -f maskpass $(GTMCRYPTDIR)/
endif
ifeq ($(HAVE_UTF8),0)
	@echo $(icuver_msg)
	(cd $(PLUGINDIR)/o/utf8 && env gtm_chset=UTF-8 ${gtm_dist}/mumps $(PLUGINDIR)/r/pinentry.m)
endif
	rm -f ${cleanupfiles}

uninstall:
	@echo ; echo "Uninstalling shared libraries from $(PLUGINDIR) and maskpass from $(PLUGINDIR)/gtmcrypt..."
	rm -f $(PLUGINDIR)/gpgagent.tab $(PLUGINDIR)/gtmtlsfuncs.tab
	rm -f $(PLUGINDIR)/libgtmcrypt*.so $(PLUGINDIR)/libgtmtls*.so
	rm -f $(PLUGINDIR)/gtmcrypt/maskpass

clean:
	@echo ; echo "Removing generated files..."
	rm -f *.so *.o ${cleanupfiles} ${gen_tls_hdrfiles}
ifeq ($(NOT_IN_GTMCRYPTDIR),1)
	rm -f maskpass
endif
