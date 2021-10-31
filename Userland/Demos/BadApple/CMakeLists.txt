serenity_component(
    BadApple
    TARGETS BadApple
)

set(SOURCES
    BadApple.cpp
)

serenity_app(BadApple ICON app-badapple)
target_link_libraries(BadApple LibGUI LibCore LibGfx LibAudio)
