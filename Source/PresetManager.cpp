#include "PresetManager.h"

namespace
{
// Valori nell'ordine di pid::all (cursori 0..1, interruttori 0/1, pitch in semitoni,
// transpose 0 = DOWN, 1 = normale, 2 = UP) + stereo ensemble.
struct Factory
{
    const char* name;
    std::array<float, pid::numMappable> v;
    bool stereo;
};

//                      ---------------- BRASS ----------------  ------------------------- STRINGS -------------------------  ------- MOD -------  MAST  PIT RTG TRN
//                      att   dec   rng   res   on  vol          low   mid   hig   16  8   cresc sust  vol          dly   rate  dep  vcf  vol
// il primo coincide con i valori di default dei parametri
const Factory factory[] = {
    { "Performer Strings",  {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.50f,0.50f,0.50f, 0, 1, 0.15f,0.35f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, false },
    { "Full Ensemble 16+8", {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.55f,0.55f,0.50f, 1, 1, 0.15f,0.50f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, true  },
    { "Slow Swell",         {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.50f,0.55f,0.45f, 1, 1, 0.75f,0.65f,0.80f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, true  },
    { "Bright Violins",     {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.35f,0.60f,0.80f, 0, 1, 0.12f,0.35f,0.70f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 2 }}, false },
    { "Dark Cellos",        {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.75f,0.50f,0.25f, 1, 0, 0.20f,0.40f,0.80f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, false },
    { "New Wave Strings",   {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.50f,0.65f,0.60f, 1, 1, 0.05f,0.30f,0.75f,  0.40f,0.55f,0.12f, 0,  0.80f, 0, 0, 1 }}, true  },
    { "Indie Pop Strings",  {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.40f,0.70f,0.35f, 0, 1, 0.08f,0.25f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, false },
    { "Vibrato Strings",    {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.50f,0.60f,0.55f, 0, 1, 0.10f,0.40f,0.75f,  0.50f,0.60f,0.25f, 0,  0.80f, 0, 0, 1 }}, false },
    { "Space Pad",          {{ 0.15f,0.60f,0.45f,0.25f, 0, 0.70f,  0.45f,0.50f,0.65f, 1, 1, 0.60f,0.90f,0.75f,  0.60f,0.25f,0.15f, 0,  0.80f, 0, 0, 1 }}, true  },
    { "Classic Brass",      {{ 0.20f,0.55f,0.50f,0.25f, 1, 0.75f,  0.50f,0.50f,0.50f, 0, 0, 0.10f,0.40f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, false },
    { "Brass Stab",         {{ 0.00f,0.25f,0.60f,0.30f, 1, 0.75f,  0.50f,0.50f,0.50f, 0, 0, 0.10f,0.40f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 1, 1 }}, false },
    { "Slow Brass",         {{ 0.70f,0.70f,0.45f,0.20f, 1, 0.75f,  0.50f,0.50f,0.50f, 0, 0, 0.10f,0.40f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, false },
    { "Resonant Sweep",     {{ 0.45f,0.65f,0.80f,0.65f, 1, 0.70f,  0.50f,0.50f,0.50f, 0, 0, 0.10f,0.40f,0.75f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, false },
    { "Wah Brass",          {{ 0.10f,0.60f,0.40f,0.55f, 1, 0.75f,  0.50f,0.50f,0.50f, 0, 0, 0.10f,0.40f,0.75f,  0.20f,0.55f,0.50f, 1,  0.80f, 0, 0, 1 }}, false },
    { "Strings & Brass",    {{ 0.25f,0.60f,0.45f,0.20f, 1, 0.55f,  0.50f,0.60f,0.50f, 1, 1, 0.15f,0.40f,0.70f,  0.30f,0.45f,0.00f, 0,  0.80f, 0, 0, 1 }}, true  },
};
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    currentName = factory[0].name;
    rescan();
}

int PresetManager::numFactory() { return (int) std::size (factory); }
juce::String PresetManager::factoryName (int i) { return juce::isPositiveAndBelow (i, numFactory()) ? factory[i].name : ""; }

juce::File PresetManager::userFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library/Audio/Presets/Collateral Effects/BIZARRE Entertainer");
   #else
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Collateral Effects/BIZARRE Entertainer/Presets");
   #endif
}

void PresetManager::rescan()
{
    list.clear();
    for (int i = 0; i < numFactory(); ++i)
        list.push_back ({ factory[i].name, true, i, {} });

    auto files = userFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    for (auto& f : files)
        list.push_back ({ f.getFileNameWithoutExtension(), false, -1, f });

    setCurrentName (currentName);
}

void PresetManager::setCurrentName (const juce::String& n)
{
    currentName = n;
    current = -1;
    // a parità di nome prevale il preset utente (è quello che si può modificare)
    for (int i = (int) list.size(); --i >= 0;)
        if (list[(size_t) i].name == n) { current = i; break; }
    ++version;
}

void PresetManager::applyValue (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (plainValue);
        if (std::abs (p->getValue() - norm) < 1.0e-6f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

void PresetManager::loadFactory (int i)
{
    const auto& f = factory[i];
    for (int k = 0; k < pid::numMappable; ++k)
        applyValue (pid::all[(size_t) k].id, f.v[(size_t) k]);
    applyValue (pid::stereoEnsembleId, f.stereo ? 1.0f : 0.0f);
}

juce::ValueTree PresetManager::capture (const juce::String& name) const
{
    juce::ValueTree t ("BIZARRE_PRESET");
    t.setProperty ("name", name, nullptr);
    t.setProperty ("version", 1, nullptr);
    auto put = [&] (const char* id)
    {
        if (auto* p = apvts.getParameter (id))
        {
            juce::ValueTree e ("PARAM");
            e.setProperty ("id", id, nullptr);
            e.setProperty ("value", p->convertFrom0to1 (p->getValue()), nullptr);
            t.appendChild (e, nullptr);
        }
    };
    for (const auto& p : pid::all) put (p.id);
    put (pid::stereoEnsembleId);
    return t;
}

bool PresetManager::loadFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    const auto t = juce::ValueTree::fromXml (*xml);
    if (! t.hasType ("BIZARRE_PRESET")) return false;
    for (auto e : t)
    {
        const auto id = e.getProperty ("id").toString();
        for (const auto& p : pid::all)
            if (id == p.id) applyValue (p.id, (float) e.getProperty ("value"));
        if (id == pid::stereoEnsembleId) applyValue (pid::stereoEnsembleId, (float) e.getProperty ("value"));
    }
    return true;
}

bool PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return false;
    const auto e = list[(size_t) index];
    if (e.factory) loadFactory (e.factoryIndex);
    else if (! loadFile (e.file)) return false;
    current = index;
    currentName = e.name;
    ++version;
    return true;
}

void PresetManager::step (int delta)
{
    const int n = (int) list.size();
    if (n == 0) return;
    const int from = current < 0 ? (delta > 0 ? -1 : 0) : current;
    load (((from + delta) % n + n) % n);
}

bool PresetManager::saveAs (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    auto dir = userFolder();
    if (! dir.createDirectory()) return false;
    auto file = dir.getChildFile (name + extension);
    auto xml = capture (name).createXml();
    if (xml == nullptr || ! xml->writeTo (file)) return false;
    currentName = name;
    rescan();
    return true;
}

bool PresetManager::save()
{
    if (current < 0 || currentIsFactory()) return false;
    return saveAs (list[(size_t) current].name);
}

bool PresetManager::remove (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) list.size()) || list[(size_t) index].factory) return false;
    const auto f = list[(size_t) index].file;
    if (! f.moveToTrash() && ! f.deleteFile()) return false;
    rescan();
    return true;
}
