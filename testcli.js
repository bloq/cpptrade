#!/usr/bin/env nodejs

const http = require('http');
const Async = require('async');

function callHttp(opts, callback) {
	var body = '';

	if (!opts.headers)
		opts.headers = {};
	if (opts.postData) {
		opts.headers['Content-Length'] = opts.postData.length.toString();
	}
	if (opts.json) {
		opts.headers['Content-Type'] = 'application/json';
	}

	const req = http.request(opts, (res) => {
		if (!opts.isbinary)
			res.setEncoding('utf8');

		res.on('data', (chunk) => {
			body += chunk;
		});

		res.on('end', () => {
			if (res.statusCode == 404) {
				callback(null, null);
				return;
			}
			if (res.statusCode != 200) {
				var e = new Error("HTTP status " +
						   res.statusCode.toString() +
						   ": " +
						   res.statusMessage);
				callback(e);
				return;
			}

			if (!opts.json) {
				callback(null, body);
			} else {
				var jval;
				try {
					jval = JSON.parse(body);
					callback(null, jval);
				}
				catch (e) {
					callback(e);
					return;
				}
			}
		});
	});

	req.on('error', (e) => {
		callback(e);
		return;
	});

	if (opts.postData)
		req.write(opts.postData);
	req.end();
}

function ApiClient() {
	this.httpOpts = {
		hostname: '127.0.0.1',
		port: 7979,
	};
}
ApiClient.prototype.info = function(callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/';
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.depth = function(callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/v1/depth';
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.book = function(callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/v1/book';
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.add = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/v1/add';
	opts.postData = JSON.stringify(orderInfo);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.cancel = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/v1/cancel';
	opts.postData = JSON.stringify(orderInfo);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

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
//	t2: test2,
//	t2a: test2a,
//	t3: test3,
//	t4: test4,
//	t5: test5,
//	t6: test6,
}, function(err, results) {
	if (err) { console.log("error: " + err); }
	console.dir(results);
});

