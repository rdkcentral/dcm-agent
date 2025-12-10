#!/bin/bash
#
# Build and run script for retry_logic_gtest
#
# This script demonstrates how to compile and run the retry logic unit tests
# using the Google Test framework in a typical RDK environment.
#

set -e

echo "Building retry_logic_gtest..."

# Set up environment variables (adjust paths as needed for your environment)
export PKG_CONFIG_SYSROOT_DIR=${PKG_CONFIG_SYSROOT_DIR:-/}
export GTEST_ROOT=${GTEST_ROOT:-/usr}
export COMMON_UTILS_PATH=${COMMON_UTILS_PATH:-../../common_utilities}

# Common compiler flags
CPPFLAGS="-std=c++11 -I. -I../include -I../src -I./mocks"
CPPFLAGS="${CPPFLAGS} -I${GTEST_ROOT}/include -I${COMMON_UTILS_PATH}/utils"
CPPFLAGS="${CPPFLAGS} -I${COMMON_UTILS_PATH}/parsejson -I${COMMON_UTILS_PATH}/dwnlutils"
CPPFLAGS="${CPPFLAGS} -I${COMMON_UTILS_PATH}/uploadutil"
CPPFLAGS="${CPPFLAGS} -DGTEST_ENABLE -DGTEST_BASIC"

# Compiler and linker flags
CXXFLAGS="-frtti -fprofile-arcs -ftest-coverage -fpermissive -Wno-write-strings -Wno-unused-result"
LDFLAGS="-lgtest -lgmock -lpthread -lcurl -lcjson -lssl -lcrypto -lgcov -lz"

# Additional libraries (adjust for your RDK environment)
LDFLAGS="${LDFLAGS} -lrbus -lfwutils -lrdkloggers"

echo "Compiling retry_logic_gtest.cpp..."

# Compile the test
g++ ${CPPFLAGS} ${CXXFLAGS} -o retry_logic_gtest retry_logic_gtest.cpp ${LDFLAGS}

echo "Build completed successfully!"
echo ""
echo "Running retry_logic_gtest..."

# Run the test
./retry_logic_gtest

echo ""
echo "Test execution completed!"
echo ""
echo "For integration with autotools, use:"
echo "  autoreconf -fiv"
echo "  ./configure"
echo "  make check"