// AAMP - src/common/ResultBundle.h
// Contrato de dados central (DOC-08 §3.1): M-05 produz, M-06 renderiza, M-07 serializa.
// Nenhum recalculo fora de M-05. Fecha as fronteiras L-040/L-041/L-042 do DOC-CR.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace aamp {

// Flag de qualidade por familia de metrica (FR-027, BR-031, DOC-07 ER-023/ER-030).
// Metricas de baixa confiabilidade NUNCA sao omitidas: recebem flag.
enum class Quality {
    Normal,         // medicao confiavel
    LowConfidence,  // SNR insuficiente (DOC-07 §5): exibida com badge / sufixo '*'
    NotApplicable,  // condicional indisponivel (ex.: crosstalk em mono) -> 'N/A' (BR-043)
    Error           // falha irrecuperavel de calculo (ER-032), degradacao graciosa (FR-040)
};

const char* qualityToString(Quality q);

// Natureza LOGICA da estimativa (P1-009, estagio 1). TERCEIRO eixo do contrato, ORTOGONAL aos
// outros dois -- nao os substitui e nao e derivavel deles:
//   Quality       -> validade / confianca da medicao
//   valuePresent  -> existencia de um valor numericamente interpretavel
//   EstimateNature-> o que o valor AFIRMA sobre a grandeza (pontual ou unilateral)
// Um limite inferior pode ser uma medicao plenamente confiavel e presente: apenas a QUANTIFICACAO
// e unilateral. E vedado representar unilateralidade por LowConfidence ou por um valor novo de
// Quality (Registro de Decisao -- P1-009).
enum class EstimateNature {
    Point,        // o valor E a grandeza:              X
    LowerBound,   // a grandeza vale AO MENOS o valor:  >= X
    UpperBound    // a grandeza vale NO MAXIMO o valor: <= X
};

// Modo de roteamento derivado automaticamente pela contagem de canais (FR-011, BR-010).
enum class ChannelMode { Mono, Stereo };

// Uma curva (eixo X/Y + unidades). Usada por MS-01, MS-02, MS-03, MS-08, MS-11.
struct Curve {
    std::string name;
    std::string xUnit;          // ex.: "Hz"
    std::string yUnit;          // ex.: "dB"
    std::vector<double> x;
    std::vector<double> y;
    std::vector<double> y2;     // canal/serie auxiliar opcional (ex.: fase junto da magnitude)
    bool hasY2 = false;
};

// Valor escalar nomeado com unidade e qualidade.
struct Scalar {
    std::string key;            // chave para CSV escalar (DOC-10 §3.2), ex.: "THD_pct"
    double value = 0.0;
    std::string unit;           // ex.: "%", "dB", "ms"
    Quality quality = Quality::Normal;
    bool valuePresent = true;   // false => 'N/A' literal na exportacao (BR-043)
    // P1-009: ULTIMO membro, com inicializador padrao, para que as inicializacoes agregadas
    // existentes {key, value, unit, quality, valuePresent} permanecam validas e para que toda
    // metrica que nao se pronuncie continue sendo, por definicao, uma estimativa PONTUAL.
    EstimateNature estimate = EstimateNature::Point;
};

// Resultado de uma familia MS-01..MS-15.
struct MetricResult {
    std::string id;                 // "MS-01" .. "MS-15"
    std::string name;
    Quality quality = Quality::Normal;
    std::vector<Scalar> scalars;    // valores escalares chave=valor
    std::vector<Curve>  curves;     // curvas associadas
};

// Metadados de medicao (DOC-10 §3.1 cabecalho, WAV INFO).
struct MeasurementMeta {
    std::string deviceName = "Loopback Virtual";
    double sampleRate = 48000.0;
    ChannelMode mode = ChannelMode::Mono;
    std::string timestamp;          // "YYYY-MM-DD HH:MM:SS"
};

// Bundle tipado entregue de M-05 -> M-06/M-07.
struct ResultBundle {
    MeasurementMeta meta;
    std::vector<MetricResult> metrics;   // MS-01..MS-15 (ordenadas)

    const MetricResult* find(const std::string& id) const {
        for (const auto& m : metrics)
            if (m.id == id) return &m;
        return nullptr;
    }
};

} // namespace aamp
