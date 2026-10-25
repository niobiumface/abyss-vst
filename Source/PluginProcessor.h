#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/AbyssEngine.h"

#include <atomic>

namespace abyss
{

class AbyssAudioProcessor : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster
{
public:
    AbyssAudioProcessor();
    ~AbyssAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Abyss"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.6; }

    int getNumPrograms() override { return 10; }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    std::unique_ptr<juce::XmlElement> createStateXml();
    void loadStateXml (const juce::XmlElement& xml);

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;
    AbyssEngine engine;

    std::atomic<int> uiScale { 3 };
    std::atomic<bool> animate { true };
    std::atomic<bool> crtFx { true };

private:
    void cachePointers();
    std::atomic<int> program { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssAudioProcessor)
};

} // namespace abyss
