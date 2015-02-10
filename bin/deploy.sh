#!/bin/bash
#
# Usage: deploy.sh PREFIX [LIBDIR]

UFO_URL=http://github.com/ufo-kit

ROOT=$(pwd)
PREFIX=$1
LIBDIR=$2

if [[ -z "$PREFIX" ]]; then
    PREFIX=$ROOT/usr
fi

if [[ -z "$LIBDIR" ]]; then
    LIBDIR=$PREFIX/lib
fi

export LD_LIBRARY_PATH=$LIBDIR
export PKG_CONFIG_PATH=$LD_LIBRARY_PATH/pkgconfig
export GI_TYPELIB_PATH=$LD_LIBRARY_PATH/girepository-1.0

function build_package() {
    printf "\n** Fetching $1\n"
    cd $ROOT

    if [ -d "$ROOT/$1" ]; then
        cd $ROOT/$1
        git pull
    else
        git clone $2
        cd $ROOT/$1
        printf "\n** Configuring $1\n"
        cmake . -DPREFIX=$PREFIX -DLIBDIR=$LIBDIR || exit 1
    fi

    printf "\n** Building $1\n"
    cd $ROOT/$1
    make || exit 1
    make install
}

build_package "ufo-core" http://github.com/ufo-kit/ufo-core
build_package "ufo-filters" http://github.com/ufo-kit/ufo-filters

# Link the typelib because the girepository framework is not searching
# /usr/local on openSUSE systems.
if [[ $EUID -eq 0 ]]; then
    TYPELIB_PATH=$(ls $GI_TYPELIB_PATH/Ufo-*.typelib)
    TYPELIB=$(basename $TYPELIB_PATH)
    ln -s $TYPELIB_PATH $LIBDIR/girepository-1.0/$TYPELIB
    ldconfig
fi
