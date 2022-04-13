serenity_component(
    LoggingServer
    REQUIRED
    TARGETS LoggingServer
)

compile_ipc(LoggingServer.ipc LoggingServerEndpoint.h)
compile_ipc(LoggingClient.ipc LoggingClientEndpoint.h)

set(SOURCES
    ConnectionFromClient.cpp
    main.cpp
    LoggingClientEndpoint.h
    LoggingServerEndpoint.h
)

serenity_bin(LoggingServer)
target_link_libraries(LoggingServer LibCore LibIPC LibMain)
