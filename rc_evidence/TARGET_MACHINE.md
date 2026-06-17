# AAMP — Fechamento dos Portões de Máquina-Alvo (turnkey)

Os portões abaixo são, pela definição da suíte (DOC-11 §2, DOC-12 §3, DOC-09 §6),
**cumpridos na máquina-alvo** (Windows 10/11 + macOS 12+ + hardware de áudio). Este
ambiente headless Linux não os executa. O scaffolding abaixo deixa cada um pronto para rodar.

## O que está turnkey
- **CMake + presets**: `windows-release` (MSVC), `macos-release` (AppleClang), `-DAAMP_WITH_JUCE=ON`.
- **CI** (`.github/workflows/ci.yml`): build Release zero-warning (P-07) por plataforma + CTest.
- **M-01 backend real**: `src/hardware/JuceDeviceBackend.h` implementa `IDeviceEnumerator`/`IAudioDevice`
  via `juce::AudioDeviceManager` (descoberta só via OS/drivers — hardware agnostic).
- **Render**: `src/export/render/JuceRenderWriters.h` implementa `IPdfWriter`/`IPngWriter`.
- **M-06 Dashboard**: `src/dashboard/DashboardComponent.h` (6 seções, gráficos interativos, progresso).

## Portões que fecham na máquina-alvo
| Portão | Como fecha | TCs |
|---|---|---|
| P-03 | Suíte automática em hardware real, Win + macOS | TC-002, TC-005 (E2E) |
| P-07 (MSVC/AppleClang) | Build Release `/W4 /WX` e `-Werror` sem warnings | CI |
| M-06 (display) | Render das 6 seções + interações | TC-040..045 |
| M-07 render | PDF (TC-053 render) + PNG (TC-054) | TC-053, TC-054 |
| SLAs | DSP <=15 s; captura <=110 s; etc. na máquina de referência (DOC-09 §6/§7) | medição |

## Nota honesta
O núcleo de lógica de M-01/M-02 **já está verificado headless** (TC-001 parte, TC-003, TC-004,
resolução mono/estéreo, ER-022, ER-025). Restam apenas os caminhos que exigem OS/driver real
(hot-plug TC-002, desconexão TC-005) e a GUI/render — todos turnkey acima.
