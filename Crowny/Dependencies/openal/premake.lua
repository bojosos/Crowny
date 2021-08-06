project "glad"
    kind "StaticLib"
    language "C"
    staticruntime "on"
    
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "common/alcomplex.cpp",
        "common/alfstream.cpp",
        "common/almalloc.cpp",
        "common/alstring.cpp",
        "common/dynload.cpp",
        "common/polyphase_resampler.cpp",
        "common/ringbuffer.cpp",
        "common/strutils.cpp",
        "common/threads.cpp",
        "core/ambdec.cpp",
        "core/ambidefs.cpp",
        "core/bformatdec.cpp",
        "core/bs2b.cpp",
        "core/bsinc_tables.cpp",
        "core/buffer_storage.cpp",
        "core/context.cpp",
        "core/converter.cpp",
        "core/cpu_caps.cpp",
        "core/devformat.cpp",
        "core/device.cpp",
        "core/except.cpp",
        "core/filters/biquad.cpp",
        "core/filters/nfc.cpp",
        "core/filters/splitter.cpp",
        "core/fmt_traits.cpp",
        "core/fpu_ctrl.cpp",
        "core/helpers.cpp",
        "core/hrtf.cpp",
        "core/logging.cpp",
        "core/mastering.cpp",
        "core/mixer.cpp",
        "core/uhjfilter.cpp",
        "core/uiddefs.cpp",
        "core/voice.cpp",
        "al/auxeffectslot.cpp",
        "al/buffer.cpp",
        "al/effect.cpp",
        "al/effects/autowah.cpp",
        "al/effects/chorus.cpp",
        "al/effects/compressor.cpp",
        "al/effects/convolution.cpp",
        "al/effects/dedicated.cpp",
        "al/effects/distortion.cpp",
        "al/effects/echo.cpp",
        "al/effects/equalizer.cpp",
        "al/effects/fshifter.cpp",
        "al/effects/modulator.cpp",
        "al/effects/null.cpp",
        "al/effects/pshifter.cpp",
        "al/effects/reverb.cpp",
        "al/effects/vmorpher.cpp",
        "al/error.cpp",
        "al/event.cpp",
        "al/extension.cpp",
        "al/filter.cpp",
        "al/listener.cpp",
        "al/source.cpp",
        "al/state.cpp",
        "alc/alc.cpp",
        "alc/alu.cpp",
        "alc/alconfig.cpp",
        "alc/context.cpp",
        "alc/device.cpp",
        "alc/effectslot.cpp",
        "alc/effects/autowah.cpp",
        "alc/effects/chorus.cpp",
        "alc/effects/compressor.cpp",
        "alc/effects/convolution.cpp",
        "alc/effects/dedicated.cpp",
        "alc/effects/distortion.cpp",
        "alc/effects/echo.cpp",
        "alc/effects/equalizer.cpp",
        "alc/effects/fshifter.cpp",
        "alc/effects/modulator.cpp",
        "alc/effects/null.cpp",
        "alc/effects/pshifter.cpp",
        "alc/effects/reverb.cpp",
        "alc/effects/vmorpher.cpp",
        "alc/panning.cpp",

        "alc/backends/alsa.cpp",

        "core/mixer/mixer_c.cpp" --- SSE?
    }

    includedirs
    {
        "include"
    }
    
    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"