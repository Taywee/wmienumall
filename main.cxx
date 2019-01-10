/* Copyright © 2019 Taylor C. Richberger <taywee@gmx.com>
 * This code is released under the license described in the LICENSE file
 */

#include "wmienumall.h"

#include <windows.h>
#include <iostream>

int WINAPI wWinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] PWSTR pCmdLine, [[maybe_unused]] int nCmdShow) {
    WmiEnum *wmiEnum = WmiEnum_new(L"Win32.*Processor.*", L".*Load.*");
    const char *error = WmiEnum_error(wmiEnum);
    if (error) {
        std::cerr << "Error opening enum: " << error << std::endl;
        WmiEnum_free(wmiEnum);
        return 1;
    }

    const size_t instanceCount = WmiEnum_instanceCount(wmiEnum);
    for (size_t instance = 0; instance < instanceCount; ++instance) {
        std::wcout << WmiEnum_instanceClassName(wmiEnum, instance) << std::endl;
        const size_t propertyCount = WmiEnum_instancePropertyCount(wmiEnum, instance);
        for (size_t property = 0; property < propertyCount; ++property) {
            std::wcout
                << WmiEnum_instancePropertyKey(wmiEnum, instance, property)
                << L" -> "
                << WmiEnum_instancePropertyValue(wmiEnum, instance, property)
                << std::endl;
        }
    }

    WmiEnum_free(wmiEnum);
    return 0;   // Program successfully completed.
}
