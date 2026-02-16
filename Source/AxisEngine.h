#pragma once
#include <JuceHeader.h>

class AxisEngine
{
public:
    void prepare (double sampleRate);
    void process (juce::AudioBuffer<float>& buffer);

    void setRotation (float value);
    void setBody (float value);
    void setLoad (float value);
    void setMass (float value);
    void setWear (float value);

private:
    double sr = 44100.0;

    // Phase accumulators
    float phaseA = 0.0f;
    float phaseB = 0.0f;
    float phaseC = 0.0f;

    // Base frequency
    float baseFreq = 55.0f;

    // Spectral rotation
    float rotation = 0.3f;
    float body = 0.5f;
    float load = 0.4f;
    float mass = 0.5f;
    float wear = 0.2f;
    
    float crossModA = 0.0f;
    float crossModB = 0.0f;
    
    float rotationSmoothed = 0.0f;
          
    // Random drift state
    float driftA = 0.0f;
    float driftB = 0.0f;
    float driftTargetA = 0.0f;
    float driftTargetB = 0.0f;

    // Simple random generator
    juce::Random random;

    float spectralPhase = 0.0f;
    
    // Inertia smoothing for filter centers
    float smoothedFcA = 400.0f;
    float smoothedFcB = 600.0f;

    // Simple damping lowpass state (post)
    float dampL = 0.0f;
    float dampR = 0.0f;

    // Sub oscillator phase
    float phaseSub = 0.0f;

    // Filters (stereo-safe TPT)
    juce::dsp::StateVariableTPTFilter<float> filterA_L, filterA_R;
    juce::dsp::StateVariableTPTFilter<float> filterB_L, filterB_R;
};

