#!/bin/sh

#  build_artifacts.sh
#  OpenEars
#
#  Created by Vyacheslav Gorlov on 7/19/21.
#  Copyright © 2021 Politepix. All rights reserved.

set -euo pipefail;

ARCHIVES_BASE_DIRECTORY="${TMPDIR}CSOpenEars"
echo "${ARCHIVES_BASE_DIRECTORY}"
rm -rf "${ARCHIVES_BASE_DIRECTORY}"

# Open Ears
OPENEARS_CATALYST_BUILD_DIRECTORY="${ARCHIVES_BASE_DIRECTORY}/openears-maccatalyst"

xcodebuild \
clean \
-sdk "macosx" \
-scheme "OpenEars" \
-configuration Debug \
build \
ENABLE_BITCODE=NO \
STRIP_SWIFT_SYMBOLS=NO \
RUN_CLANG_STATIC_ANALYZER=0 \
SUPPORTS_MACCATALYST=YES \
CONFIGURATION_BUILD_DIR="${OPENEARS_CATALYST_BUILD_DIRECTORY}"


OPENEARS_IOS_BUILD_DIRECTORY="${ARCHIVES_BASE_DIRECTORY}/openears-iphoneos"

xcodebuild \
clean \
-sdk "iphoneos" \
-scheme "OpenEars" \
-configuration Debug \
build \
ENABLE_BITCODE=NO \
STRIP_SWIFT_SYMBOLS=NO \
RUN_CLANG_STATIC_ANALYZER=0 \
CONFIGURATION_BUILD_DIR="${OPENEARS_IOS_BUILD_DIRECTORY}"

OPENEARS_SIMULATOR_BUILD_DIRECTORY="${ARCHIVES_BASE_DIRECTORY}/openears-iphonesimulator"

xcodebuild \
clean \
-sdk "iphonesimulator" \
-scheme "OpenEars" \
-configuration Debug \
build \
ENABLE_BITCODE=NO \
STRIP_SWIFT_SYMBOLS=NO \
RUN_CLANG_STATIC_ANALYZER=0 \
CONFIGURATION_BUILD_DIR="${OPENEARS_SIMULATOR_BUILD_DIRECTORY}"

xcodebuild -create-xcframework -output "${ARCHIVES_BASE_DIRECTORY}/OpenEars.xcframework" \
-framework "${OPENEARS_CATALYST_BUILD_DIRECTORY}/OpenEars.framework" \
-framework "${OPENEARS_IOS_BUILD_DIRECTORY}/OpenEars.framework" \
-framework "${OPENEARS_SIMULATOR_BUILD_DIRECTORY}/OpenEars.framework"

# Slt
SLT_CATALYST_BUILD_DIRECTORY="${ARCHIVES_BASE_DIRECTORY}/slt-maccatalyst"

xcodebuild \
clean \
-sdk "macosx" \
-scheme "Slt" \
-configuration Debug \
build \
ENABLE_BITCODE=NO \
STRIP_SWIFT_SYMBOLS=NO \
RUN_CLANG_STATIC_ANALYZER=0 \
SUPPORTS_MACCATALYST=YES \
CONFIGURATION_BUILD_DIR="${SLT_CATALYST_BUILD_DIRECTORY}"

SLT_IOS_BUILD_DIRECTORY="${ARCHIVES_BASE_DIRECTORY}/slt-iphoneos"

xcodebuild \
clean \
-sdk "iphoneos" \
-scheme "Slt" \
-configuration Debug \
build \
ENABLE_BITCODE=NO \
STRIP_SWIFT_SYMBOLS=NO \
RUN_CLANG_STATIC_ANALYZER=0 \
CONFIGURATION_BUILD_DIR="${SLT_IOS_BUILD_DIRECTORY}"

SLT_SIMULATOR_BUILD_DIRECTORY="${ARCHIVES_BASE_DIRECTORY}/slt-iphonesimulator"

xcodebuild \
clean \
-sdk "iphonesimulator" \
-scheme "Slt" \
-configuration Debug \
build \
ENABLE_BITCODE=NO \
STRIP_SWIFT_SYMBOLS=NO \
RUN_CLANG_STATIC_ANALYZER=0 \
CONFIGURATION_BUILD_DIR="${SLT_SIMULATOR_BUILD_DIRECTORY}"

xcodebuild -create-xcframework -output "${ARCHIVES_BASE_DIRECTORY}/Slt.xcframework" \
-framework "${SLT_CATALYST_BUILD_DIRECTORY}/Slt.framework" \
-framework "${SLT_IOS_BUILD_DIRECTORY}/Slt.framework" \
-framework "${SLT_SIMULATOR_BUILD_DIRECTORY}/Slt.framework"

# Finish
open "${ARCHIVES_BASE_DIRECTORY}"
