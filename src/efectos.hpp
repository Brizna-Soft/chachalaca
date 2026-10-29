#pragma once

#include <memory>
#include <string>

namespace cha
{
    enum class Efecto : int
    {
        Normal, Grave, Aguda, Ardilla, Monstruo, Duende, Raton, Caricatura,
        Robot, Autotune, RobotAutotune, Eco, Radio, Cueva,
        Cantidad
    };

    const wchar_t* NombreEfecto(Efecto e);   // para la persona
    const char* ClaveEfecto(Efecto e);       // para guardar en los ajustes
    Efecto EfectoDesdeClave(const std::string& clave);

    // Cambia la voz en bloques de muestras mono (float). Se crea en el hilo de la ventana; Procesar() corre en el
    // hilo de audio y no pide memoria.
    class ProcesadorVoz
    {
    public:
        explicit ProcesadorVoz(int frecuencia);
        ~ProcesadorVoz();
        ProcesadorVoz(const ProcesadorVoz&) = delete;
        ProcesadorVoz& operator=(const ProcesadorVoz&) = delete;

        static constexpr int BloqueMaximo = 4096;

        // n <= BloqueMaximo. «entrada» y «salida» no deben ser el mismo arreglo.
        void Procesar(Efecto efecto, const float* entrada, float* salida, int n);

    private:
        struct Impl;
        std::unique_ptr<Impl> m;
    };
}
