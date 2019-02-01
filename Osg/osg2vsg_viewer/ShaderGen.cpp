#include "ShaderGen.h"

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Fog>
#include <osg/Material>
#include <sstream>


/// State extended by mode/attribute accessors
class StateEx : public osg::State
{
public:
    StateEx() : State() {}
        
    osg::StateAttribute::GLModeValue getMode(osg::StateAttribute::GLMode mode,
                                                osg::StateAttribute::GLModeValue def = osg::StateAttribute::INHERIT) const
    {
        return getMode(_modeMap, mode, def);
    }
        
    osg::StateAttribute* getAttribute(osg::StateAttribute::Type type, unsigned int member = 0) const
    {
        return getAttribute(_attributeMap, type, member);
    }
        
    osg::StateAttribute::GLModeValue getTextureMode(unsigned int unit,
                                                    osg::StateAttribute::GLMode mode,
                                                    osg::StateAttribute::GLModeValue def = osg::StateAttribute::INHERIT) const
    {
        return unit < _textureModeMapList.size() ? getMode(_textureModeMapList[unit], mode, def) : def;
    }
        
    osg::StateAttribute* getTextureAttribute(unsigned int unit, osg::StateAttribute::Type type) const
    {
        return unit < _textureAttributeMapList.size() ? getAttribute(_textureAttributeMapList[unit], type, 0) : 0;
    }
        
    osg::UniformBase* getUniform(const std::string& name) const
    {
        UniformMap::const_iterator it = _uniformMap.find(name);
        return it != _uniformMap.end() ? 
        const_cast<osg::UniformBase *>(it->second.uniformVec.back().first) : 0;
    }
        
protected:
        
    osg::StateAttribute::GLModeValue getMode(const ModeMap& modeMap,
                                                osg::StateAttribute::GLMode mode, 
                                                osg::StateAttribute::GLModeValue def = osg::StateAttribute::INHERIT) const
    {
        ModeMap::const_iterator it = modeMap.find(mode);
        return (it != modeMap.end() && it->second.valueVec.size()) ? it->second.valueVec.back() : def;
    }
        
    osg::StateAttribute* getAttribute(const AttributeMap& attributeMap,
                                        osg::StateAttribute::Type type, unsigned int member = 0) const
    {
        AttributeMap::const_iterator it = attributeMap.find(std::make_pair(type, member));
        return (it != attributeMap.end() && it->second.attributeVec.size()) ? const_cast<osg::StateAttribute*>(it->second.attributeVec.back().first) : 0;
    }
};

//
// ShaderGenCache
//

void ShaderGenCache::setStateSet(int stateMask, osg::StateSet* stateSet)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
    _stateSetMap[stateMask] = stateSet;
}

osg::StateSet* ShaderGenCache::getStateSet(int stateMask) const
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
    StateSetMap::const_iterator it = _stateSetMap.find(stateMask);
    return (it != _stateSetMap.end()) ? it->second.get() : 0;
}

osg::StateSet* ShaderGenCache::getOrCreateStateSet(int stateMask)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
    StateSetMap::iterator it = _stateSetMap.find(stateMask);
    if (it == _stateSetMap.end())
    {
        osg::StateSet *stateSet = createStateSet(stateMask);
        _stateSetMap.insert(it, StateSetMap::value_type(stateMask, stateSet));
        return stateSet;
    }
    return it->second.get();
}


osg::StateSet* ShaderGenCache::createStateSet(int stateMask) const
{
    osg::StateSet *stateSet = new osg::StateSet;
    osg::Program *program = createProgram(stateMask);
    stateSet->setAttribute(program);

    bool usediffusemap = stateMask & (DIFFUSE_MAP);
    bool usenormalmap = stateMask & (NORMAL_MAP);

    if (usediffusemap)
    {
        osg::Uniform* diffuseMap = new osg::Uniform("diffuseMap", DIFFUSE_TEXTURE_UNIT);
        stateSet->addUniform(diffuseMap);
    }

    if (usenormalmap)
    {
        osg::Uniform *normalMap = new osg::Uniform("normalMap", NORMAL_TEXTURE_UNIT);
        stateSet->addUniform(normalMap);
    }

    return stateSet;
}

osg::Program* ShaderGenCache::createProgram(int stateMask)
{
    osg::Program *program = new osg::Program;

    bool usenormal = stateMask & (LIGHTING | NORMAL_MAP);
    bool usetex0 = stateMask & (DIFFUSE_MAP | NORMAL_MAP);
    bool uselighting = stateMask & (LIGHTING);
    bool usediffusemap = stateMask & (DIFFUSE_MAP);
    bool usenormalmap = stateMask & (NORMAL_MAP);

    std::ostringstream vertuniforms;
    std::ostringstream vertinputs;
    std::ostringstream vertoutputs;

    std::ostringstream fraguniforms;
    std::ostringstream fraginputs;
    std::ostringstream fragoutputs;
    
    int vertuniformindex = 0;

    // add vert uniforms
    vertuniforms << "uniform mat4 osg_ModelViewProjectionMatrix;\n";
    if(uselighting) vertuniforms << "uniform mat4 osg_ModelViewMatrix;\n";
    if(usenormal) vertuniforms << "uniform mat3 osg_NormalMatrix;\n";

    // add vert inputs

    vertinputs << "layout(location = 0) in vec3 osg_Vertex;\n";

    if (usenormal) vertinputs << "layout(location = 1) in vec3 osg_Normal;\n";

    if (usetex0) vertinputs << "layout(location = 3) in vec2 osg_MultiTexCoord0;\n";

    if (usenormalmap)
    {
        program->addBindAttribLocation("tangent", 6);
        vertinputs << "layout(location = 6) in vec3 tangent;\n";
    }

    // vert outputs frag inputs

    if (usetex0)
    {
        vertoutputs << "layout(location = 0) out vec2 texCoord0;\n";
        fraginputs << "layout(location = 0) in vec2 texCoord0;\n";
    }
    
    // normal dir is only passed for basic lighting, normal mapping we convert our viewdir to tangent space
    if (uselighting && !usenormalmap)
    {
        vertoutputs << "layout(location = 1) out vec3 normalDir;\n";
        fraginputs << "layout(location = 1) in vec3 normalDir;\n";
    }
    
    if (uselighting || usenormalmap)
    {
        vertoutputs << "layout(location = 2) out vec3 viewDir;\n";
        fraginputs << "layout(location = 2) in vec3 viewDir;\n";

        vertoutputs << "layout(location = 3) out vec3 lightDir;\n";
        fraginputs << "layout(location = 3) in vec3 lightDir;\n";

        std::ostringstream lightuniform;
        lightuniform << "struct osg_LightSourceParameters {"
            << "    vec4  position;"
            << "};\n"
            << "uniform osg_LightSourceParameters osg_LightSource;\n";

        vertuniforms << lightuniform.str();
        fraguniforms << lightuniform.str();
    }
    
    
    // frag uniforms

    if (uselighting || usenormalmap)
    {
        fraguniforms << "struct osgMaterial{\n"\
            "  vec4 ambient;\n"\
            "  vec4 diffuse;\n"\
            "  vec4 specular;\n"\
            "  float shine;\n"\
            "};\n"\
            "uniform osgMaterial osg_Material;\n";
    }
    
    if (usediffusemap)
    {
        fraguniforms << "layout(binding = 0) uniform sampler2D diffuseMap;\n";
    }
    
    if (usenormalmap)
    {
        fraguniforms << "layout(binding = 1) uniform sampler2D normalMap;\n";
    }

    // frag outputs
    fragoutputs << "layout(location = 0) out vec4 outColor;\n";

    // Vertex shader code
    std::string header = "#version 450\n" \
                        "#extension GL_ARB_separate_shader_objects : enable\n";

    std::ostringstream vert;

    vert << header;

    vert << vertuniforms.str();
    vert << vertinputs.str();
    vert << vertoutputs.str();
    
    vert << "\n"\
    "void main()\n"\
    "{\n"\
    "  gl_Position = osg_ModelViewProjectionMatrix * vec4(osg_Vertex, 1.0);\n";
    
    if (usetex0)
    {
        vert << "  texCoord0 = osg_MultiTexCoord0.st;\n";
    }
    
    //
    
    if (usenormalmap)
    {
        vert << 
        "  vec3 n = osg_NormalMatrix * osg_Normal;\n"\
        "  vec3 t = osg_NormalMatrix * tangent;\n"\
        "  vec3 b = cross(n, t);\n"\
        "  vec3 dir = -vec3(osg_ModelViewMatrix * vec4(osg_Vertex, 1.0));\n"\
        "  viewDir.x = dot(dir, t);\n"\
        "  viewDir.y = dot(dir, b);\n"\
        "  viewDir.z = dot(dir, n);\n";

        vert << "  vec4 lpos = osg_LightSource.position;\n";
        vert << 
        "  if (lpos.w == 0.0)\n"\
        "    dir = lpos.xyz;\n"\
        "  else\n"\
        "    dir += lpos.xyz;\n"\
        "  lightDir.x = dot(dir, t);\n"\
        "  lightDir.y = dot(dir, b);\n"\
        "  lightDir.z = dot(dir, n);\n";
    }
    else if (uselighting)
    {
        vert << 
        "  normalDir = osg_NormalMatrix * osg_Normal;\n"\
        "  vec3 dir = -vec3(osg_ModelViewMatrix * vec4(osg_Vertex, 1.0));\n"\
        "  viewDir = dir;\n";

        vert << "  vec4 lpos = osg_LightSource.position;\n";
        vert <<
        "  if (lpos.w == 0.0)\n"\
        "    lightDir = lpos.xyz;\n"\
        "  else\n"\
        "    lightDir = lpos.xyz + dir;\n";
    }
    
    vert << "}\n";
    
    //Fragment shader code

    std::ostringstream frag;

    frag << header;

    frag << fraguniforms.str();
    frag << fraginputs.str();
    frag << fragoutputs.str();

    frag << "\n"\
    "void main()\n"\
    "{\n";
    
    if (usediffusemap)
    {
        frag << "  vec4 base = texture(diffuseMap, texCoord0.st);\n";
    }
    else
    {
        frag << "  vec4 base = vec4(1.0);\n";
    }
    
    if (usenormalmap)
    {
        frag << "  vec3 normalDir = texture(normalMap, texCoord0.st).xyz*2.0-1.0;\n";        
        //frag << " normalDir.g = -normalDir.g;\n";
    }
    
    if (uselighting || usenormalmap)
    {
        frag << 
        "  vec3 nd = normalize(normalDir);\n"\
        "  vec3 ld = normalize(lightDir);\n"\
        "  vec3 vd = normalize(viewDir);\n"\
        "  vec4 color = vec4(0.01,0.01,0.01,1.0);\n"\
        "  color += osg_Material.ambient;\n"\
        "  float diff = max(dot(ld, nd), 0.0);\n"\
        "  color += osg_Material.diffuse * diff;\n"\
        "  color *= base;\n"\
        "  if (diff > 0.0)\n"\
        "  {\n"\
        "    vec3 halfDir = normalize(ld+vd);\n"\
        "    color.rgb += base.a * osg_Material.specular.rgb * \n"\
        "      pow(max(dot(halfDir, nd), 0.0), osg_Material.shine);\n"\
        "  }\n";       
    }
    else
    {
        frag << "  vec4 color = base;\n";
    }
    
    frag << "  outColor = color;\n";
    frag << "}\n";
    
    auto vertstr = vert.str();
    auto fragstr = frag.str();
    
    OSG_INFO << "ShaderGenCache Vertex shader:\n" << vertstr << std::endl;
    OSG_INFO << "ShaderGenCache Fragment shader:\n" << fragstr << std::endl;
    
    auto shadername = getNameForShader(stateMask);

    osg::Shader* vertshader = new osg::Shader(osg::Shader::VERTEX, vertstr);
    vertshader->setName(shadername);

    osg::Shader* fragShader = new osg::Shader(osg::Shader::FRAGMENT, fragstr);
    fragShader->setName(shadername);

    program->addShader(vertshader);
    program->addShader(fragShader);
    
    return program;
}

std::string ShaderGenCache::getNameForShader(int stateMask)
{
    std::string result = "";
    if(stateMask & DIFFUSE_MAP) result += "Diffuse";
    if (stateMask & NORMAL_MAP) result += "Normal";
    if (stateMask & LIGHTING) result += "Lit"; else result += "Unlit";

    return result;
}

void ShaderGenCache::convertAndOutputSpirvCompatibleShaders(osg::Program* program, const std::string& directory)
{
    if (program != nullptr)
    {
        auto shadercount = program->getNumShaders();
        for (auto i = 0; i < shadercount; i++)
        {
            osg::Shader* shader = program->getShader(i);
            std::string ext = "";
            if (shader->getType() == osg::Shader::VERTEX)
            {
                ext = ".vert";
            }
            else if (shader->getType() == osg::Shader::FRAGMENT)
            {
                ext = ".frag";
            }
            else if (shader->getType() == osg::Shader::GEOMETRY)
            {
                std::cout << "convertAndOutputSpirvCompatibleShaders: Geometry shaders not implemented, skipping" << std::endl;
                return;
            }

            auto source = shader->getShaderSource();

            // replace transfrom matrix declerations
            source = replace(source, "uniform mat4 osg_ModelViewProjectionMatrix;\n", "layout(push_constant) uniform PushConstants {\n" \
                "mat4 projection;\n" \
                "mat4 view;\n" \
                "mat4 model;\n" \
                "mat3 normal;\n" \
                "} pc;\n");
            source = replace(source, "uniform mat4 osg_ModelViewMatrix;", "");
            source = replace(source, "uniform mat4 osg_ProjectionMatrix;", "");
            source = replace(source, "uniform mat4 osg_NormalMatrix;", "");

            // replace vert transformation
            //source = replace(source, "gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n", "gl_Position = projection.matrix * view.matrix * model.matrix * vec4(inPosition, 1.0);");

            // replace matrix usages
            source = replace(source, "osg_ModelViewProjectionMatrix", "(pc.projection * pc.view * pc.model)");
            source = replace(source, "osg_ModelViewMatrix", "(pc.view * pc.model)");
            source = replace(source, "osg_NormalMatrix", "pc.normal");

            // replace inputs/attributes
            source = replace(source, "osg_Vertex", "inPosition");
            source = replace(source, "osg_Normal", "inNormal");
            source = replace(source, "osg_MultiTexCoord0", "inTexCoord");

            auto shaderpath = osgDB::concatPaths(directory, shader->getName() + ext);
            std::ofstream outstream;
            outstream.open(shaderpath, std::ofstream::out | std::ofstream::trunc);
            outstream << source.c_str();
            outstream.close();
        }
    }
}

//
// ShaderGenVisitor
//

ShaderGenVisitor::ShaderGenVisitor() :
    NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _stateCache(new ShaderGenCache),
    _state(new StateEx)
{
}

ShaderGenVisitor::ShaderGenVisitor(ShaderGenCache *stateCache) :
    NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
    _stateCache(stateCache),
    _state(new StateEx)
{
}

void ShaderGenVisitor::setRootStateSet(osg::StateSet* stateSet)
{
    if (_rootStateSet.valid())
        _state->removeStateSet(0);
    _rootStateSet = stateSet;
    if (_rootStateSet.valid())
        _state->pushStateSet(_rootStateSet.get());
}

// convienience function to set the osg_LightSource uniform values
void ShaderGenVisitor::updateLightUniforms(osg::StateSet* stateset, osg::LightSource* light)
{
    stateset->addUniform(new osg::Vec4Uniform("osg_LightSource.position", light->getLight()->getPosition()));
    /*stateset->addUniform(new osg::Vec4Uniform("osg_LightSource[0].ambient", light->getLight()->getDiffuse()));
    stateset->addUniform(new osg::Vec4Uniform("osg_LightSource[0].diffuse", light->getLight()->getAmbient()));
    stateset->addUniform(new osg::Vec4Uniform("osg_LightSource[0].specular", light->getLight()->getSpecular()));
    stateset->addUniform(new osg::Vec3Uniform("osg_LightSource[0].spotDirection", light->getLight()->getDirection()));
    stateset->addUniform(new osg::FloatUniform("osg_LightSource[0].spotExponent", light->getLight()->getSpotExponent()));
    stateset->addUniform(new osg::FloatUniform("osg_LightSource[0].spotCutoff", light->getLight()->getSpotCutoff()));
    stateset->addUniform(new osg::FloatUniform("osg_LightSource[0].constantAttenuation", light->getLight()->getConstantAttenuation()));
    stateset->addUniform(new osg::FloatUniform("osg_LightSource[0].linearAttenuation", light->getLight()->getLinearAttenuation()));
    stateset->addUniform(new osg::FloatUniform("osg_LightSource[0].quadraticAttenuation", light->getLight()->getQuadraticAttenuation()));*/
}

void ShaderGenVisitor::reset()
{
    _state->popAllStateSets();
    if (_rootStateSet.valid())
        _state->pushStateSet(_rootStateSet.get());
}

void ShaderGenVisitor::apply(osg::Node& node)
{
    osg::StateSet *stateSet = node.getStateSet();
    
    if (stateSet)
        _state->pushStateSet(stateSet);
    
    traverse(node);
    
    if (stateSet)
        _state->popStateSet();
}

void ShaderGenVisitor::apply(osg::Geode& geode)
{
    osg::StateSet *stateSet = geode.getStateSet();
    if (stateSet)
        _state->pushStateSet(stateSet);
    
    for (unsigned int i=0; i<geode.getNumDrawables(); ++i)
    {
        osg::Drawable *drawable = geode.getDrawable(i);
        osg::StateSet *ss = drawable->getStateSet();
        if (ss)
            _state->pushStateSet(ss);
        
        update(drawable);
        
        if (ss)
            _state->popStateSet();
    }
    
    if (stateSet)
        _state->popStateSet();
}

void ShaderGenVisitor::apply(osg::LightSource& light)
{
    if(_light.valid()) return;

    _light = &light;

    osg::StateSet *stateset = _light->getOrCreateStateSet();
    updateLightUniforms(stateset, _light.get());
}

void ShaderGenVisitor::update(osg::Drawable* drawable)
{
    // update only geometry due to compatibility issues with user defined drawables
    osg::Geometry* geometry = drawable->asGeometry();
#if 0//1
    if (!geometry)
        return;
#endif
    
    StateEx* state = static_cast<StateEx*>(_state.get());
    // skip nodes without state sets
    if (state->getStateSetStackSize() == (_rootStateSet.valid() ? 1u : 0u))
        return;
    
    // skip state sets with already attached programs
    if (state->getAttribute(osg::StateAttribute::PROGRAM))
        return;
    
    int stateMask = 0;
    //if (state->getMode(GL_BLEND) & osg::StateAttribute::ON)
    //    stateMask |= ShaderGen::BLEND;
    if (state->getMode(GL_LIGHTING) & osg::StateAttribute::ON)
        stateMask |= ShaderGenCache::LIGHTING;
    if (state->getTextureAttribute(0, osg::StateAttribute::TEXTURE))
        stateMask |= ShaderGenCache::DIFFUSE_MAP;
    
    if (state->getTextureAttribute(1, osg::StateAttribute::TEXTURE) && geometry != 0 && geometry->getVertexAttribArray(6))
        stateMask |= ShaderGenCache::NORMAL_MAP;
    
    // Get program and uniforms for accumulated state.
    osg::StateSet* progss = _stateCache->getOrCreateStateSet(stateMask);
    // Set program and uniforms to the last state set.
    osg::StateSet* ss = const_cast<osg::StateSet *>(state->getStateSetStack().back());
    ss->setAttribute(progss->getAttribute(osg::StateAttribute::PROGRAM));
    ss->setUniformList(progss->getUniformList());
    
    //Edit, for now we will pinch the Material colors and bind as uniforms for non fixed function to replace gl_Front Material
    osg::Material* mat = dynamic_cast<osg::Material*>(ss->getAttribute(osg::StateAttribute::MATERIAL));
    if(mat)
    {
        ss->addUniform(new osg::Uniform("osg_Material.ambient", mat->getAmbient(osg::Material::FRONT)));
        ss->addUniform(new osg::Uniform("osg_Material.diffuse", mat->getDiffuse(osg::Material::FRONT)));
        ss->addUniform(new osg::Uniform("osg_Material.specular", mat->getSpecular(osg::Material::FRONT)));
        ss->addUniform(new osg::Uniform("osg_Material.shine", mat->getShininess(osg::Material::FRONT)));
        ss->removeAttribute(osg::StateAttribute::MATERIAL);
    }
    else
    {
        //if no material then setup some reasonable defaults
        ss->addUniform(new osg::Uniform("osg_Material.ambient", osg::Vec4(0.2f,0.2f,0.2f,1.0f)));
        ss->addUniform(new osg::Uniform("osg_Material.diffuse", osg::Vec4(0.8f,0.8f,0.8f,1.0f)));
        ss->addUniform(new osg::Uniform("osg_Material.specular", osg::Vec4(1.0f,1.0f,1.0f,1.0f)));
        ss->addUniform(new osg::Uniform("osg_Material.shine", 16.0f));
    }
    
    // remove any modes that won't be appropriate when using shaders
    if ((stateMask & ShaderGenCache::LIGHTING) != 0)
    {
        ss->removeMode(GL_LIGHTING);
        ss->removeMode(GL_LIGHT0);
    }
    if ((stateMask & ShaderGenCache::DIFFUSE_MAP) != 0) ss->removeTextureMode(0, GL_TEXTURE_2D);
    if ((stateMask & ShaderGenCache::NORMAL_MAP) != 0) ss->removeTextureMode(1, GL_TEXTURE_2D);
}


