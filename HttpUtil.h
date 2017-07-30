#ifndef __HTTPUTIL_H__
#define __HTTPUTIL_H__

#include <cstdint>
#include <string>
#include <time.h>
#include <evhtp.h>

int64_t get_content_length (const evhtp_request_t *req);
std::string httpDateHdr(time_t t);
void httpJsonReply(evhtp_request_t *req, const UniValue& jval);
void build_auth_hdr(evhtp_request_t *req,
		    const std::string& auth_user,
		    const std::string& auth_secret,
		    std::string& auth_hdr);

#endif // __HTTPUTIL_H__
