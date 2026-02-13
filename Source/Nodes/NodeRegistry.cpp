#include "Nodes/NodeRegistry.h"
#include "Nodes/Audio/AudioInputNode.h"
#include "Nodes/Audio/GainNode.h"
#include "Nodes/Audio/FFTAnalyzerNode.h"
#include "Nodes/Audio/EnvelopeFollowerNode.h"
#include "Nodes/Audio/BandSplitterNode.h"
#include "Nodes/Audio/SmoothingNode.h"
#include "Nodes/Math/AddNode.h"
#include "Nodes/Math/MultiplyNode.h"
#include "Nodes/Math/MapRangeNode.h"
#include "Nodes/Math/ClampNode.h"
#include "Nodes/Visual/ColorMapNode.h"
#include "Nodes/Visual/WaveformRendererNode.h"
#include "Nodes/Visual/SpectrumRendererNode.h"
#include "Nodes/Visual/ShaderVisualNode.h"
#include "Nodes/Visual/OutputCanvasNode.h"

namespace pf
{

struct RegistryInitializer
{
    RegistryInitializer()
    {
        auto& r = NodeRegistry::instance();
        r.registerNode<AudioInputNode>();
        r.registerNode<GainNode>();
        r.registerNode<FFTAnalyzerNode>();
        r.registerNode<EnvelopeFollowerNode>();
        r.registerNode<BandSplitterNode>();
        r.registerNode<SmoothingNode>();
        r.registerNode<AddNode>();
        r.registerNode<MultiplyNode>();
        r.registerNode<MapRangeNode>();
        r.registerNode<ClampNode>();
        r.registerNode<ColorMapNode>();
        r.registerNode<WaveformRendererNode>();
        r.registerNode<SpectrumRendererNode>();
        r.registerNode<ShaderVisualNode>();
        r.registerNode<OutputCanvasNode>();
    }
};

static RegistryInitializer registryInit_;

} // namespace pf
