// BIZARRE Entertainer — emulazione del Crumar Performer a partire dagli schemi di servizio (P550-P559).
// Motore DSP indipendente da JUCE: compilabile e testabile da solo.
#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <atomic>
#include "Circuits.h"

namespace perf
{
constexpr int    kNumKeys     = 49;        // tastiera 49 tasti, Do-Do (P558)
constexpr int    kLowestNote  = 36;        // C2 = primo tasto
constexpr double kMasterClock = 2000240.0; // oscillatore master (P555)
constexpr int    kNumOctaves  = 10;

struct Params
{
    // BRASS (P557)
    float brassAttack = 0.15f, brassDecay = 0.6f, brassRange = 0.45f, brassResonance = 0.25f, brassVolume = 0.7f;
    bool  brassOn = false;
    bool  brassRetrig = false; // estensione: riattacco a ogni nota (l'originale è single-trigger)
    // STRINGS (P554/P559)
    float stringsLow = 0.5f, stringsMid = 0.5f, stringsHigh = 0.5f;
    bool  strings16 = false, strings8 = true;
    int   stringsTranspose = 0; // -1 / 0 / +1 ottava (switch TRANSPOSE della versione B)
    float crescendo = 0.15f, sustain = 0.35f, stringsVolume = 0.75f;
    // MODULATION (P556)
    float modDelay = 0.3f, modRate = 0.45f, modDepth = 0.0f;
    bool  modToVcf = false;
    // master / pannello posteriore
    float masterVolume = 0.8f;
    float pitchSemitones = 0.0f;
    bool  stereoEnsemble = false;  // estensione
};

// Tre linee BBD TCA350 (185 stadi, clock ~22,6 kHz) modulate dall'LFO a due velocità (P550/P551)
struct Ensemble
{
    void prepare (double sr);
    void reset();
    void process (float x, float& outL, float& outR, bool stereo);
    // modulazione del clock delle BBD (velocità in Hz, profondità relativa al ritardo centrale)
    // tarati sui campioni del Performer originale (spettri di modulazione AM/FM, vedi README)
    double slowHz = 0.52, fastHz = 5.8, slowDepth = 0.065, fastDepth = 0.015, slowSpacing = 1.0 / 3.0, fastSpacing = 0.25;
    void updateRates() { incSlow = slowHz / sr; incFast = fastHz / sr; }
private:
    std::vector<float> buf;
    int mask = 0, writePos = 0;
    double sr = 48000.0, phSlow = 0.0, phFast = 0.0, incSlow = 0.0, incFast = 0.0;
    std::array<circuit::Biquad, 3> outFilter; // filtro d'uscita Sallen-Key di ogni P551
    float read (double delaySamples) const;
};

class Engine
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParams (const Params& p) { params = p; }

    void noteOn (int midiNote);
    void noteOff (int midiNote);
    void allNotesOff();
    void setPitchBend (float semitones) { bendSemis = semitones; }
    void setModWheel (float v01)        { modWheel = v01; }
    void setSustainPedal (bool down);

    // uscite come il pannello posteriore: MAIN, BRASS (jack a interruttore), SIGNAL (EXTERNAL OUT)
    struct Outputs
    {
        float* mainL = nullptr; float* mainR = nullptr;
        float* brassL = nullptr; float* brassR = nullptr; // se nulli, il brass va nel MAIN
        float* signal = nullptr;
    };
    void process (const Outputs& out, int numSamples);
    void process (float* L, float* R, int numSamples) { Outputs o; o.mainL = L; o.mainR = R; process (o, numSamples); }

    // accesso per gli strumenti di misura in tools/ (render_note)
    Ensemble& ensembleForTest() { return ensemble; }
    double brassBaseHz = 100.0; // taratura di RV1 (FINE RANGE)
    // volt in ingresso al ladder per volt sul bus brass. Dallo schema sarebbe 0,272 (R13 150K / R8 56K), ma i
    // campioni AIFF mostrano armoniche pari intatte a filtro aperto: il ladder lavora quasi lineare.
    double brassDrive = 0.06;
    // LED del pannello MODULATION: uscita del comparatore IC5 (onda quadra)
    float getLfoMonitor() const { return lfoMonitor.load (std::memory_order_relaxed); }

private:
    struct Key
    {
        int cls = 0, k8 = 0, heldCount = 0;
        bool pedalHeld = false;
        float level = 0.0f;   // tensione del condensatore 4,7µ del tasto (relativa)
        float aAtt = 0.0f;    // costante d'attacco R5..R9 x 4,7µ (tabella P554)
        bool held() const { return heldCount > 0 || pedalHeld; }
    };

    void refreshHeldCount();
    static int foldNote (int note);

    Params params;
    double sr = 48000.0;
    std::array<Key, kNumKeys> keys {};
    int heldKeys = 0;
    bool prevGate = false, pedalDown = false, retrigPending = false;
    int retrigHold = 0, silentSamples = 0;

    std::array<double, 12> topFreq {}, counter {};
    std::array<std::array<float, kNumOctaves>, 12> sq {};
    std::array<bool, 12> classActive {};

    // crescendo (P559): tensione su C14 47µ
    double vC14 = 0.0;
    // LFO (P556)
    double lfoPhase = 0.25, delayGain = 0.0; int delayReset = 0;
    float bendSemis = 0.0f, modWheel = 0.0f;
    float gStr = 0, gBrass = 0, gMaster = 0, aSmooth = 0;

    // catene
    circuit::OnePole bus8, bus16, busBrass;
    circuit::StringsChain strings;
    int osFactor = 1;
    double lastV8 = 0, lastV16 = 0;
    Ensemble ensemble;
    circuit::BrassEnvelope brassEnv;
    circuit::Ladder ladder;
    std::atomic<float> lfoMonitor { 0.0f };
};
} // namespace perf
