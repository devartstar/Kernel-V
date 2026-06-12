#include "tests/test_devices.h"
#include "arch/x86/pci/pci_bind.h"
#include "arch/x86/pci/pci_cfg.h"
#include "arch/x86/pci/pci_device_registry.h"
#include "arch/x86/pci/pci_devices.h"
#include "arch/x86/pci/pci_driver.h"
#include "arch/x86/pci/pci_driver_api.h"
#include "arch/x86/pci/pci_driver_registry.h"
#include "arch/x86/pci/pci_dump.h"
#include "arch/x86/pci/pci_record_registry.h"

/* Single shared registry to avoid 64KB-per-instance BSS bloat */
static pci_record_registry_t test_record_reg;
static pci_driver_registry_t test_driver_reg;
static pci_device_registry_t test_device_reg;
static pci_bind_summary_t test_bind_summary;

/** *** START: TEST DRIVER 1 *** */

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

/* defining an array of match rules for the dummy pci driver */
static const pci_match_rule_t pci_dummy_driver_e1000_matches[] = {{
    .type = PCI_MATCH_VENDOR_DEVICE,
    .vendor_id = 0x8086,
    .device_id = 0x100e,
    .class_code = PCI_MATCH_ANY_U8,
    .subclass = PCI_MATCH_ANY_U8,
    .prog_if = PCI_MATCH_ANY_U8,
}};

/**
 * pci_dummy_driver - dummy pci driver to invoke for 0x100e device
 */
static const pci_driver_t pci_dummy_driver = {
    .name = "pci-dummy-e1000",
    .probe = pci_dummy_driver_probe,
    .remove = NULL,
    .matches = pci_dummy_driver_e1000_matches,
    .match_count = sizeof(pci_dummy_driver_e1000_matches) /
                   sizeof(pci_dummy_driver_e1000_matches[0]),
};

/** *** END: TEST DRIVER 1 *** */

/** *** START: TEST DRIVER 2 *** */

/* dummy driver data info */
typedef struct pci_dummy_net_class_driver_data {
    uint32_t mmio_bar_base;
    uint32_t io_bar_base;
    uint8_t bus_master_enabled;
} pci_dummy_net_class_driver_data_t;

static pci_dummy_net_class_driver_data_t test_driver_data;

static uint8_t pci_dummy_net_class_device_probe(pci_device_t *device) {
    const pci_bar_info_t *bar0;
    const pci_bar_info_t *io_bar;
    const pci_command_status_info_t *cmd_status_before;
    const pci_command_status_info_t *cmd_status_after;

    /* check if the reference to the device is valid */
    if (!device || !device->record->present) {
        KLOG_ERROR("DEVICE_TEST", "dummy_e1000_device_probe failed. invalid "
                                  "reference to the device.\n");
        return 0;
    }

    /* Read the BAR0 from the devcie object */
    bar0 = pci_device_get_bar(device, 0);
    if (!bar0 || !bar0->present) {
        KLOG_ERROR("DEVICE_TEST",
                   "dummy_e1000_device_probe failed. %s missing BAR0.\n",
                   device->device.name);
        return 0;
    }

    /* Read the IO BAR from the device object */
    io_bar = pci_device_get_bar_kind(device, PCI_BAR_KIND_IO);
    if (!io_bar || !io_bar->present) {
        KLOG_ERROR("DEVICE_TEST",
                   "dummy_e1000_device_probe failed. %s missing IO BAR.\n",
                   device->device.name);
        return 0;
    }

    /* Read the command & status bits before enabling bus master */
    cmd_status_before = pci_device_get_cmd_status(device);
    if (!cmd_status_before) {
        KLOG_ERROR("DEVICE_TEST",
                   "dummy_e1000_device_probe failed. %s missing cached "
                   "commad/status bits.\n",
                   device->device.name);
        return 0;
    }

    /* Update the command bits for bus master */
    if (!pci_device_enable_bus_master(device)) {
        KLOG_ERROR("DEVICE_TEST",
                   "dummy_e1000_device_probe failed. %s fialed to enable bus "
                   "master and cache it.\n",
                   device->device.name);
        return 0;
    }

    /* Read the command & status bits after enabling bus master */
    cmd_status_after = pci_device_get_cmd_status(device);
    if (!cmd_status_after) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "dummy_e1000_device_probe failed. %s failed to refresh cached "
            "commad/status bits.\n",
            device->device.name);
        return 0;
    }

    KLOG_INFO("DEVICE_TEST",
              "bound %s BAR0=%08x IOBAR=%08x cmd_before=%04x cmd_after=%04x\n",
              device->device.name, (uint32_t)bar0->base, (uint32_t)io_bar->base,
              cmd_status_before->command, cmd_status_after->command);

    test_driver_data.mmio_bar_base = bar0->base;
    test_driver_data.io_bar_base = io_bar->base;
    test_driver_data.bus_master_enabled = 1;

    pci_device_set_driver_data(device, &test_driver_data);

    return 1;
}

static const pci_match_rule_t pci_dummy_net_class_driver_matches[] = {{
    .type = PCI_MATCH_CLASS,
    .vendor_id = PCI_MATCH_ANY_U16,
    .device_id = PCI_MATCH_ANY_U16,
    .class_code = 0x02,
    .subclass = 0x00,
    .prog_if = PCI_MATCH_ANY_U8,
}};

static const pci_driver_t pci_dummy_net_class_driver = {
    .name = "dummy_net_class_driver",
    .probe = pci_dummy_net_class_device_probe,
    .remove = NULL,
    .matches = pci_dummy_net_class_driver_matches,
    .match_count = sizeof(pci_dummy_net_class_driver_matches) /
                   sizeof(pci_dummy_net_class_driver_matches[0]),
};

/** *** END: TEST DRIVER 2 *** */

/** *** START: Dummy Device <> Driver utility tests *** */

uint8_t device_pci_driver_match_test(void) {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    const pci_function_record_t *test_rec_const;
    pci_function_record_t *test_rec;
    pci_device_t pci_test_dev;

    /* enumerate bus 0 into registry */
    if (!pci_enumerate_bus0_into_record_registry(&test_record_reg)) {
        KLOG_ERROR("DEVICE_TEST", "pci_driver_match_test failed. Failed to "
                                  "probe bus0 into registry.\n");
        return 0;
    }

    /* find a specific record from registry */
    test_rec_const = pci_record_registry_find_bdf(&test_record_reg, bdf);
    if (!test_rec_const) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_driver_match_test failed. Failed to find record in "
                   "registry for %02x:%02x.%u.\n",
                   bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    test_rec = (pci_function_record_t *)test_rec_const;

    /* Initalize the pci device object with the record entry for bdf */
    if (!pci_device_init(&pci_test_dev, test_rec, NULL)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_driver_match_test failed. Failed to initialize device object"
            "for %02x:%02x.%u.\n",
            test_rec->id.bdf.bus, test_rec->id.bdf.device,
            test_rec->id.bdf.function);
        return 0;
    }

    /* check if device matches with the dummy driver */
    if (!pci_driver_matches(&pci_dummy_driver, &pci_test_dev)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_driver_match_test failed. Failed to match driver with "
                   "similar vendor (%04x) device (%04x) id for %02x:%02x.%u.\n",
                   test_rec->id.vendor_id, test_rec->id.device_id,
                   test_rec->id.bdf.bus, test_rec->id.bdf.device,
                   test_rec->id.bdf.function);
        return 0;
    }

    /* invoke the driver probe routine */
    if (!pci_dummy_driver.probe(&pci_test_dev)) {
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

uint8_t device_pci_driver_api_test(void) {
    pci_record_registry_t *reg = &test_record_reg;
    pci_bdf_t nic_bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    const pci_function_record_t *nic_const;
    pci_function_record_t *nic;
    pci_device_t dev;
    const pci_bar_info_t *bar0;
    const pci_bar_info_t *io_bar;
    const pci_command_status_info_t *cmd_before;
    const pci_command_status_info_t *cmd_after;

    if (!pci_enumerate_bus0_into_record_registry(reg)) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_api_test failed: enumeration failed.\n");
        return 0;
    }

    if (!pci_enrich_record_registry_resources(reg)) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_api_test failed: enrichment failed.\n");
        return 0;
    }

    nic_const = pci_record_registry_find_bdf(reg, nic_bdf);
    if (!nic_const) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_api_test failed: target %02x:%02x.%u not "
                   "found.\n",
                   nic_bdf.bus, nic_bdf.device, nic_bdf.function);
        return 0;
    }

    nic = (pci_function_record_t *)nic_const;

    if (!pci_device_init(&dev, nic, NULL)) {
        KLOG_ERROR("DEVICE_TEST", "device_pci_driver_api_test failed: device "
                                  "initialization failed.\n");
        return 0;
    }

    KLOG_VERBOSE("DEVICE_TEST",
                 "device_pci_driver_api_test: reading bar and cmd info.\n");
    bar0 = pci_device_get_bar(&dev, 0);
    io_bar = pci_device_get_bar_kind(&dev, PCI_BAR_KIND_IO);
    cmd_before = pci_device_get_cmd_status(&dev);

    KLOG_INFO("DEVICE_TEST", "BAR0: %08x, %u.\n", bar0, bar0->present);

    if (!bar0 || !bar0->present) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_api_test failed: BAR0 missing.\n");
        return 0;
    }

    KLOG_INFO("DEVICE_TEST", "IO BAR: %08x, %u.\n", io_bar, io_bar->present);

    if (!io_bar || !io_bar->present) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_api_test failed: IO BAR missing.\n");
        return 0;
    }

    if (!cmd_before) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "device_pci_driver_api_test failed: Command & Status missing.\n");
        return 0;
    }

    if (!pci_device_enable_bus_master(&dev)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "device_pci_driver_api_test failed: Enabling bus master failed.\n");
        return 0;
    }

    KLOG_VERBOSE("DEVICE_TEST",
                 "device_pci_driver_api_test: updating command info.\n");
    cmd_after = pci_device_get_cmd_status(&dev);

    if (!cmd_after) {
        KLOG_ERROR("DEVICE_TEST", "device_pci_driver_api_test failed: Command "
                                  "& Status refresh failed.\n");
        return 0;
    }

    if ((cmd_after->command & PCI_CMD_BUS_MASTER) == 0) {
        KLOG_ERROR("DEVICE_TEST", "device_pci_driver_api_test failed: Command "
                                  "bus master bit set failed.\n");
        return 0;
    }

    KLOG_INFO("DEVICE_TEST",
              "device_pci_driver_api_test success:"
              "\n\tBAR0=%08x, IO_BAR=%08x, cmd_before=%04x, cmd_after=%04x.\n",
              (uint32_t)bar0->base, (uint32_t)io_bar->base, cmd_before->command,
              cmd_after->command);

    return 1;
}

/** *** END: Dummy Device <> Driver utility tests *** */

/** *** START: Dummy Device <> Driver linkage tests *** */

uint8_t device_pci_device_registry_materialize_test(void) {

    pci_record_registry_t *record_registry = &test_record_reg;
    pci_device_registry_t device_registry;
    const pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    pci_device_t *device;

    /** BUILDS UP THE RECORD REGISTRY
     * 1. Probe all the devices in bus 0. Probe all the function in each device.
     * 2. In probe function it reads the value from the config space and
     * populates the function identity field of the function record.
     * 3. If a function has valid entries a call back add the function identity
     * of function record in the registry. other fields of function record -
     * like BAR, command status, capabilities are jsut intialized to default.
     * 4. While probing slot, check if its a multifunctional slot. ie.
     * (bridge)device has multiple functions.
     * 5. If multifunctional - then probe all the functions 1-7 for that slot.
     * They all will be added to registry too.
     * 6. NOTE: We just have populated the function identity in the function
     * record of the registry. Need to enrich BAR/command/capabilities values
     * registry_ctx
     * -> pointer to the registry. Registry contains array of function records.
     * -> Number of function records successfully insterted in the registry.
     * -> Number of function records unsuccessfull in inserting in registry.
     */
    if (!pci_enumerate_bus0_into_record_registry(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_materialize failed. Failed to "
                   "enumerate bus0 into registry.\n");
        return 0;
    }

    /** ENRICH THE RECORD REGISTRY
     * 1. for each record entry in the registry it will decode the bits from the
     * config space and update the structs with meaningful value.
     * 2. Decode and update the BAR information for each record.
     * 3. Decode and update the Command/Status bits for each record.
     * 4. Decode and update the Capability list for each record.
     */
    if (!pci_enrich_record_registry_resources(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_materialize failed. Failed to "
                   "enrich the registry record.\n");
        return 0;
    }

    /** BUILD DEVICE REGISTRY FROM RECORD REGISTRY
     * 1. initialize the device registry.
     * 2. for each record entry in the registry initalize a device object with
     * the device name format "pci-bus:device:function"
     * 3. add the device object in the device registry.
     */
    if (!pci_device_registry_materialize_from_record_registry(
            &device_registry, record_registry, NULL)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_materialize failed. failed to "
                   "materialize device registry using the record registry.\n");
        return 0;
    }

    /* check if we were able to assign a device registry entry for all records
     * in record registry */
    if (device_registry.count != record_registry->count) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_device_registry_materialize failed. mismatch between device "
            "registry entries (%u) vs record registry entries (%u).\n",
            device_registry.count, record_registry->count);
        return 0;
    }

    /* find the device object from the device registry */
    device = pci_device_registry_find_bdf(&device_registry, bdf);
    if (!device) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_materialize failed. failed to find "
                   "device with bdf [%02x:%0x.%u] in registry.\n",
                   bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    /* checks:
     * 1. bus type of device should be PCI.
     * 2. state of the device should be descovered, not probed since we havent
     * invoked probed just initialized device object into registry.
     * 3. Record associated with the device should not be null and bus data
     * should not ref. the record yet.
     * 4. driver information should not be present since we havent probed and
     * attached driver yet.
     */
    if (device->device.bus_type != DEVICE_BUS_PCI ||
        device->device.state != DEVICE_STATE_DISCOVERED ||
        device->record == NULL || device->device.bus_data != device->record ||
        device->device.driver_data != NULL ||
        device->device.bound_driver != NULL) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_device_registry_materialize failed. invariant mismatch.\n");
        return 0;
    }

    KLOG_INFO("DEVICE_TEST",
              "pci_device_registry_materialize succeeded. device registry has "
              "entries count %u and in which found device named %s.\n",
              device_registry.count, device->device.name);

    return 1;
}

uint8_t device_pci_driver_registry_bind_test(void) {
    pci_record_registry_t *record_registry = &test_record_reg;
    pci_driver_registry_t *driver_registry = &test_driver_reg;
    pci_device_registry_t *device_registry = &test_device_reg;
    pci_device_t *device;

    uint16_t bound_count = 0;

    /**
     * PHASE 1 : PCI DEVICE RECORD REGISTRY
     * enumerate and fill up the test record registry
     */
    if (!pci_enumerate_bus0_into_record_registry(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_registry_bind_test failed. Failed to "
                   "probe bus0 into registry.\n");
        return 0;
    }

    /** PHASE 2: ENRICH THE RECORD REGISTRY
     * 1. for each record entry in the registry it will decode the bits from the
     * config space and update the structs with meaningful value.
     * 2. Decode and update the BAR information for each record.
     * 3. Decode and update the Command/Status bits for each record.
     * 4. Decode and update the Capability list for each record.
     */
    if (!pci_enrich_record_registry_resources(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_materialize failed. Failed to "
                   "enrich the registry record.\n");
        return 0;
    }

    /** PHASE 3: BUILD PCI DEVICE REGISTRY FROM RECORD REGISTRY
     * 1. initialize the device registry.
     * 2. for each record entry in the registry initalize a device object with
     * the device name format "pci-bus:device:function"
     * 3. add the device object in the device registry.
     */
    if (!pci_device_registry_materialize_from_record_registry(
            device_registry, record_registry, NULL)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_materialize failed. failed to "
                   "materialize device registry using the record registry.\n");
        return 0;
    }

    /* PHASE 4: PCI DEVICES DRIVER REGISTRY
     * initialize the pci driver registry
     */
    pci_driver_registry_init(driver_registry);

    /* PHASE 5: UPDATE DRIVER REGISTRY WITH TEST DRIVER
     * Add the dummy test driver to the driver registry
     */
    if (!pci_driver_registry_add(driver_registry, &pci_dummy_driver)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pdevice_pci_driver_registry_bind_test failed. Failed to "
                   "add test driver to registry.\n");
        return 0;
    }

    /* PHASE 6: BIND ALL DRIVERS WITH DEVICES */
    bound_count = pci_probe_and_bind_all(device_registry, driver_registry,
                                         &test_bind_summary);
    if (bound_count <= 0) {
        KLOG_ERROR("DEVICE_TEST",
                   "pdevice_pci_driver_registry_bind_test failed. No drivers "
                   "bound, expected test driver %s to bind.\n",
                   pci_dummy_driver.name);
        return 0;
    }

    pci_dump_bind_summary(&test_bind_summary);

    KLOG_INFO("DEVICE_TEST",
              "pdevice_pci_driver_registry_bind_test successfully bound test "
              "driver %s to test device. bound count = %u.\n",
              pci_dummy_driver.name, bound_count);
    return 1;
}

uint8_t device_pci_device_driver_test(void) {
    pci_record_registry_t *record_registry = &test_record_reg;
    pci_driver_registry_t *driver_registry = &test_driver_reg;
    pci_device_registry_t *device_registry = &test_device_reg;
    const pci_device_t *device_const;
    const pci_device_t *device;
    uint8_t bound_count = 0;
    const pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    pci_dummy_net_class_driver_data_t *data;

    /** BUILDS UP THE RECORD REGISTRY
     * 1. Probe all the devices in bus 0. Probe all the function in each device.
     * 2. In probe function it reads the value from the config space and
     * populates the function identity field of the function record.
     * 3. If a function has valid entries a call back add the function identity
     * of function record in the registry. other fields of function record -
     * like BAR, command status, capabilities are jsut intialized to default.
     * 4. While probing slot, check if its a multifunctional slot. ie.
     * (bridge)device has multiple functions.
     * 5. If multifunctional - then probe all the functions 1-7 for that slot.
     * They all will be added to registry too.
     * 6. NOTE: We just have populated the function identity in the function
     * record of the registry. Need to enrich BAR/command/capabilities values
     * registry_ctx
     * -> pointer to the registry. Registry contains array of function records.
     * -> Number of function records successfully insterted in the registry.
     * -> Number of function records unsuccessfull in inserting in registry.
     */
    if (!pci_enumerate_bus0_into_record_registry(record_registry)) {
        KLOG_ERROR("DEVICE_TEST", "pci_device_driver_test failed. Failed to "
                                  "enumerate bus0 into registry.\n");
        return 0;
    }

    /** ENRICH THE RECORD REGISTRY
     * 1. for each record entry in the registry it will decode the bits from the
     * config space and update the structs with meaningful value.
     * 2. Decode and update the BAR information for each record.
     * 3. Decode and update the Command/Status bits for each record.
     * 4. Decode and update the Capability list for each record.
     */
    if (!pci_enrich_record_registry_resources(record_registry)) {
        KLOG_ERROR("DEVICE_TEST", "pci_device_driver_test failed. Failed to "
                                  "enrich the registry record.\n");
        return 0;
    }

    /** BUILD PCI DEVICE REGISTRY FROM RECORD REGISTRY
     * 1. initialize the device registry.
     * 2. for each record entry in the registry initalize a device object with
     * the device name format "pci-bus:device:function"
     * 3. add the device object in the device registry.
     */
    if (!pci_device_registry_materialize_from_record_registry(
            device_registry, record_registry, NULL)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_driver_test failed. failed to "
                   "materialize device registry using the record registry.\n");
        return 0;
    }

    /** INITIALIZE THE DRIVER REGISTRY
     * driver_registry ->
     * -> point to the array of driver object.
     *  Each driver object contains name, ids, probe routine.
     * -> number of driver objects in the registry
     *  Post init, there is no driver registered in the registry.
     */
    pci_driver_registry_init(driver_registry);

    /** REGISTER DUMMY 1000E DRIVER INTO REGISTRY
     * Add a entry in the registry for our dummy driver.
     */
    if (!pci_driver_registry_add(driver_registry,
                                 &pci_dummy_net_class_driver)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_driver_test failed. Failed to register driver "
                   "%s into registry.\n",
                   &pci_dummy_net_class_driver.name);
        return 0;
    }

    /** BIND THE DEVICE FUNCTION RECORD WITH A DRIVER ENTRY
     * 1. Probe and bind all the record entries with matching driver (match
     * criteria for now is kept simple - same vendor/device ids)
     * 2. For all entries in the record registry initialize a Device Object.
     * pci_device_t -> A PCI device object
     * -> device_t is an extension of generic device object contains pointer to
     * bound driver object, device name, parent device etc.
     *  -> function record for that device.
     * 3. Try to bind devicce object with a driver from driver registry.
     * 4. if a device and driver matches - call the probe routine of the driver.
     */
    bound_count = pci_probe_and_bind_all(device_registry, driver_registry,
                                         &test_bind_summary);

    if (bound_count == 0) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_device_driver_test failed. Failed to bind driver %s with a "
            "device.\n",
            pci_dummy_net_class_driver.name);
        return 0;
    }

    pci_dump_bind_summary(&test_bind_summary);

    /* try to find a record with bdf to find */
    device_const = pci_device_registry_find_bdf(device_registry, bdf);
    device = (pci_device_t *)device_const;
    if (!device_const) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_driver_test failed. Failed to find record for "
                   "%02x:%02x:%u.\n",
                   bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    /* Device should have been bounded to driver in probe & bind phase */
    if (device_const->device.state != DEVICE_STATE_BOUND) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_device_driver_test failed. Device %s state is un-bounded.\n",
            device_const->device.name);
        return 0;
    }

    /* Device should have the test driver attached */
    if (device_const->device.bound_driver == NULL) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_driver_test failed. Device %s is bounded but "
                   "bound_driver is invalid.\n",
                   device_const->device.name);
        return 0;
    }

    /* Device should have updates the driver_data field during probe */
    if (device_const->device.driver_data == NULL) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_driver_test failed. Device %s is not associated "
                   "with a valid driver data.\n",
                   device_const->device.name);
        return 0;
    }

    data =
        (pci_dummy_net_class_driver_data_t *)pci_device_get_driver_data(device);

    KLOG_INFO(
        "DEVICE_TEST",
        "pci_device_driver_test passed. success bound driver %s to device %s. "
        "\n\tDriver data information: mmio base = %08x, io base "
        "= %08x, bus master enabled = %u.\n",
        pci_dummy_net_class_driver.name, device_const->device.name,
        data->mmio_bar_base, data->io_bar_base, data->bus_master_enabled);

    return 1;
}

uint8_t device_pci_device_registry_lookup_test(void) {
    pci_record_registry_t *record_registry = &test_record_reg;
    pci_driver_registry_t *driver_registry = &test_driver_reg;
    pci_device_registry_t *device_registry = &test_device_reg;
    const pci_device_t *device_const;
    const pci_device_t *device;
    uint8_t bound_count = 0;
    const pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    pci_dummy_net_class_driver_data_t *data;

    /** BUILDS UP THE RECORD REGISTRY
     * 1. Probe all the devices in bus 0. Probe all the function in each device.
     * 2. In probe function it reads the value from the config space and
     * populates the function identity field of the function record.
     * 3. If a function has valid entries a call back add the function identity
     * of function record in the registry. other fields of function record -
     * like BAR, command status, capabilities are jsut intialized to default.
     * 4. While probing slot, check if its a multifunctional slot. ie.
     * (bridge)device has multiple functions.
     * 5. If multifunctional - then probe all the functions 1-7 for that slot.
     * They all will be added to registry too.
     * 6. NOTE: We just have populated the function identity in the function
     * record of the registry. Need to enrich BAR/command/capabilities values
     * registry_ctx
     * -> pointer to the registry. Registry contains array of function records.
     * -> Number of function records successfully insterted in the registry.
     * -> Number of function records unsuccessfull in inserting in registry.
     */
    if (!pci_enumerate_bus0_into_record_registry(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_lookup_test failed. Failed to "
                   "enumerate bus0 into registry.\n");
        return 0;
    }

    /** ENRICH THE RECORD REGISTRY
     * 1. for each record entry in the registry it will decode the bits from the
     * config space and update the structs with meaningful value.
     * 2. Decode and update the BAR information for each record.
     * 3. Decode and update the Command/Status bits for each record.
     * 4. Decode and update the Capability list for each record.
     */
    if (!pci_enrich_record_registry_resources(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_lookup_test failed. Failed to "
                   "enrich the registry record.\n");
        return 0;
    }

    /** BUILD PCI DEVICE REGISTRY FROM RECORD REGISTRY
     * 1. initialize the device registry.
     * 2. for each record entry in the registry initalize a device object with
     * the device name format "pci-bus:device:function"
     * 3. add the device object in the device registry.
     */
    if (!pci_device_registry_materialize_from_record_registry(
            device_registry, record_registry, NULL)) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_lookup_test failed. failed to "
                   "materialize device registry using the record registry.\n");
        return 0;
    }

    /** INITIALIZE THE DRIVER REGISTRY
     * driver_registry ->
     * -> point to the array of driver object.
     *  Each driver object contains name, ids, probe routine.
     * -> number of driver objects in the registry
     *  Post init, there is no driver registered in the registry.
     */
    pci_driver_registry_init(driver_registry);

    /** REGISTER DUMMY 1000E DRIVER INTO REGISTRY
     * Add a entry in the registry for our dummy driver.
     */
    if (!pci_driver_registry_add(driver_registry,
                                 &pci_dummy_net_class_driver)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "pci_device_registry_lookup_test failed. Failed to register driver "
            "%s into registry.\n",
            &pci_dummy_net_class_driver.name);
        return 0;
    }

    /** BIND THE DEVICE FUNCTION RECORD WITH A DRIVER ENTRY
     * 1. Probe and bind all the record entries with matching driver (match
     * criteria for now is kept simple - same vendor/device ids)
     * 2. For all entries in the record registry initialize a Device Object.
     * pci_device_t -> A PCI device object
     * -> device_t is an extension of generic device object contains pointer to
     * bound driver object, device name, parent device etc.
     *  -> function record for that device.
     * 3. Try to bind devicce object with a driver from driver registry.
     * 4. if a device and driver matches - call the probe routine of the driver.
     */
    bound_count = pci_probe_and_bind_all(device_registry, driver_registry,
                                         &test_bind_summary);

    if (bound_count == 0) {
        KLOG_ERROR("DEVICE_TEST",
                   "pci_device_registry_lookup_test failed. Failed to bind "
                   "driver %s with a "
                   "device.\n",
                   pci_dummy_net_class_driver.name);
        return 0;
    }

    pci_dump_bind_summary(&test_bind_summary);

    pci_device_t *dev_bdf, *dev_vd, *dev_bound;
    uint32_t bound_state_count = 0;

    dev_bdf = pci_device_registry_find_bdf(device_registry, bdf);
    dev_vd =
        pci_device_registry_find_vendor_device(device_registry, 0x8086, 0x100e);
    dev_bound = pci_device_registry_find_bound_vendor_device(device_registry,
                                                             0x8086, 0x100e);
    bound_state_count =
        pci_device_registry_state_count(device_registry, DEVICE_STATE_BOUND);

    if (!dev_bdf || !dev_vd || !dev_bound) {
        KLOG_ERROR("DEVICE_TEST", "pci_device_registry_lookup_test failed. "
                                  "Failed to lookup device.\n");
        return 0;
    }

    if (dev_bound->device.state != DEVICE_STATE_BOUND ||
        dev_bound->device.bound_driver == NULL) {
        KLOG_ERROR("DEVICE_TEST", "pci_device_registry_lookup_test failed. "
                                  "bound device state is invalid.\n");
        return 0;
    }

    if (bound_state_count == 0) {
        KLOG_ERROR("DEVICE_TEST", "pci_device_registry_lookup_test failed. "
                                  "no bound device count.\n");
        return 0;
    }

    pci_dump_device_registry(device_registry);

    KLOG_INFO("DEVICE_TEST",
              "pci_device_registry_lookup_test succeeded. success bound count "
              "= %u, device = %s.\n",
              bound_state_count, dev_bound->device.name);

    return 1;
}

/* check and compare the driver match score of a device with different driver */
uint8_t device_pci_driver_match_priority_test(void) {
    pci_record_registry_t *record_reg = &test_record_reg;
    const pci_function_record_t *record_const;
    pci_function_record_t *record;
    pci_device_t dev;
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};

    /* enumerate and add entries to the record registry */
    if (!pci_enumerate_bus0_into_record_registry(record_reg)) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_match_priority failed. failed to "
                   "enumerate records into registry.\n");
        return 0;
    }

    /* enrich the record registry with values */
    if (!pci_enrich_record_registry_resources(record_reg)) {
        KLOG_ERROR("DEVICE_TEST", "device_pci_driver_match_priority failed. "
                                  "failed to enrich record registry.\n");
        return 0;
    }

    /* find a function record from the record registry */
    record_const = pci_record_registry_find_bdf(record_reg, bdf);
    record = (pci_function_record_t *)record_const;

    /* initialize a PCI device using the record */
    if (!pci_device_init(&dev, record, NULL)) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_match_priority failed. failed to "
                   "initialize device [%02x:%02x.%u].\n",
                   bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    /* claculate the mathching score for 2 different driver with the device */
    pci_match_score_t class_match_score =
        pci_driver_match_score(&pci_dummy_net_class_driver, &dev);
    pci_match_score_t vendor_match_score =
        pci_driver_match_score(&pci_dummy_driver, &dev);

    /* check if match score is as expected for first driver */
    if (class_match_score != PCI_MATCH_SCORE_CLASS_SUBCLASS_PROGIF) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_match_priority failed. class score = %u, "
                   "expected = %u.\n",
                   class_match_score, PCI_MATCH_SCORE_CLASS_SUBCLASS_PROGIF);
        return 0;
    }

    /* check if match score is as expected for second driver */
    if (vendor_match_score != PCI_MATCH_SCORE_VENDOR_DEVICE) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_match_priority failed. vendor_device "
                   "score = %u, expected = %u.\n",
                   vendor_match_score, PCI_MATCH_SCORE_VENDOR_DEVICE);
        return 0;
    }

    /* check if match score for second driver with vendor_device match rule
     * should beat the score of first driver with class_subclass_progif rule */
    if (vendor_match_score < class_match_score) {
        KLOG_ERROR("DEVICE_TEST",
                   "device_pci_driver_match_priority failed. vendor_device "
                   "score (%u) should beat the class score (%u).\n",
                   vendor_match_score, class_match_score);
        return 0;
    }

    KLOG_INFO("DEVICE_TEST",
              "device_pci_driver_match_priority passed. vendor_device score = "
              "%u wins against class score = %u.\n",
              vendor_match_score, class_match_score);
    return 1;
}

/* check the linkage between the device and record registry */
uint8_t device_pci_registry_lifetime_links_test(void) {
    pci_record_registry_t *record_registry = &test_record_reg;
    pci_device_registry_t *device_registry = &test_device_reg;
    const pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    const pci_function_record_t *rec_const;
    pci_function_record_t *rec;
    pci_device_t *dev_from_registry;
    pci_device_t *dev_from_record;

    if (!pci_enumerate_bus0_into_record_registry(record_registry)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "registry_lifetime_links_test failed: enumeration failed.\n");
        return 0;
    }

    if (!pci_enrich_record_registry_resources(record_registry)) {
        KLOG_ERROR("DEVICE_TEST",
                   "registry_lifetime_links_test failed: enrichment failed.\n");
        return 0;
    }

    if (!pci_device_registry_materialize_from_record_registry(
            device_registry, record_registry, NULL)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "registry_lifetime_links_test failed: materialization failed.\n");
        return 0;
    }

    if (!pci_device_registry_validate_links(device_registry, record_registry)) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "registry_lifetime_links_test failed: link validation failed.\n");
        return 0;
    }

    rec_const = pci_record_registry_find_bdf(record_registry, bdf);
    if (!rec_const) {
        KLOG_ERROR("DEVICE_TEST",
                   "registry_lifetime_links_test failed: record not found.\n");
        return 0;
    }

    rec = (pci_function_record_t *)rec_const;

    dev_from_registry = pci_device_registry_find_bdf(device_registry, bdf);
    dev_from_record = pci_record_get_runtime_device(rec);

    if (!dev_from_registry || !dev_from_record) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "registry_lifetime_links_test failed: device lookup failed.\n");
        return 0;
    }

    if (dev_from_registry != dev_from_record) {
        KLOG_ERROR("DEVICE_TEST",
                   "registry_lifetime_links_test failed: registry device and "
                   "record runtime_device differ.\n");
        return 0;
    }

    if (dev_from_registry->record != rec) {
        KLOG_ERROR(
            "DEVICE_TEST",
            "registry_lifetime_links_test failed: device record mismatch.\n");
        return 0;
    }

    if (dev_from_registry->device.bus_data != rec) {
        KLOG_ERROR("DEVICE_TEST",
                   "registry_lifetime_links_test failed: bus_data mismatch.\n");
        return 0;
    }

    KLOG_INFO("DEVICE_TEST",
              "registry_lifetime_links_test passed: record [%02x:%02x.%u] "
              "<-> device %s links are stable.\n",
              bdf.bus, bdf.device, bdf.function,
              dev_from_registry->device.name);

    return 1;
}

/** *** END: Dummy Device <> Driver linkage tests *** */
