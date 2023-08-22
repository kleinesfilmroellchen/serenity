/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/TypeCasts.h>
#include <LibAudio/NotificationSoundCollection.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibMain/Main.h>

struct SampleRange {
    Optional<size_t> start {};
    Optional<size_t> end {};
};

static ErrorOr<Audio::Notifications::SoundID> create_sound_id(StringView id)
{
    if (id.length() > Audio::Notifications::SoundID::fixed_length())
        return Error::from_string_view("Sound ID is too long"sv);

    return Audio::Notifications::SoundID { id };
}

class Operation {
public:
    virtual ~Operation() = default;

    // Returns all errors, even those that are considered warnings under non-strict mode.
    virtual ErrorOr<void> execute(Audio::Notifications::SoundCollection& collection, bool verbose) const = 0;
};

class AddOperation final : public Operation {
public:
    virtual ~AddOperation() = default;
    AddOperation(StringView path)
        : m_path(MUST(String::from_utf8(path)))
    {
    }

    ErrorOr<void> set_id(StringView id)
    {
        m_override_id = TRY(create_sound_id(id));
        return {};
    }

    ErrorOr<void> set_range(StringView unparsed_range)
    {
        auto const parts = unparsed_range.split_view('-', SplitBehavior::KeepEmpty);
        if (parts.size() != 2)
            return Error::from_string_view("Range must be of the form 'start-end'"sv);

        auto const unparsed_start = parts.first();
        auto const unparsed_end = parts.last();
        SampleRange new_range;
        if (!unparsed_start.is_empty()) {
            new_range.start = unparsed_start.to_uint<size_t>();
            if (!new_range.start.has_value())
                return Error::from_string_view("Invalid start sample number"sv);
        }
        if (!unparsed_end.is_empty()) {
            new_range.end = unparsed_end.to_uint<size_t>();
            if (!new_range.end.has_value())
                return Error::from_string_view("Invalid end sample number"sv);
        }

        m_trim_range = new_range;
        return {};
    }

    virtual ErrorOr<void> execute(Audio::Notifications::SoundCollection& collection, bool verbose) const override
    {
        auto maybe_loader = Audio::Loader::create(m_path);
        if (maybe_loader.is_error())
            return Error::from_string_view("Input audio invalid"sv);

        auto loader = maybe_loader.release_value();
        auto maybe_audio = loader->get_more_samples(loader->total_samples());
        if (maybe_audio.is_error())
            return Error::from_string_view("Input audio invalid"sv);
        auto original_audio = maybe_audio.release_value();
        auto audio = original_audio.span();

        if (m_trim_range.end.has_value() && m_trim_range.start.has_value()) {
            if (m_trim_range.start.value() > m_trim_range.end.value())
                return Error::from_string_view("Trim end is before start"sv);
            audio = audio.slice(m_trim_range.start.value(), m_trim_range.end.value() - m_trim_range.start.value());
        } else if (m_trim_range.start.has_value()) {
            audio = audio.slice(m_trim_range.start.value());
        } else if (m_trim_range.end.has_value()) {
            audio = audio.trim(m_trim_range.end.value());
        }

        auto file_name = LexicalPath::title(m_path.to_deprecated_string());
        auto id = m_override_id.value_or_lazy_evaluated_optional([&] {
                                   return loader->metadata().title.map([&](String const& title) {
                                       return MUST(create_sound_id(title.code_points().substring_view(0, min(title.bytes().size(), Audio::Notifications::SoundID::fixed_length())).as_string()));
                                   });
                               })
                      .value_or_lazy_evaluated([&] {
                          return Audio::Notifications::SoundID {
                              file_name.substring(0, min(file_name.length(), Audio::Notifications::SoundID::fixed_length()))
                          };
                      });

        if (verbose)
            dbgln("Added sound '{}' with sample length {} from file {}", id.representable_view(), audio.size(), m_path);
        // TODO: Resample the sound into the common sample rate.
        TRY(collection.add_sound(id, audio));
        return {};
    }

private:
    String m_path;
    Optional<Audio::Notifications::SoundID> m_override_id;
    SampleRange m_trim_range;
};

class RemoveOperation final : public Operation {
public:
    virtual ~RemoveOperation() = default;
    RemoveOperation(Audio::Notifications::SoundID id)
        : m_id(id)
    {
    }

    static ErrorOr<NonnullOwnPtr<RemoveOperation>> create(StringView id)
    {
        return try_make<RemoveOperation>(TRY(create_sound_id(id)));
    }

    virtual ErrorOr<void> execute(Audio::Notifications::SoundCollection& collection, bool verbose) const override
    {
        (void)collection;
        (void)verbose;
        return {};
    }

private:
    Audio::Notifications::SoundID m_id;
};

class RenameOperation final : public Operation {
public:
    virtual ~RenameOperation() = default;
    RenameOperation(Audio::Notifications::SoundID old_id, Audio::Notifications::SoundID new_id)
        : m_old_id(old_id)
        , m_new_id(new_id)
    {
    }

    static ErrorOr<NonnullOwnPtr<RenameOperation>> create(StringView unparsed_ids)
    {
        auto ids = unparsed_ids.split_view(' ');
        if (ids.size() != 2)
            return Error::from_string_view("Exactly two ids, separated by space, must be given"sv);
        return try_make<RenameOperation>(TRY(create_sound_id(ids.first())), TRY(create_sound_id(ids.last())));
    }

    virtual ErrorOr<void> execute(Audio::Notifications::SoundCollection& collection, bool verbose) const override
    {
        (void)collection;
        (void)verbose;
        return {};
    }

private:
    Audio::Notifications::SoundID m_old_id;
    Audio::Notifications::SoundID m_new_id;
};

class ExtractSingleOperation final : public Operation {
public:
    virtual ~ExtractSingleOperation() = default;
    ExtractSingleOperation(Audio::Notifications::SoundID id)
        : m_id(id)
    {
    }

    static ErrorOr<NonnullOwnPtr<ExtractSingleOperation>> create(StringView id)
    {
        return try_make<ExtractSingleOperation>(TRY(create_sound_id(id)));
    }

    virtual ErrorOr<void> execute(Audio::Notifications::SoundCollection& collection, bool verbose) const override
    {
        (void)collection;
        (void)verbose;
        return {};
    }

private:
    Audio::Notifications::SoundID m_id;
};

class ExtractAllOperation final : public Operation {
public:
    virtual ~ExtractAllOperation() = default;
    ExtractAllOperation(StringView path)
        : m_path(MUST(String::from_utf8(path)))
    {
    }

    virtual ErrorOr<void> execute(Audio::Notifications::SoundCollection& collection, bool verbose) const override
    {
        (void)collection;
        (void)verbose;
        return {};
    }

private:
    String m_path;
};

static void list_sound_collection(Audio::Notifications::SoundCollection const& collection)
{
    outln("Collection '{}'", collection.name());
    for (auto const& entry : collection) {
        auto const& sound = entry.value;
        auto const length = static_cast<double>(sound.audio.size()) / static_cast<double>(collection.sample_rate());
        outln("  {:<16}: {:.1f}s", sound.id.representable_view(), length);
    }
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio rpath cpath wpath"));
    // TODO: Unveil only paths we access after reading command-line options.

    bool strict_mode { false };
    bool verbose { false };
    bool make_new_collection { false };
    String name;
    String collection_path;

    Vector<NonnullOwnPtr<Operation>> operations;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("Compile multiple sounds into notification sound collections.");
    args_parser.add_option(strict_mode, "Operate in strict mode, enforcing all format and operation warnings", "strict", 's');
    args_parser.add_option(verbose, "Perform verbose logging", "verbose", 'v');
    args_parser.add_option(make_new_collection, "Always create a new collection and overwrite an existing one", "new", 'n');
    args_parser.add_option(name, "Set the display name of the collection", "name", 0, "name");

    // We use errors for reporting incorrect arguments, but ArgsParser expects errors to only occur with OOM and the likes.
    auto wrap_erroring = [&](auto function) {
        return [function = move(function)](auto value) {
            auto result = function(value);
            if (result.is_error()) {
                warnln("error: {}", result.release_error());
                return false;
            }
            return true;
        };
    };

    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Add one or more sounds to the collection",
        .long_name = "add",
        .short_name = 'a',
        .value_name = "path",
        .accept_value = [&](auto path) -> ErrorOr<bool> {
            TRY(operations.try_append(TRY(try_make<AddOperation>(path))));
            return true;
        },
    });
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Set the notification sound ID for the last new sound specified via --add",
        .long_name = "id",
        .short_name = 'i',
        .value_name = "id",
        .accept_value = wrap_erroring([&](auto id) -> ErrorOr<void> {
            if (operations.is_empty() || !is<AddOperation>(operations.last().ptr()))
                return Error::from_string_view("--id can only be specified directly after --add"sv);

            return static_cast<AddOperation*>(operations.last().ptr())->set_id(id);
        }),
    });
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Trim the last new notification sound specified via --add to a certain sample range, using the syntax 'start-end'",
        .long_name = "trim",
        .short_name = 't',
        .value_name = "sample_range",
        .accept_value = wrap_erroring([&](auto range) -> ErrorOr<void> {
            if (operations.is_empty() || !is<AddOperation>(operations.last().ptr()))
                return Error::from_string_view("--trim can only be specified directly after --add"sv);

            return static_cast<AddOperation*>(operations.last().ptr())->set_range(range);
        }),
    });
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Remove a sound with the given ID",
        .long_name = "remove",
        .short_name = 'd',
        .value_name = "id",
        .accept_value = [&](auto id) -> ErrorOr<bool> {
            TRY(operations.try_append(TRY(RemoveOperation::create(id))));
            return true;
        },
    });
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Rename a sound with the first given ID to the second given ID",
        .long_name = "rename",
        .short_name = 'r',
        .value_name = "oldid newid",
        .accept_value = [&](auto unparsed_ids) -> ErrorOr<bool> {
            TRY(operations.try_append(TRY(try_make<AddOperation>(unparsed_ids))));
            return true;
        },
    });
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Extract a sound with the given ID",
        .long_name = "extract",
        .value_name = "id",
        .accept_value = [&](auto id) -> ErrorOr<bool> {
            TRY(operations.try_append(TRY(ExtractSingleOperation::create(id))));
            return true;
        },
    });
    args_parser.add_option(Core::ArgsParser::Option {
        .help_string = "Extract all sounds to a directory with the given path",
        .long_name = "extract-all",
        .value_name = "path",
        .accept_value = [&](auto path) -> ErrorOr<bool> {
            TRY(operations.try_append(TRY(try_make<ExtractAllOperation>(path))));
            return true;
        },
    });

    args_parser.add_positional_argument(collection_path, "Notification collection to create or operate on", "collection");
    args_parser.parse(arguments, Core::ArgsParser::FailureBehavior::Ignore);

    if (collection_path.is_empty()) {
        warnln("error: collection is required");
        return 1;
    }

    Optional<Audio::Notifications::SoundCollection> existing_collection;
    if (!make_new_collection) {
        auto maybe_collection_file = Core::File::open(collection_path, Core::File::OpenMode::Read);
        if (!maybe_collection_file.is_error()) {
            auto collection_load_result = Audio::Notifications::SoundCollection::load(maybe_collection_file.release_value());
            if (collection_load_result.is_error() && strict_mode) {
                warnln("error: collection at {} is invalid: {}", collection_path, collection_load_result.release_error());
                return 2;
            } else if (!collection_load_result.is_error()) {
                existing_collection = collection_load_result.release_value();
            }
        } else if (strict_mode) {
            warnln("error: could not reuse collection at {}: {}", collection_path, maybe_collection_file.release_error());
            return 2;
        } else {
            warnln("warning: collection at {} is invalid, creating a new one anyways", collection_path);
        }
    }
    // value_or will not automatically select the moving overload, so we have to do this manually.
    auto collection = existing_collection.has_value() ? existing_collection.release_value() : Audio::Notifications::SoundCollection {};

    if (operations.is_empty() || verbose)
        list_sound_collection(collection);

    for (auto const& operation : operations) {
        auto result = operation->execute(collection, verbose);
        if (result.is_error()) {
            if (strict_mode) {
                warnln("error: {}", result.release_error());
                return 2;
            }
            warnln("warning: {}", result.release_error());
        }
    }

    if (!operations.is_empty()) {
        auto output_file = TRY(Core::File::open(collection_path, Core::File::OpenMode::Write | Core::File::OpenMode::Truncate));
        TRY(collection.write(move(output_file)));
    }

    return 0;
}
