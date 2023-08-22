## Name

![Icon](/res/icons/16x16/audio-volume-high.png) SoundSettings - Sound settings application

[Open](file:///bin/SoundSettings)

## Synopsis

```**sh
$ SoundSettings
```

## Description

SoundSettings controls various sound-related settings on SerenityOS.

The Output tab configures the sound output of the system. You can set volume and mute state, and configure the sample rate of the audio device.

The Notification Sounds tab configures the notification sound behavior of the system. You can select one of the available notification sound collections available on your system. For more information see [`/res/sounds`(5)](help://man/5/sounds). You can also disable notification sounds, which is the default.

## Files

SoundSettings communicates with AudioServer, which uses the `Audio.ini` configuration file.

SoundSettings detects notification sound collections in `/res/sounds`.

## See Also

- [`/res/sounds`(5)](help://man/5/sounds)
- [Audio-subsystem(7)](help://man/7/Audio-subsystem)
