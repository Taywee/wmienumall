/* Copyright © 2019 Taylor C. Richberger <taywee@gmx.com>
 * This code is released under the license described in the LICENSE file
 */

#define _WIN32_DCOM
#include <comdef.h>
#include <wbemidl.h>
#include <regex>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

#include "wmienumall.h"

static void checkResult(const HRESULT hres, const std::string &message) {
    if (FAILED(hres)) {
        std::ostringstream oss;
        oss
            << message
            << " Error code = 0x"
            << std::hex << hres
            << std::endl;
        throw std::runtime_error(oss.str());
    }
}

struct BString {
        BSTR string;

        BString() : string(nullptr) {
        }

        BString(BSTR string) : string(string) {
        }

        BString(const OLECHAR *psz) : string(SysAllocString(psz)) {
            if (string == nullptr) {
                throw std::runtime_error("Could not allocate BStr");
            }
        }

        BString(const std::wstring &psz) : BString(psz.c_str()) {
        }

        BString(const BString &) = delete;
        BString(BString &&other) {
            string = other.string;
            other.string = nullptr;
        }
        BString &operator=(const BString &) = delete;
        BString &operator=(BString &&other) {
            std::swap(string, other.string);
            return *this;
        }

        operator BSTR*() {
            return &string;
        }

        ~BString() {
            if (string) {
                SysFreeString(string);
            }
        }
};

struct ComLibrary {
        ComLibrary() {
            checkResult(CoInitializeEx(0, COINIT_MULTITHREADED),
                    "Failed to initialize COM library.");
        }
        ~ComLibrary() {
            CoUninitialize();
        }

        ComLibrary(const ComLibrary &) = delete;
        ComLibrary(ComLibrary &&) {
        }
        ComLibrary &operator=(const ComLibrary &) = delete;
        ComLibrary &operator=(ComLibrary &&) {
            return *this;
        }
};

static void comSecurity() {
    checkResult(CoInitializeSecurity(
            NULL, 
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
            ), "Failed to initialize COM security.");
}

struct Locator {
        IWbemLocator *pLoc;
        Locator() {
            checkResult(CoCreateInstance(
                        CLSID_WbemLocator,             
                        0, 
                        CLSCTX_INPROC_SERVER, 
                        IID_IWbemLocator, (LPVOID *) &pLoc),
                    "Failed to create IWbemLocator object.");
        }

        Locator(const Locator &) = delete;
        Locator(Locator &&other) {
            pLoc = other.pLoc;
            other.pLoc = nullptr;
        }
        Locator &operator=(const Locator &) = delete;
        Locator &operator=(Locator &&other) {
            std::swap(pLoc, other.pLoc);
            return *this;
        }

        ~Locator() {
            if (pLoc) {
                pLoc->Release();
            }
        }
};

struct Services {
        const ComLibrary library;
        Locator locator;

        IWbemServices *pSvc;
        Services(const std::wstring &wmiNamespace = L"ROOT\\CIMV2") {
            comSecurity();

            const BString string(wmiNamespace.c_str()); // Object path of WMI namespace
            checkResult(locator.pLoc->ConnectServer(
                        string.string, 
                        nullptr,                    // User name. NULL = current user
                        nullptr,                    // User password. NULL = current
                        0,                       // Locale. NULL indicates current
                        0,                    // Security flags.
                        0,                       // Authority (for example, Kerberos)
                        0,                       // Context object 
                        &pSvc                    // pointer to IWbemServices proxy
                        ),
                    "Could not connect. Error code = 0x");
        }

        Services(const Services &) = delete;
        Services(Services &&other) {
            pSvc = other.pSvc;
            other.pSvc = nullptr;
        }
        Services &operator=(const Services &) = delete;
        Services &operator=(Services &&other) {
            std::swap(pSvc, other.pSvc);
            return *this;
        }

        ~Services() {
            if (pSvc) {
                pSvc->Release();
            }
        }

        void setProxyBlanket() {
            checkResult(CoSetProxyBlanket(
                    pSvc,                        // Indicates the proxy to set
                    RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
                    RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
                    NULL,                        // Server principal name 
                    RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
                    RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
                    NULL,                        // client identity
                    EOAC_NONE                    // proxy capabilities 
                    ),
                "Could not set proxy blanket.");
        }
};

struct Variant {
        VARIANT variant;
        std::wstring buffer;

        Variant() {
            VariantInit(&variant);
        }

        Variant(VARIANT &&variant) : variant(std::move(variant)) {
        }

        Variant(const Variant &) = delete;
        Variant(Variant &&other) {
            variant = std::move(other.variant);
            VariantInit(&other.variant);
        }
        Variant &operator=(const Variant &) = delete;
        Variant &operator=(Variant &&other) {
            std::swap(variant, other.variant);
            return *this;
        }

        ~Variant() {
            VariantClear(&variant);
        }

        operator VARIANT*() {
            return &variant;
        }

        /** Get all the strings contained in this variant.
         * If this is not an array type, there will only be one string.
         */
        std::vector<std::wstring> getStrings() {
            return getStrings(variant);
        }

        static std::vector<std::wstring> getStrings(VARIANT &variant) {
            std::vector<std::wstring> output;
            const VARTYPE type = variant.vt;
            if (type & VT_ARRAY) {
                SAFEARRAY *array;
                if (type & VT_BYREF) {
                    array = *variant.pparray;
                } else {
                    array = variant.parray;
                }
                if (type & VT_BSTR) {
                    BSTR *vals;
                    checkResult(SafeArrayAccessData(array, reinterpret_cast<void **>(&vals)),
                            "Failed to access array.");
                    long lowerBound, upperBound;
                    checkResult(SafeArrayGetLBound(array, 1, &lowerBound),
                            "Failed to access array lower bound.");
                    checkResult(SafeArrayGetUBound(array, 1, &upperBound),
                            "Failed to access array upperwer bound.");
                    const long elementCount = upperBound - lowerBound + 1;
                    for (long i = 0; i < elementCount; ++i) {
                        output.emplace_back(vals[i], SysStringLen(vals[i]));
                    }
                    SafeArrayUnaccessData(array);
                }
            } else {
                if (!(type == VT_EMPTY || type == VT_NULL)) {
                    Variant newVariant;
                    checkResult(VariantChangeType(newVariant, &variant, VARIANT_ALPHABOOL, VT_BSTR),
                            "Failed to convert variant to BSTR.");
                    BSTR val = newVariant.variant.bstrVal;
                    output.emplace_back(val, SysStringLen(val));
                }
            }
            return output;
        }

        /** Get the strings from this variant joined by a comma and space
         * between each, and sets the content string to be equal to this.
         *
         * This is not identical to old behavior, but that behavior was bad, and
         * would often cause undesirable things (like an array of integers being
         * interpreted as a single larger integer of multiple digits).
         */
        std::wstring &getString() {
            static const std::wstring separator = L", ";
            std::wostringstream oss;
            const auto strings = getStrings();
            auto it = std::begin(strings);
            auto end = std::end(strings);
            if (it != end) {
                oss << *it;
                for (++it; it != end; ++it) {
                    oss << separator << *it;
                }
            }
            buffer = oss.str();
            return buffer;
        }
};

struct WbemClass {
        IWbemClassObject* obj;

        WbemClass(IWbemClassObject* obj) : obj(obj) {
        }

        WbemClass(const WbemClass &) = delete;
        WbemClass(WbemClass &&other) {
            obj = other.obj;
            other.obj = nullptr;
        }
        WbemClass &operator=(const WbemClass &) = delete;
        WbemClass &operator=(WbemClass &&other) {
            std::swap(obj, other.obj);
            return *this;
        }

        ~WbemClass() {
            if (obj) {
                obj->Release();
            }
        }

        void beginEnumeration() {
            checkResult(obj->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY),
                    "Failed to begin the enumeration.");
        }

        std::optional<Variant> get(const std::wstring &property) {
            VARIANT vtProp;
            const HRESULT hres = obj->Get(property.c_str(), 0, &vtProp, nullptr, 0);
            if (SUCCEEDED(hres)) {
                return std::make_optional<Variant>(std::move(vtProp));
            } else {
                return std::nullopt;
            }
        }

        std::optional<std::tuple<std::wstring, Variant>> next() {
            BString name;
            Variant value;
            const HRESULT hres = obj->Next(
                0,
                name,
                value,
                nullptr,
                nullptr
                );
            if (hres == WBEM_S_NO_MORE_DATA) {
                return std::nullopt;
            }
            checkResult(hres, "Failed to get next value.");
            return std::make_optional<std::tuple<std::wstring, Variant>>(std::wstring(name.string, SysStringLen(name.string)), std::move(value));
        }
};

struct EnumWbemClasses {
        IEnumWbemClassObject *enumClasses;

        EnumWbemClasses() : enumClasses(nullptr) {
        }

        EnumWbemClasses(IEnumWbemClassObject *enumClasses) : enumClasses(enumClasses) {
        }

        static EnumWbemClasses classEnum(Services &services) {
            EnumWbemClasses output;
            checkResult(services.pSvc->CreateClassEnum(nullptr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, output),
                    "Could not Create class enum.");
            return output;
        }

        static EnumWbemClasses instanceEnum(Services &services, const BSTR className) {
            EnumWbemClasses output;
            checkResult(services.pSvc->CreateInstanceEnum(className, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, output),
                "Could not Create instance enum.");
            return output;
        }

        EnumWbemClasses(const EnumWbemClasses &) = delete;
        EnumWbemClasses(EnumWbemClasses &&other) {
            enumClasses = other.enumClasses;
            other.enumClasses = nullptr;
        }
        EnumWbemClasses &operator=(const EnumWbemClasses &) = delete;
        EnumWbemClasses &operator=(EnumWbemClasses &&other) {
            std::swap(enumClasses, other.enumClasses);
            return *this;
        }

        operator IEnumWbemClassObject**() {
            return &enumClasses;
        }

        ~EnumWbemClasses() {
            if (enumClasses) {
                enumClasses->Release();
            }
        }

        std::optional<std::vector<WbemClass>> next() {
            ULONG returned;
            IWbemClassObject* apObj[128];
            checkResult(enumClasses->Next(WBEM_INFINITE, 128, apObj, &returned),
                    "Could not Enum classes.");
            if (returned > 0) {
                std::vector<WbemClass> output;
                for ( ULONG n = 0; n < returned; n++ ) {
                    output.emplace_back(apObj[n]);
                }
                return std::make_optional(std::move(output));
            } else {
                return std::nullopt;
            }
        }
};

struct WmiInstance {
    std::wstring className;
    std::vector<std::tuple<std::wstring, std::wstring>> properties;
};

struct WmiEnum {
    std::optional<std::string> error;
    std::vector<WmiInstance> instances;
};

/// Always returns a WmiEnum, even in the case of error.
WmiEnum *WmiEnum_new(const wchar_t * const classRegex, const wchar_t * const propertyRegex) {
    WmiEnum *output = new WmiEnum();
    try {
        const std::wregex cRegex(classRegex), pRegex(propertyRegex);
        Services services;
        services.setProxyBlanket();
        auto enumClasses = EnumWbemClasses::classEnum(services);
        for (auto items = enumClasses.next(); items; items = enumClasses.next()) {
            for (auto &item: items.value()) {
                // Need this as a separate piece to avoid cleaning it up too early
                auto rawClassName = item.get(L"__CLASS").value();
                _bstr_t className(rawClassName.variant.bstrVal, false);
                const std::wstring classNameS(className.GetBSTR(), className.length());
                if (std::regex_match(classNameS, cRegex)) {
                    auto enumInstances = EnumWbemClasses::instanceEnum(services, className.GetBSTR());
                    for (auto instances = enumInstances.next(); instances; instances = enumInstances.next()) {
                        for (auto &instance: instances.value()) {
                            WmiInstance wmiInstance;
                            wmiInstance.className.assign(classNameS);
                            instance.beginEnumeration();
                            for (auto pair = instance.next(); pair; pair = instance.next()) {
                                if (std::regex_match(std::get<0>(pair.value()), pRegex)) {
                                    wmiInstance.properties.emplace_back(
                                            std::get<0>(pair.value()),
                                            std::get<1>(pair.value()).getString());
                                }
                            }
                            output->instances.emplace_back(std::move(wmiInstance));
                        }
                    }
                }
                className.Detach();
            }
        }
    }
    catch (const std::exception &e) {
        output->error = std::make_optional<std::string>(e.what());
        output->instances.clear();
    }
    return output;
}

const char *WmiEnum_error(const WmiEnum * const wmiEnum) {
    if (wmiEnum->error) {
        return wmiEnum->error.value().c_str();
    } else {
        return nullptr;
    }
}

void WmiEnum_free(WmiEnum * const wmiEnum) {
    delete wmiEnum;
}

size_t WmiEnum_instanceCount(const WmiEnum * const wmiEnum) {
    return wmiEnum->instances.size();
}

const wchar_t *WmiEnum_instanceClassName(const WmiEnum * const wmiEnum, const size_t instance) {
    if (instance < wmiEnum->instances.size()) {
        return wmiEnum->instances[instance].className.c_str();
    }
    return nullptr;
}

size_t WmiEnum_instancePropertyCount(const WmiEnum * const wmiEnum, const size_t instance) {
    if (instance < wmiEnum->instances.size()) {
        return wmiEnum->instances[instance].properties.size();
    }
    return 0;
}
const wchar_t *WmiEnum_instancePropertyKey(const WmiEnum * const wmiEnum, const size_t instance, const size_t property) {
    if (instance < wmiEnum->instances.size()) {
        const auto &i = wmiEnum->instances[instance];
        if (property < i.properties.size()) {
            return std::get<0>(i.properties[property]).c_str();
        }
    }
    return nullptr;
}
const wchar_t *WmiEnum_instancePropertyValue(const WmiEnum * const wmiEnum, const size_t instance, const size_t property) {
    if (instance < wmiEnum->instances.size()) {
        const auto &i = wmiEnum->instances[instance];
        if (property < i.properties.size()) {
            return std::get<1>(i.properties[property]).c_str();
        }
    }
    return nullptr;
}
