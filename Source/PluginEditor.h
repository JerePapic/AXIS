/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class AXISAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    AXISAudioProcessorEditor (AXISAudioProcessor&);
    ~AXISAudioProcessorEditor() override = default;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:

    AXISAudioProcessor& processor;
    juce::Image background;

    juce::Slider rotation, mass, body, load, wear;
    
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> attRotation, attMass, attBody, attLoad, attWear;
    
    static constexpr int baseW = 400;
    static constexpr int baseH = 600;
    
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AXISAudioProcessorEditor)
};
