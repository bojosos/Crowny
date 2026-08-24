#include "cwpch.h"

#include "Crowny/Scripting/ManagedReload.h"

#include <cstdlib>
#include <system_error>

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#endif

namespace Crowny
{
    ManagedReloadDebouncer::ManagedReloadDebouncer(std::chrono::milliseconds quietPeriod) : m_QuietPeriod(quietPeriod) {}

    void ManagedReloadDebouncer::Notify(TimePoint now)
    {
        m_LastChange = now;
        m_Pending = true;
        ++m_Generation;
    }

    bool ManagedReloadDebouncer::TryBegin(TimePoint now)
    {
        if (!m_Pending || m_Building || now - m_LastChange < m_QuietPeriod)
            return false;

        m_Pending = false;
        m_Building = true;
        return true;
    }

    void ManagedReloadDebouncer::Complete() { m_Building = false; }

    bool MonoRuntimePaths::HasRuntime() const
    {
        return !LibraryDirectory.empty() && !EtcDirectory.empty() && fs::is_directory(LibraryDirectory) && fs::is_directory(EtcDirectory);
    }

    bool MonoRuntimePaths::HasCompiler() const { return !Compiler.empty() && fs::is_regular_file(Compiler); }

    static Path GetEnvironmentPath(const char* name)
    {
        const char* value = std::getenv(name);
        return value != nullptr && value[0] != '\0' ? Path(value) : Path();
    }

    static Path FirstExistingFile(const Vector<Path>& candidates)
    {
        for (const Path& candidate : candidates)
        {
            if (fs::is_regular_file(candidate))
                return candidate;
        }
        return {};
    }

    MonoRuntimePaths ResolveMonoRuntimePaths(const Vector<Path>& roots)
    {
        MonoRuntimePaths fallback;
        for (const Path& unnormalizedRoot : roots)
        {
            if (unnormalizedRoot.empty())
                continue;

            std::error_code ec;
            const Path root = fs::weakly_canonical(unnormalizedRoot, ec);
            const Path normalizedRoot = ec ? unnormalizedRoot.lexically_normal() : root;
            MonoRuntimePaths paths;
            paths.Root = normalizedRoot;
            paths.LibraryDirectory = normalizedRoot / "lib";
            paths.EtcDirectory = normalizedRoot / "etc";
            paths.Compiler = FirstExistingFile({ normalizedRoot / "bin/csc.bat", normalizedRoot / "bin/csc", normalizedRoot / "bin/mcs.bat",
                                                 normalizedRoot / "bin/mcs", normalizedRoot / "compiler/mcs" });

            if (fallback.Root.empty())
                fallback = paths;
            if (paths.HasRuntime())
                return paths;
        }
        return fallback;
    }

    MonoRuntimePaths ResolveMonoRuntimePaths(const Path& workingDirectory)
    {
        Vector<Path> roots;
        roots.push_back(GetEnvironmentPath("CROWNY_MONO_ROOT"));
        roots.push_back(GetEnvironmentPath("MONO_SDK"));
        roots.push_back(workingDirectory / "bin/Mono");
        roots.push_back(workingDirectory / "Dependencies/Mono");

#ifdef CW_PLATFORM_WIN32
        roots.emplace_back("C:/Program Files/Mono");
#elif defined(CW_MACOSX)
        roots.emplace_back("/Library/Frameworks/Mono.framework/Versions/Current");
        roots.emplace_back("/usr/local");
#else
        roots.emplace_back("/usr");
        roots.emplace_back("/usr/local");
#endif

#if !defined(CW_PLATFORM_WIN32) && !defined(CW_MACOSX)
        for (const Path& root : { Path("/usr"), Path("/usr/local") })
        {
            MonoRuntimePaths systemPaths;
            systemPaths.Root = root;
            systemPaths.LibraryDirectory = root / "lib";
            systemPaths.EtcDirectory = root == Path("/usr") ? Path("/etc") : root / "etc";
            systemPaths.Compiler = FirstExistingFile({ root / "bin/csc", root / "bin/mcs" });
            if (systemPaths.HasRuntime())
                return systemPaths;
        }
#endif

        MonoRuntimePaths paths = ResolveMonoRuntimePaths(roots);
        return paths;
    }

    static bool CopyAtomically(const Path& source, const Path& destination, String* error)
    {
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (ec)
        {
            if (error != nullptr)
                *error = "Could not create assembly directory: " + ec.message();
            return false;
        }

        Path temporary = destination;
        temporary += ".tmp";
        fs::copy_file(source, temporary, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            if (error != nullptr)
                *error = "Could not stage assembly: " + ec.message();
            return false;
        }

#ifdef CW_PLATFORM_WIN32
        if (!MoveFileExW(temporary.wstring().c_str(), destination.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
        fs::rename(temporary, destination, ec);
#endif
        if (ec)
        {
            fs::remove(temporary);
            if (error != nullptr)
                *error = "Could not publish assembly: " + ec.message();
            return false;
        }
        return true;
    }

    bool PublishManagedAssembly(const Path& stagedAssembly, const Path& destinationAssembly, String* error)
    {
        if (!fs::is_regular_file(stagedAssembly))
        {
            if (error != nullptr)
                *error = "Compiler did not produce " + stagedAssembly.string();
            return false;
        }

        if (!CopyAtomically(stagedAssembly, destinationAssembly, error))
            return false;

        const Path stagedPdb = Path(stagedAssembly).replace_extension("pdb");
        const Path destinationPdb = Path(destinationAssembly).replace_extension("pdb");
        if (fs::is_regular_file(stagedPdb) && !CopyAtomically(stagedPdb, destinationPdb, error))
            return false;
        if (!fs::is_regular_file(stagedPdb))
            fs::remove(destinationPdb);

        const Path stagedMdb = stagedAssembly.string() + ".mdb";
        const Path destinationMdb = destinationAssembly.string() + ".mdb";
        if (fs::is_regular_file(stagedMdb) && !CopyAtomically(stagedMdb, destinationMdb, error))
            return false;
        if (!fs::is_regular_file(stagedMdb))
            fs::remove(destinationMdb);
        return true;
    }
} // namespace Crowny
