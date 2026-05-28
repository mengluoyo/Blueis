#ifndef BLUEIS_PERSISTENCE_SNAPSHOT_H
#define BLUEIS_PERSISTENCE_SNAPSHOT_H

#include <string>

namespace blueis {

class StorageEngine;

class Snapshot {
public:
    static bool save(const std::string& path);
    static bool load(const std::string& path);
};

} // namespace blueis

#endif // BLUEIS_PERSISTENCE_SNAPSHOT_H
