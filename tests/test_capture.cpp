// AAMP - tests/test_capture.cpp
// M-04 Capture Engine (loopback virtual): TC-020..TC-024 (AC-M04-01..05).
#include "TestRunner.h"
#include "../src/capture/CaptureEngine.h"
#include "../src/capture/IAudioDevice.h"
#include "../src/stimulus/StimulusGenerator.h"
#include "../src/common/Constants.h"
#include <cmath>

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

// LoopbackDevice: injecao de piso de ruido e canais (harness §2.1).
TEST_CASE("LoopbackDevice ruido e canais") {
    LoopbackDevice dev(FS, 0);
    dev.setChannels(2, 2);
    CHECK(dev.inputChannels() == 2);
    CHECK(dev.outputChannels() == 2);
    dev.setNoiseFloorDbfs(-60.0);                 // exercita o ramo de ruido
    CaptureEngine eng(FS);
    StimulusGenerator gen(FS);
    Stimulus sil = gen.getStimulus(StimulusId::ST09_Silence);
    CaptureResult c = eng.capture(sil, dev);
    CHECK(!c.samples.empty());
    // ruido presente: RMS > 0.
    double s = 0.0; for (double v : c.samples) s += v * v;
    CHECK(s > 0.0);
}

// TC-020: sincronizacao <= 1 sample via IR de ST-06.
TEST_CASE("TC-020 Sincronizacao <= 1 sample (offset via IR de ST-06)") {
    const int D = 0; // loopback ideal alinhado
    LoopbackDevice dev(FS, D);
    CaptureEngine eng(FS);
    StimulusGenerator gen(FS);
    Stimulus imp = gen.getStimulus(StimulusId::ST06_Impulse);
    CaptureResult c = eng.capture(imp, dev);
    CHECK(c.status.valid);
    CHECK(std::abs(c.syncOffsetSamples) <= 1);
    // com latencia conhecida, o offset reflete a latencia (alinhavel por M-04/M-08).
    LoopbackDevice dev2(FS, 5);
    CaptureResult c2 = eng.capture(imp, dev2);
    CHECK(std::abs(c2.syncOffsetSamples - 5) <= 1);
}

// TC-021: clipping detectado -> captura invalida (decisao de repeticao/aborto e de M-08).
TEST_CASE("TC-021 Clipping detectado marca captura invalida (ER-020)") {
    LoopbackDevice dev(FS, 0);
    dev.setGain(2.0); // forca sinal acima de 0 dBFS
    CaptureEngine eng(FS, k::CLIP_THRESHOLD_DBFS_DEFAULT);
    StimulusGenerator gen(FS);
    Stimulus s = gen.getStimulus(StimulusId::ST03_PureSine);
    CaptureResult c = eng.capture(s, dev);
    CHECK(c.status.clipped);
    CHECK(!c.status.valid);
    // coherentAverage nunca atinge N validas com clipping persistente -> aborta apos limite.
    AverageResult avg = eng.coherentAverage(s, dev, 3, k::MAX_CAPTURE_RETRIES);
    CHECK(avg.aborted);
    CHECK(avg.invalidDiscarded >= 1);
}

// TC-022: sinal baixo -> aviso nao bloqueante (flag), captura ainda valida.
TEST_CASE("TC-022 Sinal baixo: aviso nao bloqueante (ER-023)") {
    LoopbackDevice dev(FS, 0);
    dev.setGain(0.0005);              // retorno muito baixo
    CaptureEngine eng(FS);
    StimulusGenerator gen(FS);
    Stimulus s = gen.getStimulus(StimulusId::ST03_PureSine);
    const double noiseFloorRms = std::pow(10.0, -60.0 / 20.0); // piso de referencia
    CaptureResult c = eng.capture(s, dev, noiseFloorRms);
    CHECK(c.status.lowSignal);       // SNR < 20 dB acima do piso
    CHECK(!c.status.clipped);
    CHECK(c.status.valid);           // NAO bloqueia: segue valido
}

// TC-023: captura parcial -> nunca processada; samples vazio.
TEST_CASE("TC-023 Captura parcial nao processada (ER-024)") {
    LoopbackDevice dev(FS, 0);
    StimulusGenerator gen(FS);
    Stimulus s = gen.getStimulus(StimulusId::ST07_BurstTone);
    dev.setTruncateTo(100); // buffer incompleto
    CaptureEngine eng(FS);
    CaptureResult c = eng.capture(s, dev);
    CHECK(c.status.partial);
    CHECK(!c.status.valid);
    CHECK(c.samples.empty());        // dados parciais nunca entregues a M-05
}

// TC-024: averaging coerente no tempo; invalidas descartadas e substituidas.
TEST_CASE("TC-024 Averaging coerente (invalidas substituidas)") {
    LoopbackDevice dev(FS, 0);
    CaptureEngine eng(FS);
    StimulusGenerator gen(FS);
    Stimulus s = gen.getStimulus(StimulusId::ST07_BurstTone);
    AverageResult avg = eng.coherentAverage(s, dev, 4, k::MAX_CAPTURE_RETRIES);
    CHECK(!avg.aborted);
    CHECK(avg.validUsed == 4);
    CHECK(!avg.samples.empty());
    // media de capturas identicas (loopback determinístico) == a propria captura.
    CaptureResult one = eng.capture(s, dev);
    double maxErr = 0.0;
    for (std::size_t i = 0; i < avg.samples.size() && i < one.samples.size(); ++i)
        maxErr = std::max(maxErr, std::fabs(avg.samples[i] - one.samples[i]));
    CHECK(maxErr < 1e-9);
}
