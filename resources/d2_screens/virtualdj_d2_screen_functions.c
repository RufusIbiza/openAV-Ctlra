/*
 * Collation of VirtualDJ functions related to D2 screen output.
 * Based on analysis of VirtualDJ-pseudo_c.txt
 */

// --- 1. Main Screen Data Packet Construction and Sender ---
// Address: 140c40860
// Constructs the 20-byte magic header and the nested 16/20-byte rectangle command.
uint32_t sub_140c40860(void* arg1, int64_t arg2, int32_t x, int32_t width, int32_t height, char display_part, int32_t stride)
{
    // ... Initialization and Logging ...
    
    // Packet Header Construction (r9_1 + 0x2350 is the packet buffer)
    int32_t* rdx_2 = *(uint64_t*)((char*)r9_1 + 0x2350);
    
    // Offset 0x10: Total Data Size (Header 0x1C + Pixel Payload)
    rdx_2[4] = (width * height * 2) + 0x1c; 
    
    // Offset 0x00: Magic Header (CRITICAL)
    *(uint32_t*)rdx_2 = 0x3647344; // Magic ID
    
    // Offset 0x04: Display Part (Left/Right)
    rdx_2[1] = display_part;
    
    // Offset 0x08: Reserved
    rdx_2[2] = 0;
    
    // Offset 0x0C: Protocol Flags
    rdx_2[3] = 0x1e00110;

    // --- Inner Command Header (starts at offset 0x14) ---
    void* r11 = *(uint64_t*)((char*)r9_1 + 0x2350);
    *(uint16_t*)((char*)r11 + 0x14) = 0x84; // Header Magic
    *(uint32_t*)((char*)r11 + 0x17) = 0x60; // D2 Protocol Type (Big Endian)
    
    // Coordinates (Stored as Big Endian bytes)
    *(uint8_t*)((char*)r11 + 0x1c) = (char)(x >> 8);
    *(uint8_t*)((char*)r11 + 0x1d) = x;
    *(uint8_t*)((char*)r11 + 0x20) = (char)(width >> 8);
    *(uint8_t*)((char*)r11 + 0x21) = width;
    *(uint8_t*)((char*)r11 + 0x22) = (char)(height >> 8);
    *(uint8_t*)((char*)r11 + 0x23) = height;

    // --- Pixel Loop: Convert ARGB to BGR565 ---
    void* pixel_buffer = (char*)r11 + 0x28; // Data starts at 0x28
    for (int32_t i = 0; i < height; ++i) {
        for (int32_t j = 0; j < width; ++j) {
            int32_t argb = GetPixel(arg2, x + j, y + i); // 32-bit ARGB
            
            // Conversion Logic (140c40a80)
            uint16_t bgr565 = (
                ((int16_t)(argb >> 0xa) & 0x3f) |    // Green
                (int16_t)(argb >> 0x13) << 6         // Red
            ) << 5 | 
            ((int16_t)(argb >> 3) & 0x1f);           // Blue
            
            // Write Big Endian pixels to buffer
            *(uint8_t*)((char*)pixel_buffer) = (bgr565 >> 8);
            *(uint8_t*)((char*)pixel_buffer + 1) = bgr565;
            pixel_buffer += 2;
        }
    }

    // --- Send via WriteFile ---
    return sub_140c40460(arg1, handle_ptr, r11, total_size, bytes_written_ptr);
}

// --- 2. WriteFile Wrapper ---
// Address: 140c40460
// Thin wrapper around standard Windows API.
BOOL sub_140c40460(int64_t arg1, int64_t* handle_ptr, uint8_t* buffer, uint32_t size, uint32_t* bytes_written)
{
    // Standard Windows WriteFile call to the driver handle
    return WriteFile(*(uint64_t*)handle_ptr, buffer, size, bytes_written, nullptr);
}

// --- 3. Device Initialization / Activation ---
// Address: 1405a90fb
// Sends the 0x24054 IOCTL to "Unlock" the display.
uint64_t sub_1405a90fb(HANDLE hDevice)
{
    int32_t inBuffer = 0x80; // Activation Flag
    uint8_t lpOutBuffer[0x324];
    uint32_t bytesReturned;
    
    // Sends IOCTL to nikd2usb.sys
    if (DeviceIoControl(hDevice, 0x24054, &inBuffer, 4, lpOutBuffer, 0x324, &bytesReturned, nullptr))
    {
        // Success: Screen is now ready to receive data
        return 1;
    }
    return 0;
}

// --- 4. Driver Communication Wrapper (Alternative) ---
// Address: 1405a9370 (Commonly used by CD-Audio/Legacy handlers)
// Uses a different IOCTL 0x2403e for chunked data transfers.
uint64_t sub_1405a9370(HANDLE* arg1, int128_t* arg2, int32_t arg3)
{
    // Screen data transfer using 0x2403e command (0x930 bytes per chunk)
    if (DeviceIoControl(*(uint64_t*)arg1, 0x2403e, &inBuffer, 0x10, arg2, 0x930, &bytesReturned, nullptr))
    {
        // ...
    }
}
