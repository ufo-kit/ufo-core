# Initial docker draft by Harinarayan Krishnan <hkrishnan@lbl.gov>

FROM nvidia/cuda:8.0-devel-ubuntu16.04

RUN apt-get update && apt-get install -y --no-install-recommends --force-yes \
        git \
        gcc \
        g++ \
        gobject-introspection \
        make \
        ca-certificates \
        cmake \
        liblapack-dev \
        libjpeg-dev \
        libtiff-dev \
        libglib2.0-dev \
        libjson-glib-dev \
        libopenmpi-dev \
        libhdf5-dev \
        libclfft-dev \
        libgsl-dev \
        libgirepository1.0-dev \
        python \
        python-dev \
        python-gobject \
        python-pip \
        python-numpy \
        python-tifffile \
        pkg-config \
        ocl-icd-opencl-dev \
        fftw3-dev \
        clinfo \
        zlib1g-dev && \
        rm -rf /var/lib/apt/lists/*

ENV LD_LIBRARY_PATH /usr/local/lib/:${LD_LIBRARY_PATH}

RUN mkdir -p /etc/OpenCL/vendors && \
    echo "libnvidia-opencl.so.1" > /etc/OpenCL/vendors/nvidia.icd

RUN rm /usr/local/cuda-8.0/targets/x86_64-linux/lib/libOpenCL.so*

RUN git clone https://github.com/ufo-kit/ufo-core.git && \
    git clone https://github.com/ufo-kit/ufo-filters.git && \
    git clone https://github.com/ufo-kit/tofu

RUN pip install --upgrade pip && pip install setuptools

RUN cd /ufo-core && mkdir build && cd build && cmake -DCMAKE_INSTALL_PREFIX=/usr .. && make install 
RUN cd /ufo-filters && mkdir build && cd build && cmake ..  -DCMAKE_INSTALL_PREFIX=/usr && make install
RUN cd /tofu && python setup.py install
