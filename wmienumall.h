/* Copyright © 2019 Taylor C. Richberger <taywee@gmx.com>
 * This code is released under the license described in the LICENSE file
 */
#pragma once

#include <stddef.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef BUILDING_WMIENUMALL_DLL
#define WMIENUMALL_API extern WINAPI __declspec(dllexport)
#else
#define WMIENUMALL_API extern WINAPI __declspec(dllimport)
#endif
    struct WmiEnum;

    /// Always returns a WmiEnum, even in the case of error.
    WMIENUMALL_API WmiEnum *WmiEnum_new(const wchar_t *classRegex, const wchar_t *propertyRegex);

    /// Returns null if no error.  This is how error is checked for
    WMIENUMALL_API const char *WmiEnum_error(const WmiEnum *wmiEnum);

    WMIENUMALL_API void WmiEnum_free(WmiEnum *wmiEnum);
    WMIENUMALL_API size_t WmiEnum_instanceCount(const WmiEnum *wmiEnum);
    WMIENUMALL_API const wchar_t *WmiEnum_instanceClassName(const WmiEnum *wmiEnum, size_t instance);
    WMIENUMALL_API size_t WmiEnum_instancePropertyCount(const WmiEnum *wmiEnum, size_t instance);
    WMIENUMALL_API const wchar_t *WmiEnum_instancePropertyKey(const WmiEnum *wmiEnum, size_t instance, size_t property);
    WMIENUMALL_API const wchar_t *WmiEnum_instancePropertyValue(const WmiEnum *wmiEnum, size_t instance, size_t property);
#ifdef __cplusplus
}
#endif
