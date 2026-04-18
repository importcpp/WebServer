#pragma once

#include "webserver/utils/Knoncopyable.h"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace kback {

class StaticFileCache : noncopyable {
public:
  struct Entry {
    Entry(std::string fullPath, std::string contentType, size_t fileSize,
          int fileFd);
    ~Entry();

    std::string fullPath;
    std::string contentType;
    size_t fileSize;
    int fileFd;
  };

  StaticFileCache();

  void setRoot(std::string root);
  std::shared_ptr<const Entry> find(const std::string &requestPath);

private:
  std::string normalizeRequestPath(const std::string &requestPath) const;
  std::shared_ptr<Entry> loadEntry(const std::string &normalizedPath);

  std::string root_;
  mutable std::shared_mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<Entry>> entries_;
};

} // namespace kback
