#include "arch/x86/pci/pci_driver_api.h"

uint8_t pci_device_validity_check(const pci_device_t *device) {

    /* check for valid refrencing of the device and record object */
    return (device && device->record);
}

const pci_bar_info_t *pci_device_get_bar(const pci_device_t *device,
                                         uint8_t idx) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI",
                   "Failed to get BAR Info. invalid reference to device.\n");
        return NULL;
    }

    if (!device->record->bars_valid) {
        KLOG_ERROR("PCI", "Failed to get BAR Info. BARs info id not valid.\n");
        return NULL;
    }

    if (idx >= PCI_TYPE0_BAR_COUNT) {
        KLOG_ERROR("PCI",
                   "Failed to get BAR info. BAR index %u is greater than max "
                   "count %u.\n",
                   idx, PCI_TYPE0_BAR_COUNT);
        return NULL;
    }

    return &device->record->bars[idx];
}

const pci_bar_info_t *pci_device_get_bar_kind(const pci_device_t *device,
                                              pci_bar_kind_t kind) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI",
                   "Failed to get BAR Info. invalid reference to device.\n");
        return NULL;
    }

    /* iterate thru the BAR list and find matching kind */
    for (uint8_t i = 0; i < PCI_TYPE0_BAR_COUNT; i++) {
        if (device->record->bars[i].kind == kind) {
            return &device->record->bars[i];
        }
    }

    return NULL;
}

const pci_command_status_info_t *
pci_device_get_cmd_status(const pci_device_t *device) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI", "Failed to get Command & Status Info. invalid "
                          "reference to device.\n");
        return NULL;
    }

    if (!device->record->cmd_status_valid) {
        KLOG_ERROR("PCI", "Failed to get Command & Status Info. Command and "
                          "Status info is not valid.\n");
        return NULL;
    }

    return &device->record->cmd_status;
}

const pci_capability_info_t *pci_device_get_cap_kind(const pci_device_t *device,
                                                     pci_cap_kind_t kind) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI",
                   "Failed to get capability. invalid reference to device.\n");
        return NULL;
    }

    if (!device->record->caps_valid) {
        KLOG_ERROR(
            "PCI",
            "Failed to get capability. Capabilities entries are not valid.\n");
        return NULL;
    }

    for (uint8_t i = 0; i < device->record->cap_count; i++) {
        if (device->record->caps[i].kind == kind) {
            return &device->record->caps[i];
        }
    }

    return NULL;
}

uint8_t pci_device_cmd_status_refresh(const pci_device_t *device) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR(
            "PCI",
            "Failed to update command & status values. Invaid arguments.\n");
        return 0;
    }

    pci_command_status_info_t info;
    info.command = pci_read_command(device->record->id.bdf);
    info.status = pci_read_status(device->record->id.bdf);

    device->record->cmd_status_valid = 1;
    device->record->cmd_status = info;

    return 1;
}

uint8_t pci_device_enable_io(const pci_device_t *device) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI", "Enabling IO failed. invalid reference to device.\n");
        return 0;
    }

    pci_command_enable_io_space(device->record->id.bdf);
    return pci_device_cmd_status_refresh(device);
}

uint8_t pci_device_enable_mem(const pci_device_t *device) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI", "Enabling IO failed. invalid reference to device.\n");
        return 0;
    }

    pci_command_enable_mem_space(device->record->id.bdf);
    return pci_device_cmd_status_refresh(device);
}

uint8_t pci_device_enable_bus_master(const pci_device_t *device) {
    if (!pci_device_validity_check(device)) {
        KLOG_ERROR("PCI",
                   "Enabling MMIO failed. invalid reference to device.\n");
        return 0;
    }

    pci_enable_bus_master(device->record->id.bdf);
    return pci_device_cmd_status_refresh(device);
}
