#include "KHttpContext.h"
#include "webserver/tcp/KSelectedBuffer.h"
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
        request_.setQuery(question + 1, space);
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
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime) {
  bool ok = true;
  bool hasMore = true;
  // 利用状态机转移，分三部分对请求报文进行解析
  while (hasMore) {
    // 请求行解析
    if (state_ == kExpectRequestLine) {
      const char *crlf = buf->findCRLF();
      if (crlf) {
        const char *lineBegin = buf->peek();
        const char *lineEnd = crlf;
        std::string lineStorage;
        if (!buf->isSpanContiguous(crlf)) {
          lineStorage = buf->readableStringUntil(crlf);
          lineBegin = lineStorage.data();
          lineEnd = lineBegin + lineStorage.size();
        }

        ok = processRequestLine(lineBegin, lineEnd);
        if (ok) {
          request_.setReceiveTime(receiveTime);
          buf->retrieveLineAndCRLF(crlf);
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
        const char *lineBegin = buf->peek();
        const char *lineEnd = crlf;
        std::string lineStorage;
        if (!buf->isSpanContiguous(crlf)) {
          lineStorage = buf->readableStringUntil(crlf);
          lineBegin = lineStorage.data();
          lineEnd = lineBegin + lineStorage.size();
        }

        const char *colon = std::find(lineBegin, lineEnd, ':');
        if (colon != lineEnd) {
          request_.addHeader(lineBegin, colon, lineEnd);
        } else {
          // 空行，头部解析完毕
          state_ = kGotAll;
          hasMore = false;
        }
        buf->retrieveLineAndCRLF(crlf);
      } else {
        hasMore = false;
      }
    } else if (state_ == kExpectBody) {
      // 可以用于提取报文的主体部分
    }
  }
  return ok;
}
