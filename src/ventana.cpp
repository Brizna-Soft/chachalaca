#include "ventana.hpp"

#include <windows.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/gauge.h>
#include <wx/iconbndl.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>
#include <wx/tglbtn.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <cmath>

#include "ayuda.hpp"
#include "util.hpp"
#include "version.h"

namespace
{
    constexpr int IdAtajoDetener = 0x0FFF;   // la tecla Pausa detiene todos los sonidos
    constexpr int IdAtajoPrimero = 0x1000;   // un atajo por sonido: 0x1000 + número del sonido
    const wchar_t* PaginaCable = L"https://vb-audio.com/Cable/";

    const wchar_t* Descripcion =
        L"Chachalaca pone sonidos en tu micrófono y te cambia la voz, para Discord y para los juegos "
        L"con chat de voz.\n\nPara que tus amigos te escuchen necesita un micrófono virtual, por ejemplo "
        L"VB-CABLE, que es gratis y lo hace la empresa VB-Audio.";

    wxString TextoAtajo(int mods, int tecla)
    {
        if (tecla == 0) return L"";
        wxString t;
        if (mods & wxMOD_CONTROL) t += L"Ctrl+";
        if (mods & wxMOD_ALT) t += L"Alt+";
        if (mods & wxMOD_SHIFT) t += L"Mayús+";
        if (tecla >= WXK_F1 && tecla <= WXK_F24) t += wxString::Format(L"F%d", tecla - WXK_F1 + 1);
        else if (tecla >= WXK_NUMPAD0 && tecla <= WXK_NUMPAD9) t += wxString::Format(L"Num %d", tecla - WXK_NUMPAD0);
        else if (tecla == WXK_PAUSE) t += L"Pausa";
        else t += wxString((wxChar)tecla);
        return t;
    }

    // Código de tecla de Windows para RegisterHotKey (0 si no se puede usar)
    UINT TeclaWindows(int tecla)
    {
        if (tecla >= WXK_F1 && tecla <= WXK_F24) return VK_F1 + (UINT)(tecla - WXK_F1);
        if (tecla >= WXK_NUMPAD0 && tecla <= WXK_NUMPAD9) return VK_NUMPAD0 + (UINT)(tecla - WXK_NUMPAD0);
        if (tecla == WXK_PAUSE) return VK_PAUSE;
        if ((tecla >= 'A' && tecla <= 'Z') || (tecla >= '0' && tecla <= '9')) return (UINT)tecla;
        return 0;
    }

    UINT ModsWindows(int mods)
    {
        UINT r = MOD_NOREPEAT;
        if (mods & wxMOD_CONTROL) r |= MOD_CONTROL;
        if (mods & wxMOD_ALT) r |= MOD_ALT;
        if (mods & wxMOD_SHIFT) r |= MOD_SHIFT;
        return r;
    }

    // Pide una combinación de teclas
    class DialogoAtajo : public wxDialog
    {
    public:
        explicit DialogoAtajo(wxWindow* padre) : wxDialog(padre, wxID_ANY, L"Atajo de teclado")
        {
            auto* caja = new wxBoxSizer(wxVERTICAL);
            auto* ayuda = new wxStaticText(this, wxID_ANY,
                L"Pulsa la combinación que quieres usar. Funciona aunque estés dentro de un juego.\n\n"
                L"Puede ser Ctrl o Alt con una letra o un número (por ejemplo Ctrl+Alt+1),\n"
                L"una tecla F (F1 a F24) o un número del teclado numérico.\n\nEsc para cancelar.");
            caja->Add(ayuda, 0, wxALL, FromDIP(16));
            m_texto = new wxStaticText(this, wxID_ANY, L"...", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER_HORIZONTAL);
            m_texto->SetFont(m_texto->GetFont().Scaled(1.6f).Bold());
            caja->Add(m_texto, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(16));
            caja->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, FromDIP(16));
            if (auto* b = FindWindow(wxID_OK))
            {
                b->SetLabel(L"Usar este atajo");
                b->Enable(false);
            }
            if (auto* b = FindWindow(wxID_CANCEL)) b->SetLabel(L"Cancelar");
            SetSizerAndFit(caja);
            CentreOnParent();
            Bind(wxEVT_CHAR_HOOK, &DialogoAtajo::AlTecla, this);
        }

        int mods = 0, tecla = 0;

    private:
        wxStaticText* m_texto = nullptr;

        void AlTecla(wxKeyEvent& e)
        {
            const int k = e.GetKeyCode();
            if (k == WXK_ESCAPE)
            {
                EndModal(wxID_CANCEL);
                return;
            }
            if (k == WXK_RETURN || k == WXK_NUMPAD_ENTER)
            {
                if (tecla) EndModal(wxID_OK);
                return;
            }
            if (k == WXK_CONTROL || k == WXK_SHIFT || k == WXK_ALT || k == WXK_WINDOWS_LEFT || k == WXK_WINDOWS_RIGHT
                || k == WXK_RAW_CONTROL)
                return;   // solo un modificador: hay que esperar la tecla

            const int md = e.GetModifiers() & (wxMOD_CONTROL | wxMOD_ALT | wxMOD_SHIFT);
            const bool funcion = k >= WXK_F1 && k <= WXK_F24;
            const bool numerico = k >= WXK_NUMPAD0 && k <= WXK_NUMPAD9;
            const bool letraNumero = (k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9');
            if (!(funcion || numerico || (letraNumero && (md & (wxMOD_CONTROL | wxMOD_ALT)))))
            {
                m_texto->SetLabel(L"Usa Ctrl o Alt con una letra o un número, o una tecla F");
                Layout();
                return;
            }
            mods = md;
            tecla = k;
            m_texto->SetLabel(TextoAtajo(mods, tecla));
            if (auto* b = FindWindow(wxID_OK)) b->Enable(true);
            Layout();
        }
    };

    // Soltar archivos de sonido sobre la ventana los agrega
    class SoltarSonidos : public wxFileDropTarget
    {
    public:
        explicit SoltarSonidos(VentanaPrincipal* v) : m_ventana(v) {}
        bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& archivos) override
        {
            m_ventana->AgregarArchivos(archivos);
            return true;
        }

    private:
        VentanaPrincipal* m_ventana;
    };
}

VentanaPrincipal::VentanaPrincipal(cha::MotorAudio& motor, cha::Ajustes& ajustes)
    : wxFrame(nullptr, wxID_ANY, L"Chachalaca"), m_motor(motor), m_ajustes(ajustes), m_reloj(this)
{
    SetIcons(wxIconBundle(L"AAAA_APP", nullptr));

    // Menú
    auto* barra = new wxMenuBar();
    auto* archivo = new wxMenu();
    int idAgregar = wxWindow::NewControlId();
    archivo->Append(idAgregar, L"&Agregar sonidos...\tCtrl+O");
    archivo->AppendSeparator();
    archivo->Append(wxID_EXIT, L"&Salir");
    barra->Append(archivo, L"&Archivo");
    barra->Append(ayuda::CrearMenu(this, Descripcion), L"A&yuda");
    SetMenuBar(barra);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxFileDialog d(this, L"Elige tus sonidos", L"", L"",
                       L"Sonidos (*.wav;*.mp3;*.flac)|*.wav;*.mp3;*.flac|Todos los archivos (*.*)|*.*",
                       wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (d.ShowModal() != wxID_OK) return;
        wxArrayString rutas;
        d.GetPaths(rutas);
        AgregarArchivos(rutas);
    }, idAgregar);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);

    CreateStatusBar();
    CrearControles();

    // Los sonidos guardados
    for (const auto& g : m_ajustes.sonidos)
    {
        SonidoEnTablero s;
        s.datos = g;
        s.audio = cha::CargarSonido(g.ruta, s.error);
        m_sonidos.push_back(std::move(s));
    }
    RehacerTablero();

    // Tamaño y posición de la última vez (si sigue dentro de una pantalla)
    SetMinSize(FromDIP(wxSize(640, 560)));
    wxSize tamano = FromDIP(wxSize(820, 720));
    if (m_ajustes.ancho > 300 && m_ajustes.alto > 300) tamano = wxSize(m_ajustes.ancho, m_ajustes.alto);
    SetSize(tamano);
    if (m_ajustes.x != -1 && wxDisplay::GetFromPoint(wxPoint(m_ajustes.x + 40, m_ajustes.y + 20)) != wxNOT_FOUND)
        Move(m_ajustes.x, m_ajustes.y);
    else
        Centre();
    if (m_ajustes.maximizada) Maximize();

    Bind(wxEVT_CLOSE_WINDOW, &VentanaPrincipal::AlCerrar, this);
    Bind(wxEVT_HOTKEY, &VentanaPrincipal::AlAtajo, this);
    Bind(wxEVT_TIMER, &VentanaPrincipal::AlReloj, this);
    SetDropTarget(new SoltarSonidos(this));

    LlenarDispositivos();
    ArrancarAudio();
    RegistrarAtajos();
    m_reloj.Start(100);
}

void VentanaPrincipal::CrearControles()
{
    m_panel = new wxPanel(this);
    auto* caja = new wxBoxSizer(wxVERTICAL);
    const int borde = FromDIP(10);

    // Aviso del micrófono virtual
    auto* filaEstado = new wxBoxSizer(wxHORIZONTAL);
    m_estado = new wxStaticText(m_panel, wxID_ANY, L"");
    m_estado->SetFont(m_estado->GetFont().Bold());
    filaEstado->Add(m_estado, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, borde);
    m_comoInstalar = new wxButton(m_panel, wxID_ANY, L"Cómo instalar el micrófono virtual");
    m_comoInstalar->Bind(wxEVT_BUTTON, [](wxCommandEvent&) { ayuda::AbrirEnlace(PaginaCable); });
    filaEstado->Add(m_comoInstalar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    auto* buscar = new wxButton(m_panel, wxID_ANY, L"Buscar de nuevo");
    buscar->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        LlenarDispositivos();
        ArrancarAudio();
    });
    filaEstado->Add(buscar, 0, wxALIGN_CENTER_VERTICAL);
    caja->Add(filaEstado, 0, wxEXPAND | wxALL, borde);

    // Dispositivos
    auto* grupoDisp = new wxStaticBoxSizer(wxVERTICAL, m_panel, L"Dispositivos");
    wxWindow* cajaDisp = grupoDisp->GetStaticBox();
    auto* rejilla = new wxFlexGridSizer(2, FromDIP(6), FromDIP(10));
    rejilla->AddGrowableCol(1, 1);
    auto Fila = [&](const wxString& etiqueta, wxChoice*& choice) {
        rejilla->Add(new wxStaticText(cajaDisp, wxID_ANY, etiqueta), 0, wxALIGN_CENTER_VERTICAL);
        choice = new wxChoice(cajaDisp, wxID_ANY);
        rejilla->Add(choice, 1, wxEXPAND);
        choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            ArrancarAudio();
            Guardar();
        });
    };
    Fila(L"Tu micrófono:", m_eligeMic);
    Fila(L"Micrófono virtual (para tus amigos):", m_eligeVirtual);
    Fila(L"Escuchar en:", m_eligeEscucha);
    grupoDisp->Add(rejilla, 0, wxEXPAND | wxALL, FromDIP(6));
    caja->Add(grupoDisp, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, borde);

    // Tu voz
    auto* grupoVoz = new wxStaticBoxSizer(wxVERTICAL, m_panel, L"Tu voz");
    wxWindow* cajaVoz = grupoVoz->GetStaticBox();
    auto* filaVoz = new wxBoxSizer(wxHORIZONTAL);
    m_pasarVoz = new wxCheckBox(cajaVoz, wxID_ANY, L"Mandar mi voz");
    m_pasarVoz->SetValue(m_ajustes.vozActiva);
    m_pasarVoz->SetToolTip(L"Apágalo para que tus amigos solo escuchen los sonidos");
    m_pasarVoz->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        m_motor.vozActiva = m_pasarVoz->GetValue();
        Guardar();
    });
    filaVoz->Add(m_pasarVoz, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(14));
    m_escucharme = new wxCheckBox(cajaVoz, wxID_ANY, L"Escucharme");
    m_escucharme->SetValue(m_ajustes.escucharmeVoz);
    m_escucharme->SetToolTip(L"Para oír cómo suena tu voz cambiada. Mejor con audífonos, para que no haga eco.");
    m_escucharme->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        m_motor.escucharmeVoz = m_escucharme->GetValue();
        Guardar();
    });
    filaVoz->Add(m_escucharme, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(14));
    filaVoz->Add(new wxStaticText(cajaVoz, wxID_ANY, L"Volumen:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    m_volVoz = new wxSlider(cajaVoz, wxID_ANY, m_ajustes.volumenVoz, 0, 200, wxDefaultPosition, FromDIP(wxSize(140, -1)));
    m_volVoz->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
        m_motor.volumenVoz = m_volVoz->GetValue() / 100.0f;
        Guardar();
    });
    filaVoz->Add(m_volVoz, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    m_nivel = new wxGauge(cajaVoz, wxID_ANY, 100, wxDefaultPosition, FromDIP(wxSize(120, 12)));
    m_nivel->SetToolTip(L"Cuánto suena tu micrófono");
    filaVoz->Add(m_nivel, 1, wxALIGN_CENTER_VERTICAL);
    grupoVoz->Add(filaVoz, 0, wxEXPAND | wxALL, FromDIP(6));

    auto* filaEfectos = new wxWrapSizer(wxHORIZONTAL);
    for (int i = 0; i < (int)cha::Efecto::Cantidad; i++)
    {
        auto e = (cha::Efecto)i;
        auto* b = new wxToggleButton(cajaVoz, wxID_ANY, cha::NombreEfecto(e), wxDefaultPosition, FromDIP(wxSize(96, 34)));
        b->Bind(wxEVT_TOGGLEBUTTON, [this, e](wxCommandEvent&) { ElegirEfecto(e); });
        filaEfectos->Add(b, 0, wxALL, FromDIP(3));
        m_efectos.push_back(b);
    }
    grupoVoz->Add(filaEfectos, 0, wxEXPAND | wxALL, FromDIP(3));
    caja->Add(grupoVoz, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, borde);

    // Sonidos
    auto* grupoSon = new wxStaticBoxSizer(wxVERTICAL, m_panel, L"Sonidos");
    wxWindow* cajaSon = grupoSon->GetStaticBox();
    auto* filaSon = new wxBoxSizer(wxHORIZONTAL);
    auto* agregar = new wxButton(cajaSon, wxID_ANY, L"Agregar sonidos...");
    agregar->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxFileDialog d(this, L"Elige tus sonidos", L"", L"",
                       L"Sonidos (*.wav;*.mp3;*.flac)|*.wav;*.mp3;*.flac|Todos los archivos (*.*)|*.*",
                       wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (d.ShowModal() != wxID_OK) return;
        wxArrayString rutas;
        d.GetPaths(rutas);
        AgregarArchivos(rutas);
    });
    filaSon->Add(agregar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    auto* detener = new wxButton(cajaSon, wxID_ANY, L"Detener todos (Pausa)");
    detener->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_motor.DetenerSonidos(); });
    filaSon->Add(detener, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(14));
    m_escucharSonidos = new wxCheckBox(cajaSon, wxID_ANY, L"Escuchar los sonidos");
    m_escucharSonidos->SetValue(m_ajustes.escucharSonidos);
    m_escucharSonidos->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        m_motor.escucharSonidos = m_escucharSonidos->GetValue();
        Guardar();
    });
    filaSon->Add(m_escucharSonidos, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(14));
    filaSon->Add(new wxStaticText(cajaSon, wxID_ANY, L"Volumen:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    m_volSonidos = new wxSlider(cajaSon, wxID_ANY, m_ajustes.volumenSonidos, 0, 100, wxDefaultPosition, FromDIP(wxSize(140, -1)));
    m_volSonidos->Bind(wxEVT_SLIDER, [this](wxCommandEvent&) {
        m_motor.volumenSonidos = m_volSonidos->GetValue() / 100.0f;
        Guardar();
    });
    filaSon->Add(m_volSonidos, 0, wxALIGN_CENTER_VERTICAL);
    grupoSon->Add(filaSon, 0, wxEXPAND | wxALL, FromDIP(6));

    m_tablero = new wxScrolledWindow(cajaSon, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_tablero->SetScrollRate(0, FromDIP(10));
    m_tableroSizer = new wxWrapSizer(wxHORIZONTAL);
    m_tablero->SetSizer(m_tableroSizer);
    grupoSon->Add(m_tablero, 1, wxEXPAND | wxALL, FromDIP(3));
    caja->Add(grupoSon, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, borde);

    m_panel->SetSizer(caja);

    // Valores iniciales para el motor
    m_motor.vozActiva = m_ajustes.vozActiva;
    m_motor.escucharmeVoz = m_ajustes.escucharmeVoz;
    m_motor.escucharSonidos = m_ajustes.escucharSonidos;
    m_motor.volumenVoz = m_ajustes.volumenVoz / 100.0f;
    m_motor.volumenSonidos = m_ajustes.volumenSonidos / 100.0f;
    ElegirEfecto(cha::EfectoDesdeClave(m_ajustes.efecto));
}

void VentanaPrincipal::LlenarDispositivos()
{
    std::wstring micWindows, salidaWindows;
    m_mics = m_motor.Microfonos(&micWindows);
    std::vector<std::wstring> salidas = m_motor.Salidas(&salidaWindows);
    m_virtuales.clear();
    m_escuchas.clear();
    for (const auto& s : salidas)
    {
        if (cha::MotorAudio::EsMicrofonoVirtual(s)) m_virtuales.push_back(s);
        else m_escuchas.push_back(s);
    }

    m_eligeMic->Clear();
    m_eligeMic->Append(L"El de Windows" + (micWindows.empty() ? std::wstring() : L" (" + micWindows + L")"));
    for (const auto& n : m_mics) m_eligeMic->Append(n);
    auto mic = std::find(m_mics.begin(), m_mics.end(), m_ajustes.microfono);
    m_eligeMic->SetSelection(mic == m_mics.end() ? 0 : 1 + (int)(mic - m_mics.begin()));

    m_eligeVirtual->Clear();
    m_eligeVirtual->Append(L"Ninguno (solo probar aquí)");
    for (const auto& n : m_virtuales) m_eligeVirtual->Append(n);
    auto vir = std::find(m_virtuales.begin(), m_virtuales.end(), m_ajustes.microfonoVirtual);
    if (vir != m_virtuales.end()) m_eligeVirtual->SetSelection(1 + (int)(vir - m_virtuales.begin()));
    else m_eligeVirtual->SetSelection(m_virtuales.empty() ? 0 : 1);   // si hay uno instalado, se usa solo

    m_eligeEscucha->Clear();
    m_eligeEscucha->Append(L"La salida de Windows" + (salidaWindows.empty() || cha::MotorAudio::EsMicrofonoVirtual(salidaWindows)
                                                         ? std::wstring() : L" (" + salidaWindows + L")"));
    for (const auto& n : m_escuchas) m_eligeEscucha->Append(n);
    auto esc = std::find(m_escuchas.begin(), m_escuchas.end(), m_ajustes.escucharEn);
    m_eligeEscucha->SetSelection(esc == m_escuchas.end() ? 0 : 1 + (int)(esc - m_escuchas.begin()));
    m_panel->Layout();
}

void VentanaPrincipal::ArrancarAudio()
{
    int iMic = m_eligeMic->GetSelection(), iVir = m_eligeVirtual->GetSelection(), iEsc = m_eligeEscucha->GetSelection();
    std::wstring mic = iMic > 0 ? m_mics[(size_t)iMic - 1] : L"";
    std::wstring vir = iVir > 0 ? m_virtuales[(size_t)iVir - 1] : L"";
    std::wstring esc = iEsc > 0 ? m_escuchas[(size_t)iEsc - 1] : L"";
    m_ajustes.microfono = mic;
    m_ajustes.microfonoVirtual = vir;
    m_ajustes.escucharEn = esc;

    std::wstring error;
    if (!m_motor.Arrancar(mic, vir, esc, error))
    {
        MostrarEstado(error, false, m_virtuales.empty());
        return;
    }
    if (m_virtuales.empty())
    {
        MostrarEstado(L"Falta el micrófono virtual: instala VB-CABLE (gratis) y pulsa «Buscar de nuevo». "
                      L"Mientras tanto puedes probar tu voz y tus sonidos aquí.", false, true);
    }
    else if (vir.empty())
    {
        MostrarEstado(L"Modo de prueba: solo tú escuchas. Elige el micrófono virtual para que tus amigos te oigan.", false, false);
    }
    else
    {
        wxString texto = L"Listo. En Discord o en tu juego, elige «" + cha::MotorAudio::NombreParaElegir(vir) + L"» como micrófono.";
        if (m_motor.SinMicrofono()) texto = L"No pude abrir tu micrófono, pero los sonidos sí llegan a tus amigos.";
        MostrarEstado(texto, !m_motor.SinMicrofono(), false);
    }
}

void VentanaPrincipal::MostrarEstado(const wxString& texto, bool bien, bool faltaVirtual)
{
    m_estado->SetLabel(texto);
    m_estado->SetForegroundColour(bien ? wxColour(0x2E, 0x9E, 0x4F) : wxColour(0xC7, 0x6B, 0x00));
    m_comoInstalar->Show(faltaVirtual);
    m_estado->Wrap(-1);
    m_estado->Wrap(std::max(FromDIP(200), GetClientSize().GetWidth() - FromDIP(360)));
    m_panel->Layout();
}

void VentanaPrincipal::ElegirEfecto(cha::Efecto e)
{
    for (size_t i = 0; i < m_efectos.size(); i++) m_efectos[i]->SetValue((int)i == (int)e);
    m_motor.efecto = (int)e;
    m_ajustes.efecto = cha::ClaveEfecto(e);
    m_ajustes.Guardar();
    SetStatusText(wxString(L"Voz: ") + cha::NombreEfecto(e));
}

void VentanaPrincipal::AgregarArchivos(const wxArrayString& archivos)
{
    wxString errores;
    for (const auto& a : archivos)
    {
        SonidoEnTablero s;
        s.datos.ruta = a.ToStdWstring();
        s.datos.nombre = wxFileName(a).GetName().ToStdWstring();
        s.audio = cha::CargarSonido(s.datos.ruta, s.error);
        if (!s.audio)
        {
            errores += wxFileName(a).GetFullName() + L": " + s.error + L"\n";
            continue;
        }
        m_sonidos.push_back(std::move(s));
    }
    Guardar();
    RehacerTablero();
    RegistrarAtajos();
    if (!errores.empty()) ayuda::Aviso(L"Estos no se pudieron agregar:\n\n" + errores, L"Agregar sonidos", wxOK | wxICON_WARNING, this);
}

void VentanaPrincipal::RehacerTablero()
{
    m_tablero->Freeze();
    m_tableroSizer->Clear(true);
    if (m_sonidos.empty())
    {
        auto* vacio = new wxStaticText(m_tablero, wxID_ANY,
            L"Todavía no hay sonidos. Pulsa «Agregar sonidos...» o arrastra aquí archivos WAV, MP3 o FLAC.\n"
            L"Después tócalos con un clic o con un atajo de teclado (clic derecho en el sonido).");
        m_tableroSizer->Add(vacio, 0, wxALL, FromDIP(10));
    }
    for (size_t i = 0; i < m_sonidos.size(); i++)
    {
        const auto& s = m_sonidos[i];
        wxString etiqueta = s.datos.nombre;
        wxString atajo = TextoAtajo(s.datos.atajoMods, s.datos.atajoTecla);
        if (!atajo.empty()) etiqueta += L"\n" + atajo;
        if (!s.audio) etiqueta += L"\n(no se pudo abrir)";
        auto* b = new wxButton(m_tablero, wxID_ANY, etiqueta, wxDefaultPosition, FromDIP(wxSize(150, 58)));
        b->SetToolTip(s.audio ? wxString(L"Clic para tocar. Clic derecho para el atajo, el nombre y más.") : wxString(s.error));
        b->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) { Tocar(i); });
        b->Bind(wxEVT_CONTEXT_MENU, [this, i](wxContextMenuEvent&) { MenuSonido(i); });
        m_tableroSizer->Add(b, 0, wxALL, FromDIP(4));
    }
    m_tablero->FitInside();
    m_tablero->Layout();
    m_tablero->Thaw();
}

void VentanaPrincipal::Tocar(size_t i)
{
    if (i >= m_sonidos.size()) return;
    auto& s = m_sonidos[i];
    if (!s.audio)
    {
        ayuda::Aviso(s.error, L"Chachalaca", wxOK | wxICON_WARNING, this);
        return;
    }
    m_motor.Tocar(s.audio, s.datos.volumen / 100.0f);
}

void VentanaPrincipal::MenuSonido(size_t i)
{
    if (i >= m_sonidos.size()) return;
    wxMenu menu;
    const int idTocar = wxWindow::NewControlId(), idAtajo = wxWindow::NewControlId(), idSinAtajo = wxWindow::NewControlId(),
              idNombre = wxWindow::NewControlId(), idVolumen = wxWindow::NewControlId(), idQuitar = wxWindow::NewControlId();
    menu.Append(idTocar, L"Tocar");
    menu.AppendSeparator();
    menu.Append(idAtajo, L"Cambiar el atajo de teclado...");
    if (m_sonidos[i].datos.atajoTecla) menu.Append(idSinAtajo, L"Quitar el atajo");
    menu.Append(idNombre, L"Cambiar el nombre...");
    menu.Append(idVolumen, L"Volumen de este sonido...");
    menu.AppendSeparator();
    menu.Append(idQuitar, L"Quitar de la lista (el archivo no se borra)");

    // El tablero se rehace después: este menú viene del botón del sonido, que se borra al rehacerlo
    auto Rehacer = [this]() {
        CallAfter([this]() {
            RehacerTablero();
            RegistrarAtajos();
        });
    };

    const int elegido = GetPopupMenuSelectionFromUser(menu);
    if (elegido == idTocar) Tocar(i);
    else if (elegido == idAtajo)
    {
        CambiarAtajo(i);
        Rehacer();
    }
    else if (elegido == idSinAtajo)
    {
        m_sonidos[i].datos.atajoMods = m_sonidos[i].datos.atajoTecla = 0;
        Guardar();
        Rehacer();
    }
    else if (elegido == idNombre)
    {
        wxTextEntryDialog d(this, L"Nombre del sonido:", L"Cambiar el nombre", m_sonidos[i].datos.nombre);
        d.SetOKCancelLabels(L"Aceptar", L"Cancelar");
        if (d.ShowModal() == wxID_OK && !d.GetValue().Trim().empty())
        {
            m_sonidos[i].datos.nombre = d.GetValue().Trim().ToStdWstring();
            Guardar();
            Rehacer();
        }
    }
    else if (elegido == idVolumen)
    {
        wxTextEntryDialog d(this, L"Volumen de este sonido, de 0 a 200 % (100 es el normal):", L"Volumen",
                            wxString::Format(L"%d", m_sonidos[i].datos.volumen));
        d.SetOKCancelLabels(L"Aceptar", L"Cancelar");
        long v = 0;
        if (d.ShowModal() == wxID_OK && d.GetValue().Trim().ToLong(&v))
        {
            m_sonidos[i].datos.volumen = (int)std::clamp(v, 0L, 200L);
            Guardar();
        }
    }
    else if (elegido == idQuitar)
    {
        QuitarAtajos();   // los números de los atajos cambian
        m_sonidos.erase(m_sonidos.begin() + (ptrdiff_t)i);
        Guardar();
        Rehacer();
    }
}

void VentanaPrincipal::CambiarAtajo(size_t i)
{
    QuitarAtajos();   // para que el atajo de ahora no se dispare mientras se elige el nuevo
    DialogoAtajo d(this);
    if (d.ShowModal() == wxID_OK && d.tecla)
    {
        // Si otro sonido tenía ese mismo atajo, se le quita
        for (auto& s : m_sonidos)
        {
            if (s.datos.atajoTecla == d.tecla && s.datos.atajoMods == d.mods) s.datos.atajoMods = s.datos.atajoTecla = 0;
        }
        m_sonidos[i].datos.atajoMods = d.mods;
        m_sonidos[i].datos.atajoTecla = d.tecla;
        Guardar();
    }
    // Quien llama rehace el tablero y vuelve a registrar los atajos
}

void VentanaPrincipal::RegistrarAtajos()
{
    QuitarAtajos();
    HWND h = GetHWND();
    m_atajoDetenerOk = ::RegisterHotKey(h, IdAtajoDetener, MOD_NOREPEAT, VK_PAUSE) != 0;
    wxString ocupados;
    for (size_t i = 0; i < m_sonidos.size(); i++)
    {
        const auto& s = m_sonidos[i].datos;
        UINT tecla = TeclaWindows(s.atajoTecla);
        if (!tecla) continue;
        if (!::RegisterHotKey(h, IdAtajoPrimero + (int)i, ModsWindows(s.atajoMods), tecla))
            ocupados += (ocupados.empty() ? L"" : L", ") + TextoAtajo(s.atajoMods, s.atajoTecla);
    }
    m_atajosRegistrados = (int)m_sonidos.size();
    if (!ocupados.empty()) SetStatusText(L"Otro programa ya usa estos atajos: " + ocupados);
}

void VentanaPrincipal::QuitarAtajos()
{
    HWND h = GetHWND();
    ::UnregisterHotKey(h, IdAtajoDetener);
    for (int i = 0; i < m_atajosRegistrados; i++) ::UnregisterHotKey(h, IdAtajoPrimero + i);
    m_atajosRegistrados = 0;
}

void VentanaPrincipal::AlAtajo(wxKeyEvent& e)
{
    const int id = e.GetId();
    if (id == IdAtajoDetener) m_motor.DetenerSonidos();
    else if (id >= IdAtajoPrimero) Tocar((size_t)(id - IdAtajoPrimero));
}

void VentanaPrincipal::AlReloj(wxTimerEvent&)
{
    m_nivel->SetValue(std::clamp((int)(std::sqrt(m_motor.NivelMicrofono()) * 100), 0, 100));
    const int sonando = m_motor.SonidosSonando();
    static int antes = -1;
    if (sonando != antes)
    {
        antes = sonando;
        wxString voz = wxString(L"Voz: ") + cha::NombreEfecto((cha::Efecto)m_motor.efecto.load());
        SetStatusText(sonando > 0 ? voz + wxString::Format(L"   ·   Sonando: %d", sonando) : voz);
    }
}

void VentanaPrincipal::Guardar()
{
    m_ajustes.vozActiva = m_pasarVoz->GetValue();
    m_ajustes.escucharmeVoz = m_escucharme->GetValue();
    m_ajustes.escucharSonidos = m_escucharSonidos->GetValue();
    m_ajustes.volumenVoz = m_volVoz->GetValue();
    m_ajustes.volumenSonidos = m_volSonidos->GetValue();
    m_ajustes.sonidos.clear();
    for (const auto& s : m_sonidos) m_ajustes.sonidos.push_back(s.datos);
    m_ajustes.maximizada = IsMaximized();
    if (!IsMaximized() && !IsIconized())
    {
        wxRect r = GetRect();
        m_ajustes.x = r.x;
        m_ajustes.y = r.y;
        m_ajustes.ancho = r.width;
        m_ajustes.alto = r.height;
    }
    m_ajustes.Guardar();
}

void VentanaPrincipal::AlCerrar(wxCloseEvent&)
{
    m_reloj.Stop();
    Guardar();
    QuitarAtajos();
    m_motor.Detener();
    Destroy();
}
