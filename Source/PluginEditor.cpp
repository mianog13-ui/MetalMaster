#include "PluginEditor.h"

juce::Component* MetalMasterAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& name,
                                                           const juce::String& suffix)
{
    knobs.push_back (std::make_unique<Knob> (proc.apvts, id, name, suffix));
    addAndMakeVisible (*knobs.back());
    return knobs.back().get();
}

juce::Component* MetalMasterAudioProcessorEditor::addToggle (const juce::String& id, const juce::String& text)
{
    toggles.push_back (std::make_unique<ToggleBtn> (proc.apvts, id, text));
    addAndMakeVisible (*toggles.back());
    return toggles.back().get();
}

int MetalMasterAudioProcessorEditor::addGroup (const juce::String& title, float weight,
                                               std::vector<std::vector<juce::Component*>> rows)
{
    Group g;
    g.title = title;
    g.rows = std::move (rows);
    g.weight = weight;
    groups.push_back (std::move (g));
    return (int) groups.size() - 1;
}

//==============================================================================
MetalMasterAudioProcessorEditor::MetalMasterAudioProcessorEditor (MetalMasterAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto& s = proc.apvts;

    // Preset panel
    presetPanel = std::make_unique<PresetPanel>();
    presetPanel->box.addItemList (MetalMasterAudioProcessor::getPresetNames(), 1);
    presetPanel->box.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);
    presetPanel->box.onChange = [this] { proc.applyPreset (presetPanel->box.getSelectedId() - 1); };
    addAndMakeVisible (*presetPanel);

    clipExtras = std::make_unique<ClipperExtras> (s);
    addAndMakeVisible (*clipExtras);

    // ---- Groups ---------------------------------------------------------------
    const int gPreset = addGroup ("PRESET", 2.0f, { { presetPanel.get() } });

    const int gMacros = addGroup ("MACROS  (offsets on top of the knobs)", 5.0f, { {
        addKnob ("macWeight", "Weight",   " %"),
        addKnob ("macPunch",  "Punch",    " %"),
        addKnob ("macAir",    "Air",      " %"),
        addKnob ("macWidth",  "Width",    " %"),
        addKnob ("macLoud",   "Loudness", " %") } });

    const int gInput = addGroup ("INPUT", 2.0f, { {
        addKnob ("inGain", "Input", " dB"),
        addKnob ("hpFreq", "HPF",   " Hz") } });

    const int gTone = addGroup ("TONE", 6.0f, { {
        addKnob ("lowGain",  "Low 80 Hz", " dB"),
        addKnob ("lmFreq",   "LowMid Hz", " Hz"),
        addKnob ("lmGain",   "LowMid",    " dB"),
        addKnob ("presFreq", "Pres Hz",   " Hz"),
        addKnob ("presGain", "Presence",  " dB"),
        addKnob ("airGain",  "Air 10k",   " dB") } });

    const int gMb = addGroup ("MULTIBAND COMPRESSOR", 1.0f, {
        {
            addKnob ("mbThr1", "Low Thr", " dB"),   addKnob ("mbRatio1", "Low Ratio", ":1"),   addKnob ("mbGain1", "Low Gain", " dB"),
            addKnob ("mbThr2", "LoMid Thr", " dB"), addKnob ("mbRatio2", "LoMid Ratio", ":1"), addKnob ("mbGain2", "LoMid Gain", " dB"),
            addKnob ("mbThr3", "HiMid Thr", " dB"), addKnob ("mbRatio3", "HiMid Ratio", ":1"), addKnob ("mbGain3", "HiMid Gain", " dB"),
            addKnob ("mbThr4", "High Thr", " dB"),  addKnob ("mbRatio4", "High Ratio", ":1"),  addKnob ("mbGain4", "High Gain", " dB")
        },
        {
            addToggle ("mbOn", "Multiband On"),
            addKnob ("xo1",   "Xover 1",  " Hz"),
            addKnob ("xo2",   "Xover 2",  " Hz"),
            addKnob ("xo3",   "Xover 3",  " Hz"),
            addKnob ("mbAtk", "Attack",   " ms"),
            addKnob ("mbRel", "Release",  " ms")
        } });

    const int gBus = addGroup ("BUS COMPRESSOR", 7.0f, { {
        addToggle ("busOn", "Bus Comp On"),
        addKnob ("busThresh", "Threshold", " dB"),
        addKnob ("busRatio",  "Ratio",     ":1"),
        addKnob ("busAtk",    "Attack",    " ms"),
        addKnob ("busRel",    "Release",   " ms"),
        addKnob ("busMakeup", "Makeup",    " dB"),
        addKnob ("busMix",    "Mix",       " %") } });

    const int gStereo = addGroup ("STEREO", 2.5f, { {
        addKnob ("width",    "Width",      " %"),
        addKnob ("monoFreq", "Mono Below", " Hz") } });

    const int gSat = addGroup ("SATURATION", 2.0f, { {
        addKnob ("satDrive", "Drive", " dB"),
        addKnob ("satMix",   "Mix",   " %") } });

    const int gClip = addGroup ("CLIPPER", 2.4f, { {
        addKnob ("clipThresh", "Threshold", " dB"),
        clipExtras.get() } });

    const int gLim = addGroup ("LIMITER", 4.0f, { {
        addKnob ("limDrive", "Drive",   " dB"),
        addKnob ("limCeil",  "Ceiling", " dB"),
        addKnob ("limRel",   "Release", " ms"),
        addToggle ("truePeak", "True Peak") } });

    rowDefs = {
        { 0.8f, { gPreset, gMacros } },
        { 1.0f, { gInput, gTone } },
        { 1.8f, { gMb } },
        { 1.0f, { gBus, gStereo } },
        { 1.0f, { gSat, gClip, gLim } }
    };

    resetButton.onClick = [this] { proc.meter.requestReset(); };
    addAndMakeVisible (resetButton);

    setResizable (true, true);
    setResizeLimits (900, 600, 1800, 1200);
    getConstrainer()->setFixedAspectRatio (1200.0 / 800.0);
    setSize (1200, 800);

    startTimerHz (30);
}

void MetalMasterAudioProcessorEditor::timerCallback()
{
    auto follow = [] (float& shown, float target) { shown = std::max (target, shown * 0.9f); };

    follow (shownGr[0], proc.grDb.load());
    follow (shownGr[1], proc.busGrDb.load());
    for (int b = 0; b < 4; ++b)
        follow (shownGr[2 + b], proc.mbGrDb[b].load());

    repaint (meterBounds);
}

void MetalMasterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    meterBounds = area.removeFromRight (190);
    area.removeFromRight (10);

    auto layoutRow = [this] (juce::Rectangle<int> row, const std::vector<int>& idx)
    {
        float total = 0.0f;
        for (int i : idx) total += groups[(size_t) i].weight;

        float x = (float) row.getX();
        for (int i : idx)
        {
            auto& g = groups[(size_t) i];
            const float w = (float) row.getWidth() * g.weight / total;
            g.bounds = juce::Rectangle<int> ((int) x, row.getY(), (int) w, row.getHeight()).reduced (4);
            x += w;

            auto inner = g.bounds.reduced (6);
            inner.removeFromTop (20); // title strip

            const int nRows = (int) g.rows.size();
            const int rowH = inner.getHeight() / std::max (1, nRows);

            for (auto& r : g.rows)
            {
                auto rb = inner.removeFromTop (rowH);
                const int cw = rb.getWidth() / std::max (1, (int) r.size());
                for (auto* c : r)
                    c->setBounds (rb.removeFromLeft (cw));
            }
        }
    };

    float totalH = 0.0f;
    for (auto& rd : rowDefs) totalH += rd.height;

    float y = (float) area.getY();
    for (auto& rd : rowDefs)
    {
        const float h = (float) area.getHeight() * rd.height / totalH;
        layoutRow (juce::Rectangle<int> (area.getX(), (int) y, area.getWidth(), (int) h), rd.groupIdx);
        y += h;
    }

    resetButton.setBounds (meterBounds.removeFromBottom (28).reduced (4, 0));
}

void MetalMasterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14171c));

    for (auto& grp : groups)
    {
        g.setColour (juce::Colour (0xff1e232b));
        g.fillRoundedRectangle (grp.bounds.toFloat(), 8.0f);

        g.setColour (juce::Colour (0xffe8a33d));
        g.setFont (13.0f);
        g.drawText (grp.title, grp.bounds.reduced (10, 6).removeFromTop (16),
                    juce::Justification::centredLeft);
    }

    // ---- Meter panel -----------------------------------------------------------
    auto panel = meterBounds.toFloat().withTrimmedBottom (32.0f);
    g.setColour (juce::Colour (0xff1e232b));
    g.fillRoundedRectangle (panel, 8.0f);

    auto area = panel.toNearestInt().reduced (12, 8);
    g.setColour (juce::Colour (0xffe8a33d));
    g.setFont (13.0f);
    g.drawText ("OUTPUT METERS", area.removeFromTop (20), juce::Justification::centredLeft);

    auto fmt = [] (float v, const char* unit)
    {
        return v <= -99.0f ? juce::String ("-inf ") + unit : juce::String (v, 1) + " " + unit;
    };

    auto row = [&] (const juce::String& name, const juce::String& value, juce::Colour c = juce::Colours::white)
    {
        auto r = area.removeFromTop (24);
        g.setColour (juce::Colours::grey);
        g.setFont (13.0f);
        g.drawText (name, r, juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (15.0f);
        g.drawText (value, r, juce::Justification::centredRight);
    };

    const auto toDb = [] (float lin) { return juce::Decibels::gainToDecibels (lin, -100.0f); };
    const float peakDb = std::max (toDb (proc.meter.getPeakL()), toDb (proc.meter.getPeakR()));

    row ("Momentary",  fmt (proc.meter.getMomentary(), "LUFS"));
    row ("Short-term", fmt (proc.meter.getShortTerm(), "LUFS"));
    row ("Integrated", fmt (proc.meter.getIntegrated(), "LUFS"), juce::Colour (0xffe8a33d));
    area.removeFromTop (8);
    row ("True peak", fmt (peakDb, "dBTP"),
         peakDb > -0.1f ? juce::Colours::red : juce::Colours::white);
    area.removeFromTop (14);

    // Gain-reduction bars (0..12 dB)
    g.setColour (juce::Colour (0xffe8a33d));
    g.setFont (13.0f);
    g.drawText ("GAIN REDUCTION", area.removeFromTop (20), juce::Justification::centredLeft);

    auto grBar = [&] (const juce::String& name, float db)
    {
        auto r = area.removeFromTop (34);
        g.setColour (juce::Colours::grey);
        g.setFont (12.0f);
        g.drawText (name, r.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String (db, 1) + " dB", r.withTrimmedBottom (0).translated (0, -16).withHeight (16),
                    juce::Justification::centredRight);

        auto bar = r.removeFromTop (10).toFloat();
        g.setColour (juce::Colour (0xff2b313a));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (juce::Colour (0xffe8a33d));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * std::clamp (db / 12.0f, 0.0f, 1.0f)), 3.0f);
    };

    grBar ("MB Low",    shownGr[2]);
    grBar ("MB LoMid",  shownGr[3]);
    grBar ("MB HiMid",  shownGr[4]);
    grBar ("MB High",   shownGr[5]);
    grBar ("Bus comp",  shownGr[1]);
    grBar ("Limiter",   shownGr[0]);
}
