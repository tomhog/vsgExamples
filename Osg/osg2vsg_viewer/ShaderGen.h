#pragma once

#include <fstream>
#include <regex>

#include <osg/NodeVisitor>
#include <osg/State>
#include <osg/LightSource>
#include <osg/Texture2D>

#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

class ShaderGenCache : public osg::Referenced
{
public:
    enum StateMask
    {
        NONE = 0,
        LIGHTING = 1,
        DIFFUSE_MAP = 2, //< Texture in unit 0
        NORMAL_MAP = 4  //< Texture in unit 1 and tangent vector array in index 6
    };

    // taken from osg fbx plugin
    enum TextureUnit
    {
        DIFFUSE_TEXTURE_UNIT = 0,
        OPACITY_TEXTURE_UNIT,
        REFLECTION_TEXTURE_UNIT,
        EMISSIVE_TEXTURE_UNIT,
        AMBIENT_TEXTURE_UNIT,
        NORMAL_TEXTURE_UNIT,
        SPECULAR_TEXTURE_UNIT,
        SHININESS_TEXTURE_UNIT
    };
        
    typedef std::map<int, osg::ref_ptr<osg::StateSet> > StateSetMap;
        
    ShaderGenCache() {};
        
    void setStateSet(int stateMask, osg::StateSet* program);
    osg::StateSet* getStateSet(int stateMask) const;
    osg::StateSet* getOrCreateStateSet(int stateMask);

    static osg::Program* createProgram(int stateMask);
    static std::string getNameForShader(int stateMask);

    static void convertAndOutputSpirvCompatibleShaders(osg::Program* program, const std::string& directory);

    static std::string replace(const std::string& sourcestr, const std::string& findstr, const std::string& replacestr)
    {
        return std::regex_replace(sourcestr, std::regex(findstr), replacestr);
    }
        
protected:
    osg::StateSet* createStateSet(int stateMask) const;
    mutable OpenThreads::Mutex _mutex;
    StateSetMap _stateSetMap;
        
};

class ShaderGenVisitor : public osg::NodeVisitor
{
public:
    ShaderGenVisitor();
    ShaderGenVisitor(ShaderGenCache *stateCache);
        
    void setStateCache(ShaderGenCache* stateCache) { _stateCache = stateCache; }
    ShaderGenCache* getStateCache() const { return _stateCache.get(); }
        
    void setRootStateSet(osg::StateSet* stateSet);
    osg::StateSet* getRootStateSet() const { return _rootStateSet.get(); }

    osg::LightSource* getLight() const { return _light.get(); }

    // convienience function to set the osg_LightSource uniform values
    static void updateLightUniforms(osg::StateSet* stateset, osg::LightSource* light);
        
    void apply(osg::Node& node);
    void apply(osg::Geode& geode);
    void apply(osg::LightSource& light);
        
    void reset();
        
protected:
    void update(osg::Drawable* drawable);
        
    osg::ref_ptr<ShaderGenCache> _stateCache;
    osg::ref_ptr<osg::State> _state;
    osg::ref_ptr<osg::StateSet> _rootStateSet;

    osg::ref_ptr<osg::LightSource> _light;
};

class SetImageDirectoryVisitor : public osg::NodeVisitor
{
public:
    SetImageDirectoryVisitor(std::string directory) : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN), _directory(directory) { }

    virtual void apply(osg::Node& node)
    {
        osg::ref_ptr<osg::StateSet> ss = node.getStateSet();
        if (ss.valid())
        {
            osg::StateSet::AttributeList& attrs = ss->getAttributeList();
            osg::StateSet::AttributeList::iterator itr = attrs.begin();
            for (; itr != attrs.end(); ++itr)
            {
                const osg::StateSet::RefAttributePair& attrp = itr->second;
                const osg::ref_ptr<osg::StateAttribute>& attr = attrp.first;

                osg::ref_ptr<osg::Texture2D> texture = dynamic_cast<osg::Texture2D*>(attr.get());
                if (texture != nullptr && texture->getImage() != nullptr)
                {
                    auto fullpath = texture->getImage()->getFileName();
                    auto filename = osgDB::getSimpleFileName(fullpath);

                    texture->getImage()->setFileName(osgDB::concatPaths(_directory, filename));
                }
            }
        }

        traverse(node);
    }

protected:
    std::string _directory;
};

class OutputSPIRCompatibleShadersVisitor : public osg::NodeVisitor
{
public:
    OutputSPIRCompatibleShadersVisitor(std::string directory) : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN), _directory(directory) { }

    virtual void apply(osg::Node& node)
    {
        osg::ref_ptr<osg::StateSet> ss = node.getStateSet();
        if (ss.valid())
        {
            osg::Program* program =  dynamic_cast<osg::Program*>(ss->getAttribute(osg::StateAttribute::PROGRAM));
            ShaderGenCache::convertAndOutputSpirvCompatibleShaders(program, _directory);
        }

        traverse(node);
    }
protected:
    std::string _directory;
};
    