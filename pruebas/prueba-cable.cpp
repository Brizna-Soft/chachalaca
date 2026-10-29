// Prueba que no se publica: dice qué dispositivos de sonido hay (y cuáles usa Windows), o graba unos segundos de una
// entrada y dice cuánto sonó. Sirve para comprobar que lo de Chachalaca llega al micrófono virtual.
//
// Uso:
//   prueba-cable.exe --listar
//   prueba-cable.exe "CABLE Output" 6     (graba 6 segundos de la entrada cuyo nombre contenga ese texto)

#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <string>

#include "instalador.hpp"
#include "ma.h"

namespace
{
    std::wstring Ancho(const char* utf8)
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        std::wstring r(n > 0 ? n - 1 : 0, L'\0');
        if (n > 1) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, r.data(), n);
        return r;
    }

    std::wstring Minusculas(std::wstring t)
    {
        for (auto& c : t) c = (wchar_t)std::towlower(c);
        return t;
    }

    struct Medida
    {
        float pico = 0;
        double suma = 0;
        unsigned long long muestras = 0;
    };

    void AlGrabar(ma_device* d, void*, const void* entrada, ma_uint32 n)
    {
        auto* m = static_cast<Medida*>(d->pUserData);
        const float* x = static_cast<const float*>(entrada);
        for (ma_uint32 i = 0; i < n; i++)
        {
            float a = std::fabs(x[i]);
            if (a > m->pico) m->pico = a;
            m->suma += (double)x[i] * x[i];
        }
        m->muestras += n;
    }
}

int wmain(int argc, wchar_t** argv)
{
    // prueba-cable.exe --firma <ruta>: revisa la firma como lo hace Chachalaca antes de abrir el instalador
    if (argc > 2 && std::wstring(argv[1]) == L"--firma")
    {
        std::wstring error;
        HANDLE h = cha::RevisarInstaladorVBCable(argv[2], error);
        const bool ok = h != INVALID_HANDLE_VALUE;
        if (ok) CloseHandle(h);
        wprintf(L"Instalador original de VB-Audio: %s\n", ok ? L"si" : L"no");
        return ok ? 0 : 1;
    }

    ma_context contexto;
    ma_backend backends[] = { ma_backend_wasapi };
    if (ma_context_init(backends, 1, nullptr, &contexto) != MA_SUCCESS)
    {
        wprintf(L"No pude abrir el sonido de Windows\n");
        return 2;
    }
    ma_device_info* salidas = nullptr;
    ma_device_info* entradas = nullptr;
    ma_uint32 ns = 0, ne = 0;
    ma_context_get_devices(&contexto, &salidas, &ns, &entradas, &ne);

    if (argc > 1 && std::wstring(argv[1]) == L"--listar")
    {
        wprintf(L"SALIDAS:\n");
        for (ma_uint32 i = 0; i < ns; i++) wprintf(L"  %s%s\n", Ancho(salidas[i].name).c_str(), salidas[i].isDefault ? L"   <- la de Windows" : L"");
        wprintf(L"ENTRADAS:\n");
        for (ma_uint32 i = 0; i < ne; i++) wprintf(L"  %s%s\n", Ancho(entradas[i].name).c_str(), entradas[i].isDefault ? L"   <- la de Windows" : L"");
        ma_context_uninit(&contexto);
        return 0;
    }

    std::wstring busca = Minusculas(argc > 1 ? argv[1] : L"CABLE Output");
    int segundos = argc > 2 ? _wtoi(argv[2]) : 4;
    const ma_device_info* elegida = nullptr;
    for (ma_uint32 i = 0; i < ne; i++)
    {
        if (Minusculas(Ancho(entradas[i].name)).find(busca) != std::wstring::npos)
        {
            elegida = &entradas[i];
            break;
        }
    }
    if (!elegida)
    {
        wprintf(L"No encontré una entrada que diga «%s»\n", busca.c_str());
        ma_context_uninit(&contexto);
        return 3;
    }

    Medida m;
    ma_device_config c = ma_device_config_init(ma_device_type_capture);
    c.capture.pDeviceID = &elegida->id;
    c.capture.format = ma_format_f32;
    c.capture.channels = 1;
    c.sampleRate = 48000;
    c.dataCallback = AlGrabar;
    c.pUserData = &m;
    ma_device d;
    if (ma_device_init(&contexto, &c, &d) != MA_SUCCESS || ma_device_start(&d) != MA_SUCCESS)
    {
        wprintf(L"No pude grabar de %s\n", Ancho(elegida->name).c_str());
        ma_context_uninit(&contexto);
        return 4;
    }
    Sleep((DWORD)segundos * 1000);
    ma_device_uninit(&d);
    ma_context_uninit(&contexto);

    double rms = m.muestras ? std::sqrt(m.suma / (double)m.muestras) : 0;
    wprintf(L"Grabé %llu muestras de %s: pico %.3f, nivel medio %.4f\n", m.muestras, Ancho(elegida->name).c_str(), m.pico, rms);
    return 0;
}
