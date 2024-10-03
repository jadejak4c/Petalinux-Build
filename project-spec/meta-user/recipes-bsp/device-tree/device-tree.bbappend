FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += "file://system-user.dtsi"

python () {
    config_disable = d.getVar("CONFIG_DISABLE")
    if config_disable and config_disable == "1":
        d.setVarFlag("do_configure", "noexec", "1")
}

export PETALINUX
do_configure_append () {
    script="${PETALINUX}/etc/hsm/scripts/petalinux_hsm_bridge.tcl"
    data="${PETALINUX}/etc/hsm/data/"
    xsct_command="xsct -sdx -nodisp ${script} -c ${WORKDIR}/config \
        -hdf ${petalinux_hsm_bridge}/hardware_description.${HDF_EXT} \
        -repo ${S} -data ${data} -sw ${DT_FILES_PATH} -o ${DT_FILES_PATH} -a 'soc_mapping'"
    
    # Evaluate the xsct command
    eval ${xsct_command}
}
