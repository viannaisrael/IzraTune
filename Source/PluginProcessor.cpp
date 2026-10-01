#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using Range = juce::NormalisableRange<float>;

juce::AudioProcessorValueTreeState::ParameterLayout IzraTuneProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<APF> (juce::ParameterID { "pitch",   1 }, "Pitch",       Range (-12.f, 12.f, 0.01f), 0.f,
                                  juce::AudioParameterFloatAttributes().withLabel ("st")));
    l.add (std::make_unique<APF> (juce::ParameterID { "inGain",  1 }, "Input",       Range (-24.f, 24.f, 0.1f), 0.f,
                                  juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<APF> (juce::ParameterID { "outTrim", 1 }, "Output",      Range (-24.f, 24.f, 0.1f), 0.f,
                                  juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<APF> (juce::ParameterID { "outGain", 1 }, "Output Gain", Range (-24.f, 24.f, 0.1f), 0.f,
                                  juce::AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<APF> (juce::ParameterID { "tone",    1 }, "Tone",        Range (0.f, 1.f, 0.001f), 1.f));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "phase", 1 }, "Phase Invert", false));
    return l;
}

IzraTuneProcessor::IzraTuneProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pPitch = apvts.getRawParameterValue ("pitch");   pIn    = apvts.getRawParameterValue ("inGain");
    pTrim  = apvts.getRawParameterValue ("outTrim"); pOut   = apvts.getRawParameterValue ("outGain");
    pTone  = apvts.getRawParameterValue ("tone");    pPhase = apvts.getRawParameterValue ("phase");
}

bool IzraTuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == l.getMainInputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() || l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void IzraTuneProcessor::prepareToPlay (double rate, int block)
{
    sr = rate;
    setLatencySamples (PitchShifter::latency);
    juce::dsp::ProcessSpec spec { rate, (juce::uint32) block, 1 };
    for (auto& s : shifters) s.reset();
    for (auto& d : dryDelay) { d.prepare (spec); d.reset(); d.setDelay ((float) PitchShifter::latency); }
    for (auto& f : lpf) { f.prepare (spec); f.setType (juce::dsp::StateVariableTPTFilterType::lowpass); f.setResonance (0.7071f); f.reset(); }
    auto init = [rate] (auto& s, float v, double t = 0.03) { s.reset (rate, t); s.setCurrentAndTargetValue (v); };
    init (inGain, 1.f); init (trimGain, 1.f); init (outGain, 1.f); init (polarity, 1.f); init (wetMix, 0.f, 0.02); init (pitchSt, 0.f, 0.05);
    lastTone = -1.f;
}

void IzraTuneProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), chans = juce::jmin (buf.getNumChannels(), 2);
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buf.clear (c, 0, n);

    inGain.setTargetValue   (juce::Decibels::decibelsToGain (pIn->load()));
    trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    outGain.setTargetValue  (juce::Decibels::decibelsToGain (pOut->load()));
    polarity.setTargetValue (pPhase->load() > 0.5f ? -1.f : 1.f);
    pitchSt.setTargetValue  (pPitch->load());
    wetMix.setTargetValue   (std::abs (pPitch->load()) < 0.005f ? 0.f : 1.f);   // bit-transparent (delay-matched) at 0 st

    const float tone = pTone->load();
    const bool lpfOn = tone < 0.999f;
    if (tone != lastTone)
    {
        lastTone = tone;
        const float fc = juce::jlimit (20.f, (float) (sr * 0.45), 20.f * std::pow (1000.f, tone));   // 20 Hz .. 20 kHz
        for (auto& f : lpf) f.setCutoffFrequency (fc);
    }

    float pkL = 0.f, pkR = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float st = pitchSt.getNextValue();
        const float g = inGain.getNextValue() * 1.f, mix = wetMix.getNextValue();
        const float post = trimGain.getNextValue() * outGain.getNextValue() * polarity.getNextValue();
        const float ratio = std::exp2 (st / 12.f);
        for (int c = 0; c < chans; ++c)
        {
            auto* d = buf.getWritePointer (c);
            const float x = d[i] * g;
            shifters[(size_t) c].setRatio (ratio);
            const float wet = shifters[(size_t) c].process (x);
            dryDelay[(size_t) c].pushSample (0, x);
            const float dry = dryDelay[(size_t) c].popSample (0);
            float y = dry + (wet - dry) * mix;
            if (lpfOn) { y = lpf[(size_t) c * 2].processSample (0, y); y = lpf[(size_t) c * 2 + 1].processSample (0, y); }
            y *= post;
            d[i] = y;
            (c == 0 ? pkL : pkR) = juce::jmax (c == 0 ? pkL : pkR, std::abs (y));
        }
    }
    if (chans == 1) pkR = pkL;
    meterL.store (juce::jmax (pkL, meterL.load() * 0.9f));
    meterR.store (juce::jmax (pkR, meterR.load() * 0.9f));
}

void IzraTuneProcessor::getStateInformation (juce::MemoryBlock& m)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, m);
}
void IzraTuneProcessor::setStateInformation (const void* d, int s)
{
    if (auto xml = getXmlFromBinary (d, s)) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}
juce::AudioProcessorEditor* IzraTuneProcessor::createEditor() { return new IzraTuneEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IzraTuneProcessor(); }
