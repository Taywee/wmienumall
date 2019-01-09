/* Copyright © 2019 Taylor C. Richberger <taywee@gmx.com>
 * This code taken from
 * https://docs.microsoft.com/en-us/windows/desktop/WmiSdk/example--getting-wmi-data-from-the-local-computer
 * and therefore not covered by LICENSE file contents
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

class BString {
    public:
        BSTR string;
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
        IWbemServices *pSvc;
        Services(const Locator &locator, const std::wstring &wmiNamespace = L"ROOT\\CIMV2") {
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

        operator VARIANT&() {
            return variant;
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

        std::optional<Variant> get(const std::wstring &property) {
            VARIANT vtProp;
            const HRESULT hres = obj->Get(property.c_str(), 0, &vtProp, nullptr, 0);
            if (SUCCEEDED(hres)) {
                return std::make_optional<Variant>(std::move(vtProp));
            } else {
                return std::nullopt;
            }
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
            const HRESULT hres = services.pSvc->CreateClassEnum(nullptr, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_DEEP | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, output);
            if (FAILED(hres))
            {
                std::ostringstream message;
                message << "Could not Create class enum. Error code = 0x" 
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

int WINAPI wWinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] PWSTR pCmdLine, [[maybe_unused]] int nCmdShow) {
    const ComLibrary library;
    comSecurity();
    Locator locator;
    Services services(locator);
    services.setProxyBlanket();
    std::cout << "Connected to ROOT\\CIMV2 WMI namespace" << std::endl;

    auto enumClasses = EnumWbemClasses::classEnum(services);

    for (auto items = enumClasses.next(); items; enumClasses.next()) {
        for (auto &item: items.value()) {
            auto className = item.get(L"__CLASS");
            std::wcout << className.value().variant.bstrVal << std::endl;
        }
    }

    /*// Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        SysAllocString(L"WQL"), 
        SysAllocString(L"SELECT * FROM Win32_OperatingSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
        NULL,
        &pEnumerator);
    
    if (FAILED(hres))
    {
        cout << "Query for operating system name failed."
            << " Error code = 0x" 
            << std::hex << hres << std::endl;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return 1;               // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------
 
    IWbemClassObject *pclsObj = NULL;
    ULONG uReturn = 0;
   
    while (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, 
            &pclsObj, &uReturn);

        if(0 == uReturn)
        {
            break;
        }

        VARIANT vtProp;

        // Get the value of the Name property
        hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
        wcout << " OS Name : " << vtProp.bstrVal << std::endl;
        VariantClear(&vtProp);

        pclsObj->Release();
    }

    // Cleanup
    // ========
    
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();*/

    return 0;   // Program successfully completed.
 
}
