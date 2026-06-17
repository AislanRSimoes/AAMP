// AAMP - src/dashboard/DashboardComponent.h
// SCAFFOLDING DE MAQUINA-ALVO (M-06; requer JUCE GUI; nao verificado headless).
// Dashboard de 6 secoes (DOC-05/DOC-02 FR-028..031). M-06 APENAS renderiza o ResultBundle
// produzido por M-05 (DOC-08 §3.1): nao recalcula. Tema escuro, grade de 8 px, sem julgamento.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../common/ResultBundle.h"
#include "DashboardModel.h"
#include "../orchestration/MeasurementOrchestrator.h"

namespace aamp {

// Grafico interativo (FR-029): zoom/pan/crosshair/tooltip/ocultar-exibir curvas/marcadores/PNG.
class InteractiveGraph : public juce::Component {
public:
    void setCurves(const std::vector<Curve>&) { /* render + interacoes */ }
    void paint(juce::Graphics&) override {}
};

// Estados por componente (FR-031): vazio / carregando / erro / baixa-confiab. / N-A.
enum class ComponentState { Empty, Loading, Error, LowConfidence, NotApplicable, Normal };

class DashboardComponent : public juce::Component {
public:
    // Renderiza as 6 secoes a partir do bundle (Summary, Technical, Dynamic, Musical, Noise, Stereo).
    void showResults(const ResultBundle& bundle) { m_model = buildDashboardModel(bundle); repaint(); }
    // Overlay de progresso amigavel (FR-030/DOC-04 §6): sem detalhes tecnicos.
    void showProgress(const Progress& p) { (void)p; }
    void paint(juce::Graphics& g) override { (void)g; /* 6 secoes, tema escuro, grade 8 px */ }
    void resized() override {}
private:
    DashboardModel m_model;  // 6 secoes ja prontas (M-06 view-model, testado headless)
    // Settings -> Audio Configuration vive em tela separada (FR-009): a principal nao tem HW.
};

} // namespace aamp
#endif // AAMP_WITH_JUCE
