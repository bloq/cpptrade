#ifndef __HTTPUTIL_H__
#define __HTTPUTIL_H__

#include <cstdint>
#include <string>
#include <time.h>
#include <evhtp.h>

bool query_int64_range(const evhtp_request_t *req,
		     const char *query_key,
		     int64_t& vOut,
		     int64_t vMin, int64_t vMax, int64_t vDefault);
int64_t get_content_length (const evhtp_request_t *req);
std::string httpDateHdr(time_t t);
void httpJsonReply(evhtp_request_t *req, const UniValue& jval);
void build_auth_hdr(evhtp_request_t *req,
		    const std::string& auth_user,
		    const std::string& auth_secret,
		    std::string& auth_hdr);

#endif // __HTTPUTIL_H__
