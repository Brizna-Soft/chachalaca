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
    constexpr double Vuelta = 2 * 3.14159265358979;

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
        // Realce en una banda: da el tono nasal de las voces de caricatura
        static Biquad Pico(float f, float fs, float db, float q)
        {
            float a = std::pow(10.0f, db / 40.0f), w = 2 * Pi * f / fs, c = std::cos(w), alfa = std::sin(w) / (2 * q);
            float a0 = 1 + alfa / a;
            Biquad b;
            b.b0 = (1 + alfa * a) / a0; b.b1 = -2 * c / a0; b.b2 = (1 - alfa * a) / a0;
            b.a1 = -2 * c / a0; b.a2 = (1 - alfa / a) / a0;
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

    // Detector de tono (YIN) para el autotune. Trabaja a 24 kHz sobre los últimos 50 ms; no pide memoria al medir.
    class DetectorTono
    {
    public:
        explicit DetectorTono(float fs) : m_fs(fs)
        {
            m_historia.assign(Largo * 2, 0.0f);
            m_baja.assign(Largo, 0.0f);
            m_dif.assign(Ventana + 1, 0.0f);
        }

        void Agregar(const float* x, int n)
        {
            for (int i = 0; i < n; i++)
            {
                m_historia[m_pos] = x[i];
                m_pos = (m_pos + 1) % m_historia.size();
            }
        }

        // Frecuencia en Hz, o 0 si no hay voz con tono claro
        float Medir()
        {
            // Lo más reciente, de viejo a nuevo, a la mitad de muestras
            float energia = 0;
            for (size_t i = 0; i < Largo; i++)
            {
                size_t a = (m_pos + 2 * i) % m_historia.size(), b = (a + 1) % m_historia.size();
                m_baja[i] = 0.5f * (m_historia[a] + m_historia[b]);
                energia += m_baja[i] * m_baja[i];
            }
            if (std::sqrt(energia / Largo) < 0.01f) return 0;

            // Diferencia y diferencia normalizada acumulada
            for (size_t tau = 1; tau <= Ventana; tau++)
            {
                float d = 0;
                for (size_t j = 0; j < Ventana; j++)
                {
                    float e = m_baja[j] - m_baja[j + tau];
                    d += e * e;
                }
                m_dif[tau] = d;
            }
            float acumulado = 0;
            const float fsBaja = m_fs / 2;
            const size_t tauMin = (size_t)(fsBaja / 800.0f), tauMax = std::min<size_t>(Ventana - 1, (size_t)(fsBaja / 70.0f));
            for (size_t tau = 1; tau <= tauMax; tau++)
            {
                acumulado += m_dif[tau];
                m_normal[tau] = acumulado > 0 ? m_dif[tau] * (float)tau / acumulado : 1.0f;
            }
            for (size_t tau = std::max<size_t>(tauMin, 2); tau < tauMax; tau++)
            {
                if (m_normal[tau] < 0.15f && m_normal[tau] <= m_normal[tau + 1])
                {
                    // Interpolación parabólica alrededor del mínimo
                    float a = m_normal[tau - 1], b = m_normal[tau], c = m_normal[tau + 1];
                    float den = a - 2 * b + c;
                    float fino = (float)tau + (std::fabs(den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f);
                    return fino > 0 ? fsBaja / fino : 0;
                }
            }
            return 0;
        }

    private:
        static constexpr size_t Ventana = 600;   // 25 ms a 24 kHz
        static constexpr size_t Largo = Ventana * 2;
        float m_fs;
        std::vector<float> m_historia, m_baja, m_dif;
        std::array<float, Ventana + 2> m_normal{};
        size_t m_pos = 0;
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
        case cha::Efecto::Duende: return 6;
        case cha::Efecto::Raton: return 9;
        case cha::Efecto::Caricatura: return 4;
        default: return 0;
        }
    }

    bool CambiaTono(cha::Efecto e)
    {
        return Semitonos(e) != 0 || e == cha::Efecto::Autotune || e == cha::Efecto::RobotAutotune;
    }

    // Nota más cercana. Con «pentatonica», solo las notas de la escala pentatónica (suena más «robot cantando»).
    float Correccion(float hz, bool pentatonica)
    {
        float midi = 69.0f + 12.0f * std::log2(hz / 440.0f);
        float meta = std::round(midi);
        if (pentatonica)
        {
            static const int escala[] = { 0, 2, 4, 7, 9 };
            float mejor = meta, distancia = 1e9f;
            for (int octava = -1; octava <= 1; octava++)
            {
                for (int g : escala)
                {
                    float n = 12.0f * (std::floor(midi / 12.0f) + (float)octava) + (float)g;
                    if (std::fabs(n - midi) < distancia) { distancia = std::fabs(n - midi); mejor = n; }
                }
            }
            meta = mejor;
        }
        return std::clamp(meta - midi, -3.0f, 3.0f);
    }
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
    case Efecto::Duende: return L"Duende";
    case Efecto::Raton: return L"Ratón";
    case Efecto::Caricatura: return L"Caricatura";
    case Efecto::Robot: return L"Robot";
    case Efecto::Autotune: return L"Autotune";
    case Efecto::RobotAutotune: return L"Robot autotune";
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
    case Efecto::Duende: return "duende";
    case Efecto::Raton: return "raton";
    case Efecto::Caricatura: return "caricatura";
    case Efecto::Robot: return "robot";
    case Efecto::Autotune: return "autotune";
    case Efecto::RobotAutotune: return "robot-autotune";
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

    double fase = 0, faseVibrato = 0;   // robot y caricatura
    Retardo peineRobot;
    Retardo eco;
    Biquad altos1, altos2, bajos1, bajos2;   // radio
    Biquad nasal, altosRaton;                // voces de caricatura
    std::array<Peine, 8> peines;             // cueva
    std::array<PasaTodo, 4> pasaTodos;
    DetectorTono detector;
    float correccion = 0;                    // autotune, en semitonos

    explicit Impl(int frecuencia) : fs((float)frecuencia), detector((float)frecuencia)
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
        altosRaton = Biquad::PasaAltos(250, fs);
        const int largoPeines[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        const int largoPasaTodos[4] = { 556, 441, 341, 225 };
        for (size_t i = 0; i < peines.size(); i++) peines[i].r.Preparar((size_t)(largoPeines[i] * fs / 44100.0f));
        for (size_t i = 0; i < pasaTodos.size(); i++) pasaTodos[i].r.Preparar((size_t)(largoPasaTodos[i] * fs / 44100.0f));
    }

    void Reiniciar(Efecto e)
    {
        if (CambiaTono(e)) tono.reset();
        correccion = 0;
        switch (e)
        {
        case Efecto::Robot:
        case Efecto::RobotAutotune: peineRobot.Reiniciar(); fase = 0; break;
        case Efecto::Eco: eco.Reiniciar(); break;
        case Efecto::Radio: altos1.Reiniciar(); altos2.Reiniciar(); bajos1.Reiniciar(); bajos2.Reiniciar(); break;
        case Efecto::Duende: nasal = Biquad::Pico(1200, fs, 9, 1.2f); break;
        case Efecto::Raton: nasal = Biquad::Pico(2000, fs, 6, 1.0f); altosRaton.Reiniciar(); break;
        case Efecto::Caricatura: nasal = Biquad::Pico(1600, fs, 9, 1.5f); faseVibrato = 0; break;
        case Efecto::Cueva:
            for (auto& p : peines) p.Reiniciar();
            for (auto& a : pasaTodos) a.Reiniciar();
            break;
        default: break;
        }
    }

    // Robot: anillo a 55 Hz y un peine corto que suena metálico. «suave» para el robot que canta.
    void Robotizar(const float* entrada, float* salida, int n, bool suave)
    {
        const double paso = Vuelta * (suave ? 40.0 : 55.0) / fs;
        for (int i = 0; i < n; i++)
        {
            float anillo = (float)std::sin(fase);
            float x = suave ? entrada[i] * (0.6f + 0.4f * anillo) : entrada[i] * anillo * 1.6f;
            fase += paso;
            if (fase > Vuelta) fase -= Vuelta;
            float y = x + peineRobot.Leer() * (suave ? 0.45f : 0.55f);
            peineRobot.Escribir(y);
            salida[i] = y * 0.8f;
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

    if (CambiaTono(e))
    {
        float semitonos = Semitonos(e);
        if (e == Efecto::Autotune || e == Efecto::RobotAutotune)
        {
            // Autotune duro: lleva cada nota a la más cercana, al instante
            s.detector.Agregar(entrada, n);
            float hz = s.detector.Medir();
            if (hz > 0) s.correccion = Correccion(hz, e == Efecto::RobotAutotune);
            else s.correccion *= 0.9f;
            semitonos = s.correccion;
        }
        else if (e == Efecto::Caricatura)
        {
            // Un vibrato rápido y alegre
            semitonos += 0.35f * (float)std::sin(s.faseVibrato);
            s.faseVibrato += Vuelta * 5.5 * n / s.fs;
            if (s.faseVibrato > Vuelta) s.faseVibrato -= Vuelta;
        }
        s.tono.setTransposeSemitones(semitonos);
        std::copy(entrada, entrada + n, s.copia.data());
        float* entradas[1] = { s.copia.data() };
        float* salidas[1] = { salida };
        s.tono.process(entradas, n, salidas, n);

        switch (e)
        {
        case Efecto::Monstruo:
            for (int i = 0; i < n; i++) salida[i] = Saturar(salida[i] * 1.3f, 1.6f);
            break;
        case Efecto::Duende:
            for (int i = 0; i < n; i++) salida[i] = Saturar(s.nasal.Procesar(salida[i]) * 0.8f, 1.3f);
            break;
        case Efecto::Raton:
            for (int i = 0; i < n; i++) salida[i] = s.nasal.Procesar(s.altosRaton.Procesar(salida[i])) * 0.85f;
            break;
        case Efecto::Caricatura:
            for (int i = 0; i < n; i++) salida[i] = s.nasal.Procesar(salida[i]) * 0.8f;
            break;
        case Efecto::RobotAutotune:
            std::copy(salida, salida + n, s.copia.data());
            s.Robotizar(s.copia.data(), salida, n, true);
            break;
        default: break;
        }
        return;
    }

    switch (e)
    {
    case Efecto::Robot:
        s.Robotizar(entrada, salida, n, false);
        break;
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
