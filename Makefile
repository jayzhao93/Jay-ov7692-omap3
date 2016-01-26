default:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -C $(KDIR) M=$$PWD

genbin:
	echo "X" > ov7692.o_shipped
