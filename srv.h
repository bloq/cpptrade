#ifndef __SRV_H__
#define __SRV_H__

#include <sys/time.h>
#include <string>
#include <vector>
#include <cstdint>
#include <evhtp.h>
#include <openssl/sha.h>
#include "Market.h"

struct HttpApiEntry {
	bool			authReq;	// authentication req'd?

	const char		*path;
	bool			pathIsRegex;

	evhtp_callback_cb	cb;
	bool			wantInput;
	bool			jsonInput;
};

class ReqState {
public:
	std::string		body;
	SHA256_CTX		bodyHash;
	std::vector<unsigned char> md;

	std::string		path;
	struct timeval		tstamp;

	const struct HttpApiEntry *apiEnt;

	ReqState() : md(SHA256_DIGEST_LENGTH) {
		SHA256_Init(&bodyHash);
		gettimeofday(&tstamp, NULL);
	}
};

extern orderentry::Market market;
extern uint32_t nextOrderId;
bool reqPreProcessing(evhtp_request_t *req, ReqState *state);

#endif // __SRV_H__
