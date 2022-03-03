# syntax=docker/dockerfile:1.2
#################################################################
#								#
# Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	#
# All rights reserved.						#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################
# See README.md for more information about this Dockerfile
# Simple build/running directions are below:
#
# Build:
#   $ docker build -t yottadb/yottadb:latest .
#
# Use with data persistence:
#   $ docker run --rm -e ydb_chset=utf-8 -v `pwd`/ydb-data:/data -ti yottadb/yottadb:latest

ARG OS_VSN=20.04

# Stage 1: YottaDB build image
FROM ubuntu:${OS_VSN} as ydb-release-builder

ARG CMAKE_BUILD_TYPE=Release
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
                    file \
                    cmake \
                    make \
                    gcc \
                    git \
                    tcsh \
                    libconfig-dev \
                    libelf-dev \
                    libicu-dev \
                    libncurses-dev \
                    && \
    apt-get clean

RUN mkdir /tmp/yottadb-src
ADD CMakeLists.txt /tmp/yottadb-src
# We want to copy the directories themselves, not their contents.
# Unfortunately, there is no way to do this with globs, so we have to specify each directory individually.
# c.f. <https://docs.docker.com/engine/reference/builder/#add>, <https://stackoverflow.com/questions/26504846>

# This list was generated with `ls sr* -d | tr ' ' '\n' | xargs -n1 -i printf 'ADD %s /tmp/yottadb-src/%s\n' {} {}`.
ADD cmake /tmp/yottadb-src/cmake
ADD sr_aarch64 /tmp/yottadb-src/sr_aarch64
ADD sr_armv7l /tmp/yottadb-src/sr_armv7l
ADD sr_i386 /tmp/yottadb-src/sr_i386
ADD sr_linux /tmp/yottadb-src/sr_linux
ADD sr_port /tmp/yottadb-src/sr_port
ADD sr_port_cm /tmp/yottadb-src/sr_port_cm
ADD sr_unix /tmp/yottadb-src/sr_unix
ADD sr_unix_cm /tmp/yottadb-src/sr_unix_cm
ADD sr_unix_gnp /tmp/yottadb-src/sr_unix_gnp
ADD sr_unix_nsb /tmp/yottadb-src/sr_unix_nsb
ADD sr_x86_64 /tmp/yottadb-src/sr_x86_64
ADD sr_x86_regs /tmp/yottadb-src/sr_x86_regs
ADD .git /tmp/yottadb-src/.git

ENV GIT_DIR=/tmp/yottadb-src/.git
RUN mkdir -p /tmp/yottadb-build \
 && cd /tmp/yottadb-build \
 && test -f /tmp/yottadb-src/.yottadb.vsn || \
    grep YDB_ZYRELEASE /tmp/yottadb-src/sr_*/release_name.h \
    | grep -o '\(r[0-9.]*\)' \
    | sort -u \
    > /tmp/yottadb-src/.yottadb.vsn \
 && cmake \
      -D CMAKE_INSTALL_PREFIX:PATH=/tmp \
      -D YDB_INSTALL_DIR:STRING=yottadb-release \
      -D CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} \
      /tmp/yottadb-src \
 && make -j $(nproc) \
 && make install

# Stage 2: YottaDB release image
FROM ubuntu:${OS_VSN} as ydb-release

ARG DEBIAN_FRONTEND=noninteractive
RUN --mount=type=bind,from=ydb-release-builder,source=/tmp/yottadb-release,target=/tmp/staging \
    # This is a strange step: The mount volume is readonly; and we actually write to it in ydbinstall
    # So we need to copy the mount contents to a seperate folder
    cp -R /tmp/staging /tmp/ydb-release && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
                    file \
                    binutils \
                    libelf-dev \
                    libicu66 \
		    nano \
		    wget \
		    && \
    /tmp/ydb-release/ydbinstall --utf8 default --installdir /opt/yottadb/current && \
    apt-get remove -y wget && \
    apt-get autoclean -y && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    rm -rf /tmp/ydb-release

WORKDIR /data
ENV gtmdir=/data \
    LC_ALL=C.UTF-8 \
    EDITOR=/usr/bin/nano
ENTRYPOINT ["/opt/yottadb/current/ydb"]
