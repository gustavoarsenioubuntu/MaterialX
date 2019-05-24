//
// TM & (c) 2017 Lucasfilm Entertainment Company Ltd. and Lucasfilm Ltd.
// All rights reserved.  See LICENSE.txt for license.
//

#include <MaterialXFormat/XmlIo.h>

#include <MaterialXFormat/File.h>

#include <MaterialXFormat/PugiXML/pugixml.hpp>

#include <MaterialXCore/Types.h>
#include <MaterialXCore/Util.h>

#include <fstream>
#include <sstream>
#include <string.h>

using namespace pugi;

namespace MaterialX
{

const string MTLX_EXTENSION = "mtlx";

namespace {

const string SOURCE_URI_ATTRIBUTE = "__sourceUri";
const string XINCLUDE_TAG = "xi:include";

void elementFromXml(const xml_node& xmlNode, ElementPtr elem, const XmlReadOptions* readOptions)
{
    bool skipDuplicateElements = readOptions && readOptions->skipDuplicateElements;

    // Store attributes in element.
    for (const xml_attribute& xmlAttr : xmlNode.attributes())
    {
        if (xmlAttr.name() == SOURCE_URI_ATTRIBUTE)
        {
            elem->setSourceUri(xmlAttr.value());
        }
        else if (xmlAttr.name() != Element::NAME_ATTRIBUTE)
        {
            elem->setAttribute(xmlAttr.name(), xmlAttr.value());
        }
    }

    // Create child elements and recurse.
    for (const xml_node& xmlChild : xmlNode.children())
    {
        string category = xmlChild.name();
        string name;
        for (const xml_attribute& xmlAttr : xmlChild.attributes())
        {
            if (xmlAttr.name() == Element::NAME_ATTRIBUTE)
            {
                name = xmlAttr.value();
                break;
            }
        }

        // If requested, skip elements with duplicate names.
        if (skipDuplicateElements && elem->getChild(name))
        {
            continue;
        }

        ElementPtr child = elem->addChildOfCategory(category, name);
        elementFromXml(xmlChild, child, readOptions);
    }
}

void createOGSProperty(xml_node& propertiesNode, xml_node& valuesNode,
            const std::string& name, 
            const std::string& type, 
            const std::string& value,
            const std::string& semantic,
            StringMap& typeMap)
{
    // Special case filename
    if (type == "filename")
    {
        xml_node txt = propertiesNode.append_child("texture2");
        txt.append_attribute("name") = name.c_str();
        xml_node samp = propertiesNode.append_child("sampler");
        samp.append_attribute("name") = (name + "_textureSampler").c_str();
    }
    // Q: How to handle geometry streams?
    else
    { 
        string ogsType = typeMap[type];
        if (!typeMap.count(type))
            return;

        xml_node prop = propertiesNode.append_child(ogsType.c_str());
        prop.append_attribute("name") = name.c_str();
        if (!semantic.empty())
        {
            prop.append_attribute("semantic") = semantic.c_str();
            prop.append_attribute("flags") = "varyingInputParam";
        }

        xml_node val = valuesNode.append_child(ogsType.c_str());
        val.append_attribute("name") = name.c_str();
        val.append_attribute("value") = value.c_str();
    }
}

// Creates output children on "outputs" node
void createOGSOutput(xml_node& outputsNode, 
    const std::string& name,
    const std::string& type,
    const std::string& /*value*/,
    StringMap& typeMap)
{
    if (!typeMap.count(type))
        return;

    string ogsType = typeMap[type];
    xml_node prop = outputsNode.append_child(ogsType.c_str());
    prop.append_attribute("name") = name.c_str();
}

void elementToXml(ConstElementPtr elem, xml_node& xmlNode, const XmlWriteOptions* writeOptions)
{
    bool writeXIncludeEnable = writeOptions ? writeOptions->writeXIncludeEnable : true;
    ElementPredicate elementPredicate = writeOptions ? writeOptions->elementPredicate : nullptr;

    // Store attributes in XML.
    if (!elem->getName().empty())
    {
        xmlNode.append_attribute(Element::NAME_ATTRIBUTE.c_str()) = elem->getName().c_str();
    }
    for (const string& attrName : elem->getAttributeNames())
    {
        xml_attribute xmlAttr = xmlNode.append_attribute(attrName.c_str());
        xmlAttr.set_value(elem->getAttribute(attrName).c_str());
    }

    // Create child nodes and recurse.
    StringSet writtenSourceFiles;
    for (ElementPtr child : elem->getChildren())
    {
        if (elementPredicate && !elementPredicate(child))
        {
            continue;
        }

        // Write XInclude references if requested.
        if (writeXIncludeEnable && child->hasSourceUri())
        {
            string sourceUri = child->getSourceUri();
            if (sourceUri != elem->getDocument()->getSourceUri())
            {
                if (!writtenSourceFiles.count(sourceUri))
                {
                    xml_node includeNode = xmlNode.append_child(XINCLUDE_TAG.c_str());
                    xml_attribute includeAttr = includeNode.append_attribute("href");
                    includeAttr.set_value(sourceUri.c_str());
                    writtenSourceFiles.insert(sourceUri);
                }
                continue;
            }
        }

        xml_node xmlChild = xmlNode.append_child(child->getCategory().c_str());
        elementToXml(child, xmlChild, writeOptions);
    }
}

void xmlDocumentFromFile(xml_document& xmlDoc, string filename, const string& searchPath)
{
    FileSearchPath fileSearchPath = FileSearchPath(searchPath);
    fileSearchPath.append(getEnvironmentPath());

    filename = fileSearchPath.find(filename);

    xml_parse_result result = xmlDoc.load_file(filename.c_str());
    if (!result)
    {
        if (result.status == xml_parse_status::status_file_not_found ||
            result.status == xml_parse_status::status_io_error ||
            result.status == xml_parse_status::status_out_of_memory)
        {
            throw ExceptionFileMissing("Failed to open file for reading: " + filename);
        }
        else
        {
            string desc = result.description();
            string offset = std::to_string(result.offset);
            throw ExceptionParseError("XML parse error in file: " + filename +
                                      " (" + desc + " at character " + offset + ")");
        }
    }
}

void processXIncludes(DocumentPtr doc, xml_node& xmlNode, const string& searchPath, const XmlReadOptions* readOptions)
{
    // Search path for includes. Set empty and then evaluated once in the iteration through xml includes.
    string includeSearchPath;

    XmlReadFunction readXIncludeFunction = readOptions ? readOptions->readXIncludeFunction : readFromXmlFile;
    xml_node xmlChild = xmlNode.first_child();
    while (xmlChild)
    {
        if (xmlChild.name() == XINCLUDE_TAG)
        {
            // Read XInclude references if requested.
            if (readXIncludeFunction)
            {
                string filename = xmlChild.attribute("href").value();

                // Check for XInclude cycles.
                if (readOptions)
                {
                    const StringVec& parents = readOptions->parentXIncludes;
                    if (std::find(parents.begin(), parents.end(), filename) != parents.end())
                    {
                        throw ExceptionParseError("XInclude cycle detected.");
                    }
                }

                // Read the included file into a library document.
                DocumentPtr library = createDocument();
                XmlReadOptions xiReadOptions = readOptions ? *readOptions : XmlReadOptions();
                xiReadOptions.parentXIncludes.push_back(filename);

                // Prepend the directory of the parent to accomodate
                // includes relative the the parent file location.
                if (includeSearchPath.empty())
                {
                    string parentUri = doc->getSourceUri();
                    if (!parentUri.empty())
                    {
                        FileSearchPath fileSearchPath(searchPath);
                        FilePath filePath = fileSearchPath.find(parentUri);
                        if (!filePath.isEmpty())
                        {
                            // Remove the file name from the path as we want the path to the containing folder.
                            filePath.pop();
                            includeSearchPath = filePath.asString() + PATH_LIST_SEPARATOR + searchPath;
                        }
                    }
                    // Set default search path if no parent path found
                    if (includeSearchPath.empty())
                    {
                        includeSearchPath = searchPath;
                    }
                }
                readXIncludeFunction(library, filename, includeSearchPath, &xiReadOptions);

                // Import the library document.
                doc->importLibrary(library, readOptions);
            }

            // Remove include directive.
            xml_node includeNode = xmlChild;
            xmlChild = xmlChild.next_sibling();
            xmlNode.remove_child(includeNode);
        }
        else
        {
            xmlChild = xmlChild.next_sibling();
        }
    }
}

void documentFromXml(DocumentPtr doc,
                     const xml_document& xmlDoc,
                     const string& searchPath = EMPTY_STRING,
                     const XmlReadOptions* readOptions = nullptr)
{
    ScopedUpdate update(doc);
    doc->onRead();

    xml_node xmlRoot = xmlDoc.child(Document::CATEGORY.c_str());
    if (xmlRoot)
    {
        processXIncludes(doc, xmlRoot, searchPath, readOptions);
        elementFromXml(xmlRoot, doc, readOptions);
    }

    doc->upgradeVersion();
}

} // anonymous namespace

//
// XmlReadOptions methods
//

XmlReadOptions::XmlReadOptions() :
    readXIncludeFunction(readFromXmlFile)
{
}

//
// XmlWriteOptions methods
//

XmlWriteOptions::XmlWriteOptions() :
    writeXIncludeEnable(true)
{
}

//
// Reading
//

void readFromXmlBuffer(DocumentPtr doc, const char* buffer, const XmlReadOptions* readOptions)
{
    xml_document xmlDoc;
    xml_parse_result result = xmlDoc.load_string(buffer);
    if (!result)
    {
        throw ExceptionParseError("Parse error in readFromXmlBuffer");
    }

    documentFromXml(doc, xmlDoc, EMPTY_STRING, readOptions);
}

void readFromXmlStream(DocumentPtr doc, std::istream& stream, const XmlReadOptions* readOptions)
{
    xml_document xmlDoc;
    xml_parse_result result = xmlDoc.load(stream);
    if (!result)
    {
        throw ExceptionParseError("Parse error in readFromXmlStream");
    }

    documentFromXml(doc, xmlDoc, EMPTY_STRING, readOptions);
}

void readFromXmlFile(DocumentPtr doc, const string& filename, const string& searchPath, const XmlReadOptions* readOptions)
{
    xml_document xmlDoc;
    xmlDocumentFromFile(xmlDoc, filename, searchPath);

    // This must be done before parsing the XML as the source URI
    // is used for searching for include files.
    if (readOptions && !readOptions->parentXIncludes.empty())
    {
        doc->setSourceUri(readOptions->parentXIncludes[0]);
    }
    else
    {
        doc->setSourceUri(filename);
    }
    documentFromXml(doc, xmlDoc, searchPath, readOptions);
}

void readFromXmlString(DocumentPtr doc, const string& str, const XmlReadOptions* readOptions)
{
    std::istringstream stream(str);
    readFromXmlStream(doc, stream, readOptions);
}

//
// Writing
//

void writeToXmlStream(DocumentPtr doc, std::ostream& stream, const XmlWriteOptions* writeOptions)
{
    ScopedUpdate update(doc);
    doc->onWrite();

    xml_document xmlDoc;
    xml_node xmlRoot = xmlDoc.append_child("materialx");
    elementToXml(doc, xmlRoot, writeOptions);
    xmlDoc.save(stream, "  ");
}

void writeToXmlFile(DocumentPtr doc, const string& filename, const XmlWriteOptions* writeOptions)
{
    std::ofstream ofs(filename);
    writeToXmlStream(doc, ofs, writeOptions);
}

string writeToXmlString(DocumentPtr doc, const XmlWriteOptions* writeOptions)
{
    std::ostringstream stream;
    writeToXmlStream(doc, stream, writeOptions);
    return stream.str();
}

void prependXInclude(DocumentPtr doc, const string& filename)
{
    ElementPtr elem = doc->addChildOfCategory("xinclude");
    elem->setSourceUri(filename);
    doc->setChildIndex(elem->getName(), 0);
}


void createOGSWrapper(NodePtr elem, StringMap& languageMap, std::ostream& stream)
{
    NodeDefPtr nodeDef = elem->getNodeDef();
    if (!nodeDef)
    {
        return;
    }

    // Make from MTLX to OGS types
    static StringMap typeMap;
    typeMap["boolean"] = "bool";
    typeMap["float"] = "float";
    typeMap["integer"] = "int";
    typeMap["string"] = "string";
    typeMap["matrix44"] = "float4x4";
    //typeMap["matrix33"] = There is no mapping for this. What to do?
    typeMap[MaterialX::TypedValue<MaterialX::Color2>::TYPE] = "float2";
    typeMap[MaterialX::TypedValue<MaterialX::Color3>::TYPE] = "color";
    typeMap[MaterialX::TypedValue<MaterialX::Color4>::TYPE] = "colorAlpha";
    typeMap[MaterialX::TypedValue<MaterialX::Vector2>::TYPE] = "float2";
    typeMap[MaterialX::TypedValue<MaterialX::Vector3>::TYPE] = "float3";
    typeMap[MaterialX::TypedValue<MaterialX::Vector4>::TYPE] = "float4";
    typeMap[MaterialX::TypedValue<MaterialX::Matrix33>::TYPE] = "float4";

    xml_document xmlDoc;
    const string OGS_FRAGMENT("fragment");
    const string OGS_PLUMBING("plumbing");
    const string OGS_SHADERFRAGMENT("ShadeFragment");
    const string OGS_VERSION_STRING("1.3.7");
    const string OGS_PROPERTIES("properties");
    const string OGS_VALUES("values");

    xml_node xmlRoot = xmlDoc.append_child(OGS_FRAGMENT.c_str());
    xmlRoot.append_attribute("name") = elem->getName().c_str();
    xmlRoot.append_attribute("type") = OGS_PLUMBING.c_str();
    xmlRoot.append_attribute("class") = OGS_SHADERFRAGMENT.c_str();
    xmlRoot.append_attribute("version") = OGS_VERSION_STRING.c_str();

    // Scan inputs and parameters and create "properties" and 
    // "values" children from the nodeDef
    string semantic;
    xml_node xmlProperties = xmlRoot.append_child(OGS_PROPERTIES.c_str());
    xml_node xmlValues = xmlRoot.append_child(OGS_VALUES.c_str());
    for (auto input : nodeDef->getInputs())
    {
        string value = input->getValue() ? input->getValue()->getValueString() : "";

        GeomPropDefPtr geomprop = input->getDefaultGeomProp();
        if (geomprop)
        {
            string geomNodeDefName = "ND_" + geomprop->getGeomProp() + "_" + input->getType();
            NodeDefPtr geomNodeDef = elem->getDocument()->getNodeDef(geomNodeDefName);
            if (geomNodeDef)
            {
                string geompropString = geomNodeDef->getAttribute("node");
                if (geompropString == "texcoord")
                {
                    semantic = "mayaUvCoordSemantic";
                }
            }
        }
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, semantic, typeMap);
    }
    for (auto input : nodeDef->getParameters())
    {
        string value = input->getValue() ? input->getValue()->getValueString() : "";
        createOGSProperty(xmlProperties, xmlValues,
            input->getName(), input->getType(), value, "", typeMap);
    }

    // Scan outputs and create "outputs"
    xml_node xmlOutputs = xmlRoot.append_child("outputs");
    for (auto output : elem->getActiveOutputs())
    {
        string value = output->getValue() ? output->getValue()->getValueString() : "";
        createOGSOutput(xmlOutputs,
            output->getName(), output->getType(), value, typeMap);
    }
    //std::ostream stream;
    //xmlDoc.save(stream, "  ");

    // Output implementations for different languages
    //InterfaceElementPtr impl = nodeDef->getImplementation(target, language);
    if (languageMap.empty())
    {
        languageMap["GLSL"] = "4.0";
        languageMap["OSL"] = "vanila";
    }
    xml_node impls = xmlRoot.append_child("implementation");
    for (auto l : languageMap)
    {
        // Need to get the actual code via shader generation.
        xml_node impl = impls.append_child("implementation");
        {
            impl.append_attribute("render") = "OGSRenderer";
            impl.append_attribute("language") = l.first.c_str();
            impl.append_attribute("lang_version") = l.second.c_str();
        }
        xml_node func = impl.append_child("function_name");
        {
            func.append_attribute("val") = ""; // TODO : need function name
        }
        xml_node source = impl.append_child("source");
        {
            // How to embedd the text data?
            source;
        }
    }

    xmlDoc.save(stream, "  ");
}


} // namespace MaterialX
