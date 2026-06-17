// AAMP - src/export/IReportWriter.h
// Fronteiras de render (RD-03). O nucleo monta o MODELO do relatorio/grafico (testavel
// headless); o render real (PDF A4 paisagem >=150 DPI com fontes incorporadas; PNG
// >=1920x1080 com marcadores) e implementado na maquina-alvo (JUCE / lib permissiva).
#pragma once
#include <string>
#include <vector>
#include "../common/ResultBundle.h"

namespace aamp {

// Uma secao do relatorio (espelha as 6 secoes do dashboard - DOC-10 §5.1).
struct ReportSection {
    std::string title;                 // "Summary", "Technical Metrics", ...
    std::vector<std::string> lines;    // nome: valor unidade [badge]
};

// Modelo do relatorio consolidado (6 secoes). Construido por M-07 a partir do ResultBundle.
struct ReportModel {
    MeasurementMeta meta;
    std::vector<ReportSection> sections;  // exatamente 6 (DOC-10 §5.1)
};

// PDF: render na maquina-alvo (A4 paisagem, fontes incorporadas, sem "aprovado/reprovado").
class IPdfWriter {
public:
    virtual ~IPdfWriter() = default;
    virtual bool write(const ReportModel& model, const std::string& path) = 0;
};

// PNG: render na maquina-alvo (>=1920x1080, 24-bit, marcadores incluidos).
class IPngWriter {
public:
    virtual ~IPngWriter() = default;
    virtual bool write(const Curve& curve, const std::string& path) = 0;
};

} // namespace aamp
