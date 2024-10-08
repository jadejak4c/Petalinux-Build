petalinux-build -c dma
petalinux-build -c rootfs
petalinux-build -x package
petalinux-build -c dma -x do_install
