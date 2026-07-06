// AAMP - src/bridge/AampWebBridge.h
// SCAFFOLDING DE MAQUINA-ALVO (requer JUCE 8; NAO compilado/verificado neste ambiente headless).
// Mesma convencao de JuceDeviceBackend.h: codigo real contra as APIs reais, a compilar/verificar
// no build Windows/target.
//
// PAPEL: host nativo unico que hospeda o frontend HTML num juce::WebBrowserComponent e faz a ponte
// IN-PROCESS entre o frontend e o backend existente (executavel unico, offline, sem IPC). O frontend
// so envia comandos e recebe eventos; toda a logica permanece no Measurement Engine.
//
// >>> ESTRATEGIA DE DESENVOLVIMENTO (temporaria) <<<
// Nesta fase o HTML NAO e embutido (sem BinaryData / sem ResourceProvider). O WebBrowserComponent
// carrega o arquivo EXTERNO diretamente, para permitir iterar HTML/CSS/JS sem recompilar o AAMP.
// Ordem de resolucao do arquivo (ver resolveHtmlFile):
//   1) <dir do executavel>/resources/aamp_laboratory.html   (relativo — preferido)
//   2) C:\aamp\resources\aamp_laboratory.html                (fallback fixo de dev)
// Quando a integracao estabilizar, voltamos ao BinaryData para o produto final ser um unico
// executavel totalmente offline (basta reintroduzir withResourceProvider + goToURL(root)).
//
// CONTRATO da ponte (inalterado — 1:1 com o mock do frontend):
//   Comandos (JS -> C++):  enumerate | prepare{deviceId} | start | cancel | export{format}
//   Eventos  (C++ -> JS):  devices | state | progress | stimulusIntegrity | metricReady | runFinished | exported
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "../hardware/HardwareManager.h"
#include "../hardware/JuceCaptureDevice.h"          // bloco REAL de play+capture (relocado do Gerador)
#include "../orchestration/MeasurementOrchestrator.h"
#include "../capture/IAudioDevice.h"
#include "../common/ResultBundle.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

namespace aamp {

class AampWebBridge : public juce::Component {
public:
    AampWebBridge();
    ~AampWebBridge() override;

    void resized() override;

    // DEV: resolve o caminho do HTML externo (relativo ao executavel; fallback C:\aamp\resources).
    static juce::File resolveHtmlFile();

private:
    std::unique_ptr<juce::WebBrowserComponent>  m_web;
    juce::AudioDeviceManager                     m_dm;      // device REAL persistente (config + medicao)
    std::unique_ptr<HardwareManager>            m_hw;      // enumeracao (lista de conectados)
    std::vector<DeviceInfo>                      m_devices;
    std::thread                                  m_worker;
    std::thread                                  m_exportWorker;         // I/O de export (M-07)
    std::atomic<bool>                            m_running { false };
    std::atomic<bool>                            m_abort { false };      // aborto da medicao (JuceCaptureDevice)
    bool                                         m_deviceReady { false }; // m_dm aberto/configurado (READY)
    std::string                                  m_selectedDeviceName;   // device escolhido (meta.device)
    double                                       m_prepFs { 48000.0 };   // fs efetivo apos abrir o device
    ResultBundle                                 m_lastBundle;           // retido p/ ExportEngine (M-07)
    std::atomic<bool>                            m_haveBundle { false }; // bundle valido para exportar

    // emit ao JS (sempre empacotado para a thread de mensagens)
    void emitEvent(const juce::String& evt, juce::var data);

    // roteamento de comandos vindos do JS
    void onCommand(const juce::String& json);
    void doEnumerate();
    void doPrepare(juce::var args);
    void doStart();
    void doCancel();
    void doExport(juce::var args);
    // callbacks do orchestrator (worker) -> eventos ao JS (Message Thread via emitEvent)
    void wireCallbacks(MeasurementOrchestrator& orch);
};

} // namespace aamp
#endif // AAMP_WITH_JUCE
