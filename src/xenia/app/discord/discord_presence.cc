/**
******************************************************************************
* Xenia : Xbox 360 Emulator Research Project                                 *
******************************************************************************
* Copyright 2020 Ben Vanik. All rights reserved.                             *
* Released under the BSD license - see LICENSE in the root for more details. *
******************************************************************************
*/

#include "discord_presence.h"

// Discord Rich Presence uses IPC to a desktop client; not applicable on
// Android. Provide no-op stubs so the rest of the codebase compiles cleanly.
#if !XE_PLATFORM_ANDROID
#include "third_party/discord-rpc/include/discord_rpc.h"
#include "xenia/base/string.h"
#endif  // !XE_PLATFORM_ANDROID

namespace xe {
namespace discord {

#if XE_PLATFORM_ANDROID

void DiscordPresence::Initialize() {}
void DiscordPresence::NotPlaying() {}
void DiscordPresence::PlayingTitle(const std::string_view) {}
void DiscordPresence::Shutdown() {}

#else  // XE_PLATFORM_ANDROID

void HandleDiscordReady(const DiscordUser* request) {}
void HandleDiscordError(int errorCode, const char* message) {}
void HandleDiscordJoinGame(const char* joinSecret) {}
void HandleDiscordJoinRequest(const DiscordUser* request) {}
void HandleDiscordSpectateGame(const char* spectateSecret) {}

void DiscordPresence::Initialize() {
  DiscordEventHandlers handlers = {};
  handlers.ready = &HandleDiscordReady;
  handlers.errored = &HandleDiscordError;
  handlers.joinGame = &HandleDiscordJoinGame;
  handlers.joinRequest = &HandleDiscordJoinRequest;
  handlers.spectateGame = &HandleDiscordSpectateGame;
  Discord_Initialize("606840046649081857", &handlers, 0, "");
}

void DiscordPresence::NotPlaying() {
  DiscordRichPresence discordPresence = {};
  discordPresence.state = "Idle";
  discordPresence.details = "Standby";
  discordPresence.largeImageKey = "app";
  discordPresence.instance = 1;
  Discord_UpdatePresence(&discordPresence);
}

void DiscordPresence::PlayingTitle(const std::string_view game_title) {
  auto details = std::string(game_title);
  DiscordRichPresence discordPresence = {};
  discordPresence.state = "In Game";
  discordPresence.details = details.c_str();
  discordPresence.largeImageKey = "app";
  discordPresence.instance = 1;
  Discord_UpdatePresence(&discordPresence);
}

void DiscordPresence::Shutdown() { Discord_Shutdown(); }

#endif  // !XE_PLATFORM_ANDROID

}  // namespace discord
}  // namespace xe
