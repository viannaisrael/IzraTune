#pragma once
#include "PluginProcessor.h"

class IzraTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit IzraTuneEditor (IzraTuneProcessor&);
    ~IzraTuneEditor() override;
    void resized() override;

private:
    using Resource = juce::WebBrowserComponent::Resource;
    std::optional<Resource> getResource (const juce::String& url) const;
    void timerCallback() override;

    IzraTuneProcessor& proc;

    // relays MUST be declared before the WebBrowserComponent, attachments after it
    juce::WebSliderRelay pitchRelay { "pitch" }, inRelay { "inGain" }, trimRelay { "outTrim" },
                         outRelay { "outGain" }, toneRelay { "tone" };
    juce::WebToggleButtonRelay phaseRelay { "phase" };
    juce::WebBrowserComponent web;
    juce::WebSliderParameterAttachment pitchAtt, inAtt, trimAtt, outAtt, toneAtt;
    juce::WebToggleButtonParameterAttachment phaseAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IzraTuneEditor)
};
