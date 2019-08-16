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
export VERSION=1.12 OS=linux ARCH=amd64 && \  # Replace the values as needed
  wget https://dl.google.com/go/go$VERSION.$OS-$ARCH.tar.gz && \ # Downloads the required Go package
  sudo tar -C /usr/local -xzvf go$VERSION.$OS-$ARCH.tar.gz && \ # Extracts the archive
  rm go$VERSION.$OS-$ARCH.tar.gz    # Deletes the ``tar`` file

echo 'export PATH=/usr/local/go/bin:$PATH' >> /etc/profile.d/go.sh && source /etc/profile.d/go.sh

# Install Singularity
export VERSION=3.3.0 && # adjust this as necessary \
    wget https://github.com/sylabs/singularity/releases/download/v${VERSION}/singularity-${VERSION}.tar.gz && \
    tar -xzf singularity-${VERSION}.tar.gz && \
    cd singularity

./mconfig && \
    make -C builddir && \
    sudo make -C builddir install

# Check Python
echo "Python Version:"
python --version
pip install sregistry[all]
sregistry version


