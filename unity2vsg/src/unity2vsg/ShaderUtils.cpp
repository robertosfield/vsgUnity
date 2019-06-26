#include <unity2vsg/ShaderUtils.h>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>

#include "glsllang/ResourceLimits.h"

#include <algorithm>
#include <iomanip>

using namespace unity2vsg;

#if 1
#    define DEBUG_OUTPUT std::cout
#else
#    define DEBUG_OUTPUT \
        if (false) std::cout
#endif
#define INFO_OUTPUT std::cout

// create defines string based of shader mask

std::vector<std::string> createPSCDefineStrings(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, const std::vector<std::string>& customDefines)
{
    bool hasnormal = geometryAttrbutes & NORMAL;
    bool hastanget = geometryAttrbutes & TANGENT;
    bool hascolor = geometryAttrbutes & COLOR;
    bool hastex0 = geometryAttrbutes & TEXCOORD0;
    bool hastex1 = geometryAttrbutes & TEXCOORD1;

    std::vector<std::string> defines;

    // vertx inputs
    if (hasnormal) defines.push_back("VSG_NORMAL");
    if (hastanget) defines.push_back("VSG_TANGENT");
    if (hascolor) defines.push_back("VSG_COLOR");
    if (hastex0) defines.push_back("VSG_TEXCOORD0");
    if (hastex1) defines.push_back("VSG_TEXCOORD1");

    // shading modes/maps
    if (hasnormal && (shaderModeMask & LIGHTING)) defines.push_back("VSG_LIGHTING");

    if (shaderModeMask & MATERIAL) defines.push_back("VSG_MATERIAL");

    if (hastex0 && (shaderModeMask & DIFFUSE_MAP)) defines.push_back("VSG_DIFFUSE_MAP");
    if (hastex0 && (shaderModeMask & OPACITY_MAP)) defines.push_back("VSG_OPACITY_MAP");
    if (hastex0 && (shaderModeMask & AMBIENT_MAP)) defines.push_back("VSG_AMBIENT_MAP");
    if (hastex0 && (shaderModeMask & NORMAL_MAP)) defines.push_back("VSG_NORMAL_MAP");
    if (hastex0 && (shaderModeMask & SPECULAR_MAP)) defines.push_back("VSG_SPECULAR_MAP");

    if (shaderModeMask & BILLBOARD) defines.push_back("VSG_BILLBOARD");

    if (customDefines.size() > 0)
    {
        std::copy(customDefines.begin(), customDefines.end(), std::back_inserter(defines));
    }

    return defines;
}

// insert defines string after the version in source

std::string processGLSLShaderSource(const std::string& source, const std::vector<std::string>& defines)
{
    // trim leading spaces/tabs
    auto trimLeading = [](std::string& str) {
        size_t startpos = str.find_first_not_of(" \t");
        if (std::string::npos != startpos)
        {
            str = str.substr(startpos);
        }
    };

    // trim trailing spaces/tabs/newlines
    auto trimTrailing = [](std::string& str) {
        size_t endpos = str.find_last_not_of(" \t\n");
        if (endpos != std::string::npos)
        {
            str = str.substr(0, endpos + 1);
        }
    };

    // sanitise line by triming leading and trailing characters
    auto sanitise = [&trimLeading, &trimTrailing](std::string& str) {
        trimLeading(str);
        trimTrailing(str);
    };

    // return true if str starts with match string
    auto startsWith = [](const std::string& str, const std::string& match) {
        return str.compare(0, match.length(), match) == 0;
    };

    // returns the string between the start and end character
    auto stringBetween = [](const std::string& str, const char& startChar, const char& endChar) {
        auto start = str.find_first_of(startChar);
        if (start == std::string::npos) return std::string();

        auto end = str.find_first_of(endChar, start);
        if (end == std::string::npos) return std::string();

        if ((end - start) - 1 == 0) return std::string();

        return str.substr(start + 1, (end - start) - 1);
    };

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

    auto addLine = [](std::ostringstream& ss, const std::string& line) {
        ss << line << "\n";
    };

    std::istringstream iss(source);
    std::ostringstream headerstream;
    std::ostringstream sourcestream;

    const std::string versionmatch = "#version";
    const std::string importdefinesmatch = "#pragma import_defines";

    std::vector<std::string> finaldefines;

    for (std::string line; std::getline(iss, line);)
    {
        std::string sanitisedline = line;
        sanitise(sanitisedline);

        // is it the version
        if (startsWith(sanitisedline, versionmatch))
        {
            addLine(headerstream, line);
        }
        // is it the defines import
        else if (startsWith(sanitisedline, importdefinesmatch))
        {
            // get the import defines between ()
            auto csv = stringBetween(sanitisedline, '(', ')');
            auto importedDefines = split(csv, ',');

            addLine(headerstream, line);

            // loop the imported defines and see if it's also requested in defines, if so insert a define line
            for (auto importedDef : importedDefines)
            {
                auto sanitiesedImportDef = importedDef;
                sanitise(sanitiesedImportDef);

                auto finditr = std::find(defines.begin(), defines.end(), sanitiesedImportDef);
                if (finditr != defines.end())
                {
                    addLine(headerstream, "#define " + sanitiesedImportDef);
                }
            }
        }
        else
        {
            // standard source line
            addLine(sourcestream, line);
        }
    }

    return headerstream.str() + sourcestream.str();
}

std::string debugFormatShaderSource(const std::string& source)
{
    std::istringstream iss(source);
    std::ostringstream oss;

    uint32_t linecount = 1;

    for (std::string line; std::getline(iss, line);)
    {
        oss << std::setw(4) << std::setfill(' ') << linecount << ": " << line << "\n";
        linecount++;
    }
    return oss.str();
}

// read a glsl file and inject defines based on shadermodemask and geometryatts
std::string unity2vsg::readGLSLShader(const std::string& filename, const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, const std::vector<std::string>& customDefines)
{
    std::string sourceBuffer;
    if (!vsg::readFile(sourceBuffer, filename))
    {
        DEBUG_OUTPUT << "readGLSLShader: Failed to read file '" << filename << std::endl;
        return std::string();
    }

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes, customDefines);
    std::string formatedSource = processGLSLShaderSource(sourceBuffer, defines);
    return formatedSource;
}

// create an fbx vertex shader

std::string unity2vsg::createFbxVertexSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, const std::vector<std::string>& customDefines)
{
    std::string source =
        "#version 450\n"
        "#pragma import_defines ( VSG_NORMAL, VSG_TANGENT, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_NORMAL_MAP, VSG_BILLBOARD )\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        "layout(push_constant) uniform PushConstants {\n"
        "    mat4 projection;\n"
        "    mat4 modelview;\n"
        "    //mat3 normal;\n"
        "} pc; \n"
        "layout(location = 0) in vec3 osg_Vertex;\n"
        "#ifdef VSG_NORMAL\n"
        "layout(location = 1) in vec3 osg_Normal;\n"
        "layout(location = 1) out vec3 normalDir;\n"
        "#endif\n"
        "#ifdef VSG_TANGENT\n"
        "layout(location = 2) in vec4 osg_Tangent;\n"
        "#endif\n"
        "#ifdef VSG_COLOR\n"
        "layout(location = 3) in vec4 osg_Color;\n"
        "layout(location = 3) out vec4 vertColor;\n"
        "#endif\n"
        "#ifdef VSG_TEXCOORD0\n"
        "layout(location = 4) in vec2 osg_MultiTexCoord0;\n"
        "layout(location = 4) out vec2 texCoord0;\n"
        "#endif\n"
        "#ifdef VSG_LIGHTING\n"
        "layout(location = 5) out vec3 viewDir;\n"
        "layout(location = 6) out vec3 lightDir;\n"
        "#endif\n"
        "out gl_PerVertex{ vec4 gl_Position; };\n"
        "\n"
        "void main()\n"
        "{\n"
        "    mat4 modelView = pc.modelview;\n"
        "#ifdef VSG_BILLBOARD\n"
        "    // xaxis\n"
        "    modelView[0][0] = 1.0;\n"
        "    modelView[0][1] = 0.0;\n"
        "    modelView[0][2] = 0.0;\n"
        "    // yaxis\n"
        "    modelView[1][0] = 0.0;\n"
        "    modelView[1][1] = 1.0;\n"
        "    modelView[1][2] = 0.0;\n"
        "    // zaxis\n"
        "    //modelView[2][0] = 0.0;\n"
        "    //modelView[2][1] = 0.0;\n"
        "    //modelView[2][2] = 1.0;\n"
        "#endif\n"
        "    gl_Position = (pc.projection * modelView) * vec4(osg_Vertex, 1.0);\n"
        "#ifdef VSG_TEXCOORD0\n"
        "    texCoord0 = osg_MultiTexCoord0.st;\n"
        "#endif\n"
        "#ifdef VSG_NORMAL\n"
        "    vec3 n = (modelView * vec4(osg_Normal, 0.0)).xyz;\n"
        "    normalDir = n;\n"
        "#endif\n"
        "#ifdef VSG_LIGHTING\n"
        "    vec4 lpos = /*osg_LightSource.position*/ vec4(0.0, 0.25, 1.0, 0.0);\n"
        "#ifdef VSG_NORMAL_MAP\n"
        "    vec3 t = (modelView * vec4(osg_Tangent.xyz, 0.0)).xyz;\n"
        "    vec3 b = cross(n, t);\n"
        "    vec3 dir = -vec3(modelView * vec4(osg_Vertex, 1.0));\n"
        "    viewDir.x = dot(dir, t);\n"
        "    viewDir.y = dot(dir, b);\n"
        "    viewDir.z = dot(dir, n);\n"
        "    if (lpos.w == 0.0)\n"
        "        dir = lpos.xyz;\n"
        "    else\n"
        "        dir += lpos.xyz;\n"
        "    lightDir.x = dot(dir, t); \n"
        "    lightDir.y = dot(dir, b);\n"
        "    lightDir.z = dot(dir, n); \n"
        "#else\n"
        "    viewDir = -vec3(modelView * vec4(osg_Vertex, 1.0));\n"
        "    if (lpos.w == 0.0)\n"
        "        lightDir = lpos.xyz;\n"
        "    else\n"
        "        lightDir = lpos.xyz + viewDir;\n"
        "#endif\n"
        "#endif\n"
        "#ifdef VSG_COLOR\n"
        "    vertColor = osg_Color;\n"
        "#endif\n"
        "}\n";

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes, customDefines);
    std::string formatedSource = processGLSLShaderSource(source, defines);

    return formatedSource;
}

// create an fbx fragment shader

std::string unity2vsg::createFbxFragmentSource(const uint32_t& shaderModeMask, const uint32_t& geometryAttrbutes, const std::vector<std::string>& customDefines)
{
    std::string source =
        "#version 450\n"
        "#pragma import_defines ( VSG_NORMAL, VSG_COLOR, VSG_TEXCOORD0, VSG_LIGHTING, VSG_MATERIAL, VSG_DIFFUSE_MAP, VSG_OPACITY_MAP, VSG_AMBIENT_MAP, VSG_NORMAL_MAP, VSG_SPECULAR_MAP )\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        "#ifdef VSG_DIFFUSE_MAP\n"
        "layout(binding = 0) uniform sampler2D diffuseMap; \n"
        "#endif\n"
        "#ifdef VSG_OPACITY_MAP\n"
        "layout(binding = 1) uniform sampler2D opacityMap;\n"
        "#endif\n"
        "#ifdef VSG_AMBIENT_MAP\n"
        "layout(binding = 4) uniform sampler2D ambientMap; \n"
        "#endif\n"
        "#ifdef VSG_NORMAL_MAP\n"
        "layout(binding = 5) uniform sampler2D normalMap;\n"
        "#endif\n"
        "#ifdef VSG_SPECULAR_MAP\n"
        "layout(binding = 6) uniform sampler2D specularMap; \n"
        "#endif\n"

        "#ifdef VSG_MATERIAL\n"
        "layout(binding = 10) uniform MaterialData\n"
        "{\n"
        "    vec4 ambientColor;\n"
        "    vec4 diffuseColor; \n"
        "    vec4 specularColor;\n"
        "    float shine; \n"
        "} material;\n"
        "#endif\n"

        "#ifdef VSG_NORMAL\n"
        "layout(location = 1) in vec3 normalDir; \n"
        "#endif\n"
        "#ifdef VSG_COLOR\n"
        "layout(location = 3) in vec4 vertColor; \n"
        "#endif\n"
        "#ifdef VSG_TEXCOORD0\n"
        "layout(location = 4) in vec2 texCoord0;\n"
        "#endif\n"
        "#ifdef VSG_LIGHTING\n"
        "layout(location = 5) in vec3 viewDir; \n"
        "layout(location = 6) in vec3 lightDir;\n"
        "#endif\n"
        "layout(location = 0) out vec4 outColor;\n"
        "\n"
        "void main()\n"
        "{\n"
        "#ifdef VSG_DIFFUSE_MAP\n"
        "    vec4 base = texture(diffuseMap, texCoord0.st);\n"
        "#else\n"
        "    vec4 base = vec4(1.0,1.0,1.0,1.0);\n"
        "#endif\n"
        "#ifdef VSG_COLOR\n"
        "    base = base * vertColor;\n"
        "#endif\n"
        "#ifdef VSG_MATERIAL\n"
        "    vec3 ambientColor = material.ambientColor.rgb;\n"
        "    vec3 diffuseColor = material.diffuseColor.rgb;\n"
        "    vec3 specularColor = material.specularColor.rgb;\n"
        "    float shine = material.shine;\n"
        "#else\n"
        "    vec3 ambientColor = vec3(0.1,0.1,0.1);\n"
        "    vec3 diffuseColor = vec3(1.0,1.0,1.0);\n"
        "    vec3 specularColor = vec3(0.3,0.3,0.3);\n"
        "    float shine = 16.0;\n"
        "#endif\n"
        "#ifdef VSG_AMBIENT_MAP\n"
        "    ambientColor *= texture(ambientMap, texCoord0.st).r;\n"
        "#endif\n"
        "#ifdef VSG_SPECULAR_MAP\n"
        "    specularColor = texture(specularMap, texCoord0.st).rrr;\n"
        "#endif\n"
        "#ifdef VSG_LIGHTING\n"
        "#ifdef VSG_NORMAL_MAP\n"
        "    vec3 nDir = texture(normalMap, texCoord0.st).xyz*2.0 - 1.0;\n"
        "    nDir.g = -nDir.g;\n"
        "#else\n"
        "    vec3 nDir = normalDir;\n"
        "#endif\n"
        "    vec3 nd = normalize(nDir);\n"
        "    vec3 ld = normalize(lightDir);\n"
        "    vec3 vd = normalize(viewDir);\n"
        "    vec4 color = vec4(0.01, 0.01, 0.01, 1.0);\n"
        "    color.rgb += ambientColor;\n"
        "    float diff = max(dot(ld, nd), 0.0);\n"
        "    color.rgb += diffuseColor * diff;\n"
        "    color *= base;\n"
        "    if (diff > 0.0)\n"
        "    {\n"
        "        vec3 halfDir = normalize(ld + vd);\n"
        "        color.rgb += base.a * specularColor *\n"
        "            pow(max(dot(halfDir, nd), 0.0), shine);\n"
        "    }\n"
        "#else\n"
        "    vec4 color = base;\n"
        "    color.rgb *= diffuseColor;\n"
        "#endif\n"
        "    outColor = color;\n"
        "#ifdef VSG_OPACITY_MAP\n"
        "    outColor.a *= texture(opacityMap, texCoord0.st).r;\n"
        "#endif\n"
        "}\n";

    auto defines = createPSCDefineStrings(shaderModeMask, geometryAttrbutes, customDefines);
    std::string formatedSource = processGLSLShaderSource(source, defines);

    return formatedSource;
}

ShaderCompiler::ShaderCompiler(vsg::Allocator* allocator) :
    vsg::Object(allocator)
{
    glslang::InitializeProcess();
}

ShaderCompiler::~ShaderCompiler()
{
    glslang::FinalizeProcess();
}

bool ShaderCompiler::compile(vsg::ShaderStages& shaders)
{
    auto getFriendlyNameForShader = [](const vsg::ref_ptr<vsg::ShaderStage>& vsg_shader) {
        switch (vsg_shader->getShaderStageFlagBits())
        {
        case (VK_SHADER_STAGE_VERTEX_BIT): return "Vertex Shader";
        case (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT): return "Tessellation Control Shader";
        case (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT): return "Tessellation Evaluation Shader";
        case (VK_SHADER_STAGE_GEOMETRY_BIT): return "Geometry Shader";
        case (VK_SHADER_STAGE_FRAGMENT_BIT): return "Fragment Shader";
        case (VK_SHADER_STAGE_COMPUTE_BIT): return "Compute Shader";
        default: return "Unkown Shader Type";
        }
        return "";
    };

    using StageShaderMap = std::map<EShLanguage, vsg::ref_ptr<vsg::ShaderStage>>;
    using TShaders = std::list<std::unique_ptr<glslang::TShader>>;
    TShaders tshaders;

    TBuiltInResource builtInResources = glslang::DefaultTBuiltInResource;

    StageShaderMap stageShaderMap;
    std::unique_ptr<glslang::TProgram> program(new glslang::TProgram);

    for (auto& vsg_shader : shaders)
    {
        EShLanguage envStage = EShLangCount;
        switch (vsg_shader->getShaderStageFlagBits())
        {
        case (VK_SHADER_STAGE_VERTEX_BIT): envStage = EShLangVertex; break;
        case (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT): envStage = EShLangTessControl; break;
        case (VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT): envStage = EShLangTessEvaluation; break;
        case (VK_SHADER_STAGE_GEOMETRY_BIT): envStage = EShLangGeometry; break;
        case (VK_SHADER_STAGE_FRAGMENT_BIT): envStage = EShLangFragment; break;
        case (VK_SHADER_STAGE_COMPUTE_BIT): envStage = EShLangCompute; break;
#ifdef VK_SHADER_STAGE_RAYGEN_BIT_NV
        case (VK_SHADER_STAGE_RAYGEN_BIT_NV): envStage = EShLangRayGenNV; break;
        case (VK_SHADER_STAGE_ANY_HIT_BIT_NV): envStage = EShLangAnyHitNV; break;
        case (VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV): envStage = EShLangClosestHitNV; break;
        case (VK_SHADER_STAGE_MISS_BIT_NV): envStage = EShLangMissNV; break;
        case (VK_SHADER_STAGE_INTERSECTION_BIT_NV): envStage = EShLangIntersectNV; break;
        case (VK_SHADER_STAGE_CALLABLE_BIT_NV): envStage = EShLangCallableNV; break;
        case (VK_SHADER_STAGE_TASK_BIT_NV): envStage = EShLangTaskNV; ;
        case (VK_SHADER_STAGE_MESH_BIT_NV): envStage = EShLangMeshNV; break;
#endif
        default: break;
        }

        if (envStage == EShLangCount) return false;

        glslang::TShader* shader(new glslang::TShader(envStage));
        tshaders.emplace_back(shader);

        shader->setEnvInput(glslang::EShSourceGlsl, envStage, glslang::EShClientVulkan, 150);
        shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

        const char* str = vsg_shader->getShaderModule()->source().c_str();
        shader->setStrings(&str, 1);

        int defaultVersion = 110; // 110 desktop, 100 non desktop
        bool forwardCompatible = false;
        EShMessages messages = EShMsgDefault;
        bool parseResult = shader->parse(&builtInResources, defaultVersion, forwardCompatible, messages);

        if (parseResult)
        {
            program->addShader(shader);

            stageShaderMap[envStage] = vsg_shader;
        }
        else
        {
            // print error infomation
            INFO_OUTPUT << std::endl
                        << "----  " << getFriendlyNameForShader(vsg_shader) << "  ----" << std::endl
                        << std::endl;
            INFO_OUTPUT << debugFormatShaderSource(vsg_shader->getShaderModule()->source()) << std::endl;
            INFO_OUTPUT << "Warning: GLSL source failed to parse." << std::endl;
            INFO_OUTPUT << "glslang info log: " << std::endl
                        << shader->getInfoLog();
            DEBUG_OUTPUT << "glslang debug info log: " << std::endl
                         << shader->getInfoDebugLog();
            INFO_OUTPUT << "--------" << std::endl;
        }
    }

    if (stageShaderMap.empty() || stageShaderMap.size() != shaders.size())
    {
        DEBUG_OUTPUT << "ShaderCompiler::compile(Shaders& shaders) stageShaderMap.size() != shaders.size()" << std::endl;
        return false;
    }

    EShMessages messages = EShMsgDefault;
    if (!program->link(messages))
    {
        INFO_OUTPUT << std::endl
                    << "----  Program  ----" << std::endl
                    << std::endl;

        for (auto& vsg_shader : shaders)
        {
            INFO_OUTPUT << std::endl
                        << getFriendlyNameForShader(vsg_shader) << ":" << std::endl
                        << std::endl;
            INFO_OUTPUT << debugFormatShaderSource(vsg_shader->getShaderModule()->source()) << std::endl;
        }

        INFO_OUTPUT << "Warning: Program failed to link." << std::endl;
        INFO_OUTPUT << "glslang info log: " << std::endl
                    << program->getInfoLog();
        DEBUG_OUTPUT << "glslang debug info log: " << std::endl
                     << program->getInfoDebugLog();
        INFO_OUTPUT << "--------" << std::endl;

        return false;
    }

    for (int eshl_stage = 0; eshl_stage < EShLangCount; ++eshl_stage)
    {
        auto vsg_shader = stageShaderMap[(EShLanguage)eshl_stage];
        if (vsg_shader && program->getIntermediate((EShLanguage)eshl_stage))
        {
            vsg::ShaderModule::SPIRV spirv;
            std::string warningsErrors;
            spv::SpvBuildLogger logger;
            glslang::SpvOptions spvOptions;
            glslang::GlslangToSpv(*(program->getIntermediate((EShLanguage)eshl_stage)), vsg_shader->getShaderModule()->spirv(), &logger, &spvOptions);
        }
    }

    return true;
}
