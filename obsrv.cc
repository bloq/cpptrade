
#include "cscpp-config.h"

#include <string>
#include <vector>
#include <map>
#include <locale>
#include <stdio.h>
#include <stdlib.h>
#include <univalue.h>
#include <time.h>
#include <argp.h>
#include <evhtp.h>
#include <ctype.h>
#include <assert.h>
#include "Market.h"
#include "srvapi.h"
#include "srv.h"

using namespace std;
using namespace orderentry;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct HttpApiEntry {
	const char		*path;
	evhtp_callback_cb	cb;
	bool			wantInput;
	bool			jsonInput;
};

#define PROGRAM_NAME "obsrv"

static const char doc[] =
PROGRAM_NAME " - order book server";

static struct argp_option options[] = {
	{ "config", 'c', "json-file", 0,
	  "JSON server configuration file (default: config-obsrv.json)" },
	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, NULL, doc };

static std::string opt_configfn = "config-obsrv.json";
static UniValue serverCfg;

uint32_t nextOrderId = 1;
Market market;

static int64_t
get_content_length (const evhtp_request_t *req)
{
    assert(req != NULL);
    const char *content_len_str = evhtp_kv_find (req->headers_in, "Content-Length");
    if (!content_len_str) {
        return -1;
    }

    return strtoll (content_len_str, NULL, 10);
}

static std::string addressToStr(const struct sockaddr *sockaddr,
				socklen_t socklen)
{
        char name[1025] = "";
        if (!getnameinfo(sockaddr, socklen, name, sizeof(name), NULL, 0, NI_NUMERICHOST))
            return std::string(name);

	return string("");
}

static std::string formatTime(const std::string& fmt, time_t t)
{
	// convert unix seconds-since-epoch to split-out struct
	struct tm tm;
	gmtime_r(&t, &tm);

	// build formatted time string
	char timeStr[fmt.size() + 256];
	strftime(timeStr, sizeof(timeStr), fmt.c_str(), &tm);

	return string(timeStr);
}

static std::string httpDateHdr(time_t t)
{
	return formatTime("%a, %d %b %Y %H:%M:%S GMT", t);
}

static std::string isoTimeStr(time_t t)
{
	return formatTime("%FT%TZ", t);
}

static bool readJsonFile(const std::string& filename, UniValue& jval)
{
	jval.clear();

        FILE *f = fopen(filename.c_str(), "r");
	if (!f)
		return false;

        string jdata;

        char buf[4096];
        while (!feof(f)) {
                int bread = fread(buf, 1, sizeof(buf), f);
		if (ferror(f)) {
			fclose(f);
			return false;
		}

                string s(buf, bread);
                jdata += s;
        }

	if (ferror(f)) {
		fclose(f);
		return false;
	}
        fclose(f);

	if (!jval.read(jdata))
		return false;

        return true;
}

static void
logRequest(evhtp_request_t *req, ReqState *state)
{
	assert(req != NULL);
	assert(state != NULL);

	// IP address
	string addrStr = addressToStr(req->conn->saddr,
				      sizeof(struct sockaddr)); // TODO verify

	// request timestamp.  use request-completion (not request-start)
	// time instead?
	struct tm tm;
	gmtime_r(&state->tstamp.tv_sec, &tm);

	// get http method, build timestamp str
	string timeStr = isoTimeStr(state->tstamp.tv_sec);
	htp_method method = evhtp_request_get_method(req);
	const char *method_name = htparser_get_methodstr_m(method);

	// output log line
	printf("%s - - [%s] \"%s %s\" ? %lld\n",
		addrStr.c_str(),
		timeStr.c_str(),
		method_name,
		req->uri->path->full,
		(long long) get_content_length(req));
}

static evhtp_res
upload_read_cb(evhtp_request_t * req, evbuf_t * buf, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// remove data from evbuffer to malloc'd buffer
	size_t bufsz = evbuffer_get_length(buf);
	char *chunk = (char *) malloc(bufsz);
	int rc = evbuffer_remove(buf, chunk, bufsz);
	assert(rc == (int) bufsz);

	// append chunk to total body
	state->body.append(chunk, bufsz);

	// release malloc'd buffer
	free(chunk);

	return EVHTP_RES_OK;
}

static evhtp_res
req_finish_cb(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// log request, following processing
	logRequest(req, state);

	// release our per-request state
	delete state;

	return EVHTP_RES_OK;
}

static void reqInit(evhtp_request_t *req, ReqState *state)
{
	// standard Date header
	evhtp_headers_add_header(req->headers_out,
		evhtp_header_new("Date",
			 httpDateHdr(state->tstamp.tv_sec).c_str(),
			 0, 1));

	// standard Server header
	const char *serverVer = PROGRAM_NAME "/" PACKAGE_VERSION;
	evhtp_headers_add_header(req->headers_out,
		evhtp_header_new("Server", serverVer, 0, 0));

	// assign our global (to a request) state
	req->cbarg = state;

	// assign request completion hook
	evhtp_request_set_hook (req, evhtp_hook_on_request_fini, (evhtp_hook) req_finish_cb, state);
}

static evhtp_res
upload_headers_cb(evhtp_request_t * req, evhtp_headers_t * hdrs, void * arg)
{
	// handle OPTIONS
	if (evhtp_request_get_method(req) == htp_method_OPTIONS) {
		return EVHTP_RES_OK;
	}

	// alloc new per-request state
	ReqState *state = new ReqState();
	assert(state != NULL);

	// common per-request state
	reqInit(req, state);

	// special incoming-data hook
	evhtp_request_set_hook (req, evhtp_hook_on_read, (evhtp_hook) upload_read_cb, state);

	return EVHTP_RES_OK;
}

static evhtp_res
no_upload_headers_cb(evhtp_request_t * req, evhtp_headers_t * hdrs, void * arg)
{
	// handle OPTIONS
	if (evhtp_request_get_method(req) == htp_method_OPTIONS) {
		return EVHTP_RES_OK;
	}

	// alloc new per-request state
	ReqState *state = new ReqState();
	assert(state != NULL);

	// common per-request state
	reqInit(req, state);

	return EVHTP_RES_OK;
}

void reqDefault(evhtp_request_t * req, void * a)
{
	evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
}

void reqInfo(evhtp_request_t * req, void * a)
{
	UniValue obj(UniValue::VOBJ);

	// set some static information about this server
	obj.pushKV("name", "obsrv");
	obj.pushKV("version", 100);

	// successful operation.  Return JSON output.
	string body = obj.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'c':
		opt_configfn.assign(arg);
		break;

	case ARGP_KEY_END:
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static bool read_config_init()
{
	if (!readJsonFile(opt_configfn, serverCfg)) {
		perror(opt_configfn.c_str());
		return false;
	}

	if (!serverCfg.exists("bindAddress"))
		serverCfg.pushKV("bindAddress", "0.0.0.0");
	if (!serverCfg.exists("bindPort"))
		serverCfg.pushKV("bindPort", (int64_t) 7979);

	return true;
}

static const struct HttpApiEntry apiRegistry[] = {
	{ "/info", reqInfo, false, false },
	{ "/marketAdd", reqMarketAdd, true, true },
	{ "/marketList", reqMarketList, false, false },
	{ "/book", reqOrderBookList, true, true },
	{ "/orderAdd", reqOrderAdd, true, true },
	{ "/orderCancel", reqOrderCancel, true, true },
	{ "/orderModify", reqOrderModify, true, true },
};

int main(int argc, char ** argv)
{
	// parse command line
	error_t argp_rc = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (argp_rc) {
		fprintf(stderr, "%s: argp_parse failed: %s\n",
			argv[0], strerror(argp_rc));
		return EXIT_FAILURE;
	}

	// read json configuration, and initialize early defaults
	if (!read_config_init())
		return EXIT_FAILURE;

	// initialize libevent, libevhtp
	evbase_t * evbase = event_base_new();
	evhtp_t  * htp    = evhtp_new(evbase, NULL);
	evhtp_callback_t *cb = NULL;

	// default callback, if not matched
	evhtp_set_gencb(htp, reqDefault, NULL);

	// register our list of API calls and their handlers
	for (unsigned int i = 0; i < ARRAY_SIZE(apiRegistry); i++) {
		struct HttpApiEntry *apiEnt = (struct HttpApiEntry *) &apiRegistry[i];
		// register evhtp hook
		cb = evhtp_set_cb(htp,
				  apiRegistry[i].path,
				  apiRegistry[i].cb, apiEnt);

		// set standard per-callback initialization hook
		evhtp_callback_set_hook(cb, evhtp_hook_on_headers,
			apiRegistry[i].wantInput ?
				((evhtp_hook) upload_headers_cb) :
				((evhtp_hook) no_upload_headers_cb), apiEnt);
	}

	// bind to socket and start server main loop
	evhtp_bind_socket(htp,
			  serverCfg["bindAddress"].getValStr().c_str(),
			  atoi(serverCfg["bindPort"].getValStr().c_str()),
			  1024);
	event_base_loop(evbase, 0);
	return 0;
}
