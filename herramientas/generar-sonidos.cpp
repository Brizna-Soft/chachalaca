// Hace los sonidos que trae Chachalaca. Todos se inventan aquí desde cero, con matemáticas (no usan grabaciones de
// nadie), así que son de Brizna Soft y van con la licencia MIT del programa.
//
// Uso: generar-sonidos.exe <carpeta>

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr int Fs = 48000;
    constexpr double Pi = 3.14159265358979;
    using Pista = std::vector<float>;

    std::mt19937 g_azar(20261028);
    float Ruido() { return std::uniform_real_distribution<float>(-1.0f, 1.0f)(g_azar); }

    void Crecer(Pista& s, size_t n)
    {
        if (s.size() < n) s.resize(n, 0.0f);
    }

    // Envolvente: subida lineal al principio y bajada lineal al final
    double Envolvente(double t, double dur, double subida, double bajada)
    {
        if (t < 0 || t > dur) return 0;
        double a = subida > 0 ? std::min(1.0, t / subida) : 1.0;
        double b = bajada > 0 && t > dur - bajada ? (dur - t) / bajada : 1.0;
        return a * b;
    }

    // Un tono con armónicos (amps[k] es el armónico k+1). «f» da la frecuencia en cada momento; «caida» apaga
    // exponencialmente (0 = no se apaga).
    void Tono(Pista& s, double t0, double dur, const std::function<double(double)>& f, const std::vector<double>& amps,
              double volumen, double subida, double bajada, double caida = 0)
    {
        const size_t i0 = (size_t)(t0 * Fs), n = (size_t)(dur * Fs);
        Crecer(s, i0 + n);
        double fase = 0;
        for (size_t i = 0; i < n; i++)
        {
            const double t = (double)i / Fs, fr = f(t);
            fase += 2 * Pi * fr / Fs;
            double v = 0;
            for (size_t k = 0; k < amps.size(); k++)
            {
                if (fr * (double)(k + 1) > 18000) break;
                v += amps[k] * std::sin(fase * (double)(k + 1));
            }
            double env = Envolvente(t, dur, subida, bajada) * (caida > 0 ? std::exp(-caida * t) : 1.0);
            s[i0 + i] += (float)(v * volumen * env);
        }
    }

    // Ruido filtrado: «corte(t)» es la frecuencia de un pasa-bajos sencillo; «agudo» lo vuelve pasa-altos
    void Soplo(Pista& s, double t0, double dur, const std::function<double(double)>& corte, double volumen, double subida,
               double caida, bool agudo = false)
    {
        const size_t i0 = (size_t)(t0 * Fs), n = (size_t)(dur * Fs);
        Crecer(s, i0 + n);
        float y = 0;
        for (size_t i = 0; i < n; i++)
        {
            const double t = (double)i / Fs;
            const float x = Ruido();
            const float a = (float)(1.0 - std::exp(-2 * Pi * corte(t) / Fs));
            y += a * (x - y);
            const float v = agudo ? x - y : y;
            s[i0 + i] += (float)(v * volumen * Envolvente(t, dur, subida, 0.01) * std::exp(-caida * t));
        }
    }

    const std::vector<double> Metal = { 1, 0.8, 0.6, 0.45, 0.32, 0.22, 0.14, 0.09, 0.06 };
    const std::vector<double> Cuadrada = { 1, 0, 1.0 / 3, 0, 1.0 / 5, 0, 1.0 / 7, 0, 1.0 / 9 };
    std::vector<double> Sierra(int n)
    {
        std::vector<double> a;
        for (int k = 1; k <= n; k++) a.push_back(1.0 / k);
        return a;
    }

    Pista TrombonTriste()   // «cua cua cua cuaaa»
    {
        Pista s;
        const double notas[] = { 196.00, 185.00, 174.61 };
        double t = 0;
        for (double hz : notas)
        {
            Tono(s, t, 0.42, [hz](double) { return hz; }, Metal, 0.5, 0.05, 0.10);
            t += 0.47;
        }
        Tono(s, t, 1.5, [](double x) { return 164.81 * (1 + (x > 0.25 ? 0.02 * std::sin(2 * Pi * 5.5 * x) : 0)); }, Metal, 0.5,
             0.05, 0.6);
        return s;
    }

    Pista Bocina()
    {
        Pista s;
        const double inicio[] = { 0.0, 0.30, 0.60 }, largo[] = { 0.22, 0.22, 1.0 };
        for (int b = 0; b < 3; b++)
        {
            for (double hz : { 466.16, 554.37, 698.46 })
                Tono(s, inicio[b], largo[b], [hz](double) { return hz; }, Sierra(14), 0.33, 0.01, 0.05);
        }
        for (auto& v : s) v = (float)std::tanh(2.5 * v);
        return s;
    }

    Pista Wiii()
    {
        Pista s;
        Tono(s, 0, 0.85, [](double t) { return 500 * std::pow(1800.0 / 500.0, std::min(t, 0.75) / 0.75) * (1 + 0.02 * std::sin(2 * Pi * 7 * t)); },
             { 1, 0.1 }, 0.8, 0.02, 0.12);
        return s;
    }

    Pista UuuSeCae()
    {
        Pista s;
        Tono(s, 0, 1.2, [](double t) { return 1800 * std::pow(300.0 / 1800.0, t / 1.2) * (1 + 0.015 * std::sin(2 * Pi * 6 * t)); },
             { 1, 0.1 }, 0.8, 0.02, 0.15);
        return s;
    }

    Pista Boing()
    {
        Pista s;
        Tono(s, 0, 0.8, [](double t) { return 200 * (1 + 0.5 * std::exp(-5 * t) * std::sin(2 * Pi * 11 * t)); }, { 1, 0.3, 0.1 },
             0.9, 0.005, 0.1, 3);
        return s;
    }

    Pista BaDumTss()
    {
        Pista s;
        Tono(s, 0.00, 0.25, [](double t) { return 110 + 90 * std::exp(-20 * t); }, { 1, 0.2 }, 0.9, 0.002, 0.05, 12);
        Tono(s, 0.22, 0.30, [](double t) { return 85 + 70 * std::exp(-20 * t); }, { 1, 0.2 }, 0.9, 0.002, 0.05, 10);
        Soplo(s, 0.50, 1.0, [](double) { return 6000.0; }, 0.6, 0.002, 3.5, true);
        return s;
    }

    Pista Redoble()
    {
        Pista s;
        for (double t = 0; t < 1.9; t += 1.0 / 16)
        {
            const double fuerza = 0.3 + 0.7 * t / 1.9;
            Soplo(s, t, 0.06, [](double) { return 2500.0; }, 0.5 * fuerza, 0.001, 50, true);
            Tono(s, t, 0.06, [](double) { return 190.0; }, { 1 }, 0.3 * fuerza, 0.001, 0.02, 40);
        }
        Soplo(s, 2.0, 1.5, [](double) { return 7000.0; }, 0.8, 0.002, 2.0, true);
        Tono(s, 2.0, 0.4, [](double t) { return 90 + 60 * std::exp(-15 * t); }, { 1 }, 0.7, 0.002, 0.1, 8);
        return s;
    }

    Pista Victoria()
    {
        Pista s;
        const double notas[] = { 523.25, 659.25, 783.99 };
        double t = 0;
        for (double hz : notas)
        {
            Tono(s, t, 0.13, [hz](double) { return hz; }, Cuadrada, 0.4, 0.005, 0.03);
            t += 0.15;
        }
        Tono(s, t, 0.75, [](double x) { return 1046.5 * (1 + 0.01 * std::sin(2 * Pi * 6 * x)); }, Cuadrada, 0.4, 0.005, 0.4);
        return s;
    }

    Pista Error()
    {
        Pista s;
        Tono(s, 0.00, 0.16, [](double) { return 220.0; }, Cuadrada, 0.5, 0.005, 0.03);
        Tono(s, 0.22, 0.38, [](double) { return 185.0; }, Cuadrada, 0.5, 0.005, 0.08);
        return s;
    }

    Pista Ding()
    {
        Pista s;
        const double razon[] = { 1, 2.76, 5.40, 8.93 }, amp[] = { 1, 0.6, 0.35, 0.2 }, caida[] = { 2.2, 3.5, 5, 7 };
        for (int k = 0; k < 4; k++)
        {
            const double hz = 880 * razon[k];
            Tono(s, 0, 2.0, [hz](double) { return hz; }, { 1 }, 0.4 * amp[k], 0.002, 0.2, caida[k]);
        }
        return s;
    }

    Pista Grillos()
    {
        Pista s;
        for (double grupo = 0; grupo < 2.4; grupo += 0.55)
        {
            for (int p = 0; p < 3; p++)
            {
                Tono(s, grupo + p * 0.05, 0.03, [](double) { return 4400.0; }, { 1 }, 0.5, 0.005, 0.01);
                Tono(s, grupo + 0.27 + p * 0.05, 0.03, [](double) { return 4900.0; }, { 1 }, 0.3, 0.005, 0.01);
            }
        }
        return s;
    }

    Pista Sirena()
    {
        Pista s;
        Tono(s, 0, 2.6, [](double t) { return 900 + 300 * std::sin(2 * Pi * 1.1 * t - Pi / 2); }, { 1, 0.4, 0.2, 0.1 }, 0.6, 0.05, 0.25);
        return s;
    }

    Pista Laser()
    {
        Pista s;
        for (double t0 : { 0.0, 0.32 })
            Tono(s, t0, 0.28, [](double t) { return 2600 * std::exp(-9 * t) + 250; }, { 1, 0.3 }, 0.8, 0.002, 0.05, 4);
        return s;
    }

    Pista Explosion()
    {
        Pista s;
        Soplo(s, 0, 1.8, [](double t) { return 150 + 5850 * std::exp(-4 * t); }, 1.4, 0.005, 2.2);
        Tono(s, 0, 0.9, [](double t) { return 40 + 30 * std::exp(-6 * t); }, { 1 }, 0.9, 0.003, 0.2, 4);
        for (auto& v : s) v = (float)std::tanh(1.5 * v);
        return s;
    }

    Pista Pedo()
    {
        Pista s;
        Tono(s, 0, 0.9, [](double t) { return 75 + 15 * std::sin(2 * Pi * 3 * t) + 4 * Ruido(); }, Sierra(15), 0.5, 0.03, 0.25);
        Soplo(s, 0, 0.9, [](double) { return 500.0; }, 0.25, 0.03, 1.0);
        // Pasa-bajos y el temblor
        float y = 0;
        for (size_t i = 0; i < s.size(); i++)
        {
            const double t = (double)i / Fs;
            y += 0.06f * (s[i] - y);
            s[i] = (float)(y * (0.7 + 0.3 * std::sin(2 * Pi * 9 * t)) * 3.0);
        }
        return s;
    }

    Pista Suspenso()   // «dun dun duuun»
    {
        Pista s;
        Tono(s, 0.00, 0.32, [](double) { return 146.83; }, Metal, 0.5, 0.02, 0.08);
        Tono(s, 0.44, 0.32, [](double) { return 146.83; }, Metal, 0.5, 0.02, 0.08);
        for (double hz : { 123.47, 146.83, 174.61 })
            Tono(s, 0.88, 1.6, [hz](double t) { return hz * (1 + 0.004 * std::sin(2 * Pi * 6 * t)); }, Metal, 0.33, 0.03, 0.7);
        return s;
    }

    // Normaliza a -1 dB, suaviza las puntas y guarda un WAV mono de 16 bits
    bool Guardar(const std::wstring& ruta, Pista s)
    {
        float pico = 0;
        for (float v : s) pico = std::max(pico, std::fabs(v));
        const float escala = pico > 0 ? 0.89f / pico : 1.0f;
        const size_t borde = std::min<size_t>(96, s.size() / 2);
        for (size_t i = 0; i < s.size(); i++)
        {
            float g = 1;
            if (i < borde) g = (float)i / borde;
            if (i >= s.size() - borde) g = (float)(s.size() - 1 - i) / borde;
            s[i] *= escala * g;
        }
        FILE* f = nullptr;
        if (_wfopen_s(&f, ruta.c_str(), L"wb") != 0 || !f) return false;
        const uint32_t datos = (uint32_t)(s.size() * 2), total = 36 + datos, velocidad = Fs * 2, fs = Fs;
        const uint16_t pcm = 1, canales = 1, bloque = 2, bits = 16;
        const uint32_t veinte = 16;
        fwrite("RIFF", 1, 4, f); fwrite(&total, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
        fwrite(&veinte, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&canales, 2, 1, f); fwrite(&fs, 4, 1, f);
        fwrite(&velocidad, 4, 1, f); fwrite(&bloque, 2, 1, f); fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&datos, 4, 1, f);
        for (float v : s)
        {
            int16_t m = (int16_t)std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
            fwrite(&m, 2, 1, f);
        }
        return fclose(f) == 0;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        wprintf(L"Uso: generar-sonidos.exe <carpeta>\n");
        return 2;
    }
    std::wstring carpeta = argv[1];
    CreateDirectoryW(carpeta.c_str(), nullptr);
    const struct { const wchar_t* archivo; Pista (*hacer)(); } lista[] = {
        { L"trombon-triste.wav", TrombonTriste }, { L"bocina.wav", Bocina }, { L"wiii.wav", Wiii },
        { L"uuu-se-cae.wav", UuuSeCae }, { L"boing.wav", Boing }, { L"ba-dum-tss.wav", BaDumTss },
        { L"redoble.wav", Redoble }, { L"victoria.wav", Victoria }, { L"error.wav", Error }, { L"ding.wav", Ding },
        { L"grillos.wav", Grillos }, { L"sirena.wav", Sirena }, { L"laser.wav", Laser }, { L"explosion.wav", Explosion },
        { L"pedo.wav", Pedo }, { L"suspenso.wav", Suspenso },
    };
    int ok = 0;
    for (const auto& s : lista)
    {
        if (Guardar(carpeta + L"\\" + s.archivo, s.hacer())) ok++;
        else wprintf(L"No pude guardar %s\n", s.archivo);
    }
    wprintf(L"Sonidos listos: %d de %d\n", ok, (int)(sizeof(lista) / sizeof(lista[0])));
    return ok == (int)(sizeof(lista) / sizeof(lista[0])) ? 0 : 1;
}
