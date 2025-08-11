#pragma once
#include "SC_PlugIn.hpp"
#include <array>        
#include <cmath>       
#include <algorithm> 

namespace Utils {

// ===== BASIC MATH UTILITIES =====

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Math constants
inline constexpr float TWO_PI = 6.28318530717958647692f;

// Granular delay constants
inline constexpr int GRANULAR_NUM_CHANNELS = 5;
inline constexpr float GRANULAR_MAX_DELAY_TIME = 5.0f;

// Granular utility functions
float hanningWindow(float phase) {
    return (1.0f - std::cos(phase * TWO_PI)) * 0.5f;
}

// ===== BUFFER ACCESS UTILITIES =====

inline float peekNoInterp(const float* buffer, int bufSize, int index) {
    const int wrappedIndex = sc_wrap(index, 0, bufSize - 1);
    return buffer[wrappedIndex];
}

inline float peekLinearInterp(const float* buffer, int bufSize, float phase) {
    
    const float sampleIndex = phase;
    const int intPart = static_cast<int>(sampleIndex);
    const float fracPart = sampleIndex - intPart;
    
    const int idx1 = sc_wrap(intPart, 0, bufSize - 1);
    const int idx2 = sc_wrap(intPart + 1, 0, bufSize - 1);
    
    const float a = buffer[idx1];
    const float b = buffer[idx2];
    
    return lerp(a, b, fracPart);
}

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
   
    void reset() {
        m_state = 0.0f;
    }
   
    float processLowpass(float input, float coeff) {
        coeff = sc_clip(coeff, 0.0f, 1.0f);
        m_state = input * (1.0f - coeff) + m_state * coeff;
        return m_state;
    }
};

struct OnePoleFilter {
    
    float m_state{0.0f};
   
    void reset() {
        m_state = 0.0f;
    }
   
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
};

// ===== PHASE PROCESSING UTILITIES =====

struct RampToTrig {
    float lastPhase = 0.0f;
    bool lastWrap = false;
    
    bool process(float currentPhase) {
        // Detect wrap using current vs last phase
        float delta = currentPhase - lastPhase;
        float sum = currentPhase + lastPhase;
        bool currentWrap = (sum != 0.0f) && (std::abs(delta / sum) > 0.5f);
        
        // Edge detection - only trigger on rising edge of wrap
        bool trigger = currentWrap && !lastWrap;
        
        // Update state for next sample
        lastPhase = currentPhase;
        lastWrap = currentWrap;
        
        return trigger;
    }
    
    void reset() {
        lastPhase = 0.0f;
        lastWrap = false;
    }
};

// ===== EVENT UTILITIES =====

struct SubsampleEventSystem {
    // Ramp state
    float phase = 0.0f;        // Current ramp position [0,1)
    float prevPhase = 0.0f;    // Previous sample's ramp position (for trigger detection)
    float slope = 0.0f;        // Current slope (rate/sampleRate)
    bool wrapNext = false;     // Flag: will wrap on next sample
   
    // Trigger detection
    RampToTrig trigDetect;
   
    // Channel state
    std::array<float, GRANULAR_NUM_CHANNELS> channelPhases{};
    std::array<float, GRANULAR_NUM_CHANNELS> channelSlopes{};
    std::array<float, GRANULAR_NUM_CHANNELS> channelOffsets{};
    std::array<bool, GRANULAR_NUM_CHANNELS> isActive{};
    std::array<bool, GRANULAR_NUM_CHANNELS> justTriggered{};
   
    int pulseCount = 0;
   
    std::array<float, GRANULAR_NUM_CHANNELS> process(float rate, bool resetTrigger, float overlap, float sampleRate) {
        if (resetTrigger) {
            reset();
        }
       
        std::array<float, GRANULAR_NUM_CHANNELS> output{};
        justTriggered.fill(false);
       
        // Initialize on first sample
        if (slope == 0.0f) {
            slope = rate / sampleRate;
        }
       
        // 1. Handle wrap from previous sample
        if (wrapNext) {
            phase -= 1.0f;                      // Wrap the phase
            slope = rate / sampleRate;          // Latch new slope for next period
            wrapNext = false;
        }
       
        // 2. Detect trigger using previous sample's phase
        bool trigger = trigDetect.process(prevPhase);
       
        // 3. Handle trigger
        if (trigger) {
            int ch = pulseCount % GRANULAR_NUM_CHANNELS;
            justTriggered[ch] = true;
           
            if (overlap > 0.0f) {
                // Calculate and store slopes, phases and subsample offsets for this channel
                channelSlopes[ch] = slope / overlap;
                channelOffsets[ch] = prevPhase / slope;
                channelPhases[ch] = channelSlopes[ch] * channelOffsets[ch];
                isActive[ch] = true;
            }
           
            pulseCount++;
        }
       
        // 4. Process channels
        for (int ch = 0; ch < GRANULAR_NUM_CHANNELS; ++ch) {
            if (!isActive[ch]) {
                output[ch] = 0.0f;
                continue;
            }
           
            // Don't increment on trigger sample
            if (!justTriggered[ch]) {
                channelPhases[ch] += channelSlopes[ch];
            }
           
            if (channelPhases[ch] >= 1.0f) {
                isActive[ch] = false;
                output[ch] = 0.0f;
            } else {
                output[ch] = channelPhases[ch];
            }
        }
       
        // 5. Update for next sample
        prevPhase = phase;          // Store current as previous
        phase += slope;             // Increment current
       
        // 6. Check for wrap
        if (phase >= 1.0f) {
            wrapNext = true;
        }
       
        return output;
    }
   
    void reset() {
        phase = prevPhase = 0.0f;
        slope = 0.0f;
        wrapNext = false;
        trigDetect.reset();
        pulseCount = 0;
        channelPhases.fill(0.0f);
        channelSlopes.fill(0.0f);
        channelOffsets.fill(0.0f);
        isActive.fill(false);
        justTriggered.fill(false);
    }
};

} // namespace Utils