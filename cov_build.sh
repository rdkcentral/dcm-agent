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

apt-get update
apt-get install -y libdbus-1-dev

apt-get install -y libglib2.0-dev
apt-get install -y libcjson-dev
cd $INSTALL_DIR
apt-get install -y libdirectfb-dev

ls -l /usr/directfb/

ls -l /usr/include/directfb
#mv /usr/include/directfb /usr/local/include/
ls -l /usr/ssa-cpc/safec_lib/include/libsafec/safec_lib.h

cd $ROOT
#Build rbus

git clone https://github.com/rdkcentral/rbus
cmake -Hrbus -Bbuild/rbus -DBUILD_FOR_DESKTOP=ON -DCMAKE_BUILD_TYPE=Debug
make -C build/rbus && make -C build/rbus install



cd $ROOT

git clone https://mtirum011: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@github.com/rdk-e/ssa-cpc.git

git clone https://mtirum011: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@github.com/rurban/safeclib

cd safeclib

autoreconf -i
./configure  && make && make install

git clone https://mtirum011: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@github.com/djbclark/directfb-core-DirectFB.git

cp /usr/directfb-core-DirectFB/lib/fusiondale/fusiondale.h /usr/local/include/directfb

ls -l /usr/ssa-cpc/safec_lib/include/libsafec/safec_lib.h /usr/iarmbus/core 
#cd ${ROOT}/safeclib

#autoreconf --install
#./configure --prefix=${INSTALL_DIR} && make && make install


git clone https://mtirum011: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@github.com/rdk-e/iarmbus.git

cd ${ROOT}/iarmbus

autoupdate
autoreconf -fi
sed -i '/CreateComponent/ s/handler/(ComaMethodFunc) handler/' core/libIARM.c
sed -i '/IARM_CallReturn/ s/retCode/(void*) retCode/' core/libIBus.c

sed -i 's/\(void \*\)(.*), unsigned int/\1, void*/' core/libIBus.c
sed -i 's/IARM_Bus_Member_t member;/IARM_Bus_Member_t* member;/' core/libIBus.c

#cp /usr/ssa-cpc/safec_lib/include/libsafec/safec_lib.h /usr/iarmbus/core/

#cp /usr/ssa-cpc/safec_lib/include/libsafec/safe_str_lib.h /usr/iarmbus/core/

#cp /usr/ssa-cpc/safec_lib/include/libsafec/safe_config.h /usr/iarmbus/core/

#mv /usr/ssa-cpc/safec_lib /usr/local/include/
#cp /usr/include/glib-2.0/glib.h /usr/iarmbus/core
#cp -r /usr/include/glib-2.0/glib /usr/iarmbus/core
#cp /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h /usr/iarmbus/core

#cp /usr/ssa-cpc/safec_lib/include/libsafec/safec_lib.h /usr/iarmbus/core

#export CFLAGS="-I/usr/ssa-cpc/safec_lib/include/libsafec/ -I/usr/include/glib-2.0/ -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/ -I/usr/include/dbus-1.0/ -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include/ -I/usr//ssa-cpc/safec_lib/include/libsafec/ -I/usr/directfb-core-DirectFB/lib/fusiondale/"

export LDFLAGS="-lsafec"
./configure 
#CFLAGS="-I${ROOT}/ssa-cpc/safec_lib/include/libsafec/ -I${ROOT}/include/glib-2.0/ -I${ROOT}/lib/x86_64-linux-gnu/glib-2.0/include/ -I${ROOT}/include/dbus-1.0/ -I${ROOT}/lib/x86_64-linux-gnu/dbus-1.0/include/ -I${ROOT}/ssa-cpc/safec_lib/include/libsafec/ -I/usr/directfb-core-DirectFB/lib/fusiondale/ -I/usr/include/directfb"

make CFLAGS="-I${ROOT}/ssa-cpc/safec_lib/include/libsafec/ -I${ROOT}/include/glib-2.0/ -I${ROOT}/lib/x86_64-linux-gnu/glib-2.0/include/ -I${ROOT}/include/dbus-1.0/ -I${ROOT}/lib/x86_64-linux-gnu/dbus-1.0/include/ -I${ROOT}/ssa-cpc/safec_lib/include/libsafec/ -I/usr/directfb-core-DirectFB/lib/fusiondale/ -I${ROOT}/include/directfb/ -I./include/ " && make install

#cd ${ROOT}
#git clone https://mtirum011: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@github.com/rdk-e/iarmmgrs.git

#cd ${ROOT}/iarmmgrs/sysmgr

#autoreconf --install
#./configure --prefix=${INSTALL_DIR} 

#make && make install



cd $WORKDIR

export INSTALL_DIR='/usr/local'
export top_srcdir=`pwd`
export top_builddir=`pwd`

autoreconf --install

export CFLAGS="-I${ROOT}/build/rbus/deps/src/"
./configure --prefix=${INSTALL_DIR} 
make && make install

