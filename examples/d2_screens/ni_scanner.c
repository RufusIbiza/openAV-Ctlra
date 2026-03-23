#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>

/* Native Instruments Vendor ID */
#define NI_VENDOR_ID 0x17cc

void print_endpoint(const struct libusb_endpoint_descriptor *endpoint) {
  printf("      Endpoint: 0x%02x\n", endpoint->bEndpointAddress);

  printf("        Type: ");
  int type = endpoint->bmAttributes & 0x03;
  switch (type) {
  case LIBUSB_TRANSFER_TYPE_CONTROL:
    printf("Control");
    break;
  case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
    printf("Isochronous");
    break;
  case LIBUSB_TRANSFER_TYPE_BULK:
    printf("Bulk");
    break;
  case LIBUSB_TRANSFER_TYPE_INTERRUPT:
    printf("Interrupt");
    break;
  }

  printf(" | Direction: %s\n",
         (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) ? "IN" : "OUT");
  printf("        Max Packet Size: %d\n", endpoint->wMaxPacketSize);

  /* Heuristics for NI devices */
  if (type == LIBUSB_TRANSFER_TYPE_BULK &&
      !(endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)) {
    printf("        --> POTENTIAL SCREEN INTERFACE (Bulk OUT)\n");
  }
  if (type == LIBUSB_TRANSFER_TYPE_INTERRUPT) {
    printf("        --> POTENTIAL HID/CONTROL INTERFACE\n");
  }
}

void print_interface(const struct libusb_interface *interface) {
  for (int i = 0; i < interface->num_altsetting; i++) {
    const struct libusb_interface_descriptor *interdesc =
        &interface->altsetting[i];
    printf("    Interface Number: %d | Alt Setting: %d | Endpoints: %d\n",
           interdesc->bInterfaceNumber, interdesc->bAlternateSetting,
           interdesc->bNumEndpoints);

    for (int j = 0; j < interdesc->bNumEndpoints; j++) {
      print_endpoint(&interdesc->endpoint[j]);
    }
  }
}

void scan_device(libusb_device *dev) {
  struct libusb_device_descriptor desc;
  int r = libusb_get_device_descriptor(dev, &desc);
  if (r < 0) {
    fprintf(stderr, "failed to get device descriptor\n");
    return;
  }

  if (desc.idVendor != NI_VENDOR_ID)
    return;

  printf("\n--------------------------------------------------\n");
  printf("Native Instruments Device Found\n");
  printf("Product ID: 0x%04x\n", desc.idProduct);

  libusb_device_handle *handle = NULL;
  r = libusb_open(dev, &handle);
  if (r == 0) {
    unsigned char string[256];
    if (desc.iProduct > 0) {
      libusb_get_string_descriptor_ascii(handle, desc.iProduct, string,
                                         sizeof(string));
      printf("Product Name: %s\n", string);
    }
    if (desc.iSerialNumber > 0) {
      libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, string,
                                         sizeof(string));
      printf("Serial Number: %s\n", string);
    }
    libusb_close(handle);
  } else {
    printf("Warning: Could not open device (Permission denied?). Run with sudo "
           "for names.\n");
  }

  struct libusb_config_descriptor *config;
  libusb_get_config_descriptor(dev, 0, &config);

  printf("Configuration:\n");
  printf("  Interfaces: %d\n", config->bNumInterfaces);

  for (int i = 0; i < config->bNumInterfaces; i++) {
    print_interface(&config->interface[i]);
  }

  libusb_free_config_descriptor(config);
}

int main(int argc, char **argv) {
  libusb_context *ctx = NULL;
  int r = libusb_init(&ctx);
  if (r < 0) {
    fprintf(stderr, "Error initializing libusb: %d\n", r);
    return 1;
  }

  libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, 3); // Info level

  printf("Scanning for Native Instruments devices (VID 0x%04x)...\n",
         NI_VENDOR_ID);

  libusb_device **devs;
  ssize_t cnt = libusb_get_device_list(ctx, &devs);
  if (cnt < 0) {
    fprintf(stderr, "Error getting device list\n");
    return 1;
  }

  for (ssize_t i = 0; i < cnt; i++) {
    scan_device(devs[i]);
  }

  libusb_free_device_list(devs, 1);
  libusb_exit(ctx);
  return 0;
}
