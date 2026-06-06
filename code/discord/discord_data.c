#include "discord_data.h"
#include <stddef.h>

// Unified map dictionary
const lookupTable_t CampaignMaps[] = {
    // --- Main Campaign ---
    {"intro", "Prologue"},
    {"escape1", "Escape!"},
    {"escape2", "Castle Keep"},
    {"tram", "Tram Ride"},
    {"village1", "Village"},
    {"crypt1", "Catacombs"},
    {"crypt2", "Crypt"},
    {"church", "The Defiled Church"},
    {"boss1", "Tomb"},
    {"forest", "Forest Compound"},
    {"rocket", "Rocket Base"},
    {"baseout", "Radar Installation"},
    {"assault", "Air Base Assault"},
    {"sfm", "Kugelstadt"},
    {"smf", "Kugelstadt"},
    {"factory", "The Bombed Factory"},
    {"trainyard", "The Trainyards"},
    {"trainyards", "The Trainyards"},
    {"swerve", "Secret Weapons Facility"},
    {"swf", "Secret Weapons Facility"},
    {"norway", "Ice Station Norway"},
    {"xlabs", "X-Labs"},
    {"boss2", "Super Soldier"},
    {"dam", "Bramburg Dam"},
    {"village2", "Paderborn Village"},
    {"chateau", "Chateau Schufstaffel"},
    {"dark", "Unhallowed Ground"},
    {"dig", "The Dig"},
    {"castle", "Return to Castle Wolfenstein"},
    {"end", "Heinrich"},
    {"hideout", "Ice Station Cobra"},
    {"cobb", "Chateau Cobb"},
    {"keep", "Castle Keep"},
    {"fendrich", "Fendrich's Office"},

    // --- Malta Campaign ---
    {"malta_0", "Malta - Prologue"},
    {"malta_1", "Malta - The Citadel"},
    {"malta_2", "Malta - Mdina"},
    {"malta_3", "Malta - Catacombs"},
    {"malta_4", "Malta - The Harbor"},
    {"malta_5", "Malta - The Fortress"},
    {"malta_menu", "Malta - Main Menu"},

    // --- Survival Maps ---
    {"sv_barn", "Survival - Barn"},
    {"sv_boss1", "Survival - Defiled Church"},
    {"sv_castle", "Survival - Castle"},
    {"sv_crypt1", "Survival - Crypt"},
    {"sv_dig", "Survival - The Dig"},
    {"sv_escape2", "Survival - Escape"},
    {"sv_karsiah", "Survival - Karsiah"},
    {"sv_kugelstadt", "Survival - Kugelstadt"},
    {"sv_norway", "Survival - Norway"},
    {"sv_river_outpost", "Survival - River Outpost"},
    {"sv_safe", "Survival - Safe House"},

    // --- Cursed Sands / EE Expansion ---
    {"ee1", "Cursed Sands - Ras el-Hadid"},
    {"ee2", "Cursed Sands - The Excavation"},
    {"ee3", "Cursed Sands - The Temple"},
    {"ee4", "Cursed Sands - The Fortress"},
    {"ee5", "Cursed Sands - The Pyramid"},
    {"sv_ee1", "Cursed Sands Survival - Ras el-Hadid"},
    {"sv_ee2", "Cursed Sands Survival - The Excavation"},
    {"sv_ee3", "Cursed Sands Survival - The Temple"},
    {"sv_ee4", "Cursed Sands Survival - The Fortress"},
    {"sv_ee5", "Cursed Sands Survival - The Pyramid"},

    // --- Enemy Territory Single Player ---
    {"oasis", "ET SP - Siwa Oasis"},
    {"goldrush", "ET SP - Gold Rush"},
    {"radar", "ET SP - Würzburg Radar"},
    {"battery", "ET SP - Seawall Battery"},
    {"fueldump", "ET SP - Fuel Dump"},
    {"railgun", "ET SP - Rail Gun"},
    {"wurzburg", "ET SP - Würzburg Radar"},
    {"seawall", "ET SP - Seawall Battery"},
    {"siwa", "ET SP - Siwa Oasis"},
    {"ice", "ET SP - Ice"},
    {"warbell", "ET SP - Warbell"},
    {"sv_oasis", "ET SP Survival - Siwa Oasis"},
    {"sv_goldrush", "ET SP Survival - Gold Rush"},
    {"sv_radar", "ET SP Survival - Würzburg Radar North"},
    {"sv_radar2", "ET SP Survival - Würzburg Radar South"},
    {"sv_battery", "ET SP Survival - Seawall Battery"},
    {"sv_fueldump", "ET SP Survival - Fuel Dump"},
    {"sv_railgun", "ET SP Survival - Rail Gun"},
    {"sv_goldrush_day", "ET SP Survival - Gold Rush (Day)"},
    {"sv_radar2_day", "ET SP Survival - Würzburg Radar South (Day)"},
    {"sv_radar_day", "ET SP Survival - Würzburg Radar North (Day)"},
    {"zm_goldrush", "ET SP Zombie - Gold Rush"},
    {"zm_radar", "ET SP Zombie - Würzburg Radar North"},
    {"zm_radar2", "ET SP Zombie - Würzburg Radar South"},
    {"oasis_night", "ET SP - Siwa Oasis (Night)"},
    {"oasis2", "ET SP - Siwa Oasis 2"},
    {"oasis2_night", "ET SP - Siwa Oasis 2 (Night)"},
    {"oasis3", "ET SP - Siwa Oasis 3"},
    {"oasis3_night", "ET SP - Siwa Oasis 3 (Night)"},
    {"battery_night", "ET SP - Seawall Battery (Night)"},
    {"battery2", "ET SP - Seawall Battery 2"},
    {"battery2_night", "ET SP - Seawall Battery 2 (Night)"},
    {"battery3", "ET SP - Seawall Battery 3"},
    {"battery3_night", "ET SP - Seawall Battery 3 (Night)"},
    {"warbell_day", "ET SP - Warbell (Day)"},
    {"radar2", "ET SP - Würzburg Radar 2"},
    {"radar2_day", "ET SP - Würzburg Radar 2 (Day)"},
    {"winter_radar2", "ET SP - Winter Radar 2"},
    {"winter_radar2_day", "ET SP - Winter Radar 2 (Day)"},
    {"railgun_night", "ET SP - Rail Gun (Night)"},
    {"railgun2", "ET SP - Rail Gun 2"},
    {"railgun2_night", "ET SP - Rail Gun 2 (Night)"},

    // --- The Dark Army: Uprising ---
    {"dayprologue", "Dark Army - Prologue (Day)"},
    {"daystart", "Dark Army - Prologue (Day, Cutscene)"},
    {"dayvillage", "Dark Army - Village (Day)"},
    {"daysewers", "Dark Army - Sewers (Day)"},
    {"daycrypt", "Dark Army - Crypt (Day)"},
    {"daytrack_cut", "Dark Army - Track (Day, Cutscene)"},
    {"daytrack", "Dark Army - Track (Day)"},
    {"daybase", "Dark Army - Base (Day)"},
    {"dayepilogue", "Dark Army - Epilogue (Day)"},
    {"nightprologue", "Dark Army - Prologue (Night)"},
    {"nightstart", "Dark Army - Prologue (Night, Cutscene)"},
    {"nightvillage", "Dark Army - Village (Night)"},
    {"nightsewers", "Dark Army - Sewers (Night)"},
    {"nightcrypt", "Dark Army - Crypt (Night)"},
    {"nighttrack_cut", "Dark Army - Track (Night, Cutscene)"},
    {"nighttrack", "Dark Army - Track (Night)"},
    {"nightbase", "Dark Army - Base (Night)"},
    {"nightepilogue", "Dark Army - Epilogue (Night)"},
    {"darkprologue", "Dark Army - Prologue (Dark)"},
    {"darkstart", "Dark Army - Prologue (Dark, Cutscene)"},
    {"darkvillage", "Dark Army - Village (Dark)"},
    {"darksewers", "Dark Army - Sewers (Dark)"},
    {"darkcrypt", "Dark Army - Crypt (Dark)"},
    {"darktrack_cut", "Dark Army - Track (Dark, Cutscene)"},
    {"darktrack", "Dark Army - Track (Dark)"},
    {"darkbase", "Dark Army - Base (Dark)"},
    {"darkepilogue", "Dark Army - Epilogue (Dark)"},
    {"day_prologue", "Dark Army - Prologue (Day)"},
    {"day_start", "Dark Army - Prologue (Day, Cutscene)"},
    {"day_village", "Dark Army - Village (Day)"},
    {"day_sewers", "Dark Army - Sewers (Day)"},
    {"day_crypt", "Dark Army - Crypt (Day)"},
    {"day_track_cut", "Dark Army - Track (Day, Cutscene)"},
    {"day_track", "Dark Army - Track (Day)"},
    {"day_base", "Dark Army - Base (Day)"},
    {"day_epilogue", "Dark Army - Epilogue (Day)"},
    {"night_prologue", "Dark Army - Prologue (Night)"},
    {"night_start", "Dark Army - Prologue (Night, Cutscene)"},
    {"night_village", "Dark Army - Village (Night)"},
    {"night_sewers", "Dark Army - Sewers (Night)"},
    {"night_crypt", "Dark Army - Crypt (Night)"},
    {"night_track_cut", "Dark Army - Track (Night, Cutscene)"},
    {"night_track", "Dark Army - Track (Night)"},
    {"night_base", "Dark Army - Base (Night)"},
    {"night_epilogue", "Dark Army - Epilogue (Night)"},
    {"dark_prologue", "Dark Army - Prologue (Dark)"},
    {"dark_start", "Dark Army - Prologue (Dark, Cutscene)"},
    {"dark_village", "Dark Army - Village (Dark)"},
    {"dark_sewers", "Dark Army - Sewers (Dark)"},
    {"dark_crypt", "Dark Army - Crypt (Dark)"},
    {"dark_track_cut", "Dark Army - Track (Dark, Cutscene)"},
    {"dark_track", "Dark Army - Track (Dark)"},
    {"dark_base", "Dark Army - Base (Dark)"},
    {"dark_epilogue", "Dark Army - Epilogue (Dark)"},
    {"realm1", "Dark Army - Realm 1"},
    {"realm2", "Dark Army - Realm 2"},
    {"realm3", "Dark Army - Realm 3"},
    {"realm4", "Dark Army - Realm 4"},
    {"realm5", "Dark Army - Realm 5"},
    {"realm6", "Dark Army - Realm 6"},
    {"realm7", "Dark Army - Realm 7"},
    {"realm8", "Dark Army - Realm 8"},
    {"realm9", "Dark Army - Realm 9"},
    {"realm10", "Dark Army - Realm 10"},

    // --- Project 51 ---
    {"town00", "Project 51 - Town (Intro)"},
    {"town01", "Project 51 - Town (Part 1)"},
    {"town02", "Project 51 - Town (Part 2)"},
    {"town03", "Project 51 - Town (Part 3)"},
    {"town04", "Project 51 - Town (Part 4)"},
    {"tforest", "Project 51 - The Forest"},
    {"tforest_s", "Project 51 - The Forest (Secret)"},
    {"tstation", "Project 51 - Train Station"},
    {"shahta", "Project 51 - The Shaft"},
    {"airlab", "Project 51 - Secret Lab"},

    // --- Vendetta Dilogy & 3 ---
    {"vendetta_level3", "Vendetta - Level 3"},
    {"vendetta_level4", "Vendetta - Level 4"},
    {"vendetta_level5", "Vendetta - Level 5"},
    {"vendetta3", "Vendetta 3 - Ruins"},

    // --- Pharaoh's Curse ---
    {"nubis", "Pharaoh's Curse - Nubis Temple"},

    // --- Age of Horror ---
    {"blacksun1", "Age of Horror - Black Sun (Part 1)"},
    {"blacksun3", "Age of Horror - Black Sun (Part 3)"},
    {"blacksun5", "Age of Horror - Black Sun (Part 5)"},
    {"blacksun6", "Age of Horror - Black Sun (Part 6)"},

    // --- Trondheim Trilogy ---
    {"level1", "Trondheim - Level 1"},
    {"level2", "Trondheim - Level 2"},

    // --- Beach Assault ---
    {"sp_beach", "Beach Assault"},

    // --- Into the Eagle's Nest ---
    {"antarktida", "Into the Eagle's Nest - Antarctica"}};

// Unified modification folder lookup table
const lookupTable_t ModNames[] = {
    {"EE", "Cursed Sands"},
    {"karsiah", "Karsiah Raid"},
    {"2178638114", "Beach Assault"},
    {"2195436928", "Capuzzo"},
    {"2223087973", "Vendetta Dilogy"},
    {"2229917682", "Project 51"},
    {"2230658534", "Pharaoh's Curse"},
    {"2230721200", "Flying Saucers"},
    {"2231522108", "Age of Horror"},
    {"2232360724", "Stalingrad"},
    {"2232931912", "Time Gate"},
    {"2234651760", "Trondheim Trilogy"},
    {"2239016506", "Devil's Manor 2: Edge of Darkness"},
    {"2253494213", "The Dark Army: Uprising Remastered"},
    {"2600685791", "Wolfenstein: ET Single-Player"},
    {"2872954732", "Into the Eagle's Nest"},
    {"3116640063", "Vendetta 3"},
    {"3289129216", "RealRTCW Remastered Textures"},
    {"3693642344", "Enhanced Weapons: Remastered"}};

// Automate count computations via memory footprint metrics
static const size_t NumCampaignMaps =
    sizeof(CampaignMaps) / sizeof(CampaignMaps[0]);
static const size_t NumModNames = sizeof(ModNames) / sizeof(ModNames[0]);

const char *GetFriendlyWeaponName(int weap) {
  switch (weap) {
  case 1:
    return "Knife";
  case 2:
    return "Luger";
  case 3:
    return "Silenced Luger";
  case 4:
    return "Colt";
  case 5:
    return "TT-33";
  case 6:
    return "Revolver";
  case 7:
    return "HDM";
  case 8:
    return "Dual Colts";
  case 9:
    return "Dual TT-33";
  case 10:
    return "MP40";
  case 11:
    return "Thompson";
  case 12:
    return "Sten";
  case 13:
    return "PPSh-41";
  case 14:
    return "MP34";
  case 15:
    return "Mauser Rifle";
  case 16:
    return "Garand";
  case 17:
    return "Mosin-Nagant";
  case 18:
    return "De Lisle";
  case 19:
    return "M1 Garand";
  case 20:
    return "Gewehr 43";
  case 21:
    return "M1941 Johnson";
  case 22:
    return "StG 44";
  case 23:
    return "FG42";
  case 24:
    return "BAR";
  case 25:
    return "M97 Trench";
  case 26:
    return "Auto-5";
  case 27:
    return "M30 Drilling";
  case 28:
    return "Browning M1919";
  case 29:
    return "MG42";
  case 30:
    return "Panzerfaust";
  case 31:
    return "Flamethrower";
  case 32:
    return "Venom Gun";
  case 33:
    return "Tesla Gun";
  case 34:
    return "Grenade launcher";
  case 35:
    return "Pineapple grenade";
  case 36:
    return "Dynamite";
  case 37:
    return "Dynamite";
  case 38:
    return "Airstrike";
  case 39:
    return "Artillery";
  case 40:
    return "Poison Gas";
  case 41:
    return "Smoke canister";
  case 42:
    return "Holy Cross";
  case 43:
    return "Smoke Bomb";
  case 44:
    return "Scoped Mauser";
  case 45:
    return "Snooper Rifle";
  case 46:
    return "Scoped De Lisle";
  case 47:
    return "Scoped M1941";
  case 48:
    return "Scoped FG42";
  case 49:
    return "M7 grenade launcher";
  default:
    return "Unknown Weapon"; // Safe non-NULL fallback
  }
}

const char *GetFriendlySkillName(int val) {
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
    return "Unknown Difficulty";
  }
}

const char *GetModDisplayName(const char *fs_game) {
  if (!fs_game || !fs_game[0] || strcasecmp(fs_game, "main") == 0)
    return "Main Campaign"; // Avoid returning NULL or empty string markers

  for (size_t i = 0; i < NumModNames; i++) {
    if (strcasecmp(fs_game, ModNames[i].internal_name) == 0) {
      return ModNames[i].display_name;
    }
  }

  return fs_game;
}

const char *GetFriendlyMapName(const char *mapname) {
  if (!mapname || !mapname[0])
    return "Main Menu";

  for (size_t i = 0; i < NumCampaignMaps; i++) {
    if (strcasecmp(mapname, CampaignMaps[i].internal_name) == 0) {
      return CampaignMaps[i].display_name;
    }
  }

  if (strncasecmp(mapname, "cutscene", 8) == 0)
    return "Watching a Cutscene";
  if (strncasecmp(mapname, "day_", 4) == 0)
    return "Prologue";

  return "Custom Map"; // Protected string to feed safely into Discord RPC
}