// AAMP - tests/e2e/E2E_Specs.cpp
// SCAFFOLDING DE MAQUINA-ALVO (E2E/sistema; requer JUCE + hardware/display; NAO roda headless).
// Esqueletos CTest dos casos que so fecham na maquina-alvo. Guardados por AAMP_E2E para
// nunca entrarem na suite headless. Numeracao alinhada a DOC-11 §3.
#if defined(AAMP_E2E)
#include "../TestRunner.h"
// TC-002: hot-plug de dispositivo durante operacao (HardwareManager.onHotPlug -> atualiza lista).
TEST_CASE("TC-002 Hot-plug atualiza lista de dispositivos") { /* requer driver/OS real */ }
// TC-005: desconexao durante medicao -> aborto seguro (ER-002), estado ERROR, sem dados parciais.
TEST_CASE("TC-005 Desconexao durante medicao -> aborto seguro") { /* requer hardware real */ }
// TC-041: interacoes de grafico (zoom/pan/crosshair) preservam dados subjacentes.
TEST_CASE("TC-041 Interacoes de grafico") { /* requer display/GUI */ }
// TC-043: estado de carregamento exibido durante PROCESSING.
TEST_CASE("TC-043 Estado carregando") { /* requer GUI */ }
// TC-045: tooltip multi-curva mostra valores por curva no ponto.
TEST_CASE("TC-045 Tooltip multi-curva") { /* requer GUI */ }
// TC-053(render): PDF gerado com as 6 secoes (>=150 DPI, A4 paisagem, fontes incorporadas).
TEST_CASE("TC-053 Render PDF 6 secoes") { /* requer JUCE PDFRenderer */ }
// TC-054: PNG exportado >=1920x1080, 24-bit, com marcadores fixados.
TEST_CASE("TC-054 Render PNG >=1920x1080") { /* requer JUCE Graphics */ }
#endif // AAMP_E2E
