#ifndef DISCORD_DATA_H
#define DISCORD_DATA_H

#include <string.h>

#ifdef WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

typedef struct {
  const char *internal_name;
  const char *display_name;
} lookupTable_t;

// Expose lookup tables to the rest of the application
extern const lookupTable_t CampaignMaps[];
extern const lookupTable_t ModNames[];

// Clean function declarations
const char *GetFriendlyWeaponName(int weap);
const char *GetFriendlySkillName(int val);
const char *GetModDisplayName(const char *fs_game);
const char *GetFriendlyMapName(const char *mapname);

#endif // DISCORD_DATA_H