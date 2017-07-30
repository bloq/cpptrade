
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
#include <uuid/uuid.h>
#include <assert.h>
#include "Market.h"
#include "srv.h"

using namespace std;
using namespace orderentry;

static bool parseBySchema(ReqState *state,
			  const std::map<std::string,UniValue::VType>& schema,
			  UniValue& jval)
{
	if (!jval.read(state->body) ||
	    !jval.checkObject(schema))
		return false;

	return true;
}

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
	uuid_t id;
	char id_str[42];
	uuid_generate_random(id);
	uuid_unparse_lower(id, id_str);
	std::string orderId(id_str);

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
	res.pushKV("orderId", orderId);

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
	apiSchema["oid"] = UniValue::VSTR;

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
	string orderId = jval["oid"].getValStr();
	int32_t qtyDelta = liquibook::book::SIZE_UNCHANGED;
	if (jval.exists("qtyDelta"))
		qtyDelta = atoll(jval["qtyDelta"].getValStr().c_str());
	liquibook::book::Price price = liquibook::book::PRICE_UNCHANGED;
	if (jval.exists("price"))
		price = atoll(jval["price"].getValStr().c_str());

	// submit modification request to order book
	bool rc = market.orderModify(orderId, qtyDelta, price);

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
	apiSchema["oid"] = UniValue::VSTR;

	// parse input into JSON + preliminary input validation
	UniValue jval;
	if (!parseBySchema(state, apiSchema, jval)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// copy JSON input params into more manageable temporary variables
	string orderId = jval["oid"].getValStr();

	// submit cancellation request to order book
	bool rc = market.orderCancel(orderId);

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
	for (auto t = symbols.begin(); t != symbols.end(); t++) {
		res.push_back(*t);
	}

	// successful operation.  Return JSON output.
	string body = res.write(2) + "\n";

	evbuffer_add(req->buffer_out, body.c_str(), body.size());
	evhtp_send_reply(req, EVHTP_RES_OK);
}

