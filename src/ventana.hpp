#pragma once

#include <wx/arrstr.h>
#include <wx/frame.h>
#include <wx/timer.h>

#include <memory>
#include <string>
#include <vector>

#include "ajustes.hpp"
#include "audio.hpp"

class wxButton;
class wxCheckBox;
class wxChoice;
class wxGauge;
class wxPanel;
class wxScrolledWindow;
class wxSizer;
class wxSlider;
class wxStaticText;
class wxToggleButton;

class VentanaPrincipal : public wxFrame
{
public:
    VentanaPrincipal(cha::MotorAudio& motor, cha::Ajustes& ajustes);
    void AgregarArchivos(const wxArrayString& archivos);
    // Los sonidos que trae Chachalaca (carpeta «sonidos» junto al programa); se saltan los que ya están
    void AgregarSonidosDeFabrica();

private:
    struct SonidoEnTablero
    {
        cha::SonidoGuardado datos;
        std::shared_ptr<const cha::Sonido> audio;
        std::wstring error;
    };

    cha::MotorAudio& m_motor;
    cha::Ajustes& m_ajustes;
    std::vector<SonidoEnTablero> m_sonidos;
    // Los quitados se guardan hasta cerrar: si todavía sonaban, el hilo de audio no tiene que liberar memoria
    std::vector<std::shared_ptr<const cha::Sonido>> m_retirados;
    wxTimer m_reloj;
    int m_atajosRegistrados = 0;
    bool m_atajoDetenerOk = false;

    wxPanel* m_panel = nullptr;
    wxStaticText* m_estado = nullptr;
    wxButton* m_comoInstalar = nullptr;
    wxButton* m_arreglarWindows = nullptr;
    std::wstring m_micWindows, m_salidaWindows;   // los que usa Windows ahora
    wxChoice* m_eligeMic = nullptr;
    wxChoice* m_eligeVirtual = nullptr;
    wxChoice* m_eligeEscucha = nullptr;
    std::vector<std::wstring> m_mics, m_virtuales, m_escuchas;   // en el mismo orden que las listas (sin la opción 0)
    wxCheckBox* m_quitarRuido = nullptr;
    wxCheckBox* m_soloVoz = nullptr;
    wxCheckBox* m_pasarVoz = nullptr;
    wxCheckBox* m_escucharme = nullptr;
    wxCheckBox* m_escucharSonidos = nullptr;
    wxSlider* m_volVoz = nullptr;
    wxSlider* m_volSonidos = nullptr;
    wxGauge* m_nivel = nullptr;
    std::vector<wxToggleButton*> m_efectos;
    wxScrolledWindow* m_tablero = nullptr;
    wxSizer* m_tableroSizer = nullptr;

    void CrearControles();
    void LlenarDispositivos();
    void ArrancarAudio();
    void MostrarEstado(const wxString& texto, bool bien, bool faltaVirtual);
    void InstalarMicrofonoVirtual();
    void ElegirEfecto(cha::Efecto e);
    void RehacerTablero();
    void Tocar(size_t i);
    void MenuSonido(size_t i);
    void CambiarAtajo(size_t i);
    void RegistrarAtajos();
    void QuitarAtajos();
    void Guardar();
    void AlCerrar(wxCloseEvent& e);
    void AlAtajo(wxKeyEvent& e);
    void AlReloj(wxTimerEvent& e);
};
