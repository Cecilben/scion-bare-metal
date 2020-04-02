#!/bin/bash
set -e
patch_dir=$PWD

# Upgrade pip and setuptools
sudo pip2 install --upgrade pip setuptools
sudo pip3 install --upgrade pip setuptools

# Install Python 3.5.2 (the default Python 3.4.2 is too old)
sudo apt-get -y install curl
curl -O https://www.python.org/ftp/python/3.5.2/Python-3.5.2.tar.xz
tar xf Python-3.5.2.tar.xz
cd Python-3.5.2
sudo apt-get -y install libssl-dev libsqlite3-dev # needed to build pip and the sqlite module
./configure --with-ensurepip=install
make -j4
sudo make altinstall
cd ..

# Upgrade Python 3.5 pip and setuptools
sudo pip3.5 install --upgrade pip setuptools

# Set Python 3.5.2 as the new default
sudo rm -f /usr/local/bin/pip3
sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.4 10 --slave /usr/local/bin/pip3 pip3 /usr/local/bin/pip3.4
sudo update-alternatives --install /usr/bin/python3 python3 /usr/local/bin/python3.5 10 --slave /usr/local/bin/pip3 pip3 /usr/local/bin/pip3.5
sudo update-alternatives --set python3 /usr/local/bin/python3.5

# Install capnproto and libcapnp-dev from jessie-backports
sudo patch /etc/apt/sources.list $patch_dir/sources.list.patch
sudo apt-get update
sudo apt-get -y -t jessie-backports install capnproto libcapnp-dev

# Install SCION (https://netsec-ethz.github.io/scion-tutorials/native_setup/ubuntu_x86_build/)
# Configure go workspace
echo 'export GOPATH="$HOME/go"' >> ~/.profile
source ~/.profile
mkdir -p "$GOPATH/bin"
# Clone the SCION repository
mkdir -p "$GOPATH/src/github.com/scionproto/scion"
cd "$GOPATH/src/github.com/scionproto/scion"
git config --global url.https://github.com/.insteadOf git@github.com:
git clone --recursive -b scionlab git@github.com:netsec-ethz/netsec-scion .
echo 'export SC="$GOPATH/src/github.com/scionproto/scion"' >> ~/.profile
# Configure Python path variable
echo 'export PYTHONPATH="$SC/python:$SC"' >> ~/.profile

# Update path variable
echo 'PATH=$PATH:$GOPATH/bin' >> ~/.profile
echo 'PATH=$PATH:~/.local/bin' >> ~/.profile # for capnpc-cython
echo 'PATH=$PATH:/usr/local/go/bin' >> ~/.profile
source ~/.profile

cd $SC

# Remove flake8, python3-lz4, python3-nacl and python3-nose-cov from env/debian/pkgs.txt
git apply $patch_dir/pkgs.txt.patch

# Patch the Python requirements files
# Adds lz4, PyNaCl, nose-cov, flake8 to Python dependencies
# flake8 2.5.4 and pyflakes 1.1.0 seem to be incompatible:
# flake8 2.5.4 has requirement pyflakes<1.1,>=0.8.1, but you'll have pyflakes 1.1.0 which is incompatible.
cp $patch_dir/pip2_requirements.txt env/pip2/requirements.txt
cp $patch_dir/pip3_requirements.txt env/pip3/requirements.txt

# requests must be installed before installing sphinxcontrib-napoleon is attempted
sudo pip3 install requests==2.9.1

# Invoke install script
bash -c 'yes | GO_INSTALL=true ./env/deps'

# Continue as in scion_install_script.sh
# https://raw.githubusercontent.com/netsec-ethz/scion-coord/master/scion_install_script.sh
