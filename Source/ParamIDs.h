#pragma once
#include <array>

// Elenco ordinato dei parametri: l'indice è usato dalla mappa MIDI CC.
struct ParamInfo
{
    const char* id;
    const char* name;
    bool isBool;
    float defaultValue;
};

namespace pid
{
inline constexpr std::array<ParamInfo, 21> all {{
    { "brassAttack",    "Brass Attack",       false, 0.15f },
    { "brassDecay",     "Brass Decay",        false, 0.60f },
    { "brassRange",     "Brass Range",        false, 0.45f },
    { "brassResonance", "Brass Resonance",    false, 0.25f },
    { "brassOn",        "Brass On",           true,  0.0f  },
    { "brassVolume",    "Brass Volume",       false, 0.70f },
    { "stringsLow",     "Strings Low",        false, 0.50f },
    { "stringsMid",     "Strings Mid",        false, 0.60f },
    { "stringsHigh",    "Strings High",       false, 0.50f },
    { "strings16",      "Strings 16'",        true,  0.0f  },
    { "strings8",       "Strings 8'",         true,  1.0f  },
    { "crescendo",      "Crescendo",          false, 0.15f },
    { "sustain",        "Sustain",            false, 0.35f },
    { "stringsVolume",  "Strings Volume",     false, 0.75f },
    { "modDelay",       "Mod Delay",          false, 0.30f },
    { "modRate",        "Mod Rate",           false, 0.45f },
    { "modDepth",       "Mod Depth",          false, 0.00f },
    { "modToVcf",       "Mod VCF/FM",         true,  0.0f  },
    { "masterVolume",   "Master Volume",      false, 0.80f },
    { "pitch",          "Pitch",              false, 0.0f  }, // range -15..+15 semitoni
    { "brassRetrig",    "Brass Retrig",       true,  0.0f  }, // estensione, non presente nell'originale
}};

enum Index
{
    brassAttack, brassDecay, brassRange, brassResonance, brassOn, brassVolume,
    stringsLow, stringsMid, stringsHigh, strings16, strings8, crescendo, sustain, stringsVolume,
    modDelay, modRate, modDepth, modToVcf, masterVolume, pitch, brassRetrig,
    stereoEnsemble = 100 // non mappabile, non nell'elenco principale
};

inline constexpr const char* stereoEnsembleId = "stereoEnsemble";
inline constexpr int numMappable = (int) all.size();
} // namespace pid
