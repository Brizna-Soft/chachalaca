#pragma once

#include <string>
#include <vector>

namespace cha
{
    constexpr size_t MaxSonidos = 200;

    struct SonidoGuardado
    {
        std::wstring ruta;
        std::wstring nombre;
        int atajoMods = 0;   // wxMOD_CONTROL, wxMOD_ALT, wxMOD_SHIFT
        int atajoTecla = 0;  // código de tecla de wx; 0 = sin atajo
        int volumen = 100;   // en %
    };

    // Ajustes en %LOCALAPPDATA%\Chachalaca\ajustes.json
    struct Ajustes
    {
        std::wstring microfono;         // vacío = el de Windows
        std::wstring microfonoVirtual;  // la salida que va al micrófono virtual (por ejemplo «CABLE Input»)
        std::wstring escucharEn;        // vacío = la salida de Windows
        std::string efecto = "normal";
        bool quitarRuido = true;
        bool soloVoz = false;
        bool sonidosDeFabrica = false;   // ya se agregaron una vez los sonidos que trae Chachalaca
        bool vozActiva = true;
        bool escucharmeVoz = false;
        bool escucharSonidos = true;
        int volumenVoz = 100;      // 0 a 200 %
        int volumenSonidos = 80;   // 0 a 100 %
        std::vector<SonidoGuardado> sonidos;

        int x = -1, y = -1, ancho = 0, alto = 0;
        bool maximizada = false;

        void Cargar();
        void Guardar() const;
    };
}
