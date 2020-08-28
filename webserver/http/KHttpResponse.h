#include "../utils/KTypes.h"
#include "../utils/Kcopyable.h"

#include <map>

namespace kback {

class Buffer;
class HttpResponse : public copyable {
public:
  enum HttpStatusCode {
    kUnknown,
    k200Ok = 200,
    k301MovedPermanently = 301,
    k400BadRequest = 400,
    k404NotFound = 404,
  };

  explicit HttpResponse(bool close)
      : statusCode_(kUnknown), closeConnection_(close), sendFile_(false),
        fileSize_(0) {}

  void setStatusCode(HttpStatusCode code) { statusCode_ = code; }

  void setStatusMessage(const string &message) { statusMessage_ = message; }

  void setCloseConnection(bool on) { closeConnection_ = on; }

  bool closeConnection() const { return closeConnection_; }

  void setContentType(const string &contentType) {
    addHeader("Content-Type", contentType);
  }

  void addHeader(const string &key, const string &value) {
    headers_[key] = value;
  }

  void setBody(const string &body) { body_ = body; }

  void setFileSize(size_t count) {
    fileSize_ = count;
    sendFile_ = true;
  }

  void setSrcFd(size_t fd) { srcFd_ = fd; }

  size_t getFileSize() { return fileSize_; }

  void appendToBuffer(Buffer *output) const;

private:
  std::map<string, string> headers_;
  HttpStatusCode statusCode_;

  string statusMessage_;
  bool closeConnection_;
  bool sendFile_;
  string body_;
  size_t fileSize_;
  size_t srcFd_;
};

} // namespace kback
