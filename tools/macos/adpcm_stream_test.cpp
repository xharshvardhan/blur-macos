// [Blur2PC] Exercise the original VAG readers above 4 GiB, not a replacement decoder.
#include "AudioPch.h"
#include "source/core/coreheaders.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

// This CPU fixture has no application Xt types, as in audio_core_test.
void XtLibrary_Application() {}
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static void equal(F32 actual, F32 expected, const char* message) {
    require(std::abs(actual - expected) < 0.000001f, message);
}
static F32 value(U32 packet, U32 channel) {
    // Predictor zero, shift zero: signed nibble / 8. Different packets/channels
    // expose wrong 33/66-byte strides and an incorrect aligned-source offset.
    return channel ? -F32(packet + 1) / 8.f : F32(packet + 1) / 8.f;
}
static cDecodeVagInfo setup(U8* bytes, U32 channels, F32* left, F32* right,
                          S32 offset, S32 samples, U32 end, bool loop = false) {
    cDecodeVagInfo info{};
    info.mSourceVag = bytes;
    info.mVagSizeInBytes = 3 * channels * 33;
    info.mPcmReadOffset = offset;
    info.mpDestPCML = left;
    info.mpDestPCMR = right;
    info.mDestPCMSamples = samples;
    info.mLoop = loop;
    info.mPCMLoopPosEnd = end;
    info.mbStereo = channels == 2;
    return info;
}
static S32 read(Audio::Core::ADPCM4bitDecoder& decoder, U32 channels,
                cDecodeVagInfo& info, cDecodeVagWorkArea& work) {
    return channels == 1 ? decoder.ReadVagStreamMono(&info, &work)
                         : decoder.ReadVagStreamStereo(&info, &work);
}
static void run(U32 channels, U32 alignment) {
    // Fetch deliberately reads an aligned 64/128-byte window around each packet.
    // Padding is owned and initialized, exactly as the streaming buffer contract
    // requires; this is not a malformed-packet/bounds-checking claim.
    alignas(128) std::array<U8, 512> storage{};
    storage.fill(0xa5);
    U8* bytes = storage.data() + 32 + alignment;
    require((reinterpret_cast<std::uintptr_t>(bytes) >> 32) != 0,
            "Fixture did not exercise a pointer above 4 GiB");
    for (U32 packet = 0; packet < 3; ++packet) {
        for (U32 channel = 0; channel < channels; ++channel) {
            U8* block = bytes + (packet * channels + channel) * 33;
            block[0] = 0; // predictor0, shift0
            const U8 nibble = channel ? U8(15 - packet) : U8(packet + 1);
            std::fill_n(block + 1, 32, U8(nibble | (nibble << 4)));
        }
    }
    const auto original = storage;
    Audio::Core::MonoADPCM4bitDecoder mono(false, false);
    Audio::Core::StereoADPCM4bitDecoder stereo(false, false);
    Audio::Core::ADPCM4bitDecoder& decoder = channels == 1
        ? static_cast<Audio::Core::ADPCM4bitDecoder&>(mono)
        : static_cast<Audio::Core::ADPCM4bitDecoder&>(stereo);
    constexpr F32 guard = 999.f;
    std::array<F32, 210> left, right;
    auto reset = [&] { left.fill(guard); right.fill(guard); };
    auto bounds = [&](U32 count) {
        require(left[0] == guard && left[count + 1] == guard,
                "Mono/left PCM writer crossed requested bounds");
        require(right[0] == guard && right[count + 1] == guard,
                "Right PCM writer crossed requested bounds");
        if (channels == 1)
            for (F32 sample : right) require(sample == guard, "Mono reader wrote right channel");
        require(storage == original, "Decoder mutated the VAG source/padding");
    };
    cDecodeVagWorkArea work;
    reset(); work.ClearPrevious(); work.mDeltaFader = 0;
    auto info = setup(bytes, channels, left.data() + 1, right.data() + 1, 5, 79, 192);
    require(read(decoder, channels, info, work) == 84 && info.mSamplesConsumed == 79 &&
            !info.mAbortStream, "Partial packet/cross-packet stream position changed");
    for (U32 i = 0; i < 79; ++i) {
        equal(left[i + 1], value((i + 5) / 64, 0), "Left packet alignment/sample mismatch");
        if (channels == 2)
            equal(right[i + 1], value((i + 5) / 64, 1), "Right packet alignment/sample mismatch");
    }
    bounds(79);

    reset(); work.ClearPrevious(); work.mDeltaFader = 0;
    info = setup(bytes, channels, left.data() + 1, right.data() + 1, 0, 132, 128);
    require(read(decoder, channels, info, work) == 128 && info.mSamplesConsumed == 128,
            "End-of-stream position/consumed count changed");
    for (U32 i = 0; i < 132; ++i) {
        equal(left[i + 1], i < 128 ? value(i / 64, 0) : 0.f, "Left EOF samples/zero padding changed");
        if (channels == 2)
            equal(right[i + 1], i < 128 ? value(i / 64, 1) : 0.f, "Right EOF samples/zero padding changed");
    }
    bounds(132);

    reset(); work.ClearPrevious(); work.mDeltaFader = 0;
    info = setup(bytes, channels, left.data() + 1, right.data() + 1, 0, 96, 64, true);
    require(read(decoder, channels, info, work) == 32 && info.mSamplesConsumed == 96 &&
            !info.mAbortStream, "Loop did not wrap through the original sample boundary");
    const F32 restartedGain = 1.f - .95f * .95f;
    for (U32 i = 0; i < 96; ++i) {
        const F32 gain = i < 64 ? 1.f : restartedGain;
        equal(left[i + 1], value(0, 0) * gain, "Left loop predictor/fader reset changed");
        if (channels == 2)
            equal(right[i + 1], value(0, 1) * gain, "Right loop predictor/fader reset changed");
    }
    bounds(96);

    reset(); work.ClearPrevious();
    info = setup(bytes, channels, left.data() + 1, right.data() + 1, 128, 17, 128);
    require(read(decoder, channels, info, work) == 128 && info.mSamplesConsumed == 0,
            "Exhausted stream unexpectedly consumed source");
    for (U32 i = 0; i < 17; ++i) {
        equal(left[i + 1], 0.f, "Exhausted mono/left stream was not silent");
        if (channels == 2) equal(right[i + 1], 0.f, "Exhausted right stream was not silent");
    }
    bounds(17);
}
int main() {
    try {
        for (U32 channels : {1u, 2u})
            for (U32 alignment = 0; alignment < 16; ++alignment) run(channels, alignment);
        std::cout << "Original mono/stereo VAG high-pointer alignment, packet/loop/EOF checks PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
