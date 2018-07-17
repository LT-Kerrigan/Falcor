/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "Framework.h"
#include "SceneRenderPass.h"

namespace Falcor
{
    static std::string kDepth = "depth";
    static std::string kColor = "color";
    static std::string kMotionVecs = "motionVecs";
    static std::string kNormals = "normals";
    static std::string kVisBuffer = "visibilityBuffer";

    SceneRenderPass::SharedPtr SceneRenderPass::create()
    {
        try
        {
            return SharedPtr(new SceneRenderPass);
        }
        catch (const std::exception&)
        {
            return nullptr;
        }
    }

    SceneRenderPass::SceneRenderPass() : RenderPass("SceneRenderPass")
    {
        GraphicsProgram::SharedPtr pProgram = GraphicsProgram::createFromFile("RenderPasses/SceneRenderPass.slang", "", "ps");
        mpState = GraphicsState::create();
        mpState->setProgram(pProgram);
        mpVars = GraphicsVars::create(pProgram->getReflector());
        Sampler::Desc samplerDesc;
        samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
        mpVars->setSampler("gSampler", Sampler::create(samplerDesc));

        mpFbo = Fbo::create();
        
        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthTest(true).setDepthWriteMask(false).setStencilTest(false).setDepthFunc(DepthStencilState::Func::LessEqual);
        mpDsNoDepthWrite = DepthStencilState::create(dsDesc);        
    }

    void SceneRenderPass::reflect(RenderPassReflection& reflector) const
    {
        reflector.addInput(kVisBuffer);
        reflector.addInputOutput(kDepth).setFlags(RenderPassReflection::Field::Flags::Optional).setBindFlags(Resource::BindFlags::DepthStencil);
        reflector.addInputOutput(kColor);
        reflector.addOutput(kNormals).setFormat(ResourceFormat::RGBA8Unorm);
        reflector.addOutput(kMotionVecs).setFormat(ResourceFormat::RG16Float);
    }

    void SceneRenderPass::setScene(const Scene::SharedPtr& pScene)
    {
        mpSceneRenderer = nullptr;
        if (pScene)
        {
            mpSceneRenderer = SceneRenderer::create(pScene);
        }
    }

    void SceneRenderPass::initDepth(const RenderData* pRenderData)
    {
        const auto& pTexture = pRenderData->getTexture(kDepth);

        if (pTexture)
        {
            mpState->setDepthStencilState(mpDsNoDepthWrite);
            mClearFlags = FboAttachmentType::Color;
            mpFbo->attachDepthStencilTarget(pTexture);
        }
        else
        {
            mpState->setDepthStencilState(nullptr);
            mClearFlags = FboAttachmentType::Color | FboAttachmentType::Depth;
            if(mpFbo->getDepthStencilTexture() == nullptr)
            {
                auto pDepth = Texture::create2D(mpFbo->getWidth(), mpFbo->getHeight(), ResourceFormat::D32Float, 1, 1, nullptr, Resource::BindFlags::DepthStencil);
                mpFbo->attachDepthStencilTarget(pDepth);
            }
        }
    }

    void SceneRenderPass::execute(RenderContext* pContext, const RenderData* pRenderData)
    {
        initDepth(pRenderData);
        mpFbo->attachColorTarget(pRenderData->getTexture(kColor), 0);
        mpFbo->attachColorTarget(pRenderData->getTexture(kNormals), 1);
        mpFbo->attachColorTarget(pRenderData->getTexture(kMotionVecs), 2);
//        pContext->clearFbo(mpFbo.get(), mClearColor, 1, 0, mClearFlags);
        pContext->clearRtv(mpFbo->getRenderTargetView(1).get(), vec4(0));
        pContext->clearRtv(mpFbo->getRenderTargetView(2).get(), vec4(0));

        if (mpSceneRenderer)
        {
            mpVars["PerFrameCB"]["gRenderTargetDim"] = vec2(mpFbo->getWidth(), mpFbo->getHeight());
            mpVars->setTexture(kVisBuffer, pRenderData->getTexture(kVisBuffer));

            mpState->setFbo(mpFbo);
            pContext->pushGraphicsState(mpState);
            pContext->pushGraphicsVars(mpVars);
            mpSceneRenderer->renderScene(pContext);
            pContext->popGraphicsState();
            pContext->popGraphicsVars();
        }
    }

    void SceneRenderPass::renderUI(Gui* pGui, const char* uiGroup)
    {
        if(!uiGroup || pGui->beginGroup(uiGroup))
        {
            pGui->addRgbaColor("Clear color", mClearColor);

            if (uiGroup) pGui->endGroup();
        }
    }
}