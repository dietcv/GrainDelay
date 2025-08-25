#pragma once
#include "SC_PlugIn.hpp"
#include <array>
#include <vector>        
#include <cmath>       
#include <algorithm> 

namespace Utils {

// ===== BASIC MATH UTILITIES =====

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Math constants
inline constexpr float TWO_PI = 6.28318530717958647692f;

// Granular utility functions
float hanningWindow(float phase) {
    return (1.0f - std::cos(phase * TWO_PI)) * 0.5f;
}

// ===== BUFFER ACCESS UTILITIES =====

inline float peekCubicInterp(const float* buffer, int bufSize, float phase) {

    const float sampleIndex = phase;
    const int intPart = static_cast<int>(sampleIndex);
    const float fracPart = sampleIndex - intPart;
    
    const int idx0 = sc_wrap(intPart - 1, 0, bufSize - 1);
    const int idx1 = sc_wrap(intPart, 0, bufSize - 1);
    const int idx2 = sc_wrap(intPart + 1, 0, bufSize - 1);
    const int idx3 = sc_wrap(intPart + 2, 0, bufSize - 1);
    
    const float a = buffer[idx0];
    const float b = buffer[idx1];
    const float c = buffer[idx2];
    const float d = buffer[idx3];
    
    return cubicinterp(fracPart, a, b, c, d);
}

// ===== ONE POLE FILTER UTILITIES =====

struct OnePoleNormalized {
    float m_state = 0.0f;
    
    float processLowpass(float input, float coeff) {
        coeff = sc_clip(coeff, 0.0f, 1.0f);
        m_state = input * (1.0f - coeff) + m_state * coeff;
        return m_state;
    }

    void reset() {
        m_state = 0.0f;
    }
};

struct OnePoleFilter {
    
    float m_state{0.0f};
   
    float processLowpass(float input, float cutoffHz, float sampleRate) {

        // Clip slope to full Nyquist range, then take absolute value
        float slope = cutoffHz / sampleRate;
        float safeSlope = std::abs(sc_clip(slope, -0.5f, 0.5f));
        
        // Calculate coefficient: b = exp(-2π * slope)
        float coeff = std::exp(-TWO_PI * safeSlope);
        
        // OnePole formula: y[n] = x[n] * (1-b) + y[n-1] * b
        m_state = input * (1.0f - coeff) + m_state * coeff;
        
        return m_state;
    }
   
    float processHighpass(float input, float cutoffHz, float sampleRate) {
        float lowpassed = processLowpass(input, cutoffHz, sampleRate);
        return input - lowpassed;
    }

    void reset() {
        m_state = 0.0f;
    }
};

// ===== TRIGGER AND TIMING UTILITIES =====

struct RampToTrig {
    double m_lastPhase{0.0};
    bool m_lastWrap{false};
    
    bool process(double currentPhase) {
        // Detect wrap using current vs last phase
        double delta = currentPhase - m_lastPhase;
        double sum = currentPhase + m_lastPhase;
        bool currentWrap = (sum != 0.0) && (std::abs(delta / sum) > 0.5);
        
        // Edge detection - only trigger on rising edge of wrap
        bool trigger = currentWrap && !m_lastWrap;
        
        // Update state for next sample
        m_lastPhase = currentPhase;
        m_lastWrap = currentWrap;
        
        return trigger;
    }
    
    void reset() {
        m_lastPhase = 0.0;
        m_lastWrap = false;
    }
};

struct EventSystem {
    // Core timing components
    RampToTrig trigDetect;
   
    // Ramp state
    double phase{0.0};        // Current ramp position [0,1)
    double slope{0.0};        // Current slope (rate/sampleRate)
    bool wrapNext{false};     // Flag: will wrap on next sample
   
    // Dynamic channel state
    std::vector<double> channelPhases;     
    std::vector<double> channelSlopes;
    std::vector<double> channelOffsets;
    std::vector<bool> isActive;
    std::vector<bool> justTriggered;
    int numChannels;

    explicit EventSystem(int channels = 5) : numChannels(channels) {
        channelPhases.resize(numChannels, 0.0);
        channelSlopes.resize(numChannels, 0.0);
        channelOffsets.resize(numChannels, 0.0);
        isActive.resize(numChannels, false);
        justTriggered.resize(numChannels, false);
    }
   
    std::vector<float> process(float rate, bool resetTrigger, float overlap, float sampleRate) {
        std::vector<float> output(numChannels, 0.0f);
        
        // Handle reset
        if (resetTrigger) {
            reset();
            return output; // Early exit on reset
        }

        // Clear triggers for this cycle
        std::fill(justTriggered.begin(), justTriggered.end(), false);
       
        // Initialize on first sample
        if (slope == 0.0) {
            slope = rate / sampleRate;
        }
       
        // 1. Handle wrap from previous sample
        if (wrapNext) {
            phase -= 1.0;                       // Wrap the phase
            slope = rate / sampleRate;          // Latch new slope for next period
            wrapNext = false;
        }
       
        // 2. Detect trigger
        bool trigger = trigDetect.process(phase);
       
        // 3. Handle trigger - find first available channel
        if (trigger && slope != 0.0) {
            for (int ch = 0; ch < numChannels; ++ch) {
                if (!isActive[ch]) {
                    // Found available channel - trigger grain
                    justTriggered[ch] = true;
                    channelSlopes[ch] = slope / overlap;
                    channelOffsets[ch] = phase / slope;
                    channelPhases[ch] = channelSlopes[ch] * channelOffsets[ch];
                    isActive[ch] = true;
                    break;
                }
            }
        }
       
        // 4. Process channels
        for (int ch = 0; ch < numChannels; ++ch) {
            if (!isActive[ch]) {
                output[ch] = 0.0f;
                continue;
            }
           
            // Don't increment on trigger sample
            if (!justTriggered[ch]) {
                channelPhases[ch] += channelSlopes[ch];
            }
           
            if (channelPhases[ch] >= 1.0) {
                isActive[ch] = false;
                output[ch] = 0.0f;
            } else {
                output[ch] = static_cast<float>(channelPhases[ch]);
            }
        }
       
        // 5. Update for next sample
        phase += slope;
       
        // 6. Check for wrap
        if (phase >= 1.0) {
            wrapNext = true;
        }
       
        return output;
    }
   
    void reset() {
        phase = 0.0;
        slope = 0.0;
        wrapNext = false;
        trigDetect.reset();
        std::fill(channelPhases.begin(), channelPhases.end(), 0.0);
        std::fill(channelSlopes.begin(), channelSlopes.end(), 0.0);
        std::fill(channelOffsets.begin(), channelOffsets.end(), 0.0);
        std::fill(isActive.begin(), isActive.end(), false);
        std::fill(justTriggered.begin(), justTriggered.end(), false);
    }
};

} // namespace Utils