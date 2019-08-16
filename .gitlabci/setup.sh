#!/bin/bash

apt-get update && apt-get install -y wget git \
                                          build-essential \
                                          squashfs-tools \
                                          libtool \
                                          autotools-dev \
                                          libarchive-dev \
                                          automake \
                                          autoconf \
                                          uuid-dev \
                                          libssl-dev


sed -i -e 's/^Defaults\tsecure_path.*$//' /etc/sudoers

# Install go
export VERSION=1.12 OS=linux ARCH=amd64 && \
  wget https://dl.google.com/go/go$VERSION.$OS-$ARCH.tar.gz && \
  sudo tar -C /usr/local -xzvf go$VERSION.$OS-$ARCH.tar.gz && \
  rm go$VERSION.$OS-$ARCH.tar.gz  

echo 'export PATH=/usr/local/go/bin:$PATH' >> /etc/profile.d/go.sh && source /etc/profile.d/go.sh

export PATH=/usr/local/go/bin:$PATH

# Install Singularity
export VERSION=3.3.0 && # adjust this as necessary \
    wget https://github.com/sylabs/singularity/releases/download/v${VERSION}/singularity-${VERSION}.tar.gz && \
    tar -xzf singularity-${VERSION}.tar.gz && \
    cd singularity

./mconfig && \
    make -C builddir && \
    sudo make -C builddir install

#j# Check Python
#jecho "Python Version:"
#jpython --version
#jpip install sregistry[all]
#jsregistry version


