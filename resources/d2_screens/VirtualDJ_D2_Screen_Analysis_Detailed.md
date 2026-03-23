# VirtualDJ D2 Screen Communication Analysis

This document details the methods used by VirtualDJ to communicate with the Native Instruments Kontrol D2 displays, specifically addressing the use of partial updates and the low-level data transmission protocol.

---

## 1. Core Communication Method: `WriteFile` with Magic Header

VirtualDJ primarily communicates with the D2 displays by sending custom-structured packets to a handle obtained from the `nikd2usb.sys` driver via `WriteFile`. 

### 1.1 Support for Partial Updates
**YES**, VirtualDJ explicitly supports partial (rectangular) updates to the D2 displays. This is evident from the packet structure constructed in the function `sub_140c40860` (Address `140c40860`), which includes fields for `x`, `y`, `width`, and `height`.

### 1.2 The Magic Header: `0x3647344`
The most critical discovery in VirtualDJ's protocol is the use of the magic header `0x3647344`. This header identifies the packet as a display data command to the driver.

---

## 2. Packet Structure Specification

Based on analysis of `sub_140c40860`, the packet consists of a 20-byte outer header, followed by a 16-byte inner command header (similar to Traktor's), and then the pixel data.

| Offset | Size | Field Name | Value/Description |
| :--- | :--- | :--- | :--- |
| **0x00** | 4 | `magic_header` | `0x03647344` (Magic ID) |
| **0x04** | 4 | `display_part` | `0` or `1` (Left/Right screen partition) |
| **0x08** | 4 | `reserved` | `0` |
| **0x0C** | 4 | `protocol_flags`| `0x01e00110` |
| **0x10** | 4 | `total_size` | Pixel Data Size + `0x1C` |
| **0x14** | 2 | `header_magic` | `0x8400` (Traktor-style 16-byte header start) |
| **0x16** | 1 | `display_id` | `0` or `1` (Direct ID) |
| **0x17** | 4 | `protocol_type` | `0x60000000` (Identifies D2 protocol type) |
| **0x1C** | 2 | `x_coord` | X coordinate (Big Endian) |
| **0x1E** | 2 | `y_coord` | Y coordinate (Big Endian) |
| **0x20** | 2 | `width` | Rectangle Width (Big Endian) |
| **0x22** | 2 | `height` | Rectangle Height (Big Endian) |
| **0x24** | 4 | `reserved2` | `0` |
| **0x28** | ... | `pixel_data` | **BGR565** format (2 bytes per pixel) |

---

## 3. Pixel Data Processing (BGR565)

VirtualDJ converts 32-bit ARGB desktop pixels into a specific 16-bit format using the following logic (observed at `140c40a80`):

```cpp
// rdx_5 is a 32-bit ARGB pixel (0xAARRGGBB)
uint16_t bgr565 = (
    ((int16_t)(rdx_5 >> 0xa) & 0x3f) |    // Green (6 bits, from [15:8])
    (int16_t)(rdx_5 >> 0x13) << 6         // Red (5 bits, from [23:19])
) << 5 | 
((int16_t)(rdx_5 >> 3) & 0x1f);           // Blue (5 bits, from [7:3])
```
This construction results in a bit pattern: `RRRRR GGGGGG BBBBB`. The unusual shifts (`>> 19` for Red, `>> 10` for Green) align with mapping the most significant bits of the source 8-bit channels to the target 5-bit and 6-bit channels.

---

## 4. Driver Interaction via `DeviceIoControl`

In addition to `WriteFile`, VirtualDJ uses `DeviceIoControl` for initialization and device queries:
- **`0x24054`**: The "Unlock" or "Activation" command. It is sent with a 4-byte buffer containing `0x80`. This command must be issued *before* `WriteFile` will be accepted for display data.
- **`0x2403e`**: A lower-level transfer command used by some older handlers or for specific bulk operations (identified in `VirtualDJ_D2_Screen_Analysis.md`).

---

## 5. Workflow Summary

1.  **Detection**: Re-scans for D2 interfaces after the driver is loaded.
2.  **Activation**: Opens a handle to the D2 and sends IOCTL `0x24054` (Unlock).
3.  **Rendering**: 
    - Desktop waveform/UI is rendered to an internal 32-bit buffer.
    - `sub_140c40860` divides the update into rectangular regions (Partials).
    - For each region, a packet with magic `0x3647344` is built.
    - Pixels are converted from ARGB to BGR565 and packed into the payload.
4.  **Transmission**: The packet is sent to the `nikd2usb.sys` driver handle using `WriteFile`.
