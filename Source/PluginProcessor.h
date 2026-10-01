#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PitchShifter.h"

class IzraTuneProcessor : public juce::AudioProcessor
{
public:
    IzraTuneProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "IzraTune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return PitchShifter::latency / 44100.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> meterL { 0.f }, meterR { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<PitchShifter, 2> shifters;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>, 2> dryDelay
        { juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> (PitchShifter::latency + 8),
          juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> (PitchShifter::latency + 8) };
    std::array<juce::dsp::StateVariableTPTFilter<float>, 4> lpf;   // 2 channels x 2 stages = 24 dB/oct
    juce::SmoothedValue<float> inGain, trimGain, outGain, pitchSt, wetMix, polarity;
    float lastTone = -1.f;
    double sr = 44100.0;

    std::atomic<float>* pPitch, *pIn, *pTrim, *pOut, *pTone, *pPhase;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IzraTuneProcessor)
};
