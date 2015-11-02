All software in this package is part of FIS GT.M (http://fis-gtm.com) 
which is Copyright 2012 Fidelity Information Services, Inc., and
provided to you under the terms of a license. If there is a COPYING 
file included in this package, it contains the terms of the license under
which the package is provided to you. If there is not a COPYING file in
the package, you must ensure that your use of FIS GT.M complies with the
license under which it is provided. If you are unsure as to the terms of
your license, please consult with the entity that provided you with the
package.

GT.M relies on CMake to generate the Makefiles to build GT.M from source. The
prerequisites are CMake (at least 2.8.5), GNU make (at least 3.81), Linux
(either x86 or x86_64), Unicode include files and GPG. Unicode include files
are automatically installed if ICU is installed. GPG include files require
installing the GNUPG and related library development packages. Debian 6, Ubuntu
12.04 LTS and RHEL 6.0 were used to do the test builds for this distribution.
The default ICU and GPG packages were taken from the distribution repositories.

To build GT.M for Linux, do the following steps:

1. Fulfill the pre-requisites
   Install developement libraries libelf, zlib, libicu, libgpgme, libgpg-error,
   libgcrypt.

   [optional] The GT.M source tarball includes pre-generated files. To generate
   these files requires a binary distribution of GT.M.  You can download GT.M
   from http://sourceforge.net/projects/fis-gtm/ Unpack the tar file and run
   the configure script as root. Note: the tar file unpacks everything into
   your current working directory, not a new subdirectory. The Linux Standard
   Base (LSB) install path for GT.M V60000 is /opt/lsb-gtm/V6.0-000_i686 or
   /opt/lsb-gtm/V6.0-000_x8664.
   $ tar xfz gtm_V60000_linux_i686_pro.tar.gz
   $ sudo sh ./configure

   # Provide the directory path to cmake using
   #   -D GTM_DIST:PATH=$gtm_dist

2. Unpack the GT.M sources
   Change directory in the directory that you will place the GT.M source,
   here after referred to as <gtm-directory>.
   $ mkdir <gtm-directory>
   $ cd <gtm-directory>
   $ tar xfz gtm_V60000_linux_i686_src.tar.gz

   You should find this README, COPYING and CMakeLitst.txt file and sr_* source
   directories.

3. Building GT.M -
   <gtm-builddir> can be a sub directory of the source directory <gtm-directory>

   $ mkdir <gtm-builddir>
   $ cd <gtm-builddir>
   $ cmake <gtm-directory>


   # By default the build produces release versions of GT.M. To build a debug
   # version of GT.M supply the following parameter to cmake
   #     -D CMAKE_BUILD_TYPE=DEBUG
   #
   # Note that the default install location is driven by CMAKE_INSTALL_PREFIX.
   # You can change this when executing cmake
   #     -D CMAKE_INSTALL_PREFIX:PATH=/opt/lsb-gtm
   #
   $ make

   $ make install

   $ make clean

4. Packaging GT.M -
   Create a tar file from the installed directory
