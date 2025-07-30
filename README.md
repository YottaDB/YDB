# YottaDB

YottaDB<sup>Ⓡ</sup> is the fastest, highest consistency, database that scales to meet the needs of the very largest applications.

All software in this package is part of [YottaDB](https://yottadb.com) each
file of which identifies its copyright holders. The software is made available
to you under the terms of a license. Refer to the [COPYING](COPYING) and [LICENSE](LICENSE) files for details.

## Why use YottaDB?

YottaDB gives you uncompromising consistency, reliability and performance, even in large enterprise and nation-scale applications. Other databases force you to make compromises.

### The Best Data Consistency for Systems with Tens of Thousands of Concurrent Processes

The YottaDB database engine runs in-process, on one node and scales vertically. Its transactions have [completely automated retry logic](https://yottadb.com/bid-wars/) that is included out of the box and [optimistic concurrency control](http://daslab.seas.harvard.edu/reading-group/papers/kung.pdf) model for ACID  properties, even with tens of thousands of concurrent processes making tens of millions of database accesses, all on a single system.

### High performance at Massive Scale

Because YottaDB runs in-process and scales vertically, it delivers extreme performance that maximizes the throughput of the underlying hardware and operating system.

### Data Layer Flexibility Throughout the Application Lifecycle

Unlike most other databases, YottaDB's hierarchical data structure gives you flexibility in your data layer both at the time of the application initial creation as well as throughout the entire lifecycle. Permissions are implemented in the application layer and/or the operating system rather than in the database itself.

## Who Uses YottaDB?

YottaDB is used primarily, but not exclusively, in large scale healthcare and financial services applications where data consistency is critical and [support from YottaDB LLC](https://yottadb.com/product/services/) provides peace of mind. For example:

- [One of the largest banks in Thailand, with 60 million accounts uses YottaDB.](https://yottadb.com/resources/success-stories/government-savings-bank/) While most large banks use a batch system that is updated daily, they have a true real-time system that is able to handle tens of thousands of concurrent users with excellent performance.
- [A nation-scale electronic health record system in Jordan uses YottaDB.](https://yottadb.com/resources/success-stories/electronic-health-solutions/) It  uses an open source software stack including YottaDB on Linux for round-the-clock real-time access to medical records for patients and providers in a country of over 10 million people.

See [Success Stories](https://yottadb.com/resources/success-stories/) for more examples. You do not have to be a large enterprise or a nation-scale application to benefit from YottaDB, but YottaDB will not limit your growth.

## Start with Pre-built Distributions

The [Get Started page](https://yottadb.com/product/get-started/) gets you started with YottaDB, on your machine, on our virtual machine, or a Docker container. YottaDB provides pre-built binary distributions for current versions of popular Linux distributions ([Debian](https://www.debian.org/), [Red Hat](https://www.redhat.com/en), [SUSE](https://www.suse.com/), and [Ubuntu](https://ubuntu.com/), as well as selected derivatives), and the [ydbinstall.sh](https://download.yottadb.com/ydbinstall.sh) downloads and installs the current release for Supported platforms. Details are on the Get Started page.

Debian is Supported on both x86_64 and ARM64 CPUs.

## Resources

- [Website](https://yottadb.com)
- [Documentation](https://yottadb.com/resources/documentation/)
- [Source code](https://gitlab.com/YottaDB/DB/YDB)

## Build and Install YottaDB from Source

On current releases of popular Linux distributions which are Supportable but not Supported (such as [Arch Linux](https://archlinux.org/) and derivatives) the `--from-source` option of the [ydbinstall.sh](https://download.yottadb.com/ydbinstall.sh) script downloads, builds, and installs YottaDB. If you are a developer, you can also clone the repository to build and install YottaDB.

```sh
git clone https://gitlab.com/YottaDB/DB/YDB.git
```

YottaDB relies on [CMake](https://cmake.org/) to generate the Makefiles to build binaries from source.
Refer to the [Release Notes](https://gitlab.com/YottaDB/DB/YDB/-/releases) for each release for a list of the Supported platforms
in which we build and test YottaDB binary distributions.
At least CMake version 3 is required.

```sh
# Debian, Ubuntu and derivatives
sudo apt-get install --no-install-recommends cmake
# Red Hat and derivatives
sudo yum install cmake
# SUSE and derivatives
sudo zypper install cmake
```

Note: Both gcc and Clang/LLVM are supported on `x86_64`. To use Clang/LLVM you would need to
install the Clang/LLVM packages for your distribution in addition to the packages
listed below. For example for Ubuntu Linux:

```sh
sudo apt-get install --no-install-recommends clang llvm lld
```

- Install prerequisite packages

  ```sh
  Debian, Ubuntu, and derivatives
  sudo apt-get install --no-install-recommends file cmake make gcc git curl tcsh libjansson4 {libconfig,libelf,libicu,libncurses,libreadline,libjansson,libssl}-dev binutils ca-certificates

  Red Hat and derivatives
  sudo yum install file cmake make gcc git curl tcsh jansson {libconfig,libicu,ncurses,elfutils-libelf,readline,jansson,openssl}-devel binutils ca-certificates

  SUSE and derivatives
  sudo zypper install cmake make gcc git file curl tcsh binutils-gold icu libjansson4 {libconfig,libicu,ncurses,libelf,readline,libjansson,libopenssl}-devel binutils ca-certificates
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
  cmake ..
  export ydb_icu_version=$(pkg-config --modversion icu-io).suse  # needed only on SUSE and derivatives
  make -j $(getconf _NPROCESSORS_ONLN)
  make install
  cd yottadb_r*  # The latest release number will be seen in the directory name
  ```

  ### Build with Clang/LLVM
  ```sh
  export CC=/usr/bin/clang
  cmake -D CMAKE_LINKER:PATH=/usr/bin/ld.lld ..
  export ydb_icu_version=$(pkg-config --modversion icu-io).suse  # needed only on SUSE and derivatives
  make -j $(getconf _NPROCESSORS_ONLN)
  make install
  cd yottadb_r*  # The latest release number will be seen in the directory name
  ```

  Note that the ```make install``` command above does not create the final installed YottaDB.
  Instead, it stages YottaDB for distribution.

- Installing YottaDB

  Now you are ready to install YottaDB. The default installation path for each release includes the release
  (e.g. for YottaDB r2.00, the default installation path is /usr/local/lib/yottadb/r202),
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

The docker image is built using the generic `ydb_env_set` script that gives the user some sane defaults.

The commands below assume that you want to remove the docker container after running the command, which means that if you don't mount a volume that contains your database and routines they will be lost. If you want the container to persist remove the ```--rm``` parameter from the ```docker``` command.

Volumes are also supported by mounting to the ```/data``` directory. If you want to mount the local directory ```ydb-data``` into the container to save your database and routines locally and use them in the container in the future, add the following command line parameter before the yottadb/yottadb argument:

```
-v $PWD/ydb-data:/data
```

This creates a ydb-data directory in your current working directory. This can be deleted after the container is shutdown/removed if you want to remove all data created in the YottaDB container (such as your database and routines).

The [YottaDB GUI](https://gitlab.com/YottaDB/UI/YDBGUI) is available on port 9080; statistics for the GUI is running on port 9081.

### Pre-built Images

Pre-built images are available on [docker hub](https://hub.docker.com/r/yottadb/)

### Running a Pre-built Image

```
docker run --rm -it -p 9080-9081:9080-9081 download.yottadb.com/yottadb/yottadb # you can add a specific version after a ":" if desired
```

### Build & Run an Image

1. Build the image

   ```
   docker build -t yottadb/yottadb:latest-master .
   ```

1. Run the created image

   ```
   docker run --rm -it -p 9080-9081:9080-9081 yottadb/yottadb:latest-master
   ```

## Contribute to YottaDB Development

To contribute or help with further development, [fork the repository](https://docs.gitlab.com/ee/gitlab-basics/fork-project.html), clone your fork to a local copy, and submit Merge Requests! Please also set up the pre-commit and pre-rebase scripts to automatically enforce some coding conventions. Assuming you are in the top-level directory, the following will work:

```sh
ln -s ../../pre-commit .git/hooks
ln -s ../../pre-rebase .git/hooks
```

**YottaDB is a registered trademark of YottaDB LLC**
