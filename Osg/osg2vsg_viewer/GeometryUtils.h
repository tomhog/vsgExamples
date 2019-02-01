#pragma once

#include <vsg/all.h>
#include "GraphicsNodes.h"

#include <osg/NodeVisitor>

namespace osg2vsg
{
    extern vsg::vec2Array* convert(osg::Vec2Array* inarray);

    extern vsg::vec3Array* convert(osg::Vec3Array* inarray);

    extern vsg::vec4Array* convert(osg::Vec4Array* inarray);

    extern vsg::Data* convert(osg::Array* inarray);

    extern vsg::ref_ptr<vsg::GraphicsPipelineGroup> createGraphicsPipeline(vsg::Paths& searchPaths);
}

class GraphBuilderVisitor : public osg::NodeVisitor
{
public:
    GraphBuilderVisitor(vsg::Paths& searchPaths);
        
    void apply(osg::Group& group);
    void apply(osg::Transform& transform);
    void apply(osg::Geometry& geometry);
    

    vsg::ref_ptr<vsg::Node> get() { return _root; }

protected:

    vsg::ref_ptr<vsg::Group> _root;

    vsg::Paths _searchPaths;
};