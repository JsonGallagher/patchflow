cmake_minimum_required(VERSION 3.24)

project(PatchFlow VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0")

add_subdirectory(JUCE)

juce_add_gui_app(PatchFlow
    PRODUCT_NAME "PatchFlow"
    BUNDLE_ID "com.patchflow.app"
    COMPANY_NAME "PatchFlow"
    ICON_BIG ""
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE
    PLIST_TO_MERGE
        "<plist><dict><key>NSMicrophoneUsageDescription</key><string>PatchFlow needs microphone access to visualize audio input.</string></dict></plist>"
)

target_sources(PatchFlow PRIVATE
    # Main
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp

    # Graph
    Source/Graph/PortTypes.h
    Source/Graph/Connection.h
    Source/Graph/GraphModel.h
    Source/Graph/GraphModel.cpp
    Source/Graph/GraphCompiler.h
    Source/Graph/GraphCompiler.cpp
    Source/Graph/RuntimeGraph.h
    Source/Graph/RuntimeGraph.cpp

    # Nodes
    Source/Nodes/NodeBase.h
    Source/Nodes/NodeBase.cpp
    Source/Nodes/NodeRegistry.h
    Source/Nodes/NodeRegistry.cpp
    Source/Nodes/Audio/AudioInputNode.h
    Source/Nodes/Audio/AudioInputNode.cpp
    Source/Nodes/Audio/GainNode.h
    Source/Nodes/Audio/GainNode.cpp
    Source/Nodes/Audio/FFTAnalyzerNode.h
    Source/Nodes/Audio/FFTAnalyzerNode.cpp
    Source/Nodes/Audio/EnvelopeFollowerNode.h
    Source/Nodes/Audio/EnvelopeFollowerNode.cpp
    Source/Nodes/Audio/BandSplitterNode.h
    Source/Nodes/Audio/BandSplitterNode.cpp
    Source/Nodes/Audio/SmoothingNode.h
    Source/Nodes/Audio/SmoothingNode.cpp
    Source/Nodes/Audio/BeatDetectorNode.h
    Source/Nodes/Audio/BeatDetectorNode.cpp
    Source/Nodes/Audio/SpectralFeaturesNode.h
    Source/Nodes/Audio/SpectralFeaturesNode.cpp
    Source/Nodes/Audio/ChromagramNode.h
    Source/Nodes/Audio/ChromagramNode.cpp
    Source/Nodes/Math/AddNode.h
    Source/Nodes/Math/AddNode.cpp
    Source/Nodes/Math/MultiplyNode.h
    Source/Nodes/Math/MultiplyNode.cpp
    Source/Nodes/Math/MapRangeNode.h
    Source/Nodes/Math/MapRangeNode.cpp
    Source/Nodes/Math/ClampNode.h
    Source/Nodes/Math/ClampNode.cpp
    Source/Nodes/Visual/ColorMapNode.h
    Source/Nodes/Visual/ColorMapNode.cpp
    Source/Nodes/Visual/BlendNode.h
    Source/Nodes/Visual/BlendNode.cpp
    Source/Nodes/Visual/TransformNode.h
    Source/Nodes/Visual/TransformNode.cpp
    Source/Nodes/Visual/FeedbackNode.h
    Source/Nodes/Visual/FeedbackNode.cpp
    Source/Nodes/Visual/KaleidoscopeNode.h
    Source/Nodes/Visual/KaleidoscopeNode.cpp
    Source/Nodes/Visual/DisplaceNode.h
    Source/Nodes/Visual/DisplaceNode.cpp
    Source/Nodes/Visual/BloomNode.h
    Source/Nodes/Visual/BloomNode.cpp
    Source/Nodes/Visual/WaveformRendererNode.h
    Source/Nodes/Visual/WaveformRendererNode.cpp
    Source/Nodes/Visual/SpectrumRendererNode.h
    Source/Nodes/Visual/SpectrumRendererNode.cpp
    Source/Nodes/Visual/ShaderVisualNode.h
    Source/Nodes/Visual/ShaderVisualNode.cpp
    Source/Nodes/Visual/OutputCanvasNode.h
    Source/Nodes/Visual/OutputCanvasNode.cpp
    Source/Nodes/Visual/NoiseNode.h
    Source/Nodes/Visual/NoiseNode.cpp
    Source/Nodes/Visual/SDFShapeNode.h
    Source/Nodes/Visual/SDFShapeNode.cpp
    Source/Nodes/Visual/GradientNode.h
    Source/Nodes/Visual/GradientNode.cpp
    Source/Nodes/Visual/PatternNode.h
    Source/Nodes/Visual/PatternNode.cpp
    Source/Nodes/Visual/BlurNode.h
    Source/Nodes/Visual/BlurNode.cpp
    Source/Nodes/Visual/MirrorNode.h
    Source/Nodes/Visual/MirrorNode.cpp
    Source/Nodes/Visual/TileNode.h
    Source/Nodes/Visual/TileNode.cpp
    Source/Nodes/Visual/EdgeDetectNode.h
    Source/Nodes/Visual/EdgeDetectNode.cpp
    Source/Nodes/Visual/ChromaticAberrationNode.h
    Source/Nodes/Visual/ChromaticAberrationNode.cpp
    Source/Nodes/Visual/ColorGradeNode.h
    Source/Nodes/Visual/ColorGradeNode.cpp
    Source/Nodes/Visual/GlitchNode.h
    Source/Nodes/Visual/GlitchNode.cpp
    Source/Nodes/Visual/LFONode.h
    Source/Nodes/Visual/LFONode.cpp
    Source/Nodes/Visual/TriggerNode.h
    Source/Nodes/Visual/TriggerNode.cpp
    Source/Nodes/Visual/StepSequencerNode.h
    Source/Nodes/Visual/StepSequencerNode.cpp
    Source/Nodes/Visual/ReactionDiffusionNode.h
    Source/Nodes/Visual/ReactionDiffusionNode.cpp
    Source/Nodes/Visual/ParticleNode.h
    Source/Nodes/Visual/ParticleNode.cpp
    Source/Nodes/Visual/TextureInputNode.h
    Source/Nodes/Visual/TextureInputNode.cpp

    # Audio
    Source/Audio/AudioEngine.h
    Source/Audio/AudioEngine.cpp
    Source/Audio/AnalysisSnapshot.h

    # UI
    Source/UI/NodeEditorComponent.h
    Source/UI/NodeEditorComponent.cpp
    Source/UI/NodeComponent.h
    Source/UI/NodeComponent.cpp
    Source/UI/PortComponent.h
    Source/UI/PortComponent.cpp
    Source/UI/CableComponent.h
    Source/UI/CableComponent.cpp
    Source/UI/NodePaletteComponent.h
    Source/UI/NodePaletteComponent.cpp
    Source/UI/Theme.h
    Source/UI/InspectorPanel.h
    Source/UI/InspectorPanel.cpp

    # Rendering
    Source/Rendering/VisualCanvas.h
    Source/Rendering/VisualCanvas.cpp
    Source/Rendering/ShaderUtils.h
    Source/Rendering/ShaderUtils.cpp
    Source/Rendering/RenderConfig.h
)

target_compile_definitions(PatchFlow PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:PatchFlow,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:PatchFlow,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PatchFlow PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(PatchFlow PRIVATE Source)

# Bundle factory presets so the in-app preset dropdown works outside the source tree.
add_custom_command(TARGET PatchFlow POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:PatchFlow>/../Resources/ExamplePatches"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Resources/ExamplePatches"
        "$<TARGET_FILE_DIR:PatchFlow>/../Resources/ExamplePatches"
)
