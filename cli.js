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

if (cli_cmd == "help") {
	const msg =
	"Commands:\n" +
	"info\t\t\t\tShow server info\n" +
	"market.list\t\t\tShow all markets\n" +
	"market.add SYMBOL [booktype]\tAdd new market\n" +
	"book SYMBOL [depth]\t\tShow order book\n" +
	"order order-id\t\t\tShow info on a single order\n" +
	"order.cancel order-id\t\tCancel a single order\n" +
	"order.modify order-id\t\tModify a single order\n" +
	"order.add [json order info]\tAdd new order\n";

	console.log(msg);
} else if (cli_cmd == "info") {
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

} else if (cli_cmd == "book") {
	if (cli_args.length < 1) {
		console.log("missing symbol argument");
		process.exit(1);
	}

	var symbol = cli_args[0];
	var depth = 1;
	if (cli_args.length > 1) {
		depth = parseInt(cli_args[1]);
		if ((depth < 1) || (depth > 3)) {
			console.log("invalid depth argument");
			process.exit(1);
		}
	}

	var bookOpt = {
		"symbol": symbol,
		"depth": depth,
	};

	cli.book(bookOpt, function(err, res) {
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

