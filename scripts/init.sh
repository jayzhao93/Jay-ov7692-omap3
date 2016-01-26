modprobe omap3-isp dyndbg=+pt

sleep 1

media-ctl -r -l '"OV7692":0->"OMAP3 ISP CCDC":0[1],
		 "OMAP3 ISP CCDC":1->"OMAP3 ISP CCDC output":0[1]'

sleep 1

media-ctl -V '"OV7692":0[SBGGR8 640x480],
	      "OMAP3 ISP CCDC":1[SBGGR8 640x480]'

sleep 1

echo 'yavta -p -c1 -n1 -s640x480 -fSBGGR8 -Fimage-#.raw /dev/video2'
echo 'yavta -p -c10 -n1 -s640x480 -fSBGGR8 -Fimage-#.raw /dev/video2'

