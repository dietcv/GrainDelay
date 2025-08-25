#include "GrainDelay.hpp"
#include "SC_PlugIn.hpp"

static InterfaceTable* ft;

// ===== GRAIN DELAY =====

GrainDelay::GrainDelay() : 
    m_sampleRate(static_cast<float>(sampleRate())),
    m_sampleDur(static_cast<float>(sampleDur())),
    m_bufFrames(MAX_DELAY_TIME * m_sampleRate),
    m_bufSize(static_cast<int>(m_bufFrames)),
    m_eventSystem(NUM_CHANNELS)
{
    // Initialize audio buffer & graindata
    m_buffer.resize(m_bufSize, 0.0f);
    m_grainData.resize(NUM_CHANNELS);
    
    mCalcFunc = make_calc_function<GrainDelay, &GrainDelay::next_aa>();
    next_aa(1);
}

GrainDelay::~GrainDelay() = default;

void GrainDelay::next_aa(int nSamples) {
    // Get audio I/O
    const float* input = in(Input);
    float* output = out(Output);
    
    // Audio-rate parameters
    const float* triggerRateIn = in(TriggerRate);
    const float* overlapIn = in(Overlap);
    const float* delayTimeIn = in(DelayTime);
    const float* grainRateIn = in(GrainRate);
    
    // Control-rate parameters
    const float mix = sc_clip(in0(Mix), 0.0f, 1.0f);
    const float feedback = sc_clip(in0(Feedback), 0.0f, 0.99f);
    const float damping = sc_clip(in0(Damping), 0.0f, 1.0f);
    const bool freeze = in0(Freeze) > 0.5f;
    const bool reset = in0(Reset) > 0.5f;
    
    for (int i = 0; i < nSamples; ++i) {
        
        // Sample audio-rate parameters per-sample
        float triggerRate = triggerRateIn[i];
        float overlap = sc_clip(overlapIn[i], 0.001f, static_cast<float>(NUM_CHANNELS));
        float delayTime = sc_clip(delayTimeIn[i], m_sampleDur, MAX_DELAY_TIME);
        float grainRate = sc_clip(grainRateIn[i], 0.125f, 4.0f);
        
        // 1. Get trigger info from subsample-accurate system
        auto channelPhases = m_eventSystem.process(
            triggerRate, 
            reset, 
            overlap, 
            m_sampleRate
        );
        
        // 2. Process all grains
        float delayed = 0.0f;
   
        for (int g = 0; g < NUM_CHANNELS; ++g) {

            // Trigger new grain if needed
            if (m_eventSystem.justTriggered[g]) {

                // Calculate read position
                float normalizedWritePos = static_cast<float>(m_writePos) / m_bufFrames;
                float normalizedDelay = std::max(m_sampleDur, delayTime * m_sampleRate / m_bufFrames);
                float readPos = sc_wrap(normalizedWritePos - normalizedDelay, 0.0f, 1.0f);
                
                // store grain data
                m_grainData[g].readPos = readPos;
                m_grainData[g].rate = grainRate;
                m_grainData[g].hasTriggered = true;
                m_grainData[g].phase = grainRate * static_cast<float>(m_eventSystem.channelOffsets[g]);
            }
            
            // Process grain if the event system says it's active
            if (m_eventSystem.isActive[g]) {

                // Advance phase
                m_grainData[g].phase += m_grainData[g].rate;
                
                // Calculate grain phase: readPos + integrator phase
                float grainPhase = (m_grainData[g].readPos * m_bufFrames) + m_grainData[g].phase;
                
                // Get sample with interpolation
                float grainSample = Utils::peekCubicInterp(
                    m_buffer.data(), 
                    m_bufSize, 
                    grainPhase
                );
                
                // Apply Hanning window using subsample-accurate window phase
                grainSample *= Utils::hanningWindow(channelPhases[g]);
                delayed += grainSample;
            }
        }

        // 3. Apply amplitude compensation based on overlap
        float effectiveOverlap = std::max(1.0f, overlap);
        float compensationGain = 1.0f / std::sqrt(effectiveOverlap);
        delayed *= compensationGain;
        
        // 4. Apply feedback with damping filter
        float dampedFeedback = m_dampingFilter.processLowpass(delayed, damping);
        
        // 5. DC block input and write to delay buffer (only when not frozen)
        float dcBlockedInput = m_dcBlocker.processHighpass(input[i], 3.0f, m_sampleRate);
        
        if (!freeze) {
            m_buffer[m_writePos] = dcBlockedInput + dampedFeedback * feedback;
            m_writePos++;
            m_writePos = sc_wrap(m_writePos, 0, m_bufSize - 1);
        }
        
        // 6. Output with wet/dry mix
        output[i] = Utils::lerp(input[i], delayed, mix);
    }
}

void GrainDelay::reset() {
    m_eventSystem.reset();
    m_writePos = 0;
    m_dampingFilter.reset();
    m_dcBlocker.reset();
    
    // Reset grain data
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        m_grainData[i] = GrainData{};  // Reset to default values
    }
}

PluginLoad(GrainDelayUGens) {
    ft = inTable;
    registerUnit<GrainDelay>(ft, "GrainDelay", false);
}