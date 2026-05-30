/*
/^-----^\   data: 2026-05-08
V  o o  V  file: src/core/hooks/tf_gc_client_system.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/

#include "tf_gc_client_system.hpp"

#include <cstdint>

#include "core/memory/byte_patch.hpp"
#include "core/print.hpp"
#include "core/shared/sigs.hpp"
#include "features/menu/config.hpp"
#include "libsigscan/libsigscan.h"

namespace
{

constexpr int shared_object_created_event = 0;
constexpr unsigned int tf_lobby_invite_type = 2008;
constexpr int shared_object_type_vfunc_index = 2;
constexpr int lobby_invite_id_vfunc_index = 12;

#if defined(CATHOOK_TEXTMODE) && CATHOOK_TEXTMODE
constexpr bool textmode_auto_casual_join = true;
#else
constexpr bool textmode_auto_casual_join = false;
#endif

using shared_object_type_fn = unsigned int (*)(void* self);
using lobby_invite_id_fn = std::uint64_t (*)(void* self);

byte_patch accept_match_invite_guard_patch{};
byte_patch accept_match_invite_abandon_patch{};
byte_patch accept_match_invite_store_patch{};

bool apply_patch(byte_patch& patch, const char* name)
{
  if (!patch.valid())
  {
    print("[tf_gc] %s patch target missing\n", name);
    return false;
  }

  if (!patch.apply())
  {
    print("[tf_gc] failed to apply %s patch\n", name);
    return false;
  }

  return true;
}

bool auto_casual_join_enabled()
{
  return textmode_auto_casual_join || config.misc.automation.auto_casual_join;
}

unsigned int get_shared_object_type(void* shared_object)
{
  if (shared_object == nullptr)
  {
    return 0;
  }

  auto** vtable = *reinterpret_cast<void***>(shared_object);
  if (vtable == nullptr || vtable[shared_object_type_vfunc_index] == nullptr)
  {
    return 0;
  }

  auto get_type = reinterpret_cast<shared_object_type_fn>(vtable[shared_object_type_vfunc_index]);
  return get_type(shared_object);
}

std::uint64_t get_lobby_invite_id(void* shared_object)
{
  if (shared_object == nullptr)
  {
    return 0;
  }

  auto** vtable = *reinterpret_cast<void***>(shared_object);
  if (vtable == nullptr || vtable[lobby_invite_id_vfunc_index] == nullptr)
  {
    return 0;
  }

  auto get_lobby_id = reinterpret_cast<lobby_invite_id_fn>(vtable[lobby_invite_id_vfunc_index]);
  return get_lobby_id(shared_object);
}

void accept_lobby_invite(void* self, void* shared_object)
{
  if (tf_gc_client_system_request_accept_match_invite == nullptr)
  {
    return;
  }

  const std::uint64_t lobby_id = get_lobby_invite_id(shared_object);
  if (lobby_id == 0)
  {
    return;
  }

  tf_gc_client_system_request_accept_match_invite(self, lobby_id);
}

void call_original_so_event(void* self, void* shared_object, const int event_type)
{
  if (tf_gc_client_system_so_event_original == nullptr)
  {
    return;
  }

  tf_gc_client_system_so_event_original(self, shared_object, event_type);
}

} // namespace

void initialize_tf_gc_client_system_patches()
{
  std::uint8_t* accept_match_invite = static_cast<std::uint8_t*>(
    sigscan_module("client.so", sigs::tf_gc_client_system_request_accept_match_invite));
  std::uint8_t* abandon_branch = static_cast<std::uint8_t*>(
    sigscan_module("client.so", sigs::tf_gc_client_system_accept_match_invite_abandon_branch));
  std::uint8_t* store_accepted_lobby = static_cast<std::uint8_t*>(
    sigscan_module("client.so", sigs::tf_gc_client_system_accept_match_invite_store));

  if (accept_match_invite != nullptr)
  {
    accept_match_invite_guard_patch = byte_patch(accept_match_invite + 8, { 0xeb });
  }

  if (abandon_branch != nullptr)
  {
    accept_match_invite_abandon_patch = byte_patch(abandon_branch + 6, { 0x75, 0xc2 });
  }

  if (store_accepted_lobby != nullptr)
  {
    accept_match_invite_store_patch = byte_patch(store_accepted_lobby, {
      0x90, 0x90, 0x90, 0x90,
      0x90, 0x90, 0x90, 0x90
    });
  }

  apply_patch(accept_match_invite_guard_patch, "accept_match_invite_guard");
  apply_patch(accept_match_invite_abandon_patch, "accept_match_invite_abandon");
  apply_patch(accept_match_invite_store_patch, "accept_match_invite_store");
}

void tf_gc_client_system_so_event_hook(void* self, void* shared_object, const int event_type)
{
  const unsigned int object_type = get_shared_object_type(shared_object);
  const bool should_auto_join =
    auto_casual_join_enabled() &&
    event_type == shared_object_created_event;

  if (should_auto_join && object_type == tf_lobby_invite_type)
  {
    accept_lobby_invite(self, shared_object);
  }

  call_original_so_event(self, shared_object, event_type);
}
