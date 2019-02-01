#include "GeometryUtils.h"

#include "ShaderGen.h"
#include "GraphicsNodes.h"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Material>

namespace osg2vsg
{

    vsg::vec2Array* convert(osg::Vec2Array* inarray)
    {
        vsg::vec2Array* outarray(new vsg::vec2Array(inarray->size()));
        for (auto i = 0; i < inarray->size(); i++)
        {
            osg::Vec2 osg2 = inarray->at(i);
            vsg::vec2 vsg2(osg2.x(), osg2.y());
            outarray->set(i, vsg2);
        }
        return outarray;
    }

    vsg::vec3Array* convert(osg::Vec3Array* inarray)
    {
        vsg::vec3Array* outarray(new vsg::vec3Array(inarray->size()));
        for (auto i = 0; i < inarray->size(); i++)
        {
            osg::Vec3 osg3 = inarray->at(i);
            vsg::vec3 vsg3(osg3.x(), osg3.y(), osg3.z());
            outarray->set(i, vsg3);
        }
        return outarray;
    }

    vsg::vec4Array* convert(osg::Vec4Array* inarray)
    {
        vsg::vec4Array* outarray(new vsg::vec4Array(inarray->size()));
        for (auto i = 0; i < inarray->size(); i++)
        {
            osg::Vec4 osg4 = inarray->at(i);
            vsg::vec4 vsg4(osg4.x(), osg4.y(), osg4.z(), osg4.w());
            outarray->set(i, vsg4);
        }
        return outarray;
    }

    vsg::Data* convert(osg::Array* inarray)
    {
        switch (inarray->getType())
        {
        case osg::Array::Type::Vec2ArrayType: return convert(dynamic_cast<osg::Vec2Array*>(inarray));
        case osg::Array::Type::Vec3ArrayType: return convert(dynamic_cast<osg::Vec3Array*>(inarray));
        case osg::Array::Type::Vec4ArrayType: return convert(dynamic_cast<osg::Vec4Array*>(inarray));
        default: return nullptr;
        }
    }

    vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGraphicsPipeline(vsg::Paths& searchPaths)
    {
        //
        // load shaders
        //
        vsg::ref_ptr<vsg::Shader> vertexShader = vsg::Shader::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vsg::findFile("shaders/vert_PushConstants.spv", searchPaths));
        vsg::ref_ptr<vsg::Shader> fragmentShader = vsg::Shader::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", vsg::findFile("shaders/frag_PushConstants.spv", searchPaths));
        if (!vertexShader || !fragmentShader)
        {
            std::cout << "Could not create shaders." << std::endl;
            return vsg::ref_ptr<vsg::GraphicsPipelineGroup>();
        }

        //
        // set up graphics pipeline
        //
        vsg::ref_ptr<vsg::GraphicsPipelineGroup> gp = vsg::GraphicsPipelineGroup::create();

        gp->shaders = vsg::GraphicsPipelineGroup::Shaders{ vertexShader, fragmentShader };
        gp->maxSets = 1;
        gp->descriptorPoolSizes = vsg::DescriptorPoolSizes
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1} // texture
        };

        gp->descriptorSetLayoutBindings = vsg::DescriptorSetLayoutBindings
        {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr} // { binding, descriptorTpe, descriptorCount, stageFlags, pImmutableSamplers}
        };

        gp->pushConstantRanges = vsg::PushConstantRanges
        {
            {VK_SHADER_STAGE_VERTEX_BIT, 0, 196} // projection view, and model matrices
        };

        gp->vertexBindingsDescriptions = vsg::VertexInputState::Bindings
        {
            VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
            VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // colour data
            VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // tex coord data
        };

        gp->vertexAttributeDescriptions = vsg::VertexInputState::Attributes
        {
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
            VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // colour data
            VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},    // tex coord data
        };

        gp->pipelineStates = vsg::GraphicsPipelineStates
        {
            vsg::InputAssemblyState::create(),
            vsg::RasterizationState::create(),
            vsg::MultisampleState::create(),
            vsg::ColorBlendState::create(),
            vsg::DepthStencilState::create()
        };

        return gp;
    }

}

GraphBuilderVisitor::GraphBuilderVisitor(vsg::Paths& searchPaths) :
    NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _searchPaths(searchPaths)
{
    _root = vsg::Group::create();
}

void GraphBuilderVisitor::apply(osg::Group& group)
{
    traverse(group);
}

void GraphBuilderVisitor::apply(osg::Transform& transform)
{
    traverse(transform);
}

void GraphBuilderVisitor::apply(osg::Geometry& osgGeometry)
{    
    vsg::ref_ptr<vsg::Data> vertices(osg2vsg::convert(osgGeometry.getVertexArray()));
    //vsg::ref_ptr<vsg::Data> normals(osg2vsg::convert(osgGeometry.getNormalArray()));
    vsg::ref_ptr<vsg::Data> texcoords(osg2vsg::convert(osgGeometry.getTexCoordArray(0)));

    osg::Geometry::DrawElementsList drawElementsList;
    osgGeometry.getDrawElementsList(drawElementsList);

    // only support first for now
    if(drawElementsList.size() == 0) return;

    osg::DrawElements* osgindices = drawElementsList.at(0);
    auto numindcies = osgindices->getNumIndices();

    vsg::ref_ptr<vsg::ushortArray> indices(new vsg::ushortArray(numindcies));
    for (auto i = 0; i < numindcies; i++)
    {
        indices->set(i, osgindices->index(i));
    }

    //
    // create the vsg graph

    //
    // set up graphics pipeline
    //
    vsg::ref_ptr<vsg::GraphicsPipelineGroup> gp = osg2vsg::createGraphicsPipeline(_searchPaths);


    //
    // set up model transformation node
    //
    auto transform = vsg::MatrixTransform::create(); // VK_SHADER_STAGE_VERTEX_BIT, 128

    // add transform to graphics pipeline group
    gp->addChild(transform);


    //
    // create texture node
    //
    //
    std::string textureFile("textures/lz.vsgb");
    vsg::vsgReaderWriter vsgReader;
    auto textureData = vsgReader.read<vsg::Data>(vsg::findFile(textureFile, _searchPaths));
    if (!textureData)
    {
        std::cout << "Could not read texture file : " << textureFile << std::endl;
        return;
    }
    vsg::ref_ptr<vsg::Texture> texture = vsg::Texture::create();
    texture->_textureData = textureData;

    // add texture node to transform node
    transform->addChild(texture);


    //
    // set up vertex and index arrays
    //

   /* vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array
        {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
        }); // VK_FORMAT_R32G32B32_SFLOAT, VK_VERTEX_INPUT_RATE_VERTEX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE
       */
    vsg::ref_ptr<vsg::vec3Array> colors(new vsg::vec3Array(vertices->valueCount()));
    for (auto i = 0; i < vertices->valueCount(); i++)
    {
        colors->set(i, vsg::vec3(1.0f,1.0f,1.0f));
    }

    auto geometry = vsg::Geometry::create();

    // setup geometry
    geometry->_arrays = vsg::DataList{ vertices, colors, texcoords };
    geometry->_indices = indices;

    vsg::ref_ptr<vsg::DrawIndexed> drawIndexed = vsg::DrawIndexed::create(indices->valueCount(), 1, 0, 0, 0);
    geometry->_commands = vsg::Geometry::Commands{ drawIndexed };

    // add geometry to texture group
    texture->addChild(geometry);

    _root->addChild(gp);

}


