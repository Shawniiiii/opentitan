# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

load(
    "//rules:opentitan_test.bzl",
    "OPENTITANTOOL_OPENOCD_DATA_DEPS",
    "cw310_params",
    "opentitan_functest",
)
load("//rules:const.bzl", "CONST", "get_lc_items")
load("//rules:lc.bzl", "lc_raw_unlock_token")

package(default_visibility = ["//visibility:public"])

_TEST_UNLOCKED_LC_ITEMS = get_lc_items(
    CONST.LCV.TEST_UNLOCKED0,
    CONST.LCV.TEST_UNLOCKED1,
    CONST.LCV.TEST_UNLOCKED2,
    CONST.LCV.TEST_UNLOCKED3,
    CONST.LCV.TEST_UNLOCKED4,
    CONST.LCV.TEST_UNLOCKED5,
    CONST.LCV.TEST_UNLOCKED6,
    CONST.LCV.TEST_UNLOCKED7,
)

_TEST_LOCKED_LC_ITEMS = get_lc_items(
    CONST.LCV.TEST_LOCKED0,
    CONST.LCV.TEST_LOCKED1,
    CONST.LCV.TEST_LOCKED2,
    CONST.LCV.TEST_LOCKED3,
    CONST.LCV.TEST_LOCKED4,
    CONST.LCV.TEST_LOCKED5,
    CONST.LCV.TEST_LOCKED6,
)

_MISSION_MODE_LC_ITEMS = get_lc_items(
    CONST.LCV.DEV,
    CONST.LCV.PROD,
    CONST.LCV.PROD_END,
    CONST.LCV.RMA,
)

_ALL_LC_ITEMS = (
    _TEST_LOCKED_LC_ITEMS +
    _TEST_UNLOCKED_LC_ITEMS +
    _MISSION_MODE_LC_ITEMS
)

_MANUF_LC_TRANSITION_TEST_CMDS = [
    "--rom-kind=rom",
    "--bitstream=\"$(rootpath {bitstream})\"",
    "--openocd=\"$(rootpath //third_party/openocd:openocd_bin)\"",
    "--openocd-adapter-config=\"$(rootpath //third_party/openocd:jtag_adapter_cfg)\"",
    "--openocd-riscv-target-config=\"$(rootpath //util/openocd/target:lowrisc-earlgrey.cfg)\"",
    "--openocd-lc-target-config=\"$(rootpath //util/openocd/target:lowrisc-earlgrey-lc.cfg)\"",
]

_MANUF_LC_TRANSITION_TEST_CMDS_WBOOTSTRAP = _MANUF_LC_TRANSITION_TEST_CMDS + [
    "--bootstrap=\"$(rootpath {flash})\"",
]

cc_library(
    name = "otp_img_types",
    hdrs = ["otp_img_types.h"],
)

cc_library(
    name = "otp_img",
    srcs = ["otp_img.c"],
    hdrs = ["otp_img.h"],
    deps = [
        ":otp_img_types",
        "//sw/device/lib/base:status",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/testing:otp_ctrl_testutils",
    ],
)

cc_library(
    name = "individualize",
    srcs = ["individualize.c"],
    hdrs = [
        "individualize.h",
    ],
    deps = [
        "//sw/device/lib/base:status",
        "//sw/device/lib/crypto/drivers:entropy",
        "//sw/device/lib/dif:flash_ctrl",
        "//sw/device/lib/dif:lc_ctrl",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/testing:flash_ctrl_testutils",
        "//sw/device/lib/testing:lc_ctrl_testutils",
        "//sw/device/lib/testing:otp_ctrl_testutils",
    ],
)

opentitan_functest(
    name = "individualize_functest",
    srcs = ["individualize_functest.c"],
    cw310 = cw310_params(
        bitstream = "//hw/bitstream:rom_otp_dev_initial",
    ),
    targets = [
        "cw310_rom",
    ],
    deps = [
        ":individualize",
        "//hw/ip/flash_ctrl/data:flash_ctrl_regs",
        "//hw/ip/lc_ctrl/data:lc_ctrl_regs",
        "//hw/ip/otp_ctrl/data:otp_ctrl_regs",
        "//hw/top_earlgrey/sw/autogen:top_earlgrey",
        "//sw/device/lib/base:status",
        "//sw/device/lib/dif:flash_ctrl",
        "//sw/device/lib/dif:lc_ctrl",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/dif:rstmgr",
        "//sw/device/lib/testing:rstmgr_testutils",
        "//sw/device/lib/testing/test_framework:ottf_main",
    ],
)

cc_library(
    name = "individualize_preop",
    srcs = ["individualize_preop.c"],
    hdrs = [
        "individualize_preop.h",
    ],
    deps = [
        ":otp_img",
        "//hw/ip/otp_ctrl/data/sku_earlgrey_a0:otp_img_sku_earlgrey_a0_stage_individualize",
        "//sw/device/lib/base:status",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/testing:lc_ctrl_testutils",
        "//sw/device/lib/testing:otp_ctrl_testutils",
    ],
)

opentitan_functest(
    name = "individualize_preop_functest",
    srcs = ["individualize_preop_functest.c"],
    cw310 = cw310_params(
        bitstream = "//hw/bitstream:rom_otp_test_unlocked0",
    ),
    targets = [
        "cw310_rom",
    ],
    deps = [
        ":individualize_preop",
        "//hw/ip/otp_ctrl/data:otp_ctrl_regs",
        "//hw/top_earlgrey/sw/autogen:top_earlgrey",
        "//sw/device/lib/base:status",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/testing/test_framework:ottf_main",
    ],
)

cc_library(
    name = "provisioning",
    srcs = ["provisioning.c"],
    hdrs = [
        "provisioning.h",
    ],
    deps = [
        "//sw/device/lib/base:status",
        "//sw/device/lib/crypto/drivers:entropy",
        "//sw/device/lib/crypto/impl:aes",
        "//sw/device/lib/crypto/impl:ecc",
        "//sw/device/lib/crypto/impl:hash",
        "//sw/device/lib/crypto/impl:keyblob",
        "//sw/device/lib/crypto/include:datatypes",
        "//sw/device/lib/dif:flash_ctrl",
        "//sw/device/lib/dif:lc_ctrl",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/testing:flash_ctrl_testutils",
        "//sw/device/lib/testing:lc_ctrl_testutils",
        "//sw/device/lib/testing:otp_ctrl_testutils",
        "//sw/device/lib/testing/test_framework:check",
        "//sw/device/silicon_creator/manuf/keys",
    ],
)

opentitan_functest(
    name = "provisioning_functest",
    srcs = ["provisioning_functest.c"],
    cw310 = cw310_params(
        bitstream = "//hw/bitstream:rom_otp_dev_individualized",
        test_cmds = [
            "--clear-bitstream",
            "--rom-kind=rom",
            "--bitstream=\"$(location {bitstream})\"",
            "--bootstrap=\"$(location {flash})\"",
        ],
    ),
    targets = [
        "cw310_rom",
    ],
    test_harness = "//sw/host/tests/manuf/provisioning",
    deps = [
        ":provisioning",
        "//hw/ip/flash_ctrl/data:flash_ctrl_regs",
        "//hw/ip/lc_ctrl/data:lc_ctrl_regs",
        "//hw/ip/otp_ctrl/data:otp_ctrl_regs",
        "//hw/top_earlgrey/sw/autogen:top_earlgrey",
        "//sw/device/lib/base:status",
        "//sw/device/lib/dif:flash_ctrl",
        "//sw/device/lib/dif:lc_ctrl",
        "//sw/device/lib/dif:otp_ctrl",
        "//sw/device/lib/dif:rstmgr",
        "//sw/device/lib/testing:rstmgr_testutils",
        "//sw/device/lib/testing/test_framework:ottf_main",
        "//sw/device/silicon_creator/manuf/keys/fake",
    ],
)

lc_raw_unlock_token(
    name = "lc_raw_unlock_token",
)

# We bootstrap code into flash in LC states where CPU execution is enabled. In
# other LC states, we do not perform bootstrap, as it will not succeed.
[
    opentitan_functest(
        name = "manuf_scrap_functest_{}".format(lc_state.lower()),
        srcs = ["empty_functest.c"],
        cw310 = cw310_params(
            bitstream = "//hw/bitstream:rom_otp_{}".format(lc_state.lower()),
            tags = ["jtag"],
            test_cmds = (
                _MANUF_LC_TRANSITION_TEST_CMDS if (
                    (lc_state, lc_val) in _TEST_LOCKED_LC_ITEMS
                ) else _MANUF_LC_TRANSITION_TEST_CMDS_WBOOTSTRAP
            ) + [
                "--initial-lc-state=\"{}\"".format(lc_state),
            ],
        ),
        data = OPENTITANTOOL_OPENOCD_DATA_DEPS,
        targets = ["cw310_rom"],
        test_harness = "//sw/host/tests/manuf/manuf_scrap",
        deps = [
            "//sw/device/lib/runtime:log",
            "//sw/device/lib/testing/test_framework:ottf_main",
        ],
    )
    for lc_state, lc_val in _ALL_LC_ITEMS
]

test_suite(
    name = "manuf_scrap_functest",
    tags = ["manual"],
    tests = [
        ":manuf_scrap_functest_{}".format(lc_state)
        for lc_state, _ in _ALL_LC_ITEMS
    ],
)
