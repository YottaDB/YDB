# YottaDB

All software in this package is part of YottaDB (http://yottadb.com) each
file of which identifies its copyright holders. The software is made available
to you under the terms of a license. Refer to the [LICENSE](LICENSE) file for details.

YottaDB relies on CMake to generate the Makefiles to build binaries from source.
The prerequisites are CMake (at least 2.8.5), GNU make (at least 3.81), Linux
(`x86_64`), libraries and development files for libz, Unicode, OpenSSL and GPG.
Ubuntu 16.04 LTS was used to test the builds for this distribution, with default
versions of packages from the distribution repositories.

## How to build

1. Fulfill the pre-requisites

   Install developement libraries

   ```sh
    Ubuntu Linux OR Raspbian Linux OR Beagleboard Debian
	sudo aptitude install cmake tcsh {libconfig,libelf,libgcrypt,libgpg-error,libgpgme11,libicu,libncurses,libssl,zlib1g}-dev

    Arch Linux
	sudo pacman -S cmake tcsh {libconfig,libelf,libgcrypt,libgpg-error,gpgme,icu,ncurses,openssl,zlib}

    CentOS Linux OR RedHat Linux
	sudo yum install git gcc cmake tcsh {libconfig,gpgme,libicu,libgpg-error,libgcrypt,ncurses,openssl,zlib,elfutils-libelf}-devel
   ```

   There may be other library dependencies or the packages may have different names.
   If CMake issues a NOTFOUND error, please see the FAQ below.

2. Unpack the YottaDB sources

   The YottaDB source tarball extracts to a directory with the version number in the name, i.e. ```yottadb_r120```

   ```sh
   $ tar xfz yottadb_r120_src.tar.gz
   $ cd yottadb_r120_src
   ```

   You should find this README, LICENSE, COPYING and CMakeLists.txt file and sr_* source directories.

3. Build the YottaDB binaries

   ```sh
   $ mkdir build
   $ cd build
   ```

   > By default the script creates production (pro) builds of YottaDB. To create
   > a debug (dbg) build of YottaDB supply the following parameter to cmake
   >     ```-D CMAKE_BUILD_TYPE=Debug```	*Note: title case is important*
   >

   ```sh
   $ cmake -D CMAKE_INSTALL_PREFIX:PATH=$PWD ../
   $ make -j `grep -c ^processor /proc/cpuinfo`
   $ make install
   $ cd yottadb_r120
   ```

  Note that the make install done above does not create the final installed YottaDB.
  Instead, it stages YottaDB for distribution.

  Now you are ready to install YottaDB. The default installation path is ```/usr/local/lib/yottadb/r120```
  but can be controlled using the --installdir option. Run ```./ydbinstall --help``` for a list of options.

  ```sh
  $ sudo ./ydbinstall
  $ cd - ; make clean
  ```

4. Packaging YottaDB

   Create a tar file from the installed directory

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
	CMake Error: The following variables are used in this project, but they are set to NOTFOUND.
	Please set them or make sure they are set and tested correctly in the CMake files:
  This indicates that required libraries are not found. Please consult the list
  of libraries and check your distributions package manager.

