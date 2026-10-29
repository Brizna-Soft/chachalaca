#pragma once

// Instalar VB-CABLE (el micrófono virtual gratis de VB-Audio, vb-cable.com) desde la carpeta VB-CABLE que viene junto
// a Chachalaca.exe. VB-Audio permite incluirlo en programas gratis si se dice que es suyo y que es donationware.

#include <windows.h>

#include <string>

namespace cha
{
    // Ruta del instalador que viene con Chachalaca, o vacío si no está
    std::wstring InstaladorVBCable();

    // Abre el instalador sin dejar que nadie lo cambie mientras tanto y revisa que sea exactamente el de VB-Audio:
    // la huella SHA-256 fijada para el paquete que trae Chachalaca y una firma digital válida de VB-Audio.
    // Devuelve el archivo abierto (hay que cerrarlo con CloseHandle después de abrir el instalador), o
    // INVALID_HANDLE_VALUE con un texto para la persona.
    HANDLE RevisarInstaladorVBCable(const std::wstring& ruta, std::wstring& error);

    // Abre el instalador con permiso de administrador y en modo silencioso. Devuelve el proceso para esperarlo
    // (hay que cerrarlo con CloseHandle), o nullptr con un texto para la persona.
    HANDLE AbrirInstaladorVBCable(HWND padre, const std::wstring& ruta, std::wstring& error);
}
