//
// TM & (c) 2017 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXGenOgsXml/GlslFragmentGenerator.h>
#include <MaterialXGenOgsXml/OgsXmlGenerator.h>

#include <MaterialXGenShader/Shader.h>

namespace MaterialX
{

string GlslFragmentSyntax::getVariableName(const string& name, const TypeDesc* type, GenContext& context) const
{
    string variable = GlslSyntax::getVariableName(name, type, context);
    // A filename input corresponds to a texture sampler uniform
    // which requires a special suffix in OGS XML fragments.
    if (type == Type::FILENAME)
    {
        // Make sure it's not already used.
        if (variable.size() <= OgsXmlGenerator::SAMPLER_SUFFIX.size() || 
            variable.substr(variable.size() - OgsXmlGenerator::SAMPLER_SUFFIX.size()) != OgsXmlGenerator::SAMPLER_SUFFIX)
        {
            variable += OgsXmlGenerator::SAMPLER_SUFFIX;
        }
    }
    return variable;
}

const string GlslFragmentGenerator::TARGET = "ogsxml";

GlslFragmentGenerator::GlslFragmentGenerator() :
    GlslShaderGenerator()
{
    // Use our custom syntax class
    _syntax = std::make_shared<GlslFragmentSyntax>();

    // Set identifier names to match OGS naming convention.
    _tokenSubstitutions[HW::T_POSITION_WORLD]       = "Pw";
    _tokenSubstitutions[HW::T_POSITION_OBJECT]      = "Pm";
    _tokenSubstitutions[HW::T_NORMAL_WORLD]         = "Nw";
    _tokenSubstitutions[HW::T_NORMAL_OBJECT]        = "Nm";
    _tokenSubstitutions[HW::T_TANGENT_WORLD]        = "Tw";
    _tokenSubstitutions[HW::T_TANGENT_OBJECT]       = "Tm";
    _tokenSubstitutions[HW::T_BITANGENT_WORLD]      = "Bw";
    _tokenSubstitutions[HW::T_BITANGENT_OBJECT]     = "Bm";
    _tokenSubstitutions[HW::T_VERTEX_DATA_INSTANCE] = "PIX_IN";
    _tokenSubstitutions[HW::T_ENV_IRRADIANCE]       = "u_envIrradianceSampler";
    _tokenSubstitutions[HW::T_ENV_RADIANCE]         = "u_envRadianceSampler";
}

ShaderGeneratorPtr GlslFragmentGenerator::create()
{
    return std::make_shared<GlslFragmentGenerator>();
}

ShaderPtr GlslFragmentGenerator::generate(const string& name, ElementPtr element, GenContext& context) const
{
    resetIdentifiers(context);
    ShaderPtr shader = createShader(name, element, context);

    ShaderStage& stage = shader->getStage(Stage::PIXEL);

    // Turn on fixed float formatting to make sure float values are
    // emitted with a decimal point and not as integers, and to avoid
    // any scientific notation which isn't supported by all OpenGL targets.
    Value::ScopedFloatFormatting fmt(Value::FloatFormatFixed);

    const ShaderGraph& graph = shader->getGraph();

    // Add global constants and type definitions
    emitInclude("pbrlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_defines.glsl", context, stage);
    const unsigned int maxLights = std::max(1u, context.getOptions().hwMaxActiveLightSources);
    emitLine("#define MAX_LIGHT_SOURCES " + std::to_string(maxLights), stage, false);
    emitLineBreak(stage);
    emitTypeDefinitions(context, stage);

    // Add all constants
    const VariableBlock& constants = stage.getConstantBlock();
    if (!constants.empty())
    {
        emitVariableDeclarations(constants, _syntax->getConstantQualifier(), SEMICOLON, context, stage);
        emitLineBreak(stage);
    }

    bool lighting = graph.hasClassification(ShaderNode::Classification::SHADER | ShaderNode::Classification::SURFACE) ||
        graph.hasClassification(ShaderNode::Classification::BSDF);

    // Emit common math functions
    emitInclude("pbrlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_math.glsl", context, stage);
    emitLineBreak(stage);

    // Emit lighting functions
    if (lighting)
    {
        if (context.getOptions().hwSpecularEnvironmentMethod == SPECULAR_ENVIRONMENT_FIS)
        {
            emitInclude("pbrlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_environment_fis.glsl", context, stage);
        }
        else
        {
            emitInclude("pbrlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_environment_prefilter.glsl", context, stage);
        }
        emitLineBreak(stage);
    }

    // Emit sampling code if needed
    if (graph.hasClassification(ShaderNode::Classification::CONVOLUTION2D))
    {
        // Emit sampling functions
        emitInclude("stdlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_sampling.glsl", context, stage);
        emitLineBreak(stage);
    }

    // Emit uv transform function
    if (context.getOptions().fileTextureVerticalFlip)
    {
        emitInclude("stdlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_get_target_uv_vflip.glsl", context, stage);
        emitLineBreak(stage);
    }
    else
    {
        emitInclude("stdlib/" + GlslShaderGenerator::LANGUAGE + "/lib/mx_get_target_uv_noop.glsl", context, stage);
        emitLineBreak(stage);
    }

    // Add all functions for node implementations
    emitFunctionDefinitions(graph, context, stage);

    const ShaderGraphOutputSocket* outputSocket = graph.getOutputSocket();

    // Add function signature
    string functionName = shader->getName();
    context.makeIdentifier(functionName);
    setFunctionName(functionName, stage);
    emitLine("vec3 " + functionName, stage, false); // TODO: We are always outputting vec3 for now
    emitScopeBegin(stage, Syntax::PARENTHESES);
    const VariableBlock& uniforms = stage.getUniformBlock(HW::PUBLIC_UNIFORMS);
    const size_t numUniforms = uniforms.size();
    for (size_t i = 0; i < numUniforms; ++i)
    {
        emitLineBegin(stage);
        emitVariableDeclaration(uniforms[i], EMPTY_STRING, context, stage, false);
        if (i < numUniforms-1)
        {
            emitString(COMMA, stage);
        }
        emitLineEnd(stage, false);
    }
    emitScopeEnd(stage);

    // Add function body
    emitScopeBegin(stage);

    if (graph.hasClassification(ShaderNode::Classification::CLOSURE))
    {
        // Handle the case where the graph is a direct closure.
        // We don't support rendering closures without attaching 
        // to a surface shader, so just output black.
        emitLine("return vec3(0.0)", stage);
    }
    else
    {
        // Add all function calls
        emitFunctionCalls(graph, context, stage);

        // Emit final result
        const ShaderOutput* outputConnection = outputSocket->getConnection();
        if (outputConnection)
        {
            string finalOutput = outputConnection->getVariable();
            const string& channels = outputSocket->getChannels();
            if (!channels.empty())
            {
                finalOutput = _syntax->getSwizzledVariable(finalOutput, outputConnection->getType(), channels, outputSocket->getType());
            }

            if (graph.hasClassification(ShaderNode::Classification::SURFACE))
            {
                if (context.getOptions().hwTransparency)
                {
                    // emitLine("float outAlpha = clamp(1.0 - dot(" + finalOutput + ".transparency, vec3(0.3333)), 0.0, 1.0)", stage);
                    emitLine("return " + finalOutput + ".color", stage);
                }
                else
                {
                    emitLine("return " + finalOutput + ".color", stage);
                }
            }
            else
            {
                if (!outputSocket->getType()->isFloat3())
                {
                    toVec3(outputSocket->getType(), finalOutput);
                }
                emitLine("return " + finalOutput, stage);
            }
        }
        else
        {
            string outputValue = outputSocket->getValue() ? _syntax->getValue(outputSocket->getType(), *outputSocket->getValue()) : _syntax->getDefaultValue(outputSocket->getType());
            if (!outputSocket->getType()->isFloat3())
            {
                string finalOutput = outputSocket->getVariable() + "_tmp";
                emitLine(_syntax->getTypeName(outputSocket->getType()) + " " + finalOutput + " = " + outputValue, stage);
                toVec3(outputSocket->getType(), finalOutput);
                emitLine("return " + finalOutput, stage);
            }
            else
            {
                emitLine("return " + outputValue, stage);
            }
        }
    }

    // End function
    emitScopeEnd(stage);

    // Replace all tokens with real identifier names
    replaceTokens(_tokenSubstitutions, stage);

    return shader;
}

void GlslFragmentGenerator::toVec3(const TypeDesc* type, string& variable)
{
    if (type->isFloat2())
    {
        variable = "vec3(" + variable + ", 0.0)";
    }
    else if (type->isFloat4())
    {
        variable = variable + ".xyz";
    }
    else if (type == Type::FLOAT || type == Type::INTEGER)
    {
        variable = "vec3(" + variable + ", " + variable + ", " + variable + ")";
    }
    else if (type == Type::BSDF || type == Type::EDF)
    {
        variable = "vec3(" + variable + ")";
    }
    else
    {
        // Can't understand other types. Just return black.
        variable = "vec3(0.0, 0.0, 0.0)";
    }
}

} // namespace MaterialX