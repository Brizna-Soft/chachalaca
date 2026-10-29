#include "ajustes.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

#include "util.hpp"

namespace
{
    std::wstring Archivo() { return cha::CarpetaDatos() + L"\\ajustes.json"; }
}

void cha::Ajustes::Cargar()
{
    std::string texto;
    if (!LeerArchivo(Archivo(), texto)) return;
    try
    {
        auto j = nlohmann::json::parse(texto);
        microfono = Ancho(j.value("microfono", ""));
        microfonoVirtual = Ancho(j.value("microfonoVirtual", ""));
        escucharEn = Ancho(j.value("escucharEn", ""));
        efecto = j.value("efecto", "normal");
        quitarRuido = j.value("quitarRuido", true);
        soloVoz = j.value("soloVoz", false);
        sonidosDeFabrica = j.value("sonidosDeFabrica", false);
        vozActiva = j.value("vozActiva", true);
        escucharmeVoz = j.value("escucharmeVoz", false);
        escucharSonidos = j.value("escucharSonidos", true);
        volumenVoz = std::clamp(j.value("volumenVoz", 100), 0, 200);
        volumenSonidos = std::clamp(j.value("volumenSonidos", 80), 0, 100);
        sonidos.clear();
        if (j.contains("sonidos") && j["sonidos"].is_array())
        {
            for (const auto& s : j["sonidos"])
            {
                if (sonidos.size() >= MaxSonidos) break;
                if (!s.is_object()) continue;
                SonidoGuardado g;
                g.ruta = Ancho(s.value("ruta", ""));
                g.nombre = Ancho(s.value("nombre", ""));
                g.atajoMods = s.value("atajoMods", 0);
                g.atajoTecla = s.value("atajoTecla", 0);
                g.volumen = std::clamp(s.value("volumen", 100), 0, 200);
                if (g.nombre.size() > 80) g.nombre.resize(80);
                if (!g.ruta.empty() && g.ruta.size() < 2048) sonidos.push_back(g);
            }
        }
        x = j.value("x", -1);
        y = j.value("y", -1);
        ancho = j.value("ancho", 0);
        alto = j.value("alto", 0);
        maximizada = j.value("maximizada", false);
    }
    catch (...) {}
}

void cha::Ajustes::Guardar() const
{
    nlohmann::json lista = nlohmann::json::array();
    for (const auto& s : sonidos)
    {
        lista.push_back({ { "ruta", Utf8(s.ruta) }, { "nombre", Utf8(s.nombre) }, { "atajoMods", s.atajoMods },
                          { "atajoTecla", s.atajoTecla }, { "volumen", s.volumen } });
    }
    nlohmann::json j = {
        { "microfono", Utf8(microfono) }, { "microfonoVirtual", Utf8(microfonoVirtual) }, { "escucharEn", Utf8(escucharEn) },
        { "efecto", efecto }, { "quitarRuido", quitarRuido }, { "soloVoz", soloVoz }, { "sonidosDeFabrica", sonidosDeFabrica },
        { "vozActiva", vozActiva }, { "escucharmeVoz", escucharmeVoz },
        { "escucharSonidos", escucharSonidos }, { "volumenVoz", volumenVoz }, { "volumenSonidos", volumenSonidos },
        { "sonidos", lista },
        { "x", x }, { "y", y }, { "ancho", ancho }, { "alto", alto }, { "maximizada", maximizada },
    };
    EscribirSeguro(Archivo(), j.dump(1));
}
