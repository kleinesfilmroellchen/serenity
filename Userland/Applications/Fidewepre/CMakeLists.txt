serenity_component(
    Fidewepre
    RECOMMENDED
    TARGETS Fidewepre
    DEPENDS ImageDecoder FileSystemAccessServer
)

compile_gml(PresenterSettingsFooter.gml PresenterSettingsFooterGML.h presenter_settings_footer_gml)

set(SOURCES
    main.cpp
    Presentation.cpp
    PresenterSettings.cpp
    PresenterWidget.cpp
    Slide.cpp
    SlideObject.cpp
)

set(GENERATED_SOURCES
    PresenterSettingsFooterGML.h
)

serenity_app(Fidewepre ICON app-display-settings)
target_link_libraries(Fidewepre PRIVATE LibImageDecoderClient LibGUI LibGfx LibFileSystemAccessClient LibConfig LibCore LibMain LibThreading)
