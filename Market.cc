// Copyright (c) 2017 Object Computing, Inc.
// All rights reserved.
// See the file license.txt for licensing information.
#include "Market.h"
#include "Util.h"

#include <functional> 
#include <cctype>
#include <locale>

namespace {
    ///////////////////////
    // depth display helper
    void displayDepthLevel(std::ostream & out, const liquibook::book::DepthLevel & level)
    {
        out << "\tPrice "  <<  level.price();
        out << " Count: " << level.order_count();
        out << " Quantity: " << level.aggregate_qty();
        if(level.is_excess())
        {
            out << " EXCESS";
        }
        out << " Change id#: " << level.last_change();
        out << std::endl;
    }

    void publishDepth(std::ostream & out, const orderentry::BookDepth & depth)
    {
        liquibook::book::ChangeId published = depth.last_published_change();
        bool needTitle = true;
        // Iterate awkwardly
        auto pos = depth.bids();
        auto back = depth.last_bid_level();
        bool more = true;
        while(more)
        {
            if(pos->aggregate_qty() !=0 && pos->last_change() > published)
            {
                if(needTitle)
                {
                    out << "\n\tBIDS:\n";
                    needTitle = false;
                }
                displayDepthLevel(out, *pos);
            }
            ++pos;
            more = pos != back;
        }

        needTitle = true;
        pos = depth.asks();
        back = depth.last_ask_level();
        more = true;
        while(more)
        {
            if(pos->aggregate_qty() !=0 && pos->last_change() > published)
            {
                if(needTitle)
                {
                    out << "\n\tASKS:\n";
                    needTitle = false;
                }
                displayDepthLevel(out, *pos);
            }
            ++pos;
            more = pos != back;
        }
    }
}

namespace orderentry
{

uint32_t Market::orderIdSeed_ = 0;

Market::Market(std::ostream * out)
: logFile_(out)
{
}

Market::~Market()
{
}

void Market::orderSubmit(OrderBookPtr book, OrderPtr order,
			 const std::string& orderIdStr,
			 liquibook::book::OrderConditions conditions)
{
    order->onSubmitted();
    out() << "ADDING order:  " << *order << std::endl;

    orders_[orderIdStr] = order;
    book->add(order, conditions);
}

///////////
// CANCEL
bool Market::orderCancel(const std::string & orderIdStr)
{
    OrderPtr order;
    OrderBookPtr book;
    if (!findExistingOrder(orderIdStr, order, book))
    {
        return false;
    }

    out() << "Requesting Cancel: " << *order << std::endl;
    book->cancel(order);
    return true;
}

///////////
// MODIFY
bool Market::orderModify(const std::string & orderIdStr,
			 int32_t quantityChange,
			 liquibook::book::Price price)
{
    OrderPtr order;
    OrderBookPtr book;
    if(!findExistingOrder(orderIdStr, order, book))
    {
        return false;
    }

	if (price != liquibook::book::PRICE_UNCHANGED) {
		if ((price <= 0) ||
		    (price == INVALID_UINT32))
			return false;
	}

	if (quantityChange != liquibook::book::SIZE_UNCHANGED) {
		if (quantityChange == INVALID_INT32)
			return false;
	}

    book->replace(order, quantityChange, price);
    out() << "Requested Modify" ;
    if(quantityChange != liquibook::book::SIZE_UNCHANGED)
    {
        out() << " QUANTITY  += " << quantityChange;
    }
    if(price != liquibook::book::PRICE_UNCHANGED)
    {
        out() << " PRICE " << price;
    }
    out() << std::endl;
    return true;
}

///////////
// getSymbols
void Market::getSymbols(std::vector<std::string> & symbols)
{
	symbols.clear();

        for(auto pBook = books_.begin(); pBook != books_.end(); ++pBook)
        {
	    symbols.push_back(pBook->first);
        }
}

/////////////////////////////
// Order book interactions

bool
Market::symbolIsDefined(const std::string & symbol)
{
    auto book = books_.find(symbol);
    return book != books_.end();
}

OrderBookPtr
Market::addBook(const std::string & symbol, bool useDepthBook)
{
    OrderBookPtr result;
    if(useDepthBook)
    {
        out() << "Create new depth order book for " << symbol << std::endl;
        DepthOrderBookPtr depthBook = std::make_shared<DepthOrderBook>(symbol);
        depthBook->set_bbo_listener(this);
        depthBook->set_depth_listener(this);
        result = depthBook;
    }
    else
    {
        out() << "Create new order book for " << symbol << std::endl;
        result = std::make_shared<OrderBook>(symbol);
    }
    result->set_order_listener(this);
    result->set_trade_listener(this);
    result->set_order_book_listener(this);
    books_[symbol] = result;
    return result;
}

OrderBookPtr
Market::findBook(const std::string & symbol)
{
    OrderBookPtr result;
    auto entry = books_.find(symbol);
    if(entry != books_.end())
    {
        result = entry->second;
    }
    return result;
}

bool Market::findExistingOrder(const std::string & orderId, OrderPtr & order, OrderBookPtr & book)
{
    auto orderPosition = orders_.find(orderId);
    if(orderPosition == orders_.end())
    {
        out() << "--Can't find OrderID #" << orderId << std::endl;
        return false;
    }

    order = orderPosition->second;
    std::string symbol = order->symbol();
    book = findBook(symbol);
    if(!book)
    {
        out() << "--No order book for symbol" << symbol << std::endl;
        return false;
    }
    return true;
}

/////////////////////////////////////
// Implement OrderListener interface

void 
Market::on_accept(const OrderPtr& order)
{
    order->onAccepted();
    out() << "\tEvent:Accepted: " <<*order<< std::endl;
}

void 
Market::on_reject(const OrderPtr& order, const char* reason)
{
    order->onRejected(reason);
    out() << "\tEvent:Rejected: " <<*order<< ' ' << reason << std::endl;

}

void 
Market::on_fill(const OrderPtr& order, 
    const OrderPtr& matched_order, 
    liquibook::book::Quantity fill_qty, 
    liquibook::book::Cost fill_cost)
{
    order->onFilled(fill_qty, fill_cost);
    matched_order->onFilled(fill_qty, fill_cost);
    out() << (order->is_buy() ? "\tEvent:Fill-Bought: " : "\tEvent:Fill-Sold: ") 
        << fill_qty << " Shares for " << fill_cost << ' ' <<*order<< std::endl;
    out() << (matched_order->is_buy() ? "\tBought: " : "\tSold: ") 
        << fill_qty << " Shares for " << fill_cost << ' ' << *matched_order << std::endl;
}

void 
Market::on_cancel(const OrderPtr& order)
{
    order->onCancelled();
    out() << "\tEvent:Canceled: " << *order<< std::endl;
}

void Market::on_cancel_reject(const OrderPtr& order, const char* reason)
{
    order->onCancelRejected(reason);
    out() << "\tEvent:Cancel Reject: " <<*order<< ' ' << reason << std::endl;
}

void Market::on_replace(const OrderPtr& order, 
    const int32_t& size_delta, 
    liquibook::book::Price new_price)
{
    order->onReplaced(size_delta, new_price);
    out() << "\tEvent:Modify " ;
    if(size_delta != liquibook::book::SIZE_UNCHANGED)
    {
        out() << " QUANTITY  += " << size_delta;
    }
    if(new_price != liquibook::book::PRICE_UNCHANGED)
    {
        out() << " PRICE " << new_price;
    }
    out() <<*order<< std::endl;
}

void 
Market::on_replace_reject(const OrderPtr& order, const char* reason)
{
    order->onReplaceRejected(reason);
    out() << "\tEvent:Replace Reject: " <<*order<< ' ' << reason << std::endl;
}

////////////////////////////////////
// Implement TradeListener interface

void 
Market::on_trade(const OrderBook* book, 
    liquibook::book::Quantity qty, 
    liquibook::book::Cost cost)
{
    out() << "\tEvent:Trade: " << qty <<  ' ' << book->symbol() << " Cost "  << cost  << std::endl;
}

/////////////////////////////////////////
// Implement OrderBookListener interface

void 
Market::on_order_book_change(const OrderBook* book)
{
    out() << "\tEvent:Book Change: " << ' ' << book->symbol() << std::endl;
}



/////////////////////////////////////////
// Implement BboListener interface
void 
Market::on_bbo_change(const DepthOrderBook * book, const BookDepth * depth)
{
    out() << "\tEvent:BBO Change: " << ' ' << book->symbol() 
        << (depth->changed() ? " Changed" : " Unchanged")
        << " Change Id: " << depth->last_change()
        << " Published: " << depth->last_published_change()
        << std::endl;

}

/////////////////////////////////////////
// Implement DepthListener interface
void 
Market::on_depth_change(const DepthOrderBook * book, const BookDepth * depth)
{
    out() << "\tEvent:Depth Change: " << ' ' << book->symbol();
    out() << (depth->changed() ? " Changed" : " Unchanged")
        << " Change Id: " << depth->last_change()
        << " Published: " << depth->last_published_change();
    publishDepth(out(), *depth);
    out() << std::endl;
}

}  // namespace orderentry
