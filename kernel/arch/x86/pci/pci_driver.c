#include "arch/x86/pci/pci_driver.h"

/**
 * pci_match_u16 - match a 16 bit value between a match rule and a device
 * @rule_value - 16 bit value to match from the rule object in driver
 * @dev_value - 16 bit value to match from the device object
 *
 * @return - 1 if the values matches for rule value is set to match
 * unconditionally
 */
static uint8_t pci_match_u16(uint16_t rule_value, uint16_t dev_value) {
    return rule_value == PCI_MATCH_ANY_U16 || rule_value == dev_value;
}

/**
 * pci_match_u8 - match a 8 bit value between a match rule and a device
 * @rule_value - 8 bit value to match from the rule object in driver
 * @dev_value - 8 bit value to match from the device object
 *
 * @return - 1 if the values matches for rule value is set to match
 * unconditionally
 */
static uint8_t pci_match_u8(uint8_t rule_value, uint8_t dev_value) {
    return rule_value == PCI_MATCH_ANY_U8 || rule_value == dev_value;
}

uint8_t pci_driver_rule_match_device(const pci_match_rule_t *rule,
                                     const pci_device_t *pci_device) {

    const pci_function_identity_t *id;

    /* check validity of the input arguments */
    if (!rule || !pci_device || !pci_device->record ||
        !pci_device->record->present) {
        KLOG_ERROR("PCI_DRIVER",
                   "match rule failed for device %s. invalid input args.\n",
                   pci_device->device.name);
        return 0;
    }

    id = &pci_device->record->id;

    switch (rule->type) {
    case PCI_MATCH_VENDOR_DEVICE:
        return pci_match_u16(rule->vendor_id, id->vendor_id) &&
               pci_match_u16(rule->device_id, id->device_id);
    case PCI_MATCH_CLASS:
        return pci_match_u8(rule->class_code, id->class_code) &&
               pci_match_u8(rule->subclass, id->subclass) &&
               pci_match_u8(rule->prog_if, id->prog_if);
    default:
        return 0;
    }
}

uint8_t pci_driver_matches(const pci_driver_t *pci_driver,
                           const pci_device_t *pci_device) {
    /* check for valid reference to device and priver obect */
    if (!pci_driver || !pci_device || !pci_device->device.name ||
        !pci_device->record || !pci_driver->matches ||
        pci_driver->match_count == 0) {
        KLOG_ERROR("PCI_DRIVER",
                   "maching driver failed. invalid arguemnts passed.\n");
        return 0;
    }

    /* iterate thru the match rules for the driver and bind to all the
     * devices which match with them */
    for (uint8_t i = 0; i < pci_driver->match_count; i++) {
        /* get the match rules for this driver */
        const pci_match_rule_t *rule = &pci_driver->matches[i];

        /* check if the device match with the driver associated rule */
        if (pci_driver_rule_match_device(rule, pci_device)) {
            KLOG_VERBOSE("PCI_DRIVER",
                         "driver %s matched with device %s at %02x:%02x.%u "
                         "using rule %u (type=%u).\n",
                         pci_driver->name, pci_device->device.name,
                         pci_device->record->id.bdf.bus,
                         pci_device->record->id.bdf.device,
                         pci_device->record->id.bdf.function, i, rule->type);
            return 1;
        }
    }

    KLOG_ERROR("PCI_DRIVER",
               "driver %s did not match with device %s at %02x:%02x.%u "
               "using any rule.\n",
               pci_driver->name, pci_device->device.name,
               pci_device->record->id.bdf.bus,
               pci_device->record->id.bdf.device,
               pci_device->record->id.bdf.function);

    return 0;
}
