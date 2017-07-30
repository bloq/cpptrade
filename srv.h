#ifndef __SRV_H__
#define __SRV_H__

#include <sys/time.h>
#include <string>
#include <cstdint>
#include "Market.h"

class ReqState {
public:
	std::string		body;
	std::string		path;
	struct timeval		tstamp;

	ReqState() {
		gettimeofday(&tstamp, NULL);
	}
};

extern orderentry::Market market;
extern uint32_t nextOrderId;

#endif // __SRV_H__
