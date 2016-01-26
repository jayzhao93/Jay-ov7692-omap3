export KDIR=/usr/src/kernel
make -C $KDIR M=$PWD
make -C $KDIR M=$PWD modules_install
