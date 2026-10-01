# IzraTune (JUCE 8, VST3)

## Build local
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target IzraTune_VST3
(JUCE é baixado automaticamente. Requer CMake 3.22+, Visual Studio 2022 ou Xcode.)

## Instalação do VST3
- Windows: copie `IzraTune.vst3` para `C:\Program Files\Common Files\VST3\`
- macOS:   copie para `~/Library/Audio/Plug-Ins/VST3/`
(COPY_PLUGIN_AFTER_BUILD já faz isso automaticamente em build local.)
Depois, reescaneie plugins na sua DAW.

## Build na nuvem
Envie esta pasta para um repositório GitHub: a aba Actions gera `IzraTune.vst3` para Windows e macOS.

Prévia da interface sem plugin: abra ui/index.html no navegador.
