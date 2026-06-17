// AAMP - src/dashboard/DashboardSections.h
// SCAFFOLDING DE MAQUINA-ALVO (M-06/L-01; requer JUCE; nao verificado headless).
// Sub-componentes JUCE que RENDERIZAM o DashboardModel ja testado (buildDashboardModel).
// Uma classe por secao (DOC-05 §3). Tema escuro, grade 8 px, badges de estado, sem julgamento.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "DashboardModel.h"

namespace aamp {

// Renderiza uma DashboardSection: titulo + lista de itens (label / value unit / badge de estado).
class SectionPanel : public juce::Component {
public:
    void setSection(const DashboardSection& s) { m_section = s; repaint(); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff121212));                 // tema escuro
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        auto r = getLocalBounds().reduced(8);
        g.drawText(m_section.title, r.removeFromTop(28), juce::Justification::left);
        g.setFont(14.0f);
        for (const auto& it : m_section.items) {
            auto row = r.removeFromTop(22);
            g.setColour(juce::Colours::lightgrey);
            g.drawText(it.label, row.removeFromLeft(220), juce::Justification::left);
            // estado -> apresentacao (FR-031): N/A, badge '*' de baixa-confiabilidade, vazio.
            juce::String val = it.value.empty() ? juce::String("--") : juce::String(it.value);
            if (it.state == ItemState::LowConfidence) val += " *";
            g.setColour(it.state == ItemState::NotApplicable ? juce::Colours::grey
                        : it.state == ItemState::LowConfidence ? juce::Colours::orange
                        : juce::Colours::white);
            g.drawText(val + " " + it.unit, row, juce::Justification::left);
        }
    }
private:
    DashboardSection m_section;
};

// As 6 secoes em layout vertical/responsivo; alimentadas a partir do DashboardModel.
class SixSectionView : public juce::Component {
public:
    SixSectionView() { for (auto& p : m_panels) addAndMakeVisible(p); }
    void setModel(const DashboardModel& dm) {
        for (std::size_t i = 0; i < m_panels.size() && i < dm.sections.size(); ++i)
            m_panels[i].setSection(dm.sections[i]);
    }
    void resized() override {
        auto r = getLocalBounds();
        const int h = r.getHeight() / static_cast<int>(m_panels.size());
        for (auto& p : m_panels) p.setBounds(r.removeFromTop(h));
    }
private:
    std::array<SectionPanel, 6> m_panels;
};

} // namespace aamp
#endif // AAMP_WITH_JUCE
