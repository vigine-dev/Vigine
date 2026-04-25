#pragma once

/**
 * @file ifilesystem.h
 * @brief Pure-virtual filesystem abstraction (R-PlatformPortability skeleton).
 *
 * Pure interface for OS-level file operations. The platform-agnostic
 * engine consumes file I/O exclusively through @ref IFileSystem; per-
 * platform concretes live under @c src/impl/platform/<X>/ and ship
 * the actual Win32 / POSIX / mobile-runtime backends.
 *
 * INV-10 compliance: the @c I prefix marks a pure-virtual interface
 * with no state and no non-virtual method bodies. A future
 * @c AbstractFileSystem may carry shared helpers; this file is the
 * thin contract.
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace vigine
{
namespace platform
{

/**
 * @brief Opaque handle returned by @ref IFileSystem::open.
 *
 * The concrete value is platform-defined (Win32 HANDLE, POSIX file
 * descriptor, etc.) and only meaningful inside the matching
 * @ref IFileSystem implementation. Callers treat it as an opaque
 * token and pass it back to the same @ref IFileSystem instance.
 */
using FileHandle = std::uint64_t;

/**
 * @brief Sentinel returned when a file operation cannot produce a
 *        valid handle.
 */
inline constexpr FileHandle kInvalidFileHandle = 0;

/**
 * @brief Open mode flag set used by @ref IFileSystem::open.
 *
 * Bit-flag encoding so multiple modes can be combined (e.g.
 * @c Read | @c Write). Concrete implementations translate the flags
 * to the matching OS-level constants.
 */
enum OpenMode : unsigned int
{
    OpenModeRead   = 1u << 0,
    OpenModeWrite  = 1u << 1,
    OpenModeAppend = 1u << 2,
    OpenModeCreate = 1u << 3,
    OpenModeBinary = 1u << 4,
};

/**
 * @brief Pure-virtual root interface for filesystem access.
 *
 * The contract is intentionally narrow. Each method maps to one
 * common OS primitive (open, close, read, write, exists). Higher-
 * level helpers (path manipulation, recursive walks, etc.) belong on
 * a future @c AbstractFileSystem helper base, not on this interface.
 */
class IFileSystem
{
  public:
    virtual ~IFileSystem() = default;

    /**
     * @brief Open a file with the given mode flags.
     *
     * @param path  UTF-8 file path. The implementation is responsible
     *              for converting to the platform-native encoding.
     * @param flags Bit-or of @ref OpenMode values.
     * @return Opaque handle on success; @ref kInvalidFileHandle on
     *         failure.
     */
    [[nodiscard]] virtual FileHandle open(std::string_view path, unsigned int flags) = 0;

    /**
     * @brief Close a previously-opened file handle.
     *
     * @return true on success; false if the handle was already closed
     *         or never valid.
     */
    virtual bool close(FileHandle handle) = 0;

    /**
     * @brief Read up to @p size bytes from the file into @p buffer.
     *
     * @return Number of bytes actually read. Zero indicates EOF or
     *         error; SIZE_MAX is reserved for "operation failed".
     */
    [[nodiscard]] virtual std::size_t read(FileHandle handle, void *buffer, std::size_t size) = 0;

    /**
     * @brief Write up to @p size bytes from @p data to the file.
     *
     * @return Number of bytes actually written. Less than @p size
     *         indicates a short write (disk full, pipe closed, etc.).
     */
    [[nodiscard]] virtual std::size_t write(FileHandle handle, const void *data, std::size_t size) = 0;

    /**
     * @brief Check whether a file exists at @p path.
     *
     * Does not open the file. Symbolic-link resolution is platform-
     * defined; concretes may diverge here.
     */
    [[nodiscard]] virtual bool exists(std::string_view path) const = 0;

    IFileSystem(const IFileSystem &)            = delete;
    IFileSystem &operator=(const IFileSystem &) = delete;
    IFileSystem(IFileSystem &&)                 = delete;
    IFileSystem &operator=(IFileSystem &&)      = delete;

  protected:
    IFileSystem() = default;
};

} // namespace platform
} // namespace vigine
