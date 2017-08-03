
#include "cscpp-config.h"

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <locale>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <argp.h>
#include <unistd.h>
#include <evhtp.h>
#include <ctype.h>
#include <assert.h>
#include <univalue.h>
#include "Market.h"
#include "Util.h"
#include "HttpUtil.h"
#include "srvapi.h"
#include "srv.h"
#include "rocksdb/db.h"

using namespace std;
using namespace orderentry;

#define PROGRAM_NAME "obdb"

static const char doc[] =
PROGRAM_NAME " - order book server db util";

static struct argp_option options[] = {
	{ "output", 'o', "FILE", 0,
	  "Output rocksdb database root (default: " DEFAULT_DATASTORE_FN ")" },

	{ "load-accounts", 1001, "FILE", 0,
	  "JSON input file containing account data" },

	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, NULL, doc };

static string opt_output_fn = DEFAULT_DATASTORE_FN;
static string load_accounts_fn;

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {

	case 'o':
		opt_output_fn = arg;
		break;

	case 1001:
		load_accounts_fn = arg;
		break;

	case ARGP_KEY_END:
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static void dbPutJson(rocksdb::DB* db, const std::string& key,
		      const UniValue& jval)
{
	string val = jval.write();

	rocksdb::Status s = db->Put(rocksdb::WriteOptions(), key, val);
	assert(s.ok());
}

static void do_load_accounts(rocksdb::DB* db)
{
	UniValue accounts;
	bool rc = readJsonFile(load_accounts_fn, accounts);
	assert(rc);

	assert(accounts.getType() == UniValue::VOBJ);

	const std::vector<std::string>& keys = accounts.getKeys();
	const std::vector<UniValue>& values = accounts.getValues();
	for (size_t i = 0; i < keys.size(); i++) {
		dbPutJson(db, "/acct/" + keys[i], values[i]);
	}
}

int main(int argc, char ** argv)
{
	// parse command line
	error_t argp_rc = argp_parse(&argp, argc, argv, 0, NULL, NULL);
	if (argp_rc) {
		fprintf(stderr, "%s: argp_parse failed: %s\n",
			argv[0], strerror(argp_rc));
		return EXIT_FAILURE;
	}

	rocksdb::DB* db;
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status =
	  rocksdb::DB::Open(options, opt_output_fn, &db);
	assert(status.ok());

	if (!load_accounts_fn.empty())
		do_load_accounts(db);

	return 0;
}
