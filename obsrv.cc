
#include "cscpp-config.h"

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <locale>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <argp.h>
#include <unistd.h>
#include <evhtp.h>
#include <ctype.h>
#include <assert.h>
#include <univalue.h>
#include "Market.h"
#include "Util.h"
#include "HttpUtil.h"
#include "srvapi.h"
#include "srv.h"

using namespace std;
using namespace orderentry;

#define PROGRAM_NAME "obsrv"
#define DEFAULT_PID_FILE "/var/run/obsrv.pid"

enum {
	MAX_CLIENT_DRIFT	= 20,	// max seconds client time may drift
};

static const char doc[] =
PROGRAM_NAME " - order book server";

static struct argp_option options[] = {
	{ "config", 'c', "FILE", 0,
	  "JSON server configuration file (default: config-obsrv.json)" },

	{ "daemon", 1002, NULL, 0,
	  "Daemonize; run server in background." },

	{ "pid-file", 'p', "FILE", 0,
	  "Pathname to which process PID is written (default: " DEFAULT_PID_FILE "; empty string to disable)" },
	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, NULL, doc };

static std::string opt_configfn = "config-obsrv.json";
static std::string opt_pid_file = DEFAULT_PID_FILE;
static bool opt_daemon = false;
static UniValue serverCfg;
static evbase_t *evbase = NULL;

Market market;

static void
logRequest(evhtp_request_t *req, ReqState *state)
{
	assert(req && state);

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
	assert(req && buf && arg);

	ReqState *state = (ReqState *) arg;

	// remove data from evbuffer to malloc'd buffer
	size_t bufsz = evbuffer_get_length(buf);
	char *chunk = (char *) malloc(bufsz);
	int rc = evbuffer_remove(buf, chunk, bufsz);
	assert(rc == (int) bufsz);

	// append chunk to total body
	state->body.append(chunk, bufsz);

	// update in-progress content hash
	SHA256_Update(&state->bodyHash, chunk, bufsz);

	// release malloc'd buffer
	free(chunk);

	return EVHTP_RES_OK;
}

static evhtp_res
req_finish_cb(evhtp_request_t * req, void * arg)
{
	assert(req && arg);

	ReqState *state = (ReqState *) arg;

	// log request, following processing
	logRequest(req, state);

	// release our per-request state
	delete state;

	return EVHTP_RES_OK;
}

static void reqInit(evhtp_request_t *req, ReqState *state,
		    const struct HttpApiEntry *apiEnt)
{
	assert(req && state && apiEnt);

	state->apiEnt = apiEnt;

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

static bool reqVerify(evhtp_request_t *req, ReqState *state,
		      const struct HttpApiEntry *apiEnt,
		      const std::string& authUser,
		      const std::string& authSecret)
{
	assert(req && state && apiEnt);

	if (!apiEnt->authReq)
		return true;

	// required headers ETag, X-Unixtime
	const char *etagCstr = evhtp_kv_find (req->headers_in, "ETag");
	const char *unixtimeCstr = evhtp_kv_find (req->headers_in, "X-Unixtime");
	if (!etagCstr || !unixtimeCstr)
		return false;

	// validate client clock within range
	time_t timeDiff, clientTime = (time_t) strtoll(unixtimeCstr, NULL, 10);
	timeDiff = std::max(state->tstamp.tv_sec, clientTime) -
		   std::min(state->tstamp.tv_sec, clientTime);
	if (timeDiff > MAX_CLIENT_DRIFT)
		return false;

	// input remote Auth hdr
	const char *authcstr = evhtp_kv_find (req->headers_in, "Authorization");
	if (!authcstr)
		return false;
	string authHdr(authcstr);

	// generate local Auth hdr
	string authCanonical;
	build_auth_hdr(req, authUser, authSecret, authCanonical);

	// verify match
	return (authHdr == authCanonical);
}

bool reqPreProcessing(evhtp_request_t *req, ReqState *state)
{
	assert(req && state && state->apiEnt);

	// check authorization, if method requires it
	if (!reqVerify(req, state, state->apiEnt,
		       "testuser", "testpass")) {
		evhtp_send_reply(req, EVHTP_RES_FORBIDDEN);
		return false;
	}

	// final authorization step: verify supplied ETag matches body content
	const char *etagCstr = evhtp_kv_find (req->headers_in, "ETag");
	if (etagCstr) {
		string etagRemote(etagCstr);

		// finalize content hash
		SHA256_Final(&state->md[0], &state->bodyHash);

		// canonical ETag, calculated from input content
		string etagCanon = HexStr(state->md);

		// verify ETag matches expected
		if (etagRemote != etagCanon) {
			evhtp_send_reply(req, EVHTP_RES_BADREQ);
			return false;
		}
	}

	return true;
}

static evhtp_res
upload_headers_cb(evhtp_request_t * req, evhtp_headers_t * hdrs, void *arg)
{
	assert(req && hdrs && arg);

	const struct HttpApiEntry *apiEnt = (const struct HttpApiEntry *) arg;

	// handle OPTIONS
	if (evhtp_request_get_method(req) == htp_method_OPTIONS) {
		return EVHTP_RES_OK;
	}

	// alloc new per-request state
	ReqState *state = new ReqState();
	assert(state != NULL);

	// common per-request state
	reqInit(req, state, apiEnt);

	// special incoming-data hook
	evhtp_request_set_hook (req, evhtp_hook_on_read, (evhtp_hook) upload_read_cb, state);

	return EVHTP_RES_OK;
}

static evhtp_res
no_upload_headers_cb(evhtp_request_t * req, evhtp_headers_t * hdrs, void *arg)
{
	assert(req && hdrs && arg);

	const struct HttpApiEntry *apiEnt = (const struct HttpApiEntry *) arg;

	// handle OPTIONS
	if (evhtp_request_get_method(req) == htp_method_OPTIONS) {
		return EVHTP_RES_OK;
	}

	// alloc new per-request state
	ReqState *state = new ReqState();
	assert(state != NULL);

	// common per-request state
	reqInit(req, state, apiEnt);

	return EVHTP_RES_OK;
}

void reqInfo(evhtp_request_t * req, void * arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

	// current service time
	struct timeval tv;
	gettimeofday(&tv, NULL);
	UniValue timeObj(UniValue::VOBJ);
	timeObj.pushKV("unixtime", tv.tv_sec);
	timeObj.pushKV("iso", isoTimeStr(tv.tv_sec));

	// some information about this server
	UniValue obj(UniValue::VOBJ);
	obj.pushKV("name", "obsrv");
	obj.pushKV("apiversion", 100);
	obj.pushKV("time", timeObj);

	// successful operation.  Return JSON output.
	httpJsonReply(req, obj);
}

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case 'c':
		opt_configfn = arg;
		break;

	case 'p':
		opt_pid_file = arg;
		break;

	case 1002:
		opt_daemon = true;
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
	if (serverCfg.exists("daemon"))
		opt_daemon = serverCfg["daemon"].getBool();
	if (serverCfg.exists("pidFile"))
		opt_pid_file = serverCfg["pidFile"].getValStr();

	return true;
}

static void pid_file_cleanup(void)
{
	if (!opt_pid_file.empty())
		unlink(opt_pid_file.c_str());
}

static void shutdown_signal(int signo)
{
	event_base_loopbreak(evbase);
}

static std::vector<struct HttpApiEntry> apiRegistry = {
	{ false, "/info", false, reqInfo, false, false },
	{ false, "/marketAdd", false, reqMarketAdd, true, true },
	{ false, "/marketList", false, reqMarketList, false, false },
	{ false, "/book", false, reqOrderBookList, true, true },
	{ false, "/orderAdd", false, reqOrderAdd, true, true },
	{ false, "/orderCancel", false, reqOrderCancel, true, true },
	{ false, "/orderModify", false, reqOrderModify, true, true },
	{ false, "^/order/([a-z0-9-]+)", true, reqOrderInfo, true, true },
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

	// Process auto-cleanup
	signal(SIGTERM, shutdown_signal);
	signal(SIGINT, shutdown_signal);
	atexit(pid_file_cleanup);

	// initialize libevent, libevhtp
	evbase = event_base_new();
	evhtp_t  * htp    = evhtp_new(evbase, NULL);
	evhtp_callback_t *cb = NULL;

	// register our list of API calls and their handlers
	for (size_t i = 0; i < apiRegistry.size(); i++) {
		const struct HttpApiEntry *apiEnt = &apiRegistry[i];

		// register evhtp hook
		if (apiEnt->pathIsRegex)
			cb = evhtp_set_regex_cb(htp, apiEnt->path, apiEnt->cb, (void *) apiEnt);
		else
			cb = evhtp_set_cb(htp, apiEnt->path, apiEnt->cb, (void *) apiEnt);

		// set standard per-callback initialization hook
		evhtp_callback_set_hook(cb, evhtp_hook_on_headers,
			apiEnt->wantInput ?
				((evhtp_hook) upload_headers_cb) :
				((evhtp_hook) no_upload_headers_cb), (void *) apiEnt);
	}

	// Daemonize
	if (opt_daemon && daemon(0, 0) < 0) {
		perror("Failed to daemonize");
		return EXIT_FAILURE;
	}

	// Hold open PID file until process exits
	int pid_fd = write_pid_file(opt_pid_file);
	if (pid_fd < 0)
		return EXIT_FAILURE;

	// bind to socket and start server main loop
	evhtp_bind_socket(htp,
			  serverCfg["bindAddress"].getValStr().c_str(),
			  atoi(serverCfg["bindPort"].getValStr().c_str()),
			  1024);
	event_base_loop(evbase, 0);
	return 0;
}
