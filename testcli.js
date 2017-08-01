#!/usr/bin/env nodejs

const http = require('http');
const Async = require('async');
const ApiClient = require('./apiclient');

function step_info(callback) {
	var cli = new ApiClient();

	cli.info(function(err, res) {
		if (err) { throw new Error(err); }

		console.log("INFO:");
		console.dir(res);
		callback(null, true);
	});
}

function step_marketAdd(callback) {
	var cli = new ApiClient();

	var marketInfo = {
		"symbol": "GOOG",
		"booktype": "depth",
	};

	cli.marketAdd(marketInfo, function(err, res) {
		if (err) { throw new Error(err); }

		console.log("marketADD:");
		console.dir(res);
		callback(null, true);
	});
}

function step_marketList(callback) {
	var cli = new ApiClient();

	cli.marketList(function(err, res) {
		if (err) { throw new Error(err); }

		console.log("marketList:");
		console.dir(res);
		callback(null, true);
	});
}

function step_addBuy(callback) {
	var cli = new ApiClient();

	var orderInfo = {
		symbol: "GOOG",
		is_buy: true,
		price: 100,
		qty: 100,
	};

	cli.orderAdd(orderInfo, function(err, res) {
		if (err) { throw new Error(err); }

		console.log("ADD:");
		console.dir(res);
		callback(null, true);
	});
}
function step_addSell(callback) {
	var cli = new ApiClient();

	var orderInfo = {
		symbol: "GOOG",
		is_buy: false,
		price: 750,
		qty: 1000,
	};

	cli.orderAdd(orderInfo, function(err, res) {
		if (err) { throw new Error(err); }

		console.log("ADD:");
		console.dir(res);
		callback(null, true);
	});
}


function step_orderBook(callback) {
	var cli = new ApiClient();

	var bookOpt = {
		symbol: "GOOG",
		depth: 3,
	};

	cli.book(bookOpt, function(err, res) {
		if (err) { throw new Error(err); }

		console.log("BOOK:");
		console.dir(res);
		callback(null, true);
	});
}

Async.series({
	t0: step_info,
	t1: step_marketAdd,
	t2: step_marketList,
	t3a: step_addBuy,
	t3b: step_addBuy,
	t3c: step_addBuy,
	t3d: step_addBuy,
	t4a: step_addSell,
	t4b: step_addSell,
	t4c: step_addSell,
	t4d: step_addSell,
	t5: step_orderBook,
}, function(err, results) {
	if (err) { console.log("error: " + err); }
	console.dir(results);
});

