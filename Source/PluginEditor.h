#pragma once

#include "PluginProcessor.h"

class MetalMasterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MetalMasterAudioProcessorEditor (MetalMasterAudioProcessor&);
    ~MetalMasterAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using APVTS = juce::AudioProcessorValueTreeState;

    // A labelled rotary knob bound to a parameter.
    struct Knob : public juce::Component
    {
        Knob (APVTS& state, const juce::String& paramId, const juce::String& name, const juce::String& suffix)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
            slider.setTextValueSuffix (suffix);
            label.setText (name, juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centred);
            label.setMinimumHorizontalScale (0.6f);
            addAndMakeVisible (slider);
            addAndMakeVisible (label);
            attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            label.setBounds (b.removeFromTop (18));
            slider.setBounds (b);
        }

        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    // A checkbox bound to a bool parameter.
    struct ToggleBtn : public juce::Component
    {
        ToggleBtn (APVTS& state, const juce::String& paramId, const juce::String& text)
        {
            button.setButtonText (text);
            addAndMakeVisible (button);
            attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, button);
        }

        void resized() override
        {
            button.setBounds (getLocalBounds().withSizeKeepingCentre (std::max (20, getWidth() - 4), 28));
        }

        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    // Clipper on/off + type selector
    struct ClipperExtras : public juce::Component
    {
        explicit ClipperExtras (APVTS& state)
        {
            onButton.setButtonText ("Clipper On");
            typeBox.addItemList (juce::StringArray { "Soft (tanh)", "Hard" }, 1);
            addAndMakeVisible (onButton);
            addAndMakeVisible (typeBox);
            onAttach   = std::make_unique<APVTS::ButtonAttachment> (state, "clipOn", onButton);
            typeAttach = std::make_unique<APVTS::ComboBoxAttachment> (state, "clipType", typeBox);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (4);
            onButton.setBounds (b.removeFromTop (28));
            b.removeFromTop (8);
            typeBox.setBounds (b.removeFromTop (26));
        }

        juce::ToggleButton onButton;
        juce::ComboBox typeBox;
        std::unique_ptr<APVTS::ButtonAttachment> onAttach;
        std::unique_ptr<APVTS::ComboBoxAttachment> typeAttach;
    };

    // Preset drop-down
    struct PresetPanel : public juce::Component
    {
        PresetPanel() { addAndMakeVisible (box); }
        void resized() override
        {
            box.setBounds (getLocalBounds().withSizeKeepingCentre (std::max (40, getWidth() - 12), 30));
        }
        juce::ComboBox box;
    };

    struct Group
    {
        juce::String title;
        std::vector<std::vector<juce::Component*>> rows;
        float weight = 1.0f;
        juce::Rectangle<int> bounds;
    };

    struct RowDef
    {
        float height;
        std::vector<int> groupIdx;
    };

    juce::Component* addKnob (const juce::String& id, const juce::String& name, const juce::String& suffix);
    juce::Component* addToggle (const juce::String& id, const juce::String& text);
    int addGroup (const juce::String& title, float weight, std::vector<std::vector<juce::Component*>> rows);

    MetalMasterAudioProcessor& proc;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<ToggleBtn>> toggles;
    std::unique_ptr<ClipperExtras> clipExtras;
    std::unique_ptr<PresetPanel> presetPanel;

    juce::TextButton resetButton { "Reset Meters" };

    std::vector<Group> groups;
    std::vector<RowDef> rowDefs;
    juce::Rectangle<int> meterBounds;

    // Smoothed gain-reduction readouts: [0] limiter, [1] bus, [2..5] multiband bands
    float shownGr[6] { 0, 0, 0, 0, 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalMasterAudioProcessorEditor)
};
