# Evidence Package (.aamp)

O `.aamp` é o contrato oficial entre o Acquisition Engine e o Laboratory
Engine.

## Estrutura inicial

``` text
Measurement.aamp
├── manifest.json
├── session.json
├── hardware.json
├── metadata.json
├── stimuli/
├── captures/
├── cache/ (opcional)
└── reports/ (opcional)
```

Cada estímulo gera:

-   STxx_Source.wav
-   STxx_Capture.wav
