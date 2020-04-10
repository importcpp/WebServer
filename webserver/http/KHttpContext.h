#pragma once
#include "../utils/Kcopyable.h"
#include "KHttpRequest.h"

// 一个用于解析Http内容的类

namespace kback
{
class Buffer;

class HttpContext : public copyable
{
public:
    enum HttpRequestParseState
    {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext()
        : state_(kExpectRequestLine)
    {
    }

    // default copy-ctor, dtor and assignment are fine

    // return false if any error
    bool parseRequest(Buffer *buf, Timestamp receiveTime);

    bool gotAll() const
    {
        return state_ == kGotAll;
    }

    void reset()
    {
        state_ = kExpectRequestLine;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    const HttpRequest &request() const
    {
        return request_;
    }

    HttpRequest &request()
    {
        return request_;
    }

private:
    bool processRequestLine(const char *begin, const char *end);

    HttpRequestParseState state_;
    HttpRequest request_;

    static const char kCRLF[];
};

} // namespace kback