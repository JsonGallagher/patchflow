#pragma once
#include "Nodes/NodeBase.h"
#include <juce_opengl/juce_opengl.h>

namespace pf
{

class TextureInputNode : public NodeBase
{
public:
    TextureInputNode()
    {
        addOutput ("texture", PortType::Texture);

        addParam ("filePath", juce::String(), {}, {}, "File Path", "Path to PNG/JPEG/BMP image", "", "Source");
    }

    juce::String getTypeId()      const override { return "TextureInput"; }
    juce::String getDisplayName() const override { return "Texture Input"; }
    juce::String getCategory()    const override { return "Visual"; }
    bool isVisualNode()           const override { return true; }

    void renderFrame (juce::OpenGLContext& gl) override
    {
        auto filePath = getParam ("filePath").toString();

        if (filePath != loadedPath_)
        {
            loadedPath_ = filePath;
            needsReload_ = true;
        }

        if (needsReload_ && loadedPath_.isNotEmpty())
        {
            juce::File imageFile (loadedPath_);
            if (imageFile.existsAsFile())
            {
                auto image = juce::ImageFileFormat::loadFrom (imageFile);
                if (image.isValid())
                {
                    image = image.convertedToFormat (juce::Image::ARGB);
                    int w = image.getWidth();
                    int h = image.getHeight();

                    if (texture_ == 0)
                        juce::gl::glGenTextures (1, &texture_);

                    juce::gl::glBindTexture (juce::gl::GL_TEXTURE_2D, texture_);

                    juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
                    juce::gl::glTexImage2D (juce::gl::GL_TEXTURE_2D, 0, juce::gl::GL_RGBA8,
                                            w, h, 0,
                                            juce::gl::GL_BGRA, juce::gl::GL_UNSIGNED_BYTE,
                                            data.data);

                    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MIN_FILTER, juce::gl::GL_LINEAR);
                    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_MAG_FILTER, juce::gl::GL_LINEAR);
                    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_S, juce::gl::GL_CLAMP_TO_EDGE);
                    juce::gl::glTexParameteri (juce::gl::GL_TEXTURE_2D, juce::gl::GL_TEXTURE_WRAP_T, juce::gl::GL_CLAMP_TO_EDGE);
                }
            }
            needsReload_ = false;
        }

        setTextureOutput (0, texture_);
    }

private:
    juce::uint32 texture_ = 0;
    juce::String loadedPath_;
    bool needsReload_ = false;
};

} // namespace pf
