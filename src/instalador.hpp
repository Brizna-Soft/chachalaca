#pragma once

// Instalar VB-CABLE (el micrófono virtual gratis de VB-Audio, vb-cable.com) desde la carpeta VB-CABLE que viene junto
// a Chachalaca.exe. VB-Audio permite incluirlo en programas gratis si se dice que es suyo y que es donationware.

#include <windows.h>

#include <string>

namespace cha
{
    // Ruta del instalador que viene con Chachalaca, o vacío si no está
    std::wstring InstaladorVBCable();

    // true si el archivo tiene una firma digital válida de VB-Audio (Vincent Burel)
    bool FirmaDeVBAudio(const std::wstring& ruta);

    // Abre el instalador con permiso de administrador y en modo silencioso. Devuelve el proceso para esperarlo
    // (hay que cerrarlo con CloseHandle), o nullptr con un texto para la persona.
    HANDLE AbrirInstaladorVBCable(HWND padre, const std::wstring& ruta, std::wstring& error);
}
