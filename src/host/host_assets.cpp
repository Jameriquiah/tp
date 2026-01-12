#include "host/HostAssets.h"
#include <cstdlib>

static std::string buildAssetRoot() {
    const char* env = std::getenv("TP_ASSET_ROOT");
    if (env && *env) {
        return std::string(env);
    }
    return std::string("assets/GZ2E01");
}

const char* HostGetAssetRoot() {
    static std::string root = buildAssetRoot();
    return root.c_str();
}

std::string HostResolveAssetPath(const char* path) {
    if (!path || !*path) {
        return std::string(HostGetAssetRoot());
    }

    if ((path[0] && path[1] == ':') || (path[0] == '\\' && path[1] == '\\')) {
        return std::string(path);
    }

    while (*path == '/' || *path == '\\') {
        ++path;
    }

    std::string resolved = HostGetAssetRoot();
    if (!resolved.empty() && resolved.back() != '/' && resolved.back() != '\\') {
        resolved.push_back('/');
    }
    resolved.append(path);
    return resolved;
}
