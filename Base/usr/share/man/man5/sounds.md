## Name

/res/sounds - Notification sound collections

## Description

Notification sounds are played during various system events. To make selecting and distributing notification sounds easier, notifications are grouped into "collections", and each collection is in fact a single audio file. The `/res/sounds` directory contains the available notification sound collections. [`SoundSettings`(1)](help://man/1/Applications/SoundSettings) will detect all notification sound collections and allow them to be selected.

## Notification Sound Collection File Format

Notification sound collections are stored as FLAC files with additional metadata, allowing AudioServer to determine which exact sounds this collection provides and how to play them accurately. Note that the audio data of the collection file is spec-compliant FLAC data, and using the streamable subset of FLAC is recommended. It is further recommended that collections have one or two channels, use either 44.1 or 48 kHz sample rates, and have a bit depth of 16 bits (or more).

Within the collection file, all individual sounds are stringed together one after another. For the purposes of storing information about specific notification sounds, a special [FLAC Application metadata block](https://xiph.org/flac/format.html#metadata_block_application) is used. The application ID is (big-endian) 0x534f534e, corresponding to "SOSN" (SerenityOS Sound Notification format). Note that this ID is unregistered with Xiph as of August 2023 (see [the official registry](https://xiph.org/flac/id.html)), so it may be the case that other applications use the same metadata block, and therefore will not be able to read it.

A SOSN block consists of a list of notification sound entries, each exactly 24 bytes long. Each entry has the following format; all data stored big-endian:

| Bytes | Entry                                                                                                                                               |
| ----- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| 8     | Sample number where this notification sound starts.                                                                                                 |
| 16    | ID of the notification sound in UTF-8; see below. If the notification sound is shorter than 16 bytes, the end is padded with ignored spaces (0x20). |

Notification sounds stretch from their start point until either the start point of the next notification sound, or the end of the file. The notification sounds do not have to be ordered by start point (different from seek table entries). Since seeking to the start of a notification sound is a common operation, it is strongly recommended that (1) this start point lands on the beginning of a FLAC frame, and (2) this start point be in the (regular) FLAC seek table.

The display name of the collection is derived from either the file name, or (if available) the title given in regular Vorbis metadata.

## Notification Sound Types

LibAudio defines a couple of standard notification sound types. The ID of each type is a UTF-8 string up to 16 bytes long, and when stored in the SOSN block, IDs are padded with spaces at the end to fit into 16 bytes. Note that all programs handling the SOSN block will (at least by default) ignore unknown types, which means that the format is backwards-compatible when new types are added. Duplicate IDs within one file are strongly discouraged; the file will remain readable but the exact sound used is unspecified.

General sounds:

-   `error`: Generic error, when something goes wrong.
-   `startup`: Startup sound, when the system is booted.
-   `notification`: Sound for application-provided notifications.
-   The bell character (0x0a): Sound for the terminal bell (0x0a) character, when an audible bell is enabled in the terminal.
-   `plug-in`: Device was plugged in.
-   `plug-out`: Device was plugged out.
-   `volume-check`: Volume check sound, played when the user is adjusting the volume. This sound should be at full gain and provide good feedback for the current sound level.

Game sounds:

-   `catdog`: CatDog's sound.
-   `card-shuffle`: Cards are shuffled and distributed to players.
-   `card-move`: Moving cards around.
-   `piece-move`: Moving a (non-card) piece in a game, such as chess pieces.
-   `click`: A generic click sound. Occasionally used by regular applications; most relevant for games.
-   `explode`: Any explosion event in a game, such as clicking a mine in Minesweeper.

## Fallback

The notification collection named `default.flac` provides defaults for all sounds that a collection cannot provide. In particular, many collections will not provide sounds for games, while this collection provides the default game sounds.

## See Also

-   [`notifysoundc`(1)](help://man/1/notifysoundc) can compile sounds into notification sound collections
-   [`SoundSettings`(5)](help://man/1/Applications/SoundSettings) chooses sounds from /res/sounds
