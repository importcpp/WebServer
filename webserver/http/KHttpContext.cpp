#ifdef USE_RINGBUFFER
#include "../tcp/KRingBuffer.h"
#else
#include "../tcp/KBuffer.h"
#endif

#include "KHttpContext.h"
#include <algorithm>

using namespace kback;

const char HttpContext::kCRLF[] = "\r\n"; // 回车换行

bool HttpContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end)
        {
            const char *question = std::find(start, space, '?');
            if (question != space)
            {
                request_.setPath(start, question);
                request_.setQuery(question, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space + 1;
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_.setVersion(HttpRequest::kHttp11);
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

#ifdef USE_RINGBUFFER
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    string str_buf = buf->retrieveAsString();
    bool ok = true;
    bool hasMore = true;

    while (hasMore)
    {
        int start = 0;
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = std::search(str_buf.data() + start, str_buf.data() + str_buf.size(), kCRLF, kCRLF + 2);
            if (crlf)
            {
                ok = processRequestLine(str_buf.data() + start, crlf);
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    // buf->retrieveUntil(crlf + 2);
                    start = crlf + 2 - str_buf.data();
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = std::search(str_buf.data() + start, str_buf.data() + str_buf.size(), kCRLF, kCRLF + 2);
            if (crlf)
            {
                const char *colon = std::find(str_buf.data() + start, crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(str_buf.data() + start, colon, crlf);
                }
                else
                {
                    // empty line, end of header
                    // FIXME:
                    state_ = kGotAll;
                    hasMore = false;
                }
                // buf->retrieveUntil(crlf + 2);
                start = crlf + 2 - str_buf.data();
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            // FIXME:
            // 可以用于提取报文的主体部分
        }
    }
    return ok;
}

#else
// return false if any error
// 解析http请求
bool HttpContext::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    // string str_buf = buf->retrieveAsString();
    bool ok = true;
    bool hasMore = true;
    while (hasMore)
    {
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // empty line, end of header
                    // FIXME:
                    state_ = kGotAll;
                    hasMore = false;
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            // FIXME:
            // 可以用于提取报文的主体部分
        }
    }
    return ok;
}
#endif
