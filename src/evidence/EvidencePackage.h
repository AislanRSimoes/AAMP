// AAMP - src/evidence/EvidencePackage.h
// Evidence Package — modelo de domínio da SESSÃO DE AQUISIÇÃO (Etapa 1 da migração).
//
// Este modelo representa integralmente uma sessão de aquisição contendo APENAS fatos
// observados durante a aquisição, informação de configuração e a identidade dos estímulos.
// Nenhuma informação científica derivada (FFT, magnitude, fase, group delay, THD, etc.) é
// armazenada: o Acquisition Engine produz evidência, não conhecimento. Toda a ciência é
// reproduzível pelo Laboratory Engine EXCLUSIVAMENTE a partir das formas de onda aqui contidas.
//
// Independência arquitetural (Princípio 4): este cabeçalho depende apenas da STL. Não inclui
// ResultBundle (ciência), StimulusGenerator, CaptureEngine nem JUCE. O vínculo com um estímulo
// é feito pelo CÓDIGO string ("ST-01".."ST-16"), não por tipos internos de outro módulo.
//
// Esta etapa define o modelo. A serialização para .aamp e a montagem a partir do fluxo de
// aquisição ao vivo pertencem a etapas posteriores da migração.
#pragma once
#include <string>
#include <vector>

namespace aamp {
namespace evidence {

// Layout de canais da aquisição (configuração de roteamento).
enum class Channels { Mono, Stereo };

// Metadados de sessão — identidade e instante observado da aquisição.
struct SessionEvidence {
    std::string sessionId;        // identificador da sessão de aquisição
    std::string appVersion;       // versão do Acquisition Engine que produziu a evidência
    std::string timestamp;        // instante observado da aquisição ("YYYY-MM-DD HH:MM:SS")
};

// Metadados de hardware — configuração observada do dispositivo de áudio.
struct HardwareEvidence {
    std::string deviceName;                 // dispositivo utilizado na aquisição
    double      sampleRate = 48000.0;       // taxa de amostragem efetiva (configuração)
    Channels    channels   = Channels::Mono;// modo de roteamento configurado
};

// Metadados de aquisição — parâmetros de configuração determinísticos do processo.
// (Nenhum valor medido/derivado: o piso de ruído, por exemplo, é recomputável pelo Laboratory
//  a partir da captura do ST-09, presente em `captures`.)
struct AcquisitionEvidence {
    double        clipThresholdDbfs = 0.0;  // limiar de clipping configurado para a aquisição
    unsigned long prngSeed          = 1;    // seed determinístico dos estímulos (reprodutibilidade)
    double        preRollSeconds    = 0.0;  // pré-roll configurado
    double        postRollSeconds   = 0.0;  // pós-roll configurado
};

// Estímulo emitido — a forma de onda de origem que foi reproduzida (STxx_Source).
struct StimulusEvidence {
    std::string         code;         // "ST-01".."ST-16" (identidade)
    std::string         name;         // nome do estímulo
    char                category = 'A';// 'A'/'B'/'C' (organização da suíte)
    std::vector<double> source;       // forma de onda de origem emitida (fato: o que foi reproduzido)
};

// Resposta capturada de um estímulo (STxx_Capture) — áudio bruto observado + telemetria.
struct CaptureEvidence {
    std::string         code;              // estímulo ao qual esta captura responde (vínculo)
    std::vector<double> primary;           // canal principal capturado (fato observado diretamente)
    std::vector<double> secondary;         // canal direito capturado em estéreo (vazio em mono)
    // Telemetria da aquisição (evento do processo, NÃO interpretação do sinal): captura incompleta
    // por underrun/abort do dispositivo (CaptureEngine: raw.size() < stimulus.size()).
    bool                partial = false;
};

// Pacote completo da sessão de aquisição: um StimulusEvidence e um CaptureEvidence por estímulo.
struct EvidencePackage {
    SessionEvidence               session;
    HardwareEvidence              hardware;
    AcquisitionEvidence           acquisition;
    std::vector<StimulusEvidence> stimuli;    // formas de onda de origem emitidas
    std::vector<CaptureEvidence>  captures;   // formas de onda capturadas correspondentes
};

} // namespace evidence
} // namespace aamp
