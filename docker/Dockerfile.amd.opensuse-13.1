FROM opensuse:13.1

RUN zypper --non-interactive --gpg-auto-import-keys ref

RUN zypper in -y gcc gcc-c++ glib2-devel json-glib-devel
RUN zypper in -y gobject-introspection-devel
RUN zypper in -y gtk-doc python-Sphinx
RUN zypper in -y xorg-x11-driver-video
RUN zypper in -y libtiff-devel
RUN zypper in -y lam lam-devel
RUN zypper in -y python-devel
RUN zypper in -y zeromq-devel
RUN zypper in -y lsb-release
RUN zypper in -y rpm-devel
RUN zypper in -y cmake
RUN zypper in -y make
RUN zypper in -y wget
RUN zypper in -y git
RUN zypper in -y vim

RUN cd tmp && wget http://geeko.ioda.net/mirror/amd-fglrx/raw-src/amd-driver-installer-15.20.1046-x86.x86_64.run --no-check-certificate && sh amd-driver-installer-15.20.1046-x86.x86_64.run --buildpkg SuSE/SUSE-autodetection && zypper in -y fglrx*15.20.1046*.rpm

ADD AMD-APP-SDK-v3.0-0.113.50-Beta-linux64.tar.bz2 /tmp/
ADD answers /tmp/
RUN cd /tmp && cat answers | ./AMD-APP-SDK-v3.0-0.113.50-Beta-linux64.sh
RUN cd /usr/include && mkdir /usr/include/CL && cd /usr/include/CL && wget https://www.khronos.org/registry/cl/api/1.2/{opencl,cl_platform,cl,cl_ext,cl_gl,cl_gl_ext}.h --no-check-certificate

RUN GIT_SSL_NO_VERIFY=true git clone https://github.com/ufo-kit/ufo-core
RUN GIT_SSL_NO_VERIFY=true git clone https://github.com/ufo-kit/ufo-filters
RUN GIT_SSL_NO_VERIFY=true git clone https://github.com/ufo-kit/ufo-ir.git

RUN cd /ufo-core && mkdir build && cd build && export AMDAPPSDKROOT=/usr/AMDAPPSDK-3.0-0-Beta && export LD_LIBRARY_PATH="/usr/AMDAPPSDK-3.0-0-Beta/lib/x86_64/" && export OPENCL_VENDOR_PATH="/usr/AMDAPPSDK-3.0-0-Beta/etc/OpenCL/vendors/" && cmake -DLIBDIR=/usr/lib64 /ufo-core && make && make install && mkdir ufo-filters

RUN cd /ufo-ir && mkdir build && cd build && export AMDAPPSDKROOT=/usr/AMDAPPSDK-3.0-0-Beta && export LD_LIBRARY_PATH="/usr/AMDAPPSDK-3.0-0-Beta/lib/x86_64/" && export OPENCL_VENDOR_PATH="/usr/AMDAPPSDK-3.0-0-Beta/etc/OpenCL/vendors/" && cmake -DLIBDIR=/usr/lib64 /ufo-ir && make && make install

RUN cd /ufo-core/build/ufo-filters && export AMDAPPSDKROOT=/usr/AMDAPPSDK-3.0-0-Beta && export LD_LIBRARY_PATH="/usr/AMDAPPSDK-3.0-0-Beta/lib/x86_64/" && export OPENCL_VENDOR_PATH="/usr/AMDAPPSDK-3.0-0-Beta/etc/OpenCL/vendors/" && cmake /ufo-filters -DLIBDIR=/usr/lib64 -DPREFIX=/usr && make && make install
