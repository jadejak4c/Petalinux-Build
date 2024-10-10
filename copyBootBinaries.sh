mkdir bootfiles
cp images/linux/zynqmp_fsbl.elf bootfiles/
cp images/linux/u-boot.elf bootfiles/
cp images/linux/pmufw.elf bootfiles/
cp images/linux/bl31.elf bootfiles/
cp components/plnx_workspace/device-tree/device-tree/Kria_DMA_wrapper_counter.bit bootfiles/