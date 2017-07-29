
#include <string>
#include <vector>
#include <stdio.h>
#include <univalue.h>
#include <time.h>
#include <argp.h>
#include <evhtp.h>
#include <assert.h>

using namespace std;

class ReqState {
public:
	string	body;
	string	path;
};

static const char doc[] =
"obsrv - order book server";

static struct argp_option options[] = {
	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, NULL, doc };


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

static void
logRequest(evhtp_request_t *req, ReqState *state)
{
	assert(req != NULL);
	assert(state != NULL);

	time_t t = time(NULL);
	struct tm tm;
	gmtime_r(&t, &tm);

	char timeStr[80];
	strftime(timeStr, sizeof(timeStr), "%FT%TZ", &tm);

	htp_method method = evhtp_request_get_method(req);
	const char *method_name = htparser_get_methodstr_m(method);

	printf("%s %lld %s %s\n",
		timeStr,
		(long long) get_content_length(req),
		method_name,
		req->uri->path->full);
}

static evhtp_res
upload_read_cb(evhtp_request_t * req, evbuf_t * buf, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	size_t bufsz = evbuffer_get_length(buf);
	char *chunk = (char *) malloc(bufsz);
	int rc = evbuffer_remove(buf, chunk, bufsz);
	assert(rc == (int) bufsz);

	state->body.append(chunk, bufsz);

	free(chunk);

	return EVHTP_RES_OK;
}

static evhtp_res
req_finish_cb(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	logRequest(req, state);

	delete state;

	return EVHTP_RES_OK;
}

static evhtp_res
upload_headers_cb(evhtp_request_t * req, evhtp_headers_t * hdrs, void * arg)
{
	if (evhtp_request_get_method(req) == htp_method_OPTIONS) {
		return EVHTP_RES_OK;
	}

	ReqState *state = new ReqState();
	assert(state != NULL);

	req->cbarg = state;
	evhtp_request_set_hook (req, evhtp_hook_on_read, (evhtp_hook) upload_read_cb, state);
	evhtp_request_set_hook (req, evhtp_hook_on_request_fini, (evhtp_hook) req_finish_cb, state);

	return EVHTP_RES_OK;
}

static evhtp_res
upload_no_headers_cb(evhtp_request_t * req, evhtp_headers_t * hdrs, void * arg)
{
	if (evhtp_request_get_method(req) == htp_method_OPTIONS) {
		return EVHTP_RES_OK;
	}

	ReqState *state = new ReqState();
	assert(state != NULL);

	req->cbarg = state;
	evhtp_request_set_hook (req, evhtp_hook_on_request_fini, (evhtp_hook) req_finish_cb, state);

	return EVHTP_RES_OK;
}

void reqDefault(evhtp_request_t * req, void * a)
{
	evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
}

void reqPost(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	UniValue jval;
	if (!jval.read(state->body)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	UniValue obj(true);

	string body = obj.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

void reqRoot(evhtp_request_t * req, void * a)
{
	UniValue obj(UniValue::VOBJ);

	obj.pushKV("name", "obsrv");
	obj.pushKV("version", 100);

	string body = obj.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {
	case ARGP_KEY_END:
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

int main(int argc, char ** argv)
{
	error_t argp_rc = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (argp_rc) {
		fprintf(stderr, "%s: argp_parse failed: %s\n",
			argv[0], strerror(argp_rc));
		return EXIT_FAILURE;
	}

	evbase_t * evbase = event_base_new();
	evhtp_t  * htp    = evhtp_new(evbase, NULL);
	evhtp_callback_t *cb = NULL;

	evhtp_set_gencb(htp, reqDefault, NULL);

	cb = evhtp_set_cb(htp, "/", reqRoot, NULL);
	evhtp_callback_set_hook(cb, evhtp_hook_on_headers, (evhtp_hook) upload_no_headers_cb, NULL);

	cb = evhtp_set_cb(htp, "/post", reqPost, NULL);
	evhtp_callback_set_hook(cb, evhtp_hook_on_headers, (evhtp_hook) upload_headers_cb, NULL);

	evhtp_bind_socket(htp, "0.0.0.0", 7979, 1024);
	event_base_loop(evbase, 0);
	return 0;
}
