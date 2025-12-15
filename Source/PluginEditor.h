/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 */

// Custom LookAndFeel for Filmstrip Knob
struct FilmstripKnobLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Image filmstrip;
    int frameCount = 100; // I rendered 100 frames
    
    // I had to do an override of drawRotarySlider from juce::LookAndFeel_V4 to draw my filmstrip knob
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override
    {
        // Draw the filmstrip knob based on the slider position
        if (! filmstrip.isValid() || frameCount <= 0) return; // If no valid image, do nothing
        
        // Calculate frame dimensions
        const int frameHeight = filmstrip.getHeight() / frameCount;
        // Technically no need to divide by frameCount since its always 100 but just in case I want to change something last minute (like add more frames or less frames)
        // super unlikely though because rendering is a pain haha
        const int frameWidth  = filmstrip.getWidth();
        
        // Determine the current frame index by mapping slider position to frame count
        const int frameIndex = juce::jlimit (0, frameCount - 1, (int) std::round (sliderPosProportional * (frameCount - 1)));
        
        // Source Y position in the filmstrip
        const int srcY = frameIndex * frameHeight;
        
        
        g.drawImage (filmstrip, x, y, width, height,0, srcY, frameWidth, frameHeight);
    }
};

// This handles the footswitch LED
struct LedComponent : public juce::Component
{
    juce::Image offImage, onImage;
    bool isOn = false;
    
    // Override paint to draw the LED based on isOn state
    void paint (juce::Graphics& g) override
    {
        auto& img = isOn ? onImage : offImage;
        
        if(img.isValid())
            g.drawImageWithin (img, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);
    }
    
    // Set the LED state and repaint if changed
    void setOn (bool shouldBeOn)
    {
        if (isOn != shouldBeOn)
        {
            isOn = shouldBeOn;
            repaint();
        }
    }
    
    void setImages (const juce::Image& offImg, const juce::Image& onImg)
    {
        offImage = offImg;
        onImage = onImg;
        repaint();
    }
};

// This handles the tone stack bypass switch
struct ToggleImageButton : public juce::Button
{
    juce::Image offImage, onImage;
    
    ToggleImageButton (const juce::String& name) : Button (name) {}
    
    void setImages (const juce::Image& offImg, const juce::Image& onImg)
    {
        // Set images and repaint
        offImage = offImg;
        onImage = onImg;
        repaint();
    }
    
    // Override paintButton to draw based on toggle state
    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        // Dont need to use isMouseOverButton or isButtonDown for toggle button
        juce::ignoreUnused(isMouseOverButton, isButtonDown);
        
        // Choose image based on toggle state
        auto& img = getToggleState() ? onImage : offImage;
        
        // Draw the image centered and scaled to fit
        if (img.isValid())
            g.drawImageWithin (img, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);
    }
};

// A rotary slider that shows the popup bubble and supports double-click numeric entry
// Got inspiration from MicroShift by SoundToys where you double click to set numberic value
class PopupNumericSlider : public juce::Slider
{
    public:
    // Constructor: Rotary slider with no text box
    PopupNumericSlider() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox) {}
    
    // Call this from the editor to attach the built-in popup
    void attachPopupTo (juce::Component* parent)
    {
        setPopupDisplayEnabled(true, true, parent);
    }
    
    // Upon a quick google search juce::Slider has a function for double click to reset to default value so I overrode it
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        //juce::Slider::mouseDoubleClick (e); // keep base behaviour if needed
        
        // Show a numeric entry popup
        auto editorComp = std::make_unique<NumericEntryComponent> (*this);
        editorComp->setSize(80, 30);
        
        auto area = getScreenBounds(); // callout points at the knob
        
        // This part launches the callout box
        juce::CallOutBox::launchAsynchronously(std::move (editorComp), area, nullptr);
    }
    
    
    private:
    // This  component lives inside the CallOutBox
    struct NumericEntryComponent : public juce::Component
    {
        // when i tried to make it a lambda inside PopupNumericSlider::mouseDoubleClick it wouldnt compile
        // so made it a struct instead
        // also had to make explicit constructor to avoid errors
        explicit NumericEntryComponent (PopupNumericSlider& s) : slider (s)
        {
            addAndMakeVisible(text);
            
            text.setText(juce::String (slider.getValue(), 2), juce::dontSendNotification);
            
            // Select all text makes replacement easier
            text.selectAll();
            text.setJustification(juce::Justification::centred);
            
            // Press Enter to commit
            text.onReturnKey = [this]()
            {
                commitAndClose();
            };
            
            // Escape cancels
            text.onEscapeKey = [this]()
            {
                closeOnly();
            };
            
            // Focus lost also cancels
            text.onFocusLost = [this]()
            {
                closeOnly();
            };
        }
        
        void resized() override
        {
            text.setBounds(getLocalBounds().reduced (4));
        }
        
        private:
        
        // Commit the value to the slider and close the popup
        void commitAndClose()
        {
            // Get the new value from the text box
            const auto str = text.getText();
            const double newVal = str.getDoubleValue();
            
            // Clamp to slider’s range
            const double clamped = juce::jlimit(slider.getMinimum(), slider.getMaximum(), newVal);
            
            slider.setValue(clamped, juce::sendNotificationSync);
            closeOnly();
        }
        
        // Just close the popup without changing the slider value
        void closeOnly()
        {
            // Dismiss the callout box
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }
        
        PopupNumericSlider& slider;
        juce::TextEditor text;
    };
    
    // i had to use friend to allow PopupNumericSlider to create NumericEntryComponent
    // which needs access to slider reference inside PopupNumericSlider
    friend struct NumericEntryComponent;
};


class FuzzColaAudioProcessorEditor  : public juce::AudioProcessorEditor
, private juce::Slider::Listener
, private juce::Button::Listener
{
    public:
    FuzzColaAudioProcessorEditor (FuzzColaAudioProcessor&);
    ~FuzzColaAudioProcessorEditor() override;
    
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    private:
    
    
    void syncUiFromParams();
    
    // preset management
    juce::ComboBox presetBox;
    std::unique_ptr<juce::FileChooser> presetChooser;
    juce::Array<juce::File> userPresetFiles;
    
    void refreshPresetBox();
    void handlePresetSelection();
    
    // Select user preset in combo box based on file
    void selectUserPresetByFile (const juce::File& f)
    {
        const auto name = f.getFileNameWithoutExtension();
        
        for (int i = 0; i < userPresetFiles.size(); ++i)
        {
            if (userPresetFiles[i].getFileNameWithoutExtension() == name)
            {
                presetBox.setSelectedId (100 + i, juce::dontSendNotification);
                return;
            }
        }
    }
    
    FuzzColaAudioProcessor& audioProcessor;
    
    // graphics
    juce::Image background;
    
    // Preloaded images for both resolutions
    juce::Image hiBackground,  loBackground;
    
    juce::Image hiSustainStrip, loSustainStrip;
    juce::Image hiToneStrip, loToneStrip;
    juce::Image hiVolumeStrip, loVolumeStrip;
    
    juce::Image hiLedOff, hiLedOn;
    juce::Image loLedOff, loLedOn;
    
    juce::Image hiFootOff, hiFootOn;
    juce::Image loFootOff, loFootOn;
    
    juce::Image hiBypassOff, hiBypassOn;
    juce::Image loBypassOff, loBypassOn;
    
    
    // knobs
    PopupNumericSlider sustainKnob, toneKnob, volumeKnob;
    FilmstripKnobLookAndFeel sustainLNF, toneLNF, volumeLNF;
    
    // switches & LED
    ToggleImageButton bypassToggle;   // was juce::ImageButton
    ToggleImageButton footswitch;     // was juce::ImageButton
    LedComponent led;
    
    // attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> pedalOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneBypassAttachment;
    
    // HiRes / LoRes choice
    bool useHiRes = false;
    
    // states for switches
    bool pedalEngaged = true;    // footswitch: pedal on/off
    bool toneEnabled = true;   // bypass switch: tone stack bypass on/off
    
    // helpers
    void buttonClicked(juce::Button* b) override;
    void updateGraphicsForResolution();
    void sliderValueChanged(juce::Slider* slider) override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuzzColaAudioProcessorEditor)
};
