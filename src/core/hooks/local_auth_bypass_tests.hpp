#ifndef LOCAL_AUTH_BYPASS_TESTS_HOOK_HPP
#define LOCAL_AUTH_BYPASS_TESTS_HOOK_HPP

#include <cstdint>

inline std::int64_t (*local_auth_bypass_tests_validate_matchmaking_server_original)(void* self) = nullptr;
inline std::int64_t (*local_auth_bypass_tests_connect_to_server_original)(void* self, const char* connect) = nullptr;

std::int64_t local_auth_bypass_tests_validate_matchmaking_server_hook(void* self);
void local_auth_bypass_tests_on_paint();

#endif
