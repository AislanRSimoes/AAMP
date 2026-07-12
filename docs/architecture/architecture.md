# AAMP Architecture

## Princípio

O AAMP é composto por dois motores independentes em responsabilidade:

-   **Acquisition Engine (C++)**
-   **Laboratory Engine (HTML + JavaScript)**

O único contrato entre ambos é o **Evidence Package (.aamp)**.

## Acquisition Engine

Responsabilidades: - Detectar e configurar hardware - Controlar WASAPI /
ASIO - Gerar estímulos - Reproduzir estímulos - Capturar áudio -
Sincronizar aquisição - Gerar um WAV de origem e um WAV capturado para
cada estímulo - Empacotar a sessão em um `.aamp` - Informar progresso ao
Laboratory Engine

Não deve conter algoritmos científicos.

## Laboratory Engine

Responsável por: - Abrir `.aamp` - Carregar WAVs - Calcular métricas -
Gerar gráficos - Executar Investigation - Exportar relatórios

## Princípio Arquitetural

O Acquisition Engine produz **evidências**. O Laboratory Engine produz
**conhecimento**.
