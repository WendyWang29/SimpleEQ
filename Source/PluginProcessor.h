/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//(9) create enum for the slope parameters
enum Slope {
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};


//(6) extract the APVTS parameters and group them in a structure
struct ChainSettings {
    float peakFreq{ 0 },
          peakGainInDecibels{ 0 }, 
          peakQuality{ 1.f };
    float lowCutFreq{ 0 }, 
          highCutFreq{ 0 };
    Slope lowCutSlope{ Slope::Slope_12 },    //these get modified at (9)
          highCutSlope{ Slope::Slope_12 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class SimpleEQ1AudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    SimpleEQ1AudioProcessor();
    ~SimpleEQ1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //(1) declare the Audio Processor Value Tree State + parameter layout

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    juce::AudioProcessorValueTreeState apvts{*this, nullptr, "Parameters", createParameterLayout()};

private:

    //(3) create aliases for all the namespaces in the juce::dsp modules

    //peak filter
    using Filter = juce::dsp::IIR::Filter<float>;

    //adjustable slope filter (LP or HP)
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

    //mono chain (LP + parametric + HP)
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    MonoChain leftChain, rightChain;


    //(7) definition of enum to access the links in the chain
    enum ChainPositions {
        LowCut, Peak, HighCut
    };

    //(10) refactoring (start with stuff that configures the peak filter)
    void updatePeakFilter(const ChainSettings& chainSettings);

    //(10) create alias for IIR coefficients
    using Coefficients = Filter::CoefficientsPtr;
    
    //(10) function for updating cut filter coefficients
    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQ1AudioProcessor)
};
