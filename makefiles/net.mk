# ==================================================================
# Fabric Lab: Two Kernel-V nodes on one virtual cable.
# ==================================================================
LAB_DIR := $(BUILDDIR)/lab

# host disk test build type then use TestDisk image
LAB_IMG ?= $(if $(filter y,$(CONFIG_BUILD_TEST)),$(DISK_TEST_IMG),$(DISK_IMG))

# host loopback address endpoint
LAB_HOST ?= 127.0.0.1

# timeout suration
LAB_SECS ?= 30

# first 5 bytes of the virtual NICs mac address
LAB_MAC_PREFIX := 02:4b:56:00:00

# ============= LAB DESIGN =================
# 
#        QEMU process A                              QEMU process B
#  ┌─────────────────────────┐                 ┌─────────────────────────┐
#  │ Kernel-V node A         │                 │ Kernel-V node B         │
#  │          ↕              │                 │          ↕              │
#  │ virtio-net PCI device   │                 │ virtio-net PCI device   │
#  │ MAC 02:4b:50:00:00:01   │                 │ MAC 02:4b:50:00:00:02   │
#  │          ↕              │                 │          ↕              │
#  │ Network backend fab0    │                 │ Network backend fab0    │
#  │ 127.0.0.1:10001         │ ←── host UDP ─→ │ 127.0.0.1:10002         │
#  └─────────────────────────┘                 └─────────────────────────┘
# ===========================================

#
# === command template ===
# 	$(call lab_qemu,<node>,<index>,<local_port>,<remote_port>)
# 

#
# === lab_qemu ===
# 		name: labels the qemu vm (node).
# 		drive: attaches a virtual disk. 
# 			format identifies the image format
# 			file identifies the host file containing disk
# 		display node supress the graphic display
# 		no-reboot makes qemu exit when guest request for a reboot
#		serial 1 = COM1 = interactive guest console = null
#		serial 2 = COM2 = kernel logs = file to log
#		monitor managed by qemu - separate from Kernel-V console
#			unix:: create a unix-domain socket at lab/node-a.mon
#				two programs on same host can communicate, uses filesystem path
#				instead of ip/port
#			server end of the socket, listens for connection from client.
#			nowait doesn't wait for connection to established for VM to start
#		netdev creates the host networking backend
#			dgram backend to use udp protocol
#			id=fab0 identification for qemu backend
#			local is host and remote is peer
#			type=inet use an internet socket
#			host  addess for backend to bind
#			port  to recieve traffic
#		device add a device to guest
#			virtio-net-pci presents a Virtio network card on the KernelV PCI bus
#			netdev=fab0	connect the card to the qemu backend
#			mac assigns a predictable mac address
#		object attaches a packet-capture filter to qemu network backend
#			filter-dump captures the network traffic
#			id for the filter object
#			netdev selectes the network backenf
#			file to dump the nodes packets captured
#
define lab_qemu
$(QEMU) -name kv-node-$(1)	\
		-drive format=raw,file=$(LAB_IMG),snapshot=on	\
		-display none -no-reboot	\
		-serial null -serial file:$(LAB_DIR)/node-$(1).log	\
		-monitor unix:$(LAB_DIR)/node-$(1).mon,server,nowait	\
		-netdev dgram,id=fab0,local.type=inet,local.host=$(LAB_HOST),local.port=$(3),remote.type=inet,remote.host=$(LAB_HOST),remote.port=$(4)	\
		-device virtio-net-pci,netdev=fab0,mac=$(LAB_MAC_PREFIX):0$(2),disable-legacy=on	\
		-object filter-dump,id=dump0,netdev=fab0,file=$(LAB_DIR)/node-$(1).pcap
endef

.PHONY: node-a node-b lab lab-report lab-clean

$(LAB_DIR):
	$(Q)mkdir -p $@

## Run fabric node A in foreground
node-a: $(LAB_IMG) | $(LAB_DIR)
	$(call lab_qemu,a,1,10001,10002)

## Run fabric node B in foreground
node-b: $(LAB_IMG) | $(LAB_DIR)
	$(call lab_qemu,b,2,10002,10001)

## Boot nodes A+B together for LAB_SECS, then report
lab: $(LAB_IMG) | $(LAB_DIR)
	$(Q)rm -f $(LAB_DIR)/node-*
	$(ECHO) "Fabric lab: 2 nodes for $(LAB_SECS)s -> $(LAB_DIR)/"
	$(Q)( timeout $(LAB_SECS) $(call lab_qemu,a,1,10001,10002) & \
		  timeout $(LAB_SECS) $(call lab_qemu,b,2,10002,10001) & \
		  wait ) || true
	$(Q)$(MAKE) --no-print-directory lab-report

## Summarize the last lab run
lab-report:
	@for n in a b; do	\
		nic=$$(grep -q "vendor=1af4 device=1041" $(LAB_DIR)/node-$$n.log 2>/dev/null && echo yes || echo NO); \
		sz=$$(stat -c %s $(LAB_DIR)/node-$$n.pcap 2>/dev/null || echo 0); \
		printf "node-%s: virtio-net(modern) on PCI: %-3s	pcap: %6s bytes\n" $$n $$nic $$sz;	\
	done

## Remove lab logs/pcap/monitor sockets
lab-clean:
	$(Q)rm -rf $(LAB_DIR)

