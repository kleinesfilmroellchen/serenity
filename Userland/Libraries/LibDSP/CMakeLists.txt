set(SOURCES
    BandAdjustmentFilter.cpp
    Clip.cpp
    Processors/BandAdjustment.cpp
    Processors/Delay.cpp
    Processors/Mastering.cpp
    Synthesizers.cpp
    Keyboard.cpp
    Track.cpp
)

serenity_lib(LibDSP dsp)
target_link_libraries(LibDSP PRIVATE LibCore)
