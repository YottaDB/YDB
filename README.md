# YottaDB

All software in this package is part of YottaDB (https://yottadb.com) each
file of which identifies its copyright holders. The software is made available
to you under the terms of a license. Refer to the [LICENSE](LICENSE) file for details.

Homepage: https://gitlab.com/YottaDB/DB/YDB

Documentation: https://yottadb.com/resources/documentation/

## Cloning the repository for the latest updates

You may want to clone the YottaDB repository for access to the latest code.

```sh
git clone git@gitlab.com:YottaDB/DB/YDB.git
```

To contribute or help with further development, [fork the repository](https://docs.gitlab.com/ee/gitlab-basics/fork-project.html), clone your fork to a local copy and begin contributing!

## Install pre-built YottaDB binaries

To quickly get started with running YottaDB, follow the instructions on our [Get Started](https://yottadb.com/product/get-started/) page.

## Build and Install YottaDB from source

YottaDB relies on CMake to generate the Makefiles to build binaries from source.
Refer to the Release Notes for each release for a list of the Supported platforms
in which we build and test YottaDB binary distributions.

- Install prerequisite packages

  ```sh
  Ubuntu Linux OR Raspbian Linux OR Beagleboard Debian
  sudo apt-get install cmake tcsh {libconfig,libelf,libgcrypt,libgpg-error,libgpgme11,libicu,libncurses,libssl,zlib1g}-dev binutils

  Arch Linux
  sudo pacman -S cmake tcsh {libconfig,libelf,libgcrypt,libgpg-error,gpgme,icu,ncurses,openssl,zlib} binutils

  CentOS Linux OR RedHat Linux
  sudo yum install git gcc cmake tcsh {libconfig,gpgme,libicu,libgpg-error,libgcrypt,ncurses,openssl,zlib,elfutils-libelf}-devel binutils
  ```

  There may be other library dependencies or the packages may have different names.

- Building YottaDB from source tarball

  The YottaDB source tarball extracts to a directory with the version number in the name, e.g. ```yottadb_r123```

  ```sh
  tar xzf yottadb_r123_src.tar.gz
  cd yottadb_r123_src
  ```

  You should find this README, LICENSE, COPYING and CMakeLists.txt file and sr\_\* directories.

  Build the YottaDB binaries:

  ```sh
  mkdir build
  cd build
  ```

  Note: By default the script creates production (pro) builds of YottaDB. To create
  a debug (dbg) build of YottaDB supply the following parameter to cmake
      ```-D CMAKE_BUILD\_TYPE=Debug```
  (*Note: title case is important*)

  ```sh
  cmake -D CMAKE_INSTALL_PREFIX:PATH=$PWD ../
  make -j `grep -c ^processor /proc/cpuinfo`
  make install
  cd yottadb_r123
  ```

  Note that the ```make install``` command above does not create the final installed YottaDB.
  Instead, it stages YottaDB for distribution.
  If cmake or make issues an error in the above steps, please see the [FAQ](#faq) below.

- Installing YottaDB

  Now you are ready to install YottaDB. The default installation path for each release includes the release
  (e.g. for YottaDB r1.24, the default installation path is /usr/local/lib/yottadb/r124),
  but can be controlled using the ```--installdir``` option. Run ```./ydbinstall --help``` for a list of options.

  ```sh
  sudo ./ydbinstall
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
-v `pwd`/ydb-data:/data
```

This creates a ydb-data directory in your current working directory. This can be deleted after the container is shutdown/removed if you want to remove all data created in the YottaDB container (such as your database and routines).

### Pre-built images

Pre-built images are available on [docker hub](https://hub.docker.com/r/yottadb/)

### Running a Pre-built image

```
docker run --rm -it yottadb/yottadb # you can add a specific version after a ":" if desired
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

- YottaDB installation fails on Ubuntu 18.10

  Example error message that would be printed to the screen:

  ```
  %YDB-E-DLLNOOPEN, Failed to load external dynamic library /usr/local/lib/yottadb/r122/libyottadb.so
  %YDB-E-TEXT, libtinfo.so.5: cannot open shared object file: No such file or directory
  ```

  Ubuntu 18.10's default version of libtinfo6 is not backwards compatible with libtinfo5. To resolve the issue, libtinfo5 can be installed via the following command:

  ```bash
  sudo apt-get install libtinfo5
  ```
- YottaDB compilation fails with plugin needed to handle lto object

  There is a [known issue](https://sourceware.org/git/gitweb.cgi?p=binutils-gdb.git;a=commit;h=103da91bc083f94769e3758175a96d06cef1f8fe) with binutils on Ubuntu 18.10 (unknown if this affects other Linux Distributions) that may cause ar and ranlib to generate the following error messages:

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

