/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQ1AudioProcessor::SimpleEQ1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

SimpleEQ1AudioProcessor::~SimpleEQ1AudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEQ1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQ1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQ1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQ1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEQ1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQ1AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQ1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQ1AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEQ1AudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEQ1AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEQ1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //(4) prepare the filters to use (=pass the process spec) +add its attributes
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1; //each monochain can handle only 1 channel...

    //prepare the L and R monochain to run with the spec parameters...
    leftChain.prepare(spec);
    rightChain.prepare(spec);

    updateFilters();

    
}

void SimpleEQ1AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQ1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleEQ1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    //(13) refactoring made it so easy to just call....
    updateFilters();
    

    //(5) create an audio block to wrap the buffer + extract channels + create ptocessing context
    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    //pass the contexts to the mono-chains of the filters
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool SimpleEQ1AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleEQ1AudioProcessor::createEditor()
{
    //return new SimpleEQ1AudioProcessorEditor (*this);
    //(2)
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQ1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleEQ1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


//(6) define the helper function that gives us the parameters from the chain
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    return settings;
}

//(10) refactoring, update peak
void SimpleEQ1AudioProcessor::updatePeakFilter(const ChainSettings& chainSettings) {

    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(),
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels)
    );

    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients,
        peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients,
        peakCoefficients);
}

//(10) refactoring, update peak filter coeff
void SimpleEQ1AudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements) {
    *old = *replacements;
}

//(13) refactoring, function that updates all filters at once (+her helpers)

void SimpleEQ1AudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings) {
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.lowCutFreq,
        getSampleRate(),
        (chainSettings.lowCutSlope + 1) * 2 //setting the order parameter
    );

    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);
}

void SimpleEQ1AudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings) {
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
        chainSettings.highCutFreq,
        getSampleRate(),
        (chainSettings.highCutSlope + 1) * 2 //setting the order parameter
    );

    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, cutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, cutCoefficients, chainSettings.highCutSlope);
}

void SimpleEQ1AudioProcessor::updateFilters() {
    //first get hold of the chain settings
    auto chainSettings = getChainSettings(apvts);
    updatePeakFilter(chainSettings);

    //now you can call teh helpers
    updateLowCutFilters(chainSettings);
    updateHighCutFilters(chainSettings);
}

//(1) declare the parameter layout used by the audio processor value tree state
juce::AudioProcessorValueTreeState::ParameterLayout
SimpleEQ1AudioProcessor::createParameterLayout() {

    //first of all we create the layout itself
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    //we can now add paramaters to the layout
    layout.add(std::make_unique< juce::AudioParameterFloat>(
        "LowCut Freq",                                           //parameter ID
        "LowCut Freq",                                           //parameter name
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, .25f), //range, step value=1, skew
        20.f)                                                    //default value
    );

    layout.add(std::make_unique< juce::AudioParameterFloat>(
        "HighCut Freq",                                             //parameter ID
        "HighCut Freq",                                             //parameter name
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, .25f),    //range, step value=1, skew
        20000.f)                                                    //default value
    );

    layout.add(std::make_unique< juce::AudioParameterFloat>(
        "Peak Freq",                                                //parameter ID
        "Peak Freq",                                                //parameter name
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, .25f),   //range, step value=1, skew=0.25
        750.f)                                                      //default value
    );

    layout.add(std::make_unique< juce::AudioParameterFloat>(
        "Peak Gain",                                                //parameter ID
        "Peak Gain",                                                //parameter name
        juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),     //range, step value=0.5, skew
        0.0f)                                                       //default value
    );
    
    layout.add(std::make_unique< juce::AudioParameterFloat>(
        "Peak Quality",                                              //parameter ID
        "Peak Quality",                                              //parameter name
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),      //range, step value=0.5, skew
        1.f)                                                         //default value
    );

    //create the choices for steepness of LCF and HCF
    juce::StringArray stringArray;
    for (int i = 0; i < 4; i++) {
        juce::String str;
        str << (12 + i * 12);
        str << " dB/Oct";
        stringArray.add(str);
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQ1AudioProcessor();
}
