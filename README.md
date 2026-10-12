# SB-49 Ensemble — emulazione di string/brass machine divide-down (anni '70)

Plugin **VST3 / AAX / AU / Standalone** in **C++20 + JUCE 8**, che emula il circuito descritto nel manuale di servizio allegato (schede P550–P559).

## Perché C++ e JUCE
- VST3 e AAX sono SDK C++: qualunque altra scelta richiederebbe comunque un "ponte" in C++.
- JUCE gestisce da un unico codice VST3, AAX, AU e Standalone su macOS e Windows, la GUI vettoriale scalabile e il MIDI.
- Il motore DSP (`Source/dsp/`) è C++ puro, senza dipendenze: si testa da solo (`tools/render_test.cpp`).

## Compilazione

Requisiti: CMake ≥ 3.22, un compilatore C++20 (Xcode 14+ su macOS, Visual Studio 2022 su Windows).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release          # scarica JUCE 8.0.4 automaticamente
cmake --build build --config Release --target SB49_VST3 SB49_Standalone
```

Su macOS si può aggiungere `SB49_AU`. I file prodotti sono in `build/SB49_artefacts/Release/`.
Copia il `.vst3` in:
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `C:\Program Files\Common Files\VST3\`

### AAX (Pro Tools)
1. Registrati su developer.avid.com e scarica l'**AAX SDK**.
2. `cmake -B build -DAAX_SDK_PATH=/percorso/aax-sdk` e poi compila il target `SB49_AAX`.
3. Una build non firmata gira solo in **Pro Tools Developer**. Per Pro Tools normale serve la firma **PACE/iLok** (wraptool), che Avid concede tramite un accordo di sviluppo.

## Corrispondenza con il circuito originale

| Scheda | Cosa fa nell'originale | Come è emulata |
|---|---|---|
| P555 | Oscillatore master 2,00024 MHz → MK50240 (top octave) | 12 contatori con i divisori reali del MK50240 (239…451). L'intonazione ha quindi le stesse piccole stonature dell'hardware |
| P554/P558 | 12 TDA1008: divisori d'ottava, gate per tasto, scala resistiva 22K/39K/82K/150K | Onde quadre band-limited (polyBLEP) per ottava, sommate con gli stessi pesi → dente di sega "a gradini" 8' e 16'. Paraphonic su tutti i 49 tasti, con fase coerente tra le ottave come nel divide-down |
| P559 | Crescendo sul *buss bar* (brass OFF) o sul 3080 (brass ON), sustain sul pin 7, EQ a 3 bande | Inviluppo comune sul gate (se suoni legato il crescendo non riparte). Il sustain è per tasto ed è disattivato con il BRASS inserito, come da manuale. L'EQ è a 3 filtri SVF |
| P550/P551 | 3 linee BBD TCA350 modulate da un LFO a due velocità | 3 linee di ritardo con interpolazione Hermite, modulate a 0,63 Hz e 6,1 Hz con fasi a 120°, più i filtri anti-aliasing tipici delle BBD |
| P557 | VCF a ladder di transistor 4 poli, inviluppo AD (C18 differenzia il gate), Range, Resonance | Ladder ZDF con saturazione, sovracampionata 2x. Inviluppo modellato sulla topologia del circuito: il nodo D1-D2-C18 scarica tramite DECAY, C2 si carica tramite ATTACK |
| P556 | LFO triangolare, delay di 6-7 ms + salita, depth, VCF/FM, LED di velocità | Identico. Il LED sul selettore VCF/FM lampeggia alla velocità dell'LFO |
| Pannello posteriore | PITCH ±2,5 ottave | Manopola PITCH ±15 semitoni (doppio clic = 0) |

Estensioni non presenti nell'originale:
- **STEREO ENSEMBLE**: distribuisce le tre linee BBD sui due canali. L'originale è mono, quindi l'opzione è spenta di default.
- Pitch bend ±2 semitoni.
- La modwheel (CC1) agisce sulla profondità di modulazione.
- Il pedale sustain (CC64) mantiene i tasti premuti.
- Le note fuori dai 49 tasti (Do2–Do6) vengono ripiegate di ottava.

## MIDI e Arturia KeyLab

**Note, pitch bend, modwheel e pedale** funzionano subito.

**MIDI Learn**:
1. Clic destro (o ctrl-clic) su un qualunque slider, pulsante o sulla manopola PITCH.
2. Scegli "MIDI Learn".
3. Muovi il controllo sulla KeyLab.

La mappa viene salvata con il progetto. Il tooltip di ogni controllo mostra il CC assegnato. Per i pulsanti un valore ≥ 64 significa "inserito": sulla KeyLab conviene impostare i pad o i pulsanti in modalità *toggle* dal MIDI Control Center.

Mappa di partenza (modalità User/MIDI generico della KeyLab mkII; verificala nel MIDI Control Center, perché dipende dal preset caricato):

| Controllo KeyLab | CC | Parametro |
|---|---|---|
| Fader 1–4 | 73, 75, 79, 72 | Brass Attack, Decay, Range, Resonance |
| Fader 5 | 80 | Brass Volume |
| Fader 6–8 | 81, 82, 83 | Crescendo, Sustain, Strings Volume |
| Fader 9 | 85 | Master Volume |
| Encoder 1–3 | 74, 71, 76 | Strings Low, Mid, High |
| Encoder 4–6 | 77, 93, 18 | Mod Delay, Rate, Depth |
| Encoder 7 | 19 | Pitch |

Il menu contestuale include "Ripristina mappa Arturia KeyLab".

## Struttura

```
CMakeLists.txt
Source/dsp/PerformerEngine.{h,cpp}   motore DSP (senza JUCE)
Source/PluginProcessor.{h,cpp}       parametri, MIDI accurato al campione, stato
Source/MidiCCMap.h                   mappa CC lock-free + MIDI Learn
Source/PluginEditor.{h,cpp}          interfaccia (vettoriale, ridimensionabile 50–200%)
Source/PerformerLookAndFeel.h        slider, pulsanti, manopola
tools/render_test.cpp                test offline → WAV
```

## Note legali
- **JUCE 8** è distribuito sotto doppia licenza: AGPLv3, oppure licenza commerciale (gratuita sotto una certa soglia di fatturato). Per uso personale va bene così. Se lo distribuisci a codice chiuso, controlla i termini di JUCE.
- Il plugin usa un nome e una grafica propri: niente marchi o loghi del costruttore originale. Se pensi di distribuirlo, mantieni questa scelta.
