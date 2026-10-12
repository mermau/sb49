// Rende una nota tenuta con parametri da riga di comando: render_note out.wav nota secondi chiave=valore...
#include "../Source/dsp/PerformerEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
int main (int argc, char** argv)
{
    if (argc < 4) { std::puts ("uso: render_note out.wav nota secondi [param=valore ...]"); return 1; }
    const int sr = 48000; const int note = std::atoi (argv[2]); const double sec = std::atof (argv[3]);
    perf::Params p;
    for (int i = 4; i < argc; ++i)
    {
        std::string a = argv[i]; auto eq = a.find ('='); if (eq == std::string::npos) continue;
        std::string k = a.substr (0, eq); float v = std::stof (a.substr (eq + 1));
#define P(name) if (k == #name) p.name = decltype (p.name) (v);
        P(brassAttack) P(brassDecay) P(brassRange) P(brassResonance) P(brassVolume) P(brassOn)
        P(stringsLow) P(stringsMid) P(stringsHigh) P(strings16) P(strings8) P(crescendo) P(sustain)
        P(stringsVolume) P(modDelay) P(modRate) P(modDepth) P(modToVcf) P(masterVolume) P(pitchSemitones)
#undef P
    }
    perf::Engine e; e.prepare (sr); e.setParams (p);
    auto& en = e.ensembleForTest();
    for (int i = 4; i < argc; ++i)
    {
        std::string a = argv[i]; auto eq = a.find ('='); if (eq == std::string::npos) continue;
        std::string k = a.substr (0, eq); double v = std::stod (a.substr (eq + 1));
        if (k == "ensSlowHz") en.slowHz = v; if (k == "ensFastHz") en.fastHz = v;
        if (k == "ensSlowDepth") en.slowDepth = v; if (k == "ensFastDepth") en.fastDepth = v;
        if (k == "ensSlowSpacing") en.slowSpacing = v; if (k == "ensFastSpacing") en.fastSpacing = v;
    }
    en.updateRates();
    e.noteOn (note);
    const int n = (int) (sec * sr); std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; i += 256) e.process (L.data() + i, R.data() + i, std::min (256, n - i));
    FILE* f = std::fopen (argv[1], "wb");
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); }; auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + (uint32_t) n * 2); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (1); w32 (sr); w32 (sr * 2); w16 (2); w16 (16); std::fwrite ("data", 1, 4, f); w32 ((uint32_t) n * 2);
    for (int i = 0; i < n; ++i) { int16_t s = (int16_t) std::lrint (std::clamp (L[(size_t) i], -1.0f, 1.0f) * 32767); std::fwrite (&s, 2, 1, f); }
    std::fclose (f); return 0;
}
