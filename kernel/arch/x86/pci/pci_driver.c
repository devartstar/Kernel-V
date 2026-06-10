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
                   "match rule failed for device. invalid input args.\n");
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

uint8_t pci_driver_matches_boolean(const pci_driver_t *pci_driver,
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

pci_match_score_t
pci_driver_rule_match_device_score(const pci_match_rule_t *rule,
                                   const pci_device_t *pci_device) {
    const pci_function_identity_t *id;
    uint8_t match_vendor, match_device;
    uint8_t match_class, match_subclass, match_progif;

    if (!rule || !pci_device || !pci_device->record) {
        KLOG_ERROR("PCI_DRIVER",
                   "match rule failed for device. invalid input args.\n");
        return PCI_MATCH_SCORE_NONE;
    }

    id = &pci_device->record->id;

    switch (rule->type) {
    case PCI_MATCH_VENDOR_DEVICE:
        match_vendor = pci_match_u16(rule->vendor_id, id->vendor_id);
        match_device = pci_match_u16(rule->device_id, id->device_id);
        if (!match_vendor && !match_device)
            return PCI_MATCH_SCORE_NONE;
        else if (match_vendor && !match_device)
            return PCI_MATCH_SCORE_VENDOR_ONLY;
        else if (match_vendor && match_device)
            return PCI_MATCH_SCORE_VENDOR_DEVICE;
        else
            return PCI_MATCH_SCORE_NONE;

    case PCI_MATCH_CLASS:
        match_class = pci_match_u8(rule->class_code, id->class_code);
        match_subclass = pci_match_u8(rule->subclass, id->subclass);
        match_progif = pci_match_u8(rule->prog_if, id->prog_if);
        if (!match_class && !match_subclass && !match_progif)
            return PCI_MATCH_SCORE_NONE;
        else if (match_class && !match_subclass && !match_progif)
            return PCI_MATCH_SCORE_CLASS_ONLY;
        else if (match_class && match_subclass && !match_progif)
            return PCI_MATCH_SCORE_CLASS_SUBCLASS;
        else if (match_class && match_subclass && match_progif)
            return PCI_MATCH_SCORE_CLASS_SUBCLASS_PROGIF;
        else
            return PCI_MATCH_SCORE_NONE;

    default:
        return PCI_MATCH_SCORE_NONE;
    }
}

pci_match_score_t pci_driver_match_score(const pci_driver_t *pci_driver,
                                         const pci_device_t *pci_device) {
    pci_match_score_t best_score = PCI_MATCH_SCORE_NONE;

    if (!pci_driver || !pci_driver->matches || pci_driver->match_count == 0) {
        KLOG_ERROR("PCI_DRIVER", "matching driver failed. Invalid ref. to the "
                                 "driver or no match rule in driver.\n");
        return PCI_MATCH_SCORE_NONE;
    }

    if (!pci_device || !pci_device->record || !pci_device->record->present ||
        !pci_device->device.name) {
        KLOG_ERROR("PCI_DRIVER",
                   "matching driver failed. Invalid ref. to the "
                   "device to be matched to driver %s.\n",
                   pci_driver->name);
        return PCI_MATCH_SCORE_NONE;
    }

    /* iterate thru all the rules in the driver
     * instead of returning on first match get the match score
     * return the maximum match score */
    for (uint8_t i = 0; i < pci_driver->match_count; i++) {
        const pci_match_rule_t *rule = &pci_driver->matches[i];
        pci_match_score_t score;

        score = pci_driver_rule_match_device_score(rule, pci_device);
        if (score > best_score) {
            best_score = score;
        }
    }

    return best_score;
}

uint8_t pci_driver_matches(const pci_driver_t *pci_driver,
                           const pci_device_t *pci_device) {
    uint8_t do_boolean_match = 0;

    if (do_boolean_match) {
        return pci_driver_matches_boolean(pci_driver, pci_device);
    }

    return pci_driver_match_score(pci_driver, pci_device) !=
           PCI_MATCH_SCORE_NONE;
}
