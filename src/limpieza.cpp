#include "limpieza.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#pragma warning(push, 0)
#include <speex/speex_preprocess.h>
#pragma warning(pop)

namespace
{
    // Cola circular de muestras con tamaño fijo (no pide memoria después de crearse)
    struct Fila
    {
        std::vector<float> buf;
        size_t inicio = 0, cuantos = 0;

        void Preparar(size_t capacidad) { buf.assign(capacidad, 0.0f); inicio = cuantos = 0; }
        void Poner(const float* x, size_t n)
        {
            n = std::min(n, buf.size() - cuantos);
            for (size_t i = 0; i < n; i++) buf[(inicio + cuantos + i) % buf.size()] = x[i];
            cuantos += n;
        }
        size_t Sacar(float* x, size_t n)
        {
            n = std::min(n, cuantos);
            for (size_t i = 0; i < n; i++) x[i] = buf[(inicio + i) % buf.size()];
            inicio = (inicio + n) % buf.size();
            cuantos -= n;
            return n;
        }
        void Vaciar() { inicio = cuantos = 0; }
    };
}

struct cha::LimpiadorVoz::Impl
{
    int cuadro;                         // 10 ms
    SpeexPreprocessState* speex = nullptr;
    Fila entrada, salida;
    std::vector<float> uno, original;
    std::vector<spx_int16_t> enteros;
    bool activo = false;

    // Puerta: se abre con voz y se cierra despacio después de 300 ms sin voz
    float ganancia = 1.0f;
    int sinVoz = 0;
    float subida, bajada;

    explicit Impl(int frecuencia)
        : cuadro(frecuencia / 100)
    {
        speex = speex_preprocess_state_init(cuadro, frecuencia);
        if (!speex) return;   // sin Speex no se limpia nada (Procesar deja pasar la voz tal cual)
        int si = 1, no = 0, supresion = -30;
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_DENOISE, &si);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &supresion);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_AGC, &no);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_DEREVERB, &no);
        speex_preprocess_ctl(speex, SPEEX_PREPROCESS_SET_VAD, &si);
        entrada.Preparar((size_t)BloqueMaximo + cuadro);
        salida.Preparar((size_t)BloqueMaximo + 2 * cuadro);
        uno.resize(cuadro);
        original.resize(cuadro);
        enteros.resize(cuadro);
        subida = 1.0f - std::exp(-1.0f / (0.005f * frecuencia));   // 5 ms
        bajada = 1.0f - std::exp(-1.0f / (0.120f * frecuencia));   // 120 ms
    }

    ~Impl()
    {
        if (speex) speex_preprocess_state_destroy(speex);
    }

    void Reiniciar()
    {
        entrada.Vaciar();
        salida.Vaciar();
        std::fill(uno.begin(), uno.end(), 0.0f);   // al encender: los 10 ms de retraso, sin pedir memoria
        salida.Poner(uno.data(), uno.size());
        ganancia = 1.0f;
        sinVoz = 0;
    }
};

cha::LimpiadorVoz::LimpiadorVoz(int frecuencia) : m(std::make_unique<Impl>(frecuencia)) {}
cha::LimpiadorVoz::~LimpiadorVoz() = default;

void cha::LimpiadorVoz::Procesar(const float* in, float* out, int n, bool quitarRuido, bool soloVoz)
{
    auto& s = *m;
    n = std::clamp(n, 0, BloqueMaximo);
    if ((!quitarRuido && !soloVoz) || !s.speex || s.uno.empty())
    {
        s.activo = false;
        std::copy(in, in + n, out);
        return;
    }
    if (!s.activo)
    {
        s.Reiniciar();
        s.activo = true;
    }

    s.entrada.Poner(in, (size_t)n);
    while (s.entrada.cuantos >= (size_t)s.cuadro)
    {
        s.entrada.Sacar(s.uno.data(), (size_t)s.cuadro);
        float energia = 0;
        for (int i = 0; i < s.cuadro; i++)
        {
            float x = std::clamp(s.uno[i], -1.0f, 1.0f);
            s.original[i] = x;
            energia += x * x;
            s.enteros[i] = (spx_int16_t)std::lround(x * 32767.0f);
        }
        const float nivel = std::sqrt(energia / (float)s.cuadro);
        const int hayVoz = speex_preprocess_run(s.speex, s.enteros.data());

        for (int i = 0; i < s.cuadro; i++) s.uno[i] = quitarRuido ? s.enteros[i] / 32768.0f : s.original[i];

        if (soloVoz)
        {
            const bool voz = (hayVoz && nivel > 0.004f) || nivel > 0.03f;
            s.sinVoz = voz ? 0 : s.sinVoz + 1;
            const float meta = s.sinVoz < 30 ? 1.0f : 0.0f;   // 30 cuadros = 300 ms
            for (int i = 0; i < s.cuadro; i++)
            {
                s.ganancia += (meta - s.ganancia) * (meta > s.ganancia ? s.subida : s.bajada);
                s.uno[i] *= s.ganancia;
            }
        }
        s.salida.Poner(s.uno.data(), (size_t)s.cuadro);
    }

    size_t sacados = s.salida.Sacar(out, (size_t)n);
    if (sacados < (size_t)n) std::fill(out + sacados, out + n, 0.0f);
}
