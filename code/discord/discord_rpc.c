#include "discord_rpc.h"
#include <stdint.h>

#ifndef WIN32
#include "../client/client.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int discord_fd = -1;
static char discord_mapname[64] = "";
static char discord_skill[64] = "";
static char discord_display[64] = "";
static char discord_cs_message[128] = ""; // CS_MESSAGE (worldspawn map title)
static char discord_cs_missionstats[128] =
    "";                               // CS_MISSIONSTATS (live level stats)
static char discord_fs_game[64] = ""; // fs_game (active mod folder name)
static int discord_needs_update = 0;
static int discord_ready = 0;
static time_t next_discord_connect_time = 0;
static time_t next_allowed_update_time = 0; // Anti-spam throttling timer
static time_t start_time = 0;

static void discord_log(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  FILE *f = fopen("discord_rpc.log", "a");
  if (f) {
    vfprintf(f, fmt, args);
    fclose(f);
  }
  va_end(args);
}

// Maps internal map filenames to their display titles (matching Steam
// localization).
static const char *GetFriendlyMapName(const char *mapname) {
  if (!mapname || !mapname[0])
    return "Unknown Map";

  // --- Main Campaign ---
  if (strcasecmp(mapname, "intro") == 0)
    return "Prologue";
  if (strcasecmp(mapname, "escape1") == 0)
    return "Ominous Rumors";
  if (strcasecmp(mapname, "escape2") == 0)
    return "The Escape";
  if (strcasecmp(mapname, "tram") == 0)
    return "Tram Ride";
  if (strcasecmp(mapname, "village1") == 0)
    return "Wulfburg";
  if (strcasecmp(mapname, "village2") == 0)
    return "Ruined Village";
  if (strcasecmp(mapname, "crypt1") == 0)
    return "Catacombs";
  if (strcasecmp(mapname, "crypt2") == 0)
    return "Crypt";
  if (strcasecmp(mapname, "church") == 0)
    return "The Defiled Church";
  if (strcasecmp(mapname, "boss1") == 0)
    return "The Defiled Church (Boss)";
  if (strcasecmp(mapname, "forest") == 0)
    return "Forest Compound";
  if (strcasecmp(mapname, "rocket") == 0)
    return "Rocket Base";
  if (strcasecmp(mapname, "baseout") == 0)
    return "Radar Installation";
  if (strcasecmp(mapname, "assault") == 0)
    return "Air Base Assault";
  if (strcasecmp(mapname, "sfm") == 0)
    return "Kugelstadt";
  if (strcasecmp(mapname, "factory") == 0)
    return "The Ruined Factory";
  if (strcasecmp(mapname, "trainyard") == 0)
    return "The Trainyards";
  if (strcasecmp(mapname, "swerve") == 0)
    return "Secret Weapons Facility";
  if (strcasecmp(mapname, "hideout") == 0)
    return "Ice Station Cobra";
  if (strcasecmp(mapname, "chateau") == 0)
    return "Chateau";
  if (strcasecmp(mapname, "dark") == 0)
    return "Dark Descent";
  if (strcasecmp(mapname, "dig") == 0)
    return "The Dig";
  if (strcasecmp(mapname, "castle") == 0)
    return "Return to Castle Wolfenstein";
  if (strcasecmp(mapname, "end") == 0)
    return "Heinrich";
  if (strcasecmp(mapname, "boss2") == 0)
    return "Heinrich (Boss)";
  if (strcasecmp(mapname, "dam") == 0)
    return "X-Labs";
  if (strcasecmp(mapname, "cobb") == 0)
    return "Chateau Cobb";
  if (strcasecmp(mapname, "keep") == 0)
    return "Castle Keep";
  if (strcasecmp(mapname, "xlabs") == 0)
    return "X-Labs";
  if (strcasecmp(mapname, "fendrich") == 0)
    return "Fendrich's Office";
  if (strcasecmp(mapname, "norway") == 0)
    return "Norway";

  // --- Malta Campaign ---
  if (strcasecmp(mapname, "malta_0") == 0)
    return "Malta - Prologue";
  if (strcasecmp(mapname, "malta_1") == 0)
    return "Malta - The Citadel";
  if (strcasecmp(mapname, "malta_2") == 0)
    return "Malta - Mdina";
  if (strcasecmp(mapname, "malta_3") == 0)
    return "Malta - Catacombs";
  if (strcasecmp(mapname, "malta_4") == 0)
    return "Malta - The Harbor";
  if (strcasecmp(mapname, "malta_5") == 0)
    return "Malta - The Fortress";
  if (strcasecmp(mapname, "malta_menu") == 0)
    return "Malta - Main Menu";

  // --- Survival Maps ---
  if (strcasecmp(mapname, "sv_barn") == 0)
    return "Survival - Barn";
  if (strcasecmp(mapname, "sv_boss1") == 0)
    return "Survival - Boss";
  if (strcasecmp(mapname, "sv_castle") == 0)
    return "Survival - Castle";
  if (strcasecmp(mapname, "sv_crypt1") == 0)
    return "Survival - Crypt";
  if (strcasecmp(mapname, "sv_dig") == 0)
    return "Survival - The Dig";
  if (strcasecmp(mapname, "sv_escape2") == 0)
    return "Survival - Escape";
  if (strcasecmp(mapname, "sv_karsiah") == 0)
    return "Survival - Karsiah";
  if (strcasecmp(mapname, "sv_kugelstadt") == 0)
    return "Survival - Kugelstadt";
  if (strcasecmp(mapname, "sv_norway") == 0)
    return "Survival - Norway";
  if (strcasecmp(mapname, "sv_river_outpost") == 0)
    return "Survival - River Outpost";
  if (strcasecmp(mapname, "sv_safe") == 0)
    return "Survival - Safe House";

  // --- Enemy Territory / EE Expansion ---
  if (strcasecmp(mapname, "ee1") == 0)
    return "Cursed Sands (Ras el-Hadid)";
  if (strcasecmp(mapname, "ee2") == 0)
    return "Cursed Sands (The Excavation)";
  if (strcasecmp(mapname, "ee3") == 0)
    return "Cursed Sands (The Temple)";
  if (strcasecmp(mapname, "ee4") == 0)
    return "Cursed Sands (The Fortress)";
  if (strcasecmp(mapname, "ee5") == 0)
    return "Cursed Sands (The Pyramid)";

  if (strncasecmp(mapname, "cutscene", 8) == 0)
    return "Cutscene";
  if (strncasecmp(mapname, "day_", 4) == 0)
    return "Prologue";

  return NULL;
}

// Maps fs_game folder names to human-readable mod names.
static const char *GetModDisplayName(const char *fs_game) {
  if (!fs_game || !fs_game[0] || strcasecmp(fs_game, "main") == 0)
    return NULL;

  if (strcasecmp(fs_game, "EE") == 0)
    return "Cursed Sands";
  if (strcasecmp(fs_game, "karsiah") == 0)
    return "Karsiah Raid";

  // Workshop IDs
  if (strcmp(fs_game, "2178638114") == 0)
    return "Beach Assault";
  if (strcmp(fs_game, "2195436928") == 0)
    return "Capuzzo";
  if (strcmp(fs_game, "2223087973") == 0)
    return "Vendetta Dilogy";
  if (strcmp(fs_game, "2229917682") == 0)
    return "Project 51";
  if (strcmp(fs_game, "2230658534") == 0)
    return "Pharaoh's Curse";
  if (strcmp(fs_game, "2230721200") == 0)
    return "Flying Saucers";
  if (strcmp(fs_game, "2231522108") == 0)
    return "Age of Horror";
  if (strcmp(fs_game, "2232360724") == 0)
    return "Stalingrad";
  if (strcmp(fs_game, "2232931912") == 0)
    return "Time Gate";
  if (strcmp(fs_game, "2234651760") == 0)
    return "Trondheim Trilogy";
  if (strcmp(fs_game, "2239016506") == 0)
    return "Devil's Manor 2: Edge of Darkness";
  if (strcmp(fs_game, "2253494213") == 0)
    return "The Dark Army: Uprising Remastered";
  if (strcmp(fs_game, "2600685791") == 0)
    return "Wolfenstein: ET Single-Player";
  if (strcmp(fs_game, "2872954732") == 0)
    return "Into the Eagle's Nest";
  if (strcmp(fs_game, "3116640063") == 0)
    return "Vendetta 3";
  if (strcmp(fs_game, "3289129216") == 0)
    return "RealRTCW Remastered Textures";
  if (strcmp(fs_game, "3693642344") == 0)
    return "Enhanced Weapons: Remastered";

  return fs_game;
}

static const char *GetFriendlySkillName(int val) {
  switch (val) {
  case 0:
    return "Can I play, Daddy?";
  case 1:
    return "Don't hurt me.";
  case 2:
    return "Bring 'em on!";
  case 3:
    return "I am Death incarnate!";
  case 4:
    return "Realism";
  case 5:
    return "Survival";
  default:
    return "Unknown";
  }
}

static void ExtractMapBaseName(const char *in, char *out, int maxlen) {
  const char *start = strrchr(in, '/');
  start = start ? start + 1 : in;
  const char *end = strrchr(start, '.');
  int len = end ? (int)(end - start) : (int)strlen(start);
  if (len >= maxlen)
    len = maxlen - 1;
  memcpy(out, start, len);
  out[len] = '\0';
}

static void StripColorCodes(const char *in, char *out, int maxlen) {
  int j = 0;
  for (int i = 0; in[i] && j < maxlen - 1; i++) {
    if (in[i] == '^' && in[i + 1] >= '0' && in[i + 1] <= '9') {
      i++;
    } else {
      out[j++] = in[i];
    }
  }
  out[j] = '\0';
}

void Discord_Shutdown(void) {
  if (discord_fd >= 0) {
    close(discord_fd);
    discord_fd = -1;
    discord_ready = 0;
    discord_log("Discord: Connection closed.\n");
  }
}

static int Discord_Connect(void) {
  if (discord_fd >= 0)
    return 1;

  discord_log("Discord: Connecting...\n");
  const char *dirs[] = {getenv("XDG_RUNTIME_DIR"), getenv("TMPDIR"),
                        getenv("TMP"), getenv("TEMP"), "/tmp"};
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  for (int d = 0; d < 5; d++) {
    if (!dirs[d] || !dirs[d][0])
      continue;
    discord_log("Discord: Searching in %s...\n", dirs[d]);
    for (int i = 0; i < 10; i++) {
      snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/discord-ipc-%d",
               dirs[d], i);
      int fd = socket(AF_UNIX, SOCK_STREAM, 0);
      if (fd < 0)
        continue;

      if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        discord_fd = fd;
        discord_ready = 0;

        const char *handshake_json =
            "{\"v\":1,\"client_id\":\"1500118711774744737\"}";
        uint32_t header[2];
        header[0] = 0;
        header[1] = (uint32_t)strlen(handshake_json);

        write(discord_fd, header, sizeof(header));
        write(discord_fd, handshake_json, header[1]);
        discord_log("Discord: Connected successfully to %s\n", addr.sun_path);
        return 1;
      }
      close(fd);
    }
  }
  discord_log("Discord: Failed to find any active Discord IPC socket.\n");
  return 0;
}

static void Discord_Pump(void) {
  if (discord_fd < 0)
    return;

  char dummy[2048];
  while (1) {
    ssize_t r = recv(discord_fd, dummy, sizeof(dummy) - 1, MSG_DONTWAIT);
    if (r < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      discord_log("Discord: Read error %d.\n", errno);
      Discord_Shutdown();
      break;
    } else if (r == 0) {
      discord_log("Discord: EOF received (Discord closed).\n");
      Discord_Shutdown();
      break;
    } else {
      dummy[r] = '\0';
      if (r > 8) {
        // Safe unaligned binary parsing via memcpy
        uint32_t op, len;
        memcpy(&op, dummy, 4);
        memcpy(&len, dummy + 4, 4);

        discord_log("Discord reply (op %u, len %u): %s\n", op, len, dummy + 8);
        if (!discord_ready && strstr(dummy + 8, "\"evt\":\"READY\"")) {
          discord_ready = 1;
          discord_needs_update = 1;
          discord_log("Discord: Ready event received.\n");
        }
      } else {
        discord_log("Discord reply short (len %d)\n", (int)r);
      }
    }
  }
}

static void Discord_Update(void) {
  if (!discord_needs_update)
    return;

  // Enforce API Rate Limiting Cooldown (max 1 status call per 4 seconds)
  time_t cur_time = time(NULL);
  if (cur_time < next_allowed_update_time) {
    return; // Keep discord_needs_update flagged to try again on next framework
            // loops
  }

  if (!Discord_Connect()) {
    discord_log("Discord: Update deferred (not connected).\n");
    return;
  }
  if (!discord_ready) {
    discord_log("Discord: Update deferred (not ready yet).\n");
    return;
  }

  discord_needs_update = 0;
  next_allowed_update_time = cur_time + 4; // Reset internal tracking window

  char details[128] = "";
  char state[128] = "";
  char timestamp_json[64] = "";

  if (strcmp(discord_display, "#status_mainmenu") == 0 || !discord_display[0]) {
    snprintf(details, sizeof(details), "Main Menu");
    snprintf(state, sizeof(state), "Return to Castle Wolfenstein");
  } else {
    char basename[64];
    ExtractMapBaseName(discord_mapname, basename, sizeof(basename));
    const char *dict_name = GetFriendlyMapName(basename);

    if (dict_name != NULL) {
      snprintf(details, sizeof(details), "Playing %s", dict_name);
    } else if (discord_cs_message[0]) {
      char clean[128];
      StripColorCodes(discord_cs_message, clean, sizeof(clean));
      snprintf(details, sizeof(details), "Playing %s", clean);
    } else {
      const char *mod_name = GetModDisplayName(discord_fs_game);
      if (mod_name) {
        snprintf(details, sizeof(details), "%s: %s", mod_name, basename);
      } else {
        snprintf(details, sizeof(details), "Playing %s", basename);
      }
    }

    int skill_int = atoi(discord_skill);

    if (discord_cs_missionstats[0] &&
        strncmp(discord_cs_missionstats, "s=", 2) == 0) {
      char stats_buf[128];
      Q_strncpyz(stats_buf, discord_cs_missionstats + 2, sizeof(stats_buf));

      int sec = 0, sec_total = 0, treas = 0, treas_total = 0;
      // Scans out the raw game mechanics stats variables directly
      sscanf(stats_buf, ",%*d,%*d,%*d,%*d,%*d,%d,%d,%d,%d", &sec, &sec_total,
             &treas, &treas_total);

      // Build live string omitting text timer completely
      char stats_str[96] = "";
      if (sec_total > 0 && treas_total > 0) {
        snprintf(stats_str, sizeof(stats_str), "Secrets %d/%d | Treasure %d/%d",
                 sec, sec_total, treas, treas_total);
      } else if (sec_total > 0) {
        snprintf(stats_str, sizeof(stats_str), "Secrets %d/%d", sec, sec_total);
      } else if (treas_total > 0) {
        snprintf(stats_str, sizeof(stats_str), "Treasure %d/%d", treas,
                 treas_total);
      }

      if (stats_str[0]) {
        snprintf(state, sizeof(state), "%s | %s",
                 GetFriendlySkillName(skill_int), stats_str);
      } else {
        snprintf(state, sizeof(state), "%s", GetFriendlySkillName(skill_int));
      }
    } else {
      snprintf(state, sizeof(state), "Difficulty: %s",
               GetFriendlySkillName(skill_int));
    }
  }

  if (start_time != 0) {
    snprintf(timestamp_json, sizeof(timestamp_json),
             "\"timestamps\":{\"start\":%lld},", (long long)start_time);
  }

  char payload[1024];
  snprintf(payload, sizeof(payload),
           "{"
           "\"cmd\":\"SET_ACTIVITY\","
           "\"args\":{"
           "\"pid\":%d,"
           "\"activity\":{"
           "\"details\":\"%s\","
           "\"state\":\"%s\","
           "%s"
           "\"assets\":{"
           "\"large_image\":\"realrtcw\","
           "\"large_text\":\"RealRTCW\""
           "}"
           "}"
           "},"
           "\"nonce\":\"1\""
           "}",
           (int)getpid(), details, state, timestamp_json);

  discord_log("Discord: Sending update (details: '%s', state: '%s')\n", details,
              state);

  uint32_t header[2];
  header[0] = 1;
  header[1] = (uint32_t)strlen(payload);

  write(discord_fd, header, sizeof(header));
  write(discord_fd, payload, header[1]);
}

void Discord_Init(void) {}

void Discord_RunFrame(void) {
  if (clc.state >= CA_CONNECTED) {
    qboolean changed = qfalse;

    if (strcmp(discord_display, "#status_map") != 0) {
      Q_strncpyz(discord_display, "#status_map", sizeof(discord_display));
      changed = qtrue;
    }
    if (strcmp(discord_mapname, cl.mapname) != 0) {
      Q_strncpyz(discord_mapname, cl.mapname, sizeof(discord_mapname));
      changed = qtrue;
      start_time = time(NULL);
    }

    const char *cs_msg = "";
    if (cl.gameState.stringOffsets[CS_MESSAGE]) {
      cs_msg = cl.gameState.stringData + cl.gameState.stringOffsets[CS_MESSAGE];
    }
    if (strcmp(discord_cs_message, cs_msg) != 0) {
      Q_strncpyz(discord_cs_message, cs_msg, sizeof(discord_cs_message));
      changed = qtrue;
    }

    const char *cs_stats = "";
    if (cl.gameState.stringOffsets[CS_MISSIONSTATS]) {
      cs_stats =
          cl.gameState.stringData + cl.gameState.stringOffsets[CS_MISSIONSTATS];
    }
    if (strcmp(discord_cs_missionstats, cs_stats) != 0) {
      Q_strncpyz(discord_cs_missionstats, cs_stats,
                 sizeof(discord_cs_missionstats));
      changed = qtrue;
    }

    const char *fsgame_val = Cvar_VariableString("fs_game");
    if (strcmp(discord_fs_game, fsgame_val) != 0) {
      Q_strncpyz(discord_fs_game, fsgame_val, sizeof(discord_fs_game));
      changed = qtrue;
    }

    const char *skill_val = Cvar_VariableString("g_gameskill");
    if (strcmp(discord_skill, skill_val) != 0) {
      Q_strncpyz(discord_skill, skill_val, sizeof(discord_skill));
      changed = qtrue;
    }

    if (changed) {
      discord_needs_update = 1;
    }
  } else {
    if (strcmp(discord_display, "#status_mainmenu") != 0) {
      Q_strncpyz(discord_display, "#status_mainmenu", sizeof(discord_display));
      discord_mapname[0] = '\0';
      discord_skill[0] = '\0';
      discord_cs_message[0] = '\0';
      discord_cs_missionstats[0] = '\0';
      start_time = 0;
      discord_needs_update = 1;
    }
  }

  Discord_Pump();

  time_t cur = time(NULL);
  if (discord_fd < 0 && cur > next_discord_connect_time) {
    next_discord_connect_time = cur + 5;
    discord_needs_update = 1;
  }

  if (discord_needs_update) {
    Discord_Update();
  }
}

#else
void Discord_Init(void) {}
void Discord_RunFrame(void) {}
void Discord_Shutdown(void) {}
#endif