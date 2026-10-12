#include "PerformerEngine.h"
#include <algorithm>

namespace perf
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

// MK50240: divisori per Do..Si
constexpr int kDivisors[12] = { 239, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253 };

// pesi della scala di resistenze R1..R4 = 22K, 39K, 82K, 150K (P554)
constexpr double kTopDuty = 0.5; // onda quadra come da schema (confermato dai campioni AIFF: h16/h32 assenti)
constexpr float kW[4] = { 1.0f, 22.0f / 39.0f, 22.0f / 82.0f, 22.0f / 150.0f };

// P554, tabella R5..R9 in serie ai condensatori 4,7µ dei tasti: R7 (terza ottava) varia con la nota
constexpr double kR7[12] = { 1e3, 1e3, 1e3, 1e3, 1e3, 1.2e3, 1.5e3, 1.8e3, 2.2e3, 2.7e3, 3.3e3, 3.9e3 };

inline float coef (double tauSec, double sr) { return (float) (1.0 - std::exp (-1.0 / (std::max (tauSec, 1.0e-6) * sr))); }

inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
}

// impulso unipolare 0..2 con duty d, a media nulla (accoppiamento capacitivo), band-limited
inline float pulseBlep (double phase, double dt, double d)
{
    float v = (phase < d ? 2.0f : 0.0f) - (float) (2.0 * d);
    v += polyBlep (phase, dt);
    double p2 = phase - d; if (p2 < 0.0) p2 += 1.0;
    v -= polyBlep (p2, dt);
    return v;
}

inline float squareBlep (double phase, double dt)
{
    float v = phase < 0.5 ? 1.0f : -1.0f;
    v += polyBlep (phase, dt);
    double p2 = phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;
    v -= polyBlep (p2, dt);
    return v;
}

// limite morbido delle uscite degli operazionali (1458 a ±12 V: circa ±10,5 V)
inline double rails (double v) { return 10.5 * std::tanh (v / 10.5); }

// potenziometro lineare caricato da RL
inline double loadedPot (double x, double Rp, double RL) { return x * RL / (RL + x * (1.0 - x) * Rp); }
} // namespace

// ---------------------------------------------------------------------------------------
void Ensemble::prepare (double sampleRate)
{
    sr = sampleRate;
    int size = 1;
    while (size < (int) (sr * 0.02)) size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    mask = size - 1;
    // LFO a due velocità della scheda P550: velocità e profondità tarate sui campioni originali
    updateRates();
    // filtro d'uscita di ogni P551: Sallen-Key R24/R26 22K, C15 3n3, C13 220p → 8,5 kHz, Q 1,94
    for (auto& f : outFilter) f.setLowpass (8490.0, 1.94, sr);
    reset();
}

void Ensemble::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writePos = 0;
    for (auto& f : outFilter) f.reset();
}

float Ensemble::read (double d) const
{
    const double rp = (double) writePos - d;
    const double fl = std::floor (rp);
    const float t = (float) (rp - fl);
    const int i = (int) fl;
    const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)];
    const float x1 = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void Ensemble::process (float x, float& outL, float& outR, bool stereo)
{
    buf[(size_t) writePos] = x;
    phSlow += incSlow; if (phSlow >= 1.0) phSlow -= 1.0;
    phFast += incFast; if (phFast >= 1.0) phFast -= 1.0;

    // TCA350: 185 stadi, clock nominale 22,6 kHz (multivibratore 2 x 39K / 820p) → 4,09 ms
    constexpr double centerMs = 185.0 / (2.0 * 22.6) ;
    float o[3];
    for (int i = 0; i < 3; ++i)
    {
        // uscite L1, L2, L3 della P550: fasi diverse dei due oscillatori
        const double ms = centerMs * (1.0 + slowDepth * std::sin (2.0 * kPi * (phSlow + i * slowSpacing))
                                          + fastDepth * std::sin (2.0 * kPi * (phFast + i * fastSpacing)));
        o[i] = (float) outFilter[(size_t) i].process (read (ms * 0.001 * sr));
    }
    writePos = (writePos + 1) & mask;

    if (stereo)
    {
        outL = 0.62f * o[0] + 0.38f * o[1];
        outR = 0.62f * o[2] + 0.38f * o[1];
    }
    else
        outL = outR = (o[0] + o[1] + o[2]) * (1.0f / 3.0f);
}

// ---------------------------------------------------------------------------------------
int Engine::foldNote (int n)
{
    while (n < kLowestNote) n += 12;
    while (n > kLowestNote + kNumKeys - 1) n -= 12;
    return n;
}

void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    for (int c = 0; c < 12; ++c)
        topFreq[(size_t) c] = kMasterClock / kDivisors[c];

    for (int i = 0; i < kNumKeys; ++i)
    {
        const int note = kLowestNote + i;
        auto& k = keys[(size_t) i];
        k.cls = note % 12;
        const double fNom = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        k.k8 = (int) std::lround (std::log2 (topFreq[(size_t) k.cls] / fNom));
        const int octave = i / 12; // 0 = ottava più bassa
        const double R = octave <= 1 ? 1e3 : (octave == 2 ? kR7[k.cls] : 4.7e3);
        k.aAtt = coef (R * 4.7e-6, sr);
    }

    // sommatori IC13 (22K || 2,2 nF) e IC14 (8K2 || 10 nF) della P554
    bus8.set (1.0 / (2.0 * kPi * 22e3 * 2.2e-9), sr);
    bus16.set (1.0 / (2.0 * kPi * 22e3 * 2.2e-9), sr);
    busBrass.set (1.0 / (2.0 * kPi * 8.2e3 * 10e-9), sr);

    osFactor = sr < 80000.0 ? 2 : 1;
    strings.prepare (sr * osFactor);
    ensemble.prepare (sr);
    brassEnv.prepare (sr);
    ladder.prepare (sr);
    aSmooth = coef (0.02, sr);
    reset();
}

void Engine::reset()
{
    for (auto& k : keys) { k.heldCount = 0; k.pedalHeld = false; k.level = 0.0f; }
    heldKeys = 0; prevGate = false; retrigPending = false;
    counter.fill (0.0);
    vC14 = 0.0; delayGain = 0.0; delayReset = 0;
    bus8.reset(); bus16.reset(); busBrass.reset();
    strings.reset(); lastV8 = lastV16 = 0.0;
    ensemble.reset(); brassEnv.reset(); ladder.reset();
    gStr = gBrass = gMaster = 0.0f;
}

void Engine::refreshHeldCount()
{
    heldKeys = 0;
    for (auto& k : keys) if (k.held()) ++heldKeys;
}

void Engine::noteOn (int note)
{
    auto& k = keys[(size_t) (foldNote (note) - kLowestNote)];
    if (params.brassRetrig && heldKeys > 0) retrigPending = true;
    ++k.heldCount;
    refreshHeldCount();
}

void Engine::noteOff (int note)
{
    auto& k = keys[(size_t) (foldNote (note) - kLowestNote)];
    if (k.heldCount > 0) --k.heldCount;
    if (k.heldCount == 0 && pedalDown) k.pedalHeld = true;
    refreshHeldCount();
}

void Engine::allNotesOff()
{
    for (auto& k : keys) { k.heldCount = 0; k.pedalHeld = false; }
    refreshHeldCount();
}

void Engine::setSustainPedal (bool down)
{
    pedalDown = down;
    if (! down) for (auto& k : keys) k.pedalHeld = false;
    refreshHeldCount();
}

void Engine::process (float* L, float* R, int n)
{
    const auto& p = params;
    const double dt = 1.0 / sr;

    // --- coefficienti dipendenti dai comandi
    strings.setEq (p.stringsLow, p.stringsMid, p.stringsHigh);
    // SUSTAIN: pin 7 dei TDA1008; massimo limitato da 1 MΩ x 4,7 µF = 4,7 s; escluso con BRASS
    const float aRel = p.brassOn ? coef (0.008, sr) : coef (0.008 * std::pow (4.7 / 0.008, (double) p.sustain), sr);
    // CRESCENDO: TR3 generatore di corrente (DZ1 6,2 V) → rampa lineare su C14 47µ
    const double crescRate = (12.0 - 6.85) / (1e3 + 100e3 * p.crescendo) / 47e-6; // V/s
    const double crescDecay = 1.0 - std::exp (-dt / (1e3 * 47e-6));                // scarica via R43 1K
    // LFO: integratore 22K / 940n, trigger di Schmitt 10K/68K, RATE 1M (antilog) + R20 6K8, R22 2K2
    const double rRate = 1e6 * circuit::logTaper (1.0 - p.modRate);
    const double lfoHz = 2.2e3 / (4.0 * (10.0 / 78.0) * 22e3 * 940e-9 * (6.8e3 + rRate));
    // DELAY: TR4 generatore di corrente su C3 47µ, saturazione di IC2 a Vc3 ≈ 3,47 V
    const double delayTime = 3.47 * 47e-6 * (10e3 + 100e3 * p.modDelay) / (12.0 - 9.2);
    const double tStr = loadedPot (p.stringsVolume, 100e3, 47e3);
    const double tBrass = loadedPot (p.brassVolume, 100e3, 37e3);
    const double tMaster = p.masterVolume; // P6 100K lin verso il jack

    for (int s = 0; s < n; ++s)
    {
        // ---- GATE comune (P558)
        const bool gate = heldKeys > 0;
        if (gate && ! prevGate) { delayReset = (int) (0.0069 * sr); delayGain = 0.0; } // impulso 4011 (R1 100K, C2 100n)
        prevGate = gate;
        if (retrigPending) { retrigHold = (int) (0.004 * sr); retrigPending = false; }
        bool brassGate = gate;
        if (retrigHold > 0) { --retrigHold; brassGate = false; }

        // ---- LFO (P556)
        lfoPhase += lfoHz * dt; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const double tri = 4.0 * std::abs (lfoPhase - 0.5) - 1.0;
        if (gate)
        {
            if (delayReset > 0) --delayReset;
            else delayGain = std::min (1.0, delayGain + dt / delayTime);
        }
        const double depth = std::max (p.modDepth, modWheel);          // P3 10K lin
        const double vLfo = delayGain * 4.7 * std::tanh (0.83 * tri) * depth; // 3080 + IC5 (R26 4K7)
        const double modVcf = p.modToVcf ? vLfo : 0.0;
        const double fmRatio = p.modToVcf ? 1.0 : 1.0 + 0.0499 * vLfo / 3.2;   // R13 1M sul nodo 3,2 V

        // ---- oscillatore master
        const double ratio = std::exp2 ((p.pitchSemitones + bendSemis) / 12.0) * fmRatio;

        // ---- crescendo / buss bar
        if (gate) vC14 = std::min (6.05, vC14 + crescRate * dt);
        else      vC14 -= vC14 * crescDecay;
        const double e0 = 2.7 + 0.582 * vC14;                        // IC4 (P559), offset da RV1
        const float busLevel = p.brassOn ? 1.0f : (float) ((e0 - 0.6) / 5.6); // brass ON: buss bar a +6,2 V
        const double stringsVca = p.brassOn ? vC14 / 6.05 : 1.0;    // 3080 IC6 con brass ON

        // ---- condensatori dei tasti
        classActive.fill (false);
        for (auto& k : keys)
        {
            if (k.held() && busLevel > k.level) k.level += (busLevel - k.level) * k.aAtt;
            else
            {
                k.level -= k.level * aRel;
                if (k.level < 1.0e-5f) k.level = 0.0f;
            }
            if (k.level > 0.0f) classActive[(size_t) k.cls] = true;
        }

        // ---- divisori
        for (int c = 0; c < 12; ++c)
        {
            const double inc = topFreq[(size_t) c] * ratio / sr;
            double& T = counter[(size_t) c];
            T += inc;
            if (T >= 1024.0) T -= 1024.0;
            if (! classActive[(size_t) c]) continue;
            auto& out = sq[(size_t) c];
            for (int o = 1; o < kNumOctaves; ++o)
            {
                const double div = (double) (1 << o);
                const double ph = T / div - std::floor (T / div);
                out[(size_t) o] = squareBlep (ph, std::min (inc / div, 0.5));
                pulse[(size_t) c][(size_t) o] = pulseBlep (ph, std::min (inc / div, 0.5), kTopDuty);
            }
        }

        // ---- uscite TDA1008 (±2,5 V per tasto a buss bar pieno) sommate dalla scala resistiva
        double s8 = 0.0, s16 = 0.0;
        for (auto& k : keys)
        {
            if (k.level <= 0.0f) continue;
            const auto& o = sq[(size_t) k.cls];
            const int b = k.k8;
            const float top = pulse[(size_t) k.cls][(size_t) (b - 2)];
            const float w8  = kW[0] * o[(size_t) b] + kW[1] * o[(size_t) (b - 1)] + kW[2] * top;
            const float w16 = kW[0] * o[(size_t) (b + 1)] + kW[1] * o[(size_t) b] + kW[2] * o[(size_t) (b - 1)] + kW[3] * top;
            s8 += k.level * w8;
            s16 += k.level * w16;
        }
        const double v8 = bus8.process (2.5 * s8);
        const double v16 = bus16.process (2.5 * s16);
        const double vBr = busBrass.process (2.5 * (8.2 / 22.0) * s16);

        // ---- catena archi (P559 + ingresso P551), sovracampionata se serve
        double vStr = 0.0;
        {
            const double i8 = p.strings8 ? v8 : 0.0, i16 = p.strings16 ? v16 : 0.0;
            if (osFactor == 2)
            {
                vStr = 0.5 * (strings.process (0.5 * (i8 + lastV8), 0.5 * (i16 + lastV16)) + strings.process (i8, i16));
            }
            else vStr = strings.process (i8, i16);
            lastV8 = i8; lastV16 = i16;
        }
        float eL, eR;
        ensemble.process ((float) (vStr * stringsVca), eL, eR, p.stereoEnsemble);

        // ---- BRASS: inviluppo a circuito, convertitore esponenziale TR13/TR14, ladder
        const double vSum = brassEnv.process (brassGate, p.brassDecay, p.brassAttack, p.brassRange, modVcf);
        // RV1 (FINE RANGE) è un trimmer: taratura scelta = 100 Hz con RANGE a 0 e inviluppo a riposo
        const double fc = brassBaseHz * std::exp (vSum / circuit::kVT);
        double brassOut = 0.0;
        if (p.brassOn)
            brassOut = ladder.process (vBr * brassDrive, fc, p.brassResonance) * (0.272 / brassDrive);
        else ladder.reset();

        // ---- mixer IC3 (P559) e volume master
        gStr    += (float) ((tStr - gStr) * aSmooth);
        gBrass  += (float) ((tBrass - gBrass) * aSmooth);
        gMaster += (float) ((tMaster - gMaster) * aSmooth);
        // guadagno dello stadio d'uscita TR11/TR12 non ricavabile dallo schema: tarato per pareggiare gli archi
        const double b = brassOut * gBrass * (6.8 / 27.0) * 6.5;
        const double oL = rails ((eL * gStr * 1.6 + b)) * gMaster * 0.1;
        const double oR = rails ((eR * gStr * 1.6 + b)) * gMaster * 0.1;
        if (R != nullptr) { L[s] = (float) oL; R[s] = (float) oR; }
        else L[s] = (float) (0.5 * (oL + oR));

        lfoMonitor.store (lfoPhase < 0.5 ? 0.0f : 1.0f, std::memory_order_relaxed); // comparatore IC5
    }
}
} // namespace perf
