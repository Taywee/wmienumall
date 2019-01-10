/* Copyright © 2019 Taylor C. Richberger <taywee@gmx.com>
 * This code is released under the license described in the LICENSE file
 */

#define _WIN32_DCOM
#include <comdef.h>
#include <wbemidl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

class BString {
    public:
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

class ComLibrary {
    public:
        ComLibrary() {
            const HRESULT hres =  CoInitializeEx(0, COINIT_MULTITHREADED); 
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Failed to initialize COM library. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
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
    const HRESULT hres = CoInitializeSecurity(
            NULL, 
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
            );

    if (FAILED(hres))
    {
        std::ostringstream message;
        message << "Failed to initialize COM security. Error code = 0x" 
            << std::hex << hres << std::endl;
        throw std::runtime_error(message.str());
    }
}

class Locator {
    public:
        IWbemLocator *pLoc;
        Locator() {
            const HRESULT hres = CoCreateInstance(
                    CLSID_WbemLocator,             
                    0, 
                    CLSCTX_INPROC_SERVER, 
                    IID_IWbemLocator, (LPVOID *) &pLoc);
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Failed to create IWbemLocator object."
                    << " Err code = 0x"
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
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

class Services {
    public: 
        const ComLibrary library;
        Locator locator;

        IWbemServices *pSvc;
        Services(const std::wstring &wmiNamespace = L"ROOT\\CIMV2") {
            comSecurity();

            const BString string(wmiNamespace.c_str()); // Object path of WMI namespace
            const HRESULT hres = locator.pLoc->ConnectServer(
                    string.string, 
                    nullptr,                    // User name. NULL = current user
                    nullptr,                    // User password. NULL = current
                    0,                       // Locale. NULL indicates current
                    0,                    // Security flags.
                    0,                       // Authority (for example, Kerberos)
                    0,                       // Context object 
                    &pSvc                    // pointer to IWbemServices proxy
                    );
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Could not connect. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
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
            const HRESULT hres = CoSetProxyBlanket(
                    pSvc,                        // Indicates the proxy to set
                    RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
                    RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
                    NULL,                        // Server principal name 
                    RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
                    RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
                    NULL,                        // client identity
                    EOAC_NONE                    // proxy capabilities 
                    );

            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Could not set proxy blanket. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
        }
};

class Variant {
    public:
        VARIANT variant;

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
};

class WbemClass {
    public:
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
            const HRESULT hres = obj->BeginEnumeration(WBEM_FLAG_NONSYSTEM_ONLY);
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Failed to begin the enumeration. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
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

        std::optional<std::tuple<BString, Variant>> next() {
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
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Failed to get next value. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
            return std::make_optional<std::tuple<BString, Variant>>(std::move(name), std::move(value));
        }
};

class EnumWbemClasses {
    public:
        IEnumWbemClassObject *enumClasses;

        EnumWbemClasses() : enumClasses(nullptr) {
        }

        EnumWbemClasses(IEnumWbemClassObject *enumClasses) : enumClasses(enumClasses) {
        }

        static EnumWbemClasses classEnum(Services &services) {
            EnumWbemClasses output;
            const HRESULT hres = services.pSvc->CreateClassEnum(nullptr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, output);
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Could not Create class enum. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
            return output;
        }

        static EnumWbemClasses instanceEnum(Services &services, const BSTR className) {
            EnumWbemClasses output;
            const HRESULT hres = services.pSvc->CreateInstanceEnum(className, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, output);
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Could not Create instance enum. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
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
            const HRESULT hres = enumClasses->Next(WBEM_INFINITE, 128, apObj, &returned);
            if (SUCCEEDED(hres)) {
                if (returned > 0) {
                    std::vector<WbemClass> output;
                    for ( ULONG n = 0; n < returned; n++ ) {
                        output.emplace_back(apObj[n]);
                    }
                    return std::make_optional(std::move(output));
                } else {
                    return std::nullopt;
                }
            } else {
                std::ostringstream message;
                message << "Could not Enum classes. Error code = 0x" 
                    << std::hex << hres << std::endl;
                throw std::runtime_error(message.str());
            }
        }
};

int WINAPI wWinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] PWSTR pCmdLine, [[maybe_unused]] int nCmdShow) {
    Services services;
    services.setProxyBlanket();
    std::cout << "Connected to ROOT\\CIMV2 WMI namespace" << std::endl;

    auto enumClasses = EnumWbemClasses::classEnum(services);

    for (auto items = enumClasses.next(); items; items = enumClasses.next()) {
        for (auto &item: items.value()) {
            auto className = _bstr_t(item.get(L"__CLASS").value().variant.bstrVal);
            // TODO: remove this
            if (std::wstring(className.GetBSTR(), className.length()).find(L"Win32") == 0 && std::wstring(className.GetBSTR(), className.length()).find(L"Processor") != std::wstring::npos) {
                auto enumInstances = EnumWbemClasses::instanceEnum(services, className.GetBSTR());
                for (auto instances = enumInstances.next(); instances; instances = enumInstances.next()) {
                    for (auto &instance: instances.value()) {
                        auto instanceName = instance.get(L"__CLASS");
                        instance.beginEnumeration();
                        for (auto pair = instance.next(); pair; pair = instance.next()) {
                            std::wcout << instanceName.value().variant.bstrVal << " -> " << std::get<0>(pair.value()).string << std::endl;
                        }
                    }
                }
            }
        }
    }

    return 0;   // Program successfully completed.
 
}
