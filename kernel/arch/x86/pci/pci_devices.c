#include "arch/x86/pci/pci_devices.h"
#include "lib/string.h"

void pci_device_make_name(char *buf, uint32_t buf_size,
                          const pci_function_record_t *rec) {
    /* validity check of the input args */
    if (!buf || buf_size == 0 || !rec) {
        KLOG_ERROR("PCI", "Device naming failed. Invalid arguments.\n");
        return;
    }

    my_snprintf(buf, buf_size, "pci-%02x:%02x.%u", rec->id.bdf.bus,
                rec->id.bdf.device, rec->id.bdf.function);
}

uint8_t pci_device_init(pci_device_t *pci_dev, pci_function_record_t *rec,
                        device_t *parent_dev) {
    char name[DEVICE_NAME_MAX];

    /* check for validity of input args */
    if (!pci_dev || !parent_dev || !rec->present) {
        KLOG_ERROR("PCI",
                   "Error intializing PCI device. Invalid argumens passed.\n");
        return 0;
    }

    pci_device_make_name(name, sizeof(name), rec);

    /* call initializer of the generic kernel device object */
    device_init(&pci_dev->device, DEVICE_BUS_PCI, name, parent_dev);

    pci_dev->record = rec;
    pci_dev->device.bus_data = rec;

    return 1;
}
