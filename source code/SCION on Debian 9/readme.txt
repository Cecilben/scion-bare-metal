Patch for SCION dependencies on ONL (Debian 9)

Steps to install SCION on Debian 9:
1. Follow the manual instructions for Ubuntu 16.04[1] to set up environment variables and
   clone the SCION repository. The necessary commands can also be copied from the automatic
   install script scion_install_script.sh[2]:

echo 'export GOPATH="$HOME/go"' >> ~/.profile
echo 'export PATH="$HOME/.local/bin:$GOPATH/bin:/usr/local/go/bin:$PATH"' >> ~/.profile
echo 'export SC="$GOPATH/src/github.com/scionproto/scion"' >> ~/.profile
echo 'export PYTHONPATH="$SC/python:$SC"' >> ~/.profile
source ~/.profile
mkdir -p "$GOPATH"
mkdir -p "$GOPATH/src/github.com/scionproto"
cd "$GOPATH/src/github.com/scionproto"

git config --global url.https://github.com/.insteadOf git@github.com:
git clone --recursive -b scionlab git@github.com:netsec-ethz/netsec-scion scion

cd scion

2. Before running "bash -c 'yes | GO_INSTALL=true ./env/deps'" to install the dependencies,
   apply the patch:
git apply deps.patch

3. Run "bash -c 'yes | GO_INSTALL=true ./env/deps'"
4. Continue with the manual installation.

[1] https://netsec-ethz.github.io/scion-tutorials/native_setup/ubuntu_x86_build/
[2] https://raw.githubusercontent.com/netsec-ethz/scion-coord/master/scion_install_script.sh
