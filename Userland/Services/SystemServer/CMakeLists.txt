serenity_component(
    SystemServer
    REQUIRED
    TARGETS SystemServer LibSystemServer
)

compile_ipc(SystemControlServer.ipc SystemControlServerEndpoint.h)
compile_ipc(SystemControlClient.ipc SystemControlClientEndpoint.h)

set(SOURCES
    main.cpp
    ConnectionFromClient.cpp
    IPCTypes.cpp
    Service.cpp
    UnitManagement.cpp
)

set(GENERATED_SOURCES
    SystemControlServerEndpoint.h
    SystemControlClientEndpoint.h
)

serenity_bin(SystemServer)
target_link_libraries(SystemServer PRIVATE LibCore LibFileSystem LibIPC LibMain)

set(SOURCES
    IPCTypes.cpp
)

serenity_lib(LibSystemServer systemserver PRIVATE LibCore LibIPC)

# Required for conditionally creating hardcoded /dev/kcov entry
if (ENABLE_KERNEL_COVERAGE_COLLECTION)
    add_compile_definitions(ENABLE_KERNEL_COVERAGE_COLLECTION)
endif()
