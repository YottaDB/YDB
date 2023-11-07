# YottaDB

All software in this package is part of YottaDB (https://yottadb.com) each
file of which identifies its copyright holders. The software is made available
to you under the terms of a license. Refer to the [LICENSE](LICENSE) file for details.

Homepage: https://gitlab.com/YottaDB/DB/YDB

Documentation: https://yottadb.com/resources/documentation/

## Cloning the repository for the latest updates

You may want to clone the YottaDB repository for access to the latest code.

```sh
git clone https://gitlab.com/YottaDB/DB/YDB.git
```

To contribute or help with further development, [fork the repository](https://docs.gitlab.com/ee/gitlab-basics/fork-project.html), clone your fork to a local copy and begin contributing! Please also set up the pre-commit script to automatically enforce some coding conventions. Assuming you are in the top-level directory, the following will work:

```sh
ln -s ../../pre-commit .git/hooks
```

## Install pre-built YottaDB binaries

To quickly get started with running YottaDB, follow the instructions on our [Get Started](https://yottadb.com/product/get-started/) page.

## Build and Install YottaDB from source

YottaDB relies on CMake to generate the Makefiles to build binaries from source.
Refer to the Release Notes for each release for a list of the Supported platforms
in which we build and test YottaDB binary distributions.
At least cmake version 3 is required.

```
# Ubuntu or Debian-like distro
sudo apt-get install --no-install-recommends cmake
# CentOS
sudo yum install cmake3
```

On CentOS it will be installed as `cmake3` instead of cmake,
so use `cmake3` on CentOS wherever `cmake` is referenced below.

Note: Both gcc and Clang/LLVM are supported on `x86_64`. To use Clang/LLVM you would need to
install the Clang/LLVM packages for your distribution in addition to the packages
listed below. For example for Ubuntu Linux:

```sh
 sudo apt-get install --no-install-recommends clang llvm lld
 ```

- Install prerequisite packages

  ```sh
  Ubuntu Linux OR Raspbian Linux OR Beagleboard Debian
  sudo apt-get install --no-install-recommends file cmake make gcc git curl tcsh {libconfig,libelf,libicu,libncurses,libreadline}-dev binutils ca-certificates

  Arch Linux
  sudo pacman -S file cmake make gcc git curl tcsh {libconfig,libelf,icu,ncurses,readline} binutils ca-certificates

  CentOS Linux OR RedHat Linux
  sudo yum install file cmake make gcc git curl tcsh {libconfig,libicu,ncurses,elfutils-libelf,readline}-devel binutils ca-certificates

  SUSE (SLES or SLED) or OpenSUSE Leap or OpenSUSE Tumbleweed
  sudo zypper install cmake make gcc git file curl tcsh binutils-gold icu {libconfig,libicu,ncurses,libelf,readline}-devel binutils ca-certificates
  ```

  There may be other library dependencies or the packages may have different names.

- Fetch the latest released version of YottaDB source

  To obtain the source code corresponding to the latest YottaDB release and build binaries from that source please use the following set of shell commands which fetches the latest tagged release by performing a git clone. After cloning YottaDB source files can be seen in the directory named `YDB`.

  ```sh
  ydb_distrib="https://gitlab.com/api/v4/projects/7957109/repository/tags"
  ydb_tmpdir='tmpdir'
  mkdir $ydb_tmpdir
  wget -P $ydb_tmpdir ${ydb_distrib} 2>&1 1>${ydb_tmpdir}/wget_latest.log
  ydb_version=`sed 's/,/\n/g' ${ydb_tmpdir}/tags | grep -E "tag_name|.pro.tgz" | grep -B 1 ".pro.tgz" | grep "tag_name" | sort -r | head -1 | cut -d'"' -f6`
  git clone --depth 1 --branch $ydb_version https://gitlab.com/YottaDB/DB/YDB.git
  cd YDB
  ```

  You should find this README, LICENSE, COPYING and CMakeLists.txt file and `sr_*` directories.

  Build the YottaDB binaries:

  ```sh
  mkdir build
  cd build
  ```

  Note: By default the script creates production (pro) builds of YottaDB. To create
  a debug (dbg) build of YottaDB supply the `-D CMAKE_BUILD_TYPE=Debug` parameter to cmake
  (*Note: title case is important*)

  ### Build with gcc
  ```sh
  cmake -D CMAKE_INSTALL_PREFIX:PATH=$PWD ../
  export ydb_icu_version=65.1.suse # this is needed only on OpenSUSE Leap 15.4 or SLES 15.4 or SLED 15.4
  make -j `grep -c ^processor /proc/cpuinfo`
  make install	# For known errors in this step and how to work around them, consult the FAQ section below
  cd yottadb_r*  # The latest release number will be seen in the directory name
  ```

  ### Build with Clang/LLVM
  ```sh
  export CC=/usr/bin/clang
  cmake -D CMAKE_LINKER:PATH=/usr/bin/ld.lld -D CMAKE_INSTALL_PREFIX:PATH=$PWD ../
  export ydb_icu_version=65.1.suse # this is needed only on OpenSUSE Leap 15.4 or SLES 15.4 or SLED 15.4
  make -j `grep -c ^processor /proc/cpuinfo`
  make install	# For known errors in this step and how to work around them, consult the FAQ section below
  cd yottadb_r*  # The latest release number will be seen in the directory name
  ```

  Note that the ```make install``` command above does not create the final installed YottaDB.
  Instead, it stages YottaDB for distribution.
  If cmake or make issues an error in the above steps, please see the [FAQ](#faq) below.

- Installing YottaDB

  Now you are ready to install YottaDB. The default installation path for each release includes the release
  (e.g. for YottaDB r1.38, the default installation path is /usr/local/lib/yottadb/r138),
  but can be controlled using the ```--installdir``` option. Run ```./ydbinstall --help``` for a list of options.

  Note that if the ```ydb_icu_version``` env var is set to a value other than what `pkg-config --modversion icu-io`
  would return (observed on a SLED 15 or openSUSE Leap system), then the env var value needs to be preserved across
  the sudo call hence the use of ```preserve-env``` below. It is not needed on other systems but does not hurt either.

  ```sh
  sudo --preserve-env=ydb_icu_version ./ydbinstall
  cd - ; make clean
  ```

## Docker

### Pre-requisites

A working [Docker](https://www.docker.com/community-edition#/download) installation on the platform of choice.

NOTE: You must have at least docker 17.05 as [multi-stage](https://docs.docker.com/v17.09/engine/userguide/eng-image/multistage-build/) builds are used within the docker file

### Image information

The docker image is built using the generic ```ydb``` script that gives the user some sane defaults to begin exploring YottaDB. This isn't meant for production usage.

The commands below assume that you want to remove the docker container after running the command, which means that if you don't mount a volume that contains your database and routines they will be lost. If you want the container to persist remove the ```--rm``` parameter from the ```docker``` command.

Volumes are also supported by mounting to the ```/data``` directory. If you want to mount the local directory ```ydb-data``` into the container to save your database and routines locally and use them in the container in the future, add the following command line parameter before the yottadb/yottadb argument:

```
-v $PWD/ydb-data:/data
```

This creates a ydb-data directory in your current working directory. This can be deleted after the container is shutdown/removed if you want to remove all data created in the YottaDB container (such as your database and routines).

### Pre-built images

Pre-built images are available on [docker hub](https://hub.docker.com/r/yottadb/)

### Running a Pre-built image

```
docker run --rm -it download.yottadb.com/yottadb/yottadb # you can add a specific version after a ":" if desired
```

### Build Steps

1) Build the image
   ```
   docker build -t yottadb/yottadb:latest .
   ```
2) Run the created image
   ```
   docker run --rm -it yottadb/yottadb:latest
   ```

## FAQ

- The CMake build fails with the following message followed by one or more cases.

  ```
  CMake Error: The following variables are used in this project, but they are set to NOTFOUND. Please set them or make sure they are set and tested correctly in the CMake files
  ```

  This indicates that required libraries are not found. Please consult the list of libraries and check your distributions package manager.

- YottaDB installation fails with %YDB-E-DLLNOOPEN

  Example error message that would be printed to the screen:

  ```
  %YDB-E-DLLNOOPEN, Failed to load external dynamic library /usr/local/lib/yottadb/r136/libyottadb.so
  %YDB-E-TEXT, libtinfo.so.5: cannot open shared object file: No such file or directory
  ```

  This indicates that the libtinfo5 package isn't installed and libtinfo6 is not backwards compatible with libtinfo5. This has been observed on Ubutntu 18.10 and could possibly apply to other Linux distributions as well. To resolve the issue, libtinfo5 can be installed via the following command:

  ```bash
  sudo apt-get install --no-install-recommends libtinfo5
  ```
- YottaDB compilation fails with plugin needed to handle lto object

  There is a [known issue](https://sourceware.org/git/gitweb.cgi?p=binutils-gdb.git;a=commit;h=103da91bc083f94769e3758175a96d06cef1f8fe) with binutils and has been observed on Ubuntu 18.10 and could possibly apply to other Linux distributions including debian unstable that may cause ar and ranlib to generate the following error messages:

  ```
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zshow_locks.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zshow_output.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zshow_stack.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zshow_svn.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zshow_zbreaks.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zshow_zwrite.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/ztrap_save_ctxt.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zwr2format.c.o: plugin needed to handle lto object
  /usr/bin/ar: CMakeFiles/libmumps.dir/sr_port/zyerror_init.c.o: plugin needed to handle lto object
  /usr/bin/ranlib: libmumps.a(f_zwrite.c.o): plugin needed to handle lto object
  /usr/bin/ranlib: libmumps.a(fgn_glopref.c.o): plugin needed to handle lto object
  /usr/bin/ranlib: libmumps.a(fgncal_unwind.c.o): plugin needed to handle lto object
  /usr/bin/ranlib: libmumps.a(find_line_addr.c.o): plugin needed to handle lto object
  /usr/bin/ranlib: libmumps.a(find_line_start.c.o): plugin needed to handle lto object
  /usr/bin/ranlib: libmumps.a(find_mvstent.c.o): plugin needed to handle lto object
  ```

  The work around is to bump the open file descriptors limit to 4096 or higher

  bash/sh
  ```bash
  ulimit -n 4096
  ```
  OR

  tcsh
  ```tcsh
  limit openfiles 4096
  ```
