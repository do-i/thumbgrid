#pragma once

#include <QList>
#include <QString>
#include <functional>

// One row on the settings dialog's "Stored data" page: a persisted store of
// user-derived data (paths, caches) that can be cleared independently.
struct StoredDataStore {
    QString id;                      // stable id, persisted in State/clearOnExit
    QString title;                   // translated display name
    QString description;             // translated tooltip text
    std::function<QString()> detail; // human summary ("3 items", "48 MB"), computed on demand
    std::function<void()> clear;     // deletes the store's data
};

namespace StoredData {

QList<StoredDataStore> stores();

// Clears every store whose id is in State/clearOnExit. Runs from the exit
// path after Core has written its final session state.
void clearStoresMarkedForExit();

}
