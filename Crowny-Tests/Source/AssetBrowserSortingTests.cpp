#include <catch2/catch_test_macros.hpp>

#include "Panels/AssetBrowserSorting.h"

using namespace Crowny;

namespace
{
    Ref<LibraryEntry> MakeEntry(const String& name, const Path& path, LibraryEntryType type = LibraryEntryType::File)
    {
        Ref<LibraryEntry> entry = CreateRef<LibraryEntry>();
        entry->ElementName = name;
        entry->Filepath = path;
        entry->Type = type;
        return entry;
    }
} // namespace

TEST_CASE("Asset browser name sorting puts folders first and handles case and prefixes", "[Editor][AssetBrowser][Sorting]")
{
    const Ref<LibraryEntry> zebraFolder = MakeEntry("Zebra", "Assets/Zebra", LibraryEntryType::Directory);
    const Ref<LibraryEntry> alphaFolder = MakeEntry("alpha", "Assets/alpha", LibraryEntryType::Directory);
    const Ref<LibraryEntry> zebra = MakeEntry("Zebra.png", "Assets/Zebra.png");
    const Ref<LibraryEntry> alpha = MakeEntry("alpha", "Assets/alpha.txt");
    const Ref<LibraryEntry> prefix = MakeEntry("Al", "Assets/Al");
    const Ref<LibraryEntry> alpine = MakeEntry("ALPINE", "Assets/ALPINE");
    Vector<Ref<LibraryEntry>> entries{ zebra, alpine, zebraFolder, alpha, prefix, alphaFolder };

    SortAssetBrowserEntriesByName(entries);

    const Vector<Ref<LibraryEntry>> expected{ alphaFolder, zebraFolder, prefix, alpha, alpine, zebra };
    CHECK(entries == expected);
}

TEST_CASE("Asset browser equal names sort deterministically across enumeration orders", "[Editor][AssetBrowser][Sorting]")
{
    const Vector<Ref<LibraryEntry>> expected{ MakeEntry("ALPHA", "Assets/C/ALPHA"), MakeEntry("alpha", "Assets/A/alpha"),
                                              MakeEntry("alpha", "Assets/B/alpha") };
    Array<size_t, 3> order{ 0, 1, 2 };
    do
    {
        Vector<Ref<LibraryEntry>> entries;
        for (const size_t index : order)
            entries.push_back(expected[index]);
        SortAssetBrowserEntriesByName(entries);
        CHECK(entries == expected);
    } while (std::next_permutation(order.begin(), order.end()));
}

TEST_CASE("Asset browser name ordering is strict for duplicate entries", "[Editor][AssetBrowser][Sorting]")
{
    const Ref<LibraryEntry> first = MakeEntry("alpha", "Assets/alpha");
    const Ref<LibraryEntry> duplicate = MakeEntry("alpha", "Assets/alpha");
    CHECK_FALSE(AssetBrowserEntryNameLess(*first, *first));
    CHECK_FALSE(AssetBrowserEntryNameLess(*first, *duplicate));
    CHECK_FALSE(AssetBrowserEntryNameLess(*duplicate, *first));

    const Ref<LibraryEntry> upperCase = MakeEntry("ALPHA", "Assets/ALPHA");
    CHECK(AssetBrowserEntryNameLess(*upperCase, *first));
    CHECK_FALSE(AssetBrowserEntryNameLess(*first, *upperCase));
}
