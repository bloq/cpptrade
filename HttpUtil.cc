
#include <string>
#include <cstdint>
#include <evhtp.h>
#include <stdlib.h>
#include <assert.h>
#include "HttpUtil.h"
#include "Util.h"

int64_t get_content_length (const evhtp_request_t *req)
{
    assert(req != NULL);
    const char *content_len_str = evhtp_kv_find (req->headers_in, "Content-Length");
    if (!content_len_str) {
        return -1;
    }

    return strtoll (content_len_str, NULL, 10);
}

std::string httpDateHdr(time_t t)
{
	return formatTime("%a, %d %b %Y %H:%M:%S GMT", t);
}

