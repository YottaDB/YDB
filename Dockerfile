# Build:
#   $ docker build -t yottadb:latest .
# Use with data persistence:
#   $ docker run --rm -e gtm_chset=utf-8 -v `pwd`/ydb-data:/data -ti yottadb:latest

# Stage 1: YottaDB build image
FROM ubuntu as ydb-release-builder

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y \
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
      zlib1g-dev

ADD . /tmp/yottadb-src
RUN mkdir -p /tmp/yottadb-build/package \
 && cd /tmp/yottadb-build \
 && test -f /tmp/yottadb-src/.yottadb.vsn || \
    grep YDB_RELEASE_NAME /tmp/yottadb-src/sr_*/release_name.h \
    | grep -o '\(r[0-9.]*\)' \
    | sort -u \
    > /tmp/yottadb-src/.yottadb.vsn \
 && cmake \
      -D CMAKE_INSTALL_PREFIX:PATH=/tmp \
      -D GTM_INSTALL_DIR:STRING=yottadb-release \
      /tmp/yottadb-src \
 && make \
 && make install \
 && icu-config --version \
      | sed \
          -e 's/\([0-9]\)\([0-9]\)\..*/\1.\2/g' \
          -e 's/\([0-9]\)\.\([0-9]*\).*/\1.\2/g' \
      > /tmp/yottadb-src/.icu.vsn \
 && cd /tmp/yottadb-release \
 && ./gtminstall \
      --utf8 `cat /tmp/yottadb-src/.icu.vsn` \
      --installdir /opt/yottadb/`cat /tmp/yottadb-src/.yottadb.vsn`-`uname -p` \
 && cd /opt/yottadb \
 && ln -s `cat /tmp/yottadb-src/.yottadb.vsn`-`uname -p` current

# Stage 2: YottaDB release image
FROM ubuntu as ydb-release

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y libelf-dev libicu-dev
RUN locale-gen en_US.UTF-8
WORKDIR /data
COPY --from=ydb-release-builder /opt/yottadb /opt/yottadb
ENV gtmdir=/data \
    LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8  
CMD ["/opt/yottadb/current/gtm"]

