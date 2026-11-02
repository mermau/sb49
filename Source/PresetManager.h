#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ParamIDs.h"

// Preset di fabbrica (fissi, nel codice) + preset utente (file XML in una cartella dedicata).
// I preset contengono solo il suono: non la mappa MIDI e non l'instradamento delle uscite.
// Tutti i metodi vanno chiamati dal message thread.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    struct Entry
    {
        juce::String name;
        bool factory = false;
        int factoryIndex = -1;
        juce::File file;
    };

    static int numFactory();
    static juce::String factoryName (int i);
    static juce::File userFolder();
    static constexpr const char* extension = ".bizpreset";

    void rescan();                          // rilegge la cartella utente
    const std::vector<Entry>& entries() const { return list; }
    int getCurrentIndex() const { return current; }
    juce::String getCurrentName() const { return currentName; }
    bool currentIsFactory() const { return current >= 0 && current < (int) list.size() && list[(size_t) current].factory; }

    bool load (int index);
    void step (int delta);                  // preset precedente / successivo (ciclico)
    bool saveAs (const juce::String& name); // crea o sovrascrive un preset utente
    bool save();                            // sovrascrive il preset utente corrente
    bool remove (int index);

    // nome del preset salvato insieme al progetto della DAW
    void setCurrentName (const juce::String& n);

    std::atomic<int> version { 0 };         // cambia a ogni load/save (per aggiornare la GUI)

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Entry> list;
    int current = 0;
    juce::String currentName;

    void applyValue (const char* id, float plainValue);
    juce::ValueTree capture (const juce::String& name) const;
    bool loadFile (const juce::File& f);
    void loadFactory (int i);
};
