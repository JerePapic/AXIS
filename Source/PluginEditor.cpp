/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AXISAudioProcessorEditor::AXISAudioProcessorEditor (AXISAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::AXIS_BG_png, BinaryData::AXIS_BG_pngSize);
    setSize (baseW, baseH);
    
    auto setupKnob = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f,
                               true);
        s.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    };

    setupKnob (rotation);
    setupKnob (mass);
    setupKnob (body);
    setupKnob (load);
    setupKnob (wear);

    addAndMakeVisible (rotation);
    addAndMakeVisible (mass);
    addAndMakeVisible (body);
    addAndMakeVisible (load);
    addAndMakeVisible (wear);

    // Attach to your parameter IDs
    attRotation = std::make_unique<Attachment> (processor.apvts, "ROTATION", rotation);
    attMass     = std::make_unique<Attachment> (processor.apvts, "MASS",     mass);
    attBody     = std::make_unique<Attachment> (processor.apvts, "BODY",     body);
    attLoad     = std::make_unique<Attachment> (processor.apvts, "LOAD",     load);
    attWear     = std::make_unique<Attachment> (processor.apvts, "WEAR",     wear);
}


//==============================================================================
void AXISAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // Draw background scaled to editor size
    g.drawImage (background, getLocalBounds().toFloat());
}

void AXISAudioProcessorEditor::resized()
{
    const float sx = getWidth()  / (float) baseW;
    const float sy = getHeight() / (float) baseH;

    auto S = [sx, sy](int x, int y, int w, int h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * sx),
                                     juce::roundToInt (y * sy),
                                     juce::roundToInt (w * sx),
                                     juce::roundToInt (h * sy));
    };

    // Exact placements derived from the PNG (2x -> /2)
    rotation.setBounds (S (110, 210, 180, 180));

    mass.setBounds     (S ( 50, 125,  50,  50));
    body.setBounds     (S (300, 125,  50,  50));
    load.setBounds     (S ( 50, 450,  50,  50));
    wear.setBounds     (S (300, 450,  50,  50));
}
