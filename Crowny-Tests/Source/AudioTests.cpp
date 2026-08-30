#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioManager.h"
#include "Crowny/Audio/AudioUtils.h"
#include "Crowny/Audio/EFXLoader.h"
#include "Crowny/Common/DataStream.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Scene/Scene.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

using namespace Crowny;

namespace
{
    void EnsureHeadlessAudioRuntime()
    {
        if (!Application::IsStartedUp())
        {
            ApplicationDesc description;
            description.Name = "AudioTests";
            description.Headless = true;
            description.WorkingDirectory = fs::current_path();
            Application::StartUp(description);
        }
        REQUIRE(AssetManager::IsStartedUp());
        REQUIRE(AudioManager::IsStartedUp());
    }

    AssetHandle<AudioClip> CreateSilentClip()
    {
        constexpr uint32_t sampleRate = 8000;
        constexpr uint32_t sampleCount = 800;
        constexpr uint32_t streamSize = sampleCount * sizeof(int16_t);
        Ref<MemoryDataStream> memoryStream = CreateRef<MemoryDataStream>(streamSize);
        std::fill_n(memoryStream->Data(), streamSize, uint8_t{ 0 });
        Ref<DataStream> stream = memoryStream;
        AudioClipDesc description;
        description.Format = AudioFormat::PCM;
        description.ReadMode = AudioReadMode::LoadDecompressed;
        description.Frequency = sampleRate;
        description.BitDepth = 16;
        description.NumChannels = 1;
        description.KeepSourceData = true;
        Ref<AudioClip> clip = AudioClip::Create(stream, streamSize, sampleCount, description);
        return static_asset_cast<AudioClip>(AssetManager::Get().CreateAssetHandle(clip));
    }
} // namespace

TEST_CASE("Audio buffer sizes use interleaved sample counts", "[Audio]")
{
    CHECK(AudioUtils::GetBufferSize(7, 8) == 7);
    CHECK(AudioUtils::GetBufferSize(7, 16) == 14);
    CHECK(AudioUtils::GetBufferSize(7, 24) == 21);
    CHECK(AudioUtils::GetBufferSize(7, 32) == 28);
    CHECK(AudioUtils::GetBufferSize(7, 12) == 0);
    CHECK(AudioUtils::GetBufferSize(std::numeric_limits<uint32_t>::max(), 32) == 0);
}

TEST_CASE("OpenAL PCM preparation selects the supported conversion path", "[Audio][OpenAL]")
{
    AudioStreamScratch scratch;
    PreparedAudioPCM prepared;

    SECTION("signed 8-bit input becomes unsigned")
    {
        const std::array<uint8_t, 4> input = { 0x80, 0xFF, 0x00, 0x7F };
        const AudioDataInfo info = { static_cast<uint32_t>(input.size()), 48000, 1, 8 };
        REQUIRE(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::Ready);
        CHECK(prepared.BitDepth == 8);
        CHECK(prepared.Size == 4);
        CHECK_FALSE(prepared.UsedIntegerFallback);
        REQUIRE(prepared.Data != nullptr);
        const auto* converted = static_cast<const uint8_t*>(prepared.Data);
        const std::array<uint8_t, 4> expected = { 0, 127, 128, 255 };
        CHECK(std::equal(converted, converted + input.size(), expected.begin()));
    }

    SECTION("16-bit input is uploaded without a copy")
    {
        const std::array<int16_t, 3> input = { -32768, 0, 32767 };
        const AudioDataInfo info = { static_cast<uint32_t>(input.size()), 48000, 1, 16 };
        const auto* bytes = reinterpret_cast<const uint8_t*>(input.data());
        REQUIRE(AudioUtils::PrepareOpenALPCM(bytes, info, {}, scratch, prepared) == AudioPCMPreparationStatus::Ready);
        CHECK(prepared.Data == bytes);
        CHECK(prepared.BitDepth == 16);
        CHECK(prepared.Size == static_cast<ALsizei>(input.size() * sizeof(int16_t)));
        CHECK_FALSE(prepared.UsedIntegerFallback);
    }

    SECTION("24-bit input uses float when supported")
    {
        const std::array<uint8_t, 6> input = { 0x00, 0x00, 0x80, 0x00, 0xFF, 0x7F };
        const AudioDataInfo info = { 2, 48000, 1, 24 };
        AudioPCMCapabilities capabilities;
        capabilities.SupportsFloat32 = true;
        REQUIRE(AudioUtils::PrepareOpenALPCM(input.data(), info, capabilities, scratch, prepared) == AudioPCMPreparationStatus::Ready);
        CHECK(prepared.BitDepth == 32);
        CHECK(prepared.Size == static_cast<ALsizei>(2 * sizeof(float)));
        CHECK_FALSE(prepared.UsedIntegerFallback);
        const auto* converted = static_cast<const float*>(prepared.Data);
        CHECK(converted[0] == Catch::Approx(-1.0f));
        CHECK(converted[1] == Catch::Approx(0.9999695f));
    }

    SECTION("24-bit input falls back to 16-bit when float is unavailable")
    {
        const std::array<uint8_t, 6> input = { 0x00, 0x00, 0x80, 0x00, 0xFF, 0x7F };
        const AudioDataInfo info = { 2, 48000, 1, 24 };
        REQUIRE(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::Ready);
        CHECK(prepared.BitDepth == 16);
        CHECK(prepared.Size == static_cast<ALsizei>(2 * sizeof(int16_t)));
        CHECK(prepared.UsedIntegerFallback);
        std::array<int16_t, 2> converted{};
        std::memcpy(converted.data(), prepared.Data, prepared.Size);
        CHECK(converted[0] == std::numeric_limits<int16_t>::min());
        CHECK(converted[1] == std::numeric_limits<int16_t>::max());
    }
}

TEST_CASE("OpenAL PCM preparation validates capabilities and ALsizei bounds", "[Audio][OpenAL]")
{
    const std::array<uint8_t, 2> input{};
    AudioStreamScratch scratch;
    PreparedAudioPCM prepared;

    AudioDataInfo info = { 1, 48000, 3, 16 };
    CHECK(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::UnsupportedChannels);
    CHECK(prepared.Data == nullptr);

    info.NumChannels = 4;
    CHECK(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::UnsupportedMultichannel);
    AudioPCMCapabilities multichannel;
    multichannel.SupportsMultichannel = true;
    CHECK(AudioUtils::PrepareOpenALPCM(input.data(), info, multichannel, scratch, prepared) == AudioPCMPreparationStatus::Ready);

    info.NumChannels = 1;
    info.BitDepth = 12;
    CHECK(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::UnsupportedBitDepth);

    info.BitDepth = 16;
    info.SampleRate = static_cast<uint32_t>(std::numeric_limits<ALsizei>::max()) + 1u;
    CHECK(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::SizeOverflow);

    info.SampleRate = 48000;
    info.NumSamples = static_cast<uint32_t>(std::numeric_limits<ALsizei>::max()) / 2u + 1u;
    CHECK(AudioUtils::PrepareOpenALPCM(input.data(), info, {}, scratch, prepared) == AudioPCMPreparationStatus::SizeOverflow);
    CHECK(prepared.Data == nullptr);
    CHECK(prepared.Size == 0);
}

TEST_CASE("OpenAL streaming scratch is allocation-free after its high-water mark", "[Audio][OpenAL][Allocation]")
{
    struct PreparationCase
    {
        uint32_t BitDepth;
        bool SupportsFloat32;
    };
    constexpr std::array<PreparationCase, 4> cases = { PreparationCase{ 8, false }, PreparationCase{ 16, false }, PreparationCase{ 24, false },
                                                       PreparationCase{ 32, true } };

    for (const PreparationCase& preparationCase : cases)
    {
        CAPTURE(preparationCase.BitDepth, preparationCase.SupportsFloat32);
        constexpr uint32_t sampleCount = 16384;
        AudioDataInfo info = { sampleCount, 48000, 2, preparationCase.BitDepth };
        AudioPCMCapabilities capabilities;
        capabilities.SupportsFloat32 = preparationCase.SupportsFloat32;
        AudioStreamScratch scratch;
        uint32_t decodedSize = 0;
        REQUIRE(AudioUtils::TryGetBufferSize(sampleCount, preparationCase.BitDepth, decodedSize));
        scratch.DecodedSamples.resize(decodedSize);

        PreparedAudioPCM prepared;
        REQUIRE(AudioUtils::PrepareOpenALPCM(scratch.DecodedSamples.data(), info, capabilities, scratch, prepared) ==
                AudioPCMPreparationStatus::Ready);

        const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
        AudioPCMPreparationStatus status = AudioPCMPreparationStatus::InvalidInput;
        for (uint32_t refill = 0; refill < 120 * 3; refill++)
        {
            scratch.DecodedSamples.resize(decodedSize);
            status = AudioUtils::PrepareOpenALPCM(scratch.DecodedSamples.data(), info, capabilities, scratch, prepared);
        }
        const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
        const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

        CHECK(status == AudioPCMPreparationStatus::Ready);
        CHECK(delta.AllocationCount == 0u);
        CHECK(delta.RequestedBytes == 0u);
    }
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

TEST_CASE("Audio component snapshots apply settings without replacing identity", "[Audio][Ecs][Lifecycle]")
{
    AudioSourceComponent source;
    source.SetVolume(0.65f);
    source.SetPitch(1.25f);
    source.SetLooping(true);
    source.SetIsMuted(true);
    source.SetPlayOnAwake(false);
    source.SetMinDistance(2.0f);
    source.SetMaxDistance(90.0f);
    source.SetTime(3.5f);
    source.SetBusName("Effects");
    source.SetLowPassGain(0.4f);
    source.SetHighPassGain(0.7f);
    source.SetConeInnerAngle(90.0f);
    source.SetConeOuterAngle(180.0f);
    source.SetConeOuterGain(0.2f);
    source.SetConeOuterGainHF(0.3f);

    AudioSourceComponent copied(source);
    CHECK(copied.InstanceId != source.InstanceId);
    CHECK(copied.GetRuntimeSource() == nullptr);
    CHECK(copied.GetState() == AudioSourceState::Stopped);

    AudioSourceComponent destination;
    const uint64_t destinationId = destination.InstanceId;
    destination = source;
    destination.ApplyRuntimeSettings();

    CHECK(destination.InstanceId == destinationId);
    CHECK(destination.GetVolume() == 0.65f);
    CHECK(destination.GetPitch() == 1.25f);
    CHECK(destination.GetLooping());
    CHECK(destination.GetIsMuted());
    CHECK_FALSE(destination.GetPlayOnAwake());
    CHECK(destination.GetMinDistance() == 2.0f);
    CHECK(destination.GetMaxDistance() == 90.0f);
    CHECK(destination.GetTime() == 3.5f);
    CHECK(destination.GetBusName() == "Effects");
    CHECK(destination.GetLowPassGain() == 0.4f);
    CHECK(destination.GetHighPassGain() == 0.7f);
    CHECK(destination.GetConeInnerAngle() == 90.0f);
    CHECK(destination.GetConeOuterAngle() == 180.0f);
    CHECK(destination.GetConeOuterGain() == 0.2f);
    CHECK(destination.GetConeOuterGainHF() == 0.3f);

    AudioListenerComponent listener;
    const uint64_t listenerId = listener.InstanceId;
    AudioListenerComponent listenerSnapshot;
    listener = listenerSnapshot;
    CHECK(listener.InstanceId == listenerId);
}

TEST_CASE("Active audio sources survive AddOrReplace without replaying", "[Audio][Ecs][Lifecycle]")
{
    EnsureHeadlessAudioRuntime();

    Ref<Scene> scene = CreateRef<Scene>(false);
    scene->CreateEntity("Listener").AddComponent<AudioListenerComponent>();
    Entity entity = scene->CreateEntity("Replaceable audio source");
    auto& source = entity.AddComponent<AudioSourceComponent>();
    const AssetHandle<AudioClip> clip = CreateSilentClip();
    source.SetClip(clip);
    source.SetPlayOnAwake(false);
    source.SetVolume(0.2f);

    scene->OnRuntimeStart();
    AudioSourceComponent* componentAddress = &source;
    const uint64_t instanceId = source.InstanceId;
    const AudioSource* runtimeSource = source.GetRuntimeSource();
    REQUIRE(runtimeSource != nullptr);
    CHECK(source.GetState() == AudioSourceState::Stopped);

    AudioSourceComponent settings = source;
    CHECK(settings.GetRuntimeSource() == nullptr);
    settings.SetPlayOnAwake(true);
    settings.SetVolume(0.65f);
    settings.SetPitch(1.25f);
    settings.SetLooping(true);
    settings.SetIsMuted(true);
    settings.SetMinDistance(2.0f);
    settings.SetMaxDistance(90.0f);
    settings.SetLowPassGain(0.4f);
    settings.SetHighPassGain(0.7f);
    settings.SetConeInnerAngle(90.0f);
    settings.SetConeOuterAngle(180.0f);
    settings.SetConeOuterGain(0.2f);
    settings.SetConeOuterGainHF(0.3f);

    auto& replaced = entity.AddOrReplaceComponent<AudioSourceComponent>(settings);
    CHECK(&replaced == componentAddress);
    CHECK(replaced.InstanceId == instanceId);
    CHECK(replaced.GetRuntimeSource() == runtimeSource);
    CHECK(replaced.GetState() == AudioSourceState::Stopped);
    CHECK(runtimeSource->GetAudioClip().GetUUID() == clip.GetUUID());
    CHECK(runtimeSource->GetVolume() == 0.0f);
    CHECK(runtimeSource->GetPitch() == 1.25f);
    CHECK(runtimeSource->GetLooping());
    CHECK(runtimeSource->GetMinDistance() == 2.0f);
    CHECK(runtimeSource->GetMaxDistance() == 90.0f);
    CHECK(runtimeSource->GetLowPassGain() == 0.4f);
    CHECK(runtimeSource->GetHighPassGain() == 0.7f);
    CHECK(runtimeSource->GetConeInnerAngle() == 90.0f);
    CHECK(runtimeSource->GetConeOuterAngle() == 180.0f);
    CHECK(runtimeSource->GetConeOuterGain() == 0.2f);
    CHECK(runtimeSource->GetConeOuterGainHF() == 0.3f);

    scene->OnRuntimeStop();
    CHECK(replaced.GetState() == AudioSourceState::Stopped);
    entity.RemoveComponent<AudioSourceComponent>();
    CHECK_FALSE(entity.HasComponent<AudioSourceComponent>());
}
