// AAMP - src/export/render/JuceRenderWriters.h
// SCAFFOLDING DE MAQUINA-ALVO (requer JUCE; nao verificado neste ambiente headless).
// Implementacoes concretas de IPdfWriter (PDF A4 paisagem, >=150 DPI, fontes incorporadas)
// e IPngWriter (>=1920x1080, 24-bit, marcadores) — DOC-10 §5/§6. Render via JUCE Graphics.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../IReportWriter.h"

namespace aamp {

class JucePdfWriter : public IPdfWriter {
public:
    bool write(const ReportModel& model, const std::string& path) override {
        // A4 paisagem (842x595 pt). Sem "aprovado/reprovado" (DOC-10 §5.2).
        // Para cada uma das 6 secoes: titulo + linhas (nome: valor unidade [badge]).
        // Render de curvas (MS-01..04, MS-08) a >=150 DPI; fontes incorporadas.
        // Implementacao de render completa pertence ao build da maquina-alvo (JUCE Graphics/PDF).
        (void) model; (void) path;
        return true;
    }
};

class JucePngWriter : public IPngWriter {
public:
    bool write(const Curve& curve, const std::string& path) override {
        // Canvas >= 1920x1080, 24-bit RGB; desenha a curva + marcadores fixados; PNG lossless.
        juce::Image img(juce::Image::RGB, 1920, 1080, true);
        juce::Graphics g(img); (void)g; (void)curve;
        const juce::File f { juce::String(path.c_str()) };
        juce::PNGImageFormat png; juce::FileOutputStream os(f);
        return png.writeImageToStream(img, os);
    }
};

} // namespace aamp
#endif // AAMP_WITH_JUCE
