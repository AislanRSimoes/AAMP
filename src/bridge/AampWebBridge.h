// AAMP - src/bridge/AampWebBridge.h
// Host nativo ÚNICO do frontend: hospeda resources/aamp_laboratory.html num
// juce::WebBrowserComponent (WebView2) e faz a ponte IN-PROCESS entre o frontend e o
// backend existente (executável único, offline, sem IPC). O frontend só envia comandos
// e recebe eventos; toda a lógica científica permanece no Measurement Engine.
//
// SOURCE OF TRUTH do frontend: <raiz do projeto>/resources/aamp_laboratory.html.
// O build copia esse arquivo para <exe>/resources (CMake POST_BUILD); qualquer HTML em
// build/ é artefato gerado — nunca editado. resolveHtmlFile() resolve nessa ordem:
//   1) <dir do executável>/resources/aamp_laboratory.html   (artefato do build)
//   2) C:\aamp\resources\aamp_laboratory.html                (fonte, fallback de dev)
// (BinaryData/ResourceProvider entram quando a integração congelar: executável único.)
//
// CONTRATO da ponte (1:1 com o frontend):
//   Comandos (JS -> C++): enumerate | prepare{deviceId,mode,inBase,outBase} | start | cancel
//                         export{format} | getSettings | saveSettings{hardware} |
//                         setLanguage{lang} | closeConfirm | diag/fperf{msg}
//   Eventos  (C++ -> JS): devices | prepared | state | progress | stimulusIntegrity |
//                         metricReady | runFinished | exported | settings | closeRequested
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "../hardware/HardwareManager.h"
#include "../hardware/JuceCaptureDevice.h"          // bloco REAL de play+capture
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

    // Resolve o HTML oficial (ver cabeçalho: build/ é artefato; resources/ é a fonte).
    static juce::File resolveHtmlFile();

    // Encerramento conduzido pelo produto: emite 'closeRequested' ao frontend e retorna
    // true se o frontend está vivo para decidir (modal exportar/descartar); false quando a
    // página nunca falou com a ponte — o host deve então fechar diretamente (robustez).
    bool requestCloseFromHost();

private:
    std::unique_ptr<juce::WebBrowserComponent>  m_web;
    juce::AudioDeviceManager                    m_dm;       // device REAL persistente (config + medição)
    std::unique_ptr<HardwareManager>            m_hw;       // enumeração (somente conectados)
    std::vector<DeviceInfo>                     m_devices;
    std::thread                                 m_worker;
    std::thread                                 m_exportWorker;          // I/O de export (M-07)
    std::atomic<bool>                           m_running { false };
    std::atomic<bool>                           m_abort { false };       // aborto da medição
    std::atomic<bool>                           m_uiAlive { false };     // frontend já falou com a ponte
    bool                                        m_deviceReady { false }; // m_dm aberto/configurado (READY)
    std::string                                 m_selectedDeviceName;    // device escolhido (meta.device)
    double                                      m_prepFs { 48000.0 };    // fs efetivo após abrir o device
    ResultBundle                                m_lastBundle;            // retido p/ ExportEngine (M-07)
    std::atomic<bool>                           m_haveBundle { false };  // bundle válido para exportar

    // Persistência do produto (idioma + última configuração de hardware): settings.xml em
    // <userAppData>/AAMP. Nunca guarda dados científicos — Session persiste só via export.
    std::unique_ptr<juce::PropertiesFile>       m_props;
    juce::var loadSettingsVar() const;                     // -> payload do evento 'settings'
    void      saveHardwareSettings(const juce::var& hw);   // saveSettings{hardware}
    void      saveLanguage(const juce::String& lang);      // setLanguage{lang}

    // emit ao JS (sempre empacotado para a thread de mensagens)
    void emitEvent(const juce::String& evt, const juce::var& data);

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
