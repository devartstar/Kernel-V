KCONFIG_DIR := tools/kconfig

KCONFIG_SRCS_COMMON := \
    $(KCONFIG_DIR)/expr.c \
    $(KCONFIG_DIR)/menu.c \
    $(KCONFIG_DIR)/symbol.c \
    $(KCONFIG_DIR)/lkc.c \
    $(KCONFIG_DIR)/confdata.c \
    $(KCONFIG_DIR)/preprocess.c \
    $(KCONFIG_DIR)/util.c

MCONF_LXDIALOG_SRCS := \
    $(KCONFIG_DIR)/lxdialog/checklist.c \
    $(KCONFIG_DIR)/lxdialog/inputbox.c \
    $(KCONFIG_DIR)/lxdialog/menubox.c \
    $(KCONFIG_DIR)/lxdialog/msgbox.c \
    $(KCONFIG_DIR)/lxdialog/textbox.c \
    $(KCONFIG_DIR)/lxdialog/util.c \
    $(KCONFIG_DIR)/lxdialog/yesno.c

# All sources for menuconfig (mconf)
MCONF_SRCS := \
    $(KCONFIG_DIR)/mconf.c \
    $(KCONFIG_SRCS_COMMON) \
    $(MCONF_LXDIALOG_SRCS)

# All sources for config (conf)
CONF_SRCS := \
    $(KCONFIG_DIR)/conf.c \
    $(KCONFIG_SRCS_COMMON)

KCONFIG_CFLAGS := -I$(KCONFIG_DIR) -I$(KCONFIG_DIR)/lxdialog

.PHONY: config menuconfig oldconfig kconfig-clean

# Build and run command-line config
config: $(KCONFIG_DIR)/conf
	$(KCONFIG_DIR)/conf Kconfig

# Build and run menuconfig
menuconfig: $(KCONFIG_DIR)/mconf
	$(KCONFIG_DIR)/mconf Kconfig

# Build and run oldconfig
oldconfig: $(KCONFIG_DIR)/conf
	$(KCONFIG_DIR)/conf --oldconfig Kconfig

# Clean kconfig tools
kconfig-clean:
	rm -f $(KCONFIG_DIR)/conf $(KCONFIG_DIR)/mconf

# Build mconf (menuconfig UI)
$(KCONFIG_DIR)/mconf: $(MCONF_SRCS)
	gcc $(KCONFIG_CFLAGS) -o $@ $^ -lncurses

# Build conf (CLI)
$(KCONFIG_DIR)/conf: $(CONF_SRCS)
	gcc $(KCONFIG_CFLAGS) -o $@ $^

