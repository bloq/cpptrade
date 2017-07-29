
#include <string>
#include <vector>
#include <stdio.h>
#include <univalue.h>
#include <evhtp.h>
#include <assert.h>

using namespace std;

class ReqState {
public:
	string	body;
};

void reqDefault(evhtp_request_t * req, void * a)
{
	evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
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
upload_finish_cb(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

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
	evhtp_request_set_hook (req, evhtp_hook_on_request_fini, (evhtp_hook) upload_finish_cb, state);

	return EVHTP_RES_OK;
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

int main(int argc, char ** argv)
{
	evbase_t * evbase = event_base_new();
	evhtp_t  * htp    = evhtp_new(evbase, NULL);
	evhtp_callback_t *cb = NULL;

	evhtp_set_gencb(htp, reqDefault, NULL);

	evhtp_set_cb(htp, "/", reqRoot, NULL);

	cb = evhtp_set_cb(htp, "/post", reqPost, NULL);
	evhtp_callback_set_hook(cb, evhtp_hook_on_headers, (evhtp_hook) upload_headers_cb, NULL);

	evhtp_bind_socket(htp, "0.0.0.0", 7979, 1024);
	event_base_loop(evbase, 0);
	return 0;
}
