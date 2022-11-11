FROM tensorflow/tensorflow:custom-op-ubuntu16

ARG USERNAME=chfs
ARG UID=1000

SHELL ["/bin/bash", "-c"]

RUN rm -f /etc/apt/sources.list.d/jonathonf-ubuntu-python-3_7-xenial.list && apt-get update && \
    apt-get install -y \
    git \
    gdb \
    gcc \
    g++ \
    make \
    patch \
    curl \
    nano \
    unzip \
    ffmpeg \
    dnsutils \
    automake \
    cmake \
    libtool \
    pkgconf \
    bison \
    fuse \
    libfuse-dev \
    sudo \
    software-properties-common

RUN cd /usr/lib/python3/dist-packages && \
    ln -s apt_pkg.cpython-{35m,36m}-x86_64-linux-gnu.so && \
    add-apt-repository ppa:ubuntu-toolchain-r/test -y && apt update -y && \
    apt-get install gcc-9 g++-9 -y && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 \
        --slave /usr/bin/g++ g++ /usr/bin/g++-9 && \
    update-alternatives --config gcc

RUN useradd -m -u $UID -s /bin/bash $USERNAME \
    && echo "$USERNAME ALL=(ALL:ALL) NOPASSWD: ALL" >> /etc/sudoers.d/$USERNAME \
    # delete passwd
    && passwd -d $USERNAME

USER root
RUN curl -sSOL https://github.com/bazelbuild/bazelisk/releases/download/v1.11.0/bazelisk-linux-amd64 && \
    mv bazelisk-linux-amd64 /usr/local/bin/bazel && \
    chmod +x /usr/local/bin/bazel

ARG CONDA_OS=Linux

# Miniconda - Python 3.7, 64-bit, x86, latest
RUN curl -sL https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh -o mconda-install.sh && \
    bash -x mconda-install.sh -b -p miniconda && \
    rm mconda-install.sh

USER $USERNAME

ENV PATH "/miniconda/bin:$PATH"

ARG CONDA_ADD_PACKAGES=""

RUN conda create -y -q -n tfio-dev python=3.7 ${CONDA_ADD_PACKAGES}

ARG ARROW_VERSION=0.16.0

RUN echo ". /miniconda/etc/profile.d/conda.sh" >> ~/.bashrc && \
    echo "source activate tfio-dev" >> ~/.bashrc

RUN cd $HOME && \
    git clone -c feature.manyFiles=true --depth 1 https://github.com/spack/spack.git && \
    . $HOME/spack/share/spack/setup-env.sh && \
    spack external find automake autoconf libtool cmake m4 pkgconf bison && \
    git clone https://github.com/range3/chfs-spack-packages.git && \
    spack repo add chfs-spack-packages && \
    spack env create chfs && \
    spack env activate chfs && \
    spack add mochi-margo ^mercury~boostsys ^libfabric fabrics=sockets,tcp,udp && \
    spack add chfs~pmemkv && \
    spack install

RUN echo ". $HOME/spack/share/spack/setup-env.sh" >> ~/.bashrc
RUN echo "spack env activate chfs" >> ~/.bashrc
RUN echo "spack load" >> ~/.bashrc

ARG PIP_ADD_PACKAGES="pytest tensorflow"

RUN /bin/bash -c "source activate tfio-dev && python -m pip install \
    avro-python3 \
    pytest \
    pytest-benchmark \
    pylint \
    boto3 \
    google-cloud-pubsub==0.39.1 \
    google-cloud-bigquery-storage==1.1.0 \
    pyarrow==${ARROW_VERSION} \
    pandas \
    fastavro \
    gast==0.2.2 \
    ${PIP_ADD_PACKAGES} \
    "

ENV TFIO_DATAPATH bazel-bin
ENV TF_IO_CHFS_LIBRARY_DIR "$HOME/local/lib"
