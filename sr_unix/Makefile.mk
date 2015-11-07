#################################################################
#								#
#	Copyright 2013, 2014 Fidelity Information Services, Inc	#
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
# If the machine has a libgcrypt version <= 1.4.1, then FIPS mode cannot be turned on.
gcrypt_nofips = 0
# Default HP-UX OpenSSL include/lib base directory.
HPUX_OPENSSL_ROOT = /opt/openssl/1.0.1e/

# Verify that $gtm_dist is defined
ifndef gtm_dist
$(error $$gtm_dist not defined!)
endif

DISTDIR = $(gtm_dist)
PLUGINDIR = $(DISTDIR)/plugin
CURDIR = `pwd`

# Determine machine and OS type.
UNAMESTR = $(shell uname -a)
MACHTYPE = $(shell uname -m)

ifneq (,$(findstring Linux,$(UNAMESTR)))
	FILEFLAG = -L
endif
# 64 bit system? 0 for yes!
BIT64 = $(shell file $(FILEFLAG) $(DISTDIR)/mumps | grep -q -E '64-bit|ELF-64'; echo $$?)

# Default installation target. This allows for the build system to randomize `thirdparty' and `algo' thereby changing the default
# gtmcrypt install link.
install_targ = libgtmcrypt_$(thirdparty)_$(algo).so

# Setup build type -- debug or production.
ifneq ($(image),DEBUG)
	debug_flag =
	optimize = -O2
else
	debug_flag = -g -DDEBUG
	optimize =
endif

CC = cc
LD = $(CC)

# Setup common compiler flags
CFLAGS = -c $(debug_flag) $(optimize) -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_LARGEFILE64_SOURCE=1

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
# Linux
LIBFLAGS =
IFLAGS =
ifneq (,$(findstring Linux,$(UNAMESTR)))
	# -fPIC for Position Independent Code.
	CFLAGS += -fPIC
	LDFLAGS =
	# So that dependent libraries are loaded from the parent library's load path at runtime
	RPATHFLAGS = -Wl,-rpath,'$$ORIGIN'
	# So that we can build shared library.
	LDSHR = -shared
endif

# Solaris
ifneq (,$(findstring Solaris,$(UNAMESTR)))
	# -fPIC for Position Independent Code; -m64 for 64-bit
	CFLAGS += -fPIC -m64
	LDFLAGS = -Wl,-64 -m64
	# So that dependent libraries are loaded from the parent library's load path at runtime
	RPATHFLAGS = -Wl,-R,'$$ORIGIN'
	LDSHR = -G
endif

# HP-UX
ifneq (,$(findstring HP-UX,$(UNAMESTR)))
	# +Z is for Position Independent Code; -Ae for Extended ANSI mode and +DD64 for 64-bit
	CFLAGS += +Z -Ae  +DD64
	LDFLAGS = +DD64
	# So that dependent libraries are loaded from the parent library's load path at runtime
	RPATHFLAGS = -Wl,+b,\$$ORIGIN
	# -b for shared library and -B,symbolic for assigning protected export calls to symbols.
	LDSHR = -Wl,-b,-B,symbolic
	LIBFLAGS = -L $(HPUX_OPENSSL_ROOT)/lib
	IFLAGS = -I $(HPUX_OPENSSL_ROOT)/include
endif

# AIX
ifneq (,$(findstring AIX,$(UNAMESTR)))
	# -qchars=signed forces `char' type to be treated as signed chars.
	# -qsrcmsg enhances error reporting.
	# -qmaxmem limits the amount of memory used for local tables of specific, memory-intensive operations (in kilobytes).
	CFLAGS += -qchars=signed -qsrcmsg -qmaxmem=8192 -D_TPARM_COMPAT

	# -qro places string literals in read-only storage.
	# -qroconst places constants in read-only storage.
	# -q64 forces 64-bit object generation
	CFLAGS += -qro -qroconst -q64
	# -q64 for 64-bit object generation
	# -brtl for allowing both '.a' and '.so' to be searched at runtime.
	LDFLAGS = -q64 -brtl
	RPATHFLAGS =
	# -G so that we can build shared library
	# -bexpall exports all symbols from the shared library.
	# -bnoentry to tell the linker that shared library has no entry point.
	LDSHR = -Wl,-G -bexpall -bnoentry
	# On AIX, build maskpass and libgtmcryptutil.so with OpenSSL's libcrypto instead of libgcrypt.
	default_thirdparty_CFLAGS = -DUSE_OPENSSL
	default_thirdparty_LDFLAGS = -lcrypto
endif

# Common header and library paths
IFLAGS += -I /usr/local/ssl/include -I /usr/local/include -I /usr/include -I $(gtm_dist) -I $(CURDIR)
ifeq ($(BIT64),0)
	LIBFLAGS += -L /usr/local/ssl/lib -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib64
	LIBFLAGS += -L /usr/local/lib -L /usr/lib64 -L /usr/lib -L /lib64 -L /lib -L `pwd`
else
	LIBFLAGS += -L /usr/local/ssl/lib -L /usr/lib/x86-linux-gnu -L /usr/local/lib32
	LIBFLAGS += -L /usr/local/lib -L /usr/lib32 -L /usr/lib -L /lib32 -L /lib -L `pwd`
endif

CFLAGS += $(IFLAGS)
LDFLAGS += $(LIBFLAGS) -o

COMMON_LIBS = -lgtmcryptutil -lconfig

# List of all files needed for building the encryption plugin.
crypt_srcfiles = gtmcrypt_ref.c gtmcrypt_pk_ref.c gtmcrypt_dbk_ref.c gtmcrypt_sym_ref.c
crypt_objfiles = gtmcrypt_ref.o gtmcrypt_pk_ref.o gtmcrypt_dbk_ref.o gtmcrypt_sym_ref.o
all_crypt_objfiles = $(crypt_objfiles)

tls_srcfiles = gtm_tls_impl.c gtm_tls_impl.h gtm_tls_interface.h
tls_objfiles = gtm_tls_impl.o

all_objfiles = $(all_crypt_objfiles) $(tls_objfiles) maskpass.o gtmcrypt_util.o gtmcrypt_util_syslib.o

all: libgtmcryptutil.so maskpass gcrypt openssl gtmtls
	rm -f $(crypt_objfiles) maskpass.o

gtmcrypt_util.o: gtmcrypt_util.c
	$(CC) $(CFLAGS) $(default_thirdparty_CFLAGS) $^

# Rules for building libgtmcryptutil.so
libgtmcryptutil.so: gtmcrypt_util.o
	$(LD) $^ $(LDSHR) $(LDFLAGS) $@ $(default_thirdparty_LDFLAGS)

# Rules for building maskpass
maskpass.o: maskpass.c
	$(CC) $(CFLAGS) $^

# Since maskpass is a standalone utility and doesn't depend on functions like `gtm_malloc' and `gtm_free' for memory allocation,
# build gtmcrypt_util.c with -DUSE_SYSLIB_FUNCS.
gtmcrypt_util_syslib.o: gtmcrypt_util.c
	$(CC) $(CFLAGS) -DUSE_SYSLIB_FUNCS $(default_thirdparty_CFLAGS) -o $@ $^

# Since maskpass is a standalone utility, link it with gtmcrypt_utils_syslib.o instead of libgtmcryptutil.so. This allows maskpass
# to be run without the need for the user setting LD_LIBRARY_PATH/LIBPATH to load libgtmcryptutil.so.
maskpass: maskpass.o gtmcrypt_util_syslib.o
	$(LD) $(LDFLAGS) maskpass $^ $(default_thirdparty_LDFLAGS)

# Rules for building libgtmtls.so
gtm_tls_impl.o: gtm_tls_impl.c
	$(CC) $(CFLAGS) $<

gtmtls: gtm_tls_impl.o libgtmcryptutil.so
	$(LD) $< $(LDSHR) $(RPATHFLAGS) $(LDFLAGS) lib$@.so -lssl $(COMMON_LIBS)

# Rules for building all supported variations of encryption libraries. These again point to the specific ones.
gcrypt: gcrypt_AES256CFB

openssl: openssl_AES256CFB openssl_BLOWFISHCFB

# Rules for building specific encryption libraries.
gcrypt_AES256CFB: $(crypt_srcfiles) libgtmcryptutil.so
	$(CC) $(CFLAGS) -DUSE_GCRYPT -DUSE_AES256CFB $(gcrypt_nofips_flag) $(crypt_srcfiles)
	$(LD) $(crypt_objfiles) $(LDSHR) $(RPATHFLAGS) $(LDFLAGS) libgtmcrypt_$@.so -lgcrypt -lgpgme -lgpg-error $(COMMON_LIBS)

openssl_AES256CFB: $(crypt_srcfiles) libgtmcryptutil.so
	$(CC) $(CFLAGS) -DUSE_OPENSSL -DUSE_AES256CFB $(crypt_srcfiles)
	$(LD) $(crypt_objfiles) $(LDSHR) $(RPATHFLAGS) $(LDFLAGS) libgtmcrypt_$@.so -lcrypto -lgpgme -lgpg-error $(COMMON_LIBS)

openssl_BLOWFISHCFB: $(crypt_srcfiles) libgtmcryptutil.so
	$(CC) $(CFLAGS) -DUSE_OPENSSL -DUSE_BLOWFISHCFB $(crypt_srcfiles)
	$(LD) $(crypt_objfiles) $(LDSHR) $(RPATHFLAGS) $(LDFLAGS) libgtmcrypt_$@.so -lcrypto -lgpgme -lgpg-error $(COMMON_LIBS)

# The below rule is useful when the user wants to [re]build a specific target (one of gcrypt_AES256CFB, openssl_AES256CFB or
# openssl_BLOWFISHCFB).
gtmcrypt: $(thirdparty)_$(algo)


# install, uninstall and cleanup rules.
install:
	rm -f $(all_objfiles)
	mv *.so $(PLUGINDIR)
	ln -s ./$(install_targ) $(PLUGINDIR)/libgtmcrypt.so

uninstall:
	rm -f $(PLUGINDIR)/*.so
	rm -f $(PLUGINDIR)/gtmcrypt/maskpass

clean:
	rm -f $(all_objfiles)
	rm -f *.so
	rm -f maskpass
