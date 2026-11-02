#include "PluginEditor.h"

namespace
{
void showLearnMenu (PerformerAudioProcessor& proc, int paramIndex, juce::Component* target)
{
    juce::PopupMenu m;
    const int cc = proc.ccMap.ccForParam (paramIndex);
    m.addSectionHeader (juce::String (pid::all[(size_t) paramIndex].name)
                        + (cc >= 0 ? "  (CC " + juce::String (cc) + ")" : "  (nessun CC)"));
    m.addItem (1, "MIDI Learn: muovi un controllo della tastiera");
    m.addItem (2, "Rimuovi assegnazione CC", cc >= 0);
    if (proc.ccMap.learnParam.load() >= 0) m.addItem (3, "Annulla MIDI Learn");
    m.addSeparator();
    m.addItem (4, "Ripristina mappa Arturia KeyLab");

    auto* p = &proc;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [p, paramIndex] (int r)
    {
        if (r == 1) p->ccMap.learnParam.store (paramIndex);
        if (r == 2) p->ccMap.clearParam (paramIndex);
        if (r == 3) p->ccMap.learnParam.store (-1);
        if (r == 4) p->ccMap.resetToKeyLabDefaults();
    });
}

juce::Font font (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

// ---------------------------------------------------------------------------------------
void LearnSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showLearnMenu (proc, paramIndex, this);
    else juce::Slider::mouseDown (e);
}

void LearnSlider::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);
    if (learning)
    {
        g.setColour (ui::learn);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 2.0f);
    }
}

void PerfButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showLearnMenu (proc, paramIndex, this);
    else juce::Button::mouseDown (e);
}

void PerfButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    const bool on = getToggleState();

    // LED
    const float lit = ledOverride >= 0.0f ? ledOverride : (on ? 1.0f : 0.0f);
    // piccolo LED rosso sopra il tasto
    juce::Rectangle<float> led (cx - 3.5f, b.getY() + 10.0f, 7.0f, 7.0f);
    g.setColour (juce::Colour (0xff0d0d0e));
    g.fillEllipse (led.expanded (2.0f));
    if (lit > 0.05f)
    {
        g.setColour (ui::ledOn.withAlpha (0.22f * lit));
        g.fillEllipse (led.expanded (5.0f * lit));
    }
    g.setColour (ui::ledOff.interpolatedWith (ui::ledOn, lit));
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.35f * lit));
    g.fillEllipse (led.getX() + 1.6f, led.getY() + 1.2f, 2.2f, 1.8f);

    // cappuccio quadrato senza scritte: "affonda" quando inserito
    const float depth = on || down ? 1.5f : 0.0f;
    juce::Rectangle<float> cap (cx - 14.0f, b.getY() + 30.0f + depth, 28.0f, 28.0f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (cap.translated (1.5f, 3.0f - depth), 1.5f);
    auto top = light ? juce::Colour (0xffc9c8c3) : juce::Colour (0xff2b2b2e);
    if (on) top = top.darker (0.12f);
    if (highlighted) top = top.brighter (0.08f);
    juce::ColourGradient grad (top.brighter (0.12f), cap.getX(), cap.getY(), top.darker (0.25f), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (light ? 0.35f : 0.1f));
    g.drawLine (cap.getX() + 1.5f, cap.getY() + 1.0f, cap.getRight() - 1.5f, cap.getY() + 1.0f, 1.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 1.5f, 1.0f);
    if (learning)
    {
        g.setColour (ui::learn);
        g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
    }
}

// ---------------------------------------------------------------------------------------
TransposeSwitch::TransposeSwitch (PerformerAudioProcessor& p)
    : proc (p),
      attachment (*p.getParam (pid::transpose), [this] (float v) { position = juce::jlimit (0, 2, juce::roundToInt (v)); repaint(); }, nullptr)
{
    attachment.sendInitialUpdate();
    setTooltip ("TRANSPOSE (Performer versione B): archi un'ottava sotto, normali o un'ottava sopra");
}

juce::Rectangle<float> TransposeSwitch::slot() const
{
    return juce::Rectangle<float> (getWidth() * 0.5f - 27.0f, 21.0f, 54.0f, 14.0f);
}

void TransposeSwitch::paint (juce::Graphics& g)
{
    const auto r = slot();
    g.setColour (ui::cream);
    g.setFont (font (8.5f));
    g.drawText ("TRANSPOSE", getLocalBounds().removeFromTop (16), juce::Justification::centred);
    g.setFont (font (7.5f));
    g.drawText ("DOWN", juce::Rectangle<float> (0.0f, r.getY(), r.getX() - 4.0f, r.getHeight()), juce::Justification::centredRight);
    g.drawText ("UP", juce::Rectangle<float> (r.getRight() + 4.0f, r.getY(), 30.0f, r.getHeight()), juce::Justification::centredLeft);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (ui::creamDim);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    for (int i = 0; i < 3; ++i)  // tacche delle tre posizioni
        g.fillRect (juce::Rectangle<float> (r.getX() + 9.0f + i * 18.0f - 0.5f, r.getBottom() + 2.0f, 1.0f, 3.0f));
    juce::Rectangle<float> lever (r.getX() + 2.0f + position * 18.0f, r.getY() + 2.0f, 14.0f, r.getHeight() - 4.0f);
    g.setColour (ui::cream);
    g.fillRoundedRectangle (lever, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (lever.getCentreX(), lever.getY() + 2.0f, lever.getCentreX(), lever.getBottom() - 2.0f, 1.0f);
    if (learning)
    {
        g.setColour (ui::learn);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 2.0f);
    }
}

void TransposeSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showLearnMenu (proc, pid::transpose, this); return; }
    const auto r = slot();
    const int pos = juce::jlimit (0, 2, (int) ((e.position.x - r.getX()) / 18.0f));
    attachment.setValueAsCompleteGesture ((float) pos);
}

// ---------------------------------------------------------------------------------------
PresetBar::PresetBar (PerformerAudioProcessor& p) : proc (p)
{
    setTooltip ("Preset: frecce = precedente / successivo, clic sul nome = elenco e salvataggio. "
                "Anche Program Change MIDI dalla tastiera.");
}

void PresetBar::refresh()
{
    const int v = proc.presets.version.load();
    if (v != lastVersion) { lastVersion = v; repaint(); }
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (ui::creamDim);
    g.setFont (font (7.5f));
    g.drawText ("PRESET", r.withHeight (10.0f), juce::Justification::centredLeft, false);

    auto box = r.withTrimmedTop (11.0f);
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (box, 2.5f);
    g.setColour (ui::boxRed);
    g.drawRoundedRectangle (box.reduced (0.5f), 2.5f, 1.0f);

    auto arrow = [&] (juce::Rectangle<float> a, bool left)
    {
        juce::Path t;
        const auto c = a.getCentre();
        if (left) t.addTriangle (c.x + 3.0f, c.y - 4.0f, c.x + 3.0f, c.y + 4.0f, c.x - 3.0f, c.y);
        else      t.addTriangle (c.x - 3.0f, c.y - 4.0f, c.x - 3.0f, c.y + 4.0f, c.x + 3.0f, c.y);
        g.setColour (ui::cream);
        g.fillPath (t);
    };
    arrow (box.withWidth (16.0f), true);
    arrow (box.withTrimmedLeft (box.getWidth() - 16.0f), false);

    auto name = proc.presets.getCurrentName();
    if (name.isEmpty()) name = "-";
    g.setColour (proc.presets.currentIsFactory() ? ui::cream : ui::stripeAmber);
    g.setFont (font (9.5f));
    g.drawFittedText (name, box.reduced (17.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (e.position.y < 11.0f) { showMenu(); return; }
    if (e.position.x < 17.0f)                      proc.presets.step (-1);
    else if (e.position.x > (float) getWidth() - 17.0f) proc.presets.step (+1);
    else                                           showMenu();
    refresh();
}

void PresetBar::showMenu()
{
    auto& pm = proc.presets;
    pm.rescan();
    juce::PopupMenu m, fac, usr;
    const auto& list = pm.entries();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto& menu = list[(size_t) i].factory ? fac : usr;
        const juce::String prefix = list[(size_t) i].factory ? juce::String (i + 1).paddedLeft ('0', 2) + "  " : juce::String();
        menu.addItem (1000 + i, prefix + list[(size_t) i].name, true, i == pm.getCurrentIndex());
    }
    m.addSubMenu ("Preset di fabbrica", fac);
    if (usr.getNumItems() > 0) m.addSubMenu ("Preset utente", usr);
    else m.addItem (9, "Preset utente: nessuno", false);
    m.addSeparator();
    const bool user = pm.getCurrentIndex() >= 0 && ! pm.currentIsFactory();
    m.addItem (1, "Salva" + (user ? "  \"" + pm.getCurrentName() + "\"" : juce::String()), user);
    m.addItem (2, "Salva come...");
    m.addItem (3, "Elimina preset utente corrente", user);
    m.addSeparator();
    m.addItem (4, "Mostra cartella dei preset");

    juce::Component::SafePointer<PresetBar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc.presets;
        if (r >= 1000) p.load (r - 1000);
        else if (r == 1) p.save();
        else if (r == 2) safe->askSaveAs();
        else if (r == 3) p.remove (p.getCurrentIndex());
        else if (r == 4)
        {
            auto dir = PresetManager::userFolder();
            dir.createDirectory();
            dir.revealToUser();
        }
        safe->refresh();
    });
}

void PresetBar::askSaveAs()
{
    auto* w = new juce::AlertWindow ("Salva preset", "Nome del preset:", juce::MessageBoxIconType::NoIcon, this);
    const auto suggested = proc.presets.currentIsFactory() ? proc.presets.getCurrentName() + " (mio)" : proc.presets.getCurrentName();
    w->addTextEditor ("name", suggested);
    w->addButton ("Salva", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        if (! safe->proc.presets.saveAs (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Salva preset",
                                                    "Impossibile salvare in " + PresetManager::userFolder().getFullPathName());
        safe->refresh();
    }), true);
}

// ---------------------------------------------------------------------------------------
PerformerPanel::PerformerPanel (PerformerAudioProcessor& p)
    : proc (p), presetBar (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // BRASS SECTION
    sections.push_back ({ "BRASS", (int) slots.size(), 6, {} });
    addSlider (pid::brassAttack, "ATTACK");
    addSlider (pid::brassDecay, "DECAY");
    addSlider (pid::brassRange, "RANGE");
    addSlider (pid::brassResonance, "RESONANCE");
    addButton (pid::brassOn, "ON");
    addSlider (pid::brassVolume, "VOLUME");
    // STRINGS SECTION
    sections.push_back ({ "STRINGS", (int) slots.size(), 8, {} });
    addSlider (pid::stringsLow, "LOW");
    addSlider (pid::stringsMid, "MIDDLE");
    addSlider (pid::stringsHigh, "HIGH");
    for (int i = 0; i < 3; ++i) sliders[sliders.size() - 1 - (size_t) i]->getProperties().set ("eq", true);
    addButton (pid::strings16, "16'");
    buttons.back()->light = true;
    addButton (pid::strings8, "8'");
    buttons.back()->light = true;
    addSlider (pid::crescendo, "CRESCENDO");
    addSlider (pid::sustain, "SUSTAIN");
    addSlider (pid::stringsVolume, "VOLUME");
    // MODULATION SECTION
    sections.push_back ({ "MODULATION", (int) slots.size(), 4, {} });
    addSlider (pid::modDelay, "DELAY\nLENGTH");
    addSlider (pid::modRate, "RATE");
    addSlider (pid::modDepth, "DEPTH");
    addButton (pid::modToVcf, {}); // etichette VCF / FM con i simboli d'onda disegnate a parte
    modLedButton = buttons.back().get();
    // MASTER
    sections.push_back ({ "MASTER", (int) slots.size(), 1, {} });
    addSlider (pid::masterVolume, "VOLUME");

    addAndMakeVisible (presetBar);
    transposeSwitch = std::make_unique<TransposeSwitch> (proc);
    addAndMakeVisible (*transposeSwitch);

    // pannello posteriore: PITCH e opzione stereo
    pitchKnob = std::make_unique<LearnSlider> (proc, pid::pitch);
    pitchKnob->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pitchKnob->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchKnob->setDoubleClickReturnValue (true, 0.0);
    pitchKnob->setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (*pitchKnob);
    pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "pitch", *pitchKnob);

    stereoToggle.setButtonText ("STEREO ENSEMBLE");
    stereoToggle.setColour (juce::ToggleButton::textColourId, ui::cream);
    stereoToggle.setColour (juce::ToggleButton::tickColourId, ui::ledOn);
    stereoToggle.setColour (juce::ToggleButton::tickDisabledColourId, ui::creamDim);
    stereoToggle.setTooltip ("Estensione: distribuisce le tre linee di ritardo sui due canali (l'originale è mono)");
    addAndMakeVisible (stereoToggle);
    stereoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid::stereoEnsembleId, stereoToggle);

    separateToggle.setButtonText ("USCITE SEPARATE");
    separateToggle.setColour (juce::ToggleButton::textColourId, ui::cream);
    separateToggle.setColour (juce::ToggleButton::tickColourId, ui::ledOn);
    separateToggle.setColour (juce::ToggleButton::tickDisabledColourId, ui::creamDim);
    separateToggle.setTooltip ("Come i jack posteriori: MAIN = archi, BRASS = brass (prima del volume master), SIGNAL = segnale grezzo. "
                               "Nella DAW instrada le uscite aggiuntive del plugin su altre tracce.");
    addAndMakeVisible (separateToggle);
    separateAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid::separateOutputsId, separateToggle);

    retrigToggle.setButtonText ("BRASS RETRIG");
    retrigToggle.setColour (juce::ToggleButton::textColourId, ui::cream);
    retrigToggle.setColour (juce::ToggleButton::tickColourId, ui::ledOn);
    retrigToggle.setColour (juce::ToggleButton::tickDisabledColourId, ui::creamDim);
    retrigToggle.setTooltip ("Estensione: riattacco del brass a ogni nota anche suonando legato (l'originale riparte solo dal primo tasto)");
    addAndMakeVisible (retrigToggle);
    retrigAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "brassRetrig", retrigToggle);

    // tastiera 49 tasti Do-Do
    keyboard.setAvailableRange (perf::kLowestNote, perf::kLowestNote + perf::kNumKeys - 1);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff6f1e4));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff111111));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6d675a));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::ledOn.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::cream.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.5f));
    addAndMakeVisible (keyboard);

    setSize (W, H);
}

PerformerPanel::~PerformerPanel() = default;

void PerformerPanel::addSlider (int paramIndex, const juce::String& label)
{
    auto s = std::make_unique<LearnSlider> (proc, paramIndex);
    s->setSliderStyle (juce::Slider::LinearVertical);
    s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s->setPopupDisplayEnabled (true, true, this);
    s->setDoubleClickReturnValue (true, pid::all[(size_t) paramIndex].defaultValue);
    addAndMakeVisible (*s);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, pid::all[(size_t) paramIndex].id, *s));
    slots.push_back ({ label, s.get(), paramIndex, {} });
    sliders.push_back (std::move (s));
}

void PerformerPanel::addButton (int paramIndex, const juce::String& label)
{
    auto b = std::make_unique<PerfButton> (proc, paramIndex);
    addAndMakeVisible (*b);
    bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, pid::all[(size_t) paramIndex].id, *b));
    slots.push_back ({ label, b.get(), paramIndex, {} });
    buttons.push_back (std::move (b));
}

void PerformerPanel::resized()
{
    constexpr int cheek = 34, slotW = 52, gap = 18;
    int x = cheek + 13;
    for (auto& sec : sections)
    {
        const int w = sec.count * slotW + 16;
        sec.box = { x, 78, w, 240 };
        for (int i = 0; i < sec.count; ++i)
        {
            auto& slot = slots[(size_t) (sec.first + i)];
            slot.area = { x + 8 + i * slotW, 86, slotW, 226 };
            if (dynamic_cast<PerfButton*> (slot.comp) != nullptr)
                slot.comp->setBounds (slot.area.getX(), 140, slotW, 70);
            else
                slot.comp->setBounds (slot.area.getX(), 104, slotW, 166);
        }
        x += w + gap;
    }

    pitchKnob->setBounds (944, 7, 44, 44);
    {   // deviatore TRANSPOSE sotto i pulsanti 16' e 8'
        const auto a = spanOf (pid::strings16, pid::strings8);
        transposeSwitch->setBounds (a.getX() - 6, 222, a.getWidth() + 12, 42);
    }
    stereoToggle.setBounds (1022, 6, 128, 22);
    separateToggle.setBounds (1022, 30, 128, 22);
    presetBar.setBounds (744, 4, 136, 30);
    retrigToggle.setBounds (744, 36, 124, 20);

    const int kbW = W - 2 * cheek;
    keyboard.setBounds (cheek, 338, kbW, H - 338 - 8);
    keyboard.setKeyWidth ((float) kbW / 29.0f); // 29 tasti bianchi
    keyboard.setBlackNoteLengthProportion (0.62f);
}

juce::Rectangle<int> PerformerPanel::spanOf (int firstParam, int lastParam) const
{
    juce::Rectangle<int> a;
    for (auto& s : slots)
        if (s.paramIndex >= firstParam && s.paramIndex <= lastParam)
            a = a.isEmpty() ? s.area : a.getUnion (s.area);
    return a;
}

void PerformerPanel::drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    const juce::Rectangle<float> head (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (head.translated (0.6f, 0.9f));
    juce::ColourGradient grad (juce::Colour (0xffb9b9bd), head.getX(), head.getY(),
                               juce::Colour (0xff4a4a4f), head.getRight(), head.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (head);
    juce::Path slotPath;
    slotPath.addRectangle (-r * 0.8f, -0.55f, r * 1.6f, 1.1f);
    g.setColour (juce::Colour (0xff1c1c1e));
    g.fillPath (slotPath, juce::AffineTransform::rotation (angle).translated (c));
}

void PerformerPanel::drawWood (juce::Graphics& g, juce::Rectangle<float> r)
{
    // noce chiaro
    juce::ColourGradient wood (juce::Colour (0xff7a4c2a), r.getX(), r.getY(),
                               juce::Colour (0xff5c361b), r.getRight(), r.getY(), false);
    wood.addColour (0.45, juce::Colour (0xff9a663c));
    g.setGradientFill (wood);
    g.fillRoundedRectangle (r, 6.0f);
    juce::Random rnd (int (r.getX()) + 7);
    for (int i = 0; i < 16; ++i)
    {
        const float gx = r.getX() + 3.0f + rnd.nextFloat() * (r.getWidth() - 6.0f);
        g.setColour (juce::Colour (0xff3d2210).withAlpha (0.18f + 0.25f * rnd.nextFloat()));
        juce::Path grain;
        grain.startNewSubPath (gx, r.getY());
        grain.cubicTo (gx + 3.0f, r.getY() + r.getHeight() * 0.3f, gx - 3.0f, r.getY() + r.getHeight() * 0.7f,
                       gx + 1.5f, r.getBottom());
        g.strokePath (grain, juce::PathStrokeType (0.7f + rnd.nextFloat()));
    }
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (r.getX() + 2.0f, r.getY() + 4.0f, r.getX() + 2.0f, r.getBottom() - 4.0f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

namespace
{
// sottotitolo serigrafato con filetti laterali: "—— FILTER CONTROLS ——"
void drawRuledHeading (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size)
{
    const juce::Font f (juce::FontOptions (size, juce::Font::bold));
    const float tw = juce::GlyphArrangement::getStringWidth (f, text) + 10.0f;
    const float cy = r.getCentreY();
    g.setColour (ui::creamDim);
    g.drawLine (r.getX() + 6.0f, cy, r.getCentreX() - tw * 0.5f, cy, 0.9f);
    g.drawLine (r.getCentreX() + tw * 0.5f, cy, r.getRight() - 6.0f, cy, 0.9f);
    g.setColour (ui::cream);
    g.setFont (f);
    g.drawText (text, r, juce::Justification::centred, false);
}

// piccolo simbolo d'onda quadra accanto a VCF / FM
void drawPulseGlyph (juce::Graphics& g, float x, float cy, float w, float h)
{
    juce::Path p;
    p.startNewSubPath (x, cy + h * 0.5f);
    p.lineTo (x + w * 0.2f, cy + h * 0.5f);
    p.lineTo (x + w * 0.2f, cy - h * 0.5f);
    p.lineTo (x + w * 0.6f, cy - h * 0.5f);
    p.lineTo (x + w * 0.6f, cy + h * 0.5f);
    p.lineTo (x + w, cy + h * 0.5f);
    g.strokePath (p, juce::PathStrokeType (1.0f));
}
} // namespace

void PerformerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // pannello nero opaco
    juce::Rectangle<float> face (30.0f, 0.0f, (float) W - 60.0f, 322.0f);
    g.setColour (ui::face);
    g.fillRect (face);
    {   // grana opaca appena percettibile
        juce::Random rnd (1234);
        for (int i = 0; i < 1400; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.012f + 0.018f * rnd.nextFloat()));
            g.fillRect (30.0f + rnd.nextFloat() * ((float) W - 60.0f), rnd.nextFloat() * 322.0f, 1.0f, 1.0f);
        }
    }

    // intestazione: logo BIZARRE Entertainer (lettering originale, font OFL Monoton e Shrikhand)
    {
        static const auto monoton = juce::Typeface::createSystemTypefaceFor (BinaryData::MonotonRegular_ttf, BinaryData::MonotonRegular_ttfSize);
        static const auto shrikhand = juce::Typeface::createSystemTypefaceFor (BinaryData::ShrikhandRegular_ttf, BinaryData::ShrikhandRegular_ttfSize);
        const juce::Font big (juce::FontOptions (monoton).withHeight (50.0f));
        const juce::Font script (juce::FontOptions (shrikhand).withHeight (27.0f));
        const juce::String word ("BIZARRE");
        const float wBig = juce::GlyphArrangement::getStringWidth (big, word);
        g.setColour (ui::cream);
        g.setFont (big);
        g.drawText (word, juce::Rectangle<float> (48.0f, 4.0f, wBig + 10.0f, 52.0f), juce::Justification::centredLeft, false);

        const float x = 50.0f + wBig + 18.0f;
        const juce::Colour stripes[3] = { ui::stripeOrange, ui::stripeAmber, ui::stripeRed };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (stripes[i]);
            g.fillRect (juce::Rectangle<float> (x, 38.0f + (float) i * 5.0f, 715.0f - x, 3.0f));
        }
        g.setColour (ui::stripeOrange);
        g.setFont (script);
        g.drawText ("Entertainer", juce::Rectangle<float> (x, 3.0f, 260.0f, 36.0f), juce::Justification::centredLeft, false);
        const float wEnt = juce::GlyphArrangement::getStringWidth (script, "Entertainer");
        g.setColour (ui::cream);
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::italic)));
        g.drawText ("by Collateral Effects", juce::Rectangle<float> (x + wEnt + 8.0f, 19.0f, 130.0f, 14.0f), juce::Justification::centredLeft, false);
        g.setColour (ui::creamDim);
        g.setFont (font (9.5f, false));
        g.drawText ("tasto destro = MIDI Learn",
                    juce::Rectangle<float> (x + wEnt + 140.0f, 19.0f, 715.0f - x - wEnt - 140.0f, 14.0f), juce::Justification::centredRight, false);
    }
    g.setColour (ui::cream);
    g.setFont (font (10.0f));
    g.drawText ("PITCH", juce::Rectangle<float> (892.0f, 20.0f, 48.0f, 16.0f), juce::Justification::centredRight);

    // riquadri: filetto rosso spento, titolo che interrompe il bordo superiore, viti agli angoli
    for (const auto& sec : sections)
    {
        const auto box = sec.box.toFloat();
        g.setColour (ui::boxRed);
        g.drawRoundedRectangle (box, 6.0f, 1.3f);

        const juce::Font tf (juce::FontOptions (12.5f, juce::Font::bold));
        const float tw = juce::GlyphArrangement::getStringWidth (tf, sec.title) + 14.0f;
        const juce::Rectangle<float> tr (box.getCentreX() - tw * 0.5f, box.getY() - 8.0f, tw, 16.0f);
        g.setColour (ui::face);
        g.fillRect (tr);
        g.setColour (ui::cream);
        g.setFont (tf);
        g.drawText (sec.title, tr, juce::Justification::centred, false);

        const float in = 7.0f;
        int k = sec.first;
        for (auto c : { box.getTopLeft() + juce::Point<float> (in, in), box.getTopRight() + juce::Point<float> (-in, in),
                        box.getBottomLeft() + juce::Point<float> (in, -in), box.getBottomRight() + juce::Point<float> (-in, -in) })
            drawScrew (g, c, 2.8f, 0.4f + 0.9f * (float) (k++ % 3));

        for (int i = 0; i < sec.count; ++i)
        {
            const auto& slot = slots[(size_t) (sec.first + i)];
            if (slot.label.isEmpty()) continue;
            const bool twoLine = slot.label.containsChar ('\n');
            g.setColour (ui::cream);
            g.setFont (font (8.6f));
            g.drawFittedText (slot.label, slot.area.withY (twoLine ? 282 : 286).withHeight (twoLine ? 24 : 16).reduced (1, 0),
                              juce::Justification::centred, 2, 0.75f);
        }
    }

    // sottotitoli serigrafati
    {
        auto band = [] (juce::Rectangle<int> r, int y, int h) { return r.withY (y).withHeight (h).toFloat(); };
        drawRuledHeading (g, "FILTER CONTROLS", band (spanOf (pid::brassAttack, pid::brassResonance), 88, 12), 8.6f);
        drawRuledHeading (g, "E. Q. CONTROL", band (spanOf (pid::stringsLow, pid::stringsHigh), 88, 12), 8.6f);
        drawRuledHeading (g, "LENGTH", band (spanOf (pid::crescendo, pid::sustain), 273, 10), 7.8f);
    }

    // VCF / FM con i simboli d'onda, sotto il tasto della modulazione
    if (modLedButton != nullptr)
    {
        const auto b = modLedButton->getBounds().toFloat();
        const float lx = b.getCentreX() - 17.0f;
        g.setColour (ui::cream);
        g.setFont (font (8.6f));
        g.drawText ("VCF", juce::Rectangle<float> (lx, b.getBottom() + 6.0f, 22.0f, 12.0f), juce::Justification::centredLeft, false);
        g.drawText ("FM",  juce::Rectangle<float> (lx, b.getBottom() + 22.0f, 22.0f, 12.0f), juce::Justification::centredLeft, false);
        drawPulseGlyph (g, lx + 23.0f, b.getBottom() + 12.0f, 11.0f, 6.0f);
        drawPulseGlyph (g, lx + 23.0f, b.getBottom() + 28.0f, 11.0f, 6.0f);
    }

    // listello nero inclinato sopra la tastiera, con viti
    {
        const juce::Rectangle<float> strip (30.0f, 322.0f, (float) W - 60.0f, 16.0f);
        juce::ColourGradient sg (juce::Colour (0xff2c2c2f), 0.0f, strip.getY(), juce::Colour (0xff060607), 0.0f, strip.getBottom(), false);
        g.setGradientFill (sg);
        g.fillRect (strip);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine (strip.getX(), strip.getY() + 0.5f, strip.getRight(), strip.getY() + 0.5f, 1.0f);
        for (float sx : { 60.0f, (float) W * 0.5f, (float) W - 60.0f })
            drawScrew (g, { sx, strip.getCentreY() }, 3.0f, 0.7f);
    }

    // fianchi in legno
    drawWood (g, { 0.0f, 0.0f, 34.0f, (float) H });
    drawWood (g, { (float) W - 34.0f, 0.0f, 34.0f, (float) H });
}

void PerformerPanel::refresh()
{
    if (modLedButton != nullptr)
    {
        const float v = proc.getLfoMonitor();
        if (std::abs (v - modLedButton->ledOverride) > 0.02f)
        {
            modLedButton->ledOverride = v;
            modLedButton->repaint();
        }
    }

    const int learnIdx = proc.ccMap.learnParam.load();
    const int ver = proc.ccMap.version.load();
    const bool mapChanged = ver != lastMapVersion;
    lastMapVersion = ver;

    auto update = [&] (auto* comp, int idx)
    {
        const bool l = (idx == learnIdx);
        if (comp->learning != l) { comp->learning = l; comp->repaint(); }
        if (mapChanged)
        {
            const int cc = proc.ccMap.ccForParam (idx);
            comp->setTooltip (juce::String (pid::all[(size_t) idx].name)
                              + (cc >= 0 ? "  -  MIDI CC " + juce::String (cc) : "  -  nessun CC (tasto destro per MIDI Learn)"));
        }
    };
    for (auto& slot : slots)
    {
        if (auto* s = dynamic_cast<LearnSlider*> (slot.comp)) update (s, slot.paramIndex);
        else if (auto* b = dynamic_cast<PerfButton*> (slot.comp)) update (b, slot.paramIndex);
    }
    update (pitchKnob.get(), pid::pitch);
    update (transposeSwitch.get(), pid::transpose);
    presetBar.refresh();
}

// ---------------------------------------------------------------------------------------
PerformerEditor::PerformerEditor (PerformerAudioProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (PerformerPanel::W / 2, PerformerPanel::H / 2, PerformerPanel::W * 2, PerformerPanel::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) PerformerPanel::W / PerformerPanel::H);
    setSize (PerformerPanel::W, PerformerPanel::H);
    startTimerHz (30);
}

PerformerEditor::~PerformerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PerformerEditor::resized()
{
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) PerformerPanel::W));
}
