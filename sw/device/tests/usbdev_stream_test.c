// Copyright lowRISC contributors.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// USB streaming data test
//
// This test requires interaction with the USB DPI model or a test application
// on the USB host. The test initializes the USB device and configures a set of
// endpoints for data streaming using bulk transfers.
//
// The DPI model mimicks a USB host. After device initialization, it detects
// the assertion of the pullup and first assigns an address to the device.
// For this test it will then repeatedly fetch data via IN requests to
// each stream and propagate that data to the corresponding OUT endpoints.
//
// The data itself is pseudo-randomly generated by the sender and,
// independently, by the receiving code to check that the data has been
// propagated unmodified and without data loss, corruption, replication etc.

#include "sw/device/lib/dif/dif_pinmux.h"
#include "sw/device/lib/runtime/log.h"
#include "sw/device/lib/runtime/print.h"
#include "sw/device/lib/testing/pinmux_testutils.h"
#include "sw/device/lib/testing/test_framework/check.h"
#include "sw/device/lib/testing/test_framework/ottf_main.h"
#include "sw/device/lib/testing/usb_testutils.h"
#include "sw/device/lib/testing/usb_testutils_controlep.h"
#include "sw/device/lib/testing/usb_testutils_diags.h"
#include "sw/device/lib/testing/usb_testutils_streams.h"

#include "hw/top_earlgrey/sw/autogen/top_earlgrey.h"  // Generated.

// Number of streams to be tested
#ifndef NUM_STREAMS
#define NUM_STREAMS USBUTILS_STREAMS_MAX
#endif

// This takes about 256s presently with 10MHz CPU in CW310 FPGA and physical
// USB with randomized packet sizes and the default memcpy implementation;
// The _MEM_FASTER switch drops the run time to 187s
#define TRANSFER_BYTES_FPGA (0x10U << 20)

// This is appropriate for a Verilator chip simulation with 15 min timeout
#define TRANSFER_BYTES_VERILATOR 0x2400U

// This is about the amount that we can transfer within a 1 hour 'eternal' test
//#define TRANSFER_BYTES_LONG (0xD0U << 20)

/**
 * Configuration values for USB.
 */
static const uint8_t config_descriptors[] = {
    USB_CFG_DSCR_HEAD(USB_CFG_DSCR_LEN + NUM_STREAMS * (USB_INTERFACE_DSCR_LEN +
                                                        2 * USB_EP_DSCR_LEN),
                      USBUTILS_STREAMS_MAX),

    // Up to 11 interfaces and NUM_STREAMS in the descriptor head specifies how
    // many of the interfaces will be declared to the host
    VEND_INTERFACE_DSCR(0, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 1U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 1U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(1, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 2U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 2U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(2, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 3U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 3U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(3, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 4U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 4U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(4, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 5U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 5U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(5, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 6U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 6U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(6, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 7U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 7U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(7, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 8U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 8U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(8, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 9U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 9U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(9, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 10U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 10U, USBDEV_MAX_PACKET_SIZE, 0),

    VEND_INTERFACE_DSCR(10, 2, 0x50, 1),
    USB_BULK_EP_DSCR(0, 11U, USBDEV_MAX_PACKET_SIZE, 0),
    USB_BULK_EP_DSCR(1, 11U, USBDEV_MAX_PACKET_SIZE, 0),
};

/**
 * Test flags specifying the nature and direction of the data stream(s)
 */
static usbdev_stream_flags_t test_flags;

/**
 * Test descriptor
 */
static uint8_t test_descriptor[USB_TESTUTILS_TEST_DSCR_LEN];

/**
 * USB device context types.
 */
static usb_testutils_ctx_t usbdev;
static usb_testutils_controlep_ctx_t usbdev_control;

/**
 * Pinmux handle
 */
static dif_pinmux_t pinmux;

/**
 * State information for streaming data test
 */
static usb_testutils_streams_ctx_t stream_test;

/**
 * Specify whether to perform verbose logging, for visibility
 *   (Note that this substantially alters the timing of interactions with the
 * DPI model and will increase the simulation time)
 */
static bool verbose = false;

/*
 * These switches may be modified manually to experimentally measure the
 * performance of IN traffic in isolation, or IN/OUT together etc.
 *
 * Are we sending data?
 */
static bool sending = true;

/**
 * Are we generating a valid byte sequence?
 */
static bool generating = true;

/**
 * Do we want the host to retry transmissions? (DPI model only; we cannot
 * instruct a physical host to fake delivery failure/packet corruption etc)
 */
static bool retrying = true;

/**
 * Are we expecting to receive data?
 */
static bool recving = true;

/**
 * Send only maximal length packets?
 * (important for performance measurements on the USB, but obviously undesirable
 *  for testing reliability/function)
 */
static bool max_packets = false;

/**
 * Number of streams to be created
 */
static const unsigned nstreams = NUM_STREAMS;

OTTF_DEFINE_TEST_CONFIG();

bool test_main(void) {
  // Context state for streaming test
  usb_testutils_streams_ctx_t *ctx = &stream_test;

  CHECK(kDeviceType == kDeviceSimDV || kDeviceType == kDeviceSimVerilator ||
            kDeviceType == kDeviceFpgaCw310,
        "This test is not expected to run on platforms other than the "
        "Verilator simulation or CW310 FPGA. It needs logic on the host side "
        "to retrieve, scramble and return the generated byte stream");

  LOG_INFO("Running USBDEV Stream Test");

  // Check we can support the requested number of streams
  CHECK(nstreams && nstreams < USBDEV_NUM_ENDPOINTS);

  // Decide upon the number of bytes to be transferred for the entire test
  uint32_t transfer_bytes = TRANSFER_BYTES_FPGA;
  if (kDeviceType == kDeviceSimVerilator) {
    transfer_bytes = TRANSFER_BYTES_VERILATOR;
  }
  transfer_bytes = (transfer_bytes + nstreams - 1) / nstreams;
  LOG_INFO(" - %u stream(s), 0x%x bytes each", nstreams, transfer_bytes);

  CHECK_DIF_OK(dif_pinmux_init(
      mmio_region_from_addr(TOP_EARLGREY_PINMUX_AON_BASE_ADDR), &pinmux));
  pinmux_testutils_init(&pinmux);
  CHECK_DIF_OK(dif_pinmux_input_select(
      &pinmux, kTopEarlgreyPinmuxPeripheralInUsbdevSense,
      kTopEarlgreyPinmuxInselIoc7));

  // Construct the test/stream flags to be used
  test_flags = (sending ? kUsbdevStreamFlagRetrieve : 0U) |
               (generating ? kUsbdevStreamFlagCheck : 0U) |
               (retrying ? kUsbdevStreamFlagRetry : 0U) |
               (recving ? kUsbdevStreamFlagSend : 0U) |
               (max_packets ? kUsbdevStreamFlagMaxPackets : 0U);

  // Initialize the test descriptor
  const uint8_t desc[] = {
      USB_TESTUTILS_TEST_DSCR(1, NUM_STREAMS | (uint8_t)test_flags, 0, 0, 0)};
  memcpy(test_descriptor, desc, sizeof(test_descriptor));

  // Remember context state for usb_testutils context
  ctx->usbdev = &usbdev;

  // Call `usbdev_init` here so that DPI will not start until the
  // simulation has finished all of the printing, which takes a while
  // if `--trace` was passed in.
  usb_testutils_init(ctx->usbdev, /*pinflip=*/false, /*en_diff_rcvr=*/false,
                     /*tx_use_d_se0=*/false);
  usb_testutils_controlep_init(&usbdev_control, ctx->usbdev, 0,
                               config_descriptors, sizeof(config_descriptors),
                               test_descriptor, sizeof(test_descriptor));
  while (usbdev_control.device_state != kUsbTestutilsDeviceConfigured) {
    usb_testutils_poll(ctx->usbdev);
  }

  // Initialise the state of the streams
  CHECK_STATUS_OK(usb_testutils_streams_init(ctx, nstreams, transfer_bytes,
                                             test_flags, verbose));

  if (verbose) {
    LOG_INFO("Commencing data transfer...");
  }

  // Streaming loop; most of the work is done by the usb_testutils_streams base
  //   code and we don't need to specialize its behavior for this test.
  bool done = false;
  do {
    CHECK_STATUS_OK(usb_testutils_streams_service(ctx));

    // See whether any streams still have more work to do
    done = usb_testutils_streams_completed(ctx);
  } while (!done);

  // Determine the total counts of bytes sent and received
  uint32_t tx_bytes = 0U;
  uint32_t rx_bytes = 0U;
  for (unsigned s = 0U; s < nstreams; s++) {
    tx_bytes += ctx->streams[s].tx_bytes;
    rx_bytes += ctx->streams[s].rx_bytes;
  }

  LOG_INFO("USB sent 0x%x byte(s), received and checked 0x%x byte(s)", tx_bytes,
           rx_bytes);

  CHECK(tx_bytes == nstreams * transfer_bytes,
        "Unexpected count of byte(s) sent to USB host");

  return true;
}
