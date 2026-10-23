// MiSTer hybrid core: what the game asks of net_matchmaking.c (the online
// lobby, through curl) and net_portforward.cpp (UPnP and NAT-PMP), neither
// of which is built. Network games on the local network and by address work
// without them.

#include "pre_inc.h"
#include "net_matchmaking.h"
#include "net_portforward.h"
#include "post_inc.h"

struct TbNetworkSessionNameEntry matchmaking_sessions[MATCHMAKING_SESSIONS_MAX];
int matchmaking_session_count = 0;
char join_lobby_id[MATCHMAKING_ID_MAX];

void matchmaking_connect_async(void) {}
int matchmaking_connect(void) { return -1; }
int matchmaking_request_list(void) { return -1; }
void matchmaking_disconnect(void) {}
void matchmaking_refresh_sessions(void) {}
int matchmaking_create(const char *name, int udp_ipv4_port, int udp_ipv6_port) { return -1; }
int matchmaking_punch(const char *lobby_id, int udp_ipv4_port, int udp_ipv6_port, PunchAddresses *output) { return -1; }
int matchmaking_poll_punch(PunchAddresses *output) { return 0; }

int port_forward_add_mapping(uint16_t port) { return 0; }
void port_forward_remove_mapping(void) {}
