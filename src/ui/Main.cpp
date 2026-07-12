// AAMP - src/ui/Main.cpp
// Casca nativa ÚNICA do produto: uma janela JUCE hospedando o frontend oficial
// (resources/aamp_laboratory.html) através da AampWebBridge (WebView2, in-process).
// Toda a UI de produto vive no HTML; este arquivo cuida apenas de janela, ciclo de
// vida e integração com o sistema (title bar, maximização, encerramento seguro).
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../bridge/AampWebBridge.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <dwmapi.h>
 #pragma comment(lib, "dwmapi.lib")
#endif

namespace aamp {

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow()
        : juce::DocumentWindow("AAMP Laboratory",
                               juce::Colour(0xFF06090C),        // chassi do tema (fundo da casca)
                               juce::DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);                            // comportamento nativo preservado
        m_bridge = new AampWebBridge();
        setContentOwned(m_bridge, true);
        setResizable(true, true);

        // Inicialização: toda a área útil disponível — janela MAXIMIZADA por padrão
        // (fullscreen exclusivo não é apropriado para um instrumento de bancada).
        if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            setBounds(disp->userArea.reduced(60));               // fallback restaurado coerente
        setVisible(true);
        setFullScreen(true);                                     // DocumentWindow: maximiza (nativo)

        applyDarkTitleBar();                                     // title bar integrada ao tema
    }

    // Encerramento seguro conduzido pelo PRODUTO: o frontend decide (exportar/descartar/
    // cancelar) via modal próprio e confirma com o comando closeConfirm. Se a página nunca
    // respondeu à ponte (WebView indisponível), fecha diretamente — nunca prende o usuário.
    void closeButtonPressed() override {
        if (m_bridge != nullptr && m_bridge->requestCloseFromHost())
            return;                                              // aguardando decisão do produto
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    // Integra a title bar nativa ao tema escuro do AAMP (Windows 10 20H1+ / Windows 11).
    // Melhor esforço: em sistemas sem suporte o atributo é ignorado e a barra permanece nativa.
    void applyDarkTitleBar() {
       #if JUCE_WINDOWS
        if (auto* peer = getPeer()) {
            if (auto hwnd = (HWND) peer->getNativeHandle()) {
                BOOL dark = TRUE;
                // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (19 em builds 1809-1909)
                if (FAILED(::DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
                    ::DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
                const COLORREF caption = 0x000C0906;             // BGR de #06090C (Win11: cor da barra)
                ::DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
            }
        }
       #endif
    }

    AampWebBridge* m_bridge = nullptr;                           // owned por setContentOwned
};

class AampApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override    { return "AAMP"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }
    void initialise(const juce::String&) override        { m_window = std::make_unique<MainWindow>(); }
    void shutdown() override                             { m_window = nullptr; }
    void systemRequestedQuit() override                  { quit(); }
private:
    std::unique_ptr<MainWindow> m_window;
};

} // namespace aamp

START_JUCE_APPLICATION(aamp::AampApplication)
#endif // AAMP_WITH_JUCE
