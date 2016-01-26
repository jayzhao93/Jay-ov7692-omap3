#!/bin/bash
cd ..
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C $KDIR M=$PWD
sudo make INSTALL_MOD_PATH=/media/$USER/root -C $KDIR M=$PWD modules_install
