obj-y += io.o idle.o irq.o timer.o dma.o memory.o
obj-y += devices.o
obj-$(CONFIG_MSM_AMSS_VERSION_6210) += proc_comm.o
obj-$(CONFIG_MSM_AMSS_VERSION_6220) += proc_comm.o
obj-$(CONFIG_MSM_AMSS_VERSION_6225) += proc_comm.o
obj-$(CONFIG_MSM_AMSS_VERSION_WINCE) += proc_comm_wince.o
obj-y += vreg.o
obj-y += acpuclock.o
obj-y += clock-7x01a.o clock-wince.o
obj-y += gpio.o generic_gpio.o
obj-y += nand_partitions.o

obj-$(CONFIG_MSM_FIQ_SUPPORT) += fiq_glue.o
obj-$(CONFIG_MACH_TROUT) += board-trout-rfkill.o
obj-$(CONFIG_MSM_SMD) += smd.o smd_tty.o smd_qmi.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_device.o
obj-$(CONFIG_MSM_ONCRPCROUTER) += smd_rpcrouter_servers.o
obj-$(CONFIG_MSM_RPCSERVERS) += rpc_server_dog_keepalive.o
obj-$(CONFIG_MSM_RPCSERVERS) += rpc_server_time_remote.o
obj-$(CONFIG_MSM_ADSP) += qdsp5/
obj-$(CONFIG_MSM_HW3D) += hw3d.o
obj-$(CONFIG_PM) += pm.o
obj-$(CONFIG_MSM_CPU_FREQ) += cpufreq.o

obj-$(CONFIG_MACH_HTCRAPHAEL) += board-htcraphael.o board-htcraphael-keypad.o board-htcraphael-panel.o 
obj-$(CONFIG_MACH_HTCRAPHAEL_CDMA) += board-htcraphael.o board-htcraphael-keypad.o board-htcraphael-panel.o 
obj-$(CONFIG_MACH_HTCDIAMOND) += board-htcdiamond.o board-htcraphael-keypad.o
obj-$(CONFIG_MACH_HTCDIAMOND_CDMA) += board-htcdiamond.o board-htcraphael-keypad.o

obj-$(CONFIG_MACH_HALIBUT) += board-halibut.o board-halibut-keypad.o
obj-$(CONFIG_MACH_TROUT) += board-trout.o board-trout-gpio.o
obj-$(CONFIG_MACH_TROUT) += board-trout-keypad.o board-trout-panel.o
obj-$(CONFIG_MACH_TROUT) += htc_akm_cal.o htc_wifi_nvs.o htc_acoustic.o
obj-$(CONFIG_MACH_TROUT) += board-trout-mmc.o board-trout-wifi.o
obj-$(CONFIG_TROUT_H2W) += board-trout-h2w.o
obj-$(CONFIG_TROUT_BATTCHG) += htc_battery.o
obj-$(CONFIG_TROUT_PWRSINK) += htc_pwrsink.o

obj-$(CONFIG_HTC_FB_CONSOLE) += htc_fb_console.o
