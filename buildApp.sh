petalinux-build -c dma-counter
petalinux-build -c rootfs
petalinux-build -x package
petalinux-build -c dma-counter -x do_install
