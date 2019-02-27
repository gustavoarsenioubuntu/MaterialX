#include <MaterialXGenGlsl/Nodes/TimeNodeGlsl.h>

namespace MaterialX
{

ShaderNodeImplPtr TimeNodeGlsl::create()
{
    return std::make_shared<TimeNodeGlsl>();
}

void TimeNodeGlsl::createVariables(Shader& shader, const ShaderNode&, const ShaderGenerator&, GenContext&) const
{
    ShaderStage& ps = shader.getStage(HW::PIXEL_STAGE);
    addStageUniform(ps, HW::PRIVATE_UNIFORMS, Type::FLOAT, "u_frame");
}

void TimeNodeGlsl::emitFunctionCall(ShaderStage& stage, const ShaderNode& node, const ShaderGenerator& shadergen, GenContext& context) const
{
BEGIN_SHADER_STAGE(stage, HW::PIXEL_STAGE)
    shadergen.emitLineBegin(stage);
    shadergen.emitOutput(stage, context, node.getOutput(), true, false);
    shadergen.emitString(stage, " = u_frame / ");
    const ShaderInput* fpsInput = node.getInput("fps");
    const string fps = fpsInput->getValue()->getValueString();
    shadergen.emitString(stage, fps);
    shadergen.emitLineEnd(stage);
END_SHADER_STAGE(stage, HW::PIXEL_STAGE)
}

} // namespace MaterialX
