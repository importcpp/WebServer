#include "KHttpResponse.h"
#ifdef USE_RINGBUFFER
#include "../tcp/KRingBuffer.h"
#else
#include "../tcp/KBuffer.h"
#endif
#include <stdio.h>

using namespace kback;

void HttpResponse::appendToBuffer(Buffer *output) const
{
    char buf[32];
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode_);
    output->append(buf);
    output->append(statusMessage_);
    output->append("\r\n");

    if (closeConnection_)
    {
        output->append("Connection: close\r\n");
    }
    else
    {
        if (hasSetBody_ == true)
        {
            snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
        }
        else{
            // 
            snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", fileSize_);
        }
        output->append(buf);
        output->append("Connection: Keep-Alive\r\n");
    }

    for (const auto &header : headers_)
    {
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }

    output->append("\r\n");
    if (hasSetBody_ == true)
    {
        output->append(body_);
    }
}