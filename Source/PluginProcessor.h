/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin processor.
 
 ==============================================================================
 */

#pragma once
#include <JuceHeader.h>

//==============================================================================
/**
 */
class FuzzColaAudioProcessor  : public juce::AudioProcessor
{
    public:
    //==============================================================================
    FuzzColaAudioProcessor();
    ~FuzzColaAudioProcessor() override;
    
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    
#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif
    
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    
    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    
    //==============================================================================
    const juce::String getName() const override;
    
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    
    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    
    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // StateTree (APVTS) access
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    struct FactoryPreset
    {
        juce::String name;
        float sustain = 0.5f;
        float tone = 0.5f;
        float volumeDb = 0.8f;
        bool toneEnabled = true;
        bool pedalOn = true;
    };
    
    const juce::Array<FactoryPreset>& getFactoryPresets() const { return factoryPresets; }
    
    void applyFactoryPreset (int index);
    
    juce::File getPresetFolder() const;
    void savePresetToFile(juce::File file);
    void loadPresetFromFile(const juce::File& file);
    
    private:
    
    // Factory presets
    juce::Array<FactoryPreset> factoryPresets;
    void buildFactoryPresets();
    void setParamValue(const juce::String& paramID, float actualValue);
    void setParamBool(const juce::String& paramID, bool b);
    
    
    // DSP tone stack approximation
    struct ToneStack
    {
        ToneStack() = default;
        
        // Prepare the filters
        void prepare(const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            
            lowShelf.reset();
            highShelf.reset();
            
            updateFilters();
        }
        
        // Reset filters so no clicks
        void reset()
        {
            lowShelf.reset();
            highShelf.reset();
        }
        
        // tone = 0 .. 1  (0 = dark, 1 = bright)
        // Default = 0.5
        void setTone(float newTone)
        {
            tone = juce::jlimit (0.0f, 1.0f, newTone);
            updateFilters();
        }
        
        // Process audio block
        template <typename ProcessContext>
        void process(const ProcessContext& context)
        {
            // shelves in series
            lowShelf.process(context);
            highShelf.process(context);
        }
        
        private:
        // Update filter coefficients based on tone setting
        void updateFilters()
        {
            // Filter parameters
            // These corner frequencies were chosen to roughly approximate (both via electrosmash but also by ear)
            const float lpCutHz = 450.0f;   // bass / low-mid shelf corner
            const float hpCutHz = 1500.0f;  // upper-mid / treble shelf corner
            const float q = 0.7071f;
            
            // tone = 0 -> +3.5 dB bass, -5 dB treble (dark & fat)
            // tone = 0.5 -> +0.5 dB bass, +1.5 dB treble (slightly warm)
            // tone = 1 -> -2.5 dB bass, +8 dB treble (bright)
            const float bassGainDb = juce::jmap(tone,  3.5f, -2.5f);
            const float trebleGainDb = juce::jmap(tone, -5.0f,  8.0f);
            
            // Convert dB gains to linear
            const float bassGainLinear = juce::Decibels::decibelsToGain(bassGainDb);
            const float trebleGainLinear = juce::Decibels::decibelsToGain(trebleGainDb);
            
            // Update filter coefficients
            lowShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, lpCutHz, q, bassGainLinear);
            highShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, hpCutHz, q, trebleGainLinear);
        }
        
        // Defaults
        double sampleRate = 44100.0;
        float tone = 0.5f;
        
        juce::dsp::IIR::Filter<float> lowShelf;
        juce::dsp::IIR::Filter<float> highShelf;
    };
    
    
    // its always much easier to keep track of chain if enum
    enum ChainPositions
    {
        InputGainIndex = 0,  // pre-gain / Sustain
        PreHighPassIndex = 1,  // input HPF
        Clipper1Index = 2,  // clipping stage 1
        Clipper2Index = 3,  // clipping stage 2
        ToneStackIndex = 4,  // HP/LP blend tone stack
        PostLowPassIndex = 5,  // final fizz killer, when listening to original pedal i realized it needs a way to get rid of extra fizz
        OutputGainIndex = 6   // Volume
    };
    
    // 2 mono chains (L/R)
    std::array<
    juce::dsp::ProcessorChain<
    juce::dsp::Gain<float>,          // InputGainIndex
    juce::dsp::IIR::Filter<float>,   // PreHighPassIndex
    juce::dsp::WaveShaper<float>,    // Clipper1Index
    juce::dsp::WaveShaper<float>,    // Clipper2Index
    ToneStack,                       // ToneStackIndex
    juce::dsp::IIR::Filter<float>,   // PostLowPassIndex
    juce::dsp::Gain<float>           // OutputGainIndex
    >,
    2 // stereo channels
    > chains;
    
    
    double currentSampleRate = 44100.0;
    
    // StateTree
    juce::AudioProcessorValueTreeState apvts;
    
    void updateDSPFromParameters();
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzColaAudioProcessor)
};
