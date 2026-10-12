#include "PerformerEngine.h"
#include <algorithm>

namespace perf
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

// MK50240 top-octave divider: divisori per Do..Si (Do da 239 = C9, gli altri C#8..B8)
constexpr int kDivisors[12] = { 239, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253 };

// pesi della scala di resistenze R1..R4 = 22K, 39K, 82K, 150K (P554): onda a gradini ~ dente di sega
constexpr float kW[4] = { 1.0f, 22.0f / 39.0f, 22.0f / 82.0f, 22.0f / 150.0f };

inline float coef (double tauSec, double sr) { return (float) (1.0 - std::exp (-1.0 / (std::max (tauSec, 1.0e-5) * sr))); }

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

inline float softClip (float x) // tanh razionale
{
    x = std::clamp (x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}
} // namespace

// ---------------------------------------------------------------------------------------
void Svf::set (double fc, double q, double sr)
{
    const double g = std::tan (kPi * std::min (fc, sr * 0.45) / sr);
    k = (float) (1.0 / q);
    a1 = (float) (1.0 / (1.0 + g * (g + k)));
    a2 = (float) g * a1;
    a3 = (float) g * a2;
}

// ---------------------------------------------------------------------------------------
float Ladder::tick (float x, float G, float k)
{
    const float b0 = (1.0f - G) * s[0], b1 = (1.0f - G) * s[1], b2 = (1.0f - G) * s[2], b3 = (1.0f - G) * s[3];
    const float G2 = G * G, G4 = G2 * G2;
    const float S = G2 * G * b0 + G2 * b1 + G * b2 + b3;
    const float y4 = (G4 * x + S) / (1.0f + k * G4);
    float u = softClip (x * (1.0f + 0.5f * k) - k * y4);
    for (int i = 0; i < 4; ++i)
    {
        const float v = (u - s[i]) * G;
        const float y = v + s[i];
        s[i] = y + v;
        u = y;
    }
    return u;
}

float Ladder::process (float x, float cutoffHz, float resonance)
{
    cutoffHz = std::clamp (cutoffHz, 10.0f, (float) std::min (20000.0, srOs * 0.22));
    const float g = (float) std::tan (kPi * cutoffHz / srOs);
    const float G = g / (1.0f + g);
    const float k = std::clamp (resonance, 0.0f, 1.0f) * 3.9f;
    const float mid = 0.5f * (lastIn + x);
    lastIn = x;
    const float y0 = tick (mid, G, k);
    const float y1 = tick (x, G, k);
    return 0.5f * (y0 + y1);
}

// ---------------------------------------------------------------------------------------
void Ensemble::prepare (double sampleRate)
{
    sr = sampleRate;
    int size = 1;
    while (size < (int) (sr * 0.04)) size <<= 1;
    buf.assign ((size_t) size, 0.0f);
    mask = size - 1;
    incSlow = 0.63 / sr;  // LFO lento (chorus)
    incFast = 6.1 / sr;   // LFO veloce (vibrato/"shimmer")
    inputLp.set (9000.0, 0.707, sr);
    outLpCoef = coef (1.0 / (2.0 * kPi * 10000.0), sr);
    reset();
}

void Ensemble::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    writePos = 0; inputLp.reset();
    outLp[0] = outLp[1] = outLp[2] = 0.0f;
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
    float lp, bp, hp;
    inputLp.process (x, lp, bp, hp);
    buf[(size_t) writePos] = lp;

    phSlow += incSlow; if (phSlow >= 1.0) phSlow -= 1.0;
    phFast += incFast; if (phFast >= 1.0) phFast -= 1.0;

    float o[3];
    for (int i = 0; i < 3; ++i)
    {
        const double off = i / 3.0; // tre fasi a 120°
        const double ms = 5.5 + 1.7 * std::sin (2.0 * kPi * (phSlow + off))
                              + 0.22 * std::sin (2.0 * kPi * (phFast + off));
        const float y = read (ms * 0.001 * sr);
        outLp[i] += (y - outLp[i]) * outLpCoef;
        o[i] = outLp[i];
    }
    writePos = (writePos + 1) & mask;

    if (stereo)
    {
        outL = 0.62f * o[0] + 0.38f * o[1];
        outR = 0.62f * o[2] + 0.38f * o[1];
    }
    else
    {
        outL = outR = (o[0] + o[1] + o[2]) * (1.0f / 3.0f);
    }
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
    }
    ensemble.prepare (sr);
    ladder.prepare (sr);
    reset();
}

void Engine::reset()
{
    for (auto& k : keys) { k.heldCount = 0; k.pedalHeld = false; k.level = 0.0f; }
    heldKeys = 0; prevGate = false;
    counter.fill (0.0);
    bus = 0.0f; stringsVca = 0.0f; brassNode = brassEnv = 0.0f; delayEnv = 0.0f; delayHold = 0;
    eqLow.reset(); eqMid.reset(); eqHigh.reset();
    ensemble.reset(); ladder.reset();
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
    // RETRIG (estensione): ogni nuovo tasto riarma l'inviluppo brass anche suonando legato.
    // Nell'originale il gate è comune ai 49 tasti, quindi l'attacco parte solo dal primo.
    if (params.brassRetrig && heldKeys > 0)
        brassNode = 1.0f;
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

void Engine::updateCoefficients()
{
    const auto& p = params;
    aKeyAtt = coef (0.0015, sr);
    // SUSTAIN (pin 7 TDA1008): disabilitato con BRASS inserito
    aKeyRel = p.brassOn ? coef (0.004, sr) : coef (0.004 * std::pow (1000.0, p.sustain), sr);
    // CRESCENDO: salita del buss bar da +2V a +6,2V (brass OFF) o del 3080 (brass ON)
    aBus    = coef (0.002 * std::pow (1500.0, p.crescendo), sr);
    aBusOff = coef (0.003, sr);
    aVcaRel = coef (0.010, sr);
    // BRASS: P5 ATTACK 470K log su C2 4,7µ; P1 DECAY 470K log su C18 2,2µ; scarica C2 via R28+R32
    aBrAtt   = coef (0.001 * std::pow (2000.0, p.brassAttack), sr);
    aBrDecay = coef (0.02 * std::pow (200.0, p.brassDecay), sr);
    aBrFall  = coef (0.062, sr);
    // MODULATION: delay (P1 LENGHT DELAY), rate (P2 1M antilog)
    aDelay = coef (0.005 * std::pow (600.0, p.modDelay), sr);
    lfoInc = 0.25 * std::pow (60.0, (double) p.modRate) / sr;
    aSmooth = coef (0.02, sr);
    // EQ a tre bande (P559)
    eqLow.set (280.0, 0.707, sr);
    eqMid.set (1100.0, 0.8, sr);
    eqHigh.set (3200.0, 0.707, sr);
}

void Engine::process (float* L, float* R, int n)
{
    updateCoefficients();
    const auto& p = params;
    const float tStr = p.stringsVolume * p.stringsVolume;
    const float tBrass = p.brassVolume * p.brassVolume;
    const float tMaster = p.masterVolume * p.masterVolume;
    const bool anyStrings = p.strings8 || p.strings16;

    for (int s = 0; s < n; ++s)
    {
        // ---- GATE (OR dei 49 tasti, P558) e fronti
        const bool gate = heldKeys > 0;
        if (gate && ! prevGate)
        {
            brassNode = 1.0f;          // fronte del gate differenziato da C18
            delayEnv = 0.0f;           // ritardo modulazione ripartito
            delayHold = (int) (0.0065 * sr); // impulso 6-7 ms del 4011
            if (! p.brassOn) bus = 0.0f;
        }
        if (! gate && prevGate) brassNode = 0.0f; // D2 taglia il fronte negativo
        prevGate = gate;

        // ---- LFO (P556): triangolo, delay, depth
        lfoPhase += lfoInc; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const float tri = (float) (4.0 * std::abs (lfoPhase - 0.5) - 1.0);
        if (gate)
        {
            if (delayHold > 0) --delayHold;
            else delayEnv += (1.0f - delayEnv) * aDelay;
        }
        const float depth = std::max (p.modDepth, modWheel);
        const float lfo = tri * depth * delayEnv;

        // ---- oscillatore master + pitch (+ F.M.)
        const float semis = p.pitchSemitones + bendSemis + (p.modToVcf ? 0.0f : lfo * 0.6f);
        const double ratio = std::exp2 (semis / 12.0);

        // ---- buss bar (crescendo) / VCA 3080
        if (p.brassOn)
        {
            bus = 1.0f;
            if (gate) stringsVca += (1.0f - stringsVca) * aBus;
            else      stringsVca -= stringsVca * aVcaRel;
        }
        else
        {
            stringsVca = 1.0f;
            if (gate) bus += (1.0f - bus) * aBus;
            else      bus -= bus * aBusOff;
        }

        // ---- livelli dei tasti
        classActive.fill (false);
        for (auto& k : keys)
        {
            if (k.held()) k.level += (bus - k.level) * aKeyAtt;
            else
            {
                k.level -= k.level * aKeyRel;
                if (k.level < 1.0e-5f) k.level = 0.0f;
            }
            if (k.level > 0.0f) classActive[(size_t) k.cls] = true;
        }

        // ---- divisori (contatori sempre in corsa, onde calcolate solo se servono)
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
            }
        }

        // ---- somma tasti (TDA1008 + scala resistiva)
        float strings = 0.0f, brass = 0.0f;
        for (auto& k : keys)
        {
            if (k.level <= 0.0f) continue;
            const auto& o = sq[(size_t) k.cls];
            const int b = k.k8;
            const float w8  = kW[0] * o[(size_t) b] + kW[1] * o[(size_t) (b - 1)] + kW[2] * o[(size_t) (b - 2)];
            const float w16 = kW[0] * o[(size_t) (b + 1)] + kW[1] * o[(size_t) b] + kW[2] * o[(size_t) (b - 1)] + kW[3] * o[(size_t) (b - 2)];
            if (anyStrings) strings += k.level * ((p.strings8 ? w8 : 0.0f) + (p.strings16 ? w16 : 0.0f));
            if (p.brassOn)  brass += k.level * w16;
        }

        // ---- catena STRINGS: VCA 3080 -> EQ -> linee di ritardo
        strings *= 0.09f * stringsVca;
        float lp, bp, hp, t0, t1;
        eqLow.process (strings, lp, t0, t1);
        eqMid.process (strings, t0, bp, t1);
        eqHigh.process (strings, t0, t1, hp);
        const float eq = 0.3f * strings
                       + 1.5f * (p.stringsLow * lp + p.stringsMid * bp * eqMid.k + p.stringsHigh * hp);
        float sL, sR;
        ensemble.process (eq, sL, sR, p.stereoEnsemble);

        // ---- catena BRASS: inviluppo AD sul VCF a ladder
        brassNode -= brassNode * aBrDecay;
        if (brassNode > brassEnv) brassEnv += (brassNode - brassEnv) * aBrAtt;
        else                      brassEnv += (brassNode - brassEnv) * aBrFall;
        float brassOut = 0.0f;
        if (p.brassOn)
        {
            const float vcfLfo = p.modToVcf ? lfo * 2.5f : 0.0f;
            // P557: su TR13 si sommano RANGE (R30), inviluppo da C2 (R28) e modulazione (R31).
            // RANGE fissa l'apertura di base, l'inviluppo la porta più in alto.
            const float oct = 1.6f + 7.0f * p.brassRange + 3.6f * brassEnv + vcfLfo;
            brassOut = ladder.process (brass * 0.22f, 20.0f * std::exp2 (oct), p.brassResonance) * 1.6f;
        }
        else ladder.reset();

        // ---- mix finale (P559 IC3) e master
        gStr    += (tStr - gStr) * aSmooth;
        gBrass  += (tBrass - gBrass) * aSmooth;
        gMaster += (tMaster - gMaster) * aSmooth;
        const float b = brassOut * gBrass;
        // saturazione morbida dello stadio finale: trasparente sotto ~0,5, evita clip duri
        const float outL = softClip ((sL * gStr + b) * gMaster);
        const float outR = softClip ((sR * gStr + b) * gMaster);
        if (R != nullptr) { L[s] = outL; R[s] = outR; }
        else L[s] = 0.5f * (outL + outR);
    }
    lfoMonitor.store (0.5f + 0.5f * (float) (4.0 * std::abs (lfoPhase - 0.5) - 1.0), std::memory_order_relaxed);
}
} // namespace perf
