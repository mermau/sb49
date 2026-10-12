// Test offline del motore: renderizza alcune frasi in WAV e verifica livelli/NaN.
#include "../Source/dsp/PerformerEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lrint (std::clamp (L[i], -1.0f, 1.0f) * 32767.0f);
        int16_t b = (int16_t) std::lrint (std::clamp (R[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const int sr = 48000;
    perf::Engine e;
    e.prepare (sr);
    std::vector<float> L, R;
    float peak = 0.0f; bool bad = false;

    auto render = [&] (double seconds)
    {
        const int n = (int) (seconds * sr);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int pos = 0; pos < n; pos += 256)
            e.process (l.data() + pos, r.data() + pos, std::min (256, n - pos));
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) bad = true;
            peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]) });
        }
        L.insert (L.end(), l.begin(), l.end()); R.insert (R.end(), r.begin(), r.end());
    };
    auto chord = [&] (std::initializer_list<int> notes, bool on) { for (int n : notes) on ? e.noteOn (n) : e.noteOff (n); };

    // 1) Strings 8'+16', crescendo lento, ensemble stereo
    perf::Params p; p.strings16 = true; p.crescendo = 0.55f; p.sustain = 0.5f; p.stereoEnsemble = true;
    e.setParams (p);
    chord ({ 48, 55, 60, 64, 67 }, true); render (2.5); chord ({ 48, 55, 60, 64, 67 }, false); render (1.5);

    // 2) Brass: attacco medio, risonanza, range alto
    p = {}; p.brassOn = true; p.strings8 = false; p.brassAttack = 0.35f; p.brassDecay = 0.7f; p.brassRange = 0.8f; p.brassResonance = 0.55f;
    e.setParams (p);
    chord ({ 41, 48, 53, 57 }, true); render (2.0); chord ({ 41, 48, 53, 57 }, false); render (0.6);

    // 3) Brass + strings con modulazione VCF
    p.strings8 = true; p.modToVcf = true; p.modDepth = 0.6f; p.modDelay = 0.2f; p.modRate = 0.5f;
    e.setParams (p);
    chord ({ 45, 52, 57, 60 }, true); render (2.5); chord ({ 45, 52, 57, 60 }, false); render (0.8);

    // 4) Tutta la tastiera (stress test)
    p = {}; p.strings16 = true; e.setParams (p);
    for (int nn = 36; nn <= 84; ++nn) e.noteOn (nn);
    render (1.0); e.allNotesOff(); render (1.0);

    writeWav (argc > 1 ? argv[1] : "sb49_test.wav", L, R, sr);
    std::printf ("peak=%.3f  nan=%s  samples=%zu\n", peak, bad ? "SI" : "no", L.size());
    return bad ? 1 : 0;
}
