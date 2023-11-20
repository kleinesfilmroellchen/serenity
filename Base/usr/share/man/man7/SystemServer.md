## Name

SystemServer - one server to rule them all

## Description

SystemServer is the first userspace process to be started by the kernel on boot.
Its main responsibility is spawning all the other servers and other programs
that need to be autostarted (referred to as **services**).
It also manages targets, which are effectively collections of services. Collectively, these two are referred to as units.

More specifically, SystemServer has these responsibilities:
-   Serve as the userspace init process. This means setting up the bare necessary device nodes and symlinks to make UNIX programs work.
-   As userspace init, bring up the multiuser environment according to kernel parameters and configuration files.
-   As user/session init, bring up a user's (graphical or text) session according to the user's configuration.
-   Manage service processes. This means: Starting, stopping, restarting, and scheduling services according to various requests (cronjobs, user commands, socket connections).

## Services

A service is a process, usually a daemon or helper process, that may be started by SystemServer according to a specific setup.

A service can be configured to be *kept alive*, in which case SystemServer will
respawn it if it exits or crashes. A service may also be configured as *lazy*,
in which case SystemServer won't spawn it immediately, but only once a client
connects to its socket (see **Socket takeover** below).

### Socket takeover

SystemServer can be configured to set up a socket on behalf of a service
(typically, an *IPC portal* socket inside `/tmp/portal/`). SystemServer sets up
the configured sockets before spawning any services, preventing any races
between socket creation and the client trying to connect to those sockets.

When a service is spawned, SystemServer passes it an open file descriptor to the
configured socket as fd 3, and sets `SOCKET_TAKEOVER=1` in the environment to
inform the service that socket takeover is happening. SystemServer calls
[`listen`(2)](help://man/2/listen) on the file descriptor, so the service doesn't
need to do it again. The file descriptor does not have the `FD_CLOEXEC` flag set
on it.

The service is advised to set this flag using [`fcntl`(2)](help://man/2/fcntl) and
unset `SOCKET_TAKEOVER` from the environment in order not to confuse its
children.

LibCore provides `Core::LocalServer::take_over_from_system_server()` method that
performs the service side of the socket takeover automatically.

If a service is configured as *lazy*, SystemServer will actually listen on the
socket it sets up for the service, and only spawn the service once a client
tries to connect to the socket. The service should then start up and accept the
connection. This all happens transparently to the client. If a lazy service is
configured to be *kept alive*, it can even exit after some period of inactivity;
in this case SystemServer will respawn it again once there is a new connection
to its socket.

SystemServer can also be configured to accept connections on the socket and
spawn separate instances of the service for each accepted connection, passing
the accepted socket to the service process.

## Targets

A SystemServer target is a defined system state which is defined by the state of the various services (usually running or ready-to-run lazy services). The most important use cases for targets are:
- booting the system into a known state for a specific purpose (like graphical or text mode)
- perform power state switching (like shutdown or reboot)

Normal targets can be defined in the configuration file by the user. Activating a target will start all its dependent services and targets. Once that is achieved a target is said to be _reached_.

### Special targets

There exist special-case targets like `shutdown` which are managed differently from other targets. They don't have an entry in the configuration file since their name and purpose is fixed.

## History

The concepts target and unit are are borrowed from the [systemd](https://systemd.io/) init system for Linux.

## See also

* [`SystemServer`(5)](help://man/5/SystemServer)
