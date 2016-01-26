modprobe omap3-isp dyndbg=+pt

sleep 1

media-ctl -v -r -l '"OV7692":0->"OMAP3 ISP CCDC":0[1],
		 "OMAP3 ISP CCDC":2->"OMAP3 ISP preview":0[1],
		 "OMAP3 ISP preview":1->"OMAP3 ISP preview output":0[1]'

sleep 1

media-ctl -V '"OV7692":0[SBGGR8 640x480],
	      "OMAP3 ISP CCDC":2[SBGGR8 640x480],
	      "OMAP3 ISP preview":1[UYVY 622x471]'

sleep 1

echo 'yavta -c1 -n1 -s622x471 -fUYVY --file=image-#.uyvy  /dev/video4'
echo 'use the following to view the image on your workstation:'
echo 'display -size 622x472 -colorspace rgb image-000000.uyvy'
