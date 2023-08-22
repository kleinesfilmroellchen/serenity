## Name

notifysoundc - compile multiple sounds into notification sound collections

## Synopsis

```**sh
$ notifysoundc [--strict] [--verbose] [--new] [--name name] [--add path [--id id] [--trim sample_range] ]... [--remove id]... [--rename oldid newid]... [--extract id]... [--extract-all path] collection
```

## Description

`notifysoundc` creates and manipulates notification sound collections, as stored in [/res/sounds](help://man/5/sounds) and used by the system for playing notification sounds. `notifysoundc` can perform multiple operations on an existing or new collection at the same time.

`notifysoundc`'s collections have increased strictness in comparison to the format definition itself. In particular, duplicate sound IDs are never created on purpose.

To analyze and list an existing collection, omit any operational options.

### Operations

`notifysoundc` understands and executes these operations:

-   Add one or more sounds to the collection. The optional ID and trim options can be used directly after the add option to modify the sound's properties. By default, the sound's ID is taken from the file name (without extension) and "DC silence" (constant sample values) at the beginning and end are trimmed. A later sound with the same ID overwrites a previous sound, in the order they were specified on the command line or alphabetically within a directory. In strict mode, overriding an existing sound is not possible.
    -   Specifying a directory to be added will add all readable audio files within that directory to the collection, with the id taken directly from the file name (without extension). In this case, the ID option is illegal (but not the trim option, which can be used to apply a common trim to all sounds). In strict mode, an unreadable file within a directory is treated as an error.
-   Remove a sound from the collection, by ID. If the given ID is not present in the collection, in strict mode an error will be issued and a warning otherwise.
-   Rename a sound with the old ID to the new ID. This has the exact same semantics as removing the sound with the old ID, then adding a new sound with the same data under the new ID.
-   Extract a single sound, specified by ID, from the file. The output path is the current directory, and the file name will be the ID with the extension (and format) `.wav`.
-   Extract all sounds from the collection. The exact state of the collection depends on when this operation is issued on the command line. The extraction path is a directory where the sounds are dumped, and this directory is created if necessary. Extracted files are named the same way as for single-sound extraction, with their ID and in WAV format.

Each of these operations may be specified more than once. The order is always exactly the same as specified on the command line, meaning that a later operation may revert the effects of a previous operation. For example, given the collection `foo.flac` without a sound with id `bar`, the command `notifysoundc --strict --add bar.wav --remove bar foo.flac` has no effect, as it adds the sound `bar` and immediately removes it again.

## Options

-   `-v`, `--verbose`: Perform verbose logging to standard output of all operations performed, and increase collection listing detail. This will enforce the output of detailed listings under all circumstances.
-   `-s`, `--strict`: Operate in strict mode. In this mode, all format and operation warnings are treated as errors and abort the program immediately, without the collection being modified on-disk. Strict mode also has an effect on collection listings, making this option useful for checking strict compliance of the collection.
-   `-n`, `--new`: Create a new collection, even if one already exists at that file. This is an operational option, even though its effect always happens before all other operations and it cannot be repeated.
-   `--name`: Set the display name of the collection via standard title metadata. This always overrides a previous title, even though Vorbis Comments (the metadata format of FLAC) allow multiple title entries.
-   `-a`, `--add`: Add one or more sounds to the collection. The detailed behavior of this option is described above, and it is an operational option.
-   `-i`, `--id`: Set the notification sound ID for the last new sound specified via `--add`. In strict mode, this must be a valid standard sound ID known to `notifysoundc`. This option may only be specified once per sound added, and may not be specified for entire directories added via `--add`. It is an operational option.
-   `-t`, `--trim`: Trim the last new notification sound specified via `--add` to a certain sample range. The sample range may be specified in the form `start-end`, where `start` and `end` are sample indices (start inclusive, end exclusive) and either may be omitted to start at sample 0 or end at the end of the file, respectively. Therefore, specifying `--trim -` (omitting both start and end) is a method of overriding the default trim behavior and using the entirety of the audio data in every case. In strict mode, the start and end sample must lie within the actual audio data present, and in every case, start must be smaller or equal to end. This option may only be specified once per sound added, and is an operational option.
-   `-d`, `--remove`: Remove a sound with the given ID. The detailed behavior of this option is described above, and it is an operational option.
-   `-r`, `--rename`: Rename a sound with the first given ID to the second given ID. The detailed behavior of this option is described above, and it is an operational option.
-   `--extract`: Extract a sound with the given ID. The detailed behavior of this option is described above, and it is an operational option.
-   `--extract-all`: Extract all sounds to a directory with the given path. The detailed behavior of this option is described above, and it is an operational option.

Options are divided into operational and non-operational options, and when only the latter are present (i.e. nothing is to be done), the collection's contents are listed.

## Exit status

-   0 - Operations completed successfully
-   1 - Miscellaneous error, such as I/O or audio format error
-   2 - Unrecoverable format error related to the notification format itself
-   3 - Warning treated as error due to `--strict`

## Examples

```sh
# Create a new sound collection from files in a folder.
$ notifysoundc --new --add ~/my_collection /res/my_collection.flac
# Add one sound to that collection.
$ notfiysoundc --add a_new_sound.wav --type new-sound-id /res/my_collection.flac
# Remove one sound from it.
$ notifysoundc --remove plug-out /res/my_collection.flac
# Examine the collection's contents.
$ notifysoundc /res/my_collection.flac
```

## See Also

-   [`/res/sounds`(5)](help://man/5/sounds)
