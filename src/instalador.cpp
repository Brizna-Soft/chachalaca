#include "instalador.hpp"

#include <shellapi.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <cwchar>

#include "util.hpp"

namespace
{
    std::wstring CarpetaDelPrograma()
    {
        wchar_t buf[MAX_PATH * 4] = {};
        DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
        std::wstring r(buf, n);
        auto p = r.find_last_of(L"\\/");
        return p == std::wstring::npos ? r : r.substr(0, p);
    }
}

std::wstring cha::InstaladorVBCable()
{
    std::wstring ruta = CarpetaDelPrograma() + L"\\VB-CABLE\\VBCABLE_Setup_x64.exe";
    return ExisteArchivo(ruta) ? ruta : L"";
}

bool cha::FirmaDeVBAudio(const std::wstring& ruta)
{
    WINTRUST_FILE_INFO archivo = {};
    archivo.cbStruct = sizeof(archivo);
    archivo.pcwszFilePath = ruta.c_str();

    WINTRUST_DATA datos = {};
    datos.cbStruct = sizeof(datos);
    datos.dwUIChoice = WTD_UI_NONE;
    datos.fdwRevocationChecks = WTD_REVOKE_NONE;
    datos.dwUnionChoice = WTD_CHOICE_FILE;
    datos.pFile = &archivo;
    datos.dwStateAction = WTD_STATEACTION_VERIFY;

    GUID accion = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    bool ok = WinVerifyTrust(nullptr, &accion, &datos) == ERROR_SUCCESS;
    if (ok)
    {
        // Además de válida, tiene que ser de VB-Audio
        wchar_t nombre[256] = {};
        CRYPT_PROVIDER_DATA* proveedor = WTHelperProvDataFromStateData(datos.hWVTStateData);
        CRYPT_PROVIDER_SGNR* firmante = proveedor ? WTHelperGetProvSignerFromChain(proveedor, 0, FALSE, 0) : nullptr;
        CRYPT_PROVIDER_CERT* certificado = firmante ? WTHelperGetProvCertFromChain(firmante, 0) : nullptr;
        if (certificado) CertGetNameStringW(certificado->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nombre, 256);
        ok = std::wcsstr(nombre, L"BUREL VINCENT") != nullptr;
        Registrar(std::string("firma del instalador de VB-CABLE: ") + (ok ? "de VB-Audio" : Utf8(nombre)));
    }
    datos.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &accion, &datos);
    return ok;
}

HANDLE cha::AbrirInstaladorVBCable(HWND padre, const std::wstring& ruta, std::wstring& error)
{
    std::wstring carpeta = ruta.substr(0, ruta.find_last_of(L'\\'));
    SHELLEXECUTEINFOW s = {};
    s.cbSize = sizeof(s);
    s.fMask = SEE_MASK_NOCLOSEPROCESS;
    s.hwnd = padre;
    s.lpVerb = L"runas";               // el controlador necesita permiso de administrador
    s.lpFile = ruta.c_str();
    s.lpParameters = L"-i -h";         // instalar sin su ventana
    s.lpDirectory = carpeta.c_str();
    s.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&s) || !s.hProcess)
    {
        DWORD e = GetLastError();
        error = e == ERROR_CANCELLED ? L"No se instaló: Windows no recibió el permiso de administrador."
                                     : L"No pude abrir el instalador de VB-CABLE (error " + std::to_wstring(e) + L").";
        Registrar("instalador de VB-CABLE: error " + std::to_string(e));
        return nullptr;
    }
    Registrar("instalador de VB-CABLE abierto");
    return s.hProcess;
}
