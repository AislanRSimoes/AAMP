// AAMP - src/evidence/EvidenceRecorder.h
// Observador especializado da aquisição (Etapa 2 da migração): transforma os fatos observados
// durante a caracterização em um EvidencePackage. É o ÚNICO componente que conhece tanto os
// tipos de domínio da aquisição (Stimulus, CaptureResult) quanto o modelo de evidência.
//
// Papel: OBSERVADOR, não participante. O MeasurementOrchestrator apenas coordena e EMITE fatos
// (Stimulus + CaptureResult) por um callback somente-leitura; este recorder os REGISTRA. A
// transformação fato -> evidência é responsabilidade exclusiva daqui.
//
// Seleção de fatos: registra APENAS o que é observado — identidade e forma de onda de origem do
// Stimulus; captura bruta e telemetria `partial` do CaptureResult. Os campos DERIVADOS por
// processamento do CaptureResult (syncOffsetSamples, decay20/40/60, nonDecayed,
// observedDurationSamples) são deliberadamente IGNORADOS: nenhuma ciência entra na evidência.
//
// Metadados de configuração (device/fs/mode/seed/roll/clip) são conhecidos ANTES do processo e,
// por isso, fornecidos pelo dono do recorder — não são fatos observados durante a aquisição.
#pragma once
#include "EvidencePackage.h"
#include "../stimulus/StimulusGenerator.h"
#include "../capture/CaptureEngine.h"
#include <utility>

namespace aamp {
namespace evidence {

class EvidenceRecorder {
public:
    EvidenceRecorder() = default;

    // Configuração conhecida antes do processo (definida pelo dono do recorder).
    void setSession(const SessionEvidence& s)         { m_pkg.session = s; }
    void setHardware(const HardwareEvidence& h)       { m_pkg.hardware = h; }
    void setAcquisition(const AcquisitionEvidence& a) { m_pkg.acquisition = a; }

    // Observação SOMENTE-LEITURA de um par (estímulo emitido, captura observada). Não altera os
    // objetos observados: seleciona os fatos e os copia para o EvidencePackage.
    void observe(const Stimulus& s, const CaptureResult& c) {
        StimulusEvidence se;
        se.code = s.code;
        se.name = s.name;
        se.category = s.category;
        se.source = s.samples;                 // forma de onda de origem emitida (fato observado)
        m_pkg.stimuli.push_back(std::move(se));

        CaptureEvidence ce;
        ce.code = s.code;                       // vínculo estímulo <-> captura
        ce.primary = c.samples;                 // captura bruta do canal principal (fato observado)
        // ce.secondary permanece vazio: o fluxo de captura atual é mono.
        ce.partial = c.status.partial;          // telemetria de aquisição (evento do processo)
        // Campos derivados de CaptureResult (sync/decay/nonDecayed/observedDuration) NÃO entram.
        m_pkg.captures.push_back(std::move(ce));
    }

    // Pacote acumulado até o momento (propriedade do recorder).
    const EvidencePackage& package() const { return m_pkg; }

private:
    EvidencePackage m_pkg;
};

} // namespace evidence
} // namespace aamp
