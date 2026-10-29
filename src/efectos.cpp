#include "efectos.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#pragma warning(push, 0)
#include <signalsmith-stretch/signalsmith-stretch.h>
#pragma warning(pop)

namespace
{
    constexpr float Pi = 3.14159265358979f;

    // Filtro de segundo orden (fórmulas de Robert Bristow-Johnson)
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

        float Procesar(float x)
        {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void Reiniciar() { z1 = z2 = 0; }

        static Biquad PasaAltos(float f, float fs, float q = 0.7071f)
        {
            float w = 2 * Pi * f / fs, c = std::cos(w), alfa = std::sin(w) / (2 * q), a0 = 1 + alfa;
            Biquad b;
            b.b0 = (1 + c) / 2 / a0; b.b1 = -(1 + c) / a0; b.b2 = (1 + c) / 2 / a0;
            b.a1 = -2 * c / a0; b.a2 = (1 - alfa) / a0;
            return b;
        }
        static Biquad PasaBajos(float f, float fs, float q = 0.7071f)
        {
            float w = 2 * Pi * f / fs, c = std::cos(w), alfa = std::sin(w) / (2 * q), a0 = 1 + alfa;
            Biquad b;
            b.b0 = (1 - c) / 2 / a0; b.b1 = (1 - c) / a0; b.b2 = (1 - c) / 2 / a0;
            b.a1 = -2 * c / a0; b.a2 = (1 - alfa) / a0;
            return b;
        }
    };

    // Línea de retardo circular
    struct Retardo
    {
        std::vector<float> buf;
        size_t pos = 0;
        void Preparar(size_t n) { buf.assign(std::max<size_t>(n, 1), 0.0f); pos = 0; }
        float Leer() const { return buf[pos]; }
        void Escribir(float x) { buf[pos] = x; if (++pos >= buf.size()) pos = 0; }
        void Reiniciar() { std::fill(buf.begin(), buf.end(), 0.0f); pos = 0; }
    };

    // Peine con amortiguación y pasa-todo, como en Freeverb (dominio público)
    struct Peine
    {
        Retardo r;
        float filtro = 0, realim = 0.86f, amort = 0.25f;
        float Procesar(float x)
        {
            float y = r.Leer();
            filtro = y * (1 - amort) + filtro * amort;
            r.Escribir(x + filtro * realim);
            return y;
        }
        void Reiniciar() { r.Reiniciar(); filtro = 0; }
    };

    struct PasaTodo
    {
        Retardo r;
        float realim = 0.5f;
        float Procesar(float x)
        {
            float b = r.Leer();
            r.Escribir(x + b * realim);
            return b - x;
        }
        void Reiniciar() { r.Reiniciar(); }
    };

    float Saturar(float x, float fuerza) { return std::tanh(x * fuerza) / std::tanh(fuerza); }

    float Semitonos(cha::Efecto e)
    {
        switch (e)
        {
        case cha::Efecto::Grave: return -4;
        case cha::Efecto::Aguda: return 4;
        case cha::Efecto::Ardilla: return 8;
        case cha::Efecto::Monstruo: return -8;
        default: return 0;
        }
    }

    bool CambiaTono(cha::Efecto e) { return Semitonos(e) != 0; }
}

const wchar_t* cha::NombreEfecto(Efecto e)
{
    switch (e)
    {
    case Efecto::Normal: return L"Normal";
    case Efecto::Grave: return L"Grave";
    case Efecto::Aguda: return L"Aguda";
    case Efecto::Ardilla: return L"Ardilla";
    case Efecto::Monstruo: return L"Monstruo";
    case Efecto::Robot: return L"Robot";
    case Efecto::Eco: return L"Eco";
    case Efecto::Radio: return L"Radio";
    case Efecto::Cueva: return L"Cueva";
    default: return L"";
    }
}

const char* cha::ClaveEfecto(Efecto e)
{
    switch (e)
    {
    case Efecto::Normal: return "normal";
    case Efecto::Grave: return "grave";
    case Efecto::Aguda: return "aguda";
    case Efecto::Ardilla: return "ardilla";
    case Efecto::Monstruo: return "monstruo";
    case Efecto::Robot: return "robot";
    case Efecto::Eco: return "eco";
    case Efecto::Radio: return "radio";
    case Efecto::Cueva: return "cueva";
    default: return "normal";
    }
}

cha::Efecto cha::EfectoDesdeClave(const std::string& clave)
{
    for (int i = 0; i < (int)Efecto::Cantidad; i++)
    {
        if (clave == ClaveEfecto((Efecto)i)) return (Efecto)i;
    }
    return Efecto::Normal;
}

struct cha::ProcesadorVoz::Impl
{
    float fs;
    signalsmith::stretch::SignalsmithStretch<float> tono;
    Efecto ultimo = Efecto::Normal;
    std::vector<float> copia;

    double fase = 0;          // robot
    Retardo peineRobot;
    Retardo eco;
    Biquad altos1, altos2, bajos1, bajos2;   // radio
    std::array<Peine, 8> peines;             // cueva
    std::array<PasaTodo, 4> pasaTodos;

    explicit Impl(int frecuencia) : fs((float)frecuencia)
    {
        // Bloques de unos 43 ms con pasos de 11 ms: poco retraso para hablar en vivo
        tono.configure(1, 2048, 512);
        copia.resize(BloqueMaximo);
        peineRobot.Preparar((size_t)(0.006f * fs));
        eco.Preparar((size_t)(0.32f * fs));
        altos1 = Biquad::PasaAltos(400, fs);
        altos2 = altos1;
        bajos1 = Biquad::PasaBajos(3000, fs);
        bajos2 = bajos1;
        const int largoPeines[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        const int largoPasaTodos[4] = { 556, 441, 341, 225 };
        for (size_t i = 0; i < peines.size(); i++) peines[i].r.Preparar((size_t)(largoPeines[i] * fs / 44100.0f));
        for (size_t i = 0; i < pasaTodos.size(); i++) pasaTodos[i].r.Preparar((size_t)(largoPasaTodos[i] * fs / 44100.0f));
    }

    void Reiniciar(Efecto e)
    {
        if (CambiaTono(e)) tono.reset();
        switch (e)
        {
        case Efecto::Robot: peineRobot.Reiniciar(); fase = 0; break;
        case Efecto::Eco: eco.Reiniciar(); break;
        case Efecto::Radio: altos1.Reiniciar(); altos2.Reiniciar(); bajos1.Reiniciar(); bajos2.Reiniciar(); break;
        case Efecto::Cueva:
            for (auto& p : peines) p.Reiniciar();
            for (auto& a : pasaTodos) a.Reiniciar();
            break;
        default: break;
        }
    }
};

cha::ProcesadorVoz::ProcesadorVoz(int frecuencia) : m(std::make_unique<Impl>(frecuencia)) {}
cha::ProcesadorVoz::~ProcesadorVoz() = default;

void cha::ProcesadorVoz::Procesar(Efecto e, const float* entrada, float* salida, int n)
{
    auto& s = *m;
    n = std::clamp(n, 0, BloqueMaximo);
    if (e != s.ultimo)
    {
        s.Reiniciar(e);
        s.ultimo = e;
    }

    switch (e)
    {
    case Efecto::Grave:
    case Efecto::Aguda:
    case Efecto::Ardilla:
    case Efecto::Monstruo:
    {
        s.tono.setTransposeSemitones(Semitonos(e));
        std::copy(entrada, entrada + n, s.copia.data());
        float* entradas[1] = { s.copia.data() };
        float* salidas[1] = { salida };
        s.tono.process(entradas, n, salidas, n);
        if (e == Efecto::Monstruo)
        {
            for (int i = 0; i < n; i++) salida[i] = Saturar(salida[i] * 1.3f, 1.6f);
        }
        break;
    }
    case Efecto::Robot:
    {
        const double vuelta = 2 * 3.14159265358979;
        const double paso = vuelta * 55.0 / s.fs;
        for (int i = 0; i < n; i++)
        {
            float x = entrada[i] * (float)std::sin(s.fase) * 1.6f;
            s.fase += paso;
            if (s.fase > vuelta) s.fase -= vuelta;
            float y = x + s.peineRobot.Leer() * 0.55f;
            s.peineRobot.Escribir(y);
            salida[i] = y * 0.8f;
        }
        break;
    }
    case Efecto::Eco:
        for (int i = 0; i < n; i++)
        {
            float d = s.eco.Leer();
            float x = entrada[i];
            s.eco.Escribir(x + d * 0.42f);
            salida[i] = x + d * 0.6f;
        }
        break;
    case Efecto::Radio:
        for (int i = 0; i < n; i++)
        {
            float x = s.bajos2.Procesar(s.bajos1.Procesar(s.altos2.Procesar(s.altos1.Procesar(entrada[i]))));
            salida[i] = Saturar(x * 2.2f, 2.5f) * 0.7f;
        }
        break;
    case Efecto::Cueva:
        for (int i = 0; i < n; i++)
        {
            float x = entrada[i], suma = 0;
            for (auto& p : s.peines) suma += p.Procesar(x * 0.015f);
            for (auto& a : s.pasaTodos) suma = a.Procesar(suma);
            salida[i] = x * 0.75f + suma * 3.0f;
        }
        break;
    default:
        std::copy(entrada, entrada + n, salida);
        break;
    }
}
