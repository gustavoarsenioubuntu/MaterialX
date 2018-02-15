#include <MaterialXShaderGen/ShaderGenerators/Glsl/GeomColorGlsl.h>

namespace MaterialX
{

SgImplementationPtr GeomColorGlsl::creator()
{
    return std::make_shared<GeomColorGlsl>();
}

void GeomColorGlsl::createVariables(const SgNode& node, ShaderGenerator& /*shadergen*/, Shader& shader_)
{
    HwShader& shader = static_cast<HwShader&>(shader_);

    const SgOutput* output = node.getOutput();
    const SgInput* indexInput = node.getInput(INDEX);
    const string index = indexInput ? indexInput->value->getValueString() : "0";

    if (output->type == DataType::COLOR4)
    {
        shader.createAppData(DataType::COLOR4, "i_color4_" + index);
        shader.createVertexData(DataType::COLOR4, "color4_" + index);
    }
    else
    {
        shader.createAppData(DataType::COLOR3, "i_color3_" + index);
        shader.createVertexData(DataType::COLOR3, "color3_" + index);
    }
}

void GeomColorGlsl::emitFunctionCall(const SgNode& node, ShaderGenerator& shadergen, Shader& shader_)
{
    HwShader& shader = static_cast<HwShader&>(shader_);

    const string& blockInstance = shader.getVertexDataBlock().instance;
    const string blockPrefix = blockInstance.length() ? blockInstance + "." : EMPTY_STRING;

    const SgOutput* output = node.getOutput();
    const SgInput* indexInput = node.getInput(INDEX);
    string index = indexInput ? indexInput->value->getValueString() : "0";
    string variable = (output->type == DataType::COLOR4 ? "color4_" : "color3_") + index;

    BEGIN_SHADER_STAGE(shader, HwShader::VERTEX_STAGE)
        if (!shader.isCalculated(variable))
        {
            shader.addLine(blockPrefix + variable + " = i_" + variable);
            shader.setCalculated(variable);
        }
    END_SHADER_STAGE(shader, HwShader::VERTEX_STAGE)

    BEGIN_SHADER_STAGE(shader, HwShader::PIXEL_STAGE)
        string suffix = "";
        if (output->type == DataType::FLOAT)
        {
            suffix = ".r";
        }
        else if (output->type == DataType::COLOR2)
        {
            suffix = ".rg";
        }
        shader.beginLine();
        shadergen.emitOutput(node.getOutput(), true, shader);
        shader.addStr(" = " + blockPrefix + variable + suffix);
        shader.endLine();
    END_SHADER_STAGE(shader, HwShader::PIXEL_STAGE)
}

} // namespace MaterialX
