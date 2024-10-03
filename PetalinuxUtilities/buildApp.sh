petalinux-build -c devmemapp
petalinux-build -c rootfs
petalinux-build -x package
petalinux-build -c devmemapp -x do_install
