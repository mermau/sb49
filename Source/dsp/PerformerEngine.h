// SB-49 — emulazione del motore di un string/brass machine anni '70 (divide-down paraphonic)
// Motore DSP indipendente da JUCE: compilabile e testabile da solo.
#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <atomic>

namespace perf
{
constexpr int    kNumKeys     = 49;        // tastiera 49 tasti, Do-Do (P558)
constexpr int    kLowestNote  = 36;        // C2 = primo tasto (8')
constexpr double kMasterClock = 2000240.0; // quarzo/oscillatore master 2.00024 MHz (P555)
constexpr int    kNumOctaves  = 10;        // uscite divisore considerate per classe di nota

struct Params
{
    // Sezione BRASS (P557)
    float brassAttack = 0.15f, brassDecay = 0.6f, brassRange = 0.65f, brassResonance = 0.25f, brassVolume = 0.7f;
    bool  brassOn = false;
    // Sezione STRINGS (P554/P559)
    float stringsLow = 0.5f, stringsMid = 0.6f, stringsHigh = 0.5f;
    bool  strings16 = false, strings8 = true;
    float crescendo = 0.15f, sustain = 0.35f, stringsVolume = 0.75f;
    // Sezione MODULATION (P556)
    float modDelay = 0.3f, modRate = 0.45f, modDepth = 0.0f;
    bool  modToVcf = false;   // true = V.C.F., false = F.M.
    // Master / pannello posteriore
    float masterVolume = 0.8f;
    float pitchSemitones = 0.0f;   // PITCH posteriore, +/- 15 semitoni (2,5 ottave)
    bool  stereoEnsemble = false;  // estensione: ensemble sparso in stereo (l'originale è mono)
};

// --- piccoli blocchi DSP ------------------------------------------------------------
struct Svf // TPT state-variable filter (Zavalishin)
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;
    void set (double fc, double q, double sr);
    void reset() { ic1 = ic2 = 0; }
    inline void process (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
};

// Ladder a transistor 4 poli (TR2..TR10, P557) — modello ZDF con saturazione, sovracampionato 2x
struct Ladder
{
    float s[4] {};
    float lastIn = 0.0f;
    double srOs = 88200.0;
    void prepare (double sr) { srOs = sr * 2.0; reset(); }
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; lastIn = 0.0f; }
    float process (float x, float cutoffHz, float resonance);
private:
    float tick (float x, float G, float k);
};

// Tre linee di ritardo analogiche (TCA350, P551) modulate dall'LFO a due velocità (P550)
struct Ensemble
{
    void prepare (double sr);
    void reset();
    void process (float x, float& outL, float& outR, bool stereo);
private:
    std::vector<float> buf;
    int mask = 0, writePos = 0;
    double sr = 48000.0, phSlow = 0.0, phFast = 0.0, incSlow = 0.0, incFast = 0.0;
    Svf inputLp;
    float outLp[3] {}, outLpCoef = 0.5f;
    float read (double delaySamples) const;
};

// --- motore ----------------------------------------------------------------------------
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

    // scrive (non somma) numSamples campioni in L e R (R può essere nullptr per uscita mono)
    void process (float* L, float* R, int numSamples);

    float getLfoMonitor() const { return lfoMonitor.load (std::memory_order_relaxed); }

private:
    struct Key
    {
        int cls = 0;          // classe di nota (0 = Do ... 11 = Si)
        int k8 = 0;           // indice dell'uscita del divisore che dà la fondamentale a 8'
        int heldCount = 0;    // tasti MIDI che puntano a questo tasto (note ripiegate)
        bool pedalHeld = false;
        float level = 0.0f;   // tensione sul pin di ingresso TDA1008 normalizzata 0..1
        bool held() const { return heldCount > 0 || pedalHeld; }
    };

    void updateCoefficients();
    void refreshHeldCount();
    static int foldNote (int note);

    Params params;
    double sr = 48000.0;
    std::array<Key, kNumKeys> keys {};
    int heldKeys = 0;
    bool prevGate = false, pedalDown = false;

    // divisori: contatore per classe di nota (in cicli dell'uscita top del MK50240)
    std::array<double, 12> topFreq {}, counter {};
    std::array<std::array<float, kNumOctaves>, 12> sq {};
    std::array<bool, 12> classActive {};

    // inviluppi
    float bus = 0.0f;           // "common rod"/buss bar (crescendo con brass OFF)
    float stringsVca = 1.0f;    // 3080 crescendo con brass ON
    float brassNode = 0.0f, brassEnv = 0.0f; // nodo D1-D2-C18 e tensione su C2 (P557)
    float delayEnv = 0.0f; int delayHold = 0;
    double lfoPhase = 0.0;
    float bendSemis = 0.0f, modWheel = 0.0f;

    // coefficienti
    float aKeyAtt = 0, aKeyRel = 0, aBus = 0, aBusOff = 0, aVcaRel = 0;
    float aBrAtt = 0, aBrDecay = 0, aBrFall = 0, aDelay = 0, aSmooth = 0;
    double lfoInc = 0.0;

    // catena
    Svf eqLow, eqMid, eqHigh;
    Ensemble ensemble;
    Ladder ladder;
    float gStr = 0, gBrass = 0, gMaster = 0;
    std::atomic<float> lfoMonitor { 0.0f };
};
} // namespace perf
