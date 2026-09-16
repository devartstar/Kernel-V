#include "arch/x86/pci/pci_cap.h"
#include "arch/x86/pci/pci_dump.h"
#include "arch/x86/pci/pci_record_registry.h"

void pci_capability_state_init(struct pci_function_record *rec) {
    /* check if pointer to function record is valid */
    if (!rec) {
        KLOG_ERROR("PCI", "invalid reference to the function record.\n");
        return;
    }

    for (uint8_t i = 0; i < PCI_CAP_MAX_PER_FUNCTION; i++) {
        pci_capability_info_init(&rec->caps[i]);
    }

    rec->caps_valid = 0;
    rec->caps_present = 0;
    rec->cap_count = 0;
}

uint8_t pci_capability_enrich_presence(struct pci_function_record *rec) {
    /* check if pointer to function record is valid */
    if (!rec || !rec->present) {
        KLOG_ERROR("PCI", "failed enriching presence. invalid reference to the "
                          "function record or record not present.\n");
        return 0;
    }

    pci_capability_state_init(rec);

    rec->caps_valid = 1;
    rec->caps_present =
        (uint8_t)pci_capability_list_prsent_in_fuction(rec->id.bdf);

    return 1;
}

uint8_t pci_capability_offset_seen(const pci_function_record_t *rec,
                                   uint8_t offset) {
    /* check if pointer to function record is valid */
    if (!rec) {
        KLOG_ERROR("PCI", "invalid reference to the function record.\n");
        return 0;
    }

    for (uint8_t i = 0; i < rec->cap_count; i++) {
        if (rec->caps[i].present && rec->caps[i].offset == offset) {
            return 1;
        }
    }

    return 0;
}

uint8_t pci_capability_enrich_records(pci_function_record_t *rec) {
    uint8_t ptr;
    pci_bdf_t bdf;

    /* check if referce to recoed is valid */
    if (!rec || !rec->present) {
        KLOG_ERROR("PCI", "failed enriching record. invalid reference to the "
                          "function record or record not present.\n");
        return -1;
    }

    bdf.bus = rec->id.bdf.bus;
    bdf.device = rec->id.bdf.device;
    bdf.function = rec->id.bdf.function;

    /* Initialize the capability info in the record with default values */
    pci_capability_state_init(rec);

    /* set record as valid since we have looked into it */
    rec->caps_valid = 1;

    /* if the record has no capability list - still return success as valid case
     */
    if (!pci_capability_list_prsent_in_fuction(bdf)) {
        rec->caps_present = 0;
        KLOG_WARN("PCI", "[%02x:%02x.%u] doesn't have capability list.\n",
                  bdf.bus, bdf.device, bdf.function);
        return 1;
    }

    rec->caps_present = 1;

    /* read the pointer to first entry in capability list */
    ptr = pci_cfg_read8(bdf, PCI_CFG_CAP_PTR);

    while (ptr != 0) {
        uint8_t capability_id;
        uint8_t next_capability;
        pci_capability_info_t *capability_info;

        /* check if pointer to current capability is valid */
        if (pci_capability_ptr_valid(ptr)) {
            KLOG_WARN("PCI",
                      "[%02x:%02x.%u] invalid capability pointer 0x%02x.\n",
                      bdf.bus, bdf.device, bdf.function, ptr);
            break;
        }

        /* check if number of capabilities discovered is more than max count */
        if (rec->cap_count >= PCI_CAP_MAX_PER_FUNCTION) {
            KLOG_WARN(
                "PCI",
                "[%02x:%02x.%u] capability list is truncated at %u entries.\n",
                bdf.bus, bdf.device, bdf.function, rec->cap_count);
            break;
        }

        /* check for loops, exit if we have alredy seen this capability */
        if (pci_capability_offset_seen(rec, ptr)) {
            KLOG_WARN(
                "PCI",
                "[%02x:%02x.%u] capability list loop/duplicated at 0x%02x.\n",
                bdf.bus, bdf.device, bdf.function, ptr);
            break;
        }

        /* first byte is the capability header is id and second byte is next
         * capability pointer */
        capability_id = pci_cfg_read8(rec->id.bdf, ptr + 0);
        next_capability = pci_cfg_read8(rec->id.bdf, ptr + 1);

        /* populate the capability info */
        capability_info = &rec->caps[rec->cap_count];
        capability_info->present = 1;
        capability_info->id = capability_id;
        capability_info->next = next_capability;
        capability_info->offset = ptr;
        capability_info->kind = pci_capability_kind_from_id(capability_id);

        rec->cap_count++;

        ptr = next_capability;
    }

    return 1;
}

const pci_capability_info_t *
pci_capability_find_kind(const struct pci_function_record *rec,
                         pci_cap_kind_t kind) {
    /* check if reference to the function record is valid */
    if (!rec || !rec->present || !rec->caps_valid || !rec->caps_present) {
        KLOG_ERROR(
            "PCI",
            "Failed to find capability of kind %s. Invalid record reference.\n",
            pci_capability_kind_name(kind));
        return NULL;
    }

    for (uint8_t i = 0; i < rec->cap_count; i++) {
        const pci_capability_info_t *cap = &rec->caps[i];

        /* check if current capability is present and matches kind */
        if (cap->present && cap->kind == kind) {
            return cap;
        }
    }
    return NULL;
}
