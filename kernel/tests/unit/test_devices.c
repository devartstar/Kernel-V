#include "tests/test_devices.h"
#include "arch/x86/pci/pci_cfg.h"
#include "arch/x86/pci/pci_devices.h"
#include "arch/x86/pci/pci_driver.h"
#include "arch/x86/pci/pci_registry.h"

/* Single shared registry to avoid 64KB-per-instance BSS bloat */
static pci_registry_t test_reg;

/**
 * pci_dummy_driver_probe - driver call when the driver entry matches
 */
static uint8_t pci_dummy_driver_probe(pci_device_t *pci_device) {
    if (!pci_device || !pci_device->record) {
        KLOG_ERROR("DEVICE_TEST", "Probing dumm driver failed. Invalid device "
                                  "reference as argument.\n");
        return 0;
    }

    KLOG_INFO("DEVICE_TEST",
              "pci probe successful for dummy driver:"
              "\n\tName: [%s], Vendor: %04x, Device=%04x.\n",
              pci_device->device.name, pci_device->record->id.vendor_id,
              pci_device->record->id.device_id);

    return 1;
}

/**
 * pci_dummy_driver - dummy pci driver to invoke for 0x100e device
 */
static const pci_driver_t pci_dummy_driver = {
    .name = "pci-dummy-e1000",
    .vendor_id = 0x8086,
    .device_id = 0x100e,
    .probe = pci_dummy_driver_probe,
};

uint8_t device_pci_driver_match_test(void) {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    const pci_function_record_t *test_rec_const;
    pci_function_record_t *test_rec;
    pci_device_t *pci_test_dev;

    /* enumerate bus 0 into registry */
    if (!pci_enumerate_bus0_into_registry(&test_reg)) {
        KLOG_ERROR("DEVICE_TEST", "pci_driver_match_test failed. Failed to "
                                  "probe bus0 into registry.\n");
        return 0;
    }

    /* find a specific record from registry */
    test_rec_const = pci_registry_find_bdf(&test_reg, bdf);
    if (!test_rec_const) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_driver_match_test failed. Failed to find record in "
                   "registry for %02x:%02x.%u.\n",
                   test_rec_const->id.bdf.bus, test_rec_const->id.bdf.device,
                   test_rec_const->id.bdf.function);
        return 0;
    }

    test_rec = (pci_function_record_t *)test_rec_const;

    /* Initalize the pci device object with the record entry for bdf */
    if (!pci_device_init(pci_test_dev, test_rec, NULL)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_driver_match_test failed. Failed to initialize device object"
            "for %02x:%02x.%u.\n",
            test_rec->id.bdf.bus, test_rec->id.bdf.device,
            test_rec->id.bdf.function);
        return 0;
    }

    /* check if device matches with the dummy driver */
    if (!pci_driver_matches(&pci_dummy_driver, pci_test_dev)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_driver_match_test failed. Failed to match driver with "
                   "similar vendor (%04x) device (%04x) id for %02x:%02x.%u.\n",
                   test_rec->id.vendor_id, test_rec->id.device_id,
                   test_rec->id.bdf.bus, test_rec->id.bdf.device,
                   test_rec->id.bdf.function);
        return 0;
    }

    /* invoke the driver probe routine */
    if (!pci_dummy_driver.probe(pci_test_dev)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_driver_match_test failed. Failed to invoke driver "
                   "probe routine for %02x:%02x.%u.\n",
                   test_rec->id.bdf.bus, test_rec->id.bdf.device,
                   test_rec->id.bdf.function);
        return 0;
    }

    KLOG_INFO("DEVICE_TEST", "pci_driver_match_test passed. Successfully "
                             "probed matched driver routine.\n");
    return 1;
}
