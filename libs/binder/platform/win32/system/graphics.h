/*
 * Windows stub implementation for system/graphics.h
 * This file provides minimal definitions needed for Windows compilation
 */

#ifndef _WIN32_SYSTEM_GRAPHICS_H
#define _WIN32_SYSTEM_GRAPHICS_H

#ifdef __cplusplus
extern "C" {
#endif

// Minimal graphics definitions for Windows compatibility
typedef enum {
    HAL_PIXEL_FORMAT_RGBA_8888          = 1,
    HAL_PIXEL_FORMAT_RGBX_8888          = 2,
    HAL_PIXEL_FORMAT_RGB_888            = 3,
    HAL_PIXEL_FORMAT_RGB_565            = 4,
    HAL_PIXEL_FORMAT_BGRA_8888          = 5,
    HAL_PIXEL_FORMAT_YV12               = 0x32315659,
    HAL_PIXEL_FORMAT_YCrCb_420_SP       = 0x11,
    HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED = 0x22,
} android_pixel_format_t;

#ifdef __cplusplus
}
#endif

#endif // _WIN32_SYSTEM_GRAPHICS_H
