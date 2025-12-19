#!/bin/bash
#
# Apollo Build Script V3.6
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
CR_TC=$CR_DIR/toolchain
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
CR_VERSION=V12
CR_NAME=DS-ACK
# Thread count
CR_JOBS=$(nproc --all)
# Target Android version
CR_ANDROID=q
CR_PLATFORM=13.0.0
# Current Date
CR_DATE=$(date +%Y%m%d)
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
DEFAULT_TARGET=5   # star2ltekor
DEFAULT_COMPILER=3 # clang18
DEFAULT_SELINUX=2  # enforce
DEFAULT_KSU=y      # enabled
DEFAULT_SUS=n	   # Disable susfs
DEFAULT_CLEAN=n    # dirty
#####################################################
READY="$CR_DIR/buildtools"
# Function for first-time setup
first_time_setup() {
    local packages=(
        "zstd"
        "git"
        "gnupg"
        "flex"
        "bison"
        "build-essential"
        "zip"
        "curl"
        "zlib1g-dev"
        "libc6-dev-i386"
        "x11proto-core-dev"
        "libx11-dev"
        "lib32z1-dev"
        "libgl1-mesa-dev"
        "libxml2-utils"
        "xsltproc"
        "unzip"
        "fontconfig"
        "python-dev-is-python3"
        "bsdiff"
    )

    # Check if apt and dpkg-query are available (Debian-like system)
    if command -v apt >/dev/null && command -v dpkg-query >/dev/null; then
    	echo -e "\nFirst Time Setup: The following packages are required for the build tools:"
	printf ' - %s\n' "${packages[@]}"
        read -p "Do you want to install these packages? This requires sudo privileges. (y/n) > " INSTALL_BUILD_TOOLS
        if [[ "$INSTALL_BUILD_TOOLS" =~ ^[yY]$ ]]; then
            echo "Installing required packages..."
            sudo apt update
            sudo apt install -y "${packages[@]}"

            echo "Verifying package installation..."
            for package in "${packages[@]}"; do
                if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q "ok installed"; then
                    echo "Failed to install $package. Please try installing it manually."
                    exit 1
                fi
            done

            touch "$READY"
            echo -e "\n$READY created successfully. Delete this file to re-run setup.\n"
        else
            echo "Installation skipped. Please install the required packages manually and re-run."
            exit 1
        fi

    else
    	echo -e "\nFirst Time Setup: "
        echo -e "\nNon-Debian system detected (missing apt or dpkg-query)."
        echo "Please install the following packages manually:"
        printf ' - %s\n' "${packages[@]}"
        echo -e "\nTo hide this message in the future, run:"
        echo -e "\n  touch $READY\n"
    fi
}

# Compiler Selection
BUILD_COMPILER()
{

# Auto download and setup compilers
# For manually adding compiler, add it under
# Apollo/toolchain/clang-custom and select option 7

# Clang Versions and features

if [ $CR_COMPILER = "1" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r416183/clang-r416183.tar.gz
CR_CLANG=$CR_TC/clang-r416183
fi
if [ $CR_COMPILER = "2" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r450784/clang-r450784b.tar.gz
CR_CLANG=$CR_TC/clang-r450784
fi
if [ $CR_COMPILER = "3" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r522817/clang-r522817.tar.gz
CR_CLANG=$CR_TC/clang-r522817
fi
if [ $CR_COMPILER = "4" ]; then
CR_CLANG_URL=https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/llvm-r530567/clang-r530567.tar.gz
CR_CLANG=$CR_TC/clang-r530567
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
CR_CLANG=$CR_TC/clang-custom
fi

if [ $CR_COMPILER != "7" ]; then
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
	CR_IMAGE_NAME=$CR_NAME-$CR_VERSION-$CR_VARIANT-$CR_DATE
	zver=$CR_NAME-$CR_VERSION-$CR_DATE

}

# Build options
BUILD_OPTIONS()
{
	# Compute KSU_VERSION if KernelSU is enabled
	if [[ "$CR_KSU" =~ ^[yY]$ ]]; then
		KERNELSU_DIR="drivers/kernelsu"
		if [ -e "$KERNELSU_DIR/../.git" ]; then
			# Unshallow if needed
			if [ -f "$KERNELSU_DIR/../.git/shallow" ]; then
				git -C "$KERNELSU_DIR" fetch --unshallow
			fi
			GIT_VERSION=$(git -C "$KERNELSU_DIR" rev-list --count HEAD 2>/dev/null)
			if [[ "$GIT_VERSION" =~ ^[0-9]+$ ]]; then
				KSU_VERSION=$((10000 + GIT_VERSION + 200))
			else
				KSU_VERSION="unknown"
			fi
		else
			# Default
			KSU_VERSION=11998
		fi
	fi

	# Get SUSFS_VERSION if SuS is enabled
	if [[ "$CR_SUS" =~ ^[yY]$ ]]; then
		SUSFS_HEADER="include/linux/susfs.h"
		if [ -f "$SUSFS_HEADER" ]; then
			SUSFS_VERSION=$(grep -E '^#define[[:space:]]+SUSFS_VERSION' "$SUSFS_HEADER" | awk '{print $3}' | tr -d '"')
		else
			SUSFS_VERSION="unknown"
		fi
	fi

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

	if [ "$CR_SELINUX" = "1" ]; then
		echo " Selinux	- Permissive"
	else
		echo " Selinux	- Enforcing"
	fi

	if [[ "$CR_KSU" =~ ^[yY]$ ]]; then
		if [ -n "$KSU_VERSION" ]; then
			echo " KernelSU	- Version: $KSU_VERSION"
		else
			echo " KernelSU	- Enabled"
		fi

		if [[ "$CR_SUS" =~ ^[yY]$ ]]; then
			if [ -n "$SUSFS_VERSION" ]; then
				echo " SuSFS		- Version: $SUSFS_VERSION"
			else
				echo " SuSFS		- Enabled"
			fi
		else
			echo " SuSFS		- Disabled"
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
    echo " Building SElinux Permissive Kernel"
    echo "CONFIG_ALWAYS_PERMISSIVE=y" >> $CR_DEFCONFIG/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-Permissive
    zver=$zver-Permissive
  else
    echo " Building SElinux Enforced Kernel"
  fi
  # Kernel SU
  # Ensure Submodule is present
  git submodule update --init --recursive
  # Default
  git config -f .gitmodules submodule.KernelSU.branch next
  if [[ "$CR_KSU" =~ ^[yY]$ ]]; then
    echo " Building KernelSU-Next"
    # SUSFS-NEXT on Fork
    if [[ "$CR_SUS" =~ ^[yY]$ ]]; then
      REPO_URL="https://github.com/sidex15/KernelSU-Next"
      BRANCH="next-susfs"
      echo " Using sidex15 SuSFS repository and branch"
    else
      REPO_URL="https://github.com/KernelSU-Next/KernelSU-Next"
      BRANCH="next"
      echo " Using standard repository and branch"
    fi

    # Update .gitmodules with correct repository
    git config -f .gitmodules submodule.KernelSU.url "$REPO_URL"
    git config -f .gitmodules submodule.KernelSU.branch "$BRANCH"

    # Sync and update submodule
    git submodule sync --recursive
    git submodule update --init --recursive

    # Image Info
    echo "CONFIG_KSU=y" >> $CR_DEFCONFIG/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-ksu
    zver=$zver-KernelSU

  if [[ "$CR_SUS" =~ ^[yY]$ ]]; then
    echo " Adding KernelSU-Next-SuSFS"
    # SuSFS Config
    echo "CONFIG_KSU_SUSFS=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_HAS_MAGIC_MOUNT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_PATH=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_MOUNT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_KSTAT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_OVERLAYFS=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_TRY_UMOUNT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SPOOF_UNAME=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_ENABLE_LOG=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_OPEN_REDIRECT=y" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_SU=y" >> $CR_DEFCONFIG/tmp_defconfig
    CR_IMAGE_NAME=$CR_IMAGE_NAME-susfs
    zver=$zver-SuSFS
  else
    # Disable SuSFS
    echo "CONFIG_KSU_SUSFS=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_HAS_MAGIC_MOUNT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_PATH=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_MOUNT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_AUTO_ADD_SUS_BIND_MOUNT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_KSTAT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_OVERLAYFS=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_TRY_UMOUNT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_AUTO_ADD_TRY_UMOUNT_FOR_BIND_MOUNT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SPOOF_UNAME=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_ENABLE_LOG=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_OPEN_REDIRECT=n" >> $CR_DEFCONFIG/tmp_defconfig
    echo "CONFIG_KSU_SUSFS_SUS_SU=n" >> $CR_DEFCONFIG/tmp_defconfig
  fi
    echo " Fetching KernelSU-Next $BRANCH Branch from $REPO_URL"
    cd $CR_DIR/KernelSU
    git reset --hard
    git clean -fdx
    git fetch origin
    git checkout -B "$BRANCH" origin/"$BRANCH"
    git pull --ff-only origin "$BRANCH"
    cd $CR_DIR
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

LIST_COMPILED_ZIPS() {
    echo ""
    echo "----------------------------------------------"
    echo "Compiled ZIPs (this session):"
    echo "----------------------------------------------"

    for zver in "${COMPILED_ZIPS[@]}"; do
        find "$CR_PRODUCT" -type f -name "$zver*.zip" | sort | while read zipfile; do
            size_kb=$(du -k "$zipfile" | cut -f1)
            echo "$(basename "$zipfile") - ${size_kb} KB"
        done
    done

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
	echo "Make Kernel with $CR_COMPILER_ARG"
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

BUILD_ALL_COMBINATIONS(){
echo "----------------------------------------------"
echo " Compiling ALL ZIPs "
echo " ZIP - Non-Root "
echo "----------------------------------------------"
CR_COMPILER=3 # clang18
CR_SELINUX=2  # enforce
CR_KSU=n      # disable ksu
CR_SUS=n	   # Disable susfs
BUILD_ALL
echo "----------------------------------------------"
echo " ZIP - Non-Root - Permissive"
CR_COMPILER=3 # clang18
CR_SELINUX=1  # permissive
CR_KSU=n      # disable ksu
CR_SUS=n	   # Disable susfs
BUILD_ALL
echo "----------------------------------------------"
echo " ZIP - Root "
CR_COMPILER=3 # clang18
CR_SELINUX=2  # enforce
CR_KSU=y      # enable ksu
CR_SUS=n	   # Disable susfs
BUILD_ALL
echo "----------------------------------------------"
echo " ZIP - Root - Permissive"
CR_COMPILER=3 # clang18
CR_SELINUX=1  # permissive
CR_KSU=y      # enable ksu
CR_SUS=n	   # Disable susfs
BUILD_ALL
echo "----------------------------------------------"
echo " ZIP - Root - SuSFS"
CR_COMPILER=3 # clang18
CR_SELINUX=2  # enforce
CR_KSU=y      # enable ksu
CR_SUS=y	   # enable susfs
BUILD_ALL
echo "----------------------------------------------"
echo " ZIP - Root - SuSFS - Permissive"
CR_COMPILER=3 # clang18
CR_SELINUX=1  # permissive
CR_KSU=y      # enable ksu
CR_SUS=y	   # enable susfs
BUILD_ALL
}

# Preconfigured Debug build
BUILD_DEBUG(){
echo "----------------------------------------------"
echo " DEBUG : Debug build initiated "
CR_TARGET=5
CR_COMPILER=3
CR_SELINUX=0
CR_KSU="y"
CR_SUS="n"
CR_CLEAN="n"
echo " DEBUG : Set Build options "
echo " DEBUG : Variant  : $CR_VARIANT_G965N"
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
	# Store zips
	COMPILED_ZIPS+=("$zver")
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

echo ""
if [ ! -f "$READY" ]; then
    first_time_setup
    echo "----------------------------------------------"
else
    echo "Build tools are installed."
    echo "----------------------------------------------"
    echo ""
fi

# Target Menu
echo "1) starlte      2) star2lte      3) crownlte"
echo "4) starltekor   5) star2ltekor   6) crownltekor"
echo "7) Build All/ZIP"
echo "8) Build All/ZIP Combinations"
echo "9) Abort"
echo "----------------------------------------------"
read -p "Please select your build target (1-9) > " CR_TARGET
echo "----------------------------------------------"

# Abort
if [ "$CR_TARGET" = "9" ]; then
    echo "Build Aborted"
    exit 0
fi

# Compiler & Options (only if not option 8)
if [ "$CR_TARGET" != "8" ]; then
    echo ""
    echo "1) Clang 12 (LLVM +LTO)"
    echo "2) Clang 14 (LLVM +LTO)"
    echo "3) [Default] Clang 18 (LLVM +LTO PGO Bolt Mlgo Poly)"
    echo "4) Clang 19 (^)"
    echo "5) Neutron Clang 18 (^)"
    echo "6) Neutron Clang 19 (^)"
    echo "7) Other (Apollo/toolchain/clang-custom)"
    echo ""
    read -p "Please select your compiler (1-7) > " CR_COMPILER

    echo ""
    echo "1) Selinux Permissive   [Default] 2) Selinux Enforcing"
    echo ""
    read -p "Please select your SElinux mode (1-2) > " CR_SELINUX

    echo ""
    read -p "Enable KernelSU? (y/n) > " CR_KSU
    if [[ "$CR_KSU" =~ ^[yY]$ ]]; then
        read -p "Enable KSU + SuSFS? (y/n) > " CR_SUS
    fi
    echo ""
fi

# Clean Builds
read -p "Clean Builds? (y/n) > " CR_CLEAN
echo ""

# Validation
if ! [[ "$CR_TARGET" =~ ^[1-8]$ ]]; then
    CR_TARGET=$DEFAULT_TARGET
    echo "No valid target selected, defaulting to star2ltekor"
fi

# Default Entry
if ! [[ "$CR_COMPILER" =~ ^[1-7]$ ]]; then CR_COMPILER=$DEFAULT_COMPILER; fi
if ! [[ "$CR_SELINUX" =~ ^[1-2]$ ]]; then CR_SELINUX=$DEFAULT_SELINUX; fi
if ! [[ "$CR_KSU" =~ ^[yYnN]$ ]]; then CR_KSU=$DEFAULT_KSU; fi
if ! [[ "$CR_SUS" =~ ^[yYnN]$ ]]; then CR_SUS=$DEFAULT_SUS; fi
if ! [[ "$CR_CLEAN" =~ ^[yYnN]$ ]]; then CR_CLEAN=$DEFAULT_CLEAN; fi

# Build Logic
if [ "$CR_TARGET" = "8" ]; then
    echo ""
    read -p "Build Flashable ZIP Combinations? (y/n) (n to build .imgs)> " CR_MKZIP
    echo ""
    BUILD_ALL_COMBINATIONS
    echo "All combinations compiled."
    LIST_COMPILED_ZIPS
    COMPILED_ZIPS=""
elif [ "$CR_TARGET" = "7" ]; then
    echo ""
    read -p "Build Flashable ZIP? (y/n) (n to build .imgs)> " CR_MKZIP
    echo ""
    BUILD_ALL
else
    BUILD
fi
