#pragma once

#include <memory>

namespace cha
{
    // Limpia tu voz antes del efecto: quita el ruido de fondo (SpeexDSP) y, si se pide, calla el micrófono cuando no
    // hablas. Trabaja en cuadros de 10 ms, así que agrega 10 ms de retraso. Procesar() no pide memoria.
    class LimpiadorVoz
    {
    public:
        explicit LimpiadorVoz(int frecuencia);
        ~LimpiadorVoz();
        LimpiadorVoz(const LimpiadorVoz&) = delete;
        LimpiadorVoz& operator=(const LimpiadorVoz&) = delete;

        static constexpr int BloqueMaximo = 4096;

        // n <= BloqueMaximo. «entrada» y «salida» no deben ser el mismo arreglo.
        void Procesar(const float* entrada, float* salida, int n, bool quitarRuido, bool soloVoz);

    private:
        struct Impl;
        std::unique_ptr<Impl> m;
    };
}
