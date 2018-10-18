#ifndef MATERIALX_COMPARE_H
#define MATERIALX_COMPARE_H

#include <MaterialXGenShader/ShaderImplementation.h>

namespace MaterialX
{

/// Implementation of compare node
class Compare : public ShaderImplementation
{
public:
    static ShaderImplementationPtr create();

    void emitFunctionCall(const ShaderNode& node, GenContext& context, ShaderGenerator& shadergen, Shader& shader) override;

public:
    static const vector<string> INPUT_NAMES;
};

} // namespace MaterialX

#endif
