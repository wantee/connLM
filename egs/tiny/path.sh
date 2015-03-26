
export PATH=$PWD/../../bin:$PWD:$PATH
export PATH=$PWD/../../bin:$PWD:$PATH
if [ `uname` ==  "Darwin" ]; then
export DYLD_LIBRARY_PATH=$PWD/../../lib:$DYLD_LIBRARY_PATH
else
export LD_LIBRARY_PATH=$PWD/../../lib:$LD_LIBRARY_PATH
fi

export LC_ALL=C

