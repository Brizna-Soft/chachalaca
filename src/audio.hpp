#pragma once

// El motor de audio: toma tu micrófono, le cambia la voz, le suma los sonidos y lo manda al micrófono virtual
// (por ejemplo VB-CABLE). Lo mismo, sin tu voz si quieres, se escucha en tus audífonos.

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "efectos.hpp"

namespace cha
{
    constexpr int Frecuencia = 48000;

    // Un sonido ya decodificado: estéreo, float, 48 kHz
    struct Sonido
    {
        std::vector<float> muestras;   // intercaladas: izquierda, derecha
        size_t cuadros = 0;
    };

    // Carga WAV, MP3 o FLAC (hasta 10 minutos). Si falla devuelve nullptr y un texto para la persona.
    std::shared_ptr<const Sonido> CargarSonido(const std::wstring& ruta, std::wstring& error);

    class MotorAudio
    {
    public:
        MotorAudio();
        ~MotorAudio();
        MotorAudio(const MotorAudio&) = delete;
        MotorAudio& operator=(const MotorAudio&) = delete;

        bool Preparar(std::wstring& error);   // abre el sonido de Windows; una sola vez

        // Nombres de los dispositivos. «predeterminado» recibe el que usa Windows.
        std::vector<std::wstring> Microfonos(std::wstring* predeterminado = nullptr);
        std::vector<std::wstring> Salidas(std::wstring* predeterminado = nullptr);
        static bool EsMicrofonoVirtual(const std::wstring& nombreSalida);
        // Entradas que son la otra punta de un cable virtual («CABLE Output»): nunca sirven como tu micrófono
        static bool EsSalidaDeCable(const std::wstring& nombreEntrada);
        // Lo que hay que elegir en Discord para esa salida («CABLE Output» para «CABLE Input»)
        static std::wstring NombreParaElegir(const std::wstring& nombreSalida);

        // «microfono» vacío = el de Windows. «microfonoVirtual» vacío = sin micrófono virtual: solo se escucha aquí,
        // para probar. «escucharEn» vacío = la salida de Windows.
        bool Arrancar(const std::wstring& microfono, const std::wstring& microfonoVirtual, const std::wstring& escucharEn,
                      std::wstring& error);
        void Detener();
        bool Activo() const;
        bool ConMicrofonoVirtual() const;
        bool SinMicrofono() const;   // arrancó, pero sin poder abrir el micrófono (los sonidos sí van)

        // Controles: se pueden cambiar desde cualquier hilo
        std::atomic<int> efecto{ 0 };
        std::atomic<bool> quitarRuido{ false };   // quitar el ruido de fondo de tu micrófono
        std::atomic<bool> soloVoz{ false };       // callar el micrófono cuando no hablas
        std::atomic<bool> vozActiva{ true };
        std::atomic<bool> escucharmeVoz{ false };
        std::atomic<bool> escucharSonidos{ true };
        std::atomic<float> volumenVoz{ 1.0f };
        std::atomic<float> volumenSonidos{ 0.8f };

        void Tocar(std::shared_ptr<const Sonido> sonido, float volumen);
        void DetenerSonidos();
        int SonidosSonando() const;
        float NivelMicrofono();   // 0 a 1, para el medidor; baja poco a poco entre llamadas

        struct Impl;

    private:
        std::unique_ptr<Impl> m;
    };
}
