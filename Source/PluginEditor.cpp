#include "PluginEditor.h"

namespace
{
const juce::Colour kPanel  { 0xff1d3527 };
const juce::Colour kPanel2 { 0xff142619 };
const juce::Colour kCream  { 0xffeadfb4 };
const juce::Colour kAmber  { 0xffe9a23b };
}

AmpLookAndFeel::AmpLookAndFeel()
{
    setColour (juce::Label::textColourId, kCream);
    setColour (juce::Slider::trackColourId, kCream);
    setColour (juce::Slider::thumbColourId, kCream);
    setColour (juce::Slider::backgroundColourId, kPanel2);
    setColour (juce::TextEditor::textColourId, kCream);
    setColour (juce::TextEditor::backgroundColourId, kPanel2);
}

void AmpLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool stepped = s.getInterval() >= 1.0 && s.getMaximum() <= 3.0 && s.getMaximum() >= 1.0;

    g.setColour (kCream.withAlpha (0.8f));
    if (stepped)
    {
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        for (int i = 0; i <= (int) s.getMaximum(); ++i)
        {
            const float a = startAngle + (float) i / (float) s.getMaximum() * (endAngle - startAngle);
            const float r = radius - 4.0f;
            g.drawText (juce::String (i + 1),
                        juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ centre.x + std::sin (a) * r, centre.y - std::cos (a) * r }),
                        juce::Justification::centred);
        }
    }
    else
    {
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const float r1 = radius, r2 = radius - (i % 5 == 0 ? 7.0f : 4.0f);
            g.drawLine (centre.x + std::sin (a) * r1, centre.y - std::cos (a) * r1,
                        centre.x + std::sin (a) * r2, centre.y - std::cos (a) * r2, 1.2f);
        }
    }

    // cream chicken-head style knob
    const float kr = radius - 14.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4ecc8), centre.x - kr, centre.y - kr,
                                             juce::Colour (0xff9b9170), centre.x + kr, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff3b3622));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-2.2f, -kr + 2.0f, 4.4f, kr * 0.6f, 1.5f);
    g.setColour (juce::Colour (0xff1a1a14));
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void AmpLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? kAmber : kPanel2);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (highlighted ? kCream : kCream.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 8.0f, 1.5f);
    g.setColour (on ? juce::Colour (0xff1a1a14) : kCream);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

GT120Editor::GT120Editor (GT120AudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (660, 470);

    addKnob (input,     "input",     "INPUT");
    addKnob (bassBoost, "bassboost", "BASS BOOST");
    addKnob (drive,     "drive",     "DRIVE");
    addKnob (volume,    "volume",    "VOLUME");
    addKnob (bass,      "bass",      "BASS");
    addKnob (treble,    "treble",    "TREBLE");
    addKnob (presence,  "presence",  "PRESENCE");
    addKnob (sag,       "sag",       "SAG");

    addAndMakeVisible (cabButton);
    cabAtt = std::make_unique<ButtonAtt> (proc.apvts, "cab", cabButton);

    outLabel.setText ("OUTPUT", juce::dontSendNotification);
    outLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (outLabel);
    outSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    outSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (outSlider);
    outAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outSlider);
}

GT120Editor::~GT120Editor() { setLookAndFeel (nullptr); }

void GT120Editor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    auto* param = proc.apvts.getParameter (id);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void GT120Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0f0c));
    const auto body = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (kPanel.brighter (0.08f), 0, 0, kPanel2, 0, (float) getHeight(), false));
    g.fillRoundedRectangle (body, 8.0f);
    g.setColour (kCream.withAlpha (0.85f));
    g.drawRoundedRectangle (body.reduced (6.0f), 6.0f, 1.5f);

    g.setColour (kCream);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("GT120 SIM", getLocalBounds().removeFromTop (62).withTrimmedTop (14), juce::Justification::centred);
    g.setColour (kAmber);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("120 watt valve head  -  4 x EL34  /  2 x ECC83", getLocalBounds().removeFromTop (82).withTrimmedTop (62),
                juce::Justification::centred);
    g.setColour (kCream.withAlpha (0.55f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("unofficial emulation, not affiliated with any manufacturer",
                getLocalBounds().removeFromBottom (22), juce::Justification::centred);
}

void GT120Editor::resized()
{
    auto r = getLocalBounds().reduced (22);
    r.removeFromTop (74);

    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<Knob*> knobs)
    {
        const int w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto cell = row.removeFromLeft (w);
            k->label.setBounds (cell.removeFromBottom (20));
            k->slider.setBounds (cell);
        }
    };
    layoutRow (r.removeFromTop (130), { &input, &bassBoost, &drive, &volume });
    r.removeFromTop (6);
    layoutRow (r.removeFromTop (130), { &bass, &treble, &presence, &sag });

    r.removeFromTop (10);
    auto bottom = r.removeFromTop (30);
    cabButton.setBounds (bottom.removeFromLeft (80));
    bottom.removeFromLeft (24);
    outLabel.setBounds (bottom.removeFromLeft (56));
    outSlider.setBounds (bottom);
}
