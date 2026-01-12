#include "dolphin/dvd.h"
#include "host/HostAssets.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
struct HostDvdFile {
    std::FILE* file = nullptr;
    std::string path;
};

struct HostDvdDir {
    std::vector<std::string> entries;
    std::vector<bool> isDir;
};

std::mutex g_mutex;
std::unordered_map<u32, HostDvdDir> g_dirs;
std::unordered_map<std::string, u32> g_entryIds;
std::unordered_map<u32, std::string> g_entryPaths;

u32 g_nextEntryId = 1;
std::string g_currentDir;

std::string normalizeSlashes(const std::string& input) {
    std::string out = input;
    std::replace(out.begin(), out.end(), '\\', '/');
    return out;
}

std::string joinPath(const std::string& base, const std::string& rel) {
    if (base.empty()) {
        return rel;
    }
    if (rel.empty()) {
        return base;
    }
    if (base.back() == '/') {
        return base + rel;
    }
    return base + "/" + rel;
}

std::string resolveFromCwd(const char* path) {
    if (!path || !*path) {
        return std::string();
    }
    std::string in = normalizeSlashes(path);
    if (!in.empty() && in.front() == '/') {
        in.erase(in.begin());
        return in;
    }
    if (g_currentDir.empty()) {
        return in;
    }
    return joinPath(g_currentDir, in);
}

HostDvdFile* getFileInfo(DVDFileInfo* fileInfo) {
    return static_cast<HostDvdFile*>(fileInfo->cb.userData);
}

bool openFile(DVDFileInfo* fileInfo, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    HostDvdFile* info = new HostDvdFile();
    info->file = f;
    info->path = path;

    fileInfo->startAddr = 0;
    fileInfo->length = size < 0 ? 0 : static_cast<u32>(size);
    fileInfo->callback = nullptr;
    fileInfo->cb.userData = info;
    return true;
}

u32 getOrCreateEntryId(const std::string& path) {
    auto it = g_entryIds.find(path);
    if (it != g_entryIds.end()) {
        return it->second;
    }
    u32 id = g_nextEntryId++;
    g_entryIds[path] = id;
    g_entryPaths[id] = path;
    return id;
}
}

extern "C" {

DVDDiskID* DVDGetCurrentDiskID(void) {
    static DVDDiskID diskId;
    static bool initialized = false;
    if (!initialized) {
        std::memset(&diskId, 0, sizeof(diskId));
        diskId.gameName[0] = 'G';
        diskId.gameName[1] = 'Z';
        diskId.gameName[2] = '2';
        diskId.gameName[3] = 'E';
        diskId.company[0] = '0';
        diskId.company[1] = '1';
        diskId.diskNumber = 0;
        diskId.gameVersion = 0x80;
        diskId.streaming = 0;
        diskId.streamingBufSize = 0;
        initialized = true;
    }
    return &diskId;
}

s32 DVDConvertPathToEntrynum(const char* pathPtr) {
    if (!pathPtr) {
        return -1;
    }
    std::string rel = resolveFromCwd(pathPtr);
    std::string resolved = HostResolveAssetPath(rel.c_str());
    std::error_code ec;
    if (!std::filesystem::exists(resolved, ec)) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<s32>(getOrCreateEntryId(resolved));
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
    if (!fileInfo || entrynum <= 0) {
        return FALSE;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_entryPaths.find(static_cast<u32>(entrynum));
    if (it == g_entryPaths.end()) {
        return FALSE;
    }
    return openFile(fileInfo, it->second) ? TRUE : FALSE;
}

BOOL DVDOpen(const char* fileName, DVDFileInfo* fileInfo) {
    if (!fileName || !fileInfo) {
        return FALSE;
    }
    std::string rel = resolveFromCwd(fileName);
    std::string resolved = HostResolveAssetPath(rel.c_str());
    return openFile(fileInfo, resolved) ? TRUE : FALSE;
}

BOOL DVDClose(DVDFileInfo* fileInfo) {
    if (!fileInfo) {
        return FALSE;
    }
    HostDvdFile* info = getFileInfo(fileInfo);
    if (!info) {
        return FALSE;
    }
    if (info->file) {
        std::fclose(info->file);
    }
    delete info;
    fileInfo->cb.userData = nullptr;
    return TRUE;
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio) {
    (void)prio;
    s32 result = DVDReadPrio(fileInfo, addr, length, offset, prio);
    if (callback) {
        callback(result, fileInfo);
    }
    return result >= 0 ? TRUE : FALSE;
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio) {
    (void)prio;
    if (!fileInfo || !addr) {
        return DVD_RESULT_FATAL_ERROR;
    }
    HostDvdFile* info = getFileInfo(fileInfo);
    if (!info || !info->file) {
        return DVD_RESULT_FATAL_ERROR;
    }
    if (offset < 0 || length < 0) {
        return DVD_RESULT_FATAL_ERROR;
    }
    if (static_cast<u32>(offset) > fileInfo->length) {
        return DVD_RESULT_FATAL_ERROR;
    }

    u32 maxRead = fileInfo->length - static_cast<u32>(offset);
    u32 toRead = static_cast<u32>(length);
    if (toRead > maxRead) {
        toRead = maxRead;
    }

    if (std::fseek(info->file, offset, SEEK_SET) != 0) {
        return DVD_RESULT_FATAL_ERROR;
    }

    size_t readCount = std::fread(addr, 1, toRead, info->file);
    return static_cast<s32>(readCount);
}

BOOL DVDGetCurrentDir(char* path, u32 maxlen) {
    if (!path || maxlen == 0) {
        return FALSE;
    }
    std::string dir = g_currentDir;
    if (!dir.empty()) {
        dir.insert(dir.begin(), '/');
    }
    if (dir.size() + 1 > maxlen) {
        return FALSE;
    }
    std::memset(path, 0, maxlen);
    std::memcpy(path, dir.c_str(), dir.size());
    return TRUE;
}

BOOL DVDChangeDir(const char* dirName) {
    if (!dirName) {
        return FALSE;
    }
    std::string input = normalizeSlashes(dirName);
    if (input == "/" || input.empty()) {
        g_currentDir.clear();
        return TRUE;
    }

    std::vector<std::string> parts;
    if (!input.empty() && input.front() == '/') {
        input.erase(input.begin());
    } else if (!g_currentDir.empty()) {
        parts.push_back(g_currentDir);
    }

    size_t start = 0;
    while (start < input.size()) {
        size_t end = input.find('/', start);
        if (end == std::string::npos) {
            end = input.size();
        }
        std::string part = input.substr(start, end - start);
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        start = end + 1;
    }

    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            joined.push_back('/');
        }
        joined.append(parts[i]);
    }
    g_currentDir = joined;
    return TRUE;
}

int DVDOpenDir(const char* dirName, DVDDir* dir) {
    if (!dirName || !dir) {
        return FALSE;
    }

    std::string rel = resolveFromCwd(dirName);
    std::string resolved = HostResolveAssetPath(rel.c_str());

    std::error_code ec;
    if (!std::filesystem::is_directory(resolved, ec)) {
        return FALSE;
    }

    HostDvdDir listing;
    for (auto& entry : std::filesystem::directory_iterator(resolved, ec)) {
        if (ec) {
            break;
        }
        listing.entries.push_back(entry.path().filename().string());
        listing.isDir.push_back(entry.is_directory(ec));
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    u32 id = g_nextEntryId++;
    g_dirs.emplace(id, std::move(listing));
    dir->entryNum = id;
    dir->location = 0;
    dir->next = static_cast<u32>(g_dirs[id].entries.size());
    return TRUE;
}

int DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) {
    if (!dir || !dirent) {
        return FALSE;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_dirs.find(dir->entryNum);
    if (it == g_dirs.end()) {
        return FALSE;
    }
    HostDvdDir& listing = it->second;
    if (dir->location >= listing.entries.size()) {
        return FALSE;
    }

    u32 idx = dir->location++;
    dirent->entryNum = idx;
    dirent->isDir = listing.isDir[idx] ? TRUE : FALSE;
    dirent->name = const_cast<char*>(listing.entries[idx].c_str());
    return TRUE;
}

int DVDCloseDir(DVDDir* dir) {
    if (!dir) {
        return FALSE;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_dirs.erase(dir->entryNum);
    return TRUE;
}

void DVDRewindDir(DVDDir* dir) {
    if (!dir) {
        return;
    }
    dir->location = 0;
}

s32 DVDGetFileInfoStatus(const DVDFileInfo* fileInfo) {
    if (!fileInfo) {
        return DVD_STATE_FATAL_ERROR;
    }
    return DVD_STATE_END;
}

}
