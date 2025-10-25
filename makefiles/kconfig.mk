KCONFIG_DIR := tools/kconfig

# Automatically gather all .c files in kconfig and lxdialog
KCONFIG_SRCS := $(wildcard $(KCONFIG_DIR)/*.c)
KCONFIG_LXDIALOG_SRCS := $(wildcard $(KCONFIG_DIR)/lxdialog/*.c)

# All sources needed for menuconfig (TUI)
MCONF_SRCS := \
    $(KCONFIG_DIR)/mconf.c \
    $(KCONFIG_LXDIALOG_SRCS) \
    $(filter-out $(KCONFIG_DIR)/conf.c $(KCONFIG_DIR)/mconf.c, $(KCONFIG_SRCS))

# All sources needed for conf (CLI config)
CONF_SRCS := \
    $(KCONFIG_DIR)/conf.c \
    $(filter-out $(KCONFIG_DIR)/mconf.c $(KCONFIG_DIR)/conf.c, $(KCONFIG_SRCS))

KCONFIG_CFLAGS := -I$(KCONFIG_DIR) -I$(KCONFIG_DIR)/lxdialog

.PHONY: config menuconfig oldconfig kconfig-clean

menuconfig: $(KCONFIG_DIR)/mconf
	$(KCONFIG_DIR)/mconf Kconfig

config: $(KCONFIG_DIR)/conf
	$(KCONFIG_DIR)/conf Kconfig

oldconfig: $(KCONFIG_DIR)/conf
	$(KCONFIG_DIR)/conf --oldconfig Kconfig

kconfig-clean:
	rm -f $(KCONFIG_DIR)/conf $(KCONFIG_DIR)/mconf

# Build menuconfig (TUI)
$(KCONFIG_DIR)/mconf: $(MCONF_SRCS)
	gcc $(KCONFIG_CFLAGS) -o $@ $^ -lncurses

# Build conf (CLI)
$(KCONFIG_DIR)/conf: $(CONF_SRCS)
	gcc $(KCONFIG_CFLAGS) -o $@ $^