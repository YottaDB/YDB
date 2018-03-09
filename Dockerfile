# See README.md for more information about this Dockerfile
# Simple build/running directions are below:
#
# Build:
#   $ docker build -t yottadb/yottadb:latest .
#
# Use with data persistence:
#   $ docker run --rm -e gtm_chset=utf-8 -v `pwd`/ydb-data:/data -ti yottadb/yottadb:latest

# Stage 1: YottaDB build image
FROM ubuntu as ydb-release-builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y \
      file \
      cmake \
      tcsh \
      libconfig-dev \
      libelf-dev \
      libgcrypt-dev \
      libgpg-error-dev \
      libgpgme11-dev \
      libicu-dev \
      libncurses-dev \
      libssl-dev \
      zlib1g-dev \
 && apt-get clean

ADD . /tmp/yottadb-src
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
      /tmp/yottadb-src \
 && make -j `grep -c ^processor /proc/cpuinfo` \
 && make install

# Stage 2: YottaDB release image
FROM ubuntu as ydb-release

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y file binutils libelf-dev libicu-dev locales \
 && apt-get clean
RUN locale-gen en_US.UTF-8
WORKDIR /data
COPY --from=ydb-release-builder /tmp/yottadb-release /tmp/yottadb-release
COPY --from=ydb-release-builder /tmp/yottadb-src/.yottadb.vsn /tmp/yottadb-release/
RUN cd /tmp/yottadb-release  \
 && icu-config --version \
      > /tmp/yottadb-release/.icu.vsn \
 && ./ydbinstall \
      --utf8 `cat /tmp/yottadb-release/.icu.vsn` \
      --installdir /opt/yottadb/`cat /tmp/yottadb-release/.yottadb.vsn`-`uname -p` \
 && cd /opt/yottadb \
 && ln -s `cat /tmp/yottadb-release/.yottadb.vsn`-`uname -p` current \
 && rm -rf /tmp/yottadb-release
ENV gtmdir=/data \
    LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8
ENTRYPOINT ["/opt/yottadb/current/ydb"]
