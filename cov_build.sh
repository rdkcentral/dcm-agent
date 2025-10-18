##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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
##########################################################################


WORKDIR=`pwd`

## Build and install critical dependency
export ROOT=/usr
export INSTALL_DIR=${ROOT}/local
mkdir -p $INSTALL_DIR

cd $ROOT
#Build rbus

git clone https://github.com/rdkcentral/rbus
cmake -Hrbus -Bbuild/rbus -DBUILD_FOR_DESKTOP=ON -DCMAKE_BUILD_TYPE=Debug
make -C build/rbus && make -C build/rbus install

cd $WORKDIR

export INSTALL_DIR='/usr/local'
export top_srcdir=`pwd`
export top_builddir=`pwd`

autoreconf --install

cd ${ROOT}
rm -rf iarmmgrs
git clone https://github.com/rdkcentral/iarmmgrs.git


cd ${ROOT}
find -iname rdk_debug.h
cat /usr/local/include/rdk_debug.h
rm -rf telemetry
git clone https://github.com/rdkcentral/telemetry.git
cd telemetry
sh  build_inside_container.sh 


cd $WORKDIR
./configure --prefix=${INSTALL_DIR} CFLAGS="-DRDK_LOGGER -DHAS_MAINTENANCE_MANAGER -I$ROOT/iarmmgrs/maintenance/include"
make && make install
