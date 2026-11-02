#include "PerformerEngine.h"
#include <algorithm>
#include <bit>

namespace perf
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

// MK50240: divisori per Do..Si
constexpr int kDivisors[12] = { 239, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253 };

// pesi della scala di resistenze R1..R4 = 22K, 39K, 82K, 150K (P554)
constexpr float kW[4] = { 1.0f, 22.0f / 39.0f, 22.0f / 82.0f, 22.0f / 150.0f };

// P554, tabella R5..R9 in serie ai condensatori 4,7µ dei tasti: R7 (terza ottava) varia con la nota
constexpr double kR7[12] = { 1e3, 1e3, 1e3, 1e3, 1e3, 1.2e3, 1.5e3, 1.8e3, 2.2e3, 2.7e3, 3.3e3, 3.9e3 };

constexpr double kInvPow2[10] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625, 0.0078125, 0.00390625, 0.001953125 };

inline float coef (double tauSec, double sr) { return (float) (1.0 - std::exp (-1.0 / (std::max (tauSec, 1.0e-6) * sr))); }

inline float polyBlep (double t, double dt)
{
    if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.0f;
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
    updateRates();
    // filtro d'uscita di ogni P551: Sallen-Key R24/R26 22K, C15 3n3, C13 220p → 8,5 kHz, Q 1,94
    for (auto& f : outFilter) f.setLowpass (8490.0, 1.94, sr);
    reset();
}

void Ensemble::Rotor::set (double inc, double spacing)
{
    cr = std::cos (2.0 * kPi * inc);
    ci = std::sin (2.0 * kPi * inc);
    for (int i = 0; i < 3; ++i)
    {
        sr3[(size_t) i] = std::cos (2.0 * kPi * i * spacing);
        si3[(size_t) i] = std::sin (2.0 * kPi * i * spacing);
    }
}

void Ensemble::Rotor::step()
{
    const double nr = re * cr - im * ci;
    const double ni = re * ci + im * cr;
    // rinormalizzazione al primo ordine: mantiene |z| = 1 senza sqrt
    const double g = 1.5 - 0.5 * (nr * nr + ni * ni);
    re = nr * g; im = ni * g;
}

void Ensemble::updateRates()
{
    slow.set (slowHz / sr, slowSpacing);
    fast.set (fastHz / sr, fastSpacing);
}

void Ensemble::reset()
{
    for (auto& l : lines) { l.cells.fill (0.0f); l.w = 0; l.held = l.pending = l.nextCorr = 0.0f; }
    // fasi di clock iniziali diverse (i multivibratori sono indipendenti)
    lines[0].phase = 0.0; lines[1].phase = 0.37; lines[2].phase = 0.71;
    hist.fill (0.0f);
    idle = 0;
    for (auto& f : outFilter) f.reset();
}

float Ensemble::white()
{
    // xorshift32 → uniforme in [-1, 1); somma di due uniformi: distribuzione triangolare, varianza 2/3
    auto u = [this] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (int32_t) rng * (1.0f / 2147483648.0f); };
    return (u() + u()) * 1.2247449f; // varianza 1
}

void Ensemble::process (float x, float& outL, float& outR, bool stereo)
{
    slow.step(); fast.step();
    // ingresso nullo da più della lunghezza delle linee + coda dei filtri: uscita nulla, niente calcoli
    // (non con il fruscio attivo: le BBD frusciano anche senza segnale)
    const int idleLimit = (int) (0.06 * sr);
    if (! circuit::same (x, 0.0) || noise) idle = 0;
    else if (idle <= idleLimit && ++idle > idleLimit)
    {
        for (auto& l : lines) { l.cells.fill (0.0f); l.held = l.pending = l.nextCorr = 0.0f; }
        hist.fill (0.0f);
        for (auto& f : outFilter) f.reset();
    }
    if (idle > idleLimit) { outL = outR = 0.0f; return; }

    hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = hist[3]; hist[3] = x;

    float o[3];
    for (int i = 0; i < 3; ++i)
    {
        auto& l = lines[(size_t) i];
        // la tensione di modulazione allunga il periodo del clock: ritardo = celle / clock
        const double m = (1.0 + kDepthTol[i]) * (slowDepth * slow.sinAt (i) + fastDepth * fast.sinAt (i));
        const double inc = kClockHz * (1.0 + kClockTol[i]) / (1.0 + m) / sr;
        l.phase += inc;
        while (l.phase >= 1.0)
        {
            l.phase -= 1.0;
            const float d = (float) (l.phase / inc); // istante del fronte di clock, in campioni prima di "adesso"
            // campionamento dell'ingresso a quell'istante (Hermite, con un campione di latenza)
            const float t = 1.0f - d;
            const float xm1 = hist[0], x0 = hist[1], x1 = hist[2], x2 = hist[3];
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            const float in = ((c3 * t + c2) * t + c1) * t + x0;
            float outCell = l.cells[(size_t) ((l.w - kCells) & 127)];
            if (noise) outCell += noiseLevel * white(); // rumore dei trasferimenti di carica: un valore per fronte
            l.cells[(size_t) l.w] = in;
            l.w = (l.w + 1) & 127;
            // gradino del sample & hold, band-limited con polyBLEP a due campioni
            const float step = outCell - l.held;
            l.held = outCell;
            l.pending += step * 0.5f * d * d;
            l.nextCorr -= step * 0.5f * (1.0f - d) * (1.0f - d);
        }
        float y = l.pending;                 // campione precedente, ora completo
        l.pending = l.held + l.nextCorr;     // campione attuale (riceverà la correzione del prossimo fronte)
        l.nextCorr = 0.0f;
        o[i] = (float) outFilter[(size_t) i].process (y);
    }

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
    aDrift = coef (1.0 / (2.0 * kPi * 0.08), sr);
    reset();
}

void Engine::reset()
{
    for (auto& k : keys) { k.heldCount = 0; k.pedalHeld = false; k.level = 0.0f; }
    heldKeys = 0; prevGate = false; retrigPending = false; brassWasOn = false;
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

void Engine::process (const Outputs& out, int n)
{
    float* L = out.mainL; float* R = out.mainR;
    const bool separateBrass = out.brassL != nullptr;
    const auto& p = params;
    const double dt = 1.0 / sr, invSr = 1.0 / sr;

    // --- coefficienti dipendenti dai comandi
    strings.setEq (p.stringsLow, p.stringsMid, p.stringsHigh);
    ensemble.noise = p.bbdNoise;
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

        // ---- oscillatore master (con l'opzione di deriva lenta)
        double driftCents = 0.0;
        if (p.pitchDrift)
        {
            driftRng ^= driftRng << 13; driftRng ^= driftRng >> 17; driftRng ^= driftRng << 5;
            const double wn = (double) (int32_t) driftRng * (1.0 / 2147483648.0);
            drift1 += (wn - drift1) * aDrift;
            drift2 += (drift1 - drift2) * aDrift;
            driftCents = kDriftScale * drift2 * std::sqrt (sr / 48000.0);
        }
        const double ratio = std::exp2 ((p.pitchSemitones + bendSemis + 0.01 * driftCents) / 12.0) * fmRatio;

        // ---- crescendo / buss bar
        if (gate) vC14 = std::min (6.05, vC14 + crescRate * dt);
        else      vC14 -= vC14 * crescDecay;
        const double e0 = 2.7 + 0.582 * vC14;                        // IC4 (P559), offset da RV1
        const float busLevel = p.brassOn ? 1.0f : (float) ((e0 - 0.6) / 5.6); // brass ON: buss bar a +6,2 V
        const double stringsVca = p.brassOn ? vC14 / 6.05 : 1.0;    // 3080 IC6 con brass ON

        // ---- condensatori dei tasti; per ogni classe di nota si annotano le ottave del divisore richieste
        const bool use8 = p.strings8, use16 = p.strings16, useBr = p.brassOn || out.signal != nullptr;
        classOctaves.fill (0u);
        bool anyKey = false;
        for (auto& k : keys)
        {
            if (k.held() && busLevel > k.level) k.level += (busLevel - k.level) * k.aAtt;
            else if (k.level > 0.0f)
            {
                k.level -= k.level * aRel;
                if (k.level < 1.0e-5f) k.level = 0.0f;
            }
            if (k.level > 0.0f)
            {
                const int bs = std::clamp (k.k8 - p.stringsTranspose, 2, kNumOctaves - 2);
                uint32_t m = 0;
                if (use8)  m |= 0x7u << (bs - 2);
                if (use16) m |= 0xFu << (bs - 2);
                if (useBr) m |= 0xFu << (k.k8 - 2);
                classOctaves[(size_t) k.cls] |= m;
                anyKey = true;
            }
        }

        // ---- divisori (solo le uscite effettivamente usate)
        for (int c = 0; c < 12; ++c)
        {
            const double inc = topFreq[(size_t) c] * ratio * invSr;
            double& T = counter[(size_t) c];
            T += inc;
            if (T >= 1024.0) T -= 1024.0;
            uint32_t m = classOctaves[(size_t) c];
            auto& oct = sq[(size_t) c];
            while (m != 0u)
            {
                const int o = std::countr_zero (m);
                m &= m - 1u;
                const double x = T * kInvPow2[o];
                oct[(size_t) o] = squareBlep (x - std::floor (x), std::min (inc * kInvPow2[o], 0.5));
            }
        }

        // ---- uscite TDA1008 (±2,5 V per tasto a buss bar pieno) sommate dalla scala resistiva
        double s8 = 0.0, s16 = 0.0, sBr = 0.0;
        if (anyKey)
            for (auto& k : keys)
            {
                if (k.level <= 0.0f) continue;
                const auto& o = sq[(size_t) k.cls];
                const int b = k.k8;
                // TRANSPOSE (versione B): gli archi prendono le uscite del divisore un'ottava sopra o sotto;
                // il brass resta sul 16' normale
                const int bs = std::clamp (b - p.stringsTranspose, 2, kNumOctaves - 2);
                // onde quadre al 50% come da schema (confermato dai campioni AIFF: h16/h32 assenti)
                if (use8)
                    s8 += k.level * (kW[0] * o[(size_t) bs] + kW[1] * o[(size_t) (bs - 1)] + kW[2] * o[(size_t) (bs - 2)]);
                if (use16)
                    s16 += k.level * (kW[0] * o[(size_t) (bs + 1)] + kW[1] * o[(size_t) bs] + kW[2] * o[(size_t) (bs - 1)] + kW[3] * o[(size_t) (bs - 2)]);
                if (useBr)
                    sBr += k.level * (kW[0] * o[(size_t) (b + 1)] + kW[1] * o[(size_t) b] + kW[2] * o[(size_t) (b - 1)] + kW[3] * o[(size_t) (b - 2)]);
            }
        const double v8 = bus8.process (2.5 * s8);
        const double v16 = bus16.process (2.5 * s16);
        const double vBr = busBrass.process (2.5 * (8.2 / 22.0) * sBr);

        // ---- catena archi (P559 + ingresso P551), sovracampionata se serve
        // la catena archi si ferma dopo 0,3 s di silenzio (le sue costanti di tempo sono di pochi ms)
        silentSamples = anyKey ? 0 : silentSamples + 1;
        double vStr = 0.0;
        if (silentSamples < (int) (0.3 * sr))
        {
            const double i8 = p.strings8 ? v8 : 0.0, i16 = p.strings16 ? v16 : 0.0;
            if (osFactor == 2)
            {
                vStr = 0.5 * (strings.process (0.5 * (i8 + lastV8), 0.5 * (i16 + lastV16)) + strings.process (i8, i16));
            }
            else vStr = strings.process (i8, i16);
            lastV8 = i8; lastV16 = i16;
        }
        else if (silentSamples == (int) (0.3 * sr)) { strings.reset(); lastV8 = lastV16 = 0.0; }
        float eL, eR;
        ensemble.process ((float) (vStr * stringsVca), eL, eR, p.stereoEnsemble);

        // ---- BRASS: inviluppo a circuito, convertitore esponenziale TR13/TR14, ladder
        // l'inviluppo gira anche con BRASS spento, come nell'originale: riaccendendo il brass si
        // ritrova lo stato lasciato dalle note precedenti (C18 si scarica in tempi fino a ~1 s)
        const double vSum = brassEnv.process (brassGate, p.brassDecay, p.brassAttack, p.brassRange, modVcf);
        double brassOut = 0.0;
        if (p.brassOn)
        {
            // RV1 (FINE RANGE) è un trimmer: taratura scelta = 100 Hz con RANGE a 0 e inviluppo a riposo
            const double fc = brassBaseHz * std::exp (vSum / circuit::kVT);
            brassOut = ladder.process (vBr * brassDrive, fc, p.brassResonance) * (0.272 / brassDrive);
        }
        else if (brassWasOn) ladder.reset();
        brassWasOn = p.brassOn;

        // ---- mixer IC3 (P559) e volume master
        gStr    += (float) ((tStr - gStr) * aSmooth);
        gBrass  += (float) ((tBrass - gBrass) * aSmooth);
        gMaster += (float) ((tMaster - gMaster) * aSmooth);
        // guadagno dello stadio d'uscita TR11/TR12 non ricavabile dallo schema: tarato per pareggiare gli archi
        const double b = brassOut * gBrass * (6.8 / 27.0) * 6.5;
        // jack BRASS a interruttore: se usato, il brass esce solo lì (prima del volume master, come l'originale)
        const double bMain = separateBrass ? 0.0 : b;
        const double oL = rails ((eL * gStr * 1.6 + bMain)) * gMaster * 0.1;
        const double oR = rails ((eR * gStr * 1.6 + bMain)) * gMaster * 0.1;
        if (R != nullptr) { L[s] = (float) oL; R[s] = (float) oR; }
        else L[s] = (float) (0.5 * (oL + oR));
        if (separateBrass)
        {
            const float ob = (float) (rails (b) * 0.1);
            out.brassL[s] = ob;
            if (out.brassR != nullptr) out.brassR[s] = ob;
        }
        // jack SIGNAL (EXTERNAL OUT, R14 4K7 dall'uscita di IC14): bus brass grezzo, senza filtro
        if (out.signal != nullptr) out.signal[s] = (float) (rails (vBr) * 0.1);
    }
    lfoMonitor.store (lfoPhase < 0.5 ? 0.0f : 1.0f, std::memory_order_relaxed); // comparatore IC5
}
} // namespace perf
