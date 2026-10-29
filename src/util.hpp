#pragma once

// Utilidades sin wxWidgets.

#include <string>

namespace cha
{
    std::wstring Ancho(const std::string& utf8);
    std::string Utf8(const std::wstring& texto);

    std::wstring CarpetaDatos();   // %LOCALAPPDATA%\Chachalaca (la crea si no existe)
    bool ExisteArchivo(const std::wstring& ruta);

    // Escribe a un archivo temporal y lo cambia por el bueno: nunca deja el archivo a medias
    bool EscribirSeguro(const std::wstring& ruta, const std::string& contenido);
    bool LeerArchivo(const std::wstring& ruta, std::string& contenido);

    // Registro corto para ayudar a encontrar fallas (%LOCALAPPDATA%\Chachalaca\registro.txt).
    // Solo anota nombres de dispositivos y errores, nunca rutas de archivos.
    void Registrar(const std::string& texto);
}
