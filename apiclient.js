
const http = require('http');
const crypto = require('crypto');

function canonicalReq(headers, username) {
    var canon = "cscpp1-sha256\n" +
		username + "\n" +
                headers["Host"] + "\n" +
                headers["X-Unixtime"] + "\n" +
                headers["ETag"] + "\n";

    return canon;
}

function makeAuthHdr(headers, username, secret) {
    var canonTxt = canonicalReq(headers, username);

    var hmac = crypto.createHmac('sha256', secret);
    hmac.update(canonTxt, 'utf8');
    var hexDigest = hmac.digest('hex');

    var hdr = "cscpp1-sha256 " + username + " " + hexDigest;
    return hdr;
}

function callHttp(opts, callback) {
	var body = '';

	if (!opts.headers)
		opts.headers = {};
	if (opts.postData) {
		const contentLen = opts.postData.length;
		opts.headers['Content-Length'] = contentLen.toString();

		var input_hash = crypto.createHash('sha256');
		input_hash.update(opts.postData, 'utf8');
		var input_digest = input_hash.digest('hex');

		opts.headers['ETag'] = input_digest;
	}
	if (opts.apiJson) {
		opts.headers['Content-Type'] = 'application/json';
	}

	const unixTimestamp = Math.floor(Date.now() / 1000);
	opts.headers['X-Unixtime'] = unixTimestamp.toString();

	opts.headers['Host'] = opts.hostname;

	if (opts.auth256) {
		const authHdr = makeAuthHdr(opts.headers,
					    opts.username,
					    opts.secret);
		opts.headers["Authorization"] = authHdr;
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

			if (!opts.apiJson) {
				callback(null, body);
				return;
			} else {
				var jval;
				try {
					jval = JSON.parse(body);
					callback(null, jval);
					return;
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
		username: 'testuser',
		secret: 'testpass',
	};
}

ApiClient.prototype.info = function(callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/info';
	opts.apiJson = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.marketList = function(callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/marketList';
	opts.apiJson = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.marketAdd = function(marketInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/marketAdd';
	opts.postData = JSON.stringify(marketInfo);
	opts.apiJson = true;
	opts.auth256 = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.book = function(symbol, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/book/' + symbol;
	opts.apiJson = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.orderAdd = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/orderAdd';
	opts.postData = JSON.stringify(orderInfo);
	opts.apiJson = true;
	opts.auth256 = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.orderCancel = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/orderCancel';
	opts.postData = JSON.stringify(orderInfo);
	opts.apiJson = true;
	opts.auth256 = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.orderModify = function(orderInfo, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'POST';
	opts.path = '/orderModify';
	opts.postData = JSON.stringify(orderInfo);
	opts.apiJson = true;
	opts.auth256 = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

ApiClient.prototype.orderGetInfo = function(orderId, callback) {
	var opts = JSON.parse(JSON.stringify(this.httpOpts));
	opts.method = 'GET';
	opts.path = '/order/' + orderId;
	opts.apiJson = true;
	opts.auth256 = true;

	callHttp(opts, function(err, res) {
		if (err) { throw new Error(err); }

		callback(null, res);
	});
};

module.exports = ApiClient;

