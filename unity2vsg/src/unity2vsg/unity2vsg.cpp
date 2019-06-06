/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <unity2vsg/unity2vsg.h>

#include <unity2vsg/DebugLog.h>
#include <unity2vsg/GraphicsPipelineBuilder.h>
#include <unity2vsg/ShaderUtils.h>

#include <vsg/all.h>
#include <vsg/core/Objects.h>

using namespace unity2vsg;

class LeafDataCollection : public vsg::Visitor
{
public:
    vsg::ref_ptr<vsg::Objects> objects;

    LeafDataCollection()
    {
        objects = new vsg::Objects;
    }

    void apply(vsg::Object& object) override
    {
        if (typeid(object) == typeid(vsg::Texture))
        {
            vsg::Texture* texture = static_cast<vsg::Texture*>(&object);
            if (texture->_textureData)
            {
                objects->addChild(texture->_textureData);
            }
        }

        if (typeid(object) == typeid(vsg::TextureArray))
        {
            vsg::TextureArray* textureArray = static_cast<vsg::TextureArray*>(&object);
            for (auto& texture : textureArray->_textures)
            {
                if (texture->_textureData)
                {
                    objects->addChild(texture->_textureData);
                }
            }
        }

        object.traverse(*this);
    }

    void apply(vsg::Geometry& geometry) override
    {
        for (auto& data : geometry._arrays)
        {
            objects->addChild(data);
        }
        if (geometry._indices)
        {
            objects->addChild(geometry._indices);
        }
    }

    void apply(vsg::VertexIndexDraw& vid) override
    {
        for (auto& data : vid._arrays)
        {
            objects->addChild(data);
        }
        if (vid._indices)
        {
            objects->addChild(vid._indices);
        }
    }

    void apply(vsg::BindVertexBuffers& bvb) override
    {
        for (auto& data : bvb.getArrays())
        {
            objects->addChild(data);
        }
    }

    void apply(vsg::BindIndexBuffer& bib) override
    {
        if (bib.getIndices())
        {
            objects->addChild(vsg::ref_ptr<vsg::Data>(bib.getIndices()));
        }
    }

    void apply(vsg::StateGroup& stategroup) override
    {
        for (auto& command : stategroup.getStateCommands())
        {
            command->accept(*this);
        }

        stategroup.traverse(*this);
    }
};

class LeafDataRelease : public vsg::Visitor
{
public:
    LeafDataRelease() {}

    void apply(vsg::Object& object) override
    {
        if (typeid(object) == typeid(vsg::Texture))
        {
            vsg::Texture* texture = static_cast<vsg::Texture*>(&object);
            if (texture->_textureData)
            {
                texture->_textureData->dataRelease();
            }
        }

        if (typeid(object) == typeid(vsg::TextureArray))
        {
            vsg::TextureArray* textureArray = static_cast<vsg::TextureArray*>(&object);
            for (auto& texture : textureArray->_textures)
            {
                if (texture->_textureData)
                {
                    texture->_textureData->dataRelease();
                }
            }
        }

        object.traverse(*this);
    }

    void apply(vsg::Geometry& geometry) override
    {
        for (auto& data : geometry._arrays)
        {
            data->dataRelease();
        }
        if (geometry._indices)
        {
            //geometry._indices->dataRelease();
        }
    }

    void apply(vsg::VertexIndexDraw& vid) override
    {
        for (auto& data : vid._arrays)
        {
            data->dataRelease();
        }
        if (vid._indices)
        {
            //vid._indices->dataRelease();
        }
    }

    void apply(vsg::BindVertexBuffers& bvb) override
    {
        for (auto& data : bvb.getArrays())
        {
            data->dataRelease();
        }
    }

    void apply(vsg::BindIndexBuffer& bib) override
    {
        if (bib.getIndices())
        {
            //bib.getIndices()->dataRelease();
        }
    }

    void apply(vsg::StateGroup& stategroup) override
    {
        for (auto& command : stategroup.getStateCommands())
        {
            command->accept(*this);
        }

        stategroup.traverse(*this);
    }
};

class GraphBuilder : public vsg::Object
{
public:
    GraphBuilder()
    {
        _root = vsg::MatrixTransform::create();
        pushNodeToStack(_root);
    }

    //
    // Nodes
    //

    void addGroup()
    {
        auto group = vsg::Group::create();
        if (!addChildToHead(group))
        {
            DebugLog("GraphBuilder Warning: Current head is not a group");
        }
        pushNodeToStack(group);
    }

    void addMatrixTrasform(const TransformData& data)
    {
        vsg::mat4 matrix = vsg::mat4(data.matrix.ptr[0], data.matrix.ptr[1], data.matrix.ptr[2], data.matrix.ptr[3],
                                     data.matrix.ptr[4], data.matrix.ptr[5], data.matrix.ptr[6], data.matrix.ptr[7],
                                     data.matrix.ptr[8], data.matrix.ptr[9], data.matrix.ptr[10], data.matrix.ptr[11],
                                     data.matrix.ptr[12], data.matrix.ptr[13], data.matrix.ptr[14], data.matrix.ptr[15]);

        auto transform = vsg::MatrixTransform::create(matrix);

        if (!addChildToHead(transform))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }
        pushNodeToStack(transform);
    }

    void addCullNode(CullData cull)
    {
        auto cullNode = vsg::CullNode::create(vsg::sphere(cull.center, cull.radius), nullptr);
        if (!addChildToHead(cullNode))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }
        pushNodeToStack(cullNode);
    }

    void addCullGroup(CullData cull)
    {
        auto cullGroup = vsg::CullGroup::create(vsg::sphere(cull.center, cull.radius));
        if (!addChildToHead(cullGroup))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }
        pushNodeToStack(cullGroup);
    }

    void addLOD(CullData cull)
    {
        auto lod = vsg::LOD::create();
        lod->setBound(vsg::sphere(cull.center, cull.radius));
        if (!addChildToHead(lod))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }
        pushNodeToStack(lod);
    }

    void addLODChild(LODChildData lodChild)
    {
        auto group = vsg::Group::create();
        if (!addLODChildToHead(group, lodChild))
        {
            DebugLog("GraphBuilder Warning: Current head is not an LOD");
        }
        pushNodeToStack(group);
    }

    void addStateGroup()
    {
        auto stategroup = vsg::StateGroup::create();
        if (!addChildToHead(stategroup))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }
        pushNodeToStack(stategroup);
        _activeStateGroup = stategroup;
    }

    void addCommands()
    {
        auto commands = vsg::Commands::create();
        if (!addChildToHead(commands))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }
        pushNodeToStack(commands);
    }

    void addVertexIndexDraw(const MeshData& data)
    {
        vsg::ref_ptr<vsg::Node> geomNode;

        // has a geometry with this ID already been created
        std::string idstr = std::string(data.id);

        if (_geometryCache.find(idstr) != _geometryCache.end())
        {
            geomNode = _geometryCache[idstr];
        }
        else
        {
            auto geometry = vsg::VertexIndexDraw::create();

            // vertex inputs
            auto inputarrays = vsg::DataList{createVsgArray<vsg::vec3>(data.verticies.ptr, data.verticies.length)}; // always have verticies

            if (data.normals.length > 0) inputarrays.push_back(createVsgArray<vsg::vec3>(data.normals.ptr, data.normals.length));
            if (data.uv0.length > 0) inputarrays.push_back(createVsgArray<vsg::vec2>(data.uv0.ptr, data.uv0.length));

            geometry->_arrays = inputarrays;

            if (data.use32BitIndicies == 0)
            {
                // for now convert the int32 array indicies to uint16
                vsg::ref_ptr<vsg::ushortArray> indiciesushort(new vsg::ushortArray(data.triangles.length));
                for (uint32_t i = 0; i < data.triangles.length; i++)
                {
                    indiciesushort->set(i, static_cast<uint16_t>(data.triangles.ptr[i]));
                }

                geometry->_indices = indiciesushort;
            }
            else
            {
                vsg::ref_ptr<vsg::uintArray> indiciesuint(new vsg::uintArray(data.triangles.length));
                for (uint32_t i = 0; i < data.triangles.length; i++)
                {
                    indiciesuint->set(i, static_cast<uint32_t>(data.triangles.ptr[i]));
                }

                geometry->_indices = indiciesuint;
            }

            geometry->indexCount = data.triangles.length;
            geometry->instanceCount = 1;

            _geometryCache[idstr] = geometry;
            geomNode = geometry;
        }

        if (!addChildToHead(geomNode))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }

        pushNodeToStack(geomNode);
    }

    void addGeometry(const MeshData& data)
    {
        vsg::ref_ptr<vsg::Node> geomNode;

        // has a geometry with this ID already been created
        std::string idstr = std::string(data.id);

        if (_geometryCache.find(idstr) != _geometryCache.end())
        {
            geomNode = _geometryCache[idstr];
        }
        else
        {
            auto geometry = vsg::Geometry::create();

            // vertex inputs
            auto inputarrays = vsg::DataList{createVsgArray<vsg::vec3>(data.verticies.ptr, data.verticies.length)}; // always have verticies

            if (data.normals.length > 0) inputarrays.push_back(createVsgArray<vsg::vec3>(data.normals.ptr, data.normals.length));
            if (data.uv0.length > 0) inputarrays.push_back(createVsgArray<vsg::vec2>(data.uv0.ptr, data.uv0.length));

            geometry->_arrays = inputarrays;

            if (data.use32BitIndicies == 0)
            {
                // for now convert the int32 array indicies to uint16
                vsg::ref_ptr<vsg::ushortArray> indiciesushort(new vsg::ushortArray(data.triangles.length));
                for (uint32_t i = 0; i < data.triangles.length; i++)
                {
                    indiciesushort->set(i, static_cast<uint16_t>(data.triangles.ptr[i]));
                }

                geometry->_indices = indiciesushort;
            }
            else
            {
                vsg::ref_ptr<vsg::uintArray> indiciesuint(new vsg::uintArray(data.triangles.length));
                for (uint32_t i = 0; i < data.triangles.length; i++)
                {
                    indiciesuint->set(i, static_cast<uint32_t>(data.triangles.ptr[i]));
                }

                geometry->_indices = indiciesuint;
            }
            geometry->_commands = {vsg::DrawIndexed::create(data.triangles.length, 1, 0, 0, 0)};

            _geometryCache[idstr] = geometry;
            geomNode = geometry;
        }

        if (!addChildToHead(geomNode))
        {
            DebugLog("GraphBuilder Error: Current head is not a group");
        }

        pushNodeToStack(geomNode);
    }

    //
    // Meta data
    //

    void addStringValue(std::string name, std::string value)
    {
        getHead()->setValue(name, value.c_str());
    }

    //
    // Commands
    //

    vsg::ref_ptr<vsg::ShaderModule> getOrCreateShaderModule(VkShaderStageFlagBits stage, std::string shaderSourceFile, uint32_t inputAtts, uint32_t shaderMode, std::string customDefStr)
    {
        auto split = [](const std::string& str, const char& seperator) {
            std::vector<std::string> elements;

            std::string::size_type prev_pos = 0, pos = 0;

            while ((pos = str.find(seperator, pos)) != std::string::npos)
            {
                auto substring = str.substr(prev_pos, pos - prev_pos);
                elements.push_back(substring);
                prev_pos = ++pos;
            }

            elements.push_back(str.substr(prev_pos, pos - prev_pos));

            return elements;
        };

        std::string shaderkey = std::to_string((int)stage) + "," + shaderSourceFile + "," + std::to_string(inputAtts) + "," + customDefStr;
        std::vector<std::string> customdefs = customDefStr.empty() ? std::vector<std::string>() : split(customDefStr, ',');

        vsg::ref_ptr<vsg::ShaderModule> shaderModule;

        if (_shaderModulesCache.find(shaderkey) != _shaderModulesCache.end())
        {
            shaderModule = _shaderModulesCache[shaderkey];
        }
        else
        {
            if (!shaderSourceFile.empty())
            {
                shaderModule = vsg::ShaderModule::create(readGLSLShader(shaderSourceFile, shaderMode, inputAtts, customdefs));
            }
            else
            {
                if (stage == VK_SHADER_STAGE_VERTEX_BIT)
                {
                    shaderModule = vsg::ShaderModule::create(createFbxVertexSource(shaderMode, inputAtts, customdefs));
                }
                else
                {
                    shaderModule = vsg::ShaderModule::create(createFbxFragmentSource(shaderMode, inputAtts, customdefs));
                }
                _shaderModulesCache[shaderkey] = shaderModule;
            }
        }

        return shaderModule;
    }

    vsg::ref_ptr<vsg::ShaderStage> createShaderStage(VkShaderStageFlagBits stage, vsg::ref_ptr<vsg::ShaderModule> shaderModule, UIntArray specializationConstants)
    {
        auto shaderStage = vsg::ShaderStage::create(stage, "main", shaderModule);

        if (specializationConstants.length > 0)
        {
            vsg::ShaderStage::SpecializationMapEntries specialEntires;
            auto dataarray = new vsg::uintArray(specializationConstants.length);

            for (uint32_t i = 0; i < specializationConstants.length; i++)
            {
                specialEntires.push_back({i, i * sizeof(uint32_t), sizeof(uint32_t)});
                dataarray->at(i) = specializationConstants.ptr[i];
            }

            shaderStage->setSpecializationMapEntries(specialEntires);
            shaderStage->setSpecializationData(dataarray);
        }

        return shaderStage;
    }

    bool addBindGraphicsPipelineCommand(const PipelineData& data, bool addToActiveStateGroup)
    {
        std::string idstr = std::string(data.id);
        vsg::ref_ptr<vsg::BindGraphicsPipeline> bindGraphicsPipeline;

        if (_bindGraphicsPipelineCache.find(idstr) != _bindGraphicsPipelineCache.end())
        {
            bindGraphicsPipeline = _bindGraphicsPipelineCache[idstr];
        }
        else
        {
            vsg::ref_ptr<vsg::GraphicsPipelineBuilder> pipelinebuilder = vsg::GraphicsPipelineBuilder::create();
            vsg::ref_ptr<vsg::GraphicsPipelineBuilder::Traits> traits = vsg::GraphicsPipelineBuilder::Traits::create();

            // vertex input
            vsg::GraphicsPipelineBuilder::Traits::InputAttributeDescriptions inputAttributes = {{{0, VK_FORMAT_R32G32B32_SFLOAT}}};
            uint32_t inputshaderatts = VERTEX;

            if (data.hasNormals)
            {
                inputAttributes.push_back({{1, VK_FORMAT_R32G32B32_SFLOAT}});
                inputshaderatts |= NORMAL;
            }
            if (data.hasTangents)
            {
                inputAttributes.push_back({{2, VK_FORMAT_R32G32B32_SFLOAT}});
                inputshaderatts |= TANGENT;
            }
            if (data.hasColors)
            {
                inputAttributes.push_back({{3, VK_FORMAT_R32G32B32A32_SFLOAT}});
                inputshaderatts |= COLOR;
            }
            if (data.uvChannelCount > 0) // uv set 0
            {
                inputAttributes.push_back({{4, VK_FORMAT_R32G32_SFLOAT}});
                inputshaderatts |= TEXCOORD0;
            }

            traits->vertexAttributeDescriptions[VK_VERTEX_INPUT_RATE_VERTEX] = inputAttributes;

            // descriptor sets layout
            vsg::GraphicsPipelineBuilder::Traits::DescriptorBindingSet bindingSet;
            uint32_t shaderMode = 0;

            vsg::GraphicsPipelineBuilder::Traits::DescriptorBindngs vertexBindings;
            for (uint32_t i = 0; i < data.vertexDescriptorBindings.length; i++)
            {
                vsg::GraphicsPipelineBuilder::Traits::DescriptorBinding& binding = data.vertexDescriptorBindings.ptr[i];
                vertexBindings.push_back(binding);
            }
            bindingSet[VK_SHADER_STAGE_VERTEX_BIT] = vertexBindings;

            vsg::GraphicsPipelineBuilder::Traits::DescriptorBindngs fragmentBindings;
            for (uint32_t i = 0; i < data.fragmentDescriptorBindings.length; i++)
            {
                vsg::GraphicsPipelineBuilder::Traits::DescriptorBinding& binding = data.fragmentDescriptorBindings.ptr[i];
                fragmentBindings.push_back(binding);
            }
            bindingSet[VK_SHADER_STAGE_FRAGMENT_BIT] = fragmentBindings;

            traits->descriptorLayouts = {bindingSet};

            // setup shaders
            std::string vertexShaderSource = data.shader.vertexSource != nullptr ? std::string(data.shader.vertexSource) : "";
            std::string fragmentShaderSource = data.shader.fragmentSource != nullptr ? std::string(data.shader.fragmentSource) : "";
            std::string customDefs = data.shader.customDefines != nullptr ? std::string(data.shader.customDefines) : "";

            auto vertShaderModule = getOrCreateShaderModule(VK_SHADER_STAGE_VERTEX_BIT, vertexShaderSource, inputshaderatts, shaderMode, customDefs);
            auto fragShaderModule = getOrCreateShaderModule(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShaderSource, inputshaderatts, shaderMode, customDefs);

            auto vertStage = createShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule, data.shader.vertexSpecializationData);
            auto fragStage = createShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule, data.shader.fragmentSpecializationData);

            vsg::ShaderStages shaders = {vertStage, fragStage};

            ShaderCompiler shaderCompiler;
            if (!shaderCompiler.compile(shaders))
            {
                DebugLog("GraphBuilder Error: Failed to compile shaders.");
                return false;
            }

            traits->shaderStages = shaders;

            // topology
            traits->primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            // alpha blending
            if (data.useAlpha == 1)
            {
                vsg::ColorBlendState::ColorBlendAttachments colorBlendAttachments;
                VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
                colorBlendAttachment.blendEnable = VK_TRUE;
                colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                      VK_COLOR_COMPONENT_G_BIT |
                                                      VK_COLOR_COMPONENT_B_BIT |
                                                      VK_COLOR_COMPONENT_A_BIT;

                colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
                colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

                traits->colorBlendAttachments.push_back(colorBlendAttachment);
            }

            // create our graphics pipeline
            pipelinebuilder->build(traits);

            bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(pipelinebuilder->getGraphicsPipeline());
            _bindGraphicsPipelineCache[idstr] = bindGraphicsPipeline;
        }

        if (addToActiveStateGroup)
        {
            if (!addStateCommandToActiveStateGroup(bindGraphicsPipeline))
            {
                DebugLog("GraphBuilder Error: No active StateGroup");
                return false;
            }
        }
        else
        {
            if (!addCommandToHead(bindGraphicsPipeline))
            {
                DebugLog("GraphBuilder Error: Current head is not a Commands node");
                return false;
            }
        }

        _activeGraphicsPipeline = bindGraphicsPipeline->getPipeline();
        return true;
    }

    void addBindIndexBufferCommand(unity2vsg::IndexBufferData data)
    {
        std::string idstr = std::string(data.id);

        vsg::ref_ptr<vsg::Command> cmd;

        if (_bindIndexBufferCache.find(idstr) != _bindIndexBufferCache.end())
        {
            cmd = _bindIndexBufferCache[idstr];
        }
        else
        {
            if (data.use32BitIndicies == 0)
            {
                // for now convert the int32 array indicies to uint16
                vsg::ref_ptr<vsg::ushortArray> indiciesushort(new vsg::ushortArray(data.triangles.length));
                for (uint32_t i = 0; i < data.triangles.length; i++)
                {
                    indiciesushort->set(i, static_cast<uint16_t>(data.triangles.ptr[i]));
                }
                cmd = vsg::BindIndexBuffer::create(indiciesushort);
            }
            else
            {
                vsg::ref_ptr<vsg::uintArray> indiciesuint(new vsg::uintArray(data.triangles.length));
                for (uint32_t i = 0; i < data.triangles.length; i++)
                {
                    indiciesuint->set(i, static_cast<uint32_t>(data.triangles.ptr[i]));
                }

                cmd = vsg::BindIndexBuffer::create(indiciesuint);
            }
            _bindIndexBufferCache[idstr] = cmd;
        }
        addCommandToHead(cmd);
    }

    void addBindVertexBuffersCommand(unity2vsg::VertexBuffersData data)
    {
        std::string idstr = std::string(data.id);

        vsg::ref_ptr<vsg::Command> cmd;

        if (_bindVertexBuffersCache.find(idstr) != _bindVertexBuffersCache.end())
        {
            cmd = _bindVertexBuffersCache[idstr];
        }
        else
        {
            auto inputarrays = vsg::DataList{createVsgArray<vsg::vec3>(data.verticies.ptr, data.verticies.length)}; // always have verticies

            if (data.normals.length > 0) inputarrays.push_back(createVsgArray<vsg::vec3>(data.normals.ptr, data.normals.length));
            if (data.uv0.length > 0) inputarrays.push_back(createVsgArray<vsg::vec2>(data.uv0.ptr, data.uv0.length));

            cmd = vsg::BindVertexBuffers::create(0, inputarrays);
            _bindVertexBuffersCache[idstr] = cmd;
        }

        addCommandToHead(cmd);
    }

    void addDrawIndexedCommand(unity2vsg::DrawIndexedData data)
    {
        std::string idstr = std::string(data.id);

        vsg::ref_ptr<vsg::Command> cmd;

        if (_drawIndexedCache.find(idstr) != _drawIndexedCache.end())
        {
            cmd = _drawIndexedCache[idstr];
        }
        else
        {
            cmd = vsg::DrawIndexed::create(data.indexCount, data.instanceCount, data.firstIndex, data.vertexOffset, data.firstInstance);
            _drawIndexedCache[idstr] = cmd;
        }
        addCommandToHead(cmd);
    }

    void createBindDescriptorSetCommand(bool addToStateGroup)
    {
        if (addToStateGroup && !_activeStateGroup.valid())
        {
            DebugLog("GraphBuilder Error: Can't bind descriptors no StateGroup active.");
            return;
        }

        if (!_activeGraphicsPipeline.valid())
        {
            DebugLog("GraphBuilder Error: Can't bind descriptors until a graphicspipeline has been added.");
            return;
        }
        if (_descriptors.empty())
        {
            DebugLog("GraphBuilder Error: No descriptors to bind.");
            return;
        }

        // create an id combining all the object ids for the current list of descriptors, then use it to see if a matching binddescriptorset exists in the cache
        std::string fullid = "";
        for (std::vector<std::string>::const_iterator p = _descriptorObjectIds.begin(); p != _descriptorObjectIds.end(); p++)
        {
            fullid += *p;
            if (p != _descriptorObjectIds.end() - 1)
                fullid += "-";
        }

        vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;

        if (_bindDescriptorSetCache.find(fullid) != _bindDescriptorSetCache.end())
        {
            bindDescriptorSet = _bindDescriptorSetCache[fullid];
        }
        else
        {
            auto descriptorSet = vsg::DescriptorSet::create(_activeGraphicsPipeline->getPipelineLayout()->getDescriptorSetLayouts(), _descriptors);
            bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_GRAPHICS, _activeGraphicsPipeline->getPipelineLayout(), 0, descriptorSet);
            _bindDescriptorSetCache[fullid] = bindDescriptorSet;
        }

        if (addToStateGroup)
        {
            if (!addStateCommandToActiveStateGroup(bindDescriptorSet))
            {
                DebugLog("GraphBuilder Error: No Active StateGroup");
            }
        }
        else
        {
            if (!addCommandToHead(bindDescriptorSet))
            {
                DebugLog("GraphBuilder Error: Current head is not a Commands node");
            }
        }

        _descriptors.clear();
        _descriptorObjectIds.clear();
    }

    //
    // Descriptors
    //

    vsg::ref_ptr<vsg::Texture> createTexture(const TextureData& data, bool useCache = true)
    {
        vsg::ref_ptr<vsg::Texture> texture;

        std::string idstr = std::string(data.id);

        // has a texture with this ID already been created
        if (useCache && _textureCache.find(idstr) != _textureCache.end())
        {
            texture = _textureCache[idstr];
        }
        else
        {
            texture = vsg::Texture::create();

            vsg::ref_ptr<vsg::Data> texdata;
            VkFormat format = data.format;
            VkFormatSizeInfo sizeInfo = GetSizeInfoForFormat(data.format);
            sizeInfo.layout.maxNumMipmaps = data.mipmapCount;
            uint32_t blockVolume = sizeInfo.layout.blockWidth * sizeInfo.layout.blockHeight * sizeInfo.layout.blockDepth;

            if (data.depth == 1)
            {
                if (blockVolume == 1)
                {
                    switch (format)
                    {
                    //
                    // uint8 formats

                    // 1 component
                    case VK_FORMAT_R8_UNORM:
                    case VK_FORMAT_R8_SRGB:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubyteArray2D(data.width, data.height, data.pixels.ptr));
                        break;
                    }
                    // 2 component
                    case VK_FORMAT_R8G8_UNORM:
                    case VK_FORMAT_R8G8_SRGB:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubvec2Array2D(data.width, data.height, reinterpret_cast<vsg::ubvec2*>(data.pixels.ptr)));
                        break;
                    }
                    // 3 component
                    case VK_FORMAT_B8G8R8_UNORM:
                    case VK_FORMAT_B8G8R8_SRGB:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubvec3Array2D(data.width, data.height, reinterpret_cast<vsg::ubvec3*>(data.pixels.ptr)));
                        break;
                    }
                    // 4 component
                    case VK_FORMAT_R8G8B8A8_UNORM:
                    case VK_FORMAT_R8G8B8A8_SRGB:
                    case VK_FORMAT_B8G8R8A8_UNORM:
                    case VK_FORMAT_B8G8R8A8_SRGB:
                    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubvec4Array2D(data.width, data.height, reinterpret_cast<vsg::ubvec4*>(data.pixels.ptr)));
                        break;
                    }

                    //
                    // uint16 formats

                    // 1 component
                    case VK_FORMAT_R16_UNORM:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::ushortArray2D(data.width, data.height, reinterpret_cast<uint16_t*>(data.pixels.ptr)));
                        break;
                    }
                    // 2 component
                    case VK_FORMAT_R16G16_UNORM:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::usvec2Array2D(data.width, data.height, reinterpret_cast<vsg::usvec2*>(data.pixels.ptr)));
                        break;
                    }
                    // 4 component
                    case VK_FORMAT_R16G16B16A16_UNORM:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::usvec4Array2D(data.width, data.height, reinterpret_cast<vsg::usvec4*>(data.pixels.ptr)));
                        break;
                    }

                    //
                    // uint32 formats

                    // 1 component
                    case VK_FORMAT_R32_UINT:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::uintArray2D(data.width, data.height, reinterpret_cast<uint32_t*>(data.pixels.ptr)));
                        break;
                    }
                    // 2 component
                    case VK_FORMAT_R32G32_UINT:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::uivec2Array2D(data.width, data.height, reinterpret_cast<vsg::uivec2*>(data.pixels.ptr)));
                        break;
                    }
                    // 4 component
                    case VK_FORMAT_R32G32B32A32_UINT:
                    {
                        texdata = vsg::ref_ptr<vsg::Data>(new vsg::uivec4Array2D(data.width, data.height, reinterpret_cast<vsg::uivec4*>(data.pixels.ptr)));
                        break;
                    }

                    default: break;
                    }
                }
                else
                {
                    uint32_t width = data.width / sizeInfo.layout.blockWidth;
                    uint32_t height = data.height / sizeInfo.layout.blockHeight;
                    uint32_t depth = data.depth / sizeInfo.layout.blockDepth;

                    if (sizeInfo.blockSize == 64)
                    {
                        texdata = new vsg::block64Array2D(width, height, reinterpret_cast<vsg::block64*>(data.pixels.ptr));
                    }
                    else if (sizeInfo.blockSize == 128)
                    {
                        texdata = new vsg::block128Array2D(width, height, reinterpret_cast<vsg::block128*>(data.pixels.ptr));
                    }
                }
            }
            else if (data.depth > 1) // 3d textures
            {
                switch (format)
                {
                case VK_FORMAT_R8_UNORM:
                {
                    texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubyteArray3D(data.width, data.height, data.depth, data.pixels.ptr));
                    break;
                }
                case VK_FORMAT_R8G8_UNORM:
                {
                    texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubvec2Array3D(data.width, data.height, data.depth, reinterpret_cast<vsg::ubvec2*>(data.pixels.ptr)));
                    break;
                }
                case VK_FORMAT_R8G8B8A8_UNORM:
                {
                    texdata = vsg::ref_ptr<vsg::Data>(new vsg::ubvec4Array3D(data.width, data.height, data.depth, reinterpret_cast<vsg::ubvec4*>(data.pixels.ptr)));
                    break;
                }
                default: break;
                }
            }

            if (!texdata.valid())
            {
                DebugLog("GraphBuilder Error: Unable to handle texture format");
                return vsg::ref_ptr<vsg::Texture>();
            }

            texdata->setFormat(format);
            texdata->setLayout(sizeInfo.layout);

            texture->_textureData = texdata;
            texture->_samplerInfo = vkSamplerCreateInfoForTextureData(data);

            if (useCache) _textureCache[idstr] = texture;
        }

        texture->_dstBinding = data.channel;
        return texture;
    }

    void addTexture(const TextureData& data)
    {
        auto texture = createTexture(data);
        _descriptors.push_back(texture);
        _descriptorObjectIds.push_back(std::string(data.id));
    }

    /*void addTextureArray(unity2vsg::TextureDataArray data)
    {
        if (data.length == 0) return;

        vsg::ref_ptr<vsg::TextureArray> textureArray = vsg::TextureArray::create();
        std::string idstr = "";

        for (uint32_t i = 0; i < data.length; i++)
        {
            textureArray->_textures.push_back(createTexture(data.ptr[i], false));
            idstr += data.ptr[i].id;
        }

        textureArray->_dstBinding = data.ptr[0].channel;

        _descriptors.push_back(textureArray);
        _descriptorObjectIds.push_back(std::string(idstr));
    }*/

    void addTextureToArray(const TextureData& data)
    {
        auto texture = createTexture(data);
        _textureArray.push_back(texture);
    }

    void createTextureArray(unity2vsg::TextureDataArray data)
    {
        vsg::ref_ptr<vsg::TextureArray> textureArray = vsg::TextureArray::create();

        for (auto& tex : _textureArray)
        {
            textureArray->_textures.push_back(tex);
        }

        textureArray->_dstBinding = data.channel;

        _descriptors.push_back(textureArray);
        _descriptorObjectIds.push_back(std::string(data.id));

        _textureArray.clear();
    }

    //
    // Helpers
    //

    vsg::Node* getHead()
    {
        if (_nodeStack.size() == 0) return nullptr;
        return _nodeStack[_nodeStack.size() - 1];
    }

    vsg::Group* getHeadAsGroup()
    {
        if (_nodeStack.size() == 0) return nullptr;
        return dynamic_cast<vsg::Group*>(_nodeStack[_nodeStack.size() - 1].get());
    }

    vsg::LOD* getHeadAsLOD()
    {
        if (_nodeStack.size() == 0) return nullptr;
        return dynamic_cast<vsg::LOD*>(_nodeStack[_nodeStack.size() - 1].get());
    }

    vsg::StateGroup* getHeadAsStateGroup()
    {
        if (_nodeStack.size() == 0) return nullptr;
        return dynamic_cast<vsg::StateGroup*>(_nodeStack[_nodeStack.size() - 1].get());
    }

    vsg::Commands* getHeadAsCommandsNode()
    {
        if (_nodeStack.size() == 0) return nullptr;
        return dynamic_cast<vsg::Commands*>(_nodeStack[_nodeStack.size() - 1].get());
    }

    bool addChildToHead(vsg::ref_ptr<vsg::Node> node)
    {
        vsg::Group* headGroup = getHeadAsGroup();
        if (headGroup != nullptr)
        {
            headGroup->addChild(node);
            return true;
        }
        return false;
    }

    bool addLODChildToHead(vsg::ref_ptr<vsg::Node> node, LODChildData lodData)
    {
        vsg::LOD* headLOD = getHeadAsLOD();
        if (headLOD != nullptr)
        {
            vsg::LOD::LODChild lod;
            lod.child = node;
            lod.minimumScreenHeightRatio = lodData.minimumScreenHeightRatio;
            headLOD->addChild(lod);
            return true;
        }
        return false;
    }

    bool addCommandToHead(vsg::ref_ptr<vsg::Command> command)
    {
        vsg::Commands* headCommands = getHeadAsCommandsNode();
        if (headCommands != nullptr)
        {
            headCommands->addChild(command);
            return true;
        }
        return false;
    }

    bool addStateCommandToActiveStateGroup(vsg::ref_ptr<vsg::StateCommand> command)
    {
        if (_activeStateGroup != nullptr)
        {
            _activeStateGroup->add(command);
            return true;
        }
        return false;
    }

    void pushNodeToStack(vsg::ref_ptr<vsg::Node> node)
    {
        _nodeStack.push_back(node);
    }

    void popNodeFromStack()
    {
        _nodeStack.pop_back();
    }

    PipelineData createPipelineDataFromMeshData(const MeshData& mesh)
    {
        PipelineData data;
        data.hasNormals = mesh.normals.length > 0;
        data.hasColors = mesh.colors.length > 0;
        data.uvChannelCount = mesh.uv0.length > 0 ? 1 : 0;
        return data;
    }

    void writeFile(std::string fileName)
    {
        LeafDataCollection leafDataCollection;
        _root->accept(leafDataCollection);
        _root->setObject("batch", leafDataCollection.objects);

        vsg::vsgReaderWriter io;
        io.writeFile(_root.get(), fileName);
    }

    void releaseObjects()
    {
        LeafDataRelease releaser;
        _root->accept(releaser);
    }

    vsg::ref_ptr<vsg::MatrixTransform> _root;

    // the stack of nodes added, last node is the current head being acted on
    std::vector<vsg::ref_ptr<vsg::Node>> _nodeStack;

    // the current active stategroup
    vsg::ref_ptr<vsg::StateGroup> _activeStateGroup;

    // the current active graphics pipelines
    vsg::ref_ptr<vsg::GraphicsPipeline> _activeGraphicsPipeline;

    // the current set of descriptors being built
    vsg::Descriptors _descriptors;

    // the current array of textures build when we call create texture array
    std::vector<vsg::ref_ptr<vsg::Texture>> _textureArray;

    // the unique ids of the of the descriptos list being built
    std::vector<std::string> _descriptorObjectIds;

    // map of exisitng Geometryies/VertexIndexedDraws to the MeshData ID they represent
    std::map<std::string, vsg::ref_ptr<vsg::Node>> _geometryCache;

    std::map<std::string, vsg::ref_ptr<vsg::Command>> _bindVertexBuffersCache;
    std::map<std::string, vsg::ref_ptr<vsg::Command>> _bindIndexBufferCache;
    std::map<std::string, vsg::ref_ptr<vsg::Command>> _drawIndexedCache;

    // map of shader modules to the masks used to create them
    std::map<std::string, vsg::ref_ptr<vsg::ShaderModule>> _shaderModulesCache;

    // map of textures to the TextureData ID they represent
    std::map<std::string, vsg::ref_ptr<vsg::Texture>> _textureCache;

    // map of bind descriptor set to IDs
    std::map<std::string, vsg::ref_ptr<vsg::BindDescriptorSet>> _bindDescriptorSetCache;

    // map of bind graphics piplelines to IDs
    std::map<std::string, vsg::ref_ptr<vsg::BindGraphicsPipeline>> _bindGraphicsPipelineCache;

    std::string _saveFileName;
};

vsg::ref_ptr<GraphBuilder> _builder;

void unity2vsg_BeginExport()
{
    if (_builder.valid())
    {
        DebugLog("GraphBuilder Error: Export already in progress.");
        return;
    }
    _builder = vsg::ref_ptr<GraphBuilder>(new GraphBuilder());
}

void unity2vsg_EndExport(const char* saveFileName)
{
    _builder->writeFile(std::string(saveFileName));

    _builder->releaseObjects();
    _builder = nullptr;
}

void unity2vsg_AddGroupNode()
{
    _builder->addGroup();
}

void unity2vsg_AddTransformNode(unity2vsg::TransformData transform)
{
    _builder->addMatrixTrasform(transform);
}

void unity2vsg_AddCullNode(unity2vsg::CullData cull)
{
    _builder->addCullNode(cull);
}

void unity2vsg_AddCullGroupNode(unity2vsg::CullData cull)
{
    _builder->addCullGroup(cull);
}

void unity2vsg_AddLODNode(unity2vsg::CullData cull)
{
    _builder->addLOD(cull);
}

void unity2vsg_AddLODChild(unity2vsg::LODChildData lodChildData)
{
    _builder->addLODChild(lodChildData);
}

void unity2vsg_AddStateGroupNode()
{
    _builder->addStateGroup();
}

void unity2vsg_AddCommandsNode()
{
    _builder->addCommands();
}

void unity2vsg_AddVertexIndexDrawNode(unity2vsg::MeshData mesh)
{
    _builder->addVertexIndexDraw(mesh);
}

void unity2vsg_AddGeometryNode(unity2vsg::MeshData mesh)
{
    _builder->addGeometry(mesh);
}

//
// Meta data
//

void unity2vsg_AddStringValue(const char* name, const char* value)
{
    _builder->addStringValue(std::string(name), std::string(value));
}

//
// Commands
//

int unity2vsg_AddBindGraphicsPipelineCommand(unity2vsg::PipelineData pipeline, uint32_t addToStateGroup)
{
    return _builder->addBindGraphicsPipelineCommand(pipeline, addToStateGroup == 1) ? 1 : 0;
}

void unity2vsg_AddBindIndexBufferCommand(unity2vsg::IndexBufferData data)
{
    _builder->addBindIndexBufferCommand(data);
}

void unity2vsg_AddBindVertexBuffersCommand(unity2vsg::VertexBuffersData data)
{
    _builder->addBindVertexBuffersCommand(data);
}

void unity2vsg_AddDrawIndexedCommand(unity2vsg::DrawIndexedData data)
{
    _builder->addDrawIndexedCommand(data);
}

void unity2vsg_CreateBindDescriptorSetCommand(uint32_t addToStateGroup)
{
    _builder->createBindDescriptorSetCommand(addToStateGroup == 1);
}

//
// Desccriptos
//

void unity2vsg_AddTextureDescriptor(unity2vsg::TextureData texture)
{
    _builder->addTexture(texture);
}

void unity2vsg_AddTextureArrayDescriptor(unity2vsg::TextureDataArray textureArray)
{
    //_builder->addTextureArray(textureArray);
}

void unity2vsg_AddTextureDescriptorToArray(unity2vsg::TextureData texture)
{
    _builder->addTextureToArray(texture);
}

void unity2vsg_CreateTextureArrayDescriptor(unity2vsg::TextureDataArray data)
{
    _builder->createTextureArray(data);
}

void unity2vsg_EndNode()
{
    _builder->popNodeFromStack();
}

void unity2vsg_LaunchViewer(const char* filename, uint32_t useCamData, unity2vsg::CameraData camdata)
{
    try
    {
        vsg::vsgReaderWriter io;
        vsg::ref_ptr<vsg::Node> vsg_scene = io.read<vsg::Node>(filename);

        std::stringstream ss;
        ss << "cam pos: " << camdata.position.x << ", " << camdata.position.y << ", " << camdata.position.z << std::endl;
        ss << "cam look: " << camdata.lookAt.x << ", " << camdata.lookAt.y << ", " << camdata.lookAt.z << std::endl;
        ss << "cam up: " << camdata.upDir.x << ", " << camdata.upDir.y << ", " << camdata.upDir.z << std::endl;
        //DebugLog(ss.str());

        if (!vsg_scene.valid()) return;

        auto windowTraits = vsg::Window::Traits::create();
        windowTraits->windowTitle = "vsg export - " + std::string(filename);
        windowTraits->width = 800;
        windowTraits->height = 600;

        // create the viewer and assign window(s) to it
        auto viewer = vsg::Viewer::create();

        vsg::ref_ptr<vsg::Window> window(vsg::Window::create(windowTraits));
        if (!window)
        {
            std::cout << "Could not create windows." << std::endl;
            _builder->releaseObjects();
            return;
        }

        viewer->addWindow(window);

        // compute the bounds of the scene graph to help position camera
        vsg::ComputeBounds computeBounds;
        vsg_scene->accept(computeBounds);
        vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
        double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
        double nearFarRatio = 0.0001;

        // set up the camera
        vsg::ref_ptr<vsg::Perspective> perspective;
        vsg::ref_ptr<vsg::LookAt> lookAt;

        if (useCamData == 1)
        {
            perspective = vsg::ref_ptr<vsg::Perspective>(new vsg::Perspective(camdata.fov, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), camdata.nearZ, camdata.farZ));
            lookAt = vsg::ref_ptr<vsg::LookAt>(new vsg::LookAt(vsg::dvec3(camdata.position.x, camdata.position.y, camdata.position.z), vsg::dvec3(camdata.lookAt.x, camdata.lookAt.y, camdata.lookAt.z), vsg::dvec3(camdata.upDir.x, camdata.upDir.y, camdata.upDir.z)));
        }
        else
        {
            perspective = vsg::ref_ptr<vsg::Perspective>(new vsg::Perspective(30.0, static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height), nearFarRatio * radius, radius * 4.5));
            lookAt = vsg::ref_ptr<vsg::LookAt>(new vsg::LookAt(centre + vsg::dvec3(0.0, 0.0, -radius * 3.5), centre, vsg::dvec3(0.0, 1.0, 0.0)));
        }

        vsg::ref_ptr<vsg::Camera> camera(new vsg::Camera(perspective, lookAt, vsg::ViewportState::create(window->extent2D())));

        // add a GraphicsStage tp the Window to do dispatch of the command graph to the commnad buffer(s)
        window->addStage(vsg::GraphicsStage::create(vsg_scene, camera));

        // compile the Vulkan objects
        viewer->compile();

        // add close handler to respond the close window button and pressing esape
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));

        // rendering main loop
        while (viewer->advanceToNextFrame())
        {
            // pass any events into EventHandlers assigned to the Viewer
            viewer->handleEvents();

            viewer->populateNextFrame();

            viewer->submitNextFrame();
        }
    }
    catch (...)
    {
        DebugLog("An exception occured during vsg preview.");
        return;
    }
}
