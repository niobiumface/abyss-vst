#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "Version.h"
#include "presets/PresetManager.h"

namespace abyss
{

AbyssAudioProcessor::AbyssAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createParameterLayout())
{
    cachePointers();
    juce::ignoreUnused (kVersionString);
}

void AbyssAudioProcessor::cachePointers()
{
    ParamPointers p;
    p.depth = apvts.getRawParameterValue (id::depth);
    p.pressure = apvts.getRawParameterValue (id::pressure);
    p.inGain = apvts.getRawParameterValue (id::inGain);
    p.outGain = apvts.getRawParameterValue (id::outGain);
    p.mix = apvts.getRawParameterValue (id::mix);
    p.subMode = apvts.getRawParameterValue (id::subMode);
    p.sub2 = apvts.getRawParameterValue (id::sub2);
    p.ceiling = apvts.getRawParameterValue (id::ceiling);
    p.autoGain = apvts.getRawParameterValue (id::autoGain);
    p.os = apvts.getRawParameterValue (id::os);
    p.bypass = apvts.getRawParameterValue (id::bypass);
    engine.setParameters (p);
}

void AbyssAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = std::max (getTotalNumInputChannels(), getTotalNumOutputChannels());
    engine.prepare (sampleRate, samplesPerBlock, std::max (1, ch));
    int latency = 0;
    if (engine.consumeLatencyChange (latency))
        setLatencySamples (latency);
    else
        setLatencySamples (engine.getLatencySamples());
}

void AbyssAudioProcessor::releaseResources()
{
    engine.reset();
}

bool AbyssAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void AbyssAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    engine.process (buffer);

    int latency = 0;
    if (engine.consumeLatencyChange (latency))
        setLatencySamples (latency);
}

void AbyssAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);
    engine.processBypassed (buffer);
    int latency = 0;
    if (engine.consumeLatencyChange (latency))
        setLatencySamples (latency);
}

juce::AudioProcessorEditor* AbyssAudioProcessor::createEditor()
{
    return new AbyssAudioProcessorEditor (*this);
}

void AbyssAudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, 9, index);
    program.store (index);
    applyFactoryPreset (apvts, index);
}

const juce::String AbyssAudioProcessor::getProgramName (int index)
{
    return factoryPreset (index).name;
}

std::unique_ptr<juce::XmlElement> AbyssAudioProcessor::createStateXml()
{
    auto root = std::make_unique<juce::XmlElement> ("AbyssState");
    root->setAttribute ("stateVersion", 1);
    root->setAttribute ("uiScale", uiScale.load());
    root->setAttribute ("animate", animate.load() ? 1 : 0);
    root->setAttribute ("crtFx", crtFx.load() ? 1 : 0);
    if (auto params = apvts.copyState().createXml())
        root->addChildElement (params.release());
    return root;
}

void AbyssAudioProcessor::loadStateXml (const juce::XmlElement& xml)
{
    juce::MemoryBlock block;
    copyXmlToBinary (xml, block);
    setStateInformation (block.getData(), (int) block.getSize());
}

void AbyssAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto root = createStateXml())
        copyXmlToBinary (*root, destData);
}

void AbyssAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const int scale = xml->getIntAttribute ("uiScale", 3);
    uiScale.store (juce::jlimit (1, 5, scale >= 1 && scale <= 5 ? scale : 3));
    animate.store (xml->getIntAttribute ("animate", 1) != 0);
    crtFx.store (xml->getIntAttribute ("crtFx", 1) != 0);

    if (auto* params = xml->getChildByName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*params);
        if (tree.isValid())
            apvts.replaceState (tree);
    }

    sendChangeMessage();
}

juce::AudioProcessorParameter* AbyssAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (id::bypass);
}

} // namespace abyss

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new abyss::AbyssAudioProcessor();
}
