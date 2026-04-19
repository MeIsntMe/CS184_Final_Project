1. Download CUDA toolkit - Not configured yet
2. go to your vcpkg and download glew
    command .\vcpkg install glfw3:x64-windows
3. Make your own CMakePresets.json if you want
    {
        "version": 3,
        "configurePresets": [
            {
            "name": "default",
            "displayName": "Default with vcpkg",
            "generator": "Visual Studio 17 2022",
            "binaryDir": "${sourceDir}/build",
            "cacheVariables": {
                "CMAKE_TOOLCHAIN_FILE": "path/to/vcpkg",
                "VCPKG_TARGET_TRIPLET": "x64-windows"
            }
            }
        ]
    }