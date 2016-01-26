#!/bin/bash
export KDIR=/usr/src/kernel
cd ..
make -C $KDIR M=$PWD
make -C $KDIR M=$PWD modules_install
