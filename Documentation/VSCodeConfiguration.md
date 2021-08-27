## Visual Studio Code Project Configuration

Visual Studio Code does not work optimally for Serenity development, and there's a bunch of configuring and fiddling around you'll have to do.

The WSL Remote extension allows you to use VSCode in Windows while using the normal WSL workflow. This works surprisingly well, but for code comprehension speed you should put the Serenity directory on your WSL root partition.

### Note on CMake

The CMake Tools plugin for VSCode does not work with projects that don't accept a CMAKE_BUILD_TYPE. See also [this CMake Tools issue](https://github.com/microsoft/vscode-cmake-tools/issues/1639); an appropriate feature is planned for 1.9.0. For now, it is best to disable all CMake extensions when working on Serenity.

### clangd

The official clangd extension can be used for C++ comprehension. You'll have to use the following .clangd:

```yaml
CompileFlags:
  CompilationDatabase: Build/i686
```

Run cmake at least once for this to work. Note that clangd will report fake errors, as do the Microsoft C/C++ tools.

### Microsoft C/C++ tools

These extensions can be used as-is, but you need to point them to the custom Serenity compilers.

### Excludes

Excluding the generated directories keeps your file view clean and speeds up search. Put this in your `settings.json` for Serenity:

```json
"files.exclude": {
	"**/.git": true,
	"Toolchain/Local/**": true,
	"Toolchain/Tarballs/**": true,
	"Toolchain/Build/**": true,
	"Build/**": true,
	"build/**": true,
},
"search.exclude": {
	"**/.git": true,
	"Toolchain/Local/**": true,
	"Toolchain/Tarballs/**": true,
	"Toolchain/Build/**": true,
	"Build/**": true,
	"build/**": true,
},
```

### Customization

#### Custom Tasks

You can create custom tasks (`.vscode/tasks.json`) to quickly compile Serenity. The following are two example tasks, a "just build" task and a "build and run" task for i686 that also give you error highlighting:

```json
{
    "tasks": [
        {
            "label": "build",
            "type": "shell",
            "problemMatcher": [
                {
                    "base": "$gcc",
                    "fileLocation": [
                        "relative",
                        "${workspaceFolder}/Build/i686"
                    ]
                },
                {
                    "source": "gcc",
                    "fileLocation": [
                        "relative",
                        "${workspaceFolder}/Build/i686"
                    ],
                    "pattern": [
                        {
                            "regexp": "^([^\\s]*\\.S):(\\d*): (.*)$",
                            "file": 1,
                            "location": 2,
                            "message": 3
                        }
                    ]
                }
            ],
            "command": [
                "bash"
            ],
            "args": [
                "-c",
                "\"Meta/serenity.sh image\""
            ],
            "presentation": {
                "echo": true,
                "reveal": "always",
                "focus": false,
                "group": "build",
                "panel": "shared",
                "showReuseMessage": true,
                "clear": true
            }
        },
        {
            "label": "launch",
            "type": "shell",
            "linux": {
                "command": "bash",
                "args": [
                    "-c",
                    "\"Meta/serenity.sh run\""
                ],
                "options": {
                    "env": {
                        // Put your custom run configuration here, e.g. SERENITY_RAM_SIZE
                    }
                }
            },
            "problemMatcher": [
                {
                    "source": "KUBSan",
                    "owner": "cpp",
                    "fileLocation": [
                        "relative",
                        "${workspaceFolder}"
                    ],
                    "pattern": [
                        {
                            "regexp": "KUBSAN: (.*)",
                            "message": 0
                        },
                        {
                            "regexp": "KUBSAN: at ../(.*), line (\\d*), column: (\\d*)",
                            "file": 1,
                            "line": 2,
                            "column": 3
                        }
                    ]
                },
                {
                    "base": "$gcc",
                    "fileLocation": [
                        "relative",
                        "${workspaceFolder}/Build/i686"
                    ]
                },
                {
                    "source": "gcc",
                    "fileLocation": [
                        "relative",
                        "${workspaceFolder}/Build/i686"
                    ],
                    "pattern": [
                        {
                            "regexp": "^([^\\s]*\\.S):(\\d*): (.*)$",
                            "file": 1,
                            "location": 2,
                            "message": 3
                        }
                    ]
                }
            ],
            "presentation": {
                "echo": true,
                "reveal": "always",
                "group": "run",
                "focus": false,
                "panel": "shared",
                "showReuseMessage": true,
                "clear": true
            }
        },
    ]
}
```

This can easily be adopted into x86_64 by appending the architecture to the serenity.sh commands.

#### License snippet

The following snippet may be useful if you want to quickly generate a license header, put it in `.vscode/serenity.code-snippets`:
```json
{
    "License": {
        "scope": "cpp,c",
        "prefix": "license",
        "body": [
            "/*",
            " * Copyright (c) $CURRENT_YEAR, ${1:Your Name} <${2:YourName@Email.com}>.",
            " *",
            " * SPDX-License-Identifier: BSD-2-Clause",
            " */"
        ],
        "description": "Licence header"
    }
}
```
