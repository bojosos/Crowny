#include <catch2/catch_test_macros.hpp>

#include "Editor/Script/ManagedProjectDependencies.h"

using namespace Crowny;

TEST_CASE("Managed project dependencies allow an empty declaration", "[Editor][Managed][Dependencies]")
{
    ManagedProjectDependencyRequest request;
    request.ProjectRoot = fs::current_path();

    const ManagedProjectDependencyPlan plan = ResolveManagedProjectDependencies(request);

    CHECK(plan.Succeeded());
    CHECK(plan.Assemblies.empty());
}

TEST_CASE("Managed project dependencies reject missing assemblies", "[Editor][Managed][Dependencies]")
{
    ManagedProjectDependencyRequest request;
    request.ProjectRoot = fs::current_path();
    request.DeclaredAssemblies = { "does-not-exist.dll" };

    const ManagedProjectDependencyPlan plan = ResolveManagedProjectDependencies(request);

    CHECK_FALSE(plan.Succeeded());
    REQUIRE(plan.Diagnostics.size() == 1);
    CHECK(plan.Diagnostics.front().Code == "MPD101");
}
