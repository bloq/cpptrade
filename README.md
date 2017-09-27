
# Summary

C++ order booking trading / matching engine

# Dependencies

OpenSSL, RocksDB

# Building and installing

This uses the standard autotools pattern:

	$ ./autogen.sh
	$ ./configure
	$ make			# compile
	$ make check		# run tests
	$ sudo make install	# install on system

# Interface

obsrv speaks JSON-RPC over HTTP.

# Test client

A test client `cli.js` is available.  Run `./cli.js help` for a summary
of example commands:

	$ ./cli.js help
	Commands:
	info				Show server info
	market.list			Show all markets
	market.add SYMBOL [booktype]	Add new market
	book SYMBOL [depth]		Show order book
	order order-id			Show info on a single order
	order.cancel order-id		Cancel a single order
	order.modify order-id		Modify a single order
	order.add [json order info]	Add new order

