#!/bin/bash
#
# Apollo Build Script V3.5
# For Exynos9810
# Forked from Exynos8890 Script
# Coded by AnanJaser1211 @ 2019-2022
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Main Dir
CR_DIR=$(pwd)
# Compiler Dir
CR_TC=../compiler
# Target ARCH
CR_ARCH=arm64
# Define proper arch and dir for dts files
CR_DTS=arch/$CR_ARCH/boot/dts/exynos
# Define boot.img out dir
CR_OUT=$CR_DIR/Apollo/Out
CR_PRODUCT=$CR_DIR/Apollo/Product
# Kernel Zip Package
CR_ZIP=$CR_DIR/Apollo/kernelzip
CR_OUTZIP=$CR_OUT/kernelzip
# Presistant A.I.K Location
CR_AIK=$CR_DIR/Apollo/A.I.K
# Main Ramdisk Location
CR_RAMDISK=$CR_DIR/Apollo/Ramdisk
# Compiled image name and location (Image/zImage)
CR_KERNEL=$CR_DIR/arch/$CR_ARCH/boot/Image
# Compiled dtb by dtbtool
CR_DTB=$CR_DIR/arch/$CR_ARCH/boot/dtb.img
# defconfig dir
CR_DEFCONFIG=$CR_DIR/arch/$CR_ARCH/configs
# Kernel Name and Version
CR_VERSION=NEXT
CR_NAME=DS-萤火虫
# Thread count
CR_JOBS=$(nproc --all)
# Target Android version
CR_ANDROID=q
CR_PLATFORM=13.0.0
# Current Date
CR_DATE=$(date +%y%m%d)
# General init
export ANDROID_MAJOR_VERSION=$CR_ANDROID
export PLATFORM_VERSION=$CR_PLATFORM
export $CR_ARCH
##########################################
# Device specific Variables [SM-G960X]
CR_CONFIG_G960=starlte_defconfig
CR_VARIANT_G960F=G960F
CR_VARIANT_G960N=G960N
# Device specific Variables [SM-G965X]
CR_CONFIG_G965=star2lte_defconfig
CR_VARIANT_G965F=G965F
CR_VARIANT_G965N=G965N
# Device specific Variables [SM-N960X]
CR_CONFIG_N960=crownlte_defconfig
CR_VARIANT_N960F=N960F
CR_VARIANT_N960N=N960N
# Common configs
CR_CONFIG_9810=exynos9810_defconfig
CR_CONFIG_SPLIT=NULL
CR_CONFIG_APOLLO=apollo_defconfig
CR_CONFIG_INTL=eur_defconfig
CR_CONFIG_KOR=kor_defconfig
CR_SELINUX="2"
CR_KSU="n"
CR_CLEAN="n"
# Default Compilation
DEFAULT_TARGET=3   # crownlte
DEFAULT_COMPILER=3 # clang18
DEFAULT_SELINUX=2  # enforce
DEFAULT_KSU=y      # enabled
DEFAULT_CLEAN=n    # dirty
#####################################################

# Compiler Selection
BUILD_COMPILER()
{

# Auto download and setup compilers
# For manually adding compiler, add it under
# Apollo/toolchain/clang-custom and select option 7

# Clang Versions and features

if [ $CR_COMPILER = "1" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r416183/clang-r416183.tar.gz
CR_CLANG=$CR_TC/clang-12.0.4-r416183
fi
if [ $CR_COMPILER = "2" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r450784/clang-r450784b.tar.gz
CR_CLANG=$CR_TC/clang-14.0.4-r450784
fi
if [ $CR_COMPILER = "3" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r522817/clang-r522817.tar.gz
CR_CLANG=$CR_TC/clang-18.0.1-r522817
fi
if [ $CR_COMPILER = "4" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r530567/clang-r530567.tar.gz
CR_CLANG=$CR_TC/clang-19.0.0-r530567
fi
if [ $CR_COMPILER = "5" ]; then
CR_CLANG_URL=https://github.com/Neutron-Toolchains/clang-build-catalogue/releases/download/05012024/neutron-clang-05012024.tar.zst
CR_CLANG=$CR_TC/neutron-clang-18.0.0
fi
if [ $CR_COMPILER = "6" ]; then
CR_CLANG_URL=https://github.com/Neutron-Toolchains/clang-build-catalogue/releases/download/10032024/neutron-clang-10032024.tar.zst
CR_CLANG=$CR_TC/neutron-clang-19.0.0
fi
if [ $CR_COMPILER = "7" ]; then
CR_CLANG=$CR_TC/neutron-clang20-26.07.24
fi
if [ $CR_COMPILER = "8" ]; then
CR_CLANG=$CR_TC/clang-custom
fi

if [ $CR_COMPILER != "8" ]; then
	if [ ! -d "$CR_CLANG/bin" ] || [ ! -d "$CR_CLANG/lib" ]; then
		echo " "
		echo " $CR_CLANG compiler is missing"
		echo " "
		echo " "
		read -p "Download Toolchain ? (y/n) > " TC_DL
		if [ $TC_DL = "y" ]; then
			echo "Checking URL validity..."
			URL=$CR_CLANG_URL
			if curl --output /dev/null --silent --head --fail "$URL"; then
				echo "URL exists: $URL"
				echo "Downloading $CR_CLANG"
				if [ ! -e $CR_TC ]; then
					mkdir $CR_TC
				fi
				if [ ! -e $CR_CLANG ]; then
					mkdir $CR_CLANG
				else
					# Remove incomplete
					rm -rf $CR_CLANG
					mkdir $CR_CLANG
				fi
				wget -qO- $URL | tar --use-compress-program=unzstd -xv -C $CR_CLANG
				if [ $? -ne 0 ]; then
					echo "Download failed or was incomplete"
					echo "Setup Compiler and try again"
					exit 0;
				fi
				# Neutron Needs patches
				if [ $CR_COMPILER = "5" ] || [ $CR_COMPILER = "6" ]; then
					cd $CR_CLANG
					bash <(curl -s "https://raw.githubusercontent.com/Neutron-Toolchains/antman/main/antman") --patch=glibc
					cd $CR_DIR
				fi
				echo "Compiler Downloaded."
			else
				echo "Invalid URL: $URL"
				exit 0;
			fi
		else
			echo " Aborting "
			echo " Setup Compiler and try again"
			exit 0;
		fi
	fi
else
    if [ ! -d "$CR_CLANG/bin" ] || [ ! -d "$CR_CLANG/lib" ]; then
        echo "clang-custom compiler is missing in $CR_TC/clang-custom"
        exit 0;
    fi
fi

# Clang Features (18 and higher)
if [ $CR_COMPILER -ge 3 ]; then
export CONFIG_THINLTO=y
export CONFIG_UNIFIEDLTO=y
export CONFIG_LLVM_MLGO_REGISTER=y
export CONFIG_LLVM_POLLY=y
export CONFIG_LLVM_DFA_JUMP_THREAD=y
fi

export PATH=$CR_CLANG/bin:$CR_CLANG/lib:${PATH}
export CC=$CR_CLANG/bin/clang
export REAL_CC=$CR_CLANG/bin/clang
export LD=$CR_CLANG/bin/ld.lld
export AR=$CR_CLANG/bin/llvm-ar
export NM=$CR_CLANG/bin/llvm-nm
export OBJCOPY=$CR_CLANG/bin/llvm-objcopy
export OBJDUMP=$CR_CLANG/bin/llvm-objdump
export READELF=$CR_CLANG/bin/llvm-readelf
export STRIP=$CR_CLANG/bin/llvm-strip
export LLVM=1
export KALLSYMS_EXTRA_PASS=1
export ARCH=arm64 && export SUBARCH=arm64
compile="make ARCH=arm64 CC=clang"
CR_COMPILER_ARG="$CR_CLANG"
}

# Clean-up Function

BUILD_CLEAN()
{
if [[ "$CR_CLEAN" =~ ^[yY]$ ]]; then
     $compile clean && $compile mrproper
     rm -r -f $CR_DTB
     rm -r -f $CR_KERNEL
     rm -rf $CR_DTS/.*.tmp
     rm -rf $CR_DTS/.*.cmd
     rm -rf $CR_DTS/*.dtb
     rm -rf $CR_DIR/.config
     rm -rf $CR_OUT/*.img
     rm -rf $CR_OUT/*.zip
else
     rm -r -f $CR_DTB
     rm -r -f $CR_KERNEL
     rm -rf $CR_DTS/.*.tmp
     rm -rf $CR_DTS/.*.cmd
     rm -rf $CR_DTS/*.dtb
     rm -rf $CR_DIR/.config
     rm -rf $CR_DIR/.version
fi
}

# Kernel Name Function

BUILD_IMAGE_NAME()
{
	CR_IMAGE_NAME=$CR_NAME-$CR_VERSION-$CR_DATE
	zver=$CR_NAME-$CR_VERSION-$CR_DATE
    
}

# Build options
BUILD_OPTIONS()
{
	# KSU Version
	KSU_VERSION=$( [ -f "drivers/kernelsu/Makefile" ] && grep -oP '(?<=-DKSU_VERSION=)[0-9]+' drivers/kernelsu/Makefile )
	echo "----------------------------------------------"
	echo " Apollo Kernel Build Options "
	echo " "
	echo " Kernel		- $CR_IMAGE_NAME"
	echo " Device		- $CR_VARIANT"
	echo " Compiler	- $CR_COMPILER_ARG"
	if [[ "$CR_CLEAN" =~ ^[yY]$ ]]; then
		echo " Env		- Clean Build"
	else
		echo " Env		- Dirty Build"
	fi
	if [ $CR_SELINUX = "1" ]; then
		echo " SELinux	- Permissive"
	else
		echo " SELinux	- Enforcing"
	fi
	if [[ "$CR_KSU" =~ ^[yY]$ ]]; then
		if [ -n "$KSU_VERSION" ]; then
		echo " KernelSU	- Version: $KSU_VERSION"
		else
		echo " KernelSU	- Enabled"
		fi
	else
		echo " KernelSU	- Disabled"
	fi
	echo " "
}

# Config Generation Function

BUILD_GENERATE_CONFIG()
{
  # Only use for devices that are unified with 2 or more configs
  echo "----------------------------------------------"
  echo " Generating defconfig for $CR_VARIANT"
  echo " "
  # Respect CLEAN build rules
  BUILD_CLEAN
  if [ -e $CR_DEFCONFIG/tmp_defconfig ]; then
    echo " Clean-up old config "
    rm -rf $CR_DEFCONFIG/tmp_defconfig
  fi
  echo " Base	- $CR_CONFIG "
  cp -f $CR_DEFCONFIG/$CR_CONFIG $CR_DEFCONFIG/tmp_defconfig
  # Split-config support for devices with unified defconfigs (Universal + device)
  if [ $CR_CONFIG_SPLIT = NULL ]; then
    echo " No split config support! "
  else
    echo " Device - $CR_CONFIG_SPLIT "
    cat $CR_DEFCONFIG/$CR_CONFIG_SPLIT >> $CR_DEFCONFIG/tmp_defconfig
  fi
  # Regional Config
  echo " Region	- $CR_CONFIG_REGION "
  cat $CR_DEFCONFIG/$CR_CONFIG_REGION >> $CR_DEFCONFIG/tmp_defconfig
  # Apollo Custom defconfig
  echo " Apollo	- $CR_CONFIG_APOLLO "
  cat $CR_DEFCONFIG/$CR_CONFIG_APOLLO >> $CR_DEFCONFIG/tmp_defconfig
  # Selinux Never Enforce all targets
  if [ $CR_SELINUX = "1" ]; then
    echo " Building SELinux Permissive Kernel"
    echo "CONFIG_ALWAYS_PERMISSIVE=y" >> $CR_DEFCONFIG/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-Permissive
    zver=$zver-Permissive
  else
    echo " Building SELinux Enforced Kernel"
  fi
  if [[ "$CR_KSU" =~ ^[yY]$ ]]; then
    echo " Building KernelSU"
    echo "CONFIG_KSU=y" >> $CR_DEFCONFIG/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-KSU
    zver=$zver-KernelSU
  else
    echo "# CONFIG_KSU is not set" >> $CR_DEFCONFIG/tmp_defconfig
  fi
  echo " $CR_VARIANT config generated "
  echo " "
  CR_CONFIG=tmp_defconfig
}

# Kernel information Function
BUILD_OUT()
{
# KSU Version
	KSU_VERSION=$( [ -f "drivers/kernelsu/Makefile" ] && grep -oP '(?<=-DKSU_VERSION=)[0-9]+' drivers/kernelsu/Makefile )
  echo "----------------------------------------------"
  echo " 内核		- $CR_IMAGE_NAME"
  echo " 设备		- $CR_VARIANT"
  echo " 编译器 	- $CR_COMPILER_ARG"
	if [[ "$CR_CLEAN" =~ ^[yY]$ ]]; then
		echo " Env		- 清理构建"
	else
		echo " Env		- 脏构建"
	fi
	if [ $CR_SELINUX = "1" ]; then
		echo " SELinux	- Permissive"
	else
		echo " SELinux	- Enforcing"
	fi
  echo " KernelSU	- 版本：$KSU_VERSION"
  echo "----------------------------------------------"
  echo "$CR_VARIANT 内核构建完成"
  echo "编译后的 DTB 大小 = $sizdT Kb"
  echo "内核镜像     大小 = $sizT Kb"
  echo "启动镜像     大小 = $sizkT Kb"
  echo "$CR_PRODUCT/$CR_IMAGE_NAME.img 准备好了"
  echo "按任意键结束脚本"
  echo "----------------------------------------------"
}

# Kernel Compile Function
BUILD_ZIMAGE()
{
	echo "----------------------------------------------"
	echo " "
	echo "为 $CR_VARIANT 构建 zImage"
	export LOCALVERSION=_$CR_IMAGE_NAME
	echo "编译 $CR_CONFIG"
	$compile $CR_CONFIG
	echo "使用 $CR_COMPILER_ARG 编译内核"
	$compile -j$CR_JOBS
	if [ ! -e $CR_KERNEL ]; then
	exit 0;
	echo "镜像编译失败"
	echo " 中止 "
	fi
	du -k "$CR_KERNEL" | cut -f1 >sizT
	sizT=$(head -n 1 sizT)
	rm -rf sizT
	echo " "
	echo "----------------------------------------------"
}

# Device-Tree compile Function
BUILD_DTB()
{
	echo "----------------------------------------------"
	echo " "
	echo "Checking DTB for $CR_VARIANT"
	# This source does compiles dtbs while doing Image
	if [ ! -e $CR_DTB ]; then
        exit 0;
        echo "DTB Failed to Compile"
        echo " Abort "
	else
        echo "DTB Compiled at $CR_DTB"
	fi
	rm -rf $CR_DTS/.*.tmp
	rm -rf $CR_DTS/.*.cmd
	rm -rf $CR_DTS/*.dtb
	du -k "$CR_DTB" | cut -f1 >sizdT
	sizdT=$(head -n 1 sizdT)
	rm -rf sizdT
	echo " "
	echo "----------------------------------------------"
}

# Ramdisk Function
PACK_BOOT_IMG()
{
	echo "----------------------------------------------"
	echo " "
	echo "Building Boot.img for $CR_VARIANT"
	# Copy Ramdisk
	cp -rf $CR_RAMDISK/* $CR_AIK
	# Move Compiled kernel and dtb to A.I.K Folder
	mv $CR_KERNEL $CR_AIK/split_img/boot.img-zImage
	mv $CR_DTB $CR_AIK/split_img/boot.img-dtb
	# Create boot.img
	$CR_AIK/repackimg.sh
	if [ ! -e $CR_AIK/image-new.img ]; then
        exit 0;
        echo "Boot Image Failed to pack"
        echo " Abort "
	fi
	# Remove red warning at boot
	echo -n "SEANDROIDENFORCE" >> $CR_AIK/image-new.img
	# Copy boot.img to Production folder
	if [ ! -e $CR_PRODUCT ]; then
        mkdir $CR_PRODUCT
	fi
	cp $CR_AIK/image-new.img $CR_PRODUCT/$CR_IMAGE_NAME.img
	# Move boot.img to out dir
	if [ ! -e $CR_OUT ]; then
        mkdir $CR_OUT
	fi
	mv $CR_AIK/image-new.img $CR_OUT/$CR_IMAGE_NAME.img
	du -k "$CR_OUT/$CR_IMAGE_NAME.img" | cut -f1 >sizkT
	sizkT=$(head -n 1 sizkT)
	rm -rf sizkT
	echo " "
	$CR_AIK/cleanup.sh
	# Respect CLEAN build rules
	BUILD_CLEAN
}

# Single Target Build Function
BUILD()
{
	if [ "$CR_TARGET" = "1" ]; then
		echo " Galaxy S9 INTL"
		CR_CONFIG_SPLIT=$CR_CONFIG_G960
		CR_CONFIG_REGION=$CR_CONFIG_INTL
		CR_VARIANT=$CR_VARIANT_G960F
		export "CONFIG_MACH_EXYNOS9810_STARLTE_EUR_OPEN=y"
	fi
	if [ "$CR_TARGET" = "2" ]; then
		echo " Galaxy S9+ INTL"
		CR_CONFIG_SPLIT=$CR_CONFIG_G965
		CR_CONFIG_REGION=$CR_CONFIG_INTL
		CR_VARIANT=$CR_VARIANT_G965F
		export "CONFIG_MACH_EXYNOS9810_STAR2LTE_EUR_OPEN=y"
	fi
	if [ "$CR_TARGET" = "3" ]
	then
		echo " Galaxy Note9 INTL"
		CR_CONFIG_SPLIT=$CR_CONFIG_N960
		CR_CONFIG_REGION=$CR_CONFIG_INTL
		CR_VARIANT=$CR_VARIANT_N960F
		export "CONFIG_MACH_EXYNOS9810_CROWNLTE_EUR_OPEN=y"
	fi
	if [ "$CR_TARGET" = "4" ]; then
		echo " Galaxy S9 KOR"
		CR_CONFIG_SPLIT=$CR_CONFIG_G960
		CR_CONFIG_REGION=$CR_CONFIG_KOR
		CR_VARIANT=$CR_VARIANT_G960N
		export "CONFIG_MACH_EXYNOS9810_STARLTE_KOR=y"
	fi
	if [ "$CR_TARGET" = "5" ]; then
		echo " Galaxy S9+ KOR"
		CR_CONFIG_SPLIT=$CR_CONFIG_G965
		CR_CONFIG_REGION=$CR_CONFIG_KOR
		CR_VARIANT=$CR_VARIANT_G965N
		export "CONFIG_MACH_EXYNOS9810_STAR2LTE_KOR=y"
	fi
	if [ "$CR_TARGET" = "6" ]
	then
		echo " Galaxy Note9 KOR"
		CR_CONFIG_SPLIT=$CR_CONFIG_N960
		CR_CONFIG_REGION=$CR_CONFIG_KOR
		CR_VARIANT=$CR_VARIANT_N960N
		export "CONFIG_MACH_EXYNOS9810_CROWNLTE_KOR=y"
	fi	
	CR_CONFIG=$CR_CONFIG_9810
	BUILD_COMPILER
	BUILD_CLEAN
	BUILD_IMAGE_NAME
	BUILD_GENERATE_CONFIG
	# Print build options
	BUILD_OPTIONS
	BUILD_ZIMAGE
	BUILD_DTB
	if [ "$CR_MKZIP" = "y" ]; then # Allow Zip Package for mass compile only
	echo " Start Build ZIP Process "
	PACK_KERNEL_ZIP
	else
	PACK_BOOT_IMG
	BUILD_OUT
	fi
}

# Multi-Target Build Function
BUILD_ALL(){
echo "----------------------------------------------"
echo " Compiling ALL targets "
CR_TARGET=1
BUILD
export -n "CONFIG_MACH_EXYNOS9810_STARLTE_EUR_OPEN"
CR_TARGET=2
BUILD
export -n "CONFIG_MACH_EXYNOS9810_STAR2LTE_EUR_OPEN"
CR_TARGET=3
BUILD
export -n "CONFIG_MACH_EXYNOS9810_CROWNLTE_EUR_OPEN"
CR_TARGET=4
BUILD
export -n "CONFIG_MACH_EXYNOS9810_STARLTE_KOR"
CR_TARGET=5
BUILD
export -n "CONFIG_MACH_EXYNOS9810_STAR2LTE_KOR"
CR_TARGET=6
BUILD
export -n "CONFIG_MACH_EXYNOS9810_CROWNLTE_KOR"
}

# Preconfigured Debug build
BUILD_DEBUG(){
echo "----------------------------------------------"
echo " DEBUG : Debug build initiated "
CR_TARGET=5
CR_COMPILER=3
CR_SELINUX=0
CR_KSU="y"
CR_CLEAN="n"
echo " DEBUG : Set Build options "
echo " DEBUG : Variant  : $CR_VARIANT_N960F"
echo " DEBUG : Compiler : Clang 18"
echo " DEBUG : Selinux  : $CR_SELINUX Enforcing"
echo " DEBUG : Clean    : $CR_CLEAN"
echo "----------------------------------------------"
BUILD
echo "----------------------------------------------"
echo " DEBUG : build completed "
echo "----------------------------------------------"
exit 0;
}


# Pack All Images into ZIP
PACK_KERNEL_ZIP() {
echo "----------------------------------------------"
echo " Packing ZIP "

# Variables
CR_BASE_KERNEL=$CR_OUTZIP/floyd/G960F-kernel
CR_BASE_DTB=$CR_OUTZIP/floyd/G960F-dtb

# Check packages
if ! dpkg-query -W -f='${Status}' bsdiff  | grep "ok installed"; then 
	echo "bsdiff is missing and is required for ZIP Packaging."
	read -p "Do you want to install bsdiff? This requires sudo privileges. (y/n) > " INSTALL_BSDIFF
	if [ "$INSTALL_BSDIFF" = "y" ]; then
		echo "installing bsdiff."
		sudo apt update
		sudo apt install -y bsdiff
		if ! dpkg-query -W -f='${Status}' bsdiff | grep "ok installed"; then
			echo "Failed to install bsdiff. Please try installing it manually."
			exit 0;
		fi
	else
		echo "Please install bsdiff with sudo apt install bsdiff and try again."
		exit 0;
	fi
fi

# Initalize with base image (Starlte)
if [ "$CR_TARGET" = "1" ]; then # Always must run ONCE during BUILD_ALL otherwise fail. Setup directories
	echo " "
	echo " Kernel Zip Packager "
	echo " Base Target "
	echo " Clean Out directory "
	echo " "
	rm -rf $CR_OUTZIP
	cp -r $CR_ZIP $CR_OUTZIP
	echo " "
	echo " Copying $CR_BASE_KERNEL "
	echo " Copying $CR_BASE_DTB "
	echo " "
	if [ ! -e $CR_KERNEL ] || [ ! -e $CR_DTB ]; then
        exit 0;
        echo " Kernel not found!"
        echo " Abort "
	else
        cp $CR_KERNEL $CR_BASE_KERNEL
        cp $CR_DTB $CR_BASE_DTB
	fi
	# Set kernel version
fi
if [ ! "$CR_TARGET" = "1" ]; then # Generate patch files for non starlte kernels
	echo " "
	echo " Kernel Zip Packager "
	echo " "
	echo " Generating Patch kernel for $CR_VARIANT "
	echo " "
	if [ ! -e $CR_KERNEL ] || [ ! -e $CR_DTB ]; then
        echo " Kernel not found! "
        echo " Abort "
        exit 0;
	else
		bsdiff $CR_BASE_KERNEL $CR_KERNEL $CR_OUTZIP/floyd/$CR_VARIANT-kernel
		if [ ! -e $CR_OUTZIP/floyd/$CR_VARIANT-kernel ]; then
			echo "ERROR: bsdiff $CR_BASE_KERNEL $CR_KERNEL $CR_OUTZIP/floyd/$CR_VARIANT-kernel Failed!"
			exit 0;
		fi
		bsdiff $CR_BASE_DTB $CR_DTB $CR_OUTZIP/floyd/$CR_VARIANT-dtb
		if [ ! -e $CR_OUTZIP/floyd/$CR_VARIANT-kernel ]; then
			echo "ERROR: bsdiff $CR_BASE_KERNEL $CR_DTB $CR_OUTZIP/floyd/$CR_VARIANT-dtb Failed!"
			exit 0;
		fi
	fi
fi
if [ "$CR_TARGET" = "6" ]; then # Final kernel build
	echo " Generating ZIP Package for $CR_NAME-$CR_VERSION-$CR_DATE"
	sed -i "s/fkv/$zver/g" $CR_OUTZIP/META-INF/com/google/android/update-binary
	cd $CR_OUTZIP && zip -r $CR_PRODUCT/$zver.zip * && cd $CR_DIR
	du -k "$CR_PRODUCT/$zver.zip" | cut -f1 >sizdz
	sizdz=$(head -n 1 sizdz)
	rm -rf sizdz
	echo " "
	echo "----------------------------------------------"
	echo "$CR_NAME 内核构建完成"
	echo "Compiled Package Size = $sizdz Kb"
	echo "$zver.zip Ready"
	echo "Press Any key to end the script"
	echo "----------------------------------------------"
fi
}

# Main Menu
clear
echo "----------------------------------------------"
echo "$CR_NAME $CR_VERSION Build Script $CR_DATE"
if [ "$1" = "-d" ]; then
BUILD_DEBUG
fi
echo " "
echo " "
echo "1) starlte" "   2) star2lte" "   3) crownlte"
echo "4) starltekor" "5) star2ltekor" "6) crownltekor"
echo  " "
echo "7) Build All/ZIP"               "8) Abort"
echo "----------------------------------------------"
read -p "Please select your build target (1-8) > " CR_TARGET
echo "----------------------------------------------"
echo " "
echo "1) Clang 12 (LLVM +LTO)"
echo "2) Clang 14 (LLVM +LTO)"
echo "3) Clang 18 (LLVM +LTO PGO Bolt MLGO Polly)"
echo "4) Clang 19 (^)"
echo "5) Neutron Clang 18 (^)"
echo "6) Neutron Clang 19 (^)"
echo "7) Neutron Clang 20 (BETA)"
echo "8) Other (Apollo/toolchain/clang-custom)"
echo " "
read -p "Please select your compiler (1-7) > " CR_COMPILER
echo " "
echo "1) SELinux Permissive "  "2) SELinux Enforcing"
echo " "
read -p "Please select your SElinux mode (1-2) > " CR_SELINUX
echo " "
read -p "Enable KernelSU? (y/n) > " CR_KSU
echo " "
if [ "$CR_TARGET" = "8" ]; then
echo "Build Aborted"
exit
fi
echo " "
read -p "Clean Builds? (y/n) > " CR_CLEAN
echo " "

# Validate options
if ! [[ "$CR_TARGET" =~ ^[1-8]$ ]]; then
    CR_TARGET=$DEFAULT_TARGET
    echo " No target selected, defaulting to star2ltekor"
fi

if ! [[ "$CR_COMPILER" =~ ^[1-7]$ ]]; then
    CR_COMPILER=$DEFAULT_COMPILER
fi

if ! [[ "$CR_SELINUX" =~ ^[1-2]$ ]]; then
    CR_SELINUX=$DEFAULT_SELINUX
fi

if ! [[ "$CR_KSU" =~ ^[yYnN]$ ]]; then
    CR_KSU=$DEFAULT_KSU
fi
if ! [[ "$CR_CLEAN" =~ ^[yYnN]$ ]]; then
    CR_CLEAN=$DEFAULT_CLEAN
fi

# Call functions
if [ "$CR_TARGET" = "7" ]; then
echo " "
read -p "Build Flashable ZIP ? (y/n) > " CR_MKZIP
echo " "
BUILD_ALL
else
BUILD
fi
