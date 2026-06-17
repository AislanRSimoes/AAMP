// AAMP - src/ui/Main.cpp
// SCAFFOLDING DE MAQUINA-ALVO (M-06/L-01; requer JUCE; nao compilado/verificado headless).
// Entry-point da aplicacao: cabeia M-01 (hardware) -> M-08 (orquestrador) -> M-06 (dashboard).
// Tela principal SEM configuracao de hardware (FR-009): Audio Config vive em tela separada.
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../hardware/JuceDeviceBackend.h"
#include "../hardware/HardwareManager.h"
#include "../routing/RoutingManager.h"
#include "../orchestration/MeasurementOrchestrator.h"
#include "../dashboard/DashboardModel.h"
#include "../dashboard/DashboardComponent.h"

namespace aamp {

// Janela principal: um unico comando "Iniciar caracterizacao" (FR-037, DOC-00 filosofia).
class MainComponent : public juce::Component {
public:
    MainComponent() {
        addAndMakeVisible(m_dashboard);
        addAndMakeVisible(m_start);
        m_start.setButtonText("Iniciar caracterizacao");
        m_start.onClick = [this]{ runMeasurement(); };
    }
    void resized() override {
        auto r = getLocalBounds();
        m_start.setBounds(r.removeFromTop(48).reduced(8));
        m_dashboard.setBounds(r);
    }
private:
    void runMeasurement() {
        // 1) M-01: seleciona dispositivo (tela de Audio Config separada - FR-009).
        // 2) M-02: resolve roteamento mono/estereo automaticamente.
        // 3) M-08: runSuite() executa a suite inteira; progresso amigavel no overlay.
        // 4) M-06: dashboard.showResults(bundle) renderiza as 6 secoes (buildDashboardModel).
        // (fluxo real-time-safe: captura/analise fora do message thread).
    }
    DashboardComponent m_dashboard;
    juce::TextButton m_start;
};

class AampApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "AAMP"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    void initialise(const juce::String&) override {
        m_window = std::make_unique<juce::DocumentWindow>(
            "AAMP", juce::Colours::black, juce::DocumentWindow::allButtons);
        m_window->setContentOwned(new MainComponent(), true);
        m_window->centreWithSize(1280, 800);
        m_window->setVisible(true);
    }
    void shutdown() override { m_window = nullptr; }
private:
    std::unique_ptr<juce::DocumentWindow> m_window;
};

} // namespace aamp

START_JUCE_APPLICATION(aamp::AampApplication)
#endif // AAMP_WITH_JUCE
