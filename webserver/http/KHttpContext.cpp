#ifdef USE_RINGBUFFER
#include "../tcp/KRingBuffer.h"
#else
#include "../tcp/KBuffer.h"
#endif

#include "KHttpContext.h"
#include <algorithm>

using namespace kback;

const char HttpContext::kCRLF[] = "\r\n"; // 回车换行

// 对请求行的处理
bool HttpContext::processRequestLine(const char *begin, const char *end) {
  bool succeed = false;
  const char *start = begin;
  const char *space = std::find(start, end, ' ');
  if (space != end && request_.setMethod(start, space)) {
    start = space + 1;
    space = std::find(start, end, ' ');
    if (space != end) {
      const char *question = std::find(start, space, '?');
      if (question != space) {
        request_.setPath(start, question);
        request_.setQuery(question, space);
      } else {
        request_.setPath(start, space);
      }
      start = space + 1;
      succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
      if (succeed) {
        if (*(end - 1) == '1') {
          request_.setVersion(HttpRequest::kHttp11);
        } else if (*(end - 1) == '0') {
          request_.setVersion(HttpRequest::kHttp10);
        } else {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

// 解析http请求
// 线性 Buffer 与环形 RingBuffer 暴露一致的接口（findCRLF/peek/retrieveUntil），
// 因此无需在 RINGBUFFER 模式下将整个缓冲区拷贝成 std::string 再解析，
// 直接基于 Buffer 接口零拷贝解析，避免大请求时的 O(n) 拷贝开销，
// 同时消除原先 string::data()+size() 非空指针导致 findCRLF 误判的问题。
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime) {
  bool ok = true;
  bool hasMore = true;
  // 利用状态机转移，分三部分对请求报文进行解析
  while (hasMore) {
    // 请求行解析
    if (state_ == kExpectRequestLine) {
      const char *crlf = buf->findCRLF();
      if (crlf) {
        ok = processRequestLine(buf->peek(), crlf);
        if (ok) {
          request_.setReceiveTime(receiveTime);
          buf->retrieveUntil(crlf + 2);
          state_ = kExpectHeaders;
        } else {
          hasMore = false;
        }
      } else {
        hasMore = false;
      }
    }
    // 请求头解析
    else if (state_ == kExpectHeaders) {
      const char *crlf = buf->findCRLF();
      if (crlf) {
        const char *colon = std::find(buf->peek(), crlf, ':');
        if (colon != crlf) {
          request_.addHeader(buf->peek(), colon, crlf);
        } else {
          // 空行，头部解析完毕
          state_ = kGotAll;
          hasMore = false;
        }
        buf->retrieveUntil(crlf + 2);
      } else {
        hasMore = false;
      }
    } else if (state_ == kExpectBody) {
      // 可以用于提取报文的主体部分
    }
  }
  return ok;
}
