#include "OslTextureTests.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Renderer/OslTextureProgram.h"
#include "RenderTestImage.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>

namespace Crowny::RenderTests
{
    namespace
    {
        Vector<uint8_t> ReadFile(const Path& path)
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input || input.tellg() <= 0 || input.tellg() > 64 * 1024 * 1024)
                throw std::runtime_error("Missing or oversized OSL artifact: " + path.string());
            Vector<uint8_t> bytes(static_cast<size_t>(input.tellg()));
            input.seekg(0);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
                throw std::runtime_error("Cannot read OSL artifact: " + path.string());
            return bytes;
        }
    } // namespace

    int RunOslTextureTest(const Path& package, const Path& artifacts)
    {
        try
        {
            const auto binary = ReadFile(package / "evaluate.spv");
            const auto samples = ReadFile(package / "samples.bin");
            const auto expectedBytes = ReadFile(package / "reference.bin");
            constexpr uint32_t width = 65, height = 37, frames = 3;
            constexpr uint32_t count = width * height;
            if (samples.size() != count * frames * sizeof(OslTextureSample) || expectedBytes.size() != count * frames * sizeof(glm::vec4))
                throw std::runtime_error("Unexpected OSL reference sample layout");
            Vector<glm::vec4> expected(count * frames);
            std::memcpy(expected.data(), expectedBytes.data(), expectedBytes.size());
            OslTextureProgram program;
            String error;
            if (!program.Initialize(binary, error))
                throw std::runtime_error(error);
            const auto input =
              GenericGpuBuffer::Create({ count, sizeof(OslTextureSample), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_DYNAMIC_DRAW });
            const auto output =
              GenericGpuBuffer::Create({ count, sizeof(glm::vec4), GpuBufferType::Structured, BF_UNKNOWN, BufferUsage::BU_DYNAMIC_DRAW });
            if (!input || !output)
                throw std::runtime_error("Cannot create OSL test buffers");
            if (program.Dispatch(nullptr, output, error) || program.Dispatch(input, input, error))
                throw std::runtime_error("Invalid OSL dispatch was accepted");
            fs::create_directories(artifacts);
            std::ofstream raw(artifacts / "gpu.bin", std::ios::binary);
            float maximumError = 0;
            uint32_t mismatches = 0;
            for (uint32_t frame = 0; frame < frames; ++frame)
            {
                input->WriteData(0, input->GetBufferSize(), samples.data() + frame * input->GetBufferSize(), BWT_DISCARD);
                // Poison the output to catch missing dispatches and unwritten tail samples.
                Vector<glm::vec4> actual(count, glm::vec4(std::numeric_limits<float>::quiet_NaN()));
                output->WriteData(0, output->GetBufferSize(), actual.data(), BWT_DISCARD);
                if (!program.Dispatch(input, output, error))
                    throw std::runtime_error(error);
                RenderAPI::Get().SubmitCommandBuffer(nullptr);
                output->ReadData(0, output->GetBufferSize(), actual.data());
                raw.write(reinterpret_cast<const char*>(actual.data()), output->GetBufferSize());
                Image preview(width, height);
                for (uint32_t i = 0; i < count; ++i)
                    for (uint32_t component = 0; component < 4; ++component)
                    {
                        const float value = actual[i][component];
                        const float reference = expected[frame * count + i][component];
                        const float difference = std::abs(value - reference);
                        if (!std::isfinite(value) || !std::isfinite(reference) || difference > 0.00002f)
                            ++mismatches;
                        maximumError = std::max(maximumError, difference);
                        preview.Pixel(i % width, i / width)[component] =
                          std::isfinite(value) ? static_cast<uint8_t>(glm::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f) : 0;
                    }
                if (!SaveBmp(artifacts / ("frame-" + std::to_string(frame) + ".bmp"), preview, error))
                    throw std::runtime_error(error);
            }
            std::ofstream report(artifacts / "comparison.txt");
            report << "samples=" << count * frames << "\nframes=" << frames << "\nmax_absolute_error=" << maximumError
                   << "\nfailing_components=" << mismatches << "\ntolerance=0.00002\n";
            if (!raw || !report)
                throw std::runtime_error("Cannot write OSL test results");
            std::cout << "OSL GPU: " << count * frames << " samples, max error " << maximumError << ", " << mismatches << " failures\n";
            return mismatches ? 1 : 0;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "OSL GPU test failed: " << exception.what() << '\n';
            return 1;
        }
    }
} // namespace Crowny::RenderTests
