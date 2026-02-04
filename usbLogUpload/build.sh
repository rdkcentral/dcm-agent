#!/bin/bash

##############################################################################
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##############################################################################

# Build script for USB Log Upload module

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -c, --clean     Clean build artifacts"
    echo "  -d, --debug     Build with debug symbols"
    echo "  -t, --test      Build and run tests"
    echo "  -i, --install   Install the built binary"
    echo "  -h, --help      Show this help message"
}

# Default options
CLEAN=false
DEBUG=false
RUN_TESTS=false
INSTALL=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -d|--debug)
            DEBUG=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    print_status "Cleaning build artifacts..."
    make clean 2>/dev/null || true
    rm -rf obj bin config.h config.status config.log Makefile.real 2>/dev/null || true
    print_status "Clean completed"
fi

# Check if we have autotools
if command -v autoreconf &> /dev/null; then
    print_status "Using autotools build system..."
    
    # Generate configure script if it doesn't exist
    if [ ! -f configure ]; then
        print_status "Generating configure script..."
        autoreconf -fiv
    fi
    
    # Configure with appropriate options
    CONFIGURE_OPTS=""
    if [ "$DEBUG" = true ]; then
        CONFIGURE_OPTS="--enable-debug"
        print_status "Configuring with debug enabled..."
    else
        print_status "Configuring for release build..."
    fi
    
    ./configure $CONFIGURE_OPTS
    
    # Build using automake
    print_status "Building with automake..."
    make
    
else
    print_warning "Autotools not found, using direct Makefile..."
    
    # Use direct Makefile
    MAKE_OPTS=""
    if [ "$DEBUG" = true ]; then
        MAKE_OPTS="DEBUG=1"
        print_status "Building with debug enabled..."
    else
        print_status "Building release version..."
    fi
    
    make $MAKE_OPTS all
fi

# Run tests if requested
if [ "$RUN_TESTS" = true ]; then
    print_status "Building and running tests..."
    
    if [ -f Makefile ]; then
        make test
        
        # Run the test executable if it exists
        if [ -f bin/test_usblogupload ]; then
            print_status "Running integration tests..."
            ./bin/test_usblogupload
        fi
        
        # Run GTest unit tests if available
        if [ -d unittest ] && command -v g++ &> /dev/null; then
            print_status "Building and running unit tests..."
            cd unittest
            # Note: This would require a proper unittest Makefile
            print_warning "Unit test build system not yet implemented"
            cd ..
        fi
    else
        print_error "No Makefile found for testing"
        exit 1
    fi
fi

# Install if requested
if [ "$INSTALL" = true ]; then
    print_status "Installing..."
    
    if [ -f bin/usblogupload ]; then
        sudo make install
        print_status "Installation completed"
    else
        print_error "Binary not found. Build first."
        exit 1
    fi
fi

print_status "Build script completed successfully!"

if [ -f bin/usblogupload ]; then
    print_status "Binary available at: bin/usblogupload"
    print_status "Usage: ./bin/usblogupload <USB_mount_point>"
else
    print_warning "Binary not found. Check build output for errors."
fi