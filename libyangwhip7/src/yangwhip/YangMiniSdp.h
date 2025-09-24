// Thin wrapper to use mini_sdp from C code paths in yangwhip
#ifndef YANG_WHIP_YANG_MINISDP_H_
#define YANG_WHIP_YANG_MINISDP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Build a minisdp request from offer SDP and URL, send via UDP, get answer SDP.
// Returns 0 on success, non-zero on failure. On success, *premoteSdp is malloc'ed and must be freed by caller (yang_free compatible).
int32_t yang_minisdp_getSignal(const char* offerSdp,
                               const char* url,
                               char** premoteSdp);

#ifdef __cplusplus
}
#endif

#endif // YANG_WHIP_YANG_MINISDP_H_
