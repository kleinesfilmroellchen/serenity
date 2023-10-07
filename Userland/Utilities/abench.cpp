/*
 * Copyright (c) 2021, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/MemoryStream.h>
#include <AK/NumericLimits.h>
#include <AK/Types.h>
#include <LibAudio/Encoder.h>
#include <LibAudio/FlacWriter.h>
#include <LibAudio/Loader.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/System.h>
#include <LibDSP/Resampler.h>
#include <LibFileSystem/FileSystem.h>
#include <LibMain/Main.h>
#include <stdio.h>

// The Kernel has problems with large anonymous buffers, so let's limit sample reads ourselves.
static constexpr size_t MAX_CHUNK_SIZE = 1 * MiB / 2;

enum class BenchmarkOperation {
    Decode,
    Encode,
    Resample,
};

ErrorOr<int> serenity_main(Main::Arguments args)
{
    StringView path {};
    int sample_count = -1;
    int sample_rate = -1;
    BenchmarkOperation operation { BenchmarkOperation::Decode };

    Core::ArgsParser args_parser;
    args_parser.set_general_help("Benchmark audio operations");
    args_parser.add_positional_argument(path, "Path to audio input file", "path");
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Which kind of benchmark to run",
        .long_name = "type",
        .short_name = 't',
        .value_name = "benchmark-kind",
        .accept_value = [&](auto string_operation) -> bool {
            if (string_operation == "encode")
                operation = BenchmarkOperation::Encode;
            else if (string_operation == "decode")
                operation = BenchmarkOperation::Decode;
            else if (string_operation == "resample")
                operation = BenchmarkOperation::Resample;
            else
                return false;

            return true;
        },
    });
    args_parser.add_option(sample_count, "How many samples to load at maximum", "sample-count", 's', "samples");
    args_parser.add_option(sample_rate, "Target sample rate for encoding (without resampling) or resampling benchmark", "sample-rate", 'r', "rate");
    args_parser.parse(args);

    TRY(Core::System::unveil(TRY(FileSystem::absolute_path(path)), "r"sv));
    TRY(Core::System::unveil(nullptr, nullptr));
    TRY(Core::System::pledge("stdio recvfd rpath"));

    auto maybe_loader = Audio::Loader::create(path);
    if (maybe_loader.is_error()) {
        warnln("Failed to load audio file: {}", maybe_loader.error().description);
        return 1;
    }
    auto loader = maybe_loader.release_value();

    Core::ElapsedTimer sample_timer { true };
    i64 total_loader_time = 0;
    int remaining_samples = sample_count > 0 ? sample_count : NumericLimits<int>::max();
    unsigned total_loaded_samples = 0;

    Vector<Audio::Sample> all_samples;

    for (;;) {
        if (remaining_samples > 0) {
            sample_timer = sample_timer.start_new();
            auto samples = loader->get_more_samples(min(MAX_CHUNK_SIZE, remaining_samples));
            total_loader_time += sample_timer.elapsed_milliseconds();
            if (!samples.is_error()) {
                remaining_samples -= samples.value().size();
                total_loaded_samples += samples.value().size();
                if (samples.value().size() == 0)
                    break;
                if (operation != BenchmarkOperation::Decode) {
                    // If we OOM here, just don't regard all the samples.
                    (void)all_samples.try_append(samples.value().data(), samples.value().size());
                }
            } else {
                warnln("Error while loading audio: {}", samples.error().description);
                return 1;
            }
        } else
            break;
    }

    if (operation == BenchmarkOperation::Encode) {
        auto format = LexicalPath::extension(path);
        if (format == "wav"sv) {
            warnln("WAV encoding cannot be tested in-process at the moment");
            return 1;
        } else if (format == "flac"sv) {
            // FIXME: Use AllocatingMemoryStream once it's seekable.
            auto output_data = TRY(ByteBuffer::create_uninitialized(TRY(FileSystem::size(path)) * 2));
            auto output_stream = make<FixedMemoryStream>(output_data.span());
            sample_timer = sample_timer.start_new();
            auto flac_writer = TRY(Audio::FlacWriter::create(
                move(output_stream),
                loader->sample_rate(),
                loader->num_channels(),
                loader->bits_per_sample()));
            flac_writer->sample_count_hint(all_samples.size());
            TRY(flac_writer->finalize_header_format());
            TRY(flac_writer->write_samples(all_samples));
            total_loader_time = sample_timer.elapsed_milliseconds();
        } else {
            warnln("Codec {} is not supported for encoding", format);
            return 1;
        }
    } else if (operation == BenchmarkOperation::Resample) {
        if (sample_rate == -1)
            sample_rate = loader->sample_rate();
        auto sample_output = TRY(FixedArray<Audio::Sample>::create(sample_rate));
        auto input_span = all_samples.span();
        sample_timer = sample_timer.start_new();
        auto resampler = TRY((DSP::InterpolatedSincResampler<Audio::Sample, DSP::recommended_float_sinc_taps, DSP::recommended_float_oversample>::create(loader->sample_rate(), sample_rate, loader->sample_rate(), DSP::recommended_float_sinc_taps, 2'000)));
        while (!input_span.is_empty()) {
            resampler.process(input_span.trim(loader->sample_rate()), sample_output.span());
            input_span = input_span.slice(min(loader->sample_rate(), input_span.size()));
        }
        total_loader_time = sample_timer.elapsed_milliseconds();
    }

    auto time_per_sample = static_cast<double>(total_loader_time) / static_cast<double>(total_loaded_samples) * 1000.;
    auto playback_time_per_sample = (1. / static_cast<double>(loader->sample_rate())) * 1000'000.;

    auto title = ""sv;
    if (operation == BenchmarkOperation::Encode)
        title = "Encoded"sv;
    else if (operation == BenchmarkOperation::Decode)
        title = "Decoded"sv;
    else if (operation == BenchmarkOperation::Resample)
        title = "Resampled"sv;

    outln("{} {:10d} samples in {:06.3f} s, {:9.3f} µs/sample, {:6.1f}% speed (realtime {:9.3f} µs/sample)", title, total_loaded_samples, static_cast<double>(total_loader_time) / 1000., time_per_sample, playback_time_per_sample / time_per_sample * 100., playback_time_per_sample);

    return 0;
}
