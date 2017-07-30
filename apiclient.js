
const http = require('http');

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
	opts.path = '/info';
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.marketList = function(callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/marketList';
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.marketAdd = function(marketInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/marketAdd';
	opts.postData = JSON.stringify(marketInfo);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.book = function(symbol, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/book';
	postObj = { "symbol": symbol };
	opts.postData = JSON.stringify(postObj);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.orderAdd = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/orderAdd';
	opts.postData = JSON.stringify(orderInfo);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.orderCancel = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/orderCancel';
	opts.postData = JSON.stringify(orderInfo);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.orderModify = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/orderModify';
	opts.postData = JSON.stringify(orderInfo);
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

ApiClient.prototype.orderGetInfo = function(orderId, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/order/' + orderId;
	opts.json = true;

	callHttp(opts, function(err, res) {
		if (err) { callback(err); return; }

		callback(null, res);
	});
};

module.exports = ApiClient;

