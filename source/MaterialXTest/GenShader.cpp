//
// TM & (c) 2017 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXTest/Catch/catch.hpp>

#include <MaterialXCore/Document.h>
#include <MaterialXCore/Observer.h>

#include <MaterialXFormat/XmlIo.h>
#include <MaterialXFormat/File.h>

#include <MaterialXGenShader/HwShaderGenerator.h>
#include <MaterialXGenShader/Nodes/SwizzleNode.h>
#include <MaterialXGenShader/TypeDesc.h>
#include <MaterialXGenShader/Util.h>

#include <MaterialXTest/GenShaderUtil.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>

#if defined (MATERIALX_BUILD_CONTRIB)
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenOsl/OslShaderGenerator.h>
#endif

#if defined (MATERIALX_BUILD_CONTRIB)
#include <MaterialXContrib/OGSXMLFragmentWrapper.h>
#endif

namespace mx = MaterialX;

//
// Base tests
//

TEST_CASE("GenShader: Valid Libraries", "[genshader]")
{
    mx::DocumentPtr doc = mx::createDocument();

    mx::FilePath searchPath = mx::FilePath::getCurrentPath() / mx::FilePath("libraries");
    GenShaderUtil::loadLibraries({ "stdlib", "pbrlib" }, searchPath, doc);

    std::string validationErrors;
    bool valid = doc->validate(&validationErrors);
    if (!valid)
    {
        std::cout << validationErrors << std::endl;
    }
    REQUIRE(valid);
}

TEST_CASE("GenShader: TypeDesc Check", "[genshader]")
{
    // Make sure the standard types are registered
    const mx::TypeDesc* floatType = mx::TypeDesc::get("float");
    REQUIRE(floatType != nullptr);
    REQUIRE(floatType->getBaseType() == mx::TypeDesc::BASETYPE_FLOAT);
    const mx::TypeDesc* integerType = mx::TypeDesc::get("integer");
    REQUIRE(integerType != nullptr);
    REQUIRE(integerType->getBaseType() == mx::TypeDesc::BASETYPE_INTEGER);
    const mx::TypeDesc* booleanType = mx::TypeDesc::get("boolean");
    REQUIRE(booleanType != nullptr);
    REQUIRE(booleanType->getBaseType() == mx::TypeDesc::BASETYPE_BOOLEAN);
    const mx::TypeDesc* color2Type = mx::TypeDesc::get("color2");
    REQUIRE(color2Type != nullptr);
    REQUIRE(color2Type->getBaseType() == mx::TypeDesc::BASETYPE_FLOAT);
    REQUIRE(color2Type->getSemantic() == mx::TypeDesc::SEMANTIC_COLOR);
    REQUIRE(color2Type->isFloat2());
    const mx::TypeDesc* color3Type = mx::TypeDesc::get("color3");
    REQUIRE(color3Type != nullptr);
    REQUIRE(color3Type->getBaseType() == mx::TypeDesc::BASETYPE_FLOAT);
    REQUIRE(color3Type->getSemantic() == mx::TypeDesc::SEMANTIC_COLOR);
    REQUIRE(color3Type->isFloat3());
    const mx::TypeDesc* color4Type = mx::TypeDesc::get("color4");
    REQUIRE(color4Type != nullptr);
    REQUIRE(color4Type->getBaseType() == mx::TypeDesc::BASETYPE_FLOAT);
    REQUIRE(color4Type->getSemantic() == mx::TypeDesc::SEMANTIC_COLOR);
    REQUIRE(color4Type->isFloat4());

    // Make sure we can register a new sutom type
    const mx::TypeDesc* fooType = mx::TypeDesc::registerType("foo", mx::TypeDesc::BASETYPE_FLOAT, mx::TypeDesc::SEMANTIC_COLOR, 5);
    REQUIRE(fooType != nullptr);

    // Make sure we can't use a name already take
    REQUIRE_THROWS(mx::TypeDesc::registerType("color3", mx::TypeDesc::BASETYPE_FLOAT));

    // Make sure we can't request an unknown type
    REQUIRE_THROWS(mx::TypeDesc::get("bar"));
}

TEST_CASE("GenShader: OSL Reference Implementation Check", "[genshader]")
{
    mx::DocumentPtr doc = mx::createDocument();

    mx::FilePath searchPath = mx::FilePath::getCurrentPath() / mx::FilePath("libraries");
    GenShaderUtil::loadLibraries({ "stdlib" }, searchPath, doc);

    // Set source code search path
    mx::FileSearchPath sourceCodeSearchPath;
    sourceCodeSearchPath.append(searchPath);

    std::filebuf implDumpBuffer;
    std::string fileName = "osl_vanilla_implementation_check.txt";
    implDumpBuffer.open(fileName, std::ios::out);
    std::ostream implDumpStream(&implDumpBuffer);

    implDumpStream << "-----------------------------------------------------------------------" << std::endl;
    implDumpStream << "Scanning language: osl. Target: reference" << std::endl;
    implDumpStream << "-----------------------------------------------------------------------" << std::endl;

    const std::string language("osl");
    const std::string target("");

    std::vector<mx::ImplementationPtr> impls = doc->getImplementations();
    implDumpStream << "Existing implementations: " << std::to_string(impls.size()) << std::endl;
    implDumpStream << "-----------------------------------------------------------------------" << std::endl;
    for (auto impl : impls)
    {
        if (language == impl->getLanguage() && impl->getTarget().empty())
        {
            std::string msg("Impl: ");
            msg += impl->getName();

            mx::NodeDefPtr nodedef = impl->getNodeDef();
            if (!nodedef)
            {
                std::string nodedefName = impl->getNodeDefString();
                msg += ". Does NOT have a nodedef with name: " + nodedefName;
            }
            implDumpStream << msg << std::endl;
        }
    }

    std::string nodeDefNode;
    std::string nodeDefType;
    unsigned int count = 0;
    unsigned int missing = 0;
    std::string missing_str;
    std::string found_str;

    // Scan through every nodedef defined
    for (mx::NodeDefPtr nodeDef : doc->getNodeDefs())
    {
        count++;

        std::string nodeDefName = nodeDef->getName();
        std::string nodeName = nodeDef->getNodeString();
        if (!mx::requiresImplementation(nodeDef))
        {
            found_str += "No implementation required for nodedef: " + nodeDefName + ", Node: " + nodeName + ".\n";
            continue;
        }

        mx::InterfaceElementPtr inter = nodeDef->getImplementation(target, language);
        if (!inter)
        {
            missing++;
            missing_str += "Missing nodeDef implemenation: " + nodeDefName + ", Node: " + nodeName + ".\n";
        }
        else
        {
            mx::ImplementationPtr impl = inter->asA<mx::Implementation>();
            if (impl)
            {
                // Scan for file and see if we can read in the contents
                std::string sourceContents;
                mx::FilePath sourcePath = impl->getFile();
                mx::FilePath resolvedPath = sourceCodeSearchPath.find(sourcePath);
                bool found = mx::readFile(resolvedPath.asString(), sourceContents);
                if (!found)
                {
                    missing++;
                    missing_str += "Missing source code: " + sourcePath.asString() + " for nodeDef: "
                        + nodeDefName + ". Impl: " + impl->getName() + ".\n";
                }
                else
                {
                    found_str += "Found impl and src for nodedef: " + nodeDefName + ", Node: "
                        + nodeName + ". Impl: " + impl->getName() + ".\n";
                }
            }
            else
            {
                mx::NodeGraphPtr graph = inter->asA<mx::NodeGraph>();
                found_str += "Found NodeGraph impl for nodedef: " + nodeDefName + ", Node: "
                    + nodeName + ". Impl: " + graph->getName() + ".\n";
            }
        }
    }

    implDumpStream << "-----------------------------------------------------------------------" << std::endl;
    implDumpStream << "Missing: " << missing << " implementations out of: " << count << " nodedefs\n";
    implDumpStream << missing_str << std::endl;
    implDumpStream << found_str << std::endl;
    implDumpStream << "-----------------------------------------------------------------------" << std::endl;

    implDumpBuffer.close();

    // To enable once this is true
    //REQUIRE(missing == 0);
}

#if defined (MATERIALX_BUILD_CONTRIB)
TEST_CASE("GenShader: Generate OGS fragment wrappers", "[genogsfrag]")
{
    mx::DocumentPtr doc = mx::createDocument();
    try
    {
        mx::FilePath searchPath = mx::FilePath::getCurrentPath() / mx::FilePath("libraries");
        //mx::readFromXmlFile(doc, "resources/Materials/TestSuite/stdlib/geometric/streams.mtlx");
        mx::readFromXmlFile(doc, "resources/Materials/TestSuite/stdlib/texture/tiledImage.mtlx");
        //mx::readFromXmlFile(doc, "resources/Materials/TestSuite/stdlib/texture/image_addressing.mtlx");
        //mx::readFromXmlFile(doc, "resources/Materials/Examples/StandardSurface/standard_surface_default.mtlx");
        mx::StringVec libraryFolders = { "stdlib", "pbrlib", "stdlib/genglsl", "pbrlib/genglsl", "bxdf" };
        GenShaderUtil::loadLibraries(libraryFolders, searchPath, doc);

        std::vector<mx::GenContext*> contexts;
        mx::GenContext* glslContext = new mx::GenContext(mx::GlslShaderGenerator::create());
        // Stop emitting version strings
        glslContext->getOptions().emitVersionString = false;
        glslContext->registerSourceCodeSearchPath(searchPath);
        contexts.push_back(glslContext);
        mx::GenContext* oslContext = new mx::GenContext(mx::OslShaderGenerator::create());
        oslContext->registerSourceCodeSearchPath(searchPath);
        contexts.push_back(oslContext);

        // TODO: We want 1 wrapper with both languages -- not 2 wrappers
        mx::OGSXMLFragmentWrapper glslWrapper;
        glslWrapper.setOutputVertexShader(false);
        mx::OGSXMLFragmentWrapper oslWrapper;

        std::vector<mx::TypedElementPtr> renderables;
        mx::findRenderableElements(doc, renderables, false);
        for (auto elem : renderables)
        {
            mx::OutputPtr output = elem->asA<mx::Output>();
            mx::ShaderRefPtr shaderRef = elem->asA<mx::ShaderRef>();
            mx::NodeDefPtr nodeDef = nullptr;
            if (output)
            {
                // Stop emission of environment map lookups for texture nodes
                glslContext->getOptions().hwSpecularEnvironmentMethod = mx::SPECULAR_ENVIRONMENT_NONE;
                nodeDef = output->getConnectedNode()->getNodeDef();
            }
            else if (shaderRef)
            {
                // TODO: Need to setup lighting information here...
                glslContext->getOptions().hwSpecularEnvironmentMethod = mx::SPECULAR_ENVIRONMENT_FIS;
                // Don't handle direct lighting for now
                glslContext->getOptions().hwMaxActiveLightSources = 0;
                nodeDef = shaderRef->getNodeDef();
            }
            glslContext->getOptions().fileTextureVerticalFlip = true;

            if (nodeDef)
            {
                glslWrapper.generate(elem->getName(), elem, *glslContext);
                //oslWrapper.generate(elem->getName(), elem, *oslContext);
            }
        }

        std::ofstream glslStreamFile("glslOGSXMLFragmentDump.xml");
        std::stringstream glslStream;
        glslWrapper.getXML(glslStream);
        glslStreamFile << glslStream.str();
        glslStreamFile.close();

        std::ifstream fileStream("glslOGSXMLFragmentDump.xml");
        std::stringstream readFileStream;
        glslWrapper.readDocument(fileStream, readFileStream);

        std::string glslStreamString;
        glslStreamString = glslStream.str();
        std::string readFileStreamString;
        readFileStreamString = readFileStream.str();
        if (readFileStreamString.compare(glslStreamString) != 0)
        {
            std::cout << "OGS XML Wrapper read/write failure\n";
            std::cout << "================= Read document ===================\n";
            std::cout << readFileStreamString << std::endl;
            std::cout << "================= Versus Written document ===================\n";
            std::cout << glslStreamString << std::endl;
        }

        //const mx::StringMap& inputs = glslWrapper.getPathInputMap();
        //for (auto i : inputs)
        //{
        //    std::cout << "Element: " << i.first << " maps to fragment input: " << i.second << std::endl;
        //}

        std::ofstream oslStream("oslOGSXMLFragmentDump.xml");
        oslWrapper.getXML(oslStream);
        oslStream.close();

    }
    catch (mx::Exception& e)
    {
        std::cerr << "Failed to generate OGS XML wrapper: " << e.what() << std::endl;
        mx::writeToXmlFile(doc, "glslOGSXMLFragmentDump_Failed.mtlx");
    }
}
#endif
