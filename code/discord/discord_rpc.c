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

// Dynamic state-tracking variables to prevent redundant parsing/spamming
static int discord_last_health = -1;
static int discord_last_wave = -1;
static int discord_last_kills = -1;
static int discord_last_score = -1;
static int discord_last_weapon = -1;

// Persistent stream framing buffers
static char incoming_buf[4096];
static int incoming_len = 0;

#include "discord_data.h"

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
    if (in[i] == '^' && in[i + 1] != '\0') {
      char c = in[i + 1];
      if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z')) {
        i++;
        continue;
      }
    }
    out[j++] = in[i];
  }
  out[j] = '\0';
}

void Discord_Shutdown(void) {
  if (discord_fd >= 0) {
    close(discord_fd);
    discord_fd = -1;
    discord_ready = 0;
    incoming_len = 0;
    discord_log("Discord: Connection closed.\n");
  }
}

static int Discord_WriteAll(const void *buf, size_t len) {
  if (discord_fd < 0)
    return 0;

  size_t total_sent = 0;
  const char *ptr = (const char *)buf;

  while (total_sent < len) {
    ssize_t sent =
        send(discord_fd, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
    if (sent < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      discord_log("Discord: Write error occurred (%d).\n", errno);
      Discord_Shutdown();
      return 0;
    } else if (sent == 0) {
      discord_log("Discord: Connection lost during send operation.\n");
      Discord_Shutdown();
      return 0;
    }
    total_sent += sent;
  }
  return 1;
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
        incoming_len = 0;

        const char *handshake_json =
            "{\"v\":1,\"client_id\":\"1500118711774744737\"}";
        uint32_t header[2];
        header[0] = 0;
        header[1] = (uint32_t)strlen(handshake_json);

        if (!Discord_WriteAll(header, sizeof(header)) ||
            !Discord_WriteAll(handshake_json, header[1])) {
          return 0;
        }
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

  ssize_t r = recv(discord_fd, incoming_buf + incoming_len,
                   sizeof(incoming_buf) - incoming_len - 1, MSG_DONTWAIT);
  if (r < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return;
    discord_log("Discord: Read error %d.\n", errno);
    Discord_Shutdown();
    return;
  } else if (r == 0) {
    discord_log("Discord: EOF received (Discord closed).\n");
    Discord_Shutdown();
    return;
  }

  incoming_len += r;
  incoming_buf[incoming_len] = '\0';

  while (incoming_len >= 8) {
    uint32_t op, payload_len;
    memcpy(&op, incoming_buf, 4);
    memcpy(&payload_len, incoming_buf + 4, 4);

    if (incoming_len < 8 + payload_len) {
      break;
    }

    char boundary_char = incoming_buf[8 + payload_len];
    incoming_buf[8 + payload_len] = '\0';

    char *payload = incoming_buf + 8;
    discord_log("Discord reply (op %u, len %u): %s\n", op, payload_len,
                payload);

    if (!discord_ready && strstr(payload, "\"evt\":\"READY\"")) {
      discord_ready = 1;
      discord_needs_update = 1;
      discord_log("Discord: Ready event received.\n");
    }

    incoming_buf[8 + payload_len] = boundary_char;

    int packet_total_size = 8 + payload_len;
    memmove(incoming_buf, incoming_buf + packet_total_size,
            incoming_len - packet_total_size);
    incoming_len -= packet_total_size;
  }
}

static void Discord_Update(void) {
  if (!discord_needs_update)
    return;

  time_t cur_time = time(NULL);
  if (cur_time < next_allowed_update_time) {
    return;
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
  next_allowed_update_time = cur_time + 4;

  char details[128] = "";
  char state[128] = "";
  char timestamp_json[64] = "";

  // DYNAMIC MAIN MENU CHECK
  if (strcmp(discord_display, "#status_mainmenu") == 0 || !discord_display[0]) {
    const char *mod_name = GetModDisplayName(discord_fs_game);
    snprintf(details, sizeof(details), "Main Menu");
    if (mod_name) {
      snprintf(state, sizeof(state), "%s", mod_name);
    } else {
      snprintf(state, sizeof(state), "Return to Castle Wolfenstein");
    }
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

    // --- Dynamic Multi-Part State Builder ---
    int skill_int = atoi(discord_skill);
    const char *skill_name = GetFriendlySkillName(skill_int);
    qboolean is_survival =
        (Cvar_VariableIntegerValue("g_gametype") == 3); // GT_SURVIVAL = 3

    char health_str[96] = "";
    if (clc.state == CA_ACTIVE && cl.snap.valid) {
      int health = cl.snap.ps.stats[STAT_HEALTH];
      if (health < 0)
        health = 0;
      int max_health = cl.snap.ps.stats[STAT_MAX_HEALTH];
      if (max_health <= 0)
        max_health = 100;
      snprintf(health_str, sizeof(health_str), "❤️ %d/%d", health, max_health);
    }

    char wave_str[64] = "";
    if (is_survival && clc.state == CA_ACTIVE && cl.snap.valid) {
      int wave = cl.snap.ps.persistant[PERS_WAVES];
      int kills = cl.snap.ps.persistant[PERS_KILLS];
      int points = cl.snap.ps.persistant[PERS_SCORE];
      snprintf(wave_str, sizeof(wave_str), "Wave %d | ☠️ %d | ✪ %d", wave, kills,
               points);
    }

    char stats_str[96] = "";
    if (discord_cs_missionstats[0] &&
        strncmp(discord_cs_missionstats, "s=", 2) == 0) {
      char stats_buf[128];
      Q_strncpyz(stats_buf, discord_cs_missionstats + 2, sizeof(stats_buf));

      int sec = 0, sec_total = 0, treas = 0, treas_total = 0;
      sscanf(stats_buf, ",%*d,%*d,%*d,%*d,%*d,%d,%d,%d,%d", &sec, &sec_total,
             &treas, &treas_total);

      if (sec_total > 0 && treas_total > 0) {
        snprintf(stats_str, sizeof(stats_str),
                 "🔍 Secrets %d/%d | 💰 Treasure %d/%d", sec, sec_total, treas,
                 treas_total);
      } else if (sec_total > 0) {
        snprintf(stats_str, sizeof(stats_str), "🔍 Secrets %d/%d", sec,
                 sec_total);
      } else if (treas_total > 0) {
        snprintf(stats_str, sizeof(stats_str), "💰 Treasure %d/%d", treas,
                 treas_total);
      }
    }

    char weap_str[64] = "";
    if (clc.state == CA_ACTIVE && cl.snap.valid) {
      int active_weap = cl.snap.ps.weapon;
      const char *weap_name = GetFriendlyWeaponName(active_weap);
      if (weap_name) {
        snprintf(weap_str, sizeof(weap_str), "%s", weap_name);
      }
    }

    // Build components array safely
    char parts[5][96];
    int num_parts = 0;

    // Part 1: Difficulty / Mode Name
    if (skill_name && skill_name[0]) {
      if (clc.state == CA_ACTIVE && cl.snap.valid) {
        Q_strncpyz(parts[num_parts++], skill_name, sizeof(parts[0]));
      } else {
        snprintf(parts[num_parts++], sizeof(parts[0]), "Difficulty: %s",
                 skill_name);
      }
    } else {
      snprintf(parts[num_parts++], sizeof(parts[0]), "Difficulty: %s",
               GetFriendlySkillName(skill_int));
    }

    // Part 2: Survival Wave Counter
    if (wave_str[0]) {
      Q_strncpyz(parts[num_parts++], wave_str, sizeof(parts[0]));
    }

    // Part 3: Live Player Health Info
    if (health_str[0]) {
      Q_strncpyz(parts[num_parts++], health_str, sizeof(parts[0]));
    }

    // Part 4: Active Weapon Info
    if (weap_str[0]) {
      Q_strncpyz(parts[num_parts++], weap_str, sizeof(parts[0]));
    }

    // Part 5: Level Mission Statistics
    if (stats_str[0]) {
      Q_strncpyz(parts[num_parts++], stats_str, sizeof(parts[0]));
    }

    // Assembly loop
    state[0] = '\0';
    for (int i = 0; i < num_parts; i++) {
      if (i > 0) {
        Q_strcat(state, sizeof(state), " | ");
      }
      Q_strcat(state, sizeof(state), parts[i]);
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

  Discord_WriteAll(header, sizeof(header));
  Discord_WriteAll(payload, header[1]);
}

void Discord_Init(void) {}

void Discord_RunFrame(void) {
  qboolean changed = qfalse;

  // GLOBAL MOD CHECKING: Always monitor active mod directory, even at main menu
  const char *fsgame_val = Cvar_VariableString("fs_game");
  if (strcmp(discord_fs_game, fsgame_val) != 0) {
    Q_strncpyz(discord_fs_game, fsgame_val, sizeof(discord_fs_game));
    changed = qtrue;
  }

  if (clc.state >= CA_CONNECTED) {
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

    const char *skill_val = Cvar_VariableString("g_gameskill");
    if (strcmp(discord_skill, skill_val) != 0) {
      Q_strncpyz(discord_skill, skill_val, sizeof(discord_skill));
      changed = qtrue;
    }

    // CRITICAL IMPROVEMENT: Monitor real-time player data frame changes
    if (clc.state == CA_ACTIVE && cl.snap.valid) {
      int cur_health = cl.snap.ps.stats[STAT_HEALTH];
      if (cur_health < 0)
        cur_health = 0;
      if (cur_health != discord_last_health) {
        discord_last_health = cur_health;
        changed = qtrue;
      }

      int cur_wave = cl.snap.ps.persistant[PERS_WAVES];
      if (cur_wave != discord_last_wave) {
        discord_last_wave = cur_wave;
        changed = qtrue;
      }

      int cur_kills = cl.snap.ps.persistant[PERS_KILLS];
      if (cur_kills != discord_last_kills) {
        discord_last_kills = cur_kills;
        changed = qtrue;
      }

      int cur_score = cl.snap.ps.persistant[PERS_SCORE];
      if (cur_score != discord_last_score) {
        discord_last_score = cur_score;
        changed = qtrue;
      }

      int cur_weap = cl.snap.ps.weapon;
      if (cur_weap != discord_last_weapon) {
        discord_last_weapon = cur_weap;
        changed = qtrue;
      }
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
      discord_last_health = -1;
      discord_last_wave = -1;
      discord_last_kills = -1;
      discord_last_score = -1;
      discord_last_weapon = -1;
      start_time = 0;
      changed = qtrue;
    }

    if (changed) {
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