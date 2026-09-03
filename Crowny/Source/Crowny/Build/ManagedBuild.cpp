#include "cwpch.h"

#include "Crowny/Build/ManagedBuild.h"

#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <limits>
#include <optional>
#include <type_traits>

#ifdef CW_PLATFORM_WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace Crowny
{
    namespace
    {
        constexpr uint32_t CLI_DIRECTORY_INDEX = 14;
        constexpr uint32_t CLI_IL_ONLY = 0x00000001;
        constexpr uint32_t MAX_MANAGED_FILE_SIZE = 512 * 1024 * 1024;
        constexpr uint32_t MAX_METADATA_ROWS = 16 * 1024 * 1024;
        constexpr uint32_t MAX_ASSEMBLY_REFERENCES = 8192;
        constexpr size_t MAX_ASSEMBLY_REFERENCE_STRINGS = 1024 * 1024;
        constexpr size_t MAX_METADATA_STRING_HEAP = 16 * 1024 * 1024;
        constexpr size_t MAX_PUBLIC_KEY_BYTES = 16 * 1024;

        struct ProcessResult
        {
            String StandardOutput;
            String StandardError;
            int ExitCode = -1;
            bool Started = false;
            bool TimedOut = false;
            bool Cancelled = false;
            bool OutputTruncated = false;
            String Error;
        };

        bool CancellationRequested(const std::function<bool()>& cancellation, std::exception_ptr& failure)
        {
            if (!cancellation)
                return false;
            try
            {
                return cancellation();
            }
            catch (...)
            {
                failure = std::current_exception();
                return true;
            }
        }

        void AppendCaptured(String& destination, const char* data, size_t size, size_t limit, bool& truncated)
        {
            const size_t available = destination.size() < limit ? limit - destination.size() : 0;
            const size_t copied = std::min(size, available);
            destination.append(data, copied);
            truncated |= copied != size;
        }

        void AddDiagnostic(Vector<ManagedBuildDiagnostic>& diagnostics, String code, String message, const Path& subject = {})
        {
            diagnostics.push_back({ std::move(code), std::move(message), subject });
        }

        String Lowercase(String value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        String Trim(String value)
        {
            const auto notWhitespace = [](unsigned char value) { return std::isspace(value) == 0; };
            const auto first = std::find_if(value.begin(), value.end(), notWhitespace);
            const auto last = std::find_if(value.rbegin(), value.rend(), notWhitespace).base();
            return first < last ? String(first, last) : String();
        }

        String PathArgument(const Path& path)
        {
#ifdef CW_PLATFORM_WIN32
            const WString value = path.wstring();
            if (value.empty())
                return {};
            const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (length <= 0)
                return {};
            String result(static_cast<size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
            return result;
#else
            return path.string();
#endif
        }

        Path NormalizePath(const Path& path)
        {
            std::error_code error;
            const Path absolute = fs::absolute(path, error);
            const Path normalized = error ? path.lexically_normal() : absolute.lexically_normal();
            error.clear();
            const Path canonical = fs::weakly_canonical(normalized, error);
            return error ? normalized : canonical;
        }

        Path ResolveProjectPath(const Path& projectRoot, const Path& path) { return NormalizePath(path.is_absolute() ? path : projectRoot / path); }

        String ComparablePath(const Path& path)
        {
            String value = NormalizePath(path).generic_string();
#ifdef CW_PLATFORM_WIN32
            value = Lowercase(std::move(value));
#endif
            return value;
        }

        bool IsWithin(const Path& root, const Path& path)
        {
            const String normalizedRoot = ComparablePath(root);
            const String normalizedPath = ComparablePath(path);
            if (normalizedPath == normalizedRoot)
                return true;
            const String prefix = normalizedRoot.ends_with('/') ? normalizedRoot : normalizedRoot + '/';
            return normalizedPath.starts_with(prefix);
        }

        bool ReadFile(const Path& path, Vector<uint8_t>& output)
        {
            std::error_code error;
            const uintmax_t size = fs::file_size(path, error);
            if (error || size > MAX_MANAGED_FILE_SIZE || size > std::numeric_limits<size_t>::max())
                return false;
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
                return false;
            output.resize(static_cast<size_t>(size));
            return output.empty() ||
                   static_cast<bool>(stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size())));
        }

        class Sha256Builder
        {
        public:
            Sha256Builder()
            {
                mbedtls_sha256_init(&m_Context);
                m_Valid = mbedtls_sha256_starts(&m_Context, false) == 0;
            }

            ~Sha256Builder() { mbedtls_sha256_free(&m_Context); }

            void Add(StringView value)
            {
                AddSize(value.size());
                if (m_Valid && !value.empty())
                    m_Valid = mbedtls_sha256_update(&m_Context, reinterpret_cast<const uint8_t*>(value.data()), value.size()) == 0;
            }

            bool AddFile(const Path& path)
            {
                std::ifstream stream(path, std::ios::binary);
                if (!stream)
                    return false;
                std::error_code error;
                const uintmax_t size = fs::file_size(path, error);
                if (error)
                    return false;
                AddSize(size);
                Array<uint8_t, 64 * 1024> buffer{};
                while (stream)
                {
                    stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
                    const std::streamsize count = stream.gcount();
                    if (count > 0 && m_Valid)
                        m_Valid = mbedtls_sha256_update(&m_Context, buffer.data(), static_cast<size_t>(count)) == 0;
                }
                return stream.eof() && m_Valid;
            }

            String Finish()
            {
                uint8_t digest[32]{};
                if (!m_Valid || mbedtls_sha256_finish(&m_Context, digest) != 0)
                    return {};
                static constexpr char digits[] = "0123456789abcdef";
                String result(sizeof(digest) * 2, '0');
                for (size_t index = 0; index < sizeof(digest); index++)
                {
                    result[index * 2] = digits[digest[index] >> 4];
                    result[index * 2 + 1] = digits[digest[index] & 0x0f];
                }
                return result;
            }

        private:
            void AddSize(uintmax_t size)
            {
                uint8_t bytes[8]{};
                for (size_t index = 0; index < sizeof(bytes); index++)
                    bytes[index] = static_cast<uint8_t>(size >> (index * 8));
                if (m_Valid)
                    m_Valid = mbedtls_sha256_update(&m_Context, bytes, sizeof(bytes)) == 0;
            }

            mbedtls_sha256_context m_Context{};
            bool m_Valid = false;
        };

#ifdef CW_PLATFORM_WIN32
        WString Utf8ToWide(StringView value)
        {
            if (value.empty())
                return {};
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (length <= 0)
                return {};
            WString result(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
            return result;
        }

        WString QuoteWindowsArgument(const WString& argument)
        {
            if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == WString::npos)
                return argument;
            WString quoted = L"\"";
            size_t backslashes = 0;
            for (wchar_t character : argument)
            {
                if (character == L'\\')
                {
                    ++backslashes;
                    continue;
                }
                if (character == L'\"')
                    quoted.append(backslashes * 2 + 1, L'\\');
                else
                    quoted.append(backslashes, L'\\');
                backslashes = 0;
                quoted.push_back(character);
            }
            quoted.append(backslashes * 2, L'\\');
            quoted.push_back(L'\"');
            return quoted;
        }

        void DrainHandle(HANDLE handle, String& destination, size_t limit, bool& truncated, bool& closed)
        {
            Array<char, 4096> buffer{};
            while (!closed)
            {
                DWORD available = 0;
                if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr))
                {
                    closed = true;
                    break;
                }
                if (available == 0)
                    break;
                DWORD count = 0;
                const DWORD requested = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
                if (!::ReadFile(handle, buffer.data(), requested, &count, nullptr) || count == 0)
                {
                    closed = true;
                    break;
                }
                AppendCaptured(destination, buffer.data(), count, limit, truncated);
            }
        }

        ProcessResult RunProcess(const Path& executable, const Vector<String>& arguments, std::chrono::milliseconds timeout,
                                 size_t maxCapturedOutputBytes, const std::function<bool()>& cancellation = {})
        {
            ProcessResult result;
            SECURITY_ATTRIBUTES security{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
            HANDLE stdoutRead = nullptr;
            HANDLE stdoutWrite = nullptr;
            HANDLE stderrRead = nullptr;
            HANDLE stderrWrite = nullptr;
            if (!CreatePipe(&stdoutRead, &stdoutWrite, &security, 0) || !CreatePipe(&stderrRead, &stderrWrite, &security, 0))
            {
                result.Error = "Could not create compiler output pipes.";
                if (stdoutRead)
                    CloseHandle(stdoutRead);
                if (stdoutWrite)
                    CloseHandle(stdoutWrite);
                if (stderrRead)
                    CloseHandle(stderrRead);
                if (stderrWrite)
                    CloseHandle(stderrWrite);
                return result;
            }
            SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

            const WString executableWide = executable.wstring();
            WString commandLine = QuoteWindowsArgument(executableWide);
            for (const String& argument : arguments)
            {
                commandLine.push_back(L' ');
                commandLine += QuoteWindowsArgument(Utf8ToWide(argument));
            }
            Vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
            mutableCommand.push_back(L'\0');
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESTDHANDLES;
            startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            startup.hStdOutput = stdoutWrite;
            startup.hStdError = stderrWrite;
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(executableWide.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                                &process))
            {
                const DWORD error = GetLastError();
                result.Error = "Could not start compiler, Windows error " + std::to_string(error) + ".";
                CloseHandle(stdoutRead);
                CloseHandle(stdoutWrite);
                CloseHandle(stderrRead);
                CloseHandle(stderrWrite);
                return result;
            }
            result.Started = true;
            CloseHandle(stdoutWrite);
            CloseHandle(stderrWrite);
            HANDLE job = CreateJobObjectW(nullptr, nullptr);
            if (job != nullptr)
            {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
                if (!AssignProcessToJobObject(job, process.hProcess))
                {
                    CloseHandle(job);
                    job = nullptr;
                }
            }
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            auto drainDeadline = deadline;
            bool processExited = false;
            bool stdoutClosed = false;
            bool stderrClosed = false;
            std::exception_ptr cancellationFailure;
            while (!processExited || !stdoutClosed || !stderrClosed)
            {
                DrainHandle(stdoutRead, result.StandardOutput, maxCapturedOutputBytes, result.OutputTruncated, stdoutClosed);
                DrainHandle(stderrRead, result.StandardError, maxCapturedOutputBytes, result.OutputTruncated, stderrClosed);
                if (!processExited && WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0)
                {
                    processExited = true;
                    drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
                const auto now = std::chrono::steady_clock::now();
                if (!processExited && CancellationRequested(cancellation, cancellationFailure))
                {
                    result.Cancelled = true;
                    if (job != nullptr)
                        TerminateJobObject(job, ERROR_CANCELLED);
                    else
                        TerminateProcess(process.hProcess, ERROR_CANCELLED);
                    WaitForSingleObject(process.hProcess, 5000);
                    processExited = true;
                    drainDeadline = now + std::chrono::seconds(1);
                }
                else if (!processExited && now >= deadline)
                {
                    result.TimedOut = true;
                    if (job != nullptr)
                        TerminateJobObject(job, ERROR_TIMEOUT);
                    else
                        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
                    WaitForSingleObject(process.hProcess, 5000);
                    processExited = true;
                    drainDeadline = now + std::chrono::seconds(1);
                }
                if (processExited && now >= drainDeadline)
                    break;
                if (!processExited || !stdoutClosed || !stderrClosed)
                {
                    if (processExited)
                        Sleep(5);
                    else
                        WaitForSingleObject(process.hProcess, 5);
                }
            }
            DrainHandle(stdoutRead, result.StandardOutput, maxCapturedOutputBytes, result.OutputTruncated, stdoutClosed);
            DrainHandle(stderrRead, result.StandardError, maxCapturedOutputBytes, result.OutputTruncated, stderrClosed);
            DWORD exitCode = 0;
            if (GetExitCodeProcess(process.hProcess, &exitCode))
                result.ExitCode = static_cast<int>(exitCode);
            CloseHandle(stdoutRead);
            CloseHandle(stderrRead);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            if (job != nullptr)
                CloseHandle(job);
            if (cancellationFailure)
                std::rethrow_exception(cancellationFailure);
            return result;
        }
#else
        void DrainDescriptor(int descriptor, String& destination, size_t limit, bool& truncated, bool& closed)
        {
            Array<char, 4096> buffer{};
            while (!closed)
            {
                const ssize_t count = read(descriptor, buffer.data(), buffer.size());
                if (count > 0)
                    AppendCaptured(destination, buffer.data(), static_cast<size_t>(count), limit, truncated);
                else if (count == 0)
                    closed = true;
                else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                    closed = true;
                else
                    break;
            }
        }

        ProcessResult RunProcess(const Path& executable, const Vector<String>& arguments, std::chrono::milliseconds timeout,
                                 size_t maxCapturedOutputBytes, const std::function<bool()>& cancellation = {})
        {
            ProcessResult result;
            int stdoutPipe[2]{ -1, -1 };
            int stderrPipe[2]{ -1, -1 };
            if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0)
            {
                result.Error = "Could not create compiler output pipes.";
                if (stdoutPipe[0] >= 0)
                    close(stdoutPipe[0]);
                if (stdoutPipe[1] >= 0)
                    close(stdoutPipe[1]);
                if (stderrPipe[0] >= 0)
                    close(stderrPipe[0]);
                if (stderrPipe[1] >= 0)
                    close(stderrPipe[1]);
                return result;
            }
            Vector<String> storage;
            storage.reserve(arguments.size() + 1);
            storage.push_back(executable.string());
            storage.insert(storage.end(), arguments.begin(), arguments.end());
            Vector<char*> argv;
            argv.reserve(storage.size() + 1);
            for (String& value : storage)
                argv.push_back(value.data());
            argv.push_back(nullptr);

            posix_spawn_file_actions_t actions;
            posix_spawnattr_t attributes;
            int setupError = posix_spawn_file_actions_init(&actions);
            const bool actionsInitialized = setupError == 0;
            if (setupError == 0)
                setupError = posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDOUT_FILENO);
            if (setupError == 0)
                setupError = posix_spawn_file_actions_adddup2(&actions, stderrPipe[1], STDERR_FILENO);
            if (setupError == 0)
                setupError = posix_spawn_file_actions_addclose(&actions, stdoutPipe[0]);
            if (setupError == 0)
                setupError = posix_spawn_file_actions_addclose(&actions, stderrPipe[0]);
            if (setupError == 0)
                setupError = posix_spawn_file_actions_addclose(&actions, stdoutPipe[1]);
            if (setupError == 0)
                setupError = posix_spawn_file_actions_addclose(&actions, stderrPipe[1]);
            bool attributesInitialized = false;
            if (setupError == 0)
            {
                setupError = posix_spawnattr_init(&attributes);
                attributesInitialized = setupError == 0;
            }
            if (setupError == 0)
                setupError = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
            if (setupError == 0)
                setupError = posix_spawnattr_setpgroup(&attributes, 0);
            pid_t child = -1;
            const int spawnError =
              setupError == 0 ? posix_spawn(&child, executable.c_str(), &actions, &attributes, argv.data(), ::environ) : setupError;
            if (attributesInitialized)
                posix_spawnattr_destroy(&attributes);
            if (actionsInitialized)
                posix_spawn_file_actions_destroy(&actions);
            if (spawnError != 0)
            {
                result.Error = "Could not start compiler, POSIX error " + std::to_string(spawnError) + ".";
                close(stdoutPipe[0]);
                close(stdoutPipe[1]);
                close(stderrPipe[0]);
                close(stderrPipe[1]);
                return result;
            }
            result.Started = true;
            close(stdoutPipe[1]);
            close(stderrPipe[1]);
            const int stdoutFlags = fcntl(stdoutPipe[0], F_GETFL);
            const int stderrFlags = fcntl(stderrPipe[0], F_GETFL);
            if (stdoutFlags < 0 || stderrFlags < 0 || fcntl(stdoutPipe[0], F_SETFL, stdoutFlags | O_NONBLOCK) != 0 ||
                fcntl(stderrPipe[0], F_SETFL, stderrFlags | O_NONBLOCK) != 0)
            {
                kill(-child, SIGKILL);
                while (waitpid(child, nullptr, 0) < 0 && errno == EINTR)
                {
                }
                close(stdoutPipe[0]);
                close(stderrPipe[0]);
                result.Error = "Could not configure compiler output pipes.";
                return result;
            }
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            auto drainDeadline = deadline;
            bool processExited = false;
            bool stdoutClosed = false;
            bool stderrClosed = false;
            int status = 0;
            std::exception_ptr cancellationFailure;
            while (!processExited || !stdoutClosed || !stderrClosed)
            {
                pollfd descriptors[2] = { { stdoutPipe[0], POLLIN | POLLHUP, 0 }, { stderrPipe[0], POLLIN | POLLHUP, 0 } };
                poll(descriptors, 2, 5);
                DrainDescriptor(stdoutPipe[0], result.StandardOutput, maxCapturedOutputBytes, result.OutputTruncated, stdoutClosed);
                DrainDescriptor(stderrPipe[0], result.StandardError, maxCapturedOutputBytes, result.OutputTruncated, stderrClosed);
                if (!processExited && waitpid(child, &status, WNOHANG) == child)
                {
                    processExited = true;
                    drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
                const auto now = std::chrono::steady_clock::now();
                if (!processExited && CancellationRequested(cancellation, cancellationFailure))
                {
                    result.Cancelled = true;
                    kill(-child, SIGTERM);
                    const auto terminateDeadline = now + std::chrono::milliseconds(250);
                    do
                    {
                        if (waitpid(child, &status, WNOHANG) == child)
                        {
                            processExited = true;
                            break;
                        }
                        poll(nullptr, 0, 10);
                    } while (std::chrono::steady_clock::now() < terminateDeadline);
                    if (!processExited)
                    {
                        kill(-child, SIGKILL);
                        while (waitpid(child, &status, 0) < 0 && errno == EINTR)
                        {
                        }
                        processExited = true;
                    }
                    drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
                else if (!processExited && now >= deadline)
                {
                    result.TimedOut = true;
                    kill(-child, SIGTERM);
                    const auto terminateDeadline = now + std::chrono::milliseconds(250);
                    do
                    {
                        if (waitpid(child, &status, WNOHANG) == child)
                        {
                            processExited = true;
                            break;
                        }
                        poll(nullptr, 0, 10);
                    } while (std::chrono::steady_clock::now() < terminateDeadline);
                    if (!processExited)
                    {
                        kill(-child, SIGKILL);
                        while (waitpid(child, &status, 0) < 0 && errno == EINTR)
                        {
                        }
                        processExited = true;
                    }
                    drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
                }
                if (processExited && now >= drainDeadline)
                    break;
            }
            DrainDescriptor(stdoutPipe[0], result.StandardOutput, maxCapturedOutputBytes, result.OutputTruncated, stdoutClosed);
            DrainDescriptor(stderrPipe[0], result.StandardError, maxCapturedOutputBytes, result.OutputTruncated, stderrClosed);
            if (WIFEXITED(status))
                result.ExitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                result.ExitCode = 128 + WTERMSIG(status);
            close(stdoutPipe[0]);
            close(stderrPipe[0]);
            if (cancellationFailure)
                std::rethrow_exception(cancellationFailure);
            return result;
        }
#endif

        template <class T> bool ReadInteger(const Vector<uint8_t>& data, size_t offset, T& output)
        {
            static_assert(std::is_unsigned_v<T>);
            if (offset > data.size() || sizeof(T) > data.size() - offset)
                return false;
            output = 0;
            for (size_t index = 0; index < sizeof(T); index++)
                output |= static_cast<T>(data[offset + index]) << (index * 8);
            return true;
        }

        struct PeSection
        {
            uint32_t VirtualAddress = 0;
            uint32_t VirtualSize = 0;
            uint32_t RawOffset = 0;
            uint32_t RawSize = 0;
        };

        std::optional<size_t> RvaToOffset(uint32_t rva, uint32_t size, const Vector<PeSection>& sections, size_t fileSize)
        {
            for (const PeSection& section : sections)
            {
                const uint32_t extent = std::max(section.VirtualSize, section.RawSize);
                if (rva < section.VirtualAddress || rva - section.VirtualAddress > extent)
                    continue;
                const uint32_t sectionOffset = rva - section.VirtualAddress;
                if (sectionOffset > section.RawSize || size > section.RawSize - sectionOffset)
                    continue;
                const uint64_t offset = static_cast<uint64_t>(section.RawOffset) + sectionOffset;
                if (offset <= fileSize && size <= fileSize - offset)
                    return static_cast<size_t>(offset);
            }
            return std::nullopt;
        }

        class MetadataTables
        {
        public:
            MetadataTables(const Vector<uint8_t>& data, size_t streamOffset, size_t streamSize, size_t stringsOffset, size_t stringsSize,
                           size_t blobsOffset, size_t blobsSize)
              : m_Data(data), m_StreamOffset(streamOffset), m_StreamSize(streamSize), m_StringsOffset(stringsOffset), m_StringsSize(stringsSize),
                m_BlobsOffset(blobsOffset), m_BlobsSize(blobsSize)
            {
            }

            bool Parse(String& error)
            {
                if (!Contains(m_StreamOffset, 24))
                    return Fail(error, "Metadata table header is truncated.");
                m_HeapSizes = m_Data[m_StreamOffset + 6];
                if (!ReadInteger(m_Data, m_StreamOffset + 8, m_Valid))
                    return Fail(error, "Metadata table mask is truncated.");
                size_t cursor = m_StreamOffset + 24;
                for (uint32_t table = 0; table < 64; table++)
                {
                    if ((m_Valid & (uint64_t(1) << table)) == 0)
                        continue;
                    if (!ReadInteger(m_Data, cursor, m_Rows[table]) || m_Rows[table] > MAX_METADATA_ROWS)
                        return Fail(error, "Metadata table row count is invalid.");
                    cursor += 4;
                }
                for (uint32_t table = 45; table < 64; table++)
                    if (m_Rows[table] != 0)
                        return Fail(error, "Unsupported metadata tables are present.");
                size_t offset = cursor;
                for (uint32_t table = 0; table <= 44; table++)
                {
                    m_TableOffsets[table] = offset;
                    const size_t rowSize = RowSize(table);
                    if (m_Rows[table] != 0 && rowSize == 0)
                        return Fail(error, "Unsupported metadata table layout.");
                    const uint64_t bytes = static_cast<uint64_t>(rowSize) * m_Rows[table];
                    if (bytes > std::numeric_limits<size_t>::max() || !Contains(offset, static_cast<size_t>(bytes)))
                        return Fail(error, "Metadata table data is truncated.");
                    offset += static_cast<size_t>(bytes);
                }
                return true;
            }

            uint32_t Rows(uint32_t table) const { return table < m_Rows.size() ? m_Rows[table] : 0; }

            bool ReadAssembly(ManagedAssemblyIdentity& identity, String& error) const
            {
                if (Rows(32) != 1)
                    return Fail(error, "Metadata must contain exactly one Assembly row.");
                const size_t row = m_TableOffsets[32];
                size_t cursor = row + 4;
                if (!ReadInteger(m_Data, cursor, identity.Major) || !ReadInteger(m_Data, cursor + 2, identity.Minor) ||
                    !ReadInteger(m_Data, cursor + 4, identity.Build) || !ReadInteger(m_Data, cursor + 6, identity.Revision))
                    return Fail(error, "Assembly identity is truncated.");
                uint32_t flags = 0;
                if (!ReadInteger(m_Data, row + 12, flags))
                    return Fail(error, "Assembly flags are truncated.");
                cursor = row + 16;
                uint32_t publicKeyIndex = 0, nameIndex = 0, cultureIndex = 0;
                Vector<uint8_t> publicKey;
                if (!ReadIndex(cursor, BlobIndexSize(), publicKeyIndex) || !ReadBlob(publicKeyIndex, publicKey) ||
                    !ReadIndex(cursor, StringIndexSize(), nameIndex) || !ReadString(nameIndex, identity.Name) || identity.Name.empty() ||
                    !ReadIndex(cursor, StringIndexSize(), cultureIndex) || !ReadString(cultureIndex, identity.Culture) ||
                    !SetPublicKeyToken(publicKey, (flags & 0x0001) != 0, identity.PublicKeyToken))
                    return Fail(error, "Assembly name is invalid.");
                if (identity.Name.size() > 1024 || identity.Culture.size() > 256)
                    return Fail(error, "Assembly identity strings exceed their limits.");
                return true;
            }

            bool ReadAssemblyReferences(Vector<ManagedAssemblyIdentity>& references, String& error) const
            {
                if (Rows(35) > MAX_ASSEMBLY_REFERENCES)
                    return Fail(error, "Assembly reference count exceeds its limit.");
                const size_t rowSize = RowSize(35);
                size_t stringBudget = 0;
                for (uint32_t row = 0; row < Rows(35); row++)
                {
                    ManagedAssemblyIdentity identity;
                    size_t cursor = m_TableOffsets[35] + static_cast<size_t>(row) * rowSize;
                    if (!ReadInteger(m_Data, cursor, identity.Major) || !ReadInteger(m_Data, cursor + 2, identity.Minor) ||
                        !ReadInteger(m_Data, cursor + 4, identity.Build) || !ReadInteger(m_Data, cursor + 6, identity.Revision))
                        return Fail(error, "Assembly reference version is truncated.");
                    uint32_t flags = 0;
                    if (!ReadInteger(m_Data, cursor + 8, flags))
                        return Fail(error, "Assembly reference flags are truncated.");
                    cursor += 12;
                    uint32_t publicKeyIndex = 0, nameIndex = 0, cultureIndex = 0, hashIndex = 0;
                    Vector<uint8_t> publicKey;
                    Vector<uint8_t> hash;
                    if (!ReadIndex(cursor, BlobIndexSize(), publicKeyIndex) || !ReadBlob(publicKeyIndex, publicKey) ||
                        !ReadIndex(cursor, StringIndexSize(), nameIndex) || !ReadString(nameIndex, identity.Name) || identity.Name.empty() ||
                        !ReadIndex(cursor, StringIndexSize(), cultureIndex) || !ReadString(cultureIndex, identity.Culture) ||
                        !ReadIndex(cursor, BlobIndexSize(), hashIndex) || !ReadBlob(hashIndex, hash) ||
                        !SetPublicKeyToken(publicKey, (flags & 0x0001) != 0, identity.PublicKeyToken))
                        return Fail(error, "Assembly reference name is invalid.");
                    stringBudget += identity.Name.size() + identity.Culture.size() + identity.PublicKeyToken.size();
                    if (identity.Name.size() > 1024 || identity.Culture.size() > 256 || stringBudget > MAX_ASSEMBLY_REFERENCE_STRINGS)
                        return Fail(error, "Assembly reference strings exceed their limits.");
                    references.push_back(std::move(identity));
                }
                std::sort(references.begin(), references.end(), [](const auto& left, const auto& right) {
                    return std::tie(left.Name, left.Major, left.Minor, left.Build, left.Revision, left.Culture, left.PublicKeyToken) <
                           std::tie(right.Name, right.Major, right.Minor, right.Build, right.Revision, right.Culture, right.PublicKeyToken);
                });
                references.erase(std::unique(references.begin(), references.end()), references.end());
                return true;
            }

        private:
            bool Contains(size_t offset, size_t count) const
            {
                const size_t end = m_StreamOffset + m_StreamSize;
                return offset >= m_StreamOffset && offset <= end && count <= end - offset && offset <= m_Data.size() &&
                       count <= m_Data.size() - offset;
            }

            static bool Fail(String& output, String message)
            {
                output = std::move(message);
                return false;
            }

            size_t TableIndexSize(uint32_t table) const { return m_Rows[table] < 0x10000 ? 2 : 4; }
            size_t StringIndexSize() const { return (m_HeapSizes & 0x01) != 0 ? 4 : 2; }
            size_t GuidIndexSize() const { return (m_HeapSizes & 0x02) != 0 ? 4 : 2; }
            size_t BlobIndexSize() const { return (m_HeapSizes & 0x04) != 0 ? 4 : 2; }

            size_t CodedIndexSize(std::initializer_list<uint32_t> tables, uint32_t tagBits) const
            {
                uint32_t largest = 0;
                for (uint32_t table : tables)
                    largest = std::max(largest, m_Rows[table]);
                return largest < (uint32_t(1) << (16 - tagBits)) ? 2 : 4;
            }

            size_t RowSize(uint32_t table) const
            {
                const size_t str = StringIndexSize(), guid = GuidIndexSize(), blob = BlobIndexSize();
                const size_t typeDefOrRef = CodedIndexSize({ 2, 1, 27 }, 2);
                const size_t hasConstant = CodedIndexSize({ 4, 8, 23 }, 2);
                const size_t hasCustomAttribute =
                  CodedIndexSize({ 6, 4, 1, 2, 8, 9, 10, 0, 14, 23, 20, 17, 26, 27, 32, 35, 38, 39, 40, 42, 43, 44 }, 5);
                const size_t hasFieldMarshal = CodedIndexSize({ 4, 8 }, 1);
                const size_t hasDeclSecurity = CodedIndexSize({ 2, 6, 32 }, 2);
                const size_t memberRefParent = CodedIndexSize({ 2, 1, 26, 6, 27 }, 3);
                const size_t hasSemantics = CodedIndexSize({ 20, 23 }, 1);
                const size_t methodDefOrRef = CodedIndexSize({ 6, 10 }, 1);
                const size_t memberForwarded = CodedIndexSize({ 4, 6 }, 1);
                const size_t implementation = CodedIndexSize({ 38, 35, 39 }, 2);
                const size_t customAttributeType = CodedIndexSize({ 6, 10 }, 3);
                const size_t resolutionScope = CodedIndexSize({ 0, 26, 35, 1 }, 2);
                const size_t typeOrMethodDef = CodedIndexSize({ 2, 6 }, 1);
                switch (table)
                {
                case 0:
                    return 2 + str + guid * 3;
                case 1:
                    return resolutionScope + str * 2;
                case 2:
                    return 4 + str * 2 + typeDefOrRef + TableIndexSize(4) + TableIndexSize(6);
                case 3:
                    return TableIndexSize(4);
                case 4:
                    return 2 + str + blob;
                case 5:
                    return TableIndexSize(6);
                case 6:
                    return 8 + str + blob + TableIndexSize(8);
                case 7:
                    return TableIndexSize(8);
                case 8:
                    return 4 + str;
                case 9:
                    return TableIndexSize(2) + typeDefOrRef;
                case 10:
                    return memberRefParent + str + blob;
                case 11:
                    return 2 + hasConstant + blob;
                case 12:
                    return hasCustomAttribute + customAttributeType + blob;
                case 13:
                    return hasFieldMarshal + blob;
                case 14:
                    return 2 + hasDeclSecurity + blob;
                case 15:
                    return 6 + TableIndexSize(2);
                case 16:
                    return 4 + TableIndexSize(4);
                case 17:
                    return blob;
                case 18:
                    return TableIndexSize(2) + TableIndexSize(20);
                case 19:
                    return TableIndexSize(20);
                case 20:
                    return 2 + str + typeDefOrRef;
                case 21:
                    return TableIndexSize(2) + TableIndexSize(23);
                case 22:
                    return TableIndexSize(23);
                case 23:
                    return 2 + str + blob;
                case 24:
                    return 2 + TableIndexSize(6) + hasSemantics;
                case 25:
                    return TableIndexSize(2) + methodDefOrRef * 2;
                case 26:
                    return str;
                case 27:
                    return blob;
                case 28:
                    return 2 + memberForwarded + str + TableIndexSize(26);
                case 29:
                    return 4 + TableIndexSize(4);
                case 30:
                    return 8;
                case 31:
                    return 4;
                case 32:
                    return 16 + blob + str * 2;
                case 33:
                    return 4;
                case 34:
                    return 12;
                case 35:
                    return 12 + blob * 2 + str * 2;
                case 36:
                    return 4 + TableIndexSize(35);
                case 37:
                    return 12 + TableIndexSize(35);
                case 38:
                    return 4 + str + blob;
                case 39:
                    return 8 + str * 2 + implementation;
                case 40:
                    return 8 + str + implementation;
                case 41:
                    return TableIndexSize(2) * 2;
                case 42:
                    return 4 + typeOrMethodDef + str;
                case 43:
                    return methodDefOrRef + blob;
                case 44:
                    return TableIndexSize(42) + typeDefOrRef;
                default:
                    return 0;
                }
            }

            bool ReadIndex(size_t& cursor, size_t size, uint32_t& output) const
            {
                if (size == 2)
                {
                    uint16_t value = 0;
                    if (!ReadInteger(m_Data, cursor, value))
                        return false;
                    output = value;
                }
                else if (!ReadInteger(m_Data, cursor, output))
                    return false;
                cursor += size;
                return true;
            }

            bool ReadString(uint32_t index, String& output) const
            {
                if (index >= m_StringsSize)
                    return false;
                const size_t start = m_StringsOffset + index;
                const size_t limit = m_StringsOffset + m_StringsSize;
                size_t end = start;
                while (end < limit && m_Data[end] != 0)
                    ++end;
                if (end == limit)
                    return false;
                output.assign(reinterpret_cast<const char*>(m_Data.data() + start), end - start);
                return true;
            }

            bool ReadBlob(uint32_t index, Vector<uint8_t>& output) const
            {
                output.clear();
                if (index == 0)
                    return true;
                if (index >= m_BlobsSize)
                    return false;
                size_t cursor = m_BlobsOffset + index;
                const size_t limit = m_BlobsOffset + m_BlobsSize;
                if (cursor >= limit)
                    return false;
                uint32_t size = 0;
                const uint8_t first = m_Data[cursor++];
                if ((first & 0x80) == 0)
                    size = first;
                else if ((first & 0xc0) == 0x80)
                {
                    if (cursor >= limit)
                        return false;
                    size = (static_cast<uint32_t>(first & 0x3f) << 8) | m_Data[cursor++];
                }
                else if ((first & 0xe0) == 0xc0)
                {
                    if (limit - cursor < 3)
                        return false;
                    size = (static_cast<uint32_t>(first & 0x1f) << 24) | (static_cast<uint32_t>(m_Data[cursor]) << 16) |
                           (static_cast<uint32_t>(m_Data[cursor + 1]) << 8) | m_Data[cursor + 2];
                    cursor += 3;
                }
                else
                    return false;
                if (size > MAX_PUBLIC_KEY_BYTES || cursor > limit || size > limit - cursor)
                    return false;
                output.assign(m_Data.begin() + static_cast<ptrdiff_t>(cursor), m_Data.begin() + static_cast<ptrdiff_t>(cursor + size));
                return true;
            }

            static bool SetPublicKeyToken(const Vector<uint8_t>& key, bool fullKey, String& output)
            {
                output.clear();
                if (key.empty())
                    return true;
                Array<uint8_t, 8> token{};
                if (fullKey)
                {
                    uint8_t digest[20]{};
                    if (mbedtls_sha1(key.data(), key.size(), digest) != 0)
                        return false;
                    for (size_t index = 0; index < token.size(); index++)
                        token[index] = digest[sizeof(digest) - 1 - index];
                }
                else
                {
                    if (key.size() != token.size())
                        return false;
                    std::copy(key.begin(), key.end(), token.begin());
                }
                static constexpr char digits[] = "0123456789abcdef";
                output.resize(token.size() * 2);
                for (size_t index = 0; index < token.size(); index++)
                {
                    output[index * 2] = digits[token[index] >> 4];
                    output[index * 2 + 1] = digits[token[index] & 0x0f];
                }
                return true;
            }

            const Vector<uint8_t>& m_Data;
            size_t m_StreamOffset = 0;
            size_t m_StreamSize = 0;
            size_t m_StringsOffset = 0;
            size_t m_StringsSize = 0;
            size_t m_BlobsOffset = 0;
            size_t m_BlobsSize = 0;
            uint8_t m_HeapSizes = 0;
            uint64_t m_Valid = 0;
            Array<uint32_t, 64> m_Rows{};
            Array<size_t, 45> m_TableOffsets{};
        };

        bool ParseMetadata(const Vector<uint8_t>& data, size_t metadataOffset, size_t metadataSize, ManagedAssemblyInspection& inspection,
                           String& error)
        {
            uint32_t signature = 0;
            uint32_t versionLength = 0;
            if (metadataSize < 20 || metadataOffset > data.size() || metadataSize > data.size() - metadataOffset ||
                !ReadInteger(data, metadataOffset, signature) || signature != 0x424a5342 || !ReadInteger(data, metadataOffset + 12, versionLength) ||
                versionLength == 0 || versionLength > metadataSize - 16)
            {
                error = "CLI metadata root is invalid.";
                return false;
            }
            size_t cursor = metadataOffset + 16 + versionLength;
            cursor = (cursor + 3) & ~size_t(3);
            const size_t metadataEnd = metadataOffset + metadataSize;
            uint16_t streamCount = 0;
            if (cursor > metadataEnd || metadataEnd - cursor < 4 || !ReadInteger(data, cursor + 2, streamCount) || streamCount == 0 ||
                streamCount > 64)
            {
                error = "CLI metadata stream header is invalid.";
                return false;
            }
            cursor += 4;
            size_t tablesOffset = 0, tablesSize = 0, stringsOffset = 0, stringsSize = 0, blobsOffset = 0, blobsSize = 0;
            for (uint16_t index = 0; index < streamCount; index++)
            {
                uint32_t relativeOffset = 0, size = 0;
                if (cursor > metadataEnd || metadataEnd - cursor < 8 || !ReadInteger(data, cursor, relativeOffset) ||
                    !ReadInteger(data, cursor + 4, size))
                {
                    error = "CLI metadata stream is truncated.";
                    return false;
                }
                cursor += 8;
                const size_t nameStart = cursor;
                while (cursor < metadataEnd && cursor - nameStart < 32 && data[cursor] != 0)
                    ++cursor;
                if (cursor >= metadataEnd || cursor - nameStart >= 32)
                {
                    error = "CLI metadata stream name is invalid.";
                    return false;
                }
                const String name(reinterpret_cast<const char*>(data.data() + nameStart), cursor - nameStart);
                cursor = (cursor + 4) & ~size_t(3);
                if (cursor > metadataEnd)
                {
                    error = "CLI metadata stream header exceeds the metadata directory.";
                    return false;
                }
                if (relativeOffset > metadataSize || size > metadataSize - relativeOffset)
                {
                    error = "CLI metadata stream exceeds its metadata directory.";
                    return false;
                }
                if (name == "#~" || name == "#-")
                {
                    tablesOffset = metadataOffset + relativeOffset;
                    tablesSize = size;
                }
                else if (name == "#Strings")
                {
                    stringsOffset = metadataOffset + relativeOffset;
                    stringsSize = size;
                }
                else if (name == "#Blob")
                {
                    blobsOffset = metadataOffset + relativeOffset;
                    blobsSize = size;
                }
            }
            if (tablesOffset == 0 || stringsOffset == 0 || blobsOffset == 0 || stringsSize > MAX_METADATA_STRING_HEAP)
            {
                error = "CLI metadata lacks required streams or exceeds the string-heap limit.";
                return false;
            }
            MetadataTables tables(data, tablesOffset, tablesSize, stringsOffset, stringsSize, blobsOffset, blobsSize);
            if (!tables.Parse(error) || !tables.ReadAssembly(inspection.Identity, error) ||
                !tables.ReadAssemblyReferences(inspection.References, error))
                return false;
            inspection.HasPInvoke = tables.Rows(28) != 0;
            return true;
        }

        Vector<Path> SortedUniquePaths(const Vector<Path>& paths)
        {
            Vector<Path> result;
            result.reserve(paths.size());
            for (const Path& path : paths)
                result.push_back(NormalizePath(path));
            std::sort(result.begin(), result.end(), [](const Path& left, const Path& right) { return ComparablePath(left) < ComparablePath(right); });
            result.erase(std::unique(result.begin(), result.end(),
                                     [](const Path& left, const Path& right) { return ComparablePath(left) == ComparablePath(right); }),
                         result.end());
            return result;
        }

        bool IsValidSymbol(StringView symbol)
        {
            if (symbol.empty() || !(std::isalpha(static_cast<unsigned char>(symbol.front())) || symbol.front() == '_'))
                return false;
            return std::all_of(symbol.begin() + 1, symbol.end(), [](unsigned char value) { return std::isalnum(value) || value == '_'; });
        }

        Path FindOnPath(StringView filename)
        {
            const char* pathValue = std::getenv("PATH");
            if (pathValue == nullptr)
                return {};
#ifdef CW_PLATFORM_WIN32
            constexpr char separator = ';';
#else
            constexpr char separator = ':';
#endif
            String paths(pathValue);
            size_t start = 0;
            while (start <= paths.size())
            {
                const size_t end = paths.find(separator, start);
                const Path candidate = Path(paths.substr(start, end - start)) / filename;
                if (fs::is_regular_file(candidate))
                    return NormalizePath(candidate);
                if (end == String::npos)
                    break;
                start = end + 1;
            }
            return {};
        }

        Path FindCompiler(const Path& root)
        {
            Vector<Path> candidates;
            std::error_code error;
            Vector<Path> searchRoots = { root / "bin", root / "lib/mono", root / "compiler" };
            for (const Path& searchRoot : searchRoots)
            {
                if (!fs::is_directory(searchRoot))
                    continue;
                for (fs::recursive_directory_iterator iterator(searchRoot, fs::directory_options::skip_permission_denied, error), end;
                     iterator != end; iterator.increment(error))
                {
                    if (error)
                    {
                        error.clear();
                        continue;
                    }
                    if (!iterator->is_regular_file(error))
                        continue;
                    const String name = Lowercase(iterator->path().filename().string());
                    if (name == "csc.exe" || name == "mcs.exe")
                        candidates.push_back(iterator->path());
                }
            }
            std::sort(candidates.begin(), candidates.end(), [](const Path& left, const Path& right) {
                const auto score = [](const Path& path) {
                    const String value = Lowercase(path.generic_string());
                    return std::make_tuple(value.find("/current/") == String::npos, value.find("/roslyn/") == String::npos,
                                           path.filename() != Path("csc.exe"), value);
                };
                return score(left) < score(right);
            });
            return candidates.empty() ? Path() : NormalizePath(candidates.front());
        }

        Path FindReferenceDirectory(const Path& root)
        {
            Vector<Path> candidates;
            std::error_code error;
            Vector<Path> searchRoots = { root / "lib/mono", root / "Reference Assemblies" };
            for (const Path& searchRoot : searchRoots)
            {
                if (!fs::is_directory(searchRoot))
                    continue;
                for (fs::recursive_directory_iterator iterator(searchRoot, fs::directory_options::skip_permission_denied, error), end;
                     iterator != end; iterator.increment(error))
                {
                    if (error)
                    {
                        error.clear();
                        continue;
                    }
                    if (iterator->is_regular_file(error) && Lowercase(iterator->path().filename().string()) == "mscorlib.dll")
                        candidates.push_back(iterator->path().parent_path());
                }
            }
            std::sort(candidates.begin(), candidates.end(), [](const Path& left, const Path& right) {
                const String leftName = Lowercase(left.filename().string());
                const String rightName = Lowercase(right.filename().string());
                const bool leftApi = leftName.ends_with("-api");
                const bool rightApi = rightName.ends_with("-api");
                return leftApi != rightApi ? leftApi : leftName > rightName;
            });
            return candidates.empty() ? Path() : NormalizePath(candidates.front());
        }

        Path MakeStagingAssemblyPath(const Path& output)
        {
            static std::atomic<uint64_t> sequence{ 0 };
#ifdef CW_PLATFORM_WIN32
            const uint64_t process = GetCurrentProcessId();
#else
            const uint64_t process = static_cast<uint64_t>(getpid());
#endif
            const String directory =
              ".crowny-managed-staging-" + std::to_string(process) + "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            return output.parent_path() / directory / output.filename();
        }

        bool ReplaceAtomically(const Path& source, const Path& destination, String& errorMessage)
        {
#ifdef CW_PLATFORM_WIN32
            if (!MoveFileExW(source.wstring().c_str(), destination.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                errorMessage = "Windows error " + std::to_string(GetLastError()) + ".";
                return false;
            }
#else
            std::error_code error;
            fs::rename(source, destination, error);
            if (error)
            {
                errorMessage = error.message();
                return false;
            }
#endif
            return true;
        }

        void RemoveBuildOutput(const Path& assembly)
        {
            std::error_code error;
            fs::remove(assembly, error);
            Path pdb = assembly;
            pdb.replace_extension(".pdb");
            fs::remove(pdb, error);
            fs::remove(Path(assembly.string() + ".mdb"), error);
        }

        void RemoveStagingOutput(const Path& assembly)
        {
            RemoveBuildOutput(assembly);
            std::error_code error;
            fs::remove_all(assembly.parent_path(), error);
        }

        String IdentityKey(const ManagedAssemblyIdentity& identity)
        {
            return Lowercase(identity.Name) + "," + std::to_string(identity.Major) + "." + std::to_string(identity.Minor) + "." +
                   std::to_string(identity.Build) + "." + std::to_string(identity.Revision) + "," + Lowercase(identity.Culture) + "," +
                   Lowercase(identity.PublicKeyToken);
        }
    } // namespace

    String ManagedAssemblyIdentity::ToString() const
    {
        return Name + ", Version=" + std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Build) + "." +
               std::to_string(Revision) + ", Culture=" + (Culture.empty() ? "neutral" : Culture) +
               ", PublicKeyToken=" + (PublicKeyToken.empty() ? "null" : PublicKeyToken);
    }

    ManagedAssemblyInspection InspectManagedAssembly(const Path& assembly)
    {
        ManagedAssemblyInspection inspection;
        inspection.File = NormalizePath(assembly);
        Vector<uint8_t> data;
        if (!fs::is_regular_file(inspection.File) || !ReadFile(inspection.File, data))
        {
            AddDiagnostic(inspection.Diagnostics, "MB200", "Managed assembly cannot be read.", inspection.File);
            return inspection;
        }
        uint16_t dosMagic = 0;
        uint32_t peOffset = 0;
        if (!ReadInteger(data, 0, dosMagic) || dosMagic != 0x5a4d || !ReadInteger(data, 0x3c, peOffset))
        {
            AddDiagnostic(inspection.Diagnostics, "MB201", "File is not a PE image.", inspection.File);
            return inspection;
        }
        uint32_t peSignature = 0;
        uint16_t sectionCount = 0, optionalSize = 0, optionalMagic = 0;
        if (!ReadInteger(data, peOffset, peSignature) || peSignature != 0x00004550 || !ReadInteger(data, peOffset + 6, sectionCount) ||
            sectionCount == 0 || sectionCount > 96 || !ReadInteger(data, peOffset + 20, optionalSize) ||
            !ReadInteger(data, peOffset + 24, optionalMagic))
        {
            AddDiagnostic(inspection.Diagnostics, "MB202", "PE header is invalid.", inspection.File);
            return inspection;
        }
        const size_t optionalOffset = static_cast<size_t>(peOffset) + 24;
        const size_t dataDirectoryOffset = optionalOffset + (optionalMagic == 0x10b ? 96 : optionalMagic == 0x20b ? 112 : 0);
        const size_t minimumOptionalSize = dataDirectoryOffset - optionalOffset + (CLI_DIRECTORY_INDEX + 1) * 8;
        uint32_t cliRva = 0, cliSize = 0;
        if (dataDirectoryOffset == optionalOffset || optionalSize < minimumOptionalSize ||
            !ReadInteger(data, dataDirectoryOffset + CLI_DIRECTORY_INDEX * 8, cliRva) ||
            !ReadInteger(data, dataDirectoryOffset + CLI_DIRECTORY_INDEX * 8 + 4, cliSize) || cliRva == 0 || cliSize < 24)
        {
            AddDiagnostic(inspection.Diagnostics, "MB203", "PE image has no supported CLI header.", inspection.File);
            return inspection;
        }
        Vector<PeSection> sections;
        size_t sectionOffset = optionalOffset + optionalSize;
        for (uint16_t index = 0; index < sectionCount; index++, sectionOffset += 40)
        {
            PeSection section;
            if (!ReadInteger(data, sectionOffset + 8, section.VirtualSize) || !ReadInteger(data, sectionOffset + 12, section.VirtualAddress) ||
                !ReadInteger(data, sectionOffset + 16, section.RawSize) || !ReadInteger(data, sectionOffset + 20, section.RawOffset))
            {
                AddDiagnostic(inspection.Diagnostics, "MB204", "PE section table is truncated.", inspection.File);
                return inspection;
            }
            sections.push_back(section);
        }
        const std::optional<size_t> cliOffset = RvaToOffset(cliRva, cliSize, sections, data.size());
        uint32_t metadataRva = 0, metadataSize = 0, flags = 0;
        if (!cliOffset || !ReadInteger(data, *cliOffset + 8, metadataRva) || !ReadInteger(data, *cliOffset + 12, metadataSize) ||
            !ReadInteger(data, *cliOffset + 16, flags) || metadataRva == 0 || metadataSize == 0)
        {
            AddDiagnostic(inspection.Diagnostics, "MB205", "CLI header is truncated.", inspection.File);
            return inspection;
        }
        inspection.IsILOnly = (flags & CLI_IL_ONLY) != 0;
        const std::optional<size_t> metadataOffset = RvaToOffset(metadataRva, metadataSize, sections, data.size());
        if (!metadataOffset)
        {
            AddDiagnostic(inspection.Diagnostics, "MB206", "CLI metadata directory is outside the PE image.", inspection.File);
            return inspection;
        }
        String error;
        if (!ParseMetadata(data, *metadataOffset, metadataSize, inspection, error))
            AddDiagnostic(inspection.Diagnostics, "MB207", std::move(error), inspection.File);
        return inspection;
    }

    ManagedDependencyResult ResolveManagedDependencyClosure(const ManagedDependencyRequest& request)
    {
        ManagedDependencyResult result;
        const Vector<Path> roots = SortedUniquePaths(request.Roots);
        if (roots.empty())
        {
            AddDiagnostic(result.Diagnostics, "MB300", "Dependency closure needs at least one root assembly.");
            return result;
        }

        const Vector<Path> frameworkDirectories = SortedUniquePaths(request.FrameworkDirectories);
        Vector<Path> searchDirectories = request.SearchDirectories;
        searchDirectories.insert(searchDirectories.end(), frameworkDirectories.begin(), frameworkDirectories.end());
        Vector<Path> candidateFiles;
        for (const Path& directory : SortedUniquePaths(searchDirectories))
        {
            if (!fs::is_directory(directory))
            {
                AddDiagnostic(result.Diagnostics, "MB301", "Dependency search directory does not exist.", directory);
                continue;
            }
            std::error_code error;
            for (const fs::directory_entry& entry : fs::directory_iterator(directory, error))
            {
                if (error)
                    break;
                const String extension = Lowercase(entry.path().extension().string());
                if (entry.is_regular_file(error) && (extension == ".dll" || extension == ".exe"))
                    candidateFiles.push_back(entry.path());
            }
        }
        candidateFiles.insert(candidateFiles.end(), roots.begin(), roots.end());
        candidateFiles = SortedUniquePaths(candidateFiles);

        Map<String, Vector<ManagedAssemblyInspection>> candidates;
        for (const Path& file : candidateFiles)
        {
            ManagedAssemblyInspection inspection = InspectManagedAssembly(file);
            if (inspection.Diagnostics.empty())
                candidates[IdentityKey(inspection.Identity)].push_back(std::move(inspection));
        }

        Vector<ManagedAssemblyInspection> queue;
        for (const Path& root : roots)
        {
            ManagedAssemblyInspection inspection = InspectManagedAssembly(root);
            if (!inspection.Diagnostics.empty())
            {
                result.Diagnostics.insert(result.Diagnostics.end(), inspection.Diagnostics.begin(), inspection.Diagnostics.end());
                continue;
            }
            queue.push_back(std::move(inspection));
        }
        Set<String> visited;
        for (size_t index = 0; index < queue.size(); index++)
        {
            const ManagedAssemblyInspection& current = queue[index];
            const String key = IdentityKey(current.Identity);
            if (!visited.insert(key).second)
                continue;
            result.Assemblies.push_back(current.File);
            const bool isFrameworkAssembly = std::any_of(frameworkDirectories.begin(), frameworkDirectories.end(),
                                                         [&](const Path& directory) { return IsWithin(directory, current.File); });
            if (!isFrameworkAssembly && !current.IsILOnly)
                AddDiagnostic(result.Diagnostics, "MB302", "Assembly contains native machine code: " + current.Identity.ToString(), current.File);
            if (!isFrameworkAssembly && current.HasPInvoke)
                AddDiagnostic(result.Diagnostics, "MB303", "Assembly declares P/Invoke methods: " + current.Identity.ToString(), current.File);
            for (const ManagedAssemblyIdentity& reference : current.References)
            {
                const String referenceKey = IdentityKey(reference);
                if (visited.contains(referenceKey))
                    continue;
                const auto found = candidates.find(referenceKey);
                if (found == candidates.end())
                {
                    AddDiagnostic(result.Diagnostics, "MB304", "Missing managed dependency " + reference.ToString() + ".", current.File);
                    continue;
                }
                if (found->second.size() != 1)
                {
                    AddDiagnostic(result.Diagnostics, "MB305", "Ambiguous managed dependency " + reference.ToString() + ".", current.File);
                    continue;
                }
                queue.push_back(found->second.front());
            }
        }
        std::sort(result.Assemblies.begin(), result.Assemblies.end(),
                  [](const Path& left, const Path& right) { return ComparablePath(left) < ComparablePath(right); });
        result.Assemblies.erase(std::unique(result.Assemblies.begin(), result.Assemblies.end()), result.Assemblies.end());
        return result;
    }

    ManagedToolchain LocateManagedToolchain(const Path& root)
    {
        ManagedToolchain result;
        Vector<Path> roots;
        if (!root.empty())
        {
            roots.push_back(root);
            roots.push_back(root / "bin/Mono");
            roots.push_back(root / ".deps/mono");
            roots.push_back(root / "Dependencies/Mono");
            roots.push_back(root / "Crowny/Dependencies/Mono");
        }
        for (const char* variable : { "CROWNY_MONO_ROOT", "MONO_SDK" })
            if (const char* value = std::getenv(variable); value != nullptr && value[0] != '\0')
                roots.emplace_back(value);
#ifdef CW_PLATFORM_WIN32
        if (const char* programFiles = std::getenv("ProgramFiles"))
            roots.emplace_back(Path(programFiles) / "Mono");
#else
        roots.emplace_back("/usr");
        roots.emplace_back("/usr/local");
#endif
        roots = SortedUniquePaths(roots);
        for (const Path& candidateRoot : roots)
        {
            if (!fs::is_directory(candidateRoot))
                continue;
            const Path compiler = FindCompiler(candidateRoot);
            if (compiler.empty())
                continue;
#ifdef CW_PLATFORM_WIN32
            Path runtime = candidateRoot / "bin/mono.exe";
#else
            Path runtime = candidateRoot / "bin/mono";
#endif
            if (!fs::is_regular_file(runtime))
            {
#ifdef CW_PLATFORM_WIN32
                runtime = FindOnPath("mono.exe");
#else
                runtime = FindOnPath("mono");
#endif
            }
            if (!fs::is_regular_file(runtime))
                continue;
            const Path references = FindReferenceDirectory(candidateRoot);
            if (references.empty())
                continue;
            const ProcessResult version = RunProcess(runtime, { PathArgument(compiler), "/version" }, std::chrono::seconds(10), 64 * 1024);
            const String versionText = Trim(!version.StandardOutput.empty() ? version.StandardOutput : version.StandardError);
            if (!version.Started || version.ExitCode != 0 || versionText.empty())
                continue;
            result.Root = NormalizePath(candidateRoot);
            result.RuntimeExecutable = NormalizePath(runtime);
            result.CompilerAssembly = compiler;
            result.ReferenceDirectory = references;
            result.Version = versionText.substr(0, versionText.find_first_of("\r\n"));
            return result;
        }
        AddDiagnostic(result.Diagnostics, "MB100",
                      "No usable Mono/Roslyn toolchain was found. Set CROWNY_MONO_ROOT or run Scripts\\crowny.bat setup.", root);
        return result;
    }

    ManagedBuildPlan CreateManagedBuildPlan(const ManagedBuildRequest& request, const ManagedToolchain& toolchain)
    {
        ManagedBuildPlan plan;
        const Path projectRoot = NormalizePath(request.ProjectRoot);
        const Path output = ResolveProjectPath(projectRoot, request.OutputAssembly);
        if (request.ProjectRoot.empty() || !fs::is_directory(projectRoot))
            AddDiagnostic(plan.Diagnostics, "MB400", "Managed project root does not exist.", projectRoot);
        if (request.OutputAssembly.empty() || output.filename().empty() || !IsWithin(projectRoot, output))
            AddDiagnostic(plan.Diagnostics, "MB401", "Managed output must be a file inside the project root.", output);
        if (Lowercase(output.extension().string()) != ".dll")
            AddDiagnostic(plan.Diagnostics, "MB402", "Managed output must use the .dll extension.", output);
        if (!toolchain.Diagnostics.empty())
            plan.Diagnostics.insert(plan.Diagnostics.end(), toolchain.Diagnostics.begin(), toolchain.Diagnostics.end());
        if (!fs::is_regular_file(toolchain.CompilerAssembly))
            AddDiagnostic(plan.Diagnostics, "MB403", "Managed compiler assembly does not exist.", toolchain.CompilerAssembly);
        if (toolchain.Version.empty())
            AddDiagnostic(plan.Diagnostics, "MB404", "Managed compiler version is unknown.", toolchain.CompilerAssembly);
        if (!fs::is_directory(toolchain.ReferenceDirectory))
            AddDiagnostic(plan.Diagnostics, "MB416", "Managed framework reference directory does not exist.", toolchain.ReferenceDirectory);
        if (request.Timeout <= std::chrono::milliseconds::zero() || request.Timeout > std::chrono::minutes(30))
            AddDiagnostic(plan.Diagnostics, "MB417", "Managed compiler timeout must be between 1 ms and 30 minutes.");
        if (request.MaxCapturedOutputBytes == 0 || request.MaxCapturedOutputBytes > 64 * 1024 * 1024)
            AddDiagnostic(plan.Diagnostics, "MB418", "Managed compiler output limit must be between 1 byte and 64 MiB.");

        Vector<Path> resolvedSources;
        Vector<Path> resolvedReferences;
        resolvedSources.reserve(request.Sources.size());
        resolvedReferences.reserve(request.References.size());
        for (const Path& source : request.Sources)
            resolvedSources.push_back(ResolveProjectPath(projectRoot, source));
        for (const Path& reference : request.References)
            resolvedReferences.push_back(ResolveProjectPath(projectRoot, reference));
        if (fs::is_directory(toolchain.ReferenceDirectory))
        {
            Set<String> referencedNames;
            for (const Path& reference : resolvedReferences)
                referencedNames.insert(Lowercase(reference.filename().string()));
            std::error_code referenceError;
            Vector<Path> frameworkReferences;
            for (const fs::directory_entry& entry : fs::directory_iterator(toolchain.ReferenceDirectory, referenceError))
            {
                if (referenceError)
                    break;
                if (entry.is_regular_file(referenceError) && Lowercase(entry.path().extension().string()) == ".dll")
                    frameworkReferences.push_back(entry.path());
            }
            if (referenceError)
                AddDiagnostic(plan.Diagnostics, "MB419", "Managed framework references could not be enumerated: " + referenceError.message(),
                              toolchain.ReferenceDirectory);
            if (frameworkReferences.empty())
                AddDiagnostic(plan.Diagnostics, "MB420", "Managed framework reference directory contains no assemblies.",
                              toolchain.ReferenceDirectory);
            else if (std::none_of(frameworkReferences.begin(), frameworkReferences.end(),
                                  [](const Path& reference) { return Lowercase(reference.filename().string()) == "mscorlib.dll"; }))
                AddDiagnostic(plan.Diagnostics, "MB422", "Managed framework reference directory does not contain mscorlib.dll.",
                              toolchain.ReferenceDirectory);
            for (const Path& reference : SortedUniquePaths(frameworkReferences))
                if (referencedNames.insert(Lowercase(reference.filename().string())).second)
                    resolvedReferences.push_back(reference);
        }
        Vector<Path> sources = SortedUniquePaths(resolvedSources);
        Vector<Path> references = SortedUniquePaths(resolvedReferences);
        if (sources.empty())
            AddDiagnostic(plan.Diagnostics, "MB405", "Managed build has no source files.", projectRoot);
        for (const Path& source : sources)
        {
            if (!IsWithin(projectRoot, source))
                AddDiagnostic(plan.Diagnostics, "MB406", "Managed source is outside the project root.", source);
            else if (!fs::is_regular_file(source) || Lowercase(source.extension().string()) != ".cs")
                AddDiagnostic(plan.Diagnostics, "MB407", "Managed source is not a readable .cs file.", source);
        }
        for (const Path& reference : references)
            if (!fs::is_regular_file(reference) || Lowercase(reference.extension().string()) != ".dll")
                AddDiagnostic(plan.Diagnostics, "MB408", "Managed reference is not a readable .dll file.", reference);
        if (std::any_of(sources.begin(), sources.end(), [&](const Path& source) { return ComparablePath(source) == ComparablePath(output); }) ||
            std::any_of(references.begin(), references.end(),
                        [&](const Path& reference) { return ComparablePath(reference) == ComparablePath(output); }))
            AddDiagnostic(plan.Diagnostics, "MB421", "Managed output cannot also be a source or reference.", output);

        Vector<String> symbols = request.Symbols;
        symbols.push_back(request.Configuration == BuildConfiguration::Shipping ? "CROWNY_SHIPPING" : "CROWNY_DEVELOPMENT");
        std::sort(symbols.begin(), symbols.end());
        symbols.erase(std::unique(symbols.begin(), symbols.end()), symbols.end());
        for (const String& symbol : symbols)
            if (!IsValidSymbol(symbol))
                AddDiagnostic(plan.Diagnostics, "MB409", "Managed preprocessor symbol is invalid: '" + symbol + "'.");
        if (request.LanguageVersion.empty())
            AddDiagnostic(plan.Diagnostics, "MB410", "Managed language version is empty.");
        if (!plan.Diagnostics.empty())
            return plan;

        plan.CompilerArguments = { "/noconfig", "/nostdlib+", "/target:library", "/deterministic+", "/utf8output", "/fullpaths" };
        plan.CompilerArguments.push_back("/langversion:" + request.LanguageVersion);
        plan.CompilerArguments.push_back(request.Configuration == BuildConfiguration::Shipping ? "/optimize+" : "/optimize-");
        plan.CompilerArguments.push_back(request.Configuration == BuildConfiguration::Shipping ? "/debug:pdbonly" : "/debug:portable");
        plan.CompilerArguments.push_back("/out:" + PathArgument(output));
        if (!symbols.empty())
        {
            String define = "/define:";
            for (size_t index = 0; index < symbols.size(); index++)
            {
                if (index != 0)
                    define.push_back(';');
                define += symbols[index];
            }
            plan.CompilerArguments.push_back(std::move(define));
        }
        for (const Path& reference : references)
            plan.CompilerArguments.push_back("/reference:" + PathArgument(reference));
        for (const Path& source : sources)
            plan.CompilerArguments.push_back(PathArgument(source));

        Sha256Builder hash;
        hash.Add("crowny-managed-build-v2");
        hash.Add(toolchain.Version);
        hash.Add(Lowercase(toolchain.CompilerAssembly.filename().string()));
        if (!hash.AddFile(toolchain.CompilerAssembly))
            AddDiagnostic(plan.Diagnostics, "MB411", "Managed compiler could not be hashed.", toolchain.CompilerAssembly);
        if (!toolchain.RuntimeExecutable.empty())
        {
            if (!hash.AddFile(toolchain.RuntimeExecutable))
                AddDiagnostic(plan.Diagnostics, "MB412", "Managed runtime could not be hashed.", toolchain.RuntimeExecutable);
        }
        for (const String& argument : plan.CompilerArguments)
            hash.Add(argument);
        for (const Path& reference : references)
            if (!hash.AddFile(reference))
                AddDiagnostic(plan.Diagnostics, "MB413", "Managed reference could not be hashed.", reference);
        for (const Path& source : sources)
            if (!hash.AddFile(source))
                AddDiagnostic(plan.Diagnostics, "MB414", "Managed source could not be hashed.", source);
        if (plan.Diagnostics.empty())
        {
            plan.CacheKey = hash.Finish();
            if (plan.CacheKey.empty())
                AddDiagnostic(plan.Diagnostics, "MB415", "Managed build cache key could not be computed.");
        }
        return plan;
    }

    ManagedCompileResult CompileManagedAssembly(const ManagedBuildRequest& request, const ManagedToolchain& toolchain)
    {
        ManagedCompileResult result;
        result.Plan = CreateManagedBuildPlan(request, toolchain);
        if (!result.Plan.IsValid())
        {
            result.Diagnostics = result.Plan.Diagnostics;
            return result;
        }
        std::error_code error;
        const Path projectRoot = NormalizePath(request.ProjectRoot);
        const Path output = ResolveProjectPath(projectRoot, request.OutputAssembly);
        fs::create_directories(output.parent_path(), error);
        if (error)
        {
            AddDiagnostic(result.Diagnostics, "MB500", "Managed output directory could not be created: " + error.message(),
                          request.OutputAssembly.parent_path());
            return result;
        }
        Path executable = toolchain.RuntimeExecutable;
        Vector<String> arguments;
        if (!executable.empty())
            arguments.push_back(PathArgument(toolchain.CompilerAssembly));
        else
            executable = toolchain.CompilerAssembly;
        const Path stagingAssembly = MakeStagingAssemblyPath(output);
        fs::create_directories(stagingAssembly.parent_path(), error);
        if (error)
        {
            AddDiagnostic(result.Diagnostics, "MB512", "Managed staging directory could not be created: " + error.message(),
                          stagingAssembly.parent_path());
            return result;
        }
        RemoveBuildOutput(stagingAssembly);
        for (const String& argument : result.Plan.CompilerArguments)
        {
            if (argument.starts_with("/out:"))
                arguments.push_back("/out:" + PathArgument(stagingAssembly));
            else
                arguments.push_back(argument);
        }
        Path stagingPdb = stagingAssembly;
        stagingPdb.replace_extension(".pdb");
        arguments.push_back("/pdb:" + PathArgument(stagingPdb));
        const ProcessResult process =
          RunProcess(executable, arguments, request.Timeout, request.MaxCapturedOutputBytes, request.Cancellation);
        result.StandardOutput = process.StandardOutput;
        result.StandardError = process.StandardError;
        result.ExitCode = process.ExitCode;
        result.ProcessStarted = process.Started;
        result.Cancelled = process.Cancelled;
        if (!process.Error.empty())
            AddDiagnostic(result.Diagnostics, "MB501", process.Error, executable);
        if (process.TimedOut)
            AddDiagnostic(result.Diagnostics, "MB504", "Managed compiler exceeded its " + std::to_string(request.Timeout.count()) + " ms timeout.",
                          toolchain.CompilerAssembly);
        if (process.OutputTruncated)
            AddDiagnostic(result.Diagnostics, "MB505", "Managed compiler output exceeded the configured capture limit.", toolchain.CompilerAssembly);
        if (process.Error.empty() && !process.TimedOut && !process.Cancelled && process.ExitCode != 0)
            AddDiagnostic(result.Diagnostics, "MB502", "Managed compiler exited with code " + std::to_string(process.ExitCode) + ".",
                          toolchain.CompilerAssembly);
        if (process.Cancelled || !result.Diagnostics.empty())
        {
            RemoveStagingOutput(stagingAssembly);
            return result;
        }
        if (!fs::is_regular_file(stagingAssembly))
        {
            AddDiagnostic(result.Diagnostics, "MB503", "Managed compiler reported success without producing a fresh staged assembly.",
                          stagingAssembly);
            RemoveStagingOutput(stagingAssembly);
            return result;
        }
        const ManagedAssemblyInspection inspection = InspectManagedAssembly(stagingAssembly);
        if (!inspection.Diagnostics.empty() || !inspection.IsILOnly)
        {
            AddDiagnostic(result.Diagnostics, "MB506", "Managed compiler produced an invalid or mixed-mode assembly.", stagingAssembly);
            result.Diagnostics.insert(result.Diagnostics.end(), inspection.Diagnostics.begin(), inspection.Diagnostics.end());
            RemoveStagingOutput(stagingAssembly);
            return result;
        }
        String publishError;
        if (!ReplaceAtomically(stagingAssembly, output, publishError))
        {
            AddDiagnostic(result.Diagnostics, "MB507", "Managed assembly could not be published atomically: " + publishError, output);
            RemoveStagingOutput(stagingAssembly);
            return result;
        }

        Path outputPdb = output;
        outputPdb.replace_extension(".pdb");
        if (fs::is_regular_file(stagingPdb))
        {
            if (!ReplaceAtomically(stagingPdb, outputPdb, publishError))
                AddDiagnostic(result.Diagnostics, "MB508", "Managed symbols could not be published atomically: " + publishError, outputPdb);
        }
        else
        {
            std::error_code removeError;
            fs::remove(outputPdb, removeError);
            if (removeError)
                AddDiagnostic(result.Diagnostics, "MB510", "Stale managed symbols could not be removed: " + removeError.message(), outputPdb);
        }
        const Path stagingMdb = stagingAssembly.string() + ".mdb";
        const Path outputMdb = output.string() + ".mdb";
        if (fs::is_regular_file(stagingMdb))
        {
            publishError.clear();
            if (!ReplaceAtomically(stagingMdb, outputMdb, publishError))
                AddDiagnostic(result.Diagnostics, "MB509", "Managed Mono symbols could not be published atomically: " + publishError, outputMdb);
        }
        else
        {
            std::error_code removeError;
            fs::remove(outputMdb, removeError);
            if (removeError)
                AddDiagnostic(result.Diagnostics, "MB511", "Stale Mono symbols could not be removed: " + removeError.message(), outputMdb);
        }
        RemoveStagingOutput(stagingAssembly);
        return result;
    }

    DotNetSdk LocateDotNetSdk(const Path& root)
    {
        DotNetSdk result;
#ifdef CW_PLATFORM_WIN32
        constexpr const char* executableName = "dotnet.exe";
#else
        constexpr const char* executableName = "dotnet";
#endif
        Vector<Path> candidates;
        if (!root.empty())
            candidates.push_back(root / executableName);
        if (const char* configuredRoot = std::getenv("CROWNY_DOTNET_ROOT"))
            candidates.emplace_back(Path(configuredRoot) / executableName);
        const Path fromPath = FindOnPath(executableName);
        if (!fromPath.empty())
            candidates.push_back(fromPath);

        for (const Path& candidate : candidates)
        {
            if (!fs::is_regular_file(candidate))
                continue;
            const ProcessResult version = RunProcess(candidate, { "--version" }, std::chrono::seconds(10), 64 * 1024);
            const String versionText = Trim(!version.StandardOutput.empty() ? version.StandardOutput : version.StandardError);
            if (!version.Started || version.ExitCode != 0 || versionText.empty())
                continue;
            result.Executable = NormalizePath(candidate);
            result.Version = versionText.substr(0, versionText.find_first_of("\r\n"));
            return result;
        }

        AddDiagnostic(result.Diagnostics, "MB600",
                      "No usable .NET SDK was found. Run Scripts\\crowny.bat deps dotnet or set CROWNY_DOTNET_ROOT.", root);
        return result;
    }

    ManagedSdkBuildResult BuildManagedSdkProject(const ManagedSdkBuildRequest& request, const DotNetSdk& sdk)
    {
        ManagedSdkBuildResult result;
        if (!sdk.Diagnostics.empty())
            result.Diagnostics.insert(result.Diagnostics.end(), sdk.Diagnostics.begin(), sdk.Diagnostics.end());
        if (!fs::is_regular_file(sdk.Executable))
            AddDiagnostic(result.Diagnostics, "MB601", "The .NET SDK executable does not exist.", sdk.Executable);
        if (!fs::is_regular_file(request.ProjectFile))
            AddDiagnostic(result.Diagnostics, "MB602", "The SDK-style managed project does not exist.", request.ProjectFile);
        if (request.OutputDirectory.empty())
            AddDiagnostic(result.Diagnostics, "MB603", "The SDK-style managed output directory is empty.");
        if (request.TargetFramework.empty() ||
            std::any_of(request.TargetFramework.begin(), request.TargetFramework.end(), [](unsigned char character) {
                return std::isalnum(character) == 0 && character != '.' && character != '-';
            }))
            AddDiagnostic(result.Diagnostics, "MB604", "The managed target framework is invalid.");
        if (request.Timeout <= std::chrono::milliseconds::zero() || request.Timeout > std::chrono::minutes(30))
            AddDiagnostic(result.Diagnostics, "MB605", "The SDK build timeout must be between 1 ms and 30 minutes.");
        if (request.MaxCapturedOutputBytes == 0 || request.MaxCapturedOutputBytes > 64 * 1024 * 1024)
            AddDiagnostic(result.Diagnostics, "MB606", "The SDK build output limit must be between 1 byte and 64 MiB.");
        if (!result.Diagnostics.empty())
            return result;

        std::error_code error;
        fs::create_directories(request.OutputDirectory, error);
        if (error)
        {
            AddDiagnostic(result.Diagnostics, "MB607", "The SDK build output directory could not be created: " + error.message(),
                          request.OutputDirectory);
            return result;
        }

        const String configuration = request.Configuration == BuildConfiguration::Shipping ? "Release" : "Debug";
        const Vector<String> arguments = {
            "build",
            PathArgument(request.ProjectFile),
            "--nologo",
            "--disable-build-servers",
            "--configuration",
            configuration,
            "--framework",
            request.TargetFramework,
            "--output",
            PathArgument(request.OutputDirectory),
            "--property:UseAppHost=false",
            "--property:GenerateDependencyFile=true",
        };
        const ProcessResult process =
          RunProcess(sdk.Executable, arguments, request.Timeout, request.MaxCapturedOutputBytes, request.Cancellation);
        result.StandardOutput = process.StandardOutput;
        result.StandardError = process.StandardError;
        result.ExitCode = process.ExitCode;
        result.ProcessStarted = process.Started;
        result.Cancelled = process.Cancelled;
        if (!process.Error.empty())
            AddDiagnostic(result.Diagnostics, "MB608", process.Error, sdk.Executable);
        if (process.TimedOut)
            AddDiagnostic(result.Diagnostics, "MB609", "The SDK build exceeded its timeout.", request.ProjectFile);
        if (process.OutputTruncated)
            AddDiagnostic(result.Diagnostics, "MB610", "The SDK build output exceeded the configured capture limit.", request.ProjectFile);
        if (process.Error.empty() && !process.TimedOut && !process.Cancelled && process.ExitCode != 0)
            AddDiagnostic(result.Diagnostics, "MB611", "The SDK build exited with code " + std::to_string(process.ExitCode) + ".",
                          request.ProjectFile);
        return result;
    }
} // namespace Crowny
