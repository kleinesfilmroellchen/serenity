serenity_component(
    Fidewepre
    RECOMMENDED
    TARGETS Fidewepre
    DEPENDS ImageDecoder FileSystemAccessServer
)


set(SOURCES
    main.cpp
    Presentation.cpp
    PresenterWidget.cpp
    Slide.cpp
    SlideObject.cpp
)
serenity_app(Fidewepre ICON app-display-settings)
target_link_libraries(Fidewepre PRIVATE LibImageDecoderClient LibGUI LibGfx LibFileSystemAccessClient LibCore LibMain)
