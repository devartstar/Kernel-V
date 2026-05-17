#include "tests/test_pci.h"

uint8_t pci_cfg_smoke_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x0, .function = 0x00};

    uint32_t id_dword = pci_cfg_read32(bdf, 0x00);
    uint32_t class_dword = pci_cfg_read32(bdf, 0x08);
    uint32_t hdr_dword = pci_cfg_read32(bdf, 0x0C);

    if (id_dword == 0xFFFFFFFF || class_dword == 0xFFFFFFFF ||
        hdr_dword == 0xFFFFFFFF) {
        KLOG_ERROR("PCI_TEST", "cfg_smoke_test"
                               "Config read all bits set to 1\n");
        return 0;
    }

    /*
     * this reads the 4B from the config space
     * we need to extract the necessary information
     */

    KLOG_INFO("PCI_TEST",
              "cfg_smoke_test"
              "\n\tconfig read at BDF %02x.%02x.%u"
              "\n\tid=%08x, class=%08x, hdr=%08x\n",
              bdf.bus, bdf.device, bdf.function, id_dword, class_dword,
              hdr_dword);
    return 1;
}

uint8_t pci_cfg_extract_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x0, .function = 0x00};

    uint16_t vendor = pci_cfg_read16(bdf, 0x00);
    uint16_t device = pci_cfg_read16(bdf, 0x02);
    uint8_t revision = pci_cfg_read8(bdf, 0x08);
    uint8_t prog_if = pci_cfg_read8(bdf, 0x09);
    uint8_t subclass = pci_cfg_read8(bdf, 0x0A);
    uint8_t classc = pci_cfg_read8(bdf, 0x0B);
    uint8_t hdr_type = pci_cfg_read8(bdf, 0x0E);

    if (vendor == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI_TEST", "cfg_extract_test"
                               "Config read invalid vendor ID\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "extract_test"
              "\n\tconfig read at BDF %02x:%02x.%u"
              "\n\tvendor=%04x device=%04x class=%02x subclass=%02x "
              "progif=%02x rev=%02x hdr=%02x\n",
              bdf.bus, bdf.device, bdf.function, vendor, device, classc,
              subclass, prog_if, revision, hdr_type);

    return 1;
}

uint8_t pci_cfg_decode_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x00, .function = 0x00};

    uint32_t id_dword = pci_cfg_read32(bdf, PCI_CFG_VENDOR_ID);
    uint32_t class_dword = pci_cfg_read32(bdf, PCI_CFG_REVISION_ID);
    uint32_t hdr_dword = pci_cfg_read32(bdf, PCI_CFG_CACHELINE_SIZE);

    uint16_t vendor_id = pci_dword_lo16(id_dword);
    uint16_t device_id = pci_dword_hi16(id_dword);

    uint8_t revision_id = pci_dword_byte0(class_dword);
    uint8_t prog_if = pci_dword_byte1(class_dword);
    uint8_t subclass = pci_dword_byte2(class_dword);
    uint8_t class_code = pci_dword_byte3(class_dword);

    uint8_t header_type = pci_dword_byte2(hdr_dword);

    if (vendor_id == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI_TEST", "cfg_decode_test"
                               "Config read invalid vendor ID\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "decode_test"
              "\n\tconfig read at BDF %02x:%02x.%u"
              "\n\tvendor=%04x device=%04x class=%02x "
              "subclass=%02x progif=%02x rev=%02x hdr=%02x multi=%u\n",
              bdf.bus, bdf.device, bdf.function, vendor_id, device_id,
              class_code, subclass, prog_if, revision_id,
              (header_type & PCI_CFG_HEADER_TYPE_MASK),
              pci_cfg_header_type_is_multifunctional(header_type));

    return 1;
}

uint8_t pci_probe_function_test() {
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x00, .function = 0x00};
    pci_function_identity_t id;
    pci_probe_result_t res;

    res = pci_probe_function(bdf, &id);
    if (res == PCI_PROBE_ERROR) {
        KLOG_ERROR(
            "PCI_TEST",
            "probe_function_test error while probing bdf %02x:%02x.%u.\n",
            bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    if (res == PCI_PROBE_ABSENT) {
        KLOG_ERROR(
            "PCI_TEST",
            "probe_function_test function not present bfd %02x:%02x.%u.\n",
            bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "probe_function_test"
              "\n\tFunction at endpoint bdf %02x:%02x.%u."
              "\n\tvendor=%04x device=%04x class=%02x subclass=%02x "
              "progif=%02x rev=%02x hdr=%02x multi=%u layout=%02x\n",
              id.bdf.bus, id.bdf.device, id.bdf.function, id.vendor_id,
              id.device_id, id.class_code, id.subclass, id.prog_if,
              id.revision_id, id.header_type,
              pci_cfg_header_type_is_multifunctional(id.header_type),
              pci_cfg_header_type_layout(id.header_type));

    return 1;
}

typedef struct pci_slot_test_ctx {
    uint32_t count;
} pci_slot_test_ctx_t;

static void pci_slot_test_visitor(const pci_function_identity_t *id,
                                  void *ctx) {
    pci_slot_test_ctx_t *test_ctx = (pci_slot_test_ctx_t *)ctx;
    test_ctx->count++;

    KLOG_VERBOSE("PCI_TEST",
                 "slot visitor %02x:%02x.%u vendor=%04x device=%04x class=%02x "
                 "subclass=%02x hdr=%02x multi=%u\n",
                 id->bdf.bus, id->bdf.device, id->bdf.function, id->vendor_id,
                 id->device_id, id->class_code, id->subclass, id->header_type,
                 pci_cfg_header_type_is_multifunctional(id->header_type));
}

uint8_t pci_probe_slot_test() {
    pci_slot_test_ctx_t ctx = {.count = 0};
    pci_probe_result_t res;
    uint32_t fn_count = 0;

    res = pci_probe_slot(0x00, 0x00, pci_slot_test_visitor, &ctx, &fn_count);

    if (res == PCI_PROBE_ABSENT) {
        KLOG_ERROR(
            "PCI_TEST",
            "probe_slot_test failed to probe slot 00:00. SLOT ABSENT.\n");
        return 0;
    }

    if (res == PCI_PROBE_ERROR) {
        KLOG_ERROR("PCI_TEST", "probe_slot_test failed to probe slot 00:00. "
                               "SLOT PROBING ERROR.\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "probe_slot_test 00:00 result SUCCESS, Next comparing counts.\n");

    if (fn_count != ctx.count) {
        KLOG_ERROR("PCI_TEST",
                   "probe_slot_test mismatch function count reported: probe "
                   "%u, context %u.\n",
                   fn_count, ctx.count);
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "probe_slot_test function count matches: probe %u, context %u.\n",
              fn_count, ctx.count);
    return 1;
}

typedef struct pci_bus0_test_ctx {
    uint32_t callback_count;
} pci_bus0_test_ctx_t;

static void pci_bus0_test_visitor(const pci_function_identity_t *id,
                                  void *ctx) {
    pci_bus0_test_ctx_t *test_ctx = (pci_bus0_test_ctx_t *)ctx;
    test_ctx->callback_count++;

    KLOG_INFO(
        "PCI_TEST",
        "bus0 visitor %02x:%02x.%u vendor=%04x device=%04x class=%02x "
        "subclass=%02x progif=%02x rev=%02x hdr=%02x multi=%u layout=%02x\n",
        id->bdf.bus, id->bdf.device, id->bdf.function, id->vendor_id,
        id->device_id, id->class_code, id->subclass, id->prog_if,
        id->revision_id, id->header_type,
        pci_cfg_header_type_is_multifunctional(id->header_type),
        pci_cfg_header_type_layout(id->header_type));
}

uint8_t pci_scan_bus0_test() {
    pci_bus0_test_ctx_t ctx = {.callback_count = 0};
    pci_probe_result_t res;
    uint32_t total_found;

    res = pci_scan_bus0(pci_bus0_test_visitor, &ctx, &total_found);

    if (res == PCI_PROBE_ERROR) {
        KLOG_ERROR("PCI_TEST", "scan_bus0_test failed to scan bus 00. "
                               "BUS SCANNING ERROR.\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "scan_bus0_test 00 result SUCCESS, Next comparing counts.\n");

    if (total_found != ctx.callback_count) {
        KLOG_ERROR("PCI_TEST",
                   "scan_bus0_test mismatch total function count in bus 00. "
                   "probe: %u, context %u.\n",
                   total_found, ctx.callback_count);
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "scan_bus0_test total function count in bus 00 matches: probe "
              "%u, context %u.\n",
              total_found, ctx.callback_count);
    return 1;
}

uint8_t pci_registry_bus0_test() {
    pci_registry_t test_reg;

    if (!pci_enumerate_bus0_into_registry(&test_reg)) {
        KLOG_ERROR("PCI_TEST",
                   "registry_bus0_test: enumeration into registry failed.\n");
        return 0;
    }

    if (test_reg.count == 0) {
        KLOG_ERROR("PCI_TEST", "registry_bus0_test: registry count is 0 after "
                               "bus0 enumeration.\n");
        return 0;
    }

    /* sanity check: iterate through all the registered functions. */
    for (uint32_t i = 0; i < test_reg.count; i++) {
        /* it entry added but not marked present. */
        if (!test_reg.entries[i].present) {
            KLOG_ERROR("PCI_TEST",
                       "registry_bus0_test: entry %u is not marked present.\n",
                       i);
            return 0;
        }

        /* check for duplicate entries of a function identity. */
        for (uint32_t j = i + 1; j < test_reg.count; j++) {
            if (test_reg.entries[j].present &&
                pci_bdf_is_equal(test_reg.entries[i].id.bdf,
                                 test_reg.entries[j].id.bdf)) {
                KLOG_ERROR("PCI_TEST",
                           "registry_bus0_test: duplicate BDF %02x:%02x.%u "
                           "at entries %u and %u.\n",
                           test_reg.entries[i].id.bdf.bus,
                           test_reg.entries[i].id.bdf.device,
                           test_reg.entries[i].id.bdf.function, i, j);
                return 0;
            }
        }
    }

    KLOG_INFO(
        "PCI_TEST",
        "registry_bus0_test: registry count=%u and BDF uniqueness verified.\n",
        test_reg.count);
    return 1;
}

typedef struct pci_registry_dump_ctx {
    uint32_t seen;
} pci_registry_dump_ctx_t;

void pci_registry_dump_test_visitor(pci_function_record_t *record, void *ctx) {
    /* structure the memory into type pci_registry_dump_ctx */
    pci_registry_dump_ctx_t *count = (pci_registry_dump_ctx_t *)ctx;

    /* check for valid pointer to record and it should be present and pointer to
     * context should be valid */
    if (!record || !record->present || !count) {
        KLOG_ERROR("PCI_TEST", "invalid input params for callback.\n");
        return;
    }

    count->seen++;
}

uint8_t pci_dump_registry_test() {
    pci_registry_t test_reg;
    pci_registry_dump_ctx_t test_ctx = {.seen = 0};

    /* enumerate bus 0 to populate the registr structure */
    if (!pci_enumerate_bus0_into_registry(&test_reg)) {
        KLOG_ERROR("PCI_TEST",
                   "dump_registry_test: enumeration into registry failed.\n");
        return 0;
    }

    /* dump the registry structure */
    pci_dump_registry(&test_reg);

    /* count the total number of function registered/dumped */
    pci_registry_foreach(&test_reg, pci_registry_dump_test_visitor, &test_ctx);

    /* sanity check for the functions dumped by registry matches the resitry
     * record */
    if (test_ctx.seen != test_reg.count) {
        KLOG_ERROR(
            "PCI_TEST",
            "dump_registry_test: iteration count mismatch seen=%u red=%u.\n",
            test_ctx.seen, test_reg.count);
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "dump_registry_test: dump successful and iteration count (%u) "
              "verified.\n",
              test_reg.count);

    return 1;
}
