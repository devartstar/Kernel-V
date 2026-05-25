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

void pci_registry_dump_test_visitor(const pci_function_record_t *record,
                                    void *ctx) {
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
    const pci_registry_t *test_reg_cpy = &test_reg;
    pci_dump_registry(test_reg_cpy);

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

uint8_t pci_basic_validation() {
    pci_registry_t test_reg;

    /* add all the function endpoints to registry */
    uint8_t success = pci_enumerate_bus0_into_registry(&test_reg);
    if (!success || (success && test_reg.count == 0)) {
        KLOG_ERROR("PCI_TEST", "basic_validation: pci registration of "
                               "functions under bus0 failed.\n");
        return 0;
    }

    /* find a bdf in the registry entry */
    pci_bdf_t bdf_to_find = {.bus = 0x00, .device = 0x00, .function = 0x00};
    pci_function_record_t *rec;
    rec = pci_registry_find_bdf(&test_reg, bdf_to_find);
    if (!rec) {
        KLOG_ERROR(
            "PCI_TEST",
            "basic_validation: find utility provided with invalid record.\n");
        return 0;
    }

    if (rec->id.vendor_id == PCI_INVALID_VENDOR_ID) {
        KLOG_ERROR("PCI_TEST",
                   "basic_valiation: found record at %0x2:%0x2.%u with invalid "
                   "vendor id.\n",
                   bdf_to_find.bus, bdf_to_find.device, bdf_to_find.function);
        return 0;
    }

    /* log the registry record found */
    pci_dump_registry(&test_reg);
    KLOG_INFO("PCI_TEST",
              "basic_validation: successfully dumped the registry list.\n");

    return 1;
}

uint8_t pci_type0_raw_bars_test() {
    pci_registry_t test_reg;
    pci_bdf_t bdf_to_find = {.bus = 0x00, .device = 0x00, .function = 0x00};
    const pci_function_record_t *test_record0;
    pci_function_record_t *test_record;

    /* enumerate bus0 and register the records */
    if (!pci_enumerate_bus0_into_registry(&test_reg)) {
        KLOG_ERROR("PCI_TEST", "type0_raw_bars_test: failed to enumrate bus 0 "
                               "and add records to registry.\n");
        return 0;
    }

    /* find a record from the registry using bdf */
    test_record0 = pci_registry_find_bdf(&test_reg, bdf_to_find);
    test_record = (pci_function_record_t *)test_record0;

    /* read BAR raw bytes for that record */
    if (pci_read_type0_bars_raw(test_record) != PCI_BAR_RAW_READ_OK) {
        KLOG_ERROR("PCI_TEST",
                   "type0_raw_bars_test: failed or skipped reading BAR raw "
                   "bytes for %02x:%02x.%u.\n",
                   bdf_to_find.bus, bdf_to_find.device, bdf_to_find.function);
        return 0;
    }

    /* dump the BAR raw bytes read. */
    pci_dump_type0_bars(test_record0);

    return 1;
}

uint8_t pci_command_rw_test() {
    pci_registry_t test_reg;
    pci_bdf_t bdf = {.bus = 0x00, .device = 0x03, .function = 0x00};
    const pci_function_record_t *test_rec;
    uint16_t before, after;

    /* enumerate bus 0 and update into its registry contents */
    if (!pci_enumerate_bus0_into_registry(&test_reg)) {
        KLOG_ERROR("PCI_TEST", "pci_command_rw_test: enumeration failed.\n");
        return 0;
    }

    /* find the concerned bdf endpoint from the registry */
    if (!pci_registry_find_bdf(&test_reg, bdf)) {
        KLOG_ERROR("PCI_TEST",
                   "pci_command_rw_test: target %02x:%02x.%u not found.\n",
                   bdf.bus, bdf.device, bdf.function);
        return 0;
    }

    before = pci_read_command(bdf);
    pci_update_cmd_bits(bdf, PCI_CMD_BUS_MASTER, 0);
    after = pci_read_command(bdf);

    KLOG_INFO(
        "PCI_TEST",
        "pci_command_rw_test %02x:%02x.%u command before=%04x after=%04x\n",
        bdf.bus, bdf.device, bdf.function, before, after);

    /* check if the bits of bus master is updated */
    if (!(after & PCI_CMD_BUS_MASTER)) {
        KLOG_ERROR(
            "PCI_TEST",
            "pci_command_rw_test: bus master bit not set after update.\n");
        return 0;
    }

    /* restore for test cleanup */
    pci_write_command(bdf, before);

    return 1;
}

uint8_t pci_enable_policy_test() {
    pci_registry_t test_reg;
    const pci_function_record_t *test_record;
    pci_bdf_t bdf_to_find = {.bus = 0x00, .device = 0x03, .function = 0x00};
    uint16_t cmd_read_before, cmd_read_after_mem, cmd_read_after_bm;

    /* enumerate bus 0 and update into its registry contents */
    if (!pci_enumerate_bus0_into_registry(&test_reg)) {
        KLOG_ERROR("PCI_TEST", "enable_policy_test: enumeration failed.\n");
        return 0;
    }

    /* find the concerned bdf endpoint from the registry */
    if (!pci_registry_find_bdf(&test_reg, bdf_to_find)) {
        KLOG_ERROR("PCI_TEST",
                   "enable_policy_test: target %02x:%02x.%u not found.\n",
                   bdf_to_find.bus, bdf_to_find.device, bdf_to_find.function);
        return 0;
    }

    /* read the command bits from the bdf */
    cmd_read_before = pci_read_command(bdf_to_find);

    /* set the bits to enable mem space access */
    pci_command_enable_mem_space(bdf_to_find);

    /* read and verify the command bits */
    cmd_read_after_mem = pci_read_command(bdf_to_find);
    if (!(cmd_read_after_mem & PCI_CMD_MEM_SPACE)) {
        KLOG_ERROR("PCI_TEST",
                   "pci_enable_policy_test: MEM_SPACE bit not set.\n");
        return 0;
    }

    /* set the bits to enable busmaster for enabling dma */
    pci_enable_bus_master(bdf_to_find);

    /* read and verify the bus master bits */
    cmd_read_after_bm = pci_read_command(bdf_to_find);
    if ((cmd_read_after_bm & PCI_CMD_BUS_MASTER) == 0) {
        KLOG_ERROR("PCI_TEST",
                   "pci_enable_policy_test: BUS_MASTER bit not set.\n");
        return 0;
    }

    KLOG_INFO("PCI_TEST",
              "pci_enable_policy_test %02x:%02x.%u cmd before=%04x "
              "after_mem=%04x after_bm=%04x\n",
              bdf_to_find.bus, bdf_to_find.device, bdf_to_find.function,
              cmd_read_before, cmd_read_after_mem, cmd_read_after_bm);

    /* Restore original command register for test cleanliness */
    pci_write_command(bdf_to_find, cmd_read_before);

    return 1;
}
