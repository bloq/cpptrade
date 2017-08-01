
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
#include "HttpUtil.h"
#include "srv.h"

using namespace std;
using namespace orderentry;

static UniValue uvFromTv(const struct timeval *tv)
{
	string tmp = to_string(tv->tv_sec) + "." + to_string(tv->tv_usec);

	UniValue ret;
	ret.setNumStr(tmp);

	return ret;
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

void reqOrderInfo(evhtp_request_t * req, void *arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

	string orderId(req->uri->path->match_start);

	OrderPtr order;
	OrderBookPtr book;
	if (!market.findExistingOrder(orderId, order, book))
	{
		evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
		return;
	}

	UniValue res(UniValue::VOBJ);
	res.pushKV("id", orderId);
	res.pushKV("side", order->is_buy() ? "buy" : "sell");
	res.pushKV("qty", (uint64_t) order->order_qty());
	res.pushKV("price", (uint64_t) order->price());

	UniValue bval;
	bval.setBool(order->all_or_none());
	res.pushKV("aon", bval);

	bval.setBool(order->immediate_or_cancel());
	res.pushKV("ioc", bval);

	struct timeval tv = order->timestamp();
	res.pushKV("submitted_at", uvFromTv(&tv));

	string orderType;
	if (order->is_limit())
		orderType = "limit";
	else
		orderType = "market";
	if (order->stop_price() > 0) {
		orderType += "-stop";
		res.pushKV("stop_price", (uint64_t) order->stop_price());
	}
	res.pushKV("type", orderType);

	// successful operation.  Return JSON output.
	httpJsonReply(req, res);
}

void reqOrderAdd(evhtp_request_t * req, void * arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

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
	httpJsonReply(req, res);
}

void reqOrderModify(evhtp_request_t * req, void * arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

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
	httpJsonReply(req, res);
}

void reqOrderCancel(evhtp_request_t * req, void * arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

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
	httpJsonReply(req, res);
}

void reqOrderBookList(evhtp_request_t * req, void * arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

	// obtain symbol from uri regex matched substring
	string inSymbol(req->uri->path->match_start);

	// depth=N query param.  valid: 1-3, default 1.
	int64_t depth;
	if (!query_int64_range(req, "depth", depth, 1, 3, 1)) {
		evhtp_send_reply(req, EVHTP_RES_BADREQ);
		return;
	}

	// lookup order book from symbol
	auto book = market.findBook(inSymbol);
	if (!book) {
		evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
		return;
	}

	UniValue asksArr(UniValue::VARR);
	UniValue bidsArr(UniValue::VARR);
	const OrderBook::TrackerMap& asks_ = book->asks();
	const OrderBook::TrackerMap& bids_ = book->bids();

	switch (depth) {
		case 1:
		case 2:
			{
			map<liquibook::book::Price,liquibook::book::Quantity> agg_bids;
			map<liquibook::book::Price,liquibook::book::Quantity> agg_asks;

			// aggregate list of asks from order book
			for (auto ask = asks_.rbegin(); ask != asks_.rend(); ++ask) {
				liquibook::book::Price price = ask->first.price();
				liquibook::book::Quantity qty = ask->second.open_qty();
				if (agg_asks.count(price))
					agg_asks[price] += qty;
				else
					agg_asks[price] = qty;
			}

			// aggregate list of bids from order book
			for (auto bid = bids_.rbegin(); bid != bids_.rend(); ++bid) {
				liquibook::book::Price price = bid->first.price();
				liquibook::book::Quantity qty = bid->second.open_qty();
				if (agg_bids.count(price))
					agg_bids[price] += qty;
				else
					agg_bids[price] = qty;
			}

			// output best bid/ask
			if (depth == 1) {
				auto ask = agg_asks.begin();
				UniValue askObj(UniValue::VOBJ);
				askObj.pushKV("price", (int64_t) ask->first);
				askObj.pushKV("qty", (int64_t) ask->second);
				asksArr.push_back(askObj);

				auto bid = agg_bids.rbegin();
				UniValue bidObj(UniValue::VOBJ);
				bidObj.pushKV("price", (int64_t) bid->first);
				bidObj.pushKV("qty", (int64_t) bid->second);
				bidsArr.push_back(bidObj);

			// output aggregated order book
			} else {
				// gather list of aggregate asks from order book
				for (auto ask = agg_asks.begin();
				     ask != agg_asks.end(); ++ask) {
					UniValue askObj(UniValue::VOBJ);
					askObj.pushKV("price", (int64_t) ask->first);
					askObj.pushKV("qty", (int64_t) ask->second);
					asksArr.push_back(askObj);
				}

				// gather list of aggregate bids from order book
				for (auto bid = agg_bids.begin();
				     bid != agg_bids.end(); ++bid) {
					UniValue bidObj(UniValue::VOBJ);
					bidObj.pushKV("price", (int64_t) bid->first);
					bidObj.pushKV("qty", (int64_t) bid->second);
					bidsArr.push_back(bidObj);
				}
			}
			break;
			}

		case 3:
			{
			// gather list of asks from order book
			for (auto ask = asks_.rbegin(); ask != asks_.rend(); ++ask) {
				UniValue askObj(UniValue::VOBJ);
				askObj.pushKV("price", (int64_t) ask->first.price());
				askObj.pushKV("qty", (int64_t) ask->second.open_qty());
				asksArr.push_back(askObj);
			}

			// gather list of bids from order book
			for (auto bid = bids_.rbegin(); bid != bids_.rend(); ++bid) {
				UniValue bidObj(UniValue::VOBJ);
				bidObj.pushKV("price", (int64_t) bid->first.price());
				bidObj.pushKV("qty", (int64_t) bid->second.open_qty());
				bidsArr.push_back(bidObj);
			}
			break;
			}

		default:
			// should not happen
			assert(0);
			break;
	}

	// build return object
	UniValue obj(UniValue::VOBJ);
	obj.pushKV("bids", bidsArr);
	obj.pushKV("asks", asksArr);

	// successful operation.  Return JSON output.
	httpJsonReply(req, obj);
}

void reqMarketAdd(evhtp_request_t * req, void * arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

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
		evhtp_send_reply(req, EVHTP_RES_NACCEPTABLE);
		return;
	}

	// create new order book
	market.addBook(inSymbol, (inBookType == "depth"));

	UniValue bobj(true);

	// successful operation.  Return JSON output.
	httpJsonReply(req, bobj);
}

void reqMarketList(evhtp_request_t * req, void *arg)
{
	assert(req && arg);
	ReqState *state = (ReqState *) arg;

	// global pre-request processing
	if (!reqPreProcessing(req, state))
		return;		// pre-processing failed; response already sent

	UniValue res(UniValue::VARR);

	// request list of symbols from market
	vector<string> symbols;
	market.getSymbols(symbols);

	// copy vector of symbols to JSON result array
	for (auto t = symbols.begin(); t != symbols.end(); t++) {
		res.push_back(*t);
	}

	// successful operation.  Return JSON output.
	httpJsonReply(req, res);
}

