FROM ubuntu:20.04
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get -y upgrade && apt-get install -y \
        apt-utils \
        build-essential \
        meson \
        ninja-build \
        git \
        gcc \
        g++ \
        gobject-introspection \
        make \
        ca-certificates \
        cmake \
        liblapack-dev \
        libjpeg-dev \
        libtiff5-dev \
        libglib2.0-dev \
        libjson-glib-dev \
        libopenmpi-dev \
        libhdf5-dev \
        libclfft-dev \
        libgsl-dev \
        libgirepository1.0-dev \
        qt5-default \
        python3 \
        python3-dev \
        python3-gi \
        python-gi-dev \
        python3-sphinx \
        python3-pip \
        python3-numpy \
        python3-cairo \
        python3-gi-cairo \
        python3-sphinx \
        python3-pyqt5 \
        python3-pyqtgraph \
        pkg-config \
        ocl-icd-libopencl1 \
        ocl-icd-opencl-dev \
        fftw3-dev \
        opencl-headers \
        clinfo \
        zlib1g-dev && \
        rm -rf /var/lib/apt/lists/*

ENV LD_LIBRARY_PATH /usr/local/lib/:${LD_LIBRARY_PATH}
ENV GI_TYPELIB_PATH=/usr/local/lib/girepository-1.0:$GI_TYPELIB_PATH
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

RUN mkdir -p /etc/OpenCL/vendors && \
    echo "libnvidia-opencl.so.1" > /etc/OpenCL/vendors/nvidia.icd
ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility

RUN git clone https://github.com/ufo-kit/ufo-core.git && \
    git clone https://github.com/ufo-kit/ufo-filters.git && \
    git clone https://github.com/ufo-kit/tofu

RUN pip3 install --upgrade pip && pip3 install setuptools
RUN pip3 install tifffile

RUN cd /ufo-core && meson build --libdir=lib -Dbashcompletiondir=$HOME/.local/share/bash-completion/completions && cd build && ninja install
RUN cd /ufo-core/python && python3 setup.py install
RUN cd /ufo-filters && meson build --libdir=lib -Dcontrib_filters=True && cd build && ninja install
RUN cd /tofu && pip3 install -r requirements-flow.txt && pip3 install -r requirements-flow.txt && python3 setup.py install
RUN rm -rf /ufo-core /ufo-filters /tofu
