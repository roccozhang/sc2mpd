#!/bin/sh

# Helper script for building the openhome libs prior to building
# scmpdcli.  This is far from foolproof or knowledgeable, but it seems
# to get me usable (static) libs. We first clone the git repositories,
# checkout a known ok version (it's more or less random, based on the
# 1st date I tried this, but some further versions don't build), then
# build th edifferent dirs.
# You should create a top empty dir first, then "sh ohbuild.sh mytopdir"

fatal()
{
    echo $*; exit 1
}
usage()
{
    fatal "Usage: ohbuild.sh <topdir>"
}
test $# = 1 || usage
topdir=$1

cd $topdir || exit 1
topdir=`pwd`


otherfiles=`echo *.*`
test "$otherfiles"  != '*.*' && fatal not in top dir

arch=
debug=

clone_oh()
{
    cd $topdir
    for rep in \
        https://github.com/openhome/ohNet.git \
        https://github.com/openhome/ohdevtools.git \
        https://github.com/openhome/ohNetGenerated.git \
        https://github.com/openhome/ohTopology.git \
        https://github.com/openhome/ohSongcast.git \
        ; do
        dir=`echo $rep | sed -e 's+https://github.com/openhome/++' \
            -e 's/\.git$//'`
        echo $dir
        test ! -d $dir && git clone $rep
    done
}

build_ohNet()
{
    cd $topdir
    dir=ohNet
    echo building $dir
    cd  $dir
    # Apr 29 2015
    git checkout 5118af2acce9a25123336b9d3526cc87cdb96386
    make ohNetCore proxies devices TestFramework bundle || exit 1

    cd ..
}

build_ohNetGenerated()
{
    dir=ohNetGenerated
    echo building $dir
    cd  $dir
    # Apr 29 2015
    git checkout 8deb7ffd2f8e99fdc56449dadcd7d4fbc96cb829
    # e.g. Linux-x64, Linux-armhf
    arch=`basename $topdir/ohNet/Build/Bundles/ohNet-*-*.tar.gz | \
        sed -e s/ohNet-//  -e s/-[A-Z][a-z][a-z]*\.tar\.gz$//`
    # e.g. Debug, Release
    debug=`basename $topdir/ohNet/Build/Bundles/ohNet-*-*.tar.gz | \
        sed -e s/.*-//  -e s/\.tar\.gz$//`

    sd=dependencies/${arch}
    mkdir -p "$sd"
    (cd $sd;
        tar xvzf $topdir/ohNet/Build/Bundles/ohNet-${arch}-${debug}.tar.gz
    ) || exit 1

    # This should fail looking for some dll (csharp again), but that's ok
    make

    # Copy the includes from here to the ohNet dir where ohTopology
    # will want them
    tar cf - Build/Include | (cd $topdir/ohNet/;tar xvf -) || exit 1

    cd ..
}

build_ohdevtools()
{
    cd $topdir
    dir=ohdevtools
    echo building $dir
    cd  $dir
    git checkout 52445477e4201b35b4ee5174bf90d25543cd1849
    # Nothing to build
    cd ..
}


build_ohTopology()
{
    cd $topdir
    dir=ohTopology
    echo building $dir
    cd  $dir
    # Apr 29 2015
    git checkout 18f004621a7b0dc3add6ddfeec781bd3878ae42e
	
    ./go fetch --all --clean 

    ./waf configure --ohnet=../ohNet --dest-platform=Linux-x86
    # This fails, but it creates what we need.
    ./waf build

    cd ..
}

build_ohSongcast()
{
    cd $topdir
    dir=ohSongcast
    echo building $dir
    cd  $dir

    # Apr 29 2015.
    git checkout d3f6017a1ef133525f75f4d78e399e6fd6b70ce6

    # This (used to?) fails because the link options are wrong
    # (-lpthread should be at the end of the link line), but it builds
    # the objs we need.
    make release=1 Receiver
}

official_way()
{
# from README, actually Does not work, for reference
    cd ohNet
    make ohNetCore proxies devices TestFramework
    cd ../ohNetGenerated
    make
    cd ../ohNetmon
    make
    cd ../ohTopology
    ./waf configure --ohnet=../ohNet --debug --dest-platform=[Windows-x86|Mac-x86|Linux-x86]
    ./waf build
    cd ../ohSongcast
    make release=1
}

clone_oh

build_ohNet
build_ohNetGenerated
build_ohdevtools
build_ohTopology
build_ohSongcast
