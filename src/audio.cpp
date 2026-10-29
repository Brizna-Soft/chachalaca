#include "audio.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwctype>

#include "ma.h"
#include "util.hpp"

namespace
{
    constexpr int Canales = 2;
    constexpr size_t MaxCuadrosSonido = (size_t)cha::Frecuencia * 60 * 10;   // 10 minutos
    constexpr int MaxSonando = 16;

    enum class Orden { Tocar, DetenerTodo };

    struct Comando
    {
        Orden orden = Orden::Tocar;
        std::shared_ptr<const cha::Sonido> sonido;
        float volumen = 1.0f;
    };

    // Cola de un solo productor (la ventana) y un solo consumidor (el hilo de audio), sin candados
    class ColaComandos
    {
    public:
        bool Poner(Comando&& c)
        {
            unsigned cabeza = m_cabeza.load(std::memory_order_relaxed);
            if (cabeza - m_cola.load(std::memory_order_acquire) >= Tamano) return false;
            m_datos[cabeza % Tamano] = std::move(c);
            m_cabeza.store(cabeza + 1, std::memory_order_release);
            return true;
        }
        bool Sacar(Comando& c)
        {
            unsigned cola = m_cola.load(std::memory_order_relaxed);
            if (cola == m_cabeza.load(std::memory_order_acquire)) return false;
            c = std::move(m_datos[cola % Tamano]);
            m_cola.store(cola + 1, std::memory_order_release);
            return true;
        }

    private:
        static constexpr unsigned Tamano = 64;
        std::array<Comando, Tamano> m_datos;
        std::atomic<unsigned> m_cabeza{ 0 }, m_cola{ 0 };
    };

    struct Reproduccion
    {
        std::shared_ptr<const cha::Sonido> sonido;
        size_t pos = 0;
        float volumen = 1.0f;
    };

    // Recorta suave arriba de 0.9 para que nunca se escuche «roto»
    float Limitar(float x)
    {
        float a = std::fabs(x);
        if (a <= 0.9f) return x;
        float y = 0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f);
        return x < 0 ? -y : y;
    }

    std::wstring Minusculas(std::wstring t)
    {
        for (auto& c : t) c = (wchar_t)std::towlower(c);
        return t;
    }
}

std::shared_ptr<const cha::Sonido> cha::CargarSonido(const std::wstring& ruta, std::wstring& error)
{
    ma_decoder_config c = ma_decoder_config_init(ma_format_f32, Canales, Frecuencia);
    ma_decoder d;
    if (ma_decoder_init_file_w(ruta.c_str(), &c, &d) != MA_SUCCESS)
    {
        error = ExisteArchivo(ruta) ? L"No pude abrir el sonido. Chachalaca acepta archivos WAV, MP3 y FLAC."
                                    : L"Ya no encuentro el archivo. ¿Lo moviste o lo borraste?";
        return nullptr;
    }
    auto s = std::make_shared<Sonido>();
    std::vector<float> trozo(4096 * Canales);
    for (;;)
    {
        ma_uint64 leidos = 0;
        ma_result r = ma_decoder_read_pcm_frames(&d, trozo.data(), 4096, &leidos);
        if (leidos > 0) s->muestras.insert(s->muestras.end(), trozo.begin(), trozo.begin() + (ptrdiff_t)(leidos * Canales));
        if (r != MA_SUCCESS || leidos == 0) break;
        if (s->muestras.size() / Canales > MaxCuadrosSonido)
        {
            ma_decoder_uninit(&d);
            error = L"El sonido dura más de 10 minutos. Usa uno más corto.";
            return nullptr;
        }
    }
    ma_decoder_uninit(&d);
    s->cuadros = s->muestras.size() / Canales;
    if (s->cuadros == 0)
    {
        error = L"El archivo no tiene sonido o está dañado.";
        return nullptr;
    }
    return s;
}

struct cha::MotorAudio::Impl
{
    MotorAudio* dueno = nullptr;
    ma_context contexto{};
    bool contextoListo = false;
    std::unique_ptr<ma_device> principal, escucha;
    ma_pcm_rb anillo{};
    bool anilloListo = false;
    bool conVirtual = false, sinMicrofono = false;

    ProcesadorVoz voz{ Frecuencia };
    ColaComandos cola;
    std::array<Reproduccion, MaxSonando> sonando;
    std::atomic<int> cuantosSuenan{ 0 };
    std::atomic<float> nivel{ 0.0f };

    std::vector<float> bufVoz = std::vector<float>(ProcesadorVoz::BloqueMaximo);
    std::vector<float> bufSonidos = std::vector<float>((size_t)ProcesadorVoz::BloqueMaximo * Canales);
    std::vector<float> bufEscucha = std::vector<float>((size_t)ProcesadorVoz::BloqueMaximo * Canales);

    static void AlAudio(ma_device* d, void* salida, const void* entrada, ma_uint32 n)
    {
        static_cast<Impl*>(d->pUserData)->Procesar(static_cast<float*>(salida), static_cast<const float*>(entrada), n);
    }

    static void AlEscuchar(ma_device* d, void* salida, const void*, ma_uint32 n)
    {
        static_cast<Impl*>(d->pUserData)->Escuchar(static_cast<float*>(salida), n);
    }

    bool BuscarId(ma_device_type tipo, const std::wstring& nombre, ma_device_id& id)
    {
        if (nombre.empty() || !contextoListo) return false;
        ma_device_info* reproduccion = nullptr;
        ma_device_info* captura = nullptr;
        ma_uint32 nr = 0, nc = 0;
        if (ma_context_get_devices(&contexto, &reproduccion, &nr, &captura, &nc) != MA_SUCCESS) return false;
        ma_device_info* lista = tipo == ma_device_type_capture ? captura : reproduccion;
        ma_uint32 n = tipo == ma_device_type_capture ? nc : nr;
        for (ma_uint32 i = 0; i < n; i++)
        {
            if (Ancho(lista[i].name) == nombre)
            {
                id = lista[i].id;
                return true;
            }
        }
        return false;
    }

    std::vector<std::wstring> Nombres(bool deCaptura, std::wstring* predeterminado)
    {
        std::vector<std::wstring> r;
        if (!contextoListo) return r;
        ma_device_info* reproduccion = nullptr;
        ma_device_info* captura = nullptr;
        ma_uint32 nr = 0, nc = 0;
        if (ma_context_get_devices(&contexto, &reproduccion, &nr, &captura, &nc) != MA_SUCCESS) return r;
        ma_device_info* lista = deCaptura ? captura : reproduccion;
        ma_uint32 n = deCaptura ? nc : nr;
        for (ma_uint32 i = 0; i < n; i++)
        {
            r.push_back(Ancho(lista[i].name));
            if (predeterminado && lista[i].isDefault) *predeterminado = r.back();
        }
        return r;
    }

    void EscribirAnillo(const float* datos, ma_uint32 restante)
    {
        while (restante > 0)
        {
            ma_uint32 cuantos = restante;
            void* destino = nullptr;
            if (ma_pcm_rb_acquire_write(&anillo, &cuantos, &destino) != MA_SUCCESS || cuantos == 0) break;   // lleno: se pierde
            std::memcpy(destino, datos, (size_t)cuantos * Canales * sizeof(float));
            ma_pcm_rb_commit_write(&anillo, cuantos);
            datos += (size_t)cuantos * Canales;
            restante -= cuantos;
        }
    }

    void Procesar(float* salida, const float* entrada, ma_uint32 total)
    {
        // Órdenes de la ventana
        Comando c;
        while (cola.Sacar(c))
        {
            if (c.orden == Orden::DetenerTodo)
            {
                for (auto& r : sonando) r.sonido.reset();
            }
            else if (c.sonido)
            {
                auto libre = std::find_if(sonando.begin(), sonando.end(), [](const Reproduccion& r) { return !r.sonido; });
                if (libre == sonando.end())   // hay 16 sonando: se cambia el que va más avanzado
                {
                    libre = std::max_element(sonando.begin(), sonando.end(),
                                             [](const Reproduccion& a, const Reproduccion& b) { return a.pos < b.pos; });
                }
                libre->sonido = std::move(c.sonido);
                libre->pos = 0;
                libre->volumen = c.volumen;
            }
        }

        const auto efecto = (Efecto)dueno->efecto.load(std::memory_order_relaxed);
        const bool pasarVoz = dueno->vozActiva.load(std::memory_order_relaxed);
        const bool oirVoz = dueno->escucharmeVoz.load(std::memory_order_relaxed);
        const bool oirSonidos = dueno->escucharSonidos.load(std::memory_order_relaxed);
        const float gVoz = dueno->volumenVoz.load(std::memory_order_relaxed);
        const float gSonidos = dueno->volumenSonidos.load(std::memory_order_relaxed);
        float pico = 0;
        int activos = 0;

        ma_uint32 hecho = 0;
        while (hecho < total)
        {
            const int n = (int)std::min<ma_uint32>(total - hecho, (ma_uint32)ProcesadorVoz::BloqueMaximo);

            // Tu voz, con el efecto
            if (entrada && (pasarVoz || oirVoz))
            {
                const float* in = entrada + hecho;
                for (int i = 0; i < n; i++) pico = std::max(pico, std::fabs(in[i]));
                voz.Procesar(efecto, in, bufVoz.data(), n);
            }
            else
            {
                if (entrada)
                {
                    for (int i = 0; i < n; i++) pico = std::max(pico, std::fabs(entrada[hecho + i]));
                }
                std::fill_n(bufVoz.data(), n, 0.0f);
            }

            // Los sonidos
            std::fill_n(bufSonidos.data(), (size_t)n * Canales, 0.0f);
            activos = 0;
            for (auto& r : sonando)
            {
                if (!r.sonido) continue;
                const Sonido& s = *r.sonido;
                const size_t m = std::min<size_t>(s.cuadros - r.pos, (size_t)n);
                const float* origen = s.muestras.data() + r.pos * Canales;
                const float v = r.volumen * gSonidos;
                for (size_t i = 0; i < m * Canales; i++) bufSonidos[i] += origen[i] * v;
                r.pos += m;
                if (r.pos >= s.cuadros) r.sonido.reset();
                else activos++;
            }

            // Al micrófono virtual: tu voz y los sonidos
            if (salida)
            {
                float* out = salida + (size_t)hecho * Canales;
                for (int i = 0; i < n; i++)
                {
                    const float v = pasarVoz ? bufVoz[i] * gVoz : 0.0f;
                    out[i * 2] = Limitar(v + bufSonidos[i * 2]);
                    out[i * 2 + 1] = Limitar(v + bufSonidos[i * 2 + 1]);
                }
            }

            // A tus audífonos
            if (anilloListo)
            {
                for (int i = 0; i < n; i++)
                {
                    const float v = oirVoz ? bufVoz[i] * gVoz : 0.0f;
                    bufEscucha[i * 2] = Limitar(v + (oirSonidos ? bufSonidos[i * 2] : 0.0f));
                    bufEscucha[i * 2 + 1] = Limitar(v + (oirSonidos ? bufSonidos[i * 2 + 1] : 0.0f));
                }
                EscribirAnillo(bufEscucha.data(), (ma_uint32)n);
            }
            hecho += (ma_uint32)n;
        }

        cuantosSuenan.store(activos, std::memory_order_relaxed);
        if (pico > nivel.load(std::memory_order_relaxed)) nivel.store(pico, std::memory_order_relaxed);
    }

    void Escuchar(float* salida, ma_uint32 n)
    {
        if (!anilloListo)
        {
            std::fill(salida, salida + (size_t)n * Canales, 0.0f);
            return;
        }
        // Si se juntó más de 80 ms, se salta lo viejo para no escuchar con retraso
        const ma_uint32 maximo = Frecuencia * 80 / 1000;
        const ma_uint32 disponible = ma_pcm_rb_available_read(&anillo);
        if (disponible > maximo + n) ma_pcm_rb_seek_read(&anillo, disponible - maximo);

        ma_uint32 hecho = 0;
        while (hecho < n)
        {
            ma_uint32 cuantos = n - hecho;
            void* origen = nullptr;
            if (ma_pcm_rb_acquire_read(&anillo, &cuantos, &origen) != MA_SUCCESS || cuantos == 0) break;
            std::memcpy(salida + (size_t)hecho * Canales, origen, (size_t)cuantos * Canales * sizeof(float));
            ma_pcm_rb_commit_read(&anillo, cuantos);
            hecho += cuantos;
        }
        if (hecho < n) std::fill(salida + (size_t)hecho * Canales, salida + (size_t)n * Canales, 0.0f);
    }
};

cha::MotorAudio::MotorAudio() : m(std::make_unique<Impl>())
{
    m->dueno = this;
}

cha::MotorAudio::~MotorAudio()
{
    Detener();
    if (m->contextoListo) ma_context_uninit(&m->contexto);
}

bool cha::MotorAudio::Preparar(std::wstring& error)
{
    if (m->contextoListo) return true;
    ma_backend backends[] = { ma_backend_wasapi };
    ma_context_config cc = ma_context_config_init();
    ma_result r = ma_context_init(backends, 1, &cc, &m->contexto);
    if (r != MA_SUCCESS)
    {
        error = L"No pude abrir el sonido de Windows (" + Ancho(ma_result_description(r)) + L").";
        Registrar(std::string("contexto: ") + ma_result_description(r));
        return false;
    }
    m->contextoListo = true;
    return true;
}

std::vector<std::wstring> cha::MotorAudio::Microfonos(std::wstring* predeterminado)
{
    return m->Nombres(true, predeterminado);
}

std::vector<std::wstring> cha::MotorAudio::Salidas(std::wstring* predeterminado)
{
    return m->Nombres(false, predeterminado);
}

bool cha::MotorAudio::EsMicrofonoVirtual(const std::wstring& nombreSalida)
{
    // VB-CABLE en un Windows en español se llama «Altavoces (VB-Audio Virtual Cable)»; en inglés, «CABLE Input (...)»
    const std::wstring n = Minusculas(nombreSalida);
    return n.find(L"vb-audio virtual cable") != std::wstring::npos || n.find(L"cable input") != std::wstring::npos
        || n.find(L"voicemeeter input") != std::wstring::npos || n.find(L"voicemeeter aux input") != std::wstring::npos
        || n.find(L"voicemeeter vaio3 input") != std::wstring::npos;
}

bool cha::MotorAudio::EsSalidaDeCable(const std::wstring& nombreEntrada)
{
    const std::wstring n = Minusculas(nombreEntrada);
    return n.find(L"vb-audio") != std::wstring::npos || n.find(L"cable output") != std::wstring::npos
        || n.find(L"voicemeeter out") != std::wstring::npos;
}

std::wstring cha::MotorAudio::NombreParaElegir(const std::wstring& nombreSalida)
{
    const std::wstring n = Minusculas(nombreSalida);
    if (n.find(L"vb-audio virtual cable") != std::wstring::npos || n.find(L"cable input") != std::wstring::npos)
        return L"CABLE Output";
    if (n.find(L"voicemeeter aux input") != std::wstring::npos) return L"Voicemeeter Out B2";
    if (n.find(L"voicemeeter vaio3 input") != std::wstring::npos) return L"Voicemeeter Out B3";
    if (n.find(L"voicemeeter input") != std::wstring::npos) return L"Voicemeeter Out B1";
    return L"la salida de tu micrófono virtual";
}

bool cha::MotorAudio::Arrancar(const std::wstring& microfono, const std::wstring& microfonoVirtual,
                               const std::wstring& escucharEn, std::wstring& error)
{
    Detener();
    if (!m->contextoListo)
    {
        error = L"El sonido de Windows no está listo.";
        return false;
    }

    ma_device_id idMic{}, idVirtual{}, idEscucha{};
    const bool hayMic = m->BuscarId(ma_device_type_capture, microfono, idMic);
    const bool hayVirtual = m->BuscarId(ma_device_type_playback, microfonoVirtual, idVirtual);
    const bool hayEscucha = m->BuscarId(ma_device_type_playback, escucharEn, idEscucha);
    if (!microfonoVirtual.empty() && !hayVirtual)
    {
        error = L"No encuentro el micrófono virtual «" + microfonoVirtual + L"».";
        return false;
    }

    // Lo que va a los audífonos pasa por un anillo de 250 ms
    if (ma_pcm_rb_init(ma_format_f32, Canales, Frecuencia / 4, nullptr, nullptr, &m->anillo) == MA_SUCCESS) m->anilloListo = true;

    auto Configurar = [&](ma_device_type tipo) {
        ma_device_config c = ma_device_config_init(tipo);
        c.sampleRate = Frecuencia;
        c.periodSizeInMilliseconds = 10;
        c.performanceProfile = ma_performance_profile_low_latency;
        c.capture.pDeviceID = hayMic ? &idMic : nullptr;
        c.capture.format = ma_format_f32;
        c.capture.channels = 1;
        c.capture.shareMode = ma_share_mode_shared;
        c.playback.pDeviceID = hayVirtual ? &idVirtual : nullptr;
        c.playback.format = ma_format_f32;
        c.playback.channels = Canales;
        c.playback.shareMode = ma_share_mode_shared;
        c.dataCallback = &Impl::AlAudio;
        c.pUserData = m.get();
        return c;
    };

    // Micrófono → efecto → micrófono virtual. Sin micrófono virtual, solo se toma el micrófono (para probar).
    m->principal = std::make_unique<ma_device>();
    ma_device_config c = Configurar(hayVirtual ? ma_device_type_duplex : ma_device_type_capture);
    ma_result r = ma_device_init(&m->contexto, &c, m->principal.get());
    if (r != MA_SUCCESS && hayVirtual)
    {
        // Sin micrófono que se pueda abrir: al menos los sonidos llegan al micrófono virtual
        Registrar(std::string("microfono: ") + ma_result_description(r));
        c = Configurar(ma_device_type_playback);
        r = ma_device_init(&m->contexto, &c, m->principal.get());
        m->sinMicrofono = r == MA_SUCCESS;
    }
    if (r != MA_SUCCESS)
    {
        m->principal.reset();
        error = L"No pude abrir el micrófono (" + Ancho(ma_result_description(r)) + L").";
        Registrar(std::string("principal: ") + ma_result_description(r));
        Detener();
        return false;
    }

    // Tus audífonos (si fallan, lo demás sigue igual)
    ma_device_config e = ma_device_config_init(ma_device_type_playback);
    e.sampleRate = Frecuencia;
    e.periodSizeInMilliseconds = 10;
    e.performanceProfile = ma_performance_profile_low_latency;
    e.playback.pDeviceID = hayEscucha ? &idEscucha : nullptr;
    e.playback.format = ma_format_f32;
    e.playback.channels = Canales;
    e.dataCallback = &Impl::AlEscuchar;
    e.pUserData = m.get();
    m->escucha = std::make_unique<ma_device>();
    if (ma_device_init(&m->contexto, &e, m->escucha.get()) != MA_SUCCESS)
    {
        m->escucha.reset();
        Registrar("no pude abrir la salida para escuchar");
    }

    r = ma_device_start(m->principal.get());
    if (r != MA_SUCCESS)
    {
        error = L"No pude arrancar el sonido (" + Ancho(ma_result_description(r)) + L").";
        Registrar(std::string("arrancar: ") + ma_result_description(r));
        Detener();
        return false;
    }
    if (m->escucha) ma_device_start(m->escucha.get());
    m->conVirtual = hayVirtual;
    Registrar("audio en marcha; micrófono virtual: " + (hayVirtual ? Utf8(microfonoVirtual) : std::string("ninguno")));
    return true;
}

void cha::MotorAudio::Detener()
{
    // ma_device_uninit para el hilo de audio y lo espera: después ya se puede tocar todo sin cuidado
    if (m->principal)
    {
        ma_device_uninit(m->principal.get());
        m->principal.reset();
    }
    if (m->escucha)
    {
        ma_device_uninit(m->escucha.get());
        m->escucha.reset();
    }
    if (m->anilloListo)
    {
        ma_pcm_rb_uninit(&m->anillo);
        m->anilloListo = false;
    }
    m->conVirtual = false;
    m->sinMicrofono = false;
    for (auto& r : m->sonando) r.sonido.reset();
    Comando c;
    while (m->cola.Sacar(c)) {}
    m->cuantosSuenan = 0;
}

bool cha::MotorAudio::Activo() const { return m->principal != nullptr; }
bool cha::MotorAudio::ConMicrofonoVirtual() const { return m->conVirtual; }
bool cha::MotorAudio::SinMicrofono() const { return m->sinMicrofono; }

void cha::MotorAudio::Tocar(std::shared_ptr<const Sonido> sonido, float volumen)
{
    if (!sonido || !m->principal) return;
    m->cola.Poner(Comando{ Orden::Tocar, std::move(sonido), volumen });
}

void cha::MotorAudio::DetenerSonidos()
{
    if (!m->principal) return;
    m->cola.Poner(Comando{ Orden::DetenerTodo, nullptr, 0.0f });
}

int cha::MotorAudio::SonidosSonando() const { return m->cuantosSuenan.load(std::memory_order_relaxed); }

float cha::MotorAudio::NivelMicrofono()
{
    float v = m->nivel.load(std::memory_order_relaxed);
    m->nivel.store(v * 0.8f, std::memory_order_relaxed);
    return std::min(v, 1.0f);
}
