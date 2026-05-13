#include "arch/x86/pci/pci.h"
#include "lib/printk.h"

pci_probe_result_t pci_probe_function(pci_bdf_t bdf,
                                      pci_function_identity_t *out) {
    uint16_t vendor_id;

    if (!out) {
        KLOG_ERROR("PCI", "Invalid pointer to pci function identifier.\n");
        return PCI_PROBE_ERROR;
    }

    /* check if the endpoint to search is valid */
    if (!is_bdf_valid(bdf)) {
        KLOG_ERROR("PCI", "Invalid bdf (%02x:%02x.%u) endpoint.\n", bdf.bus,
                   bdf.device, bdf.function);
        return PCI_PROBE_ERROR;
    }

    /* Set default values to the function identifier */
    out->bdf = bdf;
    out->vendor_id = PCI_INVALID_VENDOR_ID;
    out->device_id = 0;
    out->class_code = 0;
    out->subclass = 0;
    out->prog_if = 0;
    out->revision_id = 0;
    out->header_type = 0;

    /* read the vendor id from the config space */
    vendor_id = pci_cfg_read16(bdf, PCI_CFG_VENDOR_ID);
    if (vendor_id == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI", "Invalid vendor id 0x%08x.\n", vendor_id);
        return PCI_PROBE_ABSENT;
    }

    /* Populate the fields of the function identifier reading from config space
     */
    out->vendor_id = vendor_id;
    out->device_id = pci_cfg_read16(bdf, PCI_CFG_DEVICE_ID);
    out->class_code = pci_cfg_read8(bdf, PCI_CFG_CLASS_CODE);
    out->subclass = pci_cfg_read8(bdf, PCI_CFG_SUBCLASS);
    out->prog_if = pci_cfg_read8(bdf, PCI_CFG_PROG_IF);
    out->revision_id = pci_cfg_read8(bdf, PCI_CFG_REVISION_ID);
    out->header_type = pci_cfg_read8(bdf, PCI_CFG_HEADER_TYPE);

    return PCI_PROBE_PRESENT;
}
