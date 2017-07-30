
#include <string>
#include <cstdint>
#include <evhtp.h>
#include <stdlib.h>
#include <assert.h>
#include <univalue.h>
#include "HttpUtil.h"
#include "Util.h"

using namespace std;

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

void httpJsonReply(evhtp_request_t *req, const UniValue& jval)
{
	string body = jval.write(2) + "\n";

	evhtp_headers_add_header(req->headers_out,
		evhtp_header_new("Content-Type", "application/json; charset=utf-8", 0, 0));
	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

