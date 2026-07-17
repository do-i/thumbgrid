#pragma once

#include <QString>
#include <functional>

// Shared "safe overwrite" sequence used by anything that saves pixel data
// over a file that may already exist: back the existing file up to a
// hash-suffixed tmp file, run the caller-supplied save step, then either
// drop the backup (on success) or restore it (on failure).
namespace SafeSave {

// destPath is what gets backed up (and what the tmp file name is derived
// from); restorePath is where the backup is written back to if doSave()
// fails. They are almost always the same path - kept separate only because
// existing callers are not fully consistent about it.
bool withBackup(const QString &destPath, const QString &restorePath, const std::function<bool()> &doSave);

}
