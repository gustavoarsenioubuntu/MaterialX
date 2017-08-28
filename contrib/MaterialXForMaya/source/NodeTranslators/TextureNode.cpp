// Copyright 2017 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
//
#include <NodeTranslators/TextureNode.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

namespace MaterialXForMaya
{

mx::NodePtr TextureNode::exportColorBalance(const MObject& mayaNode, mx::NodePtr node)
{
    mx::NodeGraphPtr parent = node->getParent()->asA<mx::NodeGraph>();
    const string& outputType = node->getType();
    const std::string name = getNodeName(mayaNode, outputType);

    MFnDependencyNode fnNode(mayaNode);

    MPlug exposurePlug = fnNode.findPlug("exposure", false);
    if (!exposurePlug.isNull() && exposurePlug.asFloat() != 0.0f)
    {
        mx::NodePtr exponent = parent->addNode("exponent", name + "_exponent", "float");
        exponent->setInputValue("in1", 2.0f);
        exponent->setInputValue("in2", exposurePlug.asFloat());

        mx::NodePtr multiply = parent->addNode("multiply", name + "_multiply", outputType);
        multiply->setConnectedNode("in1", node);
        multiply->setConnectedNode("in2", exponent);

        node = multiply;
    }

    MPlug invertPlug = fnNode.findPlug("invert", false);
    if (!invertPlug.isNull() && invertPlug.asBool())
    {
        mx::NodePtr invert = parent->addNode("invert", name + "_invert", outputType);
        invert->setConnectedNode("in", node);
        node = invert;
    }

    return node;
}

} // namespace MaterialXForMaya

