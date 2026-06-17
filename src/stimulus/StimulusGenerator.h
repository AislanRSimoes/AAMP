// AAMP - src/stimulus/StimulusGenerator.h
// M-03 (DOC-08 §3): generateSuite(), getStimulus(id). 16 estimulos determinísticos.
// Parametros EXATOS de DOC-03. Ordem da suite: A -> B -> C (DOC-03 §5).
#pragma once
#include <vector>
#include <string>

namespace aamp {

enum class StimulusId {
    ST01_ESS, ST02_MultiTone, ST03_PureSine, ST04_WhiteNoise, ST05_PinkNoise,
    ST06_Impulse, ST07_BurstTone, ST08_DynamicStep, ST09_Silence,
    ST10_HardAttack, ST11_SoftAttack, ST12_PulseTrain, ST13_Sustained,
    ST14_HarmonicCluster, ST15_DynamicPerformance, ST16_LatencyRef
};

struct Stimulus {
    StimulusId id;
    std::string code;       // "ST-01" .. "ST-16"
    std::string name;
    char category;          // 'A', 'B', 'C'
    std::vector<double> samples; // sinal de saida mono (double), referencia
};

class StimulusGenerator {
public:
    explicit StimulusGenerator(double sampleRate);

    // Gera a suite completa na ordem determinística A -> B -> C (FR-014, BR-020).
    std::vector<Stimulus> generateSuite();

    // Gera um estimulo individual por id.
    Stimulus getStimulus(StimulusId id) const;

    double sampleRate() const { return m_fs; }

private:
    double m_fs;

    // helpers
    std::vector<double> ess() const;             // ST-01
    std::vector<double> multiTone() const;       // ST-02
    std::vector<double> pureSine() const;        // ST-03
    std::vector<double> whiteNoise() const;      // ST-04
    std::vector<double> pinkNoise() const;       // ST-05
    std::vector<double> impulse() const;         // ST-06
    std::vector<double> burstTone() const;       // ST-07
    std::vector<double> dynamicStep() const;     // ST-08
    std::vector<double> silence() const;         // ST-09
    std::vector<double> hardAttack() const;      // ST-10
    std::vector<double> softAttack() const;      // ST-11
    std::vector<double> pulseTrain() const;      // ST-12
    std::vector<double> sustained() const;       // ST-13
    std::vector<double> harmonicCluster() const; // ST-14
    std::vector<double> dynamicPerformance() const; // ST-15
    std::vector<double> latencyRef() const;      // ST-16
};

// Utilitarios compartilhados (M-03): conversao dBFS, fades, normalizacao composta.
double dbfsToLinear(double dbfs);
double peak(const std::vector<double>& x);
double rms(const std::vector<double>& x);
void applyHannFade(std::vector<double>& x, int fadeSamples);     // fade-in/out Hann
void normalizeCompositePeak(std::vector<double>& x, double targetDbfs); // -> alvo dBFS de pico

} // namespace aamp
