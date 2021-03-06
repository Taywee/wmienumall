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

    /// Returns null if no error.  This is how error is checked for.
    WMIENUMALL_API const char *WmiEnum_error(const WmiEnum *wmiEnum);

    /** Free the WmiEnum and its error string.
     * This should be called even in the case of error.
     */
    WMIENUMALL_API void WmiEnum_free(WmiEnum *wmiEnum);

    /// Get the number of instances, used for iterating.
    WMIENUMALL_API size_t WmiEnum_instanceCount(const WmiEnum *wmiEnum);
    
    /** Get an instance's class name based on its index.
     * Returns NULL on bad index.
     */
    WMIENUMALL_API const wchar_t *WmiEnum_instanceClassName(const WmiEnum *wmiEnum, size_t instance);

    /** Get an instance's property count based on its index.
     * Returns 0 on bad index.
     */
    WMIENUMALL_API size_t WmiEnum_instancePropertyCount(const WmiEnum *wmiEnum, size_t instance);

    /** Get an instance's property's key based on its index.
     * Returns NULL on bad index.
     */
    WMIENUMALL_API const wchar_t *WmiEnum_instancePropertyKey(const WmiEnum *wmiEnum, size_t instance, size_t property);

    /** Get an instance's property's value based on its index.
     * Returns NULL on bad index.
     */
    WMIENUMALL_API const wchar_t *WmiEnum_instancePropertyValue(const WmiEnum *wmiEnum, size_t instance, size_t property);
#ifdef __cplusplus
}
#endif
