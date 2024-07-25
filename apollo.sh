#!/bin/bash
#
# Apollo Build Script V3.0
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
# Define proper arch and dir for dts files
CR_DTS=arch/arm64/boot/dts/exynos
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
CR_KERNEL=$CR_DIR/arch/arm64/boot/Image
# Compiled dtb by dtbtool
CR_DTB=$CR_DIR/arch/arm64/boot/dtb.img
# Kernel Name and Version
CR_VERSION=V6.5
CR_NAME=DS
# Thread count
CR_JOBS=$(nproc --all)
# Target Android version
CR_ANDROID=q
CR_PLATFORM=13.0.0
# Target ARCH
CR_ARCH=arm64
# Current Date
CR_DATE=$(date +%d.%m.%Y)
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
CR_SELINUX="1"
CR_KSU="n"
CR_CLEAN="n"
# Compiler Paths
CR_GCC4=~/compiler/aarch64-linux-android-4.9/bin/aarch64-linux-android-
CR_GCC9=~/compiler/aarch64-linux-gnu-9.x/bin/aarch64-linux-gnu-
CR_GCC12=~/compiler/aarch64-linux-gnu-12.x/bin/aarch64-linux-gnu-
CR_GCC13=~/compiler/aarch64-linux-gnu-13.x/bin/aarch64-linux-gnu-
CR_CLANG=~/compiler/proton-clang-20210522
#####################################################

# Compiler Selection
BUILD_COMPILER()
{
if [ $CR_COMPILER = "1" ]; then
export CROSS_COMPILE=$CR_GCC4
compile="make"
CR_COMPILER="$CR_GCC4"
fi
if [ $CR_COMPILER = "2" ]; then
export CROSS_COMPILE=$CR_GCC9
compile="make"
CR_COMPILER="$CR_GCC9"
fi
if [ $CR_COMPILER = "3" ]; then
export CROSS_COMPILE=$CR_GCC12
compile="make"
CR_COMPILER="$CR_GCC12"
fi
if [ $CR_COMPILER = "4" ]; then
export CROSS_COMPILE=$CR_GCC13
compile="make"
CR_COMPILER="$CR_GCC13"
fi
if [ $CR_COMPILER = "5" ]; then
# Check packages
if ! dpkg-query -W -f='${Status}' gcc-arm-linux-gnueabi  | grep "ok installed"; then
	echo " gcc-arm-linux-gnueabi is missing, please install with sudo apt-get install gcc-arm-linux-gnueabi"
	exit 0;
fi
# Check packages
if ! dpkg-query -W -f='${Status}' gcc-aarch64-linux-gnu  | grep "ok installed"; then
	echo " gcc-aarch64-linux-gnu is missing, please install with sudo apt-get install gcc-aarch64-linux-gnu"
	exit 0;
fi
export PATH=$CR_CLANG/bin:$CR_CLANG/lib:${PATH}
export CLANG_TRIPLE=$CR_GCC9
export CROSS_COMPILE=$CR_GCC9
export CROSS_COMPILE_ARM32=arm-linux-gnueabi-
export CC=$CR_CLANG/bin/clang
#export REAL_CC=$CR_CLANG/bin/clang
#export LD=$CR_CLANG/bin/ld.lld
#export AR=$CR_CLANG/bin/llvm-ar
#export NM=$CR_CLANG/bin/llvm-nm
#export OBJCOPY=$CR_CLANG/bin/llvm-objcopy
#export OBJDUMP=$CR_CLANG/bin/llvm-objdump
#export READELF=$CR_CLANG/bin/llvm-readelf
#export STRIP=$CR_CLANG/bin/llvm-strip
#export LLVM=1
#export LLVM_IAS=1
export ARCH=arm64 && export SUBARCH=arm64
compile="make ARCH=arm64 CC=clang"
CR_COMPILER="$CR_CLANG"
fi
}

# Clean-up Function

BUILD_CLEAN()
{
if [ $CR_CLEAN = "y" ]; then
     echo " "
     echo " Cleaning build dir"
     $compile clean && $compile mrproper
     rm -r -f $CR_DTB
     rm -r -f $CR_KERNEL
     rm -rf $CR_DTS/.*.tmp
     rm -rf $CR_DTS/.*.cmd
     rm -rf $CR_DTS/*.dtb
     rm -rf $CR_DIR/.config
     rm -rf $CR_OUT/*.img
     rm -rf $CR_OUT/*.zip
fi
if [ $CR_CLEAN = "n" ]; then
     echo " "
     echo " Skip Full cleaning"
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
	CR_IMAGE_NAME=$CR_NAME-$CR_VERSION-$CR_VARIANT-$CR_DATE
	zver=$CR_NAME-$CR_VERSION-$CR_DATE
    
}

# Config Generation Function

BUILD_GENERATE_CONFIG()
{
  # Only use for devices that are unified with 2 or more configs
  echo "----------------------------------------------"
  echo "Building defconfig for $CR_VARIANT"
  echo " "
  # Respect CLEAN build rules
  BUILD_CLEAN
  if [ -e $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig ]; then
    echo " cleanup old configs "
    rm -rf $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
  fi
  echo " Copy $CR_CONFIG "
  cp -f $CR_DIR/arch/$CR_ARCH/configs/$CR_CONFIG $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
  # Split-config support for devices with unified defconfigs (Universal + device)
  if [ $CR_CONFIG_SPLIT = NULL ]; then
    echo " No split config support! "
  else
    echo " Copy $CR_CONFIG_SPLIT "
    cat $CR_DIR/arch/$CR_ARCH/configs/$CR_CONFIG_SPLIT >> $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
  fi
  # Regional Config
  echo " Copy $CR_CONFIG_REGION "
  cat $CR_DIR/arch/$CR_ARCH/configs/$CR_CONFIG_REGION >> $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
  # Apollo Custom defconfig
  echo " Copy $CR_CONFIG_APOLLO "
  cat $CR_DIR/arch/$CR_ARCH/configs/$CR_CONFIG_APOLLO >> $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
  # Selinux Never Enforce all targets
  if [ $CR_SELINUX = "1" ]; then
    echo " Building Permissive Kernel"
    echo "CONFIG_ALWAYS_PERMISSIVE=y" >> $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-Permissive
    zver=$zver-Permissive
  fi
  if [ $CR_KSU = "y" ]; then
    echo " Building KernelSU Kernel"
    echo "CONFIG_KSU=y" >> $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-KSU
    zver=$zver-KernelSU
  else
    echo "# CONFIG_KSU is not set" >> $CR_DIR/arch/$CR_ARCH/configs/tmp_defconfig
  fi
  echo " Set $CR_VARIANT to generated config "
  CR_CONFIG=tmp_defconfig
}

# Kernel information Function
BUILD_OUT()
{
  echo " "
  echo "----------------------------------------------"
  echo "$CR_VARIANT kernel build finished."
  echo "Compiled DTB Size = $sizdT Kb"
  echo "Kernel Image Size = $sizT Kb"
  echo "Boot Image   Size = $sizkT Kb"
  echo "$CR_PRODUCT/$CR_IMAGE_NAME.img Ready"
  echo "Press Any key to end the script"
  echo "----------------------------------------------"
}

# Kernel Compile Function
BUILD_ZIMAGE()
{
	echo "----------------------------------------------"
	echo " "
	echo "Building zImage for $CR_VARIANT"
	export LOCALVERSION=-$CR_IMAGE_NAME
	echo "Make $CR_CONFIG"
	$compile $CR_CONFIG
	echo "Make Kernel with $CR_COMPILER"
	$compile -j$CR_JOBS
	if [ ! -e $CR_KERNEL ]; then
	exit 0;
	echo "Image Failed to Compile"
	echo " Abort "
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
	echo -n "SEANDROIDENFORCE" » $CR_AIK/image-new.img
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
		echo " Galaxy Note 9 INTL"
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
		echo " Galaxy Note 9 KOR"
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
CR_COMPILER=2
CR_SELINUX=1
CR_KSU="y"
CR_CLEAN="n"
echo " DEBUG : Set Build options "
echo " DEBUG : Variant  : $CR_VARIANT_G965N"
echo " DEBUG : Compiler : $CR_GCC9"
echo " DEBUG : Selinux  : $CR_SELINUX Permissive"
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
	echo " bsdiff is missing, please install with sudo apt install bsdiff" 
	exit 0; 
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
	echo "$CR_NAME kernel build finished."
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
echo "1) $CR_GCC4 (GCC 4.9)"
echo "2) $CR_GCC9 (GCC 9.x)" 
echo "3) $CR_GCC12 (GCC 12.x)" 
echo "4) $CR_GCC13 (GCC 13.x)" 
echo "5) $CR_CLANG (Proton CLANG 13)"
echo " "
read -p "Please select your compiler (1-5) > " CR_COMPILER
echo " "
echo "1) SELinux Permissive " "2) SELinux Enforcing"
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
# Call functions
if [ "$CR_TARGET" = "7" ]; then
echo " "
read -p "Build Flashable ZIP ? (y/n) > " CR_MKZIP
echo " "
BUILD_ALL
else
BUILD
fi
