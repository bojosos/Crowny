#include "cwtpch.h"

#include "Panels/ScriptInspectorPath.h"

#include <fstream>

using namespace Crowny;

TEST_CASE("Script path settings resolve dynamic parents and extensions", "[Editor][Scripting][Path]")
{
    const Path projectRoot = fs::temp_directory_path() / "crowny-path-project";
    const ScriptValue root =
      ScriptValue::Object({ { "DynamicParent", ScriptValue::Text("Assets/Resources") }, { "DynamicExtensions", ScriptValue::Text(".cs, png") } });
    ScriptPathSettings settings;
    settings.ParentFolder = "$DynamicParent";
    settings.Extensions = "$DynamicExtensions";

    CHECK(ScriptInspectorPath::ResolveParentFolder(settings, root, projectRoot) == (projectRoot / "Assets/Resources").lexically_normal());
    const Vector<DialogFilter> filters = ScriptInspectorPath::Filters(settings, root);
    REQUIRE(filters.size() == 2);
    CHECK(filters[0].FilterSpec == "*.cs");
    CHECK(filters[1].FilterSpec == "*.png");

    const Path selected = projectRoot / "Assets/Resources/Scripts/Player.cs";
    CHECK(ScriptInspectorPath::Normalize(selected.string(), settings, root, projectRoot) == "Scripts/Player.cs");
    settings.AbsolutePath = true;
    CHECK(Path(ScriptInspectorPath::Normalize(selected.string(), settings, root, projectRoot)).lexically_normal() == selected.lexically_normal());
}

TEST_CASE("Script path settings normalize separators and omitted extensions", "[Editor][Scripting][Path]")
{
    const Path rootPath = fs::temp_directory_path() / ("crowny-script-path-test-" + UuidGenerator::Generate().ToString());
    struct Cleanup
    {
        Path Root;
        ~Cleanup()
        {
            std::error_code error;
            fs::remove_all(Root, error);
        }
    } cleanup{ rootPath };
    fs::create_directories(rootPath / "Content");
    std::ofstream(rootPath / "Content/notes.txt") << "test";

    const ScriptValue root = ScriptValue::Object({});
    ScriptPathSettings settings;
    settings.ParentFolder = "Content";
    settings.Extensions = "txt";
    settings.IncludeFileExtension = false;
    settings.UseBackslashes = true;

    CHECK(ScriptInspectorPath::Normalize((rootPath / "Content/notes.txt").string(), settings, root, rootPath) == "notes");
    CHECK(ScriptInspectorPath::Exists("notes", settings, root, rootPath));

    settings.Kind = ScriptPathKind::Folder;
    settings.UseBackslashes = false;
    CHECK(ScriptInspectorPath::Exists(".", settings, root, rootPath));
}
