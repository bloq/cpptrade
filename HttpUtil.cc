
#include <string>
#include <vector>
#include <cstdint>
#include <evhtp.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <univalue.h>
#include "HttpUtil.h"
#include "Util.h"

using namespace std;

bool query_int64_range(const evhtp_request_t *req,
		     const char *query_key,
		     int64_t& vOut,
		     int64_t vMin, int64_t vMax, int64_t vDefault)
{
	assert(req && query_key);

	vOut = vDefault;

	// no query string; return default
	if (!req->uri || !req->uri->query)
		return true;

	// query key not present; return default
	const char *value = evhtp_kv_find (req->uri->query, query_key);
	if (!value)
		return true;

	// invalid query value; return error
	errno = 0;
	int64_t val = (int64_t) strtoll(value, NULL, 10);
	if (errno != 0)
		return false;

	// valid value within range; return value
	if ((val >= vMin) && (val <= vMax)) {
		vOut = val;
		return true;
	}

	// invalid value outside range; return error
	return false;
}

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

void build_auth_hdr(evhtp_request_t *req,
		    const std::string& auth_user,
		    const std::string& auth_secret,
		    std::string& auth_hdr)
{
	evhtp_headers_t *hdrs = req->headers_in;

	// build canonical pseudo-header

	// phdr header: static auth method, auth user id
	string phdr("cscpp1-sha256\n");
	phdr += auth_user + "\n";

	// absorb Host, X-Unixtime, ETag
	// It is assumed ETag will guaranteed message contents
	// ETag is signed here, checked elsewhere.
	const char *hdr = evhtp_kv_find(hdrs, "Host");
	if (hdr && *hdr)
		phdr += string(hdr) + "\n";
	hdr = evhtp_kv_find(hdrs, "X-Unixtime");
	if (hdr && *hdr)
		phdr += string(hdr) + "\n";
	hdr = evhtp_kv_find(hdrs, "ETag");
	if (hdr && *hdr)
		phdr += string(hdr) + "\n";

	// create HMAC signature
	vector<unsigned char> md(SHA256_DIGEST_LENGTH);
	HMAC(EVP_sha256(),
             &auth_secret[0], auth_secret.size(),
             (const unsigned char *) phdr.c_str(), phdr.size(),
             &md[0], NULL);
       string signature(HexStr(md));

       // format: bbnode1-sha256 $username $signature
       auth_hdr = "cscpp1-sha256 " + auth_user + " " + signature;
}

