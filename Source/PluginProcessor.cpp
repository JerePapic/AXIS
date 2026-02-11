/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AXISAudioProcessor::AXISAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
    #if ! JucePlugin_IsMidiEffect
     #if ! JucePlugin_IsSynth
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
     #endif
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
    #endif
    )
    , apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
}

AXISAudioProcessor::~AXISAudioProcessor()
{
}

//==============================================================================
const juce::String AXISAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AXISAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AXISAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AXISAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AXISAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AXISAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AXISAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AXISAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AXISAudioProcessor::getProgramName (int index)
{
    return {};
}

void AXISAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AXISAudioProcessor::createParameterLayout()
{
    using Param = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("ROTATION", "Rotation", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 0.5f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("MASS", "Mass", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 0.5f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("BODY", "Body", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 0.5f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("LOAD", "Load", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 0.5f), 0.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> ("WEAR", "Wear", juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f, 0.5f), 0.2f));

    return { params.begin(), params.end() };
}


//==============================================================================
void AXISAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
   engine.prepare (sampleRate);
}

void AXISAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AXISAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AXISAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto rot  = apvts.getRawParameterValue ("ROTATION")->load();
    auto body = apvts.getRawParameterValue ("BODY")->load();
    auto load = apvts.getRawParameterValue ("LOAD")->load();
    auto mass = apvts.getRawParameterValue ("MASS")->load();
    auto wear = apvts.getRawParameterValue ("WEAR")->load();

    engine.setRotation (rot);
    engine.setBody (body);
    engine.setLoad (load);
    engine.setMass (mass);
    engine.setWear (wear);

    engine.process (buffer);
}


//==============================================================================
bool AXISAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AXISAudioProcessor::createEditor()
{
    return new AXISAudioProcessorEditor (*this);
}

//==============================================================================
void AXISAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AXISAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AXISAudioProcessor();
}
