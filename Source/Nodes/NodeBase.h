#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_opengl/juce_opengl.h>
#include "Graph/PortTypes.h"
#include <span>
#include <vector>

namespace pf
{

struct Port
{
    juce::String  name;
    PortType      type;
    PortDirection direction;
    int           index = 0;
};

struct NodeParam
{
    juce::String name;
    juce::var    defaultValue;
    juce::var    minValue;
    juce::var    maxValue;
};

class NodeBase
{
public:
    virtual ~NodeBase() = default;

    virtual juce::String getTypeId() const = 0;
    virtual juce::String getDisplayName() const = 0;
    virtual juce::String getCategory() const = 0;

    //==============================================================================
    // Port accessors
    const std::vector<Port>& getInputs()  const { return inputs_; }
    const std::vector<Port>& getOutputs() const { return outputs_; }
    const std::vector<NodeParam>& getParams() const { return params_; }

    int getNumInputs()  const { return static_cast<int> (inputs_.size()); }
    int getNumOutputs() const { return static_cast<int> (outputs_.size()); }

    //==============================================================================
    // Lifecycle
    virtual void prepareToPlay (double sampleRate, int blockSize)
    {
        sampleRate_ = sampleRate;
        blockSize_  = blockSize;
    }

    /** Audio/Control-rate processing (audio thread). */
    virtual void processBlock (int /*numSamples*/) {}

    /** Frame-rate visual processing (GL thread). */
    virtual void renderFrame (juce::OpenGLContext& /*gl*/) {}

    /** Whether this node runs on the GL thread. */
    virtual bool isVisualNode() const { return false; }

    //==============================================================================
    // Output data storage — nodes write to these during processBlock/renderFrame
    float* getAudioOutputBuffer (int outputIndex)
    {
        if (outputIndex < static_cast<int> (audioOutputBuffers_.size()))
            return audioOutputBuffers_[outputIndex].data();
        return nullptr;
    }

    float getSignalOutputValue (int outputIndex) const
    {
        if (outputIndex < static_cast<int> (signalOutputValues_.size()))
            return signalOutputValues_[outputIndex];
        return 0.f;
    }

    void setSignalOutputValue (int outputIndex, float val)
    {
        if (outputIndex < static_cast<int> (signalOutputValues_.size()))
            signalOutputValues_[outputIndex] = val;
    }

    std::span<const float> getBufferOutputData (int outputIndex) const
    {
        if (outputIndex < static_cast<int> (bufferOutputData_.size()))
            return { bufferOutputData_[outputIndex].data(),
                     bufferOutputData_[outputIndex].size() };
        return {};
    }

    float getVisualOutputValue (int outputIndex) const
    {
        if (outputIndex < static_cast<int> (visualOutputValues_.size()))
            return visualOutputValues_[outputIndex];
        return 0.f;
    }

    void setVisualOutputValue (int outputIndex, float val)
    {
        if (outputIndex < static_cast<int> (visualOutputValues_.size()))
            visualOutputValues_[outputIndex] = val;
    }

    juce::uint32 getTextureOutput (int outputIndex) const
    {
        if (outputIndex < static_cast<int> (textureOutputs_.size()))
            return textureOutputs_[outputIndex];
        return 0;
    }

    void setTextureOutput (int outputIndex, juce::uint32 tex)
    {
        if (outputIndex < static_cast<int> (textureOutputs_.size()))
            textureOutputs_[outputIndex] = tex;
    }

    //==============================================================================
    // Input connection pointers — set by RuntimeGraph
    struct InputConnection
    {
        NodeBase* sourceNode = nullptr;
        int       sourceOutputIndex = 0;
    };

    void setInputConnection (int inputIndex, NodeBase* source, int sourceOutputIdx)
    {
        if (inputIndex < static_cast<int> (inputConnections_.size()))
            inputConnections_[inputIndex] = { source, sourceOutputIdx };
    }

    // Reading connected input values
    float* getConnectedAudioBuffer (int inputIndex) const
    {
        auto& conn = inputConnections_[inputIndex];
        return conn.sourceNode ? conn.sourceNode->getAudioOutputBuffer (conn.sourceOutputIndex) : nullptr;
    }

    float getConnectedSignalValue (int inputIndex) const
    {
        auto& conn = inputConnections_[inputIndex];
        return conn.sourceNode ? conn.sourceNode->getSignalOutputValue (conn.sourceOutputIndex) : 0.f;
    }

    std::span<const float> getConnectedBufferData (int inputIndex) const
    {
        auto& conn = inputConnections_[inputIndex];
        return conn.sourceNode ? conn.sourceNode->getBufferOutputData (conn.sourceOutputIndex) : std::span<const float>{};
    }

    float getConnectedVisualValue (int inputIndex) const
    {
        auto& conn = inputConnections_[inputIndex];
        if (! conn.sourceNode) return 0.f;
        auto srcType = conn.sourceNode->getOutputs()[conn.sourceOutputIndex].type;
        if (srcType == PortType::Signal)
            return conn.sourceNode->getSignalOutputValue (conn.sourceOutputIndex);
        return conn.sourceNode->getVisualOutputValue (conn.sourceOutputIndex);
    }

    juce::uint32 getConnectedTexture (int inputIndex) const
    {
        auto& conn = inputConnections_[inputIndex];
        return conn.sourceNode ? conn.sourceNode->getTextureOutput (conn.sourceOutputIndex) : 0;
    }

    bool isInputConnected (int inputIndex) const
    {
        return inputIndex < static_cast<int> (inputConnections_.size())
            && inputConnections_[inputIndex].sourceNode != nullptr;
    }

    //==============================================================================
    // Parameters (read from ValueTree, written by GUI)
    void setParamTree (juce::ValueTree paramTree) { paramTree_ = paramTree; }

    juce::var getParam (const juce::String& name) const
    {
        return paramTree_.getProperty (juce::Identifier (name));
    }

    float getParamAsFloat (const juce::String& name, float fallback = 0.f) const
    {
        auto val = paramTree_.getProperty (juce::Identifier (name));
        return val.isVoid() ? fallback : static_cast<float> (val);
    }

    int getParamAsInt (const juce::String& name, int fallback = 0) const
    {
        auto val = paramTree_.getProperty (juce::Identifier (name));
        return val.isVoid() ? fallback : static_cast<int> (val);
    }

    //==============================================================================
    // State
    bool isBypassed() const { return bypassed_; }
    void setBypassed (bool b) { bypassed_ = b; }

    juce::String nodeId;  // unique instance ID

protected:
    void addInput (const juce::String& name, PortType type)
    {
        inputs_.push_back ({ name, type, PortDirection::Input, static_cast<int> (inputs_.size()) });
        inputConnections_.push_back ({});
    }

    void addOutput (const juce::String& name, PortType type)
    {
        int idx = static_cast<int> (outputs_.size());
        outputs_.push_back ({ name, type, PortDirection::Output, idx });

        switch (type)
        {
            case PortType::Audio:
                audioOutputBuffers_.emplace_back();
                break;
            case PortType::Signal:
                signalOutputValues_.push_back (0.f);
                break;
            case PortType::Buffer:
                bufferOutputData_.emplace_back();
                break;
            case PortType::Visual:
                visualOutputValues_.push_back (0.f);
                break;
            case PortType::Texture:
                textureOutputs_.push_back (0);
                break;
        }
    }

    void addParam (const juce::String& name, juce::var defaultVal,
                   juce::var minVal = {}, juce::var maxVal = {})
    {
        params_.push_back ({ name, defaultVal, minVal, maxVal });
    }

    void resizeAudioBuffer (int outputLocalIndex, int numSamples)
    {
        if (outputLocalIndex < static_cast<int> (audioOutputBuffers_.size()))
            audioOutputBuffers_[outputLocalIndex].resize (static_cast<size_t> (numSamples), 0.f);
    }

    void resizeBufferOutput (int outputLocalIndex, int numFloats)
    {
        if (outputLocalIndex < static_cast<int> (bufferOutputData_.size()))
            bufferOutputData_[outputLocalIndex].resize (static_cast<size_t> (numFloats), 0.f);
    }

    std::vector<float>& getAudioOutputBufferVec (int idx) { return audioOutputBuffers_[idx]; }
    std::vector<float>& getBufferOutputVec (int idx)      { return bufferOutputData_[idx]; }

    double sampleRate_ = 44100.0;
    int    blockSize_  = 512;

private:
    std::vector<Port>      inputs_;
    std::vector<Port>      outputs_;
    std::vector<NodeParam> params_;

    // Output storage — one entry per output port of each type
    std::vector<std::vector<float>> audioOutputBuffers_;
    std::vector<float>              signalOutputValues_;
    std::vector<std::vector<float>> bufferOutputData_;
    std::vector<float>              visualOutputValues_;
    std::vector<juce::uint32>       textureOutputs_;

    // Resolved input connections
    std::vector<InputConnection> inputConnections_;

    juce::ValueTree paramTree_;
    bool bypassed_ = false;
};

} // namespace pf
