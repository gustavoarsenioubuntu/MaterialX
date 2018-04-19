#include <MaterialXShaderGen/ShaderGenerators/Osl/OslSyntax.h>

namespace MaterialX
{

OslSyntax::OslSyntax()
{
    // Add in all restricted names and keywords in OSL
    addRestrictedNames(
    {
        "and", "break", "closure", "color", "continue", "do", "else", "emit", "float", "for", "if", "illuminance",
        "illuminate", "int", "matrix", "normal", "not", "or", "output", "point", "public", "return", "string",
        "struct", "vector", "void", "while",
        "bool", "case", "catch", "char", "class", "const", "delete", "default", "double", "enum", "extern",
        "false", "friend", "goto", "inline", "long", "new", "operator", "private", "protected", "short",
        "signed", "sizeof", "static", "switch", "template", "this", "throw", "true", "try", "typedef", "uniform",
        "union", "unsigned", "varying", "virtual", "volatile",
        "emission"
    });

    //
    // Add syntax information for each data type.
    //
    // TODO: Make this setup data driven (e.g read from a config file),
    //       to support new types without requiring a rebuild.
    //

    addTypeSyntax
    (
        DataType::FLOAT,
        TypeSyntax
        (
            "float",        // type name
            "0.0",          // default value
            "0.0",          // default value in a shader param initialization context
            "",             // custom type definition to add in source code
            "output float"  // type name in output context
        )
    );

    addTypeSyntax
    (
        DataType::INTEGER,
        TypeSyntax
        (
            "int", 
            "0", 
            "0",
            "",
            "output int" 
        )
    );

    addTypeSyntax
    (
        DataType::BOOLEAN,
        TypeSyntax
        (
            "int", 
            "0", 
            "0",
            "#define true 1\n#define false 0",
            "output int"
        )
    );

    addTypeSyntax
    (
        DataType::COLOR2,
        TypeSyntax
        (
            "color2", 
            "color2(0.0, 0.0)", 
            "color2(0.0, 0.0)",
            "",
            "output color2"
        )
    );

    addTypeSyntax
    (
        DataType::COLOR3,
        TypeSyntax
        (
            "color", 
            "color(0.0, 0.0, 0.0)", 
            "color(0.0, 0.0, 0.0)",
            "",
            "output color"
        )
    );

    addTypeSyntax
    (
        DataType::COLOR4,
        TypeSyntax
        (
            "color4",
            "color4(color(0.0), 0.0)",
            "color4(color(0.0), 0.0)",
            "color4 color4_pack(float r, float g, float b, float a) { return color4(color(r,g,b), a); }",
            "output color4"
        )
    );

    addTypeSyntax
    (
        DataType::VECTOR2,
        TypeSyntax
        (
            "vector2", 
            "vector2(0.0, 0.0)", 
            "vector2(0.0, 0.0)",
            "",
            "output vector2"
        )
    );

    addTypeSyntax
    (
        DataType::VECTOR3,
        TypeSyntax
        (
            "vector", 
            "vector(0.0, 0.0, 0.0)", 
            "vector(0.0, 0.0, 0.0)",
            "",
            "output vector"
        )
    );

    addTypeSyntax
    (
        DataType::VECTOR4,
        TypeSyntax
        (
            "vector4", 
            "vector4(0.0, 0.0, 0.0, 0.0)",
            "vector4(0.0, 0.0, 0.0, 0.0)",
            "",
            "output vector4"
        )
    );

    addTypeSyntax
    (
        DataType::MATRIX3,
        TypeSyntax
        (
            "matrix",
            "1",
            "1",
            "",
            "out matrix"
        )
    );

    addTypeSyntax
    (
        DataType::MATRIX4,
        TypeSyntax
        (
            "matrix",
            "1",
            "1",
            "",
            "out matrix"
        )
    );

    addTypeSyntax
    (
        DataType::STRING,
        TypeSyntax
        (
            "string", 
            "\"\"", 
            "\"\"",
            "",
            "output string"
        )
    );

    addTypeSyntax
    (
        DataType::FILENAME,
        TypeSyntax
        (
            "string", 
            "\"\"", 
            "\"\"",
            "",
            "output string"
        )
    );

    addTypeSyntax
    (
        DataType::BSDF,
        TypeSyntax
        (
            "BSDF", 
            "null_closure", 
            "0",
            "#define BSDF closure color",
            "output BSDF"
        )
    );

    addTypeSyntax
    (
        DataType::EDF,
        TypeSyntax
        (
            "EDF", 
            "null_closure", 
            "0",
            "#define EDF closure color",
            "output EDF"
        )
    );

    addTypeSyntax
    (
        DataType::VDF,
        TypeSyntax
        (
            "VDF", 
            "null_closure", 
            "0",
            "#define VDF closure color",
            "output VDF"
        )
    );

    addTypeSyntax
    (
        DataType::SURFACE,
        TypeSyntax
        (
            "surfaceshader", 
            "null_closure", 
            "0",
            "#define surfaceshader closure color",
            "output surfaceshader" 
        )
    );

    addTypeSyntax
    (
        DataType::VOLUME,
        TypeSyntax
        (
            "volumeshader", 
            "{0,0,0}", 
            "0",
            "struct volumeshader { VDF vdf; EDF edf; color absorption; };",
            "output volumeshader" 
        )
    );

    addTypeSyntax
    (
        DataType::DISPLACEMENT,
        TypeSyntax
        (
            "displacementshader", 
            "{0,0}", 
            "0",
            "struct displacementshader { vector offset; float scale; };",
            "output displacementshader"
        )
    );


    //
    // Add value constructor syntax for data types that needs this
    //

    addValueConstructSyntax(
        DataType::COLOR2,
        ValueConstructSyntax(
            "color2(", ")", // Value constructor syntax
            "color2(", ")", // Value constructor syntax in a shader param initialization context
            {".r", ".a"}    // Syntax for each vector component
        )
    );

    addValueConstructSyntax
    (
        DataType::COLOR3,
        ValueConstructSyntax
        (
            "color(", ")",
            "color(", ")",
            {"[0]", "[1]", "[2]"}
    )
    );

    addValueConstructSyntax
    (
        DataType::COLOR4,
        ValueConstructSyntax
        (
            "color4_pack(", ")",
            "color4_pack(", ")",
            {".rgb[0]", ".rgb[1]", ".rgb[2]", ".a"}
        )
    );

    addValueConstructSyntax(
        DataType::VECTOR2,
        ValueConstructSyntax
        (
            "vector2(", ")",
            "vector2(", ")",
            {".x", ".y"}
        )
    );

    addValueConstructSyntax
    (
        DataType::VECTOR3,
        ValueConstructSyntax
        (
            "vector(", ")",
            "vector(", ")",
            {"[0]", "[1]", "[2]"}
        )
    );

    addValueConstructSyntax
    (
        DataType::VECTOR4,
        ValueConstructSyntax
        (
            "vector4(", ")",
            "vector4(", ")",
            {".x", ".y", ".z", ".w"}
        )
    );

    addValueConstructSyntax
    (
        DataType::STRING,
        ValueConstructSyntax
        (
            "\"", "\"",
            "\"", "\"",
            {}
        )
    );

    addValueConstructSyntax
    (
        DataType::FILENAME,
        ValueConstructSyntax
        (
            "\"", "\"",
            "\"", "\"",
            {}
        )
    );
}

} // namespace MaterialX
