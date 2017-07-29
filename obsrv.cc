
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

using namespace std;
using namespace orderentry;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct HttpApiEntry {
	const char		*path;
	evhtp_callback_cb	cb;
	bool			wantInput;
	bool			jsonInput;
};

class ReqState {
public:
	string			body;
	string			path;
	struct timeval		tstamp;

	ReqState() {
		gettimeofday(&tstamp, NULL);
	}
};

#define PROGRAM_NAME "obsrv"

static const char doc[] =
PROGRAM_NAME " - order book server";

static struct argp_option options[] = {
	{ "bind-address", 1001, "IP/hostname", 0,
	  "Address to which HTTP server is bound" },
	{ "bind-port", 1002, "port", 0,
	  "TCP port to which HTTP server is bound" },
	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, NULL, doc };

static const char *opt_bind_address = "0.0.0.0";
static uint16_t opt_bind_port = 7979;

static uint32_t nextOrderId = 1;
static Market market;

static bool validSymbol(const std::string& sym)
{
	// between 1 and 16 chars
	if ((sym.size() == 0) || (sym.size() > 16))
		return false;

	// must be uppercase alpha
	for (size_t i = 0; i < sym.size(); i++) {
		char ch = sym[i];
		if (!isalpha(ch) || !isupper(ch))
			return false;
	}

	return true;
}

static bool validBookType(const std::string& btype)
{
	if ((btype != "simple") &&
	    (btype != "depth"))
		return false;

	return true;
}

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

static bool parseBySchema(ReqState *state,
			  const std::map<std::string,UniValue::VType>& schema,
			  UniValue& jval)
{
	if (!jval.read(state->body) ||
	    !jval.checkObject(schema))
		return false;

	return true;
}

void reqOrderAdd(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// required JSON parameters and their types
	std::map<std::string,UniValue::VType> apiSchema;
	apiSchema["symbol"] = UniValue::VSTR;
	apiSchema["qty"] = UniValue::VNUM;
	apiSchema["price"] = UniValue::VNUM;
	apiSchema["is_buy"] = UniValue::VBOOL;

	// parse input into JSON + preliminary input validation
	UniValue jval;
	if (!parseBySchema(state, apiSchema, jval)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// copy JSON input params into more manageable temporary variables
	string symbol = jval["symbol"].getValStr();
	liquibook::book::Quantity quantity = atoll(jval["qty"].getValStr().c_str());
	liquibook::book::Price price = atoll(jval["price"].getValStr().c_str());
	bool isBuy = jval["is_buy"].getBool();
	bool aon = jval["aon"].getBool();
	bool ioc = jval["ioc"].getBool();
	liquibook::book::Price stopPrice = 0;
	if (jval.exists("stop"))
		stopPrice = atoll(jval["stop"].getValStr().c_str());

	// lookup order book from symbol
	auto book = market.findBook(symbol);
	if (!book) {
		evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
		return;
	}

	// generate new order id
	uint32_t thisOrderId = nextOrderId++;
	std::string orderId = std::to_string(thisOrderId);

	// build new order instance, given input params above
	OrderPtr order = std::make_shared<Order>(orderId, isBuy, quantity, symbol, price, stopPrice, aon, ioc);

	const liquibook::book::OrderConditions AON(liquibook::book::oc_all_or_none);
	const liquibook::book::OrderConditions IOC(liquibook::book::oc_immediate_or_cancel);
	const liquibook::book::OrderConditions NOC(liquibook::book::oc_no_conditions);

	const liquibook::book::OrderConditions conditions = 
	    (aon ? AON : NOC) | (ioc ? IOC : NOC);

	// submit order to order book
	market.orderSubmit(book, order, orderId, conditions);

	// return order data
	UniValue res(UniValue::VOBJ);
	res.pushKV("orderId", (uint64_t) thisOrderId);

	// successful operation.  Return JSON output.
	string body = res.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

void reqOrderModify(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// required JSON parameters and their types
	std::map<std::string,UniValue::VType> apiSchema;
	apiSchema["order_id"] = UniValue::VSTR;

	// parse input into JSON + preliminary input validation
	UniValue jval;
	if (!parseBySchema(state, apiSchema, jval)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	if (!jval.exists("price") &&
	    !jval.exists("qtyDelta")) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// copy JSON input params into more manageable temporary variables
	string orderIdStr = jval["order_id"].getValStr();
	int32_t qtyDelta = liquibook::book::SIZE_UNCHANGED;
	if (jval.exists("qtyDelta"))
		qtyDelta = atoll(jval["qtyDelta"].getValStr().c_str());
	liquibook::book::Price price = liquibook::book::PRICE_UNCHANGED;
	if (jval.exists("price"))
		price = atoll(jval["price"].getValStr().c_str());

	// submit modification request to order book
	bool rc = market.orderModify(orderIdStr, qtyDelta, price);

	UniValue res(rc);

	// successful operation.  Return JSON output.
	string body = res.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

void reqOrderCancel(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// required JSON parameters and their types
	std::map<std::string,UniValue::VType> apiSchema;
	apiSchema["order_id"] = UniValue::VSTR;

	// parse input into JSON + preliminary input validation
	UniValue jval;
	if (!parseBySchema(state, apiSchema, jval)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// copy JSON input params into more manageable temporary variables
	string orderIdStr = jval["order_id"].getValStr();

	// submit cancellation request to order book
	bool rc = market.orderCancel(orderIdStr);

	UniValue res(rc);

	// successful operation.  Return JSON output.
	string body = res.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

void reqOrderBookList(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// required JSON parameters and their types
	std::map<std::string,UniValue::VType> apiSchema;
	apiSchema["symbol"] = UniValue::VSTR;

	// parse input into JSON + preliminary input validation
	UniValue jval;
	if (!parseBySchema(state, apiSchema, jval)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// copy JSON input params into more manageable temporary variables
	string inSymbol = jval["symbol"].getValStr();

	// lookup order book from symbol
	auto book = market.findBook(inSymbol);
	if (!book) {
		evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
		return;
	}

	// gather list of asks from order book
	UniValue asksArr(UniValue::VARR);
	const OrderBook::TrackerMap& asks_ = book->asks();
	for (auto ask = asks_.rbegin(); ask != asks_.rend(); ++ask) {
		UniValue askObj(UniValue::VOBJ);
		askObj.pushKV("price", (int64_t) ask->first.price());
		askObj.pushKV("qty", (int64_t) ask->second.open_qty());
		asksArr.push_back(askObj);
	}

	// gather list of bids from order book
	UniValue bidsArr(UniValue::VARR);
	const OrderBook::TrackerMap& bids_ = book->bids();
	for (auto bid = bids_.rbegin(); bid != bids_.rend(); ++bid) {
		UniValue bidObj(UniValue::VOBJ);
		bidObj.pushKV("price", (int64_t) bid->first.price());
		bidObj.pushKV("qty", (int64_t) bid->second.open_qty());
		bidsArr.push_back(bidObj);
	}

	// build return object
	UniValue obj(UniValue::VOBJ);
	obj.pushKV("bids", bidsArr);
	obj.pushKV("asks", asksArr);

	// successful operation.  Return JSON output.
	string body = obj.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

void reqMarketAdd(evhtp_request_t * req, void * arg)
{
	ReqState *state = (ReqState *) arg;
	assert(state != NULL);

	// required JSON parameters and their types
	std::map<std::string,UniValue::VType> apiSchema;
	apiSchema["symbol"] = UniValue::VSTR;
	apiSchema["booktype"] = UniValue::VSTR;

	// parse input into JSON + preliminary input validation
	UniValue jval;
	if (!parseBySchema(state, apiSchema, jval)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// copy JSON input params into more manageable temporary variables
	string inSymbol = jval["symbol"].getValStr();
	string inBookType = jval["booktype"].getValStr();

	// validate symbol name and book type
	if (!validSymbol(inSymbol) ||
	    !validBookType(inBookType)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// verify this is not a duplicate
	if (market.symbolIsDefined(inSymbol)) {
		evhtp_send_reply(req, EVHTP_RES_FORBIDDEN);
		return;
	}

	// create new order book
	market.addBook(inSymbol, (inBookType == "depth"));

	UniValue obj(true);

	// successful operation.  Return JSON output.
	string body = obj.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

void reqMarketList(evhtp_request_t * req, void * a)
{
	UniValue res(UniValue::VARR);

	// request list of symbols from market
	vector<string> symbols;
	market.getSymbols(symbols);

	// copy vector of symbols to JSON result array
	for (vector<string>::iterator t = symbols.begin();
	     t != symbols.end(); t++) {
		res.push_back(*t);
	}

	// successful operation.  Return JSON output.
	string body = res.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
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
	case 1001:	// --bind-address
		opt_bind_address = arg;
		break;

	case 1002:	// --bind-port
		{
		int v = atoi(arg);
		if ((v > 0) && (v < 65536))
			opt_bind_port = (uint16_t) v;
		else
			argp_usage(state);
		break;
		}

	case ARGP_KEY_END:
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
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
	evhtp_bind_socket(htp, opt_bind_address, opt_bind_port, 1024);
	event_base_loop(evbase, 0);
	return 0;
}
