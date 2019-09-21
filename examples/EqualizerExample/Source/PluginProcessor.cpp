/*
  ==============================================================================

    A simple equalizer using the JUCE dsp module

  ==============================================================================
*/

#include "PluginProcessor.h"

//==============================================================================

namespace IDs
{
    static String paramOutput  { "output" };
    static String paramType    { "type" };
    static String paramFreq    { "freq" };
    static String paramGain    { "gain" };
    static String paramQuality { "quality" };
    static String paramActive  { "active" };
}

StringArray filterNames =
{
    NEEDS_TRANS ("No filter"),
    NEEDS_TRANS ("High pass"),
    NEEDS_TRANS ("1st order high pass"),
    NEEDS_TRANS ("Low shelf"),
    NEEDS_TRANS ("Band pass"),
    NEEDS_TRANS ("Notch"),
    NEEDS_TRANS ("Peak"),
    NEEDS_TRANS ("High shelf"),
    NEEDS_TRANS ("1st order low pass"),
    NEEDS_TRANS ("Low pass")
};

std::unique_ptr<AudioProcessorParameterGroup> createParametersForFilter (const String& prefix,
                                                                         const String& name,
                                                                         EqualizerExampleAudioProcessor::FilterType type,
                                                                         float frequency,
                                                                         float gain,
                                                                         float quality,
                                                                         bool active)
{
    const float maxLevel = 24.0f;

    juce::NormalisableRange<float> freqRange (20.0f, 20000.0f, [](float start, float end, float normalised)
                                              {
                                                  return start + (std::pow (2.0f, normalised * 10.0f) - 1.0f) * (end - start) / 1023.0f;
                                              },
                                              [](float start, float end, float value)
                                              {
                                                  return (std::log (((value - start) * 1023.0f / (end - start)) + 1.0f) / std::log ( 2.0f)) / 10.0f;
                                              },
                                              [](float start, float end, float value)
                                              {
                                                  if (value > 3000.0f)
                                                      return jlimit (start, end, 100.0f * roundToInt (value / 100.0f));

                                                  if (value > 1000.0f)
                                                      return jlimit (start, end, 10.0f * roundToInt (value / 10.0f));

                                                  return jlimit (start, end, float (roundToInt (value)));
                                              });

    auto typeParameter = std::make_unique<AudioParameterChoice> (prefix + IDs::paramType,
                                                                 prefix + TRANS ("Filter Type"),
                                                                 filterNames,
                                                                 type);

    auto freqParameter = std::make_unique<AudioParameterFloat> (prefix + IDs::paramFreq,
                                                                prefix + TRANS ("Frequency"),
                                                                freqRange,
                                                                frequency,
                                                                String(),
                                                                AudioProcessorParameter::genericParameter,
                                                                [](float value, int) { return (value < 1000) ?
                                                                    String (value, 0) + " Hz" :
                                                                    String (value / 1000.0) + " kHz"; },
                                                                [](String text) { return text.endsWith(" kHz") ?
                                                                    text.getFloatValue() * 1000.0 :
                                                                    text.getFloatValue(); });

    auto qltyParameter = std::make_unique<AudioParameterFloat> (prefix + IDs::paramQuality,
                                                                prefix + TRANS ("Quality"),
                                                                NormalisableRange<float> {0.1f, 10.0f, 0.1f, std::log (0.5f) / std::log (0.9f / 9.9f)},
                                                                quality,
                                                                String(),
                                                                AudioProcessorParameter::genericParameter,
                                                                [](float value, int) { return String (value, 1); },
                                                                [](const String& text) { return text.getFloatValue(); });

    auto gainParameter = std::make_unique<AudioParameterFloat> (prefix + IDs::paramGain,
                                                                prefix + TRANS ("Gain"),
                                                                NormalisableRange<float> {-maxLevel, maxLevel, 0.1f},
                                                                gain,
                                                                String(),
                                                                AudioProcessorParameter::genericParameter,
                                                                [](float value, int) {return String (value, 1) + " dB";},
                                                                [](String text) {return text.getFloatValue();});

    auto actvParameter = std::make_unique<AudioParameterBool> (prefix + IDs::paramActive,
                                                               prefix + TRANS ("Active"),
                                                               active,
                                                               String(),
                                                               [](float value, int) {return value > 0.5f ? TRANS ("active") : TRANS ("bypassed");},
                                                               [](String text) {return text == TRANS ("active");});

    auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + prefix, prefix + name, "|",
                                                                 std::move (typeParameter),
                                                                 std::move (freqParameter),
                                                                 std::move (qltyParameter),
                                                                 std::move (gainParameter),
                                                                 std::move (actvParameter));

    return group;
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<AudioProcessorParameterGroup>> params;

    auto param = std::make_unique<AudioParameterFloat> (IDs::paramOutput, TRANS ("Output"),
                                                        NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f,
                                                        String(),
                                                        AudioProcessorParameter::genericParameter,
                                                        [](float value, int) {return String (Decibels::gainToDecibels(value), 1) + " dB";},
                                                        [](String text) {return Decibels::decibelsToGain (text.getFloatValue());});

    auto group = std::make_unique<AudioProcessorParameterGroup> ("global", TRANS ("Globals"), "|", std::move (param));
    params.push_back (std::move (group));

    params.push_back (createParametersForFilter ("Q1", NEEDS_TRANS ("Lowest"),    EqualizerExampleAudioProcessor::HighPass,     40.0f, 0.707f, 1.0f, true));
    params.push_back (createParametersForFilter ("Q2", NEEDS_TRANS ("Low"),       EqualizerExampleAudioProcessor::LowShelf,    250.0f, 0.707f, 1.0f, true));
    params.push_back (createParametersForFilter ("Q3", NEEDS_TRANS ("Low Mids"),  EqualizerExampleAudioProcessor::Peak,        500.0f, 0.707f, 1.0f, true));
    params.push_back (createParametersForFilter ("Q4", NEEDS_TRANS ("High Mids"), EqualizerExampleAudioProcessor::Peak,       1000.0f, 0.707f, 1.0f, true));
    params.push_back (createParametersForFilter ("Q5", NEEDS_TRANS ("High"),      EqualizerExampleAudioProcessor::HighShelf,  5000.0f, 0.707f, 1.0f, true));
    params.push_back (createParametersForFilter ("Q6", NEEDS_TRANS ("Highest"),   EqualizerExampleAudioProcessor::LowPass,   12000.0f, 0.707f, 1.0f, true));

    return { params.begin(), params.end() };
}


//==============================================================================
EqualizerExampleAudioProcessor::EqualizerExampleAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    :
#endif
    treeState (*this, nullptr, JucePlugin_Name, createParameterLayout())
{
    treeState.addParameterListener (IDs::paramGain, this);
}

EqualizerExampleAudioProcessor::~EqualizerExampleAudioProcessor()
{
    treeState.removeParameterListener (IDs::paramGain, this);
}

//==============================================================================
void EqualizerExampleAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = getTotalNumOutputChannels();

    dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = uint32 (samplesPerBlock);
    spec.numChannels = uint32 (numChannels);

    filter.get<6>().setGainLinear (*treeState.getRawParameterValue (IDs::paramOutput));

    attachment1.setSampleRate (sampleRate);
    attachment2.setSampleRate (sampleRate);
    attachment3.setSampleRate (sampleRate);
    attachment4.setSampleRate (sampleRate);
    attachment5.setSampleRate (sampleRate);
    attachment6.setSampleRate (sampleRate);

    filter.prepare (spec);
}

void EqualizerExampleAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EqualizerExampleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
#else

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void EqualizerExampleAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    filter.setBypassed<0>(attachment1.isActive() == false);
    filter.setBypassed<1>(attachment2.isActive() == false);
    filter.setBypassed<2>(attachment3.isActive() == false);
    filter.setBypassed<3>(attachment4.isActive() == false);
    filter.setBypassed<4>(attachment5.isActive() == false);
    filter.setBypassed<5>(attachment6.isActive() == false);

    dsp::AudioBlock<float>              ioBuffer (buffer);
    dsp::ProcessContextReplacing<float> context  (ioBuffer);
    filter.process (context);
}

void EqualizerExampleAudioProcessor::parameterChanged (const String& parameterID, float newValue)
{
    filter.get<6>().setGainLinear (newValue);
}

//==============================================================================

EqualizerExampleAudioProcessor::FilterAttachment::FilterAttachment (AudioProcessorValueTreeState& stateToUse, FilterBand& filterToControl, const String& prefixToUse, const CriticalSection& lock)
  : state               (stateToUse),
    filter              (filterToControl),
    prefix              (prefixToUse),
    callbackLock        (lock),
    frequencyAttachment (*this, frequency, prefix + IDs::paramFreq),
    gainAttachment      (*this, gain,      prefix + IDs::paramGain),
    qualityAttachment   (*this, quality,   prefix + IDs::paramQuality),
    activeAttachment    (*this, active,    prefix + IDs::paramActive)
{
    state.addParameterListener (prefix + IDs::paramType, this);
    updateFilter();
}

EqualizerExampleAudioProcessor::FilterAttachment::~FilterAttachment()
{
    state.removeParameterListener (prefix + IDs::paramType, this);
}

void EqualizerExampleAudioProcessor::FilterAttachment::updateFilter()
{
    if (sampleRate < 20.0)
        return;

    dsp::IIR::Coefficients<float>::Ptr newCoefficients;
    switch (type)
    {
        case NoFilter:
            newCoefficients = new dsp::IIR::Coefficients<float> (1, 0, 1, 0);
            break;
        case LowPass:
            newCoefficients = dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, frequency, quality);
            break;
        case LowPass1st:
            newCoefficients = dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sampleRate, frequency);
            break;
        case LowShelf:
            newCoefficients = dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, frequency, quality, Decibels::decibelsToGain (gain));
            break;
        case BandPass:
            newCoefficients = dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, frequency, quality);
            break;
        case Notch:
            newCoefficients = dsp::IIR::Coefficients<float>::makeNotch (sampleRate, frequency, quality);
            break;
        case Peak:
            newCoefficients = dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, frequency, quality, Decibels::decibelsToGain (gain));
            break;
        case HighShelf:
            newCoefficients = dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, frequency, quality, Decibels::decibelsToGain (gain));
            break;
        case HighPass1st:
            newCoefficients = dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, frequency);
            break;
        case HighPass:
            newCoefficients = dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, frequency, quality);
            break;
        default:
            break;
    }

    if (newCoefficients)
    {
        ScopedLock processLock (callbackLock);
        *filter.state = *newCoefficients;
    }
}

void EqualizerExampleAudioProcessor::FilterAttachment::parameterChanged (const String& parameterID, float newValue)
{
    type = FilterType (roundToInt (newValue));
    updateFilter();
}

void EqualizerExampleAudioProcessor::FilterAttachment::setSampleRate (double sampleRateToUse)
{
    sampleRate = sampleRateToUse;
    updateFilter();
}

template<typename ValueType>
EqualizerExampleAudioProcessor::FilterAttachment::AttachedValue<ValueType>::AttachedValue (FilterAttachment& ownerToUse, ValueType& valueToUse, const String& paramToUse)
  : owner (ownerToUse),
    value (valueToUse),
    paramID (paramToUse)
{
    value = *owner.state.getRawParameterValue (paramID);
    owner.state.addParameterListener (paramID, this);
}

template<typename ValueType>
EqualizerExampleAudioProcessor::FilterAttachment::AttachedValue<ValueType>::~AttachedValue()
{
    owner.state.removeParameterListener (paramID, this);
}

template<typename ValueType>
void EqualizerExampleAudioProcessor::FilterAttachment::AttachedValue<ValueType>::parameterChanged (const String& parameterID, float newValue)
{
    value = newValue;
    owner.updateFilter();
}

template<>
void EqualizerExampleAudioProcessor::FilterAttachment::AttachedValue<bool>::parameterChanged (const String& parameterID, float newValue)
{
    value = (newValue > 0.5f);
    owner.updateFilter();
}

//==============================================================================
bool EqualizerExampleAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* EqualizerExampleAudioProcessor::createEditor()
{
    auto* editor = new foleys::MagicPluginEditor (magicState);
    editor->restoreGUI (BinaryData::magic_xml, BinaryData::magic_xmlSize);
    return editor;
}

//==============================================================================
void EqualizerExampleAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    MemoryOutputStream stream(destData, false);
    treeState.state.writeToStream (stream);
}

void EqualizerExampleAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ValueTree tree = ValueTree::readFromData (data, size_t (sizeInBytes));
    if (tree.isValid())
        treeState.state = tree;
}

//==============================================================================
const String EqualizerExampleAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EqualizerExampleAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EqualizerExampleAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EqualizerExampleAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EqualizerExampleAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EqualizerExampleAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EqualizerExampleAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EqualizerExampleAudioProcessor::setCurrentProgram (int index)
{
}

const String EqualizerExampleAudioProcessor::getProgramName (int index)
{
    return {};
}

void EqualizerExampleAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqualizerExampleAudioProcessor();
}
