// AAMP - src/dashboard/DashboardComponent.h
// SCAFFOLDING DE MAQUINA-ALVO (M-06; requer JUCE GUI).
// Dashboard de 6 secoes (DOC-05/DOC-02 FR-028..031). M-06 APENAS renderiza o ResultBundle
// produzido por M-05 (DOC-08 3.1): nao recalcula. Tema escuro, grade de 8 px, sem julgamento.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../common/ResultBundle.h"
#include "DashboardModel.h"
#include "DashboardSections.h"
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
        DashboardComponent() { addAndMakeVisible(m_sections); }

        void showResults(const ResultBundle& bundle) {
            m_model = buildDashboardModel(bundle);
            m_sections.setModel(m_model);
            repaint();
        }
        void showProgress(const Progress& p) { (void)p; }
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff121212)); }
        void resized() override { m_sections.setBounds(getLocalBounds()); }
    private:
        DashboardModel m_model;
        SixSectionView m_sections;
    };

} // namespace aamp
#endif // AAMP_WITH_JUCE