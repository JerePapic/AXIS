#include "AxisEngine.h"

void AxisEngine::prepare (double sampleRate)
{
    sr = sampleRate;

    phaseA = phaseB = phaseC = 0.0f;
    phaseSub = 0.0f;
    spectralPhase = 0.0f;

    // Init smoothing / damping state
    smoothedFcA = 400.0f;
    smoothedFcB = 600.0f;
    dampL = dampR = 0.0f;

    // WEAR drift state
    driftA = driftB = 0.0f;
    driftTargetA = driftTargetB = 0.0f;

    juce::dsp::ProcessSpec spec { sr, 512, 1 };

    filterA_L.prepare (spec);
    filterA_R.prepare (spec);
    filterB_L.prepare (spec);
    filterB_R.prepare (spec);

    filterA_L.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    filterA_R.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    filterB_L.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    filterB_R.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

    filterA_L.reset();
    filterA_R.reset();
    filterB_L.reset();
    filterB_R.reset();
}

void AxisEngine::setRotation (float value)
{
    rotation = juce::jlimit (0.0f, 1.0f, value);
}

void AxisEngine::setBody (float value)
{
    body = juce::jlimit (0.0f, 1.0f, value);
}

void AxisEngine::setLoad (float value)
{
    load = juce::jlimit (0.0f, 1.0f, value);
}

void AxisEngine::setMass (float value)
{
    mass = juce::jlimit (0.0f, 1.0f, value);
}

void AxisEngine::setWear (float value)
{
    wear = juce::jlimit (0.0f, 1.0f, value);
}


static inline float diodeClip (float x, float drive, float asym)
{
    float kPos = drive;
    float kNeg = drive * asym;

    if (x >= 0.0f)
        return x / (1.0f + kPos * std::abs (x));
    else
        return x / (1.0f + kNeg * std::abs (x));
}


void AxisEngine::process (juce::AudioBuffer<float>& buffer)
{
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (numCh > 1 ? 1 : 0);

    // ---- Block-level mappings (polish: avoid recalculating per sample) ----

    // WEAR drift settings
    const float driftAmount   = juce::jmap (wear, 0.0f, 0.15f);
    const float driftSpeedHz  = juce::jmap (wear, 0.1f, 2.0f);
    const int   driftInterval = juce::jmax (1, (int) (sr / driftSpeedHz));
    
    // Torque: MASS controls inertia of rotation
    const float torqueSpeed = juce::jmap (mass, 0.2f, 0.01f); // low mass = fast response

    rotationSmoothed += torqueSpeed * (rotation - rotationSmoothed);

    // ROTATION + MASS: speed & depth
    const float rotationRateBase = juce::jmap (rotationSmoothed, 0.0005f, 0.03f);
    const float rotationRate     = rotationRateBase * juce::jmap (mass, 1.0f, 0.35f);

    const float sweepOctavesBase = juce::jmap (rotationSmoothed, 0.2f, 3.0f);
    const float sweepOctaves     = sweepOctavesBase * juce::jmap (mass, 1.0f, 0.45f);

    // BODY: spectral center bias
    const float baseCentre = juce::jmap (body, 80.0f, 1200.0f);

    // Phase relationship between the two rotating filters
    const float phaseOffset = juce::MathConstants<float>::pi * 0.5f;

    // MASS inertia smoothing (1-pole)
    const float tauSeconds = juce::jmap (mass, 0.02f, 0.60f);
    const float a = std::exp (-1.0f / (tauSeconds * (float) sr));

    // BODY as topology control
    const float bodyLow  = juce::jlimit (0.0f, 1.0f, body * 3.0f);          // 0..1
    const float bodyMid  = juce::jlimit (0.0f, 1.0f, body * 3.0f - 1.0f);   // 0..1
    const float bodyHigh = juce::jlimit (0.0f, 1.0f, body * 3.0f - 2.0f);   // 0..1
    
    // Base resonance per regime
    const float resLow  = juce::jmap (bodyLow,  0.25f, 1.0f);
    const float resMid  = juce::jmap (bodyMid,  1.0f, 3.5f);
    const float resHigh = juce::jmap (bodyHigh, 3.5f, 6.5f); // LOWER than before
    
    // BODY high enables cross-mod, MASS limits it
    const float crossAmount = bodyHigh * juce::jmap (mass, 0.4f, 0.1f);

    // Crossfade regimes
    float resonance =
        resLow * (1.0f - bodyMid)
        + resMid * (1.0f - bodyHigh)
        + resHigh * bodyHigh;

    // LOAD safety scaling
    resonance *= juce::jmap (load, 1.0f, 0.65f);

    // BODY high creates asymmetrical Q between filters
    const float qSkew = bodyHigh * 0.35f;

    filterA_L.setResonance (resonance * (1.0f + qSkew));
    filterA_R.setResonance (resonance * (1.0f + qSkew));
    filterB_L.setResonance (resonance * (1.0f - qSkew));
    filterB_R.setResonance (resonance * (1.0f - qSkew));

    // LOAD drive
    const float preGainDb = juce::jmap (load, 0.0f, 24.0f);
    const float preGain   = juce::Decibels::decibelsToGain (preGainDb);
    const float postTrim  = juce::jmap (load, 1.0f, 0.25f);

    // MASS: sub amount, damping mix, damping filter coeff
    const float subGain  = juce::jmap (mass, 0.0f, 0.35f);
    const float dampMix  = juce::jmap (mass, 0.0f, 0.65f);

    const float dampCut = juce::jmap (mass, 10000.0f, 1200.0f);
    const float x = std::exp (-juce::MathConstants<float>::twoPi * dampCut / (float) sr);
    const float g = 1.0f - x;

    // WEAR: oscillator instability + post saturation
    const float instability = juce::jmap (wear, 0.0f, 0.003f);
    const float wearDrive   = juce::jmap (wear, 1.0f, 3.5f);

    // ---- Sample loop ----
    for (int i = 0; i < numSamples; ++i)
    {
        // Drift update (occasionally retarget, then smooth toward target)
        if (i % driftInterval == 0)
        {
            driftTargetA = random.nextFloat() * 2.0f - 1.0f;
            driftTargetB = random.nextFloat() * 2.0f - 1.0f;
        }

        driftA += 0.0005f * (driftTargetA - driftA);
        driftB += 0.0005f * (driftTargetB - driftB);

        // Spectral rotation phase
        spectralPhase += rotationRate / (float) sr;
        if (spectralPhase > 1.0f)
            spectralPhase -= 1.0f;

        const float phi = spectralPhase * juce::MathConstants<float>::twoPi;

        // Rotating modulators
        const float modA = std::sin (phi);
        const float modB = std::sin (phi + phaseOffset);

        // Exponential frequency sweep (use exp2 for 2^x)
        float fcA = baseCentre * std::exp2 (modA * sweepOctaves);
        float fcB = baseCentre * std::exp2 (modB * sweepOctaves);

        // WEAR drift on filter centers
        fcA *= (1.0f + driftA * driftAmount);
        fcB *= (1.0f + driftB * driftAmount);

        // Safety clamp
        fcA = juce::jlimit (20.0f, 18000.0f, fcA);
        fcB = juce::jlimit (20.0f, 18000.0f, fcB);

        // MASS inertia smoothing of cutoff
        smoothedFcA = a * smoothedFcA + (1.0f - a) * fcA;
        smoothedFcB = a * smoothedFcB + (1.0f - a) * fcB;

        filterA_L.setCutoffFrequency (smoothedFcA);
        filterA_R.setCutoffFrequency (smoothedFcA);
        filterB_L.setCutoffFrequency (smoothedFcB);
        filterB_R.setCutoffFrequency (smoothedFcB);

        // ----- Oscillator stack -----
        const float freqA = baseFreq * (1.0f + instability * driftA);
        const float freqB = baseFreq * 1.01f * (1.0f - instability * driftB);
        const float freqC = baseFreq * 0.5f;

        phaseA += freqA / (float) sr;
        phaseB += freqB / (float) sr;
        phaseC += freqC / (float) sr;

        phaseA -= (int) phaseA;
        phaseB -= (int) phaseB;
        phaseC -= (int) phaseC;

        const float sineA = std::sin (juce::MathConstants<float>::twoPi * phaseA);
        const float sineB = std::sin (juce::MathConstants<float>::twoPi * phaseB);
        
        // Soft wavefold
        float foldAmount = 1.0f + load * 4.0f;  
        float folded = std::tanh (sineA * foldAmount);

        // Secondary fold
        folded = std::tanh (folded * (1.5f + body * 2.0f));

        float osc = (sineA * 0.3f) + (sineB * 0.2f) + (folded * 0.5f);
        float grind = osc * std::abs(osc);
        osc = juce::jmap (bodyHigh, osc, grind);


        // Sub layer (MASS)
        phaseSub += (baseFreq * 0.5f) / (float) sr;
        phaseSub -= (int) phaseSub;

        const float sub = std::sin (juce::MathConstants<float>::twoPi * phaseSub);
        osc += sub * subGain;

        // LOAD drive + excitation
        float driven = std::tanh (osc * preGain);
        driven *= postTrim;
        
        // BODY high = stressed input (pre-filter)
        float stress = 1.0f + bodyHigh * 0.6f;
        float stressed = std::tanh (driven * stress);


        // ----- Filter network -----
        const float outA_L = filterA_L.processSample (0, stressed);
        const float outA_R = filterA_R.processSample (0, stressed);

        const float outB_L = filterB_L.processSample (0, stressed);
        const float outB_R = filterB_R.processSample (0, stressed);
        
        // ----- Cross modulation between filters -----

        float energyA = 0.5f * (std::abs (outA_L) + std::abs (outA_R));
        float energyB = 0.5f * (std::abs (outB_L) + std::abs (outB_R));

        // Smoothing
        crossModA += 0.001f * (energyA - crossModA);
        crossModB += 0.001f * (energyB - crossModB);

        // Apply very small cutoff nudges
        smoothedFcA *= (1.0f + crossAmount * crossModB);
        smoothedFcB *= (1.0f + crossAmount * crossModA);

        // Clamp safety
        smoothedFcA = juce::jlimit (20.0f, 18000.0f, smoothedFcA);
        smoothedFcB = juce::jlimit (20.0f, 18000.0f, smoothedFcB);
        
        // Stereo spectral rotation weight
        float stereoPhase = spectralPhase * juce::MathConstants<float>::twoPi;

        const float width = juce::jmap (rotationSmoothed, 0.05f, 1.0f); // small width at low ROTATION
        float weightL = 0.5f + 0.5f * width * std::sin (stereoPhase);
        float weightR = 0.5f + 0.5f * width * std::sin (stereoPhase + juce::MathConstants<float>::pi);
        
        weightL = juce::jlimit (0.0f, 1.0f, weightL);
        weightR = juce::jlimit (0.0f, 1.0f, weightR);

        // Crossfade between filter A and B per channel
        float outL = outA_L * weightL + outB_L * (1.0f - weightL);
        float outR = outA_R * weightR + outB_R * (1.0f - weightR);

        // WEAR post saturation
        float diodeDrive = juce::jmap (wear, 0.5f, 6.0f);
        float asym       = juce::jmap (bodyHigh, 1.0f, 2.2f);

        outL = diodeClip (outL, diodeDrive, asym);
        outR = diodeClip (outR, diodeDrive, asym);

        
        // Mid grit (cheap nonlinearity) - adds texture without pitch
        float grit = (outL * outL * outL) - outL; // odd harmonics
        outL += grit * body * 0.02f;
        grit = (outR * outR * outR) - outR;
        outR += grit * body * 0.02f;
        


        // MASS damping (1 pole lowpass)
        dampL += g * (outL - dampL);
        dampR += g * (outR - dampR);

        // Blend damp/raw
        left[i]  = outL * (1.0f - dampMix) + dampL * dampMix;
        right[i] = outR * (1.0f - dampMix) + dampR * dampMix;
    }
}

