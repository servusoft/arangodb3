#Cross-Compiling ArangoDB

__Problem__

To use ArangoDB on their ARM Board, but compiling on their x64 computer.<br>

__Solution__

For solving the problem requires the following steps:<br>

1. Establishment of a cross-compiler<br> 
2. Preparation of the environment variables<br> 
3. Compilation of SSL<br> 
4. Compilation the ArangoDB<br> 
5. Preparation for testing on an ARM board<br>

##1. Establishment of a cross-compiler

There are two ways to use a cross-compiler

- As an installation in a Linux system (Debian or Ubuntu)
- Without installation, only the unpacked in a directory (binary) version

For use as an installation the following commands (as root in terminal) is required:

_apt-get install g++-arm-linux-gnueabihf_ - for ARMv7 (32-bit) architectures<br>
_apt-get install g++-aarch64-linux-gnu_ - for ARMv8 (64-bit) architectures<br>

Without installing the following packages should be downloaded:<br>
- For ARMv7 arm-linux-gnueabihf compiler is used, which can be downloaded at the link:<br>
https://releases.linaro.org/components/toolchain/binaries/latest-5/arm-linux-gnueabihf/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf.tar.xz<br>
- For ARMv8 'aarch64-linux-gnu' is required:<br>
https://releases.linaro.org/components/toolchain/binaries/latest-5/aarch64-linux-gnu/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu.tar.xz<br>

The latest compiler (5.x) support the latest versions of Ubuntu and Debian. Should ArangoDB run on an older version of the operating system, as an earlier version of the compiler is required. The gcc version 4.9 can be found under the link:<br>
https://releases.linaro.org/components/toolchain/binaries/4.9-2016.02/arm-linux-gnueabihf/gcc-linaro-4.9-2016.02-x86_64_arm-linux-gnueabihf.tar.xz<br>

Downloading can be done with _wget 'filename'_. The downloaded files should be placed in a directory where that can be unzipped and then use. To set up the compiler absolute paths are required, so we assume that the files under<br>
__/mnt/sda4/_LINARAO__ be saved. The compressed files can be decompressed with _tar xpvf 'filename'_.<br>

##2. Preparation of the environment variables<br>

It is necessary to compile several Tools I, which is suitable for a particular architecture. The variable-names are set, thus setting up the environment variables is simplified. As a base variable is defined that is used for other definitions.<br>
Means the basic variable for the following Architerktugen done so:<br>
For the installed ARMv7 it is:<br>
_export TOOL_PREFIX=arm-linux-gnueabihf_<br>
For the unpacked ARMv7 version:<br>
_export TOOL_PREFIX=/mnt/sda4/_LINARO/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf_<br>
According installed ARMv8:<br>
_export TOOL_PREFIX=aarch64-linux-gnu_<br>
And the unpacked ARMv8 version:<br>
_export TOOL_PREFIX=/mnt/sda4/_LINARO/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu_<br>

Other definitions are:<br>
_export CXX=$TOOL_PREFIX-g++_<br>
_export AR=$TOOL_PREFIX-ar_<br>
_export RANLIB=$TOOL_PREFIX-ranlib_<br>
_export CC=$TOOL_PREFIX-gcc_<br>
_export LD=$TOOL_PREFIX-g++_<br>
_export LINK=$TOOL_PREFIX-g++_<br>
_export STRIP=$TOOL_PREFIX-strip_<br>

##3. Compilation of SSL

ArangoDB uses OpenSSL, but this is not included in the source code. For SSL takes packs are downloaded and installed separately. 
It is appropriate to require the installation once per architecture (with installed or unpacked version of the compiler). 
To compile the appropriate environment variables should be set. A separate directory should be used for each architecture.<br>

The steps for ARMv7 look like this (Assuming in /mnt/sda4/_SSL stored):<br>
_mkdir /mnt/sda4/_SSL/openssl-armv7_ <br>
_cd /mnt/sda4/_SSL/openssl-armv7_ <br>
_wget https://www.openssl.org/source/openssl-1.0.2h.tar.gz_<br>
_tar xzf openssl-1.0.2h.tar.gz_<br>
For ARMv8 it is stored in __/mnt//sda4/_SSL/openssl-armv8__<br>

It can also be the latest GIT version can be cloned. The Cloning and Compilation for respective versions:<br>

__ARMv7__:<br>
_git clone --single-branch --depth 1 -b OpenSSL_1_0_2-stable git://github.com/openssl/openssl ./openssl/arm-hf_<br>
_cd ./openssl/arm-hf_<br>
_./Configure linux-armv4 --openssldir=/opt/gnuarm-hf_<br>
_make -j4_<br>
_make install_<br>

__ARMv8__:<br>
_git clone --single-branch --depth 1 -b OpenSSL_1_0_2-stable git://github.com/openssl/openssl ./openssl/arm-64_<br>
_cd ./openssl/arm-64_<br>
_./Configure linux-aarch64 --openssldir=/opt/gnuarm-64_<br>
_make -j4_<br>
_make install_<br>
The unpacked versions could be similarly compiled.

##4. Compilation of ArangoDB

First ArangoDB should be cloned using GIT:<br>
_git clone git: //github.com/arangodb/arangodb.git_ - current devel branch<br>
_git clone git -b 3.0: //github.com/arangodb/arangodb.git_ - the current 3.0 release branch<br>

The compilation of ArangoDB not significantly different, except for the adjustment of CMake variables that vary per architecture.<br>
The steps are<br>
_mkdir -p A32_ - for ARMv7 architecture:<br>
_cd A32_<br>
_cmake .. -DCROSS_COMPILING=true -DOPENSSL_ROOT_DIR=/opt/gnuarm-hf_<br>

_mkdir -p A64_ - for ARMv8 architecture:<br>
_cd A64_<br>
_cmake .. -DCROSS_COMPILING=true -DOPENSSL_ROOT_DIR=/opt/gnuarm-64_<br>

Finally, _make -j4_ runs. (4 = number of available cores in the compilation)<br>
As a result of compiling the files are stored in the directory __./Axx/bin/__.

##5. Preparation for testing on an ARM board

So that ArangoDB can be tested on the respective architecture, the result should be packed in a package.
Here a peculiarity is required: CPack uses the local strip variant that does not come with binary files for ARM to right.
Therefore, the files should be treated after compilation with _$STRIP ./bin/*_ (strip variant of the ARM architecture). 
Then can be created that can be extracted and tested on an ARM board with _cpack -G STGZ_ an executable file. _cpack -G ZIP_ creates a ZIP file.
