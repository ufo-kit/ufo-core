#!/bin/bash
#
# Usage: deploy.sh PREFIX LIBSUFFIX

ZEROMQ_BASE=zeromq-3.2.2
ZEROMQ_TARBALL=$ZEROMQ_BASE.tar.gz
ZEROMQ_URL=http://download.zeromq.org/$ZEROMQ_TARBALL
UFO_URL=http://ufo.kit.edu/git

ROOT=$(pwd)
PREFIX=$1
LIBSUFFIX=$2

export LD_LIBRARY_PATH=$PREFIX/lib$LIBSUFFIX
export PKG_CONFIG_PATH=$LD_LIBRARY_PATH/pkgconfig
export GI_TYPELIB_PATH=$LD_LIBRARY_PATH/girepository-1.0

function update() {
    cd $ROOT/$1
    git pull
    make || exit 1
}

function build_package() {
    pkg-config --cflags --libs libzmq
    printf "\n** Fetching $1\n"
    cd $ROOT

    if [ -d "$ROOT/$1" ]; then
        cd $ROOT/$1
        git pull
    else
        git clone $UFO_URL/$1
        cd $ROOT/$1
    fi

    printf "\n** Building $1\n"
    cd $ROOT/$1
    cmake . -DCMAKE_INSTALL_PREFIX=$PREFIX -DLIB_SUFFIX=$LIBSUFFIX || exit 1
    make || exit 1
    make install
}

function install_package() {
    cd $ROOT/$1
    make install
}

function build() {
    if [ ! -f $ROOT/$ZEROMQ_TARBALL ]; then
        printf "\n** Fetching ZeroMQ\n"
        wget $ZEROMQ_URL || die "ERROR: Could not fetch ZeroMQ"
    fi

    printf "\n** Building ZeroMQ\n"
    tar xfz $ZEROMQ_TARBALL
    cd $ROOT/$ZEROMQ_BASE
    ./configure --prefix=$PREFIX --libdir=$LD_LIBRARY_PATH
    make -j 4

    build_package "ufo-core"
    build_package "oclfft"
    build_package "ufo-filters"

    # Link the typelib because the girepository framework is not searching
    # /usr/local on openSUSE systems.
    if [[ $EUID -eq 0 ]]; then
        TYPELIB_PATH=$(ls $GI_TYPELIB_PATH/Ufo-*.typelib)
        TYPELIB=$(basename $TYPELIB_PATH)
        ln -s $TYPELIB_PATH /usr/lib$LIBSUFFIX/girepository-1.0/$TYPELIB
        ldconfig
    fi
}

if [ "$PREFIX" == "" ]; then
    PREFIX=$ROOT/usr
    printf "** Prefix not set, installing into $PREFIX\n"
fi

if [ "$PREFIX" == "update" ]; then
    update "ufo-core"
    update "ufo-filters"
elif [ "$PREFIX" == "install" ]; then
    install_package "$ZEROMQ_BASE"
    install_package "ufo-core"
    install_package "oclfft"
    install_package "ufo-filters"
else
    build
fi
