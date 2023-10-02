serenity_component(
    Fidewepre
    RECOMMENDED
    TARGETS Fidewepre
    DEPENDS ImageDecoder FileSystemAccessServer
)

compile_gml(PresenterSettingsFooter.gml PresenterSettingsFooterGML.cpp)
compile_gml(PresenterSettingsPerformance.gml PresenterSettingsPerformanceGML.cpp)

set(SOURCES
    main.cpp
    PDFPage.cpp
    Presentation.cpp
    PresenterSettings.cpp
    PresenterWidget.cpp
    Slide.cpp
    SlideObject.cpp
    PresenterSettingsFooterGML.cpp
    PresenterSettingsPerformanceGML.cpp
)

serenity_app(Fidewepre ICON app-presenter)
target_link_libraries(Fidewepre PRIVATE
    LibCore LibMain LibGUI LibGfx LibThreading
    LibFileSystemAccessClient LibImageDecoderClient LibConfig LibIPC LibPDF
    LibSyntax LibWeb LibJS LibCMake LibCpp LibMarkdown LibSQL LibShell
)
