#!/usr/bin/env nodejs

const ApiClient = require('./apiclient');

var startTime = new Date();
var cli = new ApiClient();

if (process.argv.length < 3) {
    console.log("missing arguments");
    process.exit(1);
}
var cli_cmd = process.argv[2];
var cli_args = process.argv.slice(3);

if (cli_cmd == "info") {
	cli.info(function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else if (cli_cmd == "market.list") {
	cli.marketList(function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else if (cli_cmd == "market.add") {
	if (cli_args.length != 1) {
		console.log("missing symbol argument");
		process.exit(1);
	}

	var marketInfo = {
		symbol: cli_args[0],
		booktype: "simple",
	};

	if (cli_args.length > 1)
		marketInfo.booktype = cli_args[1];

	cli.marketAdd(marketInfo, function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else if (cli_cmd == "order") {
	if (cli_args.length != 1) {
		console.log("missing order-id argument");
		process.exit(1);
	}

	var orderId = cli_args[0];

	cli.orderGetInfo(orderId, function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else if (cli_cmd == "order.cancel") {
	if (cli_args.length != 1) {
		console.log("missing order-id argument");
		process.exit(1);
	}

	var orderId = cli_args[0];

	cli.orderCancel(orderId, function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else if (cli_cmd == "order.modify") {
	if (cli_args.length != 3) {
		console.log("missing order-id,price,qtyDelta arguments");
		process.exit(1);
	}

	var orderInfo = {
		oid:		cli_args[0],
		price:		parseInt(cli_args[1]),
		qtyDelta:	parseInt(cli_args[2]),
	};

	cli.orderModify(orderInfo, function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else if (cli_cmd == "order.add") {
	if (cli_args.length != 1) {
		console.log("missing json-order argument");
		process.exit(1);
	}

	var orderInfo = JSON.parse(cli_args[0]);

	cli.orderAdd(orderInfo, function(err, res) {
		if (err) { throw new Error(err); }

		console.dir(res);
	});

} else {
	console.log("unknown command");
	process.exit(1);
}

