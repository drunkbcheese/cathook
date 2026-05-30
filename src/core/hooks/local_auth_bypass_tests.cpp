#include "local_auth_bypass_tests.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>

#include "core/print.hpp"
#include "features/menu/config.hpp"
#include "games/tf2/sdk/interfaces/steam_networking_utils.hpp"
#include "games/tf2/sdk/interfaces/steam_runtime.hpp"

namespace
{

constexpr std::ptrdiff_t connect_string_offset = 0x748;
constexpr int sdr_client_debug_ticket_address = 30;
constexpr std::size_t max_connect_string_length = 512;

using set_global_config_value_string_fn = bool (*)(steam_networking_utils* self, int value, const char* data);

char last_connect_string[max_connect_string_length]{};

set_global_config_value_string_fn resolve_set_global_config_value_string()
{
  return reinterpret_cast<set_global_config_value_string_fn>(
    dlsym(RTLD_DEFAULT, "SteamAPI_ISteamNetworkingUtils_SetGlobalConfigValueString"));
}

steam_networking_utils* resolve_local_auth_steam_networking_utils()
{
  using steam_networking_utils_factory_fn = steam_networking_utils* (*)();

  steam_networking_utils_factory_fn factory =
    steam_runtime::resolve_loaded_symbol<steam_networking_utils_factory_fn>(
      "SteamAPI_SteamNetworkingUtils_SteamAPI_v004");
  if (factory != nullptr)
  {
    if (steam_networking_utils* utils = factory())
    {
      return utils;
    }
  }

  constexpr const char* modules[] = {
    "./bin/linux64/steamclient.so",
    "steamclient.so",
    "./bin/linux64/libsteam_api.so",
    "libsteam_api.so",
  };

  constexpr const char* versions[] = {
    "SteamNetworkingUtils004",
    "SteamNetworkingUtils003",
  };

  for (const char* module : modules)
  {
    for (const char* version : versions)
    {
      if (steam_networking_utils* utils = static_cast<steam_networking_utils*>(get_interface(module, version)))
      {
        return utils;
      }
    }
  }

  return nullptr;
}

bool is_valid_connect_string(const char* connect)
{
  return connect != nullptr && connect[0] != '\0';
}

bool is_local_auth_bypass_tests_enabled()
{
  return config.debug.insider_settings_unlocked && config.misc.exploits.local_auth_bypass_tests;
}

bool set_debug_ticket_address(const char* connect)
{
  set_global_config_value_string_fn set_global_config_value_string = resolve_set_global_config_value_string();
  if (set_global_config_value_string == nullptr)
  {
    static bool warned_config_setter_unavailable = false;
    if (!warned_config_setter_unavailable)
    {
      print("[local_auth_bypass_tests] SteamNetworking config setter unavailable\n");
      warned_config_setter_unavailable = true;
    }
    return false;
  }

  if (steam_networking_utils_interface == nullptr)
  {
    steam_networking_utils_interface = resolve_local_auth_steam_networking_utils();
  }

  if (steam_networking_utils_interface == nullptr)
  {
    static bool warned_utils_unavailable = false;
    if (!warned_utils_unavailable)
    {
      print("[local_auth_bypass_tests] SteamNetworkingUtils unavailable\n");
      warned_utils_unavailable = true;
    }
    return false;
  }

  if (!set_global_config_value_string(
        steam_networking_utils_interface,
        sdr_client_debug_ticket_address,
        connect))
  {
    static bool warned_set_failed = false;
    if (!warned_set_failed)
    {
      print("[local_auth_bypass_tests] failed to set debug ticket address: %s\n", connect);
      warned_set_failed = true;
    }
    return false;
  }

  return true;
}

void remember_connect_string(const char* connect)
{
  if (!is_valid_connect_string(connect))
  {
    last_connect_string[0] = '\0';
    return;
  }

  std::strncpy(last_connect_string, connect, max_connect_string_length - 1);
  last_connect_string[max_connect_string_length - 1] = '\0';
}

}

std::int64_t local_auth_bypass_tests_validate_matchmaking_server_hook(void* self)
{
  if (!is_local_auth_bypass_tests_enabled() || self == nullptr)
  {
    return local_auth_bypass_tests_validate_matchmaking_server_original != nullptr
      ? local_auth_bypass_tests_validate_matchmaking_server_original(self)
      : 0;
  }

  const char* connect = reinterpret_cast<const char*>(static_cast<std::uint8_t*>(self) + connect_string_offset);
  if (is_valid_connect_string(connect) && local_auth_bypass_tests_connect_to_server_original != nullptr)
  {
    remember_connect_string(connect);
    if (!set_debug_ticket_address(connect))
    {
      if (local_auth_bypass_tests_validate_matchmaking_server_original == nullptr)
      {
        return 0;
      }

      return local_auth_bypass_tests_validate_matchmaking_server_original(self);
    }

    print("[local_auth_bypass_tests] trying matchmaking connect without client validation: %s\n", connect);
    return local_auth_bypass_tests_connect_to_server_original(self, connect);
  }

  if (local_auth_bypass_tests_validate_matchmaking_server_original == nullptr)
  {
    return 0;
  }

  return local_auth_bypass_tests_validate_matchmaking_server_original(self);
}

void local_auth_bypass_tests_on_paint()
{
  if (!is_local_auth_bypass_tests_enabled())
  {
    last_connect_string[0] = '\0';
    return;
  }

  if (!is_valid_connect_string(last_connect_string))
  {
    return;
  }

  set_debug_ticket_address(last_connect_string);
}
