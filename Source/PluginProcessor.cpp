#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout GT120AudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto knob = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    auto db = [&p] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                  juce::NormalisableRange<float> (lo, hi, 0.1f), def));
    };
    db   ("input",     "Input",      -24.0f, 24.0f, 0.0f);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "drive", 1 }, "Drive",
                                                               juce::StringArray { "1", "2", "3", "4" }, 2));
    knob ("bassboost", "Bass Boost", 0.00f);
    knob ("volume",    "Volume",     0.50f);
    knob ("bass",      "Bass",       0.50f);
    knob ("treble",    "Treble",     0.60f);
    knob ("presence",  "Presence",   0.40f);
    knob ("sag",       "Sag",        0.25f);
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "cab", 1 }, "Cab", true));
    db   ("output",    "Output",     -24.0f, 12.0f, -6.0f);
    return { p.begin(), p.end() };
}

GT120AudioProcessor::GT120AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pIn = apvts.getRawParameterValue ("input");
    pDrive = apvts.getRawParameterValue ("drive");
    pBB = apvts.getRawParameterValue ("bassboost");
    pVol = apvts.getRawParameterValue ("volume");
    pBass = apvts.getRawParameterValue ("bass");
    pTreble = apvts.getRawParameterValue ("treble");
    pPres = apvts.getRawParameterValue ("presence");
    pSag = apvts.getRawParameterValue ("sag");
    pCab = apvts.getRawParameterValue ("cab");
    pOut = apvts.getRawParameterValue ("output");
}

void GT120AudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatencySamples());
}

bool GT120AudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void GT120AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    gt120::Params p;
    p.inputDb   = pIn->load();
    p.drive     = (int) std::lround (pDrive->load());
    p.bassBoost = pBB->load();
    p.volume    = pVol->load();
    p.bass      = pBass->load();
    p.treble    = pTreble->load();
    p.presence  = pPres->load();
    p.sag       = pSag->load();
    p.cabOn     = pCab->load() > 0.5f;
    p.outputDb  = pOut->load();

    engine.process (buffer, p);
}

juce::AudioProcessorEditor* GT120AudioProcessor::createEditor() { return new GT120Editor (*this); }

void GT120AudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void GT120AudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GT120AudioProcessor(); }
