
#include <string>
#include <time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <univalue.h>
#include <stdio.h>
#include "Util.h"

using namespace std;

std::string addressToStr(const struct sockaddr *sockaddr, socklen_t socklen)
{
        char name[1025] = "";
        if (!getnameinfo(sockaddr, socklen, name, sizeof(name), NULL, 0, NI_NUMERICHOST))
            return std::string(name);

	return string("");
}

std::string formatTime(const std::string& fmt, time_t t)
{
	// convert unix seconds-since-epoch to split-out struct
	struct tm tm;
	gmtime_r(&t, &tm);

	// build formatted time string
	char timeStr[fmt.size() + 256];
	strftime(timeStr, sizeof(timeStr), fmt.c_str(), &tm);

	return string(timeStr);
}

bool readJsonFile(const std::string& filename, UniValue& jval)
{
	jval.clear();

        FILE *f = fopen(filename.c_str(), "r");
	if (!f)
		return false;

        string jdata;

        char buf[4096];
        while (!feof(f)) {
                int bread = fread(buf, 1, sizeof(buf), f);
		if (ferror(f)) {
			fclose(f);
			return false;
		}

                string s(buf, bread);
                jdata += s;
        }

	if (ferror(f)) {
		fclose(f);
		return false;
	}
        fclose(f);

	if (!jval.read(jdata))
		return false;

        return true;
}

