#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Audio/EFXLoader.h"
#include "Crowny/Audio/AudioUtils.h"

#include <array>
#include <limits>

using namespace Crowny;

TEST_CASE("Audio buffer sizes use interleaved sample counts", "[Audio]")
{
    CHECK(AudioUtils::GetBufferSize(7, 8) == 7);
    CHECK(AudioUtils::GetBufferSize(7, 16) == 14);
    CHECK(AudioUtils::GetBufferSize(7, 24) == 21);
    CHECK(AudioUtils::GetBufferSize(7, 32) == 28);
    CHECK(AudioUtils::GetBufferSize(7, 12) == 0);
}

TEST_CASE("Signed 8-bit PCM is converted to OpenAL unsigned PCM", "[Audio]")
{
    const std::array<uint8_t, 4> input = { 0x80, 0xFF, 0x00, 0x7F };
    std::array<uint8_t, 4> output{};
    AudioUtils::ConvertSigned8ToUnsigned(input.data(), output.data(), static_cast<uint32_t>(input.size()));
    CHECK(output == std::array<uint8_t, 4>{ 0, 127, 128, 255 });
}

TEST_CASE("Integer PCM conversion reaches the normalized endpoints", "[Audio]")
{
    const std::array<uint8_t, 4> input = { 0x80, 0xFF, 0x00, 0x7F };
    std::array<float, 4> output{};
    AudioUtils::ConvertToFloat(input.data(), 8, output.data(), static_cast<uint32_t>(input.size()));

    CHECK(output[0] == Catch::Approx(-1.0f));
    CHECK(output[1] == Catch::Approx(-1.0f / 128.0f));
    CHECK(output[2] == Catch::Approx(0.0f));
    CHECK(output[3] == Catch::Approx(1.0f));
}

TEST_CASE("24-bit PCM converts and downmixes without losing sign", "[Audio]")
{
    const std::array<uint8_t, 6> endpoints = { 0x00, 0x00, 0x80, 0x00, 0xFF, 0x7F };
    std::array<int16_t, 2> converted{};
    AudioUtils::ConvertBitDepth(endpoints.data(), 24, reinterpret_cast<uint8_t*>(converted.data()), 16, 2);
    CHECK(converted[0] == std::numeric_limits<int16_t>::min());
    CHECK(converted[1] == std::numeric_limits<int16_t>::max());

    const std::array<uint8_t, 12> stereo = {
        0x00, 0x00, 0x40, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x20, 0x00, 0x00, 0x60,
    };
    std::array<uint8_t, 6> mono{};
    AudioUtils::ConvertToMono(stereo.data(), mono.data(), 24, 2, 2);
    CHECK(mono == std::array<uint8_t, 6>{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x40 });
}

TEST_CASE("Global pause resumes only a source that was playing", "[Audio]")
{
    CHECK(AudioUtils::ShouldResumeAfterGlobalPause(AudioSourceState::Playing));
    CHECK_FALSE(AudioUtils::ShouldResumeAfterGlobalPause(AudioSourceState::Paused));
    CHECK_FALSE(AudioUtils::ShouldResumeAfterGlobalPause(AudioSourceState::Stopped));
}

TEST_CASE("EFX capability failures remain inspectable without an OpenAL device", "[Audio][EFX]")
{
    EFX efx;
    CHECK(efx.Status == EFXLoadStatus::NotLoaded);
    CHECK_FALSE(efx.Available);
    CHECK(String(EFX::GetStatusName(efx.Status)) == "not loaded");
    CHECK(String(efx.GetMissingRequiredEntrypoint()) == "alGenEffects");

    CHECK_FALSE(efx.Load(nullptr));
    CHECK(efx.Status == EFXLoadStatus::NoDevice);
    CHECK_FALSE(efx.Available);

    efx.Reset();
    CHECK(efx.Status == EFXLoadStatus::NotLoaded);
    CHECK(efx.MissingEntrypoint == nullptr);
}
