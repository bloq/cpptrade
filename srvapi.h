#ifndef __OBSRV_API_H__
#define __OBSRV_API_H__

#include <evhtp.h>

void reqOrderAdd(evhtp_request_t * req, void * arg);
void reqOrderModify(evhtp_request_t * req, void * arg);
void reqOrderCancel(evhtp_request_t * req, void * arg);
void reqOrderBookList(evhtp_request_t * req, void * arg);
void reqMarketAdd(evhtp_request_t * req, void * arg);
void reqMarketList(evhtp_request_t * req, void * a);

#endif // __OBSRV_API_H__
