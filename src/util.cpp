#include "util.hpp"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <mutex>

std::wstring cha::Ancho(const std::string& utf8)
{
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring r(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), r.data(), n);
    return r;
}

std::string cha::Utf8(const std::wstring& texto)
{
    if (texto.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, texto.data(), (int)texto.size(), nullptr, 0, nullptr, nullptr);
    std::string r(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, texto.data(), (int)texto.size(), r.data(), n, nullptr, nullptr);
    return r;
}

std::wstring cha::CarpetaDatos()
{
    PWSTR ruta = nullptr;
    std::wstring r;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &ruta))) r = ruta;
    CoTaskMemFree(ruta);
    r += L"\\Chachalaca";
    CreateDirectoryW(r.c_str(), nullptr);
    return r;
}

bool cha::ExisteArchivo(const std::wstring& ruta)
{
    DWORD a = GetFileAttributesW(ruta.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool cha::EscribirSeguro(const std::wstring& ruta, const std::string& contenido)
{
    std::wstring temporal = ruta + L".nuevo";
    HANDLE h = CreateFileW(temporal.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD escritos = 0;
    BOOL ok = WriteFile(h, contenido.data(), (DWORD)contenido.size(), &escritos, nullptr) && escritos == contenido.size();
    ok = ok && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) { DeleteFileW(temporal.c_str()); return false; }
    return MoveFileExW(temporal.c_str(), ruta.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool cha::LeerArchivo(const std::wstring& ruta, std::string& contenido)
{
    HANDLE h = CreateFileW(ruta.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER t;
    bool ok = GetFileSizeEx(h, &t) && t.QuadPart < 16LL * 1024 * 1024;
    if (ok)
    {
        contenido.resize((size_t)t.QuadPart);
        DWORD leidos = 0;
        ok = contenido.empty() || (ReadFile(h, contenido.data(), (DWORD)contenido.size(), &leidos, nullptr) && leidos == contenido.size());
    }
    CloseHandle(h);
    return ok;
}

void cha::Registrar(const std::string& texto)
{
    static std::mutex m;
    std::lock_guard g(m);
    std::wstring ruta = CarpetaDatos() + L"\\registro.txt";
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (GetFileAttributesExW(ruta.c_str(), GetFileExInfoStandard, &d) && d.nFileSizeLow > 256 * 1024)
        MoveFileExW(ruta.c_str(), (ruta + L".anterior").c_str(), MOVEFILE_REPLACE_EXISTING);
    HANDLE h = CreateFileW(ruta.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char hora[32];
    snprintf(hora, sizeof(hora), "%04d-%02d-%02d %02d:%02d:%02d ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    std::string linea = hora + texto + "\r\n";
    DWORD escritos = 0;
    WriteFile(h, linea.data(), (DWORD)linea.size(), &escritos, nullptr);
    CloseHandle(h);
}
