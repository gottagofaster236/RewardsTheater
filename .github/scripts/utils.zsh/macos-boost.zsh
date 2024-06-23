#!/bin/sh
# Based on https://stackoverflow.com/a/69924892/6120487

rm -rf arm64 x86_64 universal stage bin.v2
rm -f b2 project-config*
./bootstrap.sh cxxflags="-arch x86_64 -arch arm64" cflags="-arch x86_64 -arch arm64" linkflags="-arch x86_64 -arch arm64" link=static --with-libraries=system,url,json
./b2 toolset=clang-darwin target-os=darwin architecture=arm abi=aapcs cxxflags="-arch arm64" cflags="-arch arm64" linkflags="-arch arm64" -a
mkdir -p arm64 && cp stage/lib/*.a arm64
./b2 toolset=clang-darwin target-os=darwin architecture=x86 cxxflags="-arch x86_64" cflags="-arch x86_64" linkflags="-arch x86_64" abi=sysv binary-format=mach-o -a
mkdir x86_64 && cp stage/lib/*.a x86_64
mkdir universal
for lib in arm64/*; do
lipo -create -arch arm64 $lib -arch x86_64 x86_64/$(basename $lib) -output universal/$(basename $lib);
done
for lib in universal/*; do
lipo $lib -info;
done
