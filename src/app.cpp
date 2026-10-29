// Chachalaca: sonidos y cambio de voz en tu micrófono, gratis, de Brizna Soft.

#include <windows.h>

#include <wx/app.h>
#include <wx/snglinst.h>
#include <wx/utils.h>

#include <memory>

#include "ajustes.hpp"
#include "audio.hpp"
#include "ayuda.hpp"
#include "util.hpp"
#include "ventana.hpp"
#include "version.h"

class Aplicacion : public wxApp
{
public:
    bool OnInit() override;
    int OnExit() override;

private:
    std::unique_ptr<wxSingleInstanceChecker> m_unica;
    std::unique_ptr<cha::MotorAudio> m_motor;
    cha::Ajustes m_ajustes;
};

wxIMPLEMENT_APP(Aplicacion);

bool Aplicacion::OnInit()
{
    // Una sola copia: si ya está abierto, se trae al frente
    m_unica = std::make_unique<wxSingleInstanceChecker>(L"Chachalaca-" + wxGetUserId());
    if (m_unica->IsAnotherRunning())
    {
        if (HWND otra = FindWindowW(nullptr, L"Chachalaca"))
        {
            ShowWindow(otra, SW_RESTORE);
            SetForegroundWindow(otra);
        }
        return false;
    }

    cha::Registrar(std::string("Chachalaca ") + APP_VERSION_TEXTO);
    MSWEnableDarkMode();   // claro u oscuro, como Windows
    SetAppDisplayName(L"Chachalaca");

    m_ajustes.Cargar();
    m_motor = std::make_unique<cha::MotorAudio>();
    std::wstring error;
    if (!m_motor->Preparar(error))
    {
        ayuda::Aviso(error, L"Chachalaca", wxOK | wxICON_ERROR);
        return false;
    }
    auto* ventana = new VentanaPrincipal(*m_motor, m_ajustes);
    ventana->Show();
    return true;
}

int Aplicacion::OnExit()
{
    m_motor.reset();
    return wxApp::OnExit();
}
