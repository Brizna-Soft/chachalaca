#include "instalador.hpp"

#include <bcrypt.h>
#include <shellapi.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <cwchar>
#include <vector>

#include "util.hpp"

namespace
{
    // VBCABLE_Setup_x64.exe del paquete VBCABLE_Driver_Pack45.zip (octubre de 2024), el que incluye empaquetar.ps1.
    // Si VB-Audio publica otro paquete, hay que revisarlo y cambiar las dos huellas a la vez.
    const char* HuellaInstalador = "734c35dfa6d98f48782a451633ceb471166ec70d60482fd89a1123d0ee3c4f41";

    std::wstring CarpetaDelPrograma()
    {
        wchar_t buf[MAX_PATH * 4] = {};
        DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
        std::wstring r(buf, n);
        auto p = r.find_last_of(L"\\/");
        return p == std::wstring::npos ? r : r.substr(0, p);
    }

    std::string Sha256(HANDLE archivo)
    {
        std::string r;
        LARGE_INTEGER inicio = {};
        SetFilePointerEx(archivo, inicio, nullptr, FILE_BEGIN);
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0
            && BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0)
        {
            std::vector<unsigned char> buf(1 << 16);
            DWORD leidos = 0;
            bool ok = true;
            while (ReadFile(archivo, buf.data(), (DWORD)buf.size(), &leidos, nullptr) && leidos > 0)
            {
                if (BCryptHashData(hash, buf.data(), leidos, 0) != 0) { ok = false; break; }
            }
            unsigned char huella[32];
            if (ok && BCryptFinishHash(hash, huella, sizeof(huella), 0) == 0)
            {
                static const char* hex = "0123456789abcdef";
                for (unsigned char b : huella) { r += hex[b >> 4]; r += hex[b & 15]; }
            }
        }
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        return r;
    }

    // Firma válida de VB-Audio sobre el archivo ya abierto
    bool FirmaDeVBAudio(const std::wstring& ruta, HANDLE archivo)
    {
        WINTRUST_FILE_INFO info = {};
        info.cbStruct = sizeof(info);
        info.pcwszFilePath = ruta.c_str();
        info.hFile = archivo;

        WINTRUST_DATA datos = {};
        datos.cbStruct = sizeof(datos);
        datos.dwUIChoice = WTD_UI_NONE;
        datos.fdwRevocationChecks = WTD_REVOKE_NONE;   // sin internet también tiene que funcionar; la huella fija el archivo
        datos.dwUnionChoice = WTD_CHOICE_FILE;
        datos.pFile = &info;
        datos.dwStateAction = WTD_STATEACTION_VERIFY;

        GUID accion = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        bool ok = WinVerifyTrust(nullptr, &accion, &datos) == ERROR_SUCCESS;
        if (ok)
        {
            wchar_t nombre[256] = {};
            CRYPT_PROVIDER_DATA* proveedor = WTHelperProvDataFromStateData(datos.hWVTStateData);
            CRYPT_PROVIDER_SGNR* firmante = proveedor ? WTHelperGetProvSignerFromChain(proveedor, 0, FALSE, 0) : nullptr;
            CRYPT_PROVIDER_CERT* certificado = firmante ? WTHelperGetProvCertFromChain(firmante, 0) : nullptr;
            if (certificado) CertGetNameStringW(certificado->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nombre, 256);
            ok = std::wcsstr(nombre, L"BUREL VINCENT") != nullptr;
        }
        datos.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(nullptr, &accion, &datos);
        return ok;
    }
}

std::wstring cha::InstaladorVBCable()
{
    std::wstring ruta = CarpetaDelPrograma() + L"\\VB-CABLE\\VBCABLE_Setup_x64.exe";
    return ExisteArchivo(ruta) ? ruta : L"";
}

HANDLE cha::RevisarInstaladorVBCable(const std::wstring& ruta, std::wstring& error)
{
    // Abierto solo para leer, sin dejar que otro lo escriba, lo borre o lo cambie de nombre hasta cerrarlo
    HANDLE archivo = CreateFileW(ruta.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (archivo == INVALID_HANDLE_VALUE)
    {
        error = L"No pude abrir el instalador de VB-CABLE que viene con Chachalaca.";
        return INVALID_HANDLE_VALUE;
    }
    const std::string huella = Sha256(archivo);
    const bool firmaOk = FirmaDeVBAudio(ruta, archivo);
    Registrar("instalador de VB-CABLE: huella " + std::string(huella == HuellaInstalador ? "correcta" : "distinta")
              + ", firma " + (firmaOk ? "de VB-Audio" : "no válida"));
    if (huella != HuellaInstalador || !firmaOk)
    {
        CloseHandle(archivo);
        error = L"El instalador de VB-CABLE que viene con Chachalaca no es el original de VB-Audio. Por seguridad no lo abro.";
        return INVALID_HANDLE_VALUE;
    }
    return archivo;
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
