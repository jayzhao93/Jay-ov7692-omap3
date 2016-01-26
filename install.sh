export KDIR=/home/adam/Code/linux
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C $KDIR M=$PWD
sudo make INSTALL_MOD_PATH=/media/adam/root -C $KDIR M=/home/adam/Code/ov7692 modules_install
