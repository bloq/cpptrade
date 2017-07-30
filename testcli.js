#!/usr/bin/env nodejs

const http = require('http');
const Async = require('async');
const ApiClient = require('./apiclient');

function test1(callback) {
	var cli = new ApiClient();

	cli.info(function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("INFO:");
		console.dir(res);
		callback(null, true);
	});
}

function test2(callback) {
	var cli = new ApiClient();

	var marketInfo = {
		"symbol": "GOOG",
		"booktype": "depth",
	};

	cli.marketAdd(marketInfo, function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("marketADD:");
		console.dir(res);
		callback(null, true);
	});
}

function test2b(callback) {
	var cli = new ApiClient();

	cli.marketList(function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("marketList:");
		console.dir(res);
		callback(null, true);
	});
}

function test2x(callback) {
	var cli = new ApiClient();

	var orderInfo = {
		is_buy: true,
		price: 1000,
		qty: 10,
	};

	cli.add(orderInfo, function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("ADD:");
		console.dir(res);
		callback(null, true);
	});
}

function test2a(callback) {
	var cli = new ApiClient();

	var orderInfo = {
		is_buy: true,
		price: 2000,
		qty: 10,
	};

	cli.add(orderInfo, function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("ADD 2:");
		console.dir(res);
		callback(null, true);
	});
}

function test3(callback) {
	var cli = new ApiClient();

	cli.depth(function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("DEPTH:");
		console.dir(res);
		callback(null, true);
	});
}

function test4(callback) {
	var cli = new ApiClient();

	cli.book(function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("BOOK:");
		console.dir(res);
		callback(null, true);
	});
}

function test5(callback) {
	var cli = new ApiClient();

	var orderInfo = {
		is_buy: true,
		price: 1000,
		qty: 10,
		order_id: 0,
	};


	cli.cancel(orderInfo, function(err, res) {
		if (err) { console.dir(err); console.dir(res); callback(null, false); return; }

		console.log("CANCEL:");
		console.dir(res);
		callback(null, true);
	});
}

function test6(callback) {
	var cli = new ApiClient();

	cli.book(function(err, res) {
		if (err) { callback(null, false); return; }

		console.log("BOOK 2:");
		console.dir(res);
		callback(null, true);
	});
}

Async.series({
	t1: test1,
	t2: test2,
	t2b: test2b,
//	t2a: test2a,
//	t3: test3,
//	t4: test4,
//	t5: test5,
//	t6: test6,
}, function(err, results) {
	if (err) { console.log("error: " + err); }
	console.dir(results);
});

