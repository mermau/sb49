# BIZARRE Entertainer — emulazione del Crumar Performer

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

Ogni blocco è ricavato dai valori dei componenti degli schemi di servizio. I blocchi lineari e l'inviluppo brass sono stati verificati con ngspice: il modello C++ coincide con la simulazione entro 0,05 dB (catena archi) ed entro pochi millivolt (inviluppo).

| Scheda | Circuito | Come è emulato |
|---|---|---|
| P555 | Oscillatore master 2,00024 MHz → MK50240 | Divisori reali 239…451: stesse piccole stonature dell'originale |
| P554 | 12 TDA1008, scala resistiva 22K/39K/82K/150K, sommatori IC13 (22K ∥ 2,2 nF) e IC14 (8K2 ∥ 10 nF) | Onde quadre band-limited con gli stessi pesi. Passa-basso dei sommatori: 3,29 kHz per gli archi, 1,94 kHz per il brass |
| P554 | Attacco dei tasti: R5…R9 in serie a 4,7 µF | Costante per tasto dalla tabella: 4,7 ms nelle due ottave basse, da 4,7 a 18 ms nell'ottava centrale (R7 da 1K a 3K9), 22 ms in quella alta |
| P559 | Crescendo: TR3 generatore di corrente (DZ1 6,2 V) su C14 47 µF | Rampa **lineare** da 55 ms a 5,5 s (P4 100K lin), scarica in 47 ms (R43 1K). Il buss bar parte da circa il 37% del livello pieno |
| P559 | Miscelatore 8'/16', preenfasi IC2, EQ a giratori, R22/C13 | Simulazione nodale del circuito vero e proprio (25 nodi, 4 operazionali e 3 giratori). Bande a 341 Hz, 881 Hz e 3,2 kHz; da −13 a +13 dB circa, piatto a metà corsa; potenziometri lineari come nell'originale |
| P551 | Filtri d'ingresso e d'uscita delle BBD (Sallen-Key) | Ingresso 7,3 kHz Q 4,1 (nel circuito simulato), uscita 8,5 kHz Q 1,9. Insieme alla preenfasi danno una risposta piatta fino a 6 kHz con taglio netto sopra |
| P550/P551 | Tre TCA350 (185 stadi), clock 22,6 kHz | Ritardo centrale 4,09 ms. LFO lento 0,52 Hz (±6,5%, tre fasi a 120°), veloce 5,8 Hz (±1,5%, fasi a 90°): **tarati sui campioni del Performer originale** |
| P557 | Inviluppo brass: TR16, C19, TR15, C18, D1/D2, DECAY, ATTACK, C2, nodo di somma | Simulazione a livello di circuito con diodi e transistor. Riproduce il ritardo di circa 50 ms dell'apertura, la dipendenza della profondità dall'ATTACK e il trigger unico del gate comune |
| P557 | Convertitore esponenziale TR13/TR14, RANGE (4 ottave), modulazione (R33 100K) | 17,9 mV/ottava. Somma di RANGE, inviluppo e LFO con i pesi delle resistenze |
| P557 | Ladder 4 poli, 4 × 47 nF | Modello non lineare a transistor (Huovilainen), sovracampionato 2x. Livello d'ingresso **tarato sui campioni**: a filtro aperto le armoniche pari restano intatte, quindi il ladder lavora quasi in zona lineare (lo schema farebbe pensare a una forte saturazione) |
| P556 | LFO: integratore 22K/940 nF, Schmitt 10K/68K, RATE 1M antilog | Triangolo da 0,21 a 30 Hz. Il 3080 arrotonda i picchi. DELAY: reset 6,9 ms poi salita **lineare** da 0,58 a 6,4 s. FM fino a circa ±0,8 semitoni; VCF fino a circa ±4 ottave. Il LED segue l'uscita del comparatore (onda quadra) |
| P559/P556 | Volumi 100K lineari | Curva lineare caricata, non logaritmica |

### Taratura sui campioni originali
Venti campioni di un Performer (La2, mono, MP3) sono stati analizzati misurando lo spettro di modulazione di frequenza e d'ampiezza di dieci armoniche. Velocità, profondità e fasi dei due LFO dell'ensemble sono state cercate in modo sistematico finché gli spettri del plugin coincidono con quelli dei campioni. Altri quattro set AIFF senza compressione (archi e brass, Do1–Do5) hanno confermato:
- **forme d'onda**: 8ª e 24ª armonica presenti, 16ª e 32ª assenti su archi 16' e brass: esattamente le quattro onde quadre al 50% dello schema. (Un'ipotesi di impulso al 25%, dedotta dagli MP3, è stata scartata);
- **intonazione**: i Do risultano leggermente calanti, come previsto dal divisore 239 dell'MK50240;
- **brass**: filtro aperto fino a 0,15–0,3 s e chiuso entro 0,5 s; a riposo si posa intorno a 130–150 Hz su tutte le note, coerente con la taratura di RV1. Le code di volume lineari dei campioni sono dissolvenze aggiunte in editing.

Lo strumento di misura è in `tools/render_note.cpp`; le posizioni dei comandi durante la registrazione non sono note, quindi EQ, brass e trimmer non sono stati tarati su questi campioni.

### Valori non ricavabili dallo schema (scelte dichiarate)
- **C12 (EQ)**: sullo schema si legge "47n". Con 47 nF gli acuti cadrebbero progressivamente; con 47 pF la catena è piatta come previsto dal progetto. Uso 47 pF.
- **Trimmer di taratura**: RV1 FINE RANGE (P557) è tarato a 100 Hz con RANGE a 0. RV1 del crescendo (P559) ha l'offset scelto perché il buss bar arrivi a 6,2 V a fine rampa. RV2 FINE RES (P557) permette l'auto-oscillazione a risonanza massima.
- **TDA1008**: il datasheet non è reperibile. Il rilascio senza sustain (8 ms) e la curva del SUSTAIN sono stimati; il massimo è limitato da 1 MΩ × 4,7 µF = 4,7 s.
- **Livelli relativi archi/brass**: il guadagno dello stadio d'uscita del ladder non è ricavabile; i due livelli sono pareggiati.

Estensioni non presenti nell'originale:
- **BRASS RETRIG**: fa ripartire l'attacco del brass a ogni nuova nota anche suonando legato. Spento di default, come nell'originale.
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

## Nome, logo e icona
Il nome BIZARRE Entertainer, il lettering e l'icona sono originali. I font sono Monoton (Vernon Adams) e Shrikhand (Jonny Pinhorn), entrambi con licenza SIL Open Font License 1.1: le licenze sono in `Resources/`. Il codice interno del plugin (Sb49) non è cambiato, così i progetti salvati con le versioni precedenti si riaprono normalmente.
