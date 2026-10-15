#pragma once

#include <QString>

/**
 * Where the application keeps its writable state.
 *
 * On Windows the shipped build is a portable archive, so everything lives next
 * to the executable whenever a `portable.txt` marker is present there. Without
 * the marker (developer builds, installed packages) the OS locations are used.
 */
namespace AppPaths {

/** True when a `portable.txt` marker sits next to the executable. */
bool isPortable();

/** Root of writable data: `<appdir>/Data` when portable, OS location else. */
QString dataRoot();

/** Full path of the ini file; the parent directory is created on demand. */
QString settingsFile();

/** Directory holding `lkplayer.log`. */
QString logDir();

/** Directory holding the generated covers. */
QString coverCacheDir();

} // namespace AppPaths
