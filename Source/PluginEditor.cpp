#include "PluginEditor.h"
#include "BinaryData.h"

static std::vector<std::byte> toBytes (const char* d, int n)
{
    auto* p = reinterpret_cast<const std::byte*> (d);
    return { p, p + n };
}

IzraTuneEditor::IzraTuneEditor (IzraTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      web (juce::WebBrowserComponent::Options{}
               .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (juce::WebBrowserComponent::Options::WinWebView2{}
                   .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
               .withNativeIntegrationEnabled()
               .withOptionsFrom (pitchRelay).withOptionsFrom (inRelay).withOptionsFrom (trimRelay)
               .withOptionsFrom (outRelay).withOptionsFrom (toneRelay).withOptionsFrom (phaseRelay)
               .withResourceProvider ([this] (const auto& url) { return getResource (url); })),
      pitchAtt (*p.apvts.getParameter ("pitch"),   pitchRelay, nullptr),
      inAtt    (*p.apvts.getParameter ("inGain"),  inRelay,    nullptr),
      trimAtt  (*p.apvts.getParameter ("outTrim"), trimRelay,  nullptr),
      outAtt   (*p.apvts.getParameter ("outGain"), outRelay,   nullptr),
      toneAtt  (*p.apvts.getParameter ("tone"),    toneRelay,  nullptr),
      phaseAtt (*p.apvts.getParameter ("phase"),   phaseRelay, nullptr)
{
    addAndMakeVisible (web);
    web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    setSize (1068, 546);
    startTimerHz (30);
}

IzraTuneEditor::~IzraTuneEditor() { stopTimer(); }
void IzraTuneEditor::resized() { web.setBounds (getLocalBounds()); }

std::optional<IzraTuneEditor::Resource> IzraTuneEditor::getResource (const juce::String& url) const
{
    const auto path = url == "/" ? juce::String ("index.html") : url.fromFirstOccurrenceOf ("/", false, false);
    if (path == "index.html")        return Resource { toBytes (BinaryData::index_html, BinaryData::index_htmlSize), "text/html" };
    if (path == "juce_frontend.js")  return Resource { toBytes (BinaryData::juce_frontend_js, BinaryData::juce_frontend_jsSize), "text/javascript" };
    return std::nullopt;
}

void IzraTuneEditor::timerCallback()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("l", proc.meterL.load());
    o->setProperty ("r", proc.meterR.load());
    web.emitEventIfBrowserIsVisible ("meter", juce::var (o));
}
