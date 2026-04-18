#include "KStaticFileCache.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace kback {

namespace {

std::string detectContentType(const std::string &path) {
  const std::string::size_type dot = path.find_last_of('.');
  if (dot == std::string::npos) {
    return "application/octet-stream";
  }

  const std::string extension = path.substr(dot);
  if (extension == ".html" || extension == ".htm") {
    return "text/html; charset=utf-8";
  }
  if (extension == ".css") {
    return "text/css; charset=utf-8";
  }
  if (extension == ".js") {
    return "application/javascript; charset=utf-8";
  }
  if (extension == ".json") {
    return "application/json; charset=utf-8";
  }
  if (extension == ".txt") {
    return "text/plain; charset=utf-8";
  }
  if (extension == ".svg") {
    return "image/svg+xml";
  }
  if (extension == ".png") {
    return "image/png";
  }
  if (extension == ".jpg" || extension == ".jpeg") {
    return "image/jpeg";
  }
  if (extension == ".gif") {
    return "image/gif";
  }
  if (extension == ".ico") {
    return "image/x-icon";
  }
  return "application/octet-stream";
}

} // namespace

StaticFileCache::Entry::Entry(std::string fullPath, std::string contentType,
                              size_t fileSize, int fileFd)
    : fullPath(std::move(fullPath)), contentType(std::move(contentType)),
      fileSize(fileSize), fileFd(fileFd) {}

StaticFileCache::Entry::~Entry() {
  if (fileFd >= 0) {
    ::close(fileFd);
  }
}

StaticFileCache::StaticFileCache() : root_(".") {}

void StaticFileCache::setRoot(std::string root) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  root_ = root.empty() ? "." : std::move(root);
  entries_.clear();
}

std::shared_ptr<const StaticFileCache::Entry>
StaticFileCache::find(const std::string &requestPath) {
  const std::string normalizedPath = normalizeRequestPath(requestPath);
  if (normalizedPath.empty()) {
    return nullptr;
  }

  {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    const auto it = entries_.find(normalizedPath);
    if (it != entries_.end()) {
      return it->second;
    }
  }

  std::shared_ptr<Entry> loaded = loadEntry(normalizedPath);
  if (loaded == nullptr) {
    return nullptr;
  }

  std::unique_lock<std::shared_mutex> lock(mutex_);
  const auto [it, inserted] =
      entries_.emplace(normalizedPath, std::move(loaded));
  return it->second;
}

std::string
StaticFileCache::normalizeRequestPath(const std::string &requestPath) const {
  std::string normalizedPath;
  size_t segmentStart = requestPath.empty() ? 0 : 1;

  for (;;) {
    const size_t segmentEnd = requestPath.find('/', segmentStart);
    const size_t length =
        (segmentEnd == std::string::npos ? requestPath.size() : segmentEnd) -
        segmentStart;
    const std::string_view segment(requestPath.data() + segmentStart, length);

    if (!segment.empty() && segment != ".") {
      if (segment == "..") {
        return {};
      }
      if (!normalizedPath.empty()) {
        normalizedPath.push_back('/');
      }
      normalizedPath.append(segment.data(), segment.size());
    }

    if (segmentEnd == std::string::npos) {
      break;
    }
    segmentStart = segmentEnd + 1;
  }

  if (normalizedPath.empty()) {
    normalizedPath = "index.html";
  }
  return normalizedPath;
}

std::shared_ptr<StaticFileCache::Entry>
StaticFileCache::loadEntry(const std::string &normalizedPath) {
  std::string root;
  {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    root = root_;
  }

  const std::string fullPath = root + "/" + normalizedPath;
  const int fileFd = ::open(fullPath.c_str(), O_RDONLY | O_CLOEXEC);
  if (fileFd < 0) {
    return nullptr;
  }

  struct stat statBuffer;
  if (::fstat(fileFd, &statBuffer) < 0 || !S_ISREG(statBuffer.st_mode)) {
    ::close(fileFd);
    return nullptr;
  }

  return std::make_shared<Entry>(fullPath, detectContentType(fullPath),
                                 static_cast<size_t>(statBuffer.st_size),
                                 fileFd);
}

} // namespace kback
