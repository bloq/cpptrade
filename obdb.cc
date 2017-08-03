
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

	{ "load-auth", 1002, "FILE", 0,
	  "JSON input file containing account data" },

	{ "keys", 1003, NULL, 0,
	  "Dump all keys" },

	{ "dump", 1004, NULL, 0,
	  "Dump all keys and values" },

	{ "clear", 1005, NULL, 0,
	  "Delete all data, before loading" },

	{ }
};

static error_t parse_opt (int key, char *arg, struct argp_state *state);
static const struct argp argp = { options, parse_opt, NULL, doc };

static string opt_output_fn = DEFAULT_DATASTORE_FN;
static string load_accounts_fn;
static string load_auth_fn;
static bool opt_dump_keys = false;
static bool opt_dump_db = false;
static bool opt_clear_db = false;

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	switch (key) {

	case 'o':
		opt_output_fn = arg;
		break;

	case 1001:	// --load-acounts=file
		load_accounts_fn = arg;
		break;

	case 1002:	// --load-auth=file
		load_auth_fn = arg;
		break;

	case 1003:	// --keys
		opt_dump_keys = true;
		break;

	case 1004:	// --dump
		opt_dump_db = true;
		break;

	case 1005:
		opt_clear_db = true;
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

static void dbPrefixedLoad(rocksdb::DB* db,
			  const std::string& fn,
			  const std::string& prefix)
{
	UniValue jobj;
	bool rc = readJsonFile(fn, jobj);
	if (!rc) {
		perror(fn.c_str());
		exit(1);
	}

	assert(jobj.getType() == UniValue::VOBJ);

	const std::vector<std::string>& keys = jobj.getKeys();
	const std::vector<UniValue>& values = jobj.getValues();
	for (size_t i = 0; i < keys.size(); i++) {
		dbPutJson(db, prefix + keys[i], values[i]);
	}
}

static void dump_db(rocksdb::DB* db)
{
	rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		cout << it->key().ToString() << ": " << it->value().ToString() << endl;
	}
	assert(it->status().ok()); // Check for any errors found during the scan
	delete it;
}

static void dump_keys(rocksdb::DB* db)
{
	rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		cout << it->key().ToString() << endl;
	}
	assert(it->status().ok()); // Check for any errors found during the scan
	delete it;
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

	if (opt_clear_db)
		DestroyDB(opt_output_fn, rocksdb::Options());

	rocksdb::DB* db;
	rocksdb::Options options;
	options.create_if_missing = true;
	rocksdb::Status status =
	  rocksdb::DB::Open(options, opt_output_fn, &db);
	assert(status.ok());

	// input

	if (!load_accounts_fn.empty())
		dbPrefixedLoad(db, load_accounts_fn, "/acct/");
	if (!load_auth_fn.empty())
		dbPrefixedLoad(db, load_auth_fn, "/auth/");

	// output

	if (opt_dump_keys)
		dump_keys(db);
	if (opt_dump_db)
		dump_db(db);

	return 0;
}
