// Test fixture generator only. The editor and production compiler never run
// this executable. Runtime materials evaluate SPIR-V directly on the GPU.
#include <OSL/oslexec.h>
#include <OSL/oslquery.h>
#include <OSL/rendererservices.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    using Values = std::vector<std::array<float, 4>>;
    struct Sample
    {
        float U, V, Time, Reserved;
        float Dudx, Dudy, Dvdx, Dvdy;
    };
    static_assert(sizeof(Sample) == 32);

    class Errors final : public OIIO::ErrorHandler
    {
    public:
        bool Failed = false;
        void operator()(int code, const std::string& message) override
        {
            Failed |= (code & 0xffff0000) >= EH_ERROR;
            std::cerr << message << '\n';
        }
    };

    void Generate(const std::string& shader, const std::string& output)
    {
        OSL::OSLQuery query;
        if (!query.open(shader))
            throw std::runtime_error(query.geterror());
        const auto* result = query.getparam(output);
        if (!result || !result->isoutput || result->type != OIIO::TypeColor)
            throw std::runtime_error("Expected a color output");
        std::vector<const OSL::OSLQuery::Parameter*> inputs;
        Values defaults;
        bool textures = false;
        for (const auto& p : query.parameters())
        {
            if (p.isoutput)
                continue;
            if (p.type == OIIO::TypeString)
            {
                textures = true;
                continue;
            }
            if (!p.validdefault || p.type.is_array() || p.isstruct || p.isclosure ||
                !(p.type == OIIO::TypeFloat || p.type == OIIO::TypeInt || p.type == OIIO::TypeColor || p.type == OIIO::TypePoint ||
                  p.type == OIIO::TypeVector || p.type == OIIO::TypeNormal))
                throw std::runtime_error("Unsupported test input: " + p.name.string());
            inputs.push_back(&p);
            std::array<float, 4> value{};
            std::memcpy(value.data(), p.type == OIIO::TypeInt ? static_cast<const void*>(p.idefault.data()) : p.fdefault.data(), p.type.size());
            defaults.push_back(value);
        }
        std::array<Values, 3> edited;
        std::ofstream parameters("surface-parameters.bin", std::ios::binary);
        for (int frame = 0; frame < 3; ++frame)
        {
            edited[frame] = defaults;
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                const auto& p = *inputs[i];
                if (p.type == OIIO::TypeInt)
                {
                    const int value = p.idefault[0] + frame * 2;
                    std::memcpy(edited[frame][i].data(), &value, sizeof(value));
                }
                else
                    for (size_t c = 0; c < p.type.aggregate; ++c)
                        edited[frame][i][c] = p.fdefault[c] * (1.0f + 0.25f * frame) + (0.125f + c * 0.1f) * frame;
            }
            parameters.write(reinterpret_cast<const char*>(edited[frame].data()), edited[frame].size() * 16);
        }
        if (!parameters)
            throw std::runtime_error("Cannot write test parameters");
        // Image tests bind real engine textures and compare with a GLSL oracle.
        // OIIO cannot reproduce those asset bindings and GPU sampler settings.
        if (textures)
            return;
        Errors errors;
        OSL::RendererServices renderer;
        OSL::ShadingSystem system(&renderer, nullptr, &errors);
        auto group = system.ShaderGroupBegin("reference");
        std::vector<OSL::SymLocationDesc> locations;
        for (size_t i = 0; i < inputs.size(); ++i)
        {
            const auto& p = *inputs[i];
            if (!system.Parameter(p.name, p.type, defaults[i].data(), OSL::ParamHints::interpolated))
                throw std::runtime_error("Cannot bind test input");
            locations.emplace_back(p.name, p.type, false, OSL::SymArena::UserData, i * 16, 0);
        }
        if (!system.Shader("surface", shader, "texture") || !system.ShaderGroupEnd())
            throw std::runtime_error("Cannot create reference group");
        const OSL::SymLocationDesc location(output, OIIO::TypeColor, false, OSL::SymArena::Outputs, 0, 12);
        system.add_symlocs(group.get(), { &location, 1 });
        system.add_symlocs(group.get(), locations);
        auto* thread = system.create_thread_info();
        auto* ctx = system.get_context(thread);
        std::ofstream samples("samples.bin", std::ios::binary);
        std::ofstream reference("reference.bin", std::ios::binary);
        std::ofstream surface("surface-reference.bin", std::ios::binary);
        for (int frame = 0; frame < 3; ++frame)
            for (int y = 0; y < 37; ++y)
                for (int x = 0; x < 65; ++x)
                {
                    Sample sample{
                        (x + 0.5f) / 65 * 3 - 1, (y + 0.5f) / 37 * 2 - 0.5f, frame * 0.73f, 0, 3.0f / 65, 0.003f * frame, -0.002f * frame, 2.0f / 37
                    };
                    OSL::ShaderGlobals sg{};
                    sg.u = sample.U;
                    sg.v = sample.V;
                    sg.time = sample.Time;
                    sg.dudx = sample.Dudx;
                    sg.dudy = sample.Dudy;
                    sg.dvdx = sample.Dvdx;
                    sg.dvdy = sample.Dvdy;
                    float rgba[]{ 0, 0, 0, 1 };
                    if (!system.execute(*ctx, *group, 0, 0, sg, defaults.data(), rgba) || errors.Failed)
                        throw std::runtime_error("CPU reference execution failed");
                    samples.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
                    reference.write(reinterpret_cast<const char*>(rgba), sizeof(rgba));
                    sg.dudy = sg.dvdx = 0;
                    if (!system.execute(*ctx, *group, 0, 0, sg, edited[frame].data(), rgba) || errors.Failed)
                        throw std::runtime_error("CPU surface reference execution failed");
                    surface.write(reinterpret_cast<const char*>(rgba), sizeof(rgba));
                }
        system.release_context(ctx);
        system.destroy_thread_info(thread);
        if (!samples || !reference || !surface)
            throw std::runtime_error("Cannot write reference data");
        std::cout << "Generated 7215 CPU test reference samples\n";
    }
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: crowny-osl-reference shader.oso color-output\n";
        return 2;
    }
    try
    {
        Generate(std::filesystem::absolute(argv[1]).string(), argv[2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "OSL test reference failed: " << error.what() << '\n';
        return 1;
    }
}
