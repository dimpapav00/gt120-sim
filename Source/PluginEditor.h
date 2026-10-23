#pragma once
#include "PluginProcessor.h"

class AmpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmpLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class GT120Editor : public juce::AudioProcessorEditor
{
public:
    explicit GT120Editor (GT120AudioProcessor&);
    ~GT120Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);

    GT120AudioProcessor& proc;
    AmpLookAndFeel lnf;

    Knob input, bassBoost, drive, volume, bass, treble, presence, sag;
    juce::ToggleButton cabButton { "CAB" };
    juce::Label outLabel;
    juce::Slider outSlider;

    std::unique_ptr<ButtonAtt> cabAtt;
    std::unique_ptr<SliderAtt> outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GT120Editor)
};
