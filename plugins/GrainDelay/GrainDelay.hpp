#pragma once
#include "SC_PlugIn.hpp"
#include "Utils.hpp"
#include <vector>

// ===== GRAIN DELAY =====

class GrainDelay : public SCUnit {
public:
    GrainDelay();
    ~GrainDelay();

private:
    void next_aa(int nSamples);
    void reset();
    
    // Constants cached at construction
    const float m_sampleRate;
    const float m_sampleDur;
    const float m_bufFrames;
    const int m_bufSize;
   
    // Core trigger system
    Utils::EventSystem m_eventSystem;

    // Constants
    static constexpr int NUM_CHANNELS = 32;
    static constexpr float MAX_DELAY_TIME = 5.0f;
   
    // Audio buffer and processing
    std::vector<float> m_buffer;
    int m_writePos = 0;
   
    // Grain data structure
    struct GrainData {
        float readPos = 0.0f;
        float rate = 1.0f; 
        float phase = 0.0f;
        bool hasTriggered = false;
    };
   
    // grain voices
    std::vector<GrainData> m_grainData;
   
    // Feedback processing filters
    Utils::OnePoleNormalized m_dampingFilter;  // For feedback damping (0-1)
    Utils::OnePoleFilter m_dcBlocker;          // For DC blocking (3Hz)
   
    // Input parameters for audio processing
    enum InputParams {
        Input,          // Audio input
        TriggerRate,    // Grain trigger rate (Hz) - density control
        Overlap,        // Grain overlap amount  
        DelayTime,      // Delay time in seconds
        GrainRate,      // Grain playback rate (0.5-2.0, 1.0=normal)
        Mix,            // Wet/dry mix (0=dry, 1=wet)
        Feedback,       // Feedback amount (0-0.95)
        Damping,        // Feedback filter (0=dark, 1=bright)
        Freeze,         // Freeze buffer (0=record, 1=freeze)
        Reset,          // Reset trigger
        NumInputParams
    };
   
    enum Outputs {
        Output,        
        NumOutputParams
    };
};

// ===== EVENT SYSTEM =====
/*
class EventSystem : public SCUnit {
public:
    EventSystem();
    ~EventSystem();

private:
    void next_aa(int nSamples);
    void reset();
   
    // Core processing
    Utils::EventSystem m_eventSystem;

    // Constants
    const float m_sampleRate;
    int m_numChannels;
   
    // Input parameters
    enum InputParams {
        NumChannels,    // Number of output channels (init-rate)
        TriggerRate,    // Grain trigger rate (Hz)
        Overlap,        // Grain overlap amount  
        Reset,          // Reset trigger
       
        NumInputParams
    };
   
    // Outputs: numChannels ramp phases
    // Output indices calculated as: out(ch)
};
*/