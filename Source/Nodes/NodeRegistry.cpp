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
#include "Nodes/Visual/BlendNode.h"
#include "Nodes/Visual/TransformNode.h"
#include "Nodes/Visual/FeedbackNode.h"
#include "Nodes/Visual/KaleidoscopeNode.h"
#include "Nodes/Visual/DisplaceNode.h"
#include "Nodes/Visual/BloomNode.h"
#include "Nodes/Visual/WaveformRendererNode.h"
#include "Nodes/Visual/SpectrumRendererNode.h"
#include "Nodes/Visual/ShaderVisualNode.h"
#include "Nodes/Visual/OutputCanvasNode.h"
// New nodes
#include "Nodes/Audio/BeatDetectorNode.h"
#include "Nodes/Audio/SpectralFeaturesNode.h"
#include "Nodes/Audio/ChromagramNode.h"
#include "Nodes/Visual/NoiseNode.h"
#include "Nodes/Visual/SDFShapeNode.h"
#include "Nodes/Visual/GradientNode.h"
#include "Nodes/Visual/PatternNode.h"
#include "Nodes/Visual/BlurNode.h"
#include "Nodes/Visual/MirrorNode.h"
#include "Nodes/Visual/TileNode.h"
#include "Nodes/Visual/EdgeDetectNode.h"
#include "Nodes/Visual/ChromaticAberrationNode.h"
#include "Nodes/Visual/ColorGradeNode.h"
#include "Nodes/Visual/GlitchNode.h"
#include "Nodes/Visual/LFONode.h"
#include "Nodes/Visual/TriggerNode.h"
#include "Nodes/Visual/StepSequencerNode.h"
#include "Nodes/Visual/ReactionDiffusionNode.h"
#include "Nodes/Visual/ParticleNode.h"
#include "Nodes/Visual/TextureInputNode.h"

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
        r.registerNode<BlendNode>();
        r.registerNode<TransformNode>();
        r.registerNode<FeedbackNode>();
        r.registerNode<KaleidoscopeNode>();
        r.registerNode<DisplaceNode>();
        r.registerNode<BloomNode>();
        r.registerNode<WaveformRendererNode>();
        r.registerNode<SpectrumRendererNode>();
        r.registerNode<ShaderVisualNode>();
        r.registerNode<OutputCanvasNode>();
        // New nodes
        r.registerNode<BeatDetectorNode>();
        r.registerNode<SpectralFeaturesNode>();
        r.registerNode<ChromagramNode>();
        r.registerNode<NoiseNode>();
        r.registerNode<SDFShapeNode>();
        r.registerNode<GradientNode>();
        r.registerNode<PatternNode>();
        r.registerNode<BlurNode>();
        r.registerNode<MirrorNode>();
        r.registerNode<TileNode>();
        r.registerNode<EdgeDetectNode>();
        r.registerNode<ChromaticAberrationNode>();
        r.registerNode<ColorGradeNode>();
        r.registerNode<GlitchNode>();
        r.registerNode<LFONode>();
        r.registerNode<TriggerNode>();
        r.registerNode<StepSequencerNode>();
        r.registerNode<ReactionDiffusionNode>();
        r.registerNode<ParticleNode>();
        r.registerNode<TextureInputNode>();
    }
};

static RegistryInitializer registryInit_;

} // namespace pf
