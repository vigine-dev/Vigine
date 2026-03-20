# vigine

## Prerequisites

Before compiling the project, you need to install the Vulkan SDK:

1. Download and install the Vulkan SDK from the official website:  
   [https://vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)

2. Select the latest version for your platform.

3. After installation, ensure that the `VULKAN_SDK` environment variable is set (usually done automatically by the installer).

4. If the installer did not set the variable, add the path to the SDK (e.g., `C:\VulkanSDK\1.3.x.x` on Windows) to the `VULKAN_SDK` environment variable.

You also need PostgreSQL client libraries and `libpqxx`:

1. Install PostgreSQL 16 (or compatible) so `libpq` is available.

2. Install `libpqxx` using vcpkg in the project root:

   `vcpkg install --triplet x64-windows`

3. Ensure `vcpkg.json` contains at least the following:

```json
{
   "name": "vigine",
   "version-string": "0.1.0",
   "dependencies": [
      "libpqxx"
   ],
   "builtin-baseline": "<your-vcpkg-baseline-commit>"
}
```

Where `<your-vcpkg-baseline-commit>` is a commit hash from the vcpkg registry that pins package versions for reproducible builds.

How to get it:

1. Run this command in the project root:

   `vcpkg x-update-baseline --add-initial-baseline`

2. vcpkg will automatically write/update the `builtin-baseline` field in `vcpkg.json`.

How to update it later (optional):

`vcpkg x-update-baseline`

4. Keep `vcpkg.json` in the repository and configure CMake with the vcpkg toolchain so `find_package(libpqxx CONFIG REQUIRED)` works.