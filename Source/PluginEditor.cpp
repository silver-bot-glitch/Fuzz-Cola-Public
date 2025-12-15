/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================


FuzzColaAudioProcessorEditor::FuzzColaAudioProcessorEditor (FuzzColaAudioProcessor& p)
: AudioProcessorEditor (&p), audioProcessor (p), bypassToggle ("Bypass"), footswitch ("Footswitch")
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 600);
    
    // Backgrounds
    hiBackground = juce::ImageCache::getFromMemory (BinaryData::HiResBackground0001_png, BinaryData::HiResBackground0001_pngSize);
    
    loBackground = juce::ImageCache::getFromMemory (BinaryData::LoResBackground0001_png, BinaryData::LoResBackground0001_pngSize);
    
    // Knob strips
    hiSustainStrip = juce::ImageCache::getFromMemory (BinaryData::HiResSustainKnob_filmstrip_png, BinaryData::HiResSustainKnob_filmstrip_pngSize);
    
    loSustainStrip = juce::ImageCache::getFromMemory (BinaryData::LoResSustainKnob_filmstrip_png, BinaryData::LoResSustainKnob_filmstrip_pngSize);
    
    hiToneStrip = juce::ImageCache::getFromMemory (BinaryData::HiResToneKnob_filmstrip_png, BinaryData::HiResToneKnob_filmstrip_pngSize);
    
    loToneStrip = juce::ImageCache::getFromMemory (BinaryData::LoResToneKnob_filmstrip_png, BinaryData::LoResToneKnob_filmstrip_pngSize);
    
    hiVolumeStrip = juce::ImageCache::getFromMemory (BinaryData::HiResVolumeKnob_filmstrip_png, BinaryData::HiResVolumeKnob_filmstrip_pngSize);
    
    loVolumeStrip = juce::ImageCache::getFromMemory (BinaryData::LoResVolumeKnob_filmstrip_png, BinaryData::LoResVolumeKnob_filmstrip_pngSize);
    
    // LED frames
    hiLedOff = juce::ImageCache::getFromMemory (BinaryData::HiResLED0001_png, BinaryData::HiResLED0001_pngSize);
    
    hiLedOn = juce::ImageCache::getFromMemory (BinaryData::HiResLED0038_png, BinaryData::HiResLED0038_pngSize);
    
    loLedOff = juce::ImageCache::getFromMemory (BinaryData::LoResLED0001_png, BinaryData::LoResLED0001_pngSize);
    
    loLedOn = juce::ImageCache::getFromMemory (BinaryData::LoResLED0038_png, BinaryData::LoResLED0038_pngSize);
    
    // Footswitch
    hiFootOff = juce::ImageCache::getFromMemory (BinaryData::HiResOnOff0001_png, BinaryData::HiResOnOff0001_pngSize);
    
    hiFootOn = juce::ImageCache::getFromMemory (BinaryData::HiResOnOff0002_png, BinaryData::HiResOnOff0002_pngSize);
    
    loFootOff = juce::ImageCache::getFromMemory (BinaryData::LoResOnOff0001_png, BinaryData::LoResOnOff0001_pngSize);
    
    loFootOn = juce::ImageCache::getFromMemory (BinaryData::LoResOnOff0002_png, BinaryData::LoResOnOff0002_pngSize);
    
    // Bypass switch
    hiBypassOff = juce::ImageCache::getFromMemory (BinaryData::HiResBypassSwitch0001_png, BinaryData::HiResBypassSwitch0001_pngSize);
    
    hiBypassOn = juce::ImageCache::getFromMemory (BinaryData::HiResBypassSwitch0002_png, BinaryData::HiResBypassSwitch0002_pngSize);
    
    loBypassOff = juce::ImageCache::getFromMemory (BinaryData::LoResBypassSwitch0001_png, BinaryData::LoResBypassSwitch0001_pngSize);
    
    loBypassOn = juce::ImageCache::getFromMemory (BinaryData::LoResBypassSwitch0002_png, BinaryData::LoResBypassSwitch0002_pngSize);
    
    // Knobs setup, its a lambda that we call for each knob
    auto setupKnob = [this](PopupNumericSlider& s, FilmstripKnobLookAndFeel& lnf)
    {
        s.setLookAndFeel(&lnf);
        s.setSliderSnapsToMousePosition(false); // so it doesnt snap on double click
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag); // Because plugin is small, rotary
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // No text box, taken care of by popup
        
        // Rotary angles
        const float startAngle = juce::MathConstants<float>::pi * 1.25f;
        const float endAngle = juce::MathConstants<float>::pi * 2.75f;
        s.setRotaryParameters(startAngle, endAngle, true);
        
        // built-in popup while changing
        s.attachPopupTo(this);
    };
    
    // Each knob with its Look and feel
    setupKnob(sustainKnob, sustainLNF);
    setupKnob(toneKnob, toneLNF);
    setupKnob(volumeKnob, volumeLNF);
    volumeKnob.setTextValueSuffix(" dB");
    
    addAndMakeVisible(volumeKnob);
    addAndMakeVisible(toneKnob);
    addAndMakeVisible(sustainKnob);
    
    // LED
    addAndMakeVisible(led);
    
    // Footswitch sets the use the size of your Hi/LoResOnOff images
    footswitch.setClickingTogglesState(true);
    footswitch.addListener(this);
    addAndMakeVisible(footswitch);
    
    // Small bypass toggle size of Hi/LoResBypassSwitch images
    bypassToggle.setClickingTogglesState(true);
    
    bypassToggle.addListener(this);
    addAndMakeVisible(bypassToggle);
    
    juce::AudioProcessorValueTreeState& state = audioProcessor.getAPVTS();
    
    // Since we have attachments we dont reallyy need listen to sliders/buttons
    sliderValueChanged (nullptr);
    sustainAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(state, "SUSTAIN", sustainKnob));
    toneAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(state, "TONE",    toneKnob));
    volumeAttachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(state, "VOLUME",  volumeKnob));
    volumeKnob.setSkewFactorFromMidPoint (0.0); // set skew for volume knob (midpoint at 0 dB)
    
    pedalOnAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(state, "PEDALON",    footswitch));
    toneBypassAttachment.reset(new juce::AudioProcessorValueTreeState::ButtonAttachment(state, "TONEBYPASS", bypassToggle));
    
    addAndMakeVisible(presetBox);
    
    // lamba for preset selection
    presetBox.onChange = [this]() {handlePresetSelection();};
    
    //Change color + opacity of preset box
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.5f));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::white);
    
    // my attempt at refreshing preset box
    refreshPresetBox();
    
    // Sync local flags to parameter values, then update LED & graphics
    pedalEngaged = (*state.getRawParameterValue("PEDALON") > 0.5f);
    toneEnabled = (*state.getRawParameterValue("TONEBYPASS") > 0.5f);
    useHiRes = toneEnabled;  // hi-res when tone is in-circuit
    
    led.setOn(pedalEngaged);
    updateGraphicsForResolution();
    resized();
}

FuzzColaAudioProcessorEditor::~FuzzColaAudioProcessorEditor()
{
    // Avoid dangling pointers (I would sometimes get a jassert)
    sustainKnob.setLookAndFeel(nullptr);
    toneKnob.setLookAndFeel(nullptr);
    volumeKnob.setLookAndFeel(nullptr);
    
}

void FuzzColaAudioProcessorEditor::updateGraphicsForResolution()
{
    
    // Chooses hi or lo based on useHiRes
    if (useHiRes)
    {
        background = hiBackground;
        sustainLNF.filmstrip = hiSustainStrip;
        toneLNF.filmstrip = hiToneStrip;
        volumeLNF.filmstrip = hiVolumeStrip;
        
        led.setImages(hiLedOff, hiLedOn);
        footswitch.setImages(hiFootOff, hiFootOn);
        bypassToggle.setImages(hiBypassOff, hiBypassOn);
    }
    else
    {
        background = loBackground;
        sustainLNF.filmstrip = loSustainStrip;
        toneLNF.filmstrip = loToneStrip;
        volumeLNF.filmstrip = loVolumeStrip;
        
        led.setImages(loLedOff, loLedOn);
        footswitch.setImages(loFootOff, loFootOn);
        bypassToggle.setImages(loBypassOff, loBypassOn);
    }
    
    // Update button states without sending notifications
    footswitch.setToggleState(pedalEngaged, juce::dontSendNotification);
    bypassToggle.setToggleState(toneEnabled, juce::dontSendNotification);
    led.setOn(pedalEngaged);
    
    repaint();
    
}
//==============================================================================
void FuzzColaAudioProcessorEditor::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    //        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    //
    //        g.setColour (juce::Colours::white);
    //        g.setFont (juce::FontOptions (15.0f));
    //        g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
    
    g.fillAll (juce::Colours::black);
    
    if (background.isValid())
    {
        g.drawImage (background,
                     0, 0, getWidth(), getHeight(),
                     0, 0, background.getWidth(), background.getHeight());
    }
    
}

void FuzzColaAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    volumeKnob.setBounds (75, 76, 99, 112);
    toneKnob.setBounds (152, 163, 93, 104);
    sustainKnob.setBounds (218, 81, 94, 99);
    
    led.setBounds(240, 397, 37, 45);
    
    footswitch.setBounds(136, 373, 106, 128);
    bypassToggle.setBounds(160, 268, 87, 41);
    
    // Center the preset box near top
    const int boxW = 180;
    const int boxH = 24;
    const int boxX = (getWidth() - boxW) / 2;
    const int boxY = 3;
    
    presetBox.setBounds (boxX, boxY, boxW, boxH);
    
}

void FuzzColaAudioProcessorEditor::buttonClicked(juce::Button* b)
{
    // Footswitch: controls LED + local state, the parameter itself updated via APVTS attachment
    if (b == &footswitch)
    {
        pedalEngaged = footswitch.getToggleState();
        led.setOn (pedalEngaged);
        return;
    }
    
    // Bypass switch: tone bypass + hi/lo res
    if (b == &bypassToggle)
    {
        toneEnabled = bypassToggle.getToggleState();
        
        // hi-res when tone is active, lo-res when bypassed
        useHiRes =  toneEnabled;
        
        updateGraphicsForResolution();
        return;
    }
    
}

// not really needed bc of attachments
void FuzzColaAudioProcessorEditor::sliderValueChanged(juce::Slider* s){}

// Preset management
void FuzzColaAudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear();
    userPresetFiles.clear();
    
    const auto& factories = audioProcessor.getFactoryPresets();
    
    // Factory presets: IDs 1..N
    for (int i = 0; i < factories.size(); ++i)
        presetBox.addItem("Factory: " + factories[i].name, i + 1);
    
    presetBox.addSeparator();
    
    // User presets from folder: IDs 100.. (100 + count-1)
    auto folder = audioProcessor.getPresetFolder();
    folder.createDirectory();
    
    folder.findChildFiles(userPresetFiles, juce::File::findFiles, false, "*.xml");
    
    for (int i = 0; i < userPresetFiles.size(); ++i)
        presetBox.addItem("User: " + userPresetFiles[i].getFileNameWithoutExtension(), 100 + i);
    
    presetBox.addSeparator();
    
    presetBox.addItem("Save current as...", 1000);
    presetBox.addItem("Rescan presets",   1001);
    presetBox.addItem("Open preset folder", 1002);
    presetBox.setTextWhenNothingSelected("Presets"); // default text
    
}

// Handle preset selection changes
void FuzzColaAudioProcessorEditor::handlePresetSelection()
{
    // Get selected ID
    const int id = presetBox.getSelectedId();
    if (id == 0) return; // nothing selected
    
    // Factory preset count
    const auto factoryCount = audioProcessor.getFactoryPresets().size();
    
    // Factory
    if (id >= 1 && id <= factoryCount)
    {
        audioProcessor.applyFactoryPreset (id - 1);
        syncUiFromParams();
        return;
    }
    
    // User presets
    if (id >= 100 && id < 100 + userPresetFiles.size())
    {
        audioProcessor.loadPresetFromFile (userPresetFiles[id - 100]);
        syncUiFromParams();
        return;
    }
    
    // Actions
    if (id == 1000) // Save current as...
    {
        auto folder = audioProcessor.getPresetFolder();
        presetChooser = std::make_unique<juce::FileChooser>("Save preset...", folder.getChildFile ("MyPreset.xml"), "*.xml");
        
        // Async save, lamba function called when done to save and refresh
        presetChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                   [this](const juce::FileChooser& fc)
                                   {
            auto f = fc.getResult();
            if (f != juce::File{})
            {
                audioProcessor.savePresetToFile(f);
                refreshPresetBox();          // repopulates userPresetFiles
                selectUserPresetByFile(f);  // selects the new one so it doesn't go blank
            }
        });
        
        return;
    }
    
    if (id == 1001) // Rescan
    {
        refreshPresetBox();
        return;
    }
    
    if (id == 1002) // Open folder
    {
        audioProcessor.getPresetFolder().revealToUser();
        return;
    }
}

// Sync UI state from parameter values
void FuzzColaAudioProcessorEditor::syncUiFromParams()
{
    auto& st = audioProcessor.getAPVTS();
    
    const bool pedal = (*st.getRawParameterValue("PEDALON") > 0.5f);
    const bool bypass = (*st.getRawParameterValue("TONEBYPASS") > 0.5f);
    
    pedalEngaged = pedal;
    toneEnabled = bypass;
    useHiRes = toneEnabled;
    
    led.setOn(pedalEngaged);
    updateGraphicsForResolution();
}
