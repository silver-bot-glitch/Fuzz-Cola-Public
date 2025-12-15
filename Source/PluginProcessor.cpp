/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin processor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FuzzColaAudioProcessor::FuzzColaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
: AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                  ),
apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    buildFactoryPresets();
    getPresetFolder().createDirectory();
}

FuzzColaAudioProcessor::~FuzzColaAudioProcessor()
{
}

// Parameter Layout
juce::AudioProcessorValueTreeState::ParameterLayout
FuzzColaAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "SUSTAIN", 1 }, "Sustain",
                                                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "TONE", 1 }, "Tone",
                                                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    
    auto volRange = juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f); // 0.1 dB steps
    
    layout.add (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "VOLUME", 1 }, "Volume", volRange, 0.0f)); // default 0 dB
    
    layout.add (std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "PEDALON", 1 }, "Pedal On", true));
    
    layout.add (std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "TONEBYPASS", 1 }, "Tone Enabled", true));
    
    return layout;
}

//==============================================================================
const juce::String FuzzColaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FuzzColaAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool FuzzColaAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool FuzzColaAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double FuzzColaAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FuzzColaAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int FuzzColaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FuzzColaAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FuzzColaAudioProcessor::getProgramName (int index)
{
    return {};
}

void FuzzColaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FuzzColaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    currentSampleRate = sampleRate;
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();
    
    for (std::size_t i = 0; i < chains.size(); ++i)
    {
        chains[i].prepare(spec);
        
        // Input booster / Sustain pre-gain
        auto& inputGain = chains[i].get<InputGainIndex>();
        inputGain.setRampDurationSeconds (0.001f);
        
        // Input high-pass
        auto& preFilter = chains[i].get<PreHighPassIndex>();
        preFilter.reset();
        preFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 30.0f);
        
        // Clipping stages
        auto& clip1 = chains[i].get<Clipper1Index>();
        auto& clip2 = chains[i].get<Clipper2Index>();
        
        // Both of the stages use tanh-based shaping functions
        
        // Stage 1: soft, pretty symmetric pre-shaping
        clip1.functionToUse = [] (float x)
        {
            const float vClip = 0.9f; // vclip controls output amplitude
            const float drive = 3.0f; // drive is amount of saturation basically
            
            const float y = drive * x / vClip; // scale input by drive and ampltiude
            return vClip * std::tanh (y);
        };
        
        // Stage 2: add even harmonics for warmth
        // I realized that before I was only doing odd harmonics so I have to offset to also get some even ones
        clip2.functionToUse = [] (float x)
        {
            const float vClip = 0.8f;
            const float drive = 5.0f;
            const float Offset  = 0.25f;   // controls asymmetry/offset
            
            // subtract tanh(drive * offset) to center around 0 since tanh is odd
            const float yOffset = drive * (x + Offset);
            const float center  = std::tanh (drive * Offset);
            
            const float shaped = std::tanh (yOffset) - center;
            
            return vClip * shaped;
        };
        
        // Global post low-pass to smooth the very top fizz
        // I added as i noticed that the real pedal doesnt have much high end above like 5.5 kHz
        auto& postLowPass = chains[i].get<PostLowPassIndex>();
        postLowPass.reset();
        postLowPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sampleRate, 5500.0f);
        
        // Output gain (Volume)
        auto& outputGain = chains[i].get<OutputGainIndex>();
        outputGain.setRampDurationSeconds(0.001f);
    }
    
    updateDSPFromParameters();
    
}

void FuzzColaAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FuzzColaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

// Updates Parameters in the DSP from the APVTS
void FuzzColaAudioProcessor::updateDSPFromParameters()
{
    const float sustain = *apvts.getRawParameterValue ("SUSTAIN");
    const float tone = *apvts.getRawParameterValue ("TONE");
    const float volumeDb = *apvts.getRawParameterValue ("VOLUME");
    const bool  toneBypass = (*apvts.getRawParameterValue ("TONEBYPASS") > 0.5f);
    
    // Gives the pedal some built-in dirt even at minimum
    const float sustainDb = juce::jmap (sustain, 0.0f, 1.0f,
                                        15.0f, 45.0f);
    
    // ye old processor chain
    for (std::size_t i = 0; i < chains.size(); ++i)
    {
        juce::dsp::ProcessorChain<
        
        juce::dsp::Gain<float>,
        juce::dsp::IIR::Filter<float>,
        juce::dsp::WaveShaper<float>,
        juce::dsp::WaveShaper<float>,
        ToneStack,
        juce::dsp::IIR::Filter<float>,
        juce::dsp::Gain<float>
        
        >& chain = chains[i];
        
        juce::dsp::Gain<float>& inputGain  = chain.get<InputGainIndex>();
        ToneStack&toneStage = chain.get<ToneStackIndex>();
        juce::dsp::Gain<float>& outputGain = chain.get<OutputGainIndex>();
        
        inputGain.setGainDecibels(sustainDb);
        outputGain.setGainDecibels(volumeDb);
        
        const float effectiveTone = toneBypass ? tone : 0.5f;  // neutral-ish when bypassed
        toneStage.setTone(effectiveTone);
        
    }
}

// Process Block
void FuzzColaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    
    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    
    for (int channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
    
    // Footswitch -> hard bypass of whole pedal
    const bool pedalOn = (*apvts.getRawParameterValue ("PEDALON") > 0.5f);
    
    if (! pedalOn)
        return; // passthrough (input already in buffer)
    
    updateDSPFromParameters();
    
    juce::dsp::AudioBlock<float> block (buffer);
    
    // if mono, process single channel
    if (buffer.getNumChannels() == 1)
    {
        juce::dsp::ProcessContextReplacing<float> monoContext (block);
        chains[0].process(monoContext);
    }
    // otherwise process stereo
    else
    {
        juce::dsp::AudioBlock<float> leftBlock = block.getSingleChannelBlock (0);
        juce::dsp::AudioBlock<float> rightBlock = block.getSingleChannelBlock (1);
        
        juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
        juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
        
        chains[0].process(leftContext);
        chains[1].process(rightContext);
    }
}

//==============================================================================
bool FuzzColaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FuzzColaAudioProcessor::createEditor()
{
    return new FuzzColaAudioProcessorEditor (*this);
}

//==============================================================================
// store/load parameters
void FuzzColaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ValueTree state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary(*xml, destData);
}

// load parameters
void FuzzColaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        juce::ValueTree state = juce::ValueTree::fromXml (*xmlState);
        apvts.replaceState (state);
    }
}

// load presets from folder
juce::File FuzzColaAudioProcessor::getPresetFolder() const
{
    return juce::File::getSpecialLocation
    (juce::File::userApplicationDataDirectory).getChildFile("SilverDSP").getChildFile("FuzzCola").getChildFile("Presets");
}

// save preset to file
void FuzzColaAudioProcessor::savePresetToFile (juce::File file)
{
    if (file == juce::File{}) return;
    
    if (file.getFileExtension().isEmpty())
        file = file.withFileExtension (".xml");
    
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    
    if (xml != nullptr)
        xml->writeTo (file);
}

// load preset from file
void FuzzColaAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    if (! file.existsAsFile()) return;
    
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (file));
    if (xml == nullptr) return;
    
    if (xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml (*xml));
}

// sets parameter value
void FuzzColaAudioProcessor::setParamValue (const juce::String& paramID, float actualValue)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (paramID)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(actualValue));
        p->endChangeGesture();
    }
}

// sets boolean parameter for toggles
void FuzzColaAudioProcessor::setParamBool (const juce::String& paramID, bool b)
{
    setParamValue(paramID, b ? 1.0f : 0.0f);
}

// factory presets
void FuzzColaAudioProcessor::buildFactoryPresets()
{
    factoryPresets.clear();
    
    factoryPresets.add({ "Wall Of Sound", 0.90f, 0.42f,  0.0f, true, true });
    factoryPresets.add({ "Scooped Rhythm", 0.72f, 0.30f, -3.0f, true, true });
    factoryPresets.add({ "Tight Lead", 0.60f, 0.65f, +2.0f, true, true });
    factoryPresets.add({ "Tone Bypass Hit", 0.85f, 0.50f,  0.0f, false,  true  });
}

// actually apply factory preset by index
void FuzzColaAudioProcessor::applyFactoryPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, factoryPresets.size()))
        return;
    
    const auto& p = factoryPresets.getReference (index);
    
    setParamValue("SUSTAIN", p.sustain);
    setParamValue("TONE", p.tone);
    setParamValue("VOLUME", p.volumeDb);
    setParamBool("TONEBYPASS", p.toneEnabled);
    setParamBool("PEDALON", p.pedalOn);
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FuzzColaAudioProcessor();
}
