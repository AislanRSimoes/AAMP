// AAMP - src/export/ExportEngine.h
// M-07 (DOC-08 §3): exportCSV(), exportWAV(), exportPDF(), exportPNG().
// Serializa o ResultBundle conforme DOC-10. N/A explicito (BR-043); baixa confiab. com '*'.
// Toda I/O em ExportThread; ER-040 preserva dados (o bundle nunca e mutado).
#pragma once
#include <string>
#include <vector>
#include "../common/ResultBundle.h"
#include "IReportWriter.h"

namespace aamp {

struct ExportStatus {
    bool ok = false;
    std::string message;     // amigavel (DOC-07 ER-040) em caso de falha
    std::string path;
};

class ExportEngine {
public:
    explicit ExportEngine(const std::string& appVersion = "v2.0");

    // CSV de curvas (DOC-10 §3.1): cabecalho '#', colunas, dados UTF-8, ponto decimal, 3 casas.
    ExportStatus exportCurvesCSV(const ResultBundle& b, const std::string& path) const;

    // CSV de metricas escalares (DOC-10 §3.2): chave=valor; '*' p/ baixa confiab.; N/A.
    ExportStatus exportScalarsCSV(const ResultBundle& b, const std::string& path) const;

    // WAV da IR (DOC-10 §4): PCM IEEE float 32-bit, SR da medicao, INFO (ISFT/ICRD/ICMT).
    ExportStatus exportWAV(const std::vector<double>& ir, double fs, int channels,
                           const std::string& path) const;

    // Modelo do relatorio (6 secoes) — render PDF via IPdfWriter na maquina-alvo.
    ReportModel buildReportModel(const ResultBundle& b) const;
    ExportStatus exportPDF(const ResultBundle& b, IPdfWriter& writer, const std::string& path) const;

    // Nomenclatura DOC-10 §7: aamp_{tipo}_{YYYYMMDD}_{HHMMSS}.{ext}
    static std::string makeFilename(const std::string& tipo, const std::string& ext,
                                    const std::string& timestamp);

    // Formatacao de valor (3 casas, ponto, 'N/A' / sufixo '*').
    static std::string formatScalar(const Scalar& s);

private:
    std::string m_version;
    std::string metadataHeader(const MeasurementMeta& m) const;
};

} // namespace aamp
