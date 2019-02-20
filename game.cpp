#include "game.h"
#include "rng.h"
#include "keypress.h"
#include "output.h"
#include "skill.h"
#include "line.h"
#include "computer.h"
#include "veh_interact.h"
#include "options.h"
#include "mapbuffer.h"
#include "posix_time.h"
#include "file.h"
#include "recent_msg.h"
#include "saveload.h"

#include <fstream>
#include <sstream>
#include <math.h>
#include <sys/stat.h>
#include <utility>

using namespace cataclysm;

template<> int discard<int>::x = 0;

#define MAX_MONSTERS_MOVING 40 // Efficiency!

void intro();
nc_color sev(int a);	// Right now, ONLY used for scent debugging....

// This is the main game set-up process.
game::game()
{
 clear();	// Clear the screen
 intro();	// Print an intro screen, make sure we're at least 80x25
// Gee, it sure is init-y around here!
 ter_t::init();		// for terrain tiles (map.cpp)
 item::init();	      // Set up item types                (SEE itypedef.cpp)
 mtype::init();	      // Set up monster types             (SEE mtypedef.cpp)
 mtype::init_items();     // Set up the items monsters carry  (SEE monitemsdef.cpp)
 trap::init();	      // Set up the trap types            (SEE trapdef.cpp)
 map::init();     // Set up which items appear where  (SEE mapitemsdef.cpp)
 recipe::init();      // Set up crafting reciptes         (SEE crafting.cpp)
 mongroup::init();      // Set up monster categories        (SEE mongroupdef.cpp)
 mission_type::init();     // Set up mission templates         (SEE missiondef.cpp)
 constructable::init(); // Set up constructables            (SEE construction.cpp)
 mutation_branch::init();
 init_vehicles();     // Set up vehicles                  (SEE veh_typedef.cpp)
 load_keyboard_settings();
// Set up the main UI windows.
 w_terrain = newwin(SEEY * 2 + 1, SEEX * 2 + 1, 0, 0);
 werase(w_terrain);
 w_minimap = newwin(7, 7, 0, SEEX * 2 + 1);
 werase(w_minimap);
 w_HP = newwin(14, 7, 7, SEEX * 2 + 1);
 werase(w_HP);
 w_moninfo = newwin(12, 48, 0, SEEX * 2 + 8);
 werase(w_moninfo);
 w_messages = newwin(9, 48, 12, SEEX * 2 + 8);
 werase(w_messages);
 w_status = newwin(4, 55, 21, SEEX * 2 + 1);
 werase(w_status);

 gamemode = new special_game;	// Nothing, basically.
}

game::~game()	// XXX cf main() no delete call, OS cleanup (thus destructor not called and issues with dynamic memory allocation from init functions also not handled) \todo fix
{
 delete gamemode;
 delwin(w_terrain);
 delwin(w_minimap);
 delwin(w_HP);
 delwin(w_moninfo);
 delwin(w_messages);
 delwin(w_status);
}

void game::setup()	// early part looks like it belongs in game::game (but we return to the start screen rather than completely drop out
{
 u = player();
 m = map(); // Init the root map with our vectors
 z.reserve(1000); // Reserve some space

// Even though we may already have 'd', nextinv will be incremented as needed
 nextinv = 'd';
 next_npc_id = 1;
 next_faction_id = 1;
 next_mission_id = 1;
// Clear monstair values
 monstair = tripoint(-1,-1,-1);
 last_target = -1;	// We haven't targeted any monsters yet
 uquit = QUIT_NO;	// We haven't quit the game
 debugmon = false;	// We're not printing debug messages
 no_npc = false;		// We're not suppressing NPC spawns

// ... Unless data/no_npc.txt exists.
 std::ifstream ifile("data/no_npc.txt");
 if (ifile) no_npc = true;

 weather = WEATHER_CLEAR; // Start with some nice weather...
 nextweather = MINUTES(STARTING_MINUTES + 30); // Weather shift in 30
 turnssincelastmon = 0; //Auto safe mode init
 autosafemode = OPTIONS[OPT_AUTOSAFEMODE];

 footsteps.clear();
 z.clear();
 coming_to_stairs.clear();
 active_npc.clear();
 factions.clear();
 active_missions.clear();
 items_dragged.clear();
 messages.clear();
 events.clear();
 clear_scents();

 messages.turn.season = SUMMER;    // ... with winter conveniently a long ways off

 for (int i = 0; i < num_monsters; i++)	// Reset kill counts to 0
  kills[i] = 0;
 if (opening_screen()) {// Opening menu
// Finally, draw the screen!
  refresh_all();
  draw();
 }
}

// range of sel2 is 0..
static void _main_menu(WINDOW* const w_open, const int sel1)
{
	static const std::pair<int, const char*> menu[] = {
		{ 4, "MOTD" },
		{ 5, "New Game" },
		{ 6, "Load Game" },
		{ 7, "Special..." },
		{ 8, "Help" },
		{ 9, "Quit" } };
	for (auto x : menu) {
		mvwprintz(w_open, x.first, 1, (sel1 == x.first - 4 ? h_white : c_white), x.second);
	}
}

// range of sel2 is 1..3
static void _game_type_menu(WINDOW* const w_open,const int sel2)
{
	static const std::pair<int, const char*> menu[PLTYPE_MAX] = {
		{5, "Custom Character" },
		{6, "Template Character" },
		{7, "Random Character"} };
	for (auto x : menu) {
		mvwprintz(w_open, x.first, 12, (sel2 == x.first-4 ? h_white : c_white), x.second);
	}
}

bool game::opening_screen()
{

 WINDOW* w_open = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 erase();
 for (int i = 0; i < SCREEN_WIDTH; i++)
  mvwputch(w_open, 21, i, c_white, LINE_OXOX);
 mvwprintz(w_open, 0, 1, c_blue, "Welcome to Cataclysm!");
 mvwprintz(w_open, 1, 0, c_red, "\
This alpha release is highly unstable. Please report any crashes or bugs to\n\
http://github.com/zaimoni/Cataclysm .");
 refresh();
 wrefresh(w_open);
 refresh();
 std::vector<std::string> savegames, templates;
 std::string tmp;
#ifdef ZAIMONI_HAS_MICROSOFT_IO_H
 // Zaimoni: total reimplementation for MSVC.
 {
	 OS_dir working;
	 try {
		 if (!working.force_dir("save")) {
			 debugmsg("Could not make './save' directory");
			 endwin();
			 exit(EXIT_FAILURE);
		 }
		 if (working.exists("save/*.sav")) {
			 working.get_filenames(savegames);
			 for (auto& name : savegames) name = name.substr(0, name.find(".sav"));
		 }
		 if (working.exists("data/*.template")) {
			 working.get_filenames(templates);
			 for (auto& name : templates) name = name.substr(0, name.find(".template"));
		 }
	 }
	 catch (int e) {
		 debugmsg("unplanned file system state, not practical to open required directories.");
		 endwin();
		 exit(EXIT_FAILURE);
	 }
 }
#else
 dirent *dp;
 DIR *dir = opendir("save");
 if (!dir) {
#if (defined _WIN32 || defined __WIN32__)
  mkdir("save");
#else
  mkdir("save", 0777);
#endif
  dir = opendir("save");
 }
 if (!dir) {
  debugmsg("Could not make './save' directory");
  endwin();
  exit(1);
 }
 while (dp = readdir(dir)) {
  tmp = dp->d_name;
  if (tmp.find(".sav") != std::string::npos)
   savegames.push_back(tmp.substr(0, tmp.find(".sav")));
 }
 closedir(dir);
 dir = opendir("data");
 while (dp = readdir(dir)) {
  tmp = dp->d_name;
  if (tmp.find(".template") != std::string::npos)
   templates.push_back(tmp.substr(0, tmp.find(".template")));
 }
#endif
 int sel1 = 0, sel2 = 1, layer = 1;
 char ch;
 bool start = false;

// Load MOTD and store it in a string
 std::vector<std::string> motd;
 std::ifstream motd_file;
 motd_file.open("data/motd");
 if (!motd_file.is_open())
  motd.push_back("No message today.");
 else {
  while (!motd_file.eof()) {
   std::string tmp;
   getline(motd_file, tmp);
   if (tmp[0] != '#')
    motd.push_back(tmp);
  }
 }

 do {
  if (layer == 1) {
   _main_menu(w_open, sel1);
   
   if (sel1 == 0) {	// Print the MOTD.
    for (int i = 0; i < motd.size() && i < 16; i++)
     mvwprintz(w_open, i + 4, 12, c_ltred, motd[i].c_str());
   } else {	// Clear the lines if not viewing MOTD.
    for (int i = 4; i < 20; i++) {
     for (int j = 12; j < 79; j++)
      mvwputch(w_open, i, j, c_black, 'x');
    }
   }

   wrefresh(w_open);
   refresh();
   ch = input();
   if (ch == 'k') {
    if (sel1 > 0)
     sel1--;
    else
     sel1 = 5;
   } else if (ch == 'j') {
    if (sel1 < 5)
     sel1++;
    else
     sel1 = 0;
   } else if ((ch == 'l' || ch == '\n' || ch == '>') && sel1 > 0) {
    if (sel1 == 5) {
     uquit = QUIT_MENU;
     return false;
    } else if (sel1 == 4) {
     help();
     clear();
     mvwprintz(w_open, 0, 1, c_blue, "Welcome to Cataclysm!");
     mvwprintz(w_open, 1, 0, c_red, "\
This alpha release is highly unstable. Please report any crashes or bugs to\n\
http://github.com/zaimoni/Cataclysm .");
     refresh();
     wrefresh(w_open);
     refresh();
    } else {
     sel2 = 1;
     layer = 2;
    }
	_main_menu(w_open, sel1);
   }
  } else if (layer == 2) {
   if (sel1 == 1) {	// New Character
	_game_type_menu(w_open,sel2);
    wrefresh(w_open);
    refresh();
    ch = input();
    if (ch == 'k') {
     if (sel2 > 1)
      sel2--;
     else
      sel2 = 3;
    } if (ch == 'j') {
     if (sel2 < 3)
      sel2++;
     else
      sel2 = 1;
    } else if (ch == 'h' || ch == '<' || ch == KEY_ESCAPE) {
     mvwprintz(w_open, 5, 12, c_black, "                ");
     mvwprintz(w_open, 6, 12, c_black, "                ");
     mvwprintz(w_open, 7, 12, c_black, "                ");
     layer = 1;
     sel1 = 1;
    }
    if (ch == 'l' || ch == '\n' || ch == '>') {
     if (sel2 == 1) {
      if (!u.create(this, PLTYPE_CUSTOM)) {
       u = player();
       delwin(w_open);
       return (opening_screen());
      }
      start_game();
      start = true;
      ch = 0;
     }
     if (sel2 == 2) {
      layer = 3;
      sel1 = 0;
	  _game_type_menu(w_open, sel2);
     }
     if (sel2 == 3) {
      if (!u.create(this, PLTYPE_RANDOM)) {
       u = player();
       delwin(w_open);
       return (opening_screen());
      }
      start_game();
      start = true;
      ch = 0;
     }
    }
   } else if (sel1 == 2) {	// Load Character
    if (savegames.size() == 0)
     mvwprintz(w_open, 6, 12, c_red, "No save games found!");
    else {
     int savestart = (sel2 < 7 ?  0 : sel2 - 7),
         saveend   = (sel2 < 7 ? 14 : sel2 + 7);
     for (int i = savestart; i < saveend; i++) {
      int line = 6 + i - savestart;
      mvwprintz(w_open, line, 12, c_black, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
      if (i < savegames.size())
       mvwprintz(w_open, line, 12, (sel2 - 1 == i ? h_white : c_white),
                 savegames[i].c_str());
     }
    }
    wrefresh(w_open);
    refresh();
    ch = input();
    if (ch == 'k') {
     if (sel2 > 1)
      sel2--;
     else
      sel2 = savegames.size();
    } else if (ch == 'j') {
     if (sel2 < savegames.size())
      sel2++;
     else
      sel2 = 1;
    } else if (ch == 'h' || ch == '<' || ch == KEY_ESCAPE) {
     layer = 1;
     for (int i = 0; i < 14; i++)
      mvwprintz(w_open, 6 + i, 12, c_black, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    }
    if (ch == 'l' || ch == '\n' || ch == '>') {
     if (sel2 > 0 && savegames.size() > 0) {
      load(savegames[sel2 - 1]);
      start = true;
      ch = 0;
     }
    }
   } else if (sel1 == 3) {	// Special game
    for (int i = 1; i < NUM_SPECIAL_GAMES; i++) {
     mvwprintz(w_open, 6 + i, 12, c_black, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
     mvwprintz(w_open, 6 + i, 12, (sel2 == i ? h_white : c_white),
               special_game_name( special_game_id(i) ).c_str());
    }
    wrefresh(w_open);
    refresh();
    ch = input();
    if (ch == 'k') {
     if (sel2 > 1)
      sel2--;
     else
      sel2 = NUM_SPECIAL_GAMES - 1;
    } else if (ch == 'j') {
     if (sel2 < NUM_SPECIAL_GAMES - 1)
      sel2++;
     else
      sel2 = 1;
    } else if (ch == 'h' || ch == '<' || ch == KEY_ESCAPE) {
     layer = 1;
     for (int i = 6; i < 15; i++)
      mvwprintz(w_open, i, 12, c_black, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    }
    if (ch == 'l' || ch == '\n' || ch == '>') {
     if (sel2 >= 1 && sel2 < NUM_SPECIAL_GAMES) {
      delete gamemode;
      gamemode = get_special_game( special_game_id(sel2) );
      if (!gamemode->init(this)) {
       delete gamemode;
       gamemode = new special_game;
       u = player();
       delwin(w_open);
       return (opening_screen());
      }
      start = true;
      ch = 0;
     }
    }
   }
  } else if (layer == 3) {	// Character Templates
   if (templates.size() == 0)
    mvwprintz(w_open, 6, 12, c_red, "No templates found!");
   else {
    int tempstart = (sel1 < 6 ?  0 : sel1 - 6),
        tempend   = (sel1 < 6 ? 14 : sel1 + 6);
    for (int i = tempstart; i < tempend; i++) {
     int line = 6 + i - tempstart;
     mvwprintz(w_open, line, 29, c_black, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
     if (i < templates.size())
      mvwprintz(w_open, line, 29, (sel1 == i ? h_white : c_white),
                templates[i].c_str());
    }
   }
   wrefresh(w_open);
   refresh();
   ch = input();
   if (ch == 'k') {
    if (sel1 > 0)
     sel1--;
    else
     sel1 = templates.size() - 1;
   } else if (ch == 'j') {
    if (sel1 < templates.size() - 1)
     sel1++;
    else
     sel1 = 0;
   } else if (ch == 'h' || ch == '<' || ch == KEY_ESCAPE) {
    sel1 = 1;
    layer = 2;
    for (int i = 0; i < templates.size() && i < 21; i++)
     mvwprintz(w_open, 6 + i, 12, c_black, "                                 ");
    for (int i = 22; i < VIEW; i++)
     mvwprintw(w_open, i, 0, "                                                 \
                                ");
   }
   if (ch == 'l' || ch == '\n' || ch == '>') {
    if (!u.create(this, PLTYPE_TEMPLATE, templates[sel1])) {
     u = player();
     delwin(w_open);
     return (opening_screen());
    }
    start_game();
    start = true;
    ch = 0;
   }
  }
 } while (ch != 0);
 delwin(w_open);
 if (start == false) uquit = QUIT_MENU;
 return start;
}

// Set up all default values for a new game
void game::start_game()
{
 messages.turn = MINUTES(STARTING_MINUTES);// It's turn 0...
 run_mode = (OPTIONS[OPT_SAFEMODE] ? 1 : 0);
 mostseen = 0;	// ...and mostseen is 0, we haven't seen any monsters yet.

// Init some factions.
 if (!load_master())	// Master data record contains factions.
  create_factions();
 cur_om = overmap(this, 0, 0, 0);	// We start in the (0,0,0) overmap.
// Find a random house on the map, and set us there.
 cur_om.first_house(lev.x, lev.y);
 lev.x -= int(int(MAPSIZE / 2) / 2);
 lev.y -= int(int(MAPSIZE / 2) / 2);
 lev.z = 0;
// Start the overmap out with none of it seen by the player...
 for (int i = 0; i < OMAPX; i++) {
  for (int j = 0; j < OMAPX; j++)
   cur_om.seen(i, j) = false;
 }
// ...except for our immediate neighborhood.
 for (int i = -15; i <= 15; i++) {
  for (int j = -15; j <= 15; j++)
   cur_om.seen(lev.x + i, lev.y + j) = true;
 }
// Convert the overmap coordinates to submap coordinates
 lev.x = lev.x * 2 - 1;
 lev.y = lev.y * 2 - 1;
 set_adjacent_overmaps(true);
// Init the starting map at this location.
 //MAPBUFFER.load();
 m.load(this, lev.x, lev.y);
// Start us off somewhere in the shelter.
 u.posx = SEEX * int(MAPSIZE / 2) + 5;
 u.posy = SEEY * int(MAPSIZE / 2) + 5;
 u.str_cur = u.str_max;
 u.per_cur = u.per_max;
 u.int_cur = u.int_max;
 u.dex_cur = u.dex_max;
 nextspawn = int(messages.turn);
 temperature = 65; // Springtime-appropriate?

// Put some NPCs in there!
 create_starting_npcs();

}

void game::create_factions()
{
 int num = dice(4, 3);
 faction tmp(0);
 tmp.make_army();
 factions.push_back(tmp);
 for (int i = 0; i < num; i++) {
  tmp = faction(assign_faction_id());
  tmp.randomize();
  tmp.likes_u = 100;
  tmp.respects_u = 100;
  tmp.known_by_u = true;
  factions.push_back(tmp);
 }
}

void game::create_starting_npcs()
{
 npc tmp;
 tmp.normalize(this);
 tmp.randomize(this, (one_in(2) ? NC_DOCTOR : NC_NONE));
 tmp.spawn_at(&cur_om, lev.x, lev.y);
 tmp.posx = SEEX * int(MAPSIZE / 2) + SEEX;
 tmp.posy = SEEY * int(MAPSIZE / 2) + 6;
 tmp.form_opinion(&u);
 tmp.attitude = NPCATT_NULL;
 tmp.mission = NPC_MISSION_SHELTER;
 tmp.chatbin.first_topic = TALK_SHELTER;
 tmp.chatbin.missions.push_back(reserve_random_mission(ORIGIN_OPENER_NPC, om_location(), tmp.id) );

 active_npc.push_back(tmp);
}
 

// MAIN GAME LOOP
// Returns true if game is over (death, saved, quit, etc)
bool game::do_turn()
{
 if (is_game_over()) {
  write_msg();
// Save the monsters before we die!
  for (int i = 0; i < z.size(); i++) {
   if (z[i].is_static_spawn()) {	// Static spawn, move them back there
    tinymap tmp;
    tmp.load(this, z[i].spawnmap.x, z[i].spawnmap.y);
    tmp.add_spawn(&(z[i]));
    tmp.save(&cur_om, messages.turn, z[i].spawnmap.x, z[i].spawnmap.y);
   } else {	// Absorb them back into a group
    int group = valid_group((mon_id)(z[i].type->id), lev.x, lev.y);
    if (group != -1) {
     cur_om.zg[group].population++;
     if (cur_om.zg[group].population / pow(cur_om.zg[group].radius, 2.0) > 5)
      cur_om.zg[group].radius++;
    }
   }
  }
  if (uquit == QUIT_DIED) popup_top("Game over! Press spacebar...");
  if (uquit == QUIT_DIED || uquit == QUIT_SUICIDE) death_screen();
  return true;
 }
// Actual stuff
 gamemode->per_turn(this);
 messages.turn.increment();
 process_events();
 process_missions();
 if (messages.turn.hour == 0 && messages.turn.minute == 0 && messages.turn.second == 0) // Midnight!
  cur_om.process_mongroups();

// Check if we've overdosed... in any deadly way.
 if (u.stim > 250) {
  messages.add("You have a sudden heart attack!");
  u.hp_cur[hp_torso] = 0;
 } else if (u.stim < -200 || u.pkill > 240) {
  messages.add("Your breathing stops completely.");
  u.hp_cur[hp_torso] = 0;
 }

 if (messages.turn % 50 == 0) {	// Hunger, thirst, & fatigue up every 5 minutes
  if ((!u.has_trait(PF_LIGHTEATER) || !one_in(3)) &&
      (!u.has_bionic(bio_recycler) || messages.turn % 300 == 0))
   u.hunger++;
  if ((!u.has_bionic(bio_recycler) || messages.turn % 100 == 0) &&
      (!u.has_trait(PF_PLANTSKIN) || !one_in(5)))
   u.thirst++;
  u.fatigue++;
  if (u.fatigue == 192 && !u.has_disease(DI_LYING_DOWN) &&
      !u.has_disease(DI_SLEEP)) {
   if (u.activity.type == ACT_NULL)
    messages.add("You're feeling tired.  Press '$' to lie down for sleep.");
   else
    cancel_activity_query("You're feeling tired.");
  }
  if (u.stim < 0) u.stim++;
  if (u.stim > 0) u.stim--;
  if (u.pkill > 0) u.pkill--;
  if (u.pkill < 0) u.pkill++;
  if (u.has_bionic(bio_solar) && is_in_sunlight(u.posx, u.posy))
   u.charge_power(1);
 }
 if (messages.turn % 300 == 0) {	// Pain up/down every 30 minutes
  if (u.pain > 0)
   u.pain -= 1 + int(u.pain / 10);
  else if (u.pain < 0)
   u.pain++;
// Mutation healing effects
  if (u.has_trait(PF_FASTHEALER2) && one_in(5)) u.healall(1);
  if (u.has_trait(PF_REGEN) && one_in(2)) u.healall(1);
  if (u.has_trait(PF_ROT2) && one_in(5)) u.hurtall(1);
  if (u.has_trait(PF_ROT3) && one_in(2)) u.hurtall(1);

  if (u.radiation > 1 && one_in(3)) u.radiation--;
  u.get_sick(this);
// Auto-save on the half-hour
  save();
 }
// Update the weather, if it's time.
 if (messages.turn >= nextweather) update_weather();

// The following happens when we stay still; 10/40 minutes overdue for spawn
 if ((!u.has_trait(PF_INCONSPICUOUS) && messages.turn > nextspawn +  100) ||
     ( u.has_trait(PF_INCONSPICUOUS) && messages.turn > nextspawn +  400)   ) {
  spawn_mon(-1 + 2 * rng(0, 1), -1 + 2 * rng(0, 1));
  nextspawn = messages.turn;
 }

 process_activity();

 while (u.moves > 0) {
  cleanup_dead();
  if (!u.has_disease(DI_SLEEP) && u.activity.type == ACT_NULL)
   draw();
  get_input();
  if (is_game_over()) {
   if (uquit == QUIT_DIED) popup_top("Game over! Press spacebar...");
   if (uquit == QUIT_DIED || uquit == QUIT_SUICIDE) death_screen();
   return true;
  }
 }
 update_scent();
 m.vehmove(this);
 m.process_fields(this);
 m.process_active_items(this);
 m.step_in_field(u.posx, u.posy, this);

 monmove();
 update_stair_monsters();
 om_npcs_move();
 u.reset(this);
 u.process_active_items(this);
 u.suffer(this);

 if (lev.z >= 0) {
  (weather_datum::data[weather].effect)(this);
  check_warmth();
 }

 if (u.has_disease(DI_SLEEP) && int(messages.turn) % 300 == 0) {
  draw();
  refresh();
 }

 update_skills();
 if (messages.turn % 10 == 0) u.update_morale();
 return false;
}

void game::update_skills()
{
//    SKILL   TURNS/--
//	1	4096
//	2	2048
//	3	1024
//	4	 512
//	5	 256
//	6	 128
//	7+	  64
 for (int i = 0; i < num_skill_types; i++) {
  int tmp = u.sklevel[i] > 7 ? 7 : u.sklevel[i];
  if (u.sklevel[i] > 0 && messages.turn % (8192 / int(pow(2, double(tmp - 1)))) == 0 &&
      (( u.has_trait(PF_FORGETFUL) && one_in(3)) ||
       (!u.has_trait(PF_FORGETFUL) && one_in(4))   )) {
   if (u.has_bionic(bio_memory) && u.power_level > 0) {
    if (one_in(5))
     u.power_level--;
   } else
    u.skexercise[i]--;
  }
  if (u.skexercise[i] < -100) {
   u.sklevel[i]--;
   messages.add("Your skill in %s has reduced to %d!",
           skill_name(skill(i)).c_str(), u.sklevel[i]);
   u.skexercise[i] = 0;
  } else if (u.skexercise[i] >= 100) {
   u.sklevel[i]++;
   messages.add("Your skill in %s has increased to %d!",
           skill_name(skill(i)).c_str() ,u.sklevel[i]);
   u.skexercise[i] = 0;
  }
 }
}

void game::process_events()
{
 for (int i = 0; i < events.size(); i++) {
  events[i].per_turn(this);
  if (events[i].turn <= int(messages.turn)) {
   events[i].actualize(this);
   events.erase(events.begin() + i);
   i--;
  }
 }
}

void game::process_activity()
{
 if (u.activity.type != ACT_NULL) {
  if (int(messages.turn) % 150 == 0) draw();
  if (u.activity.type == ACT_WAIT) {	// Based on time, not speed
   u.activity.moves_left -= 100;
   u.pause(this);
  } else if (u.activity.type == ACT_REFILL_VEHICLE) {
   vehicle *veh = m.veh_at(u.activity.placement);
   if (!veh) {  // Vehicle must've moved or something!
    u.activity.moves_left = 0;
    return;
   }
   veh->refill (AT_GAS, 200);
   u.pause(this);
   u.activity.moves_left -= 100;
  } else {
   u.activity.moves_left -= u.moves;
   u.moves = 0;
  }

  if (u.activity.moves_left <= 0) {	// We finished our activity!

   switch (u.activity.type) {

   case ACT_RELOAD:
    u.weapon.reload(u, u.activity.index);
    if (u.weapon.is_gun() && u.weapon.has_flag(IF_RELOAD_ONE)) {
     messages.add("You insert a cartridge into your %s.",
             u.weapon.tname(this).c_str());
     if (u.recoil < 8) u.recoil = 8;
     if (u.recoil > 8) u.recoil = (8 + u.recoil) / 2;
    } else {
     messages.add("You reload your %s.", u.weapon.tname(this).c_str());
     u.recoil = 6;
    }
    break;

   case ACT_READ:
    {
    const it_book* reading = dynamic_cast<const it_book*>(-2 == u.activity.index ? u.weapon.type : u.inv[u.activity.index].type);

    if (reading->fun != 0) {
     std::stringstream morale_text;
     u.add_morale(MORALE_BOOK, reading->fun * 5, reading->fun * 15, reading);
    }

    if (u.sklevel[reading->type] < reading->level) {
     messages.add("You learn a little about %s!", skill_name(reading->type).c_str());
     int min_ex = reading->time / 10 + u.int_cur / 4,
         max_ex = reading->time /  5 + u.int_cur / 2 - u.sklevel[reading->type];
     if (min_ex < 1) min_ex = 1;
     if (max_ex < 2) max_ex = 2;
     if (max_ex > 10) max_ex = 10;
     u.skexercise[reading->type] += rng(min_ex, max_ex);
     if (u.sklevel[reading->type] +
        (u.skexercise[reading->type] >= 100 ? 1 : 0) >= reading->level)
      messages.add("You can no longer learn from this %s.", reading->name.c_str());
    }
	}
    break;

   case ACT_WAIT:
    messages.add("You finish waiting.");
    break;

   case ACT_CRAFT:
    complete_craft();
    break;

   case ACT_BUTCHER:
    complete_butcher(u.activity.index);
    break;

   case ACT_BUILD:
    complete_construction();
    break;

   case ACT_TRAIN:
    if (u.activity.index < 0) {
     messages.add("You learn %s.", item::types[0 - u.activity.index]->name.c_str());
     u.styles.push_back( itype_id(0 - u.activity.index) );
    } else {
     u.sklevel[ u.activity.index ]++;
	 messages.add("You finish training %s to level %d.",
             skill_name(u.activity.index).c_str(), u.sklevel[u.activity.index]);
    }
    break;
    
   case ACT_VEHICLE:
    complete_vehicle (this);
    break;
   }

   bool act_veh = (u.activity.type == ACT_VEHICLE);
   u.activity.type = ACT_NULL;
   if (act_veh) {
    if (u.activity.values.size() < 7)
     debugmsg ("process_activity invalid ACT_VEHICLE values:%d",
                u.activity.values.size());
    else {
     vehicle* const veh = m.veh_at(u.activity.values[0], u.activity.values[1]);
     if (veh) {
      exam_vehicle(*veh, u.activity.values[2], u.activity.values[3]);
      return;
     } else
      debugmsg ("process_activity ACT_VEHICLE: vehicle not found");
    }
   }
  }
 }
}

void game::cancel_activity()
{
 u.cancel_activity();
}

void game::cancel_activity_query(const char* message, ...)
{
 char buff[1024];
 va_list ap;
 va_start(ap, message);
 vsprintf(buff, message, ap);
 va_end(ap);
 std::string s(buff);

 bool doit = false;;

 switch (u.activity.type) {
  case ACT_NULL:
   doit = false;
   break;
  case ACT_READ:
   if (query_yn("%s Stop reading?", s.c_str())) doit = true;
   break;
  case ACT_RELOAD:
   if (query_yn("%s Stop reloading?", s.c_str())) doit = true;
   break;
  case ACT_CRAFT:
   if (query_yn("%s Stop crafting?", s.c_str())) doit = true;
   break;
  case ACT_BUTCHER:
   if (query_yn("%s Stop butchering?", s.c_str())) doit = true;
   break;
  case ACT_BUILD:
  case ACT_VEHICLE:
   if (query_yn("%s Stop construction?", s.c_str())) doit = true;
   break;
  case ACT_TRAIN:
   if (query_yn("%s Stop training?", s.c_str())) doit = true;
   break;
  default:
   doit = true;
 }

 if (doit)
  u.cancel_activity();
}

void game::update_weather()
{
	/* Chances for each season, for the weather listed on the left to shift to the
	* weather listed across the top.
	*/
	static const int weather_shift[4][NUM_WEATHER_TYPES][NUM_WEATHER_TYPES] = {
		{ // SPRING
		  //         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
			/* NUL */{ 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
			/* CLR */{ 0,  5,  2,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
			/* SUN */{ 0,  4,  7,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
			/* CLD */{ 0,  3,  0,  4,  3,  1,  0,  0,  1,  0,  1,  0,  0 },
			/* DRZ */{ 0,  1,  0,  3,  6,  3,  1,  0,  2,  0,  0,  0,  0 },
			/* RAI */{ 0,  0,  0,  4,  5,  7,  3,  1,  0,  0,  0,  0,  0 },
			//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
			/* TND */{ 0,  0,  0,  2,  2,  4,  5,  3,  0,  0,  0,  0,  0 },
			/* LGT */{ 0,  0,  0,  1,  1,  4,  5,  5,  0,  0,  0,  0,  0 },
			/* AC1 */{ 0,  1,  0,  1,  1,  1,  0,  0,  3,  3,  0,  0,  0 },
			/* AC2 */{ 0,  0,  0,  1,  1,  1,  0,  0,  4,  2,  0,  0,  0 },
			/* SN1 */{ 0,  1,  0,  3,  2,  1,  1,  0,  0,  0,  2,  1,  0 },
			//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
			/* SN2 */{ 0,  0,  0,  1,  1,  2,  1,  0,  0,  0,  3,  1,  1 },
			/* SN3 */{ 0,  0,  0,  0,  1,  3,  2,  1,  0,  0,  1,  1,  1 }
		},

{ // SUMMER
  //         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* NUL */{ 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	/* CLR */{ 0,  5,  5,  2,  2,  1,  1,  0,  1,  0,  1,  0,  0 },
	/* SUN */{ 0,  3,  7,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	/* CLD */{ 0,  1,  1,  6,  5,  2,  1,  0,  2,  0,  1,  0,  0 },
	/* DRZ */{ 0,  2,  2,  3,  6,  3,  1,  0,  2,  0,  0,  0,  0 },
	//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* RAI */{ 0,  1,  1,  3,  4,  5,  4,  2,  0,  0,  0,  0,  0 },
	/* TND */{ 0,  0,  0,  2,  3,  5,  4,  5,  0,  0,  0,  0,  0 },
	/* LGT */{ 0,  0,  0,  0,  0,  3,  3,  5,  0,  0,  0,  0,  0 },
	/* AC1 */{ 0,  1,  1,  2,  1,  1,  0,  0,  3,  4,  0,  0,  0 },
	/* AC2 */{ 0,  1,  0,  1,  1,  1,  0,  0,  5,  3,  0,  0,  0 },
	//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* SN1 */{ 0,  4,  0,  4,  2,  2,  1,  0,  0,  0,  2,  1,  0 },
	/* SN2 */{ 0,  0,  0,  2,  2,  4,  2,  0,  0,  0,  3,  1,  1 },
	/* SN3 */{ 0,  0,  0,  2,  1,  3,  3,  1,  0,  0,  2,  2,  0 }
},

{ // AUTUMN
  //         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* NUL */{ 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	/* CLR */{ 0,  6,  3,  3,  3,  1,  1,  0,  1,  0,  1,  0,  0 },
	/* SUN */{ 0,  4,  5,  2,  1,  0,  0,  0,  0,  0,  0,  0,  0 },
	/* CLD */{ 0,  1,  1,  8,  5,  2,  0,  0,  2,  0,  1,  0,  0 },
	/* DRZ */{ 0,  1,  0,  3,  6,  3,  1,  0,  2,  0,  0,  0,  0 },
	//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* RAI */{ 0,  1,  1,  3,  4,  5,  4,  2,  0,  0,  0,  0,  0 },
	/* TND */{ 0,  0,  0,  2,  3,  5,  4,  5,  0,  0,  0,  0,  0 },
	/* LGT */{ 0,  0,  0,  0,  0,  3,  3,  5,  0,  0,  0,  0,  0 },
	/* AC1 */{ 0,  1,  1,  2,  1,  1,  0,  0,  3,  4,  0,  0,  0 },
	/* AC2 */{ 0,  0,  0,  1,  1,  1,  0,  0,  4,  4,  0,  0,  0 },
	//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* SN1 */{ 0,  2,  0,  4,  2,  1,  0,  0,  0,  0,  2,  1,  0 },
	/* SN2 */{ 0,  0,  0,  2,  2,  5,  2,  0,  0,  0,  2,  1,  1 },
	/* SN3 */{ 0,  0,  0,  2,  1,  5,  2,  0,  0,  0,  2,  1,  1 }
},

{ // WINTER
  //         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* NUL */{ 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	/* CLR */{ 0,  9,  3,  4,  1,  0,  0,  0,  1,  0,  2,  0,  0 },
	/* SUN */{ 0,  4,  8,  1,  0,  0,  0,  0,  0,  0,  1,  0,  0 },
	/* CLD */{ 0,  1,  1,  8,  1,  0,  0,  0,  1,  0,  4,  2,  1 },
	/* DRZ */{ 0,  1,  0,  4,  3,  1,  0,  0,  1,  0,  3,  0,  0 },
	//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* RAI */{ 0,  0,  0,  3,  2,  2,  1,  1,  0,  0,  4,  4,  0 },
	/* TND */{ 0,  0,  0,  2,  1,  2,  2,  1,  0,  0,  2,  4,  1 },
	/* LGT */{ 0,  0,  0,  3,  0,  3,  3,  1,  0,  0,  2,  4,  4 },
	/* AC1 */{ 0,  1,  1,  4,  1,  0,  0,  0,  3,  1,  1,  0,  0 },
	/* AC2 */{ 0,  0,  0,  2,  1,  1,  0,  0,  4,  1,  1,  1,  0 },
	//         NUL CLR SUN CLD DRZ RAI THN LGT AC1 AC2 SN1 SN2 SN3
	/* SN1 */{ 0,  1,  0,  5,  1,  0,  0,  0,  0,  0,  7,  2,  0 },
	/* SN2 */{ 0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  2,  7,  3 },
	/* SN3 */{ 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  4,  6 }
}
	};

 season_type season = messages.turn.season;
// Pick a new weather type (most likely the same one)
 int chances[NUM_WEATHER_TYPES];
 int total = 0;
 for (int i = 0; i < NUM_WEATHER_TYPES; i++) {
  const auto& w_data = weather_datum::data[i];
// Reduce the chance for freezing-temp-only weather to 0 if it's above freezing
// and vice versa.
  if (   (w_data.avg_temperature[season] < 32 && temperature > 32)
      || (w_data.avg_temperature[season] > 32 && temperature < 32))
   chances[i] = 0;
  else {
   chances[i] = weather_shift[season][weather][i];
   if (w_data.dangerous && u.has_artifact_with(AEP_BAD_WEATHER))
    chances[i] = chances[i] * 4 + 10;
   total += chances[i];
  }
 }
  
 int choice = rng(0, total - 1);
 weather_type old_weather = weather;
 weather_type new_weather = WEATHER_CLEAR;
 if (total != 0) {
  while (choice >= chances[new_weather]) {
   choice -= chances[new_weather];
   new_weather = weather_type(int(new_weather) + 1);
  }
 }
// Advance the weather timer
 int minutes = rng(weather_datum::data[new_weather].mintime,
                   weather_datum::data[new_weather].maxtime);
 nextweather = messages.turn + MINUTES(minutes);
 weather = new_weather;
 if (weather == WEATHER_SUNNY && messages.turn.is_night()) weather = WEATHER_CLEAR;

 const auto& w_data = weather_datum::data[weather];
 if (weather != old_weather && w_data.dangerous &&
     lev.z >= 0 && m.is_outside(u.posx, u.posy)) {
  std::stringstream weather_text;
  weather_text << "The weather changed to " << w_data.name << "!";
  cancel_activity_query(weather_text.str().c_str());
 }

// Now update temperature
 if (!one_in(4)) { // 3 in 4 chance of respecting avg temp for the weather
  int average = w_data.avg_temperature[season];
  if (temperature < average)
   temperature++;
  else if (temperature > average)
   temperature--;
 } else // 1 in 4 chance of random walk
  temperature += rng(-1, 1);

 temperature += messages.turn.is_night() ? rng(-2, 1) : rng(-1, 2);
}

void game::give_mission(mission_id type)
{
 mission tmp = mission_type::types[type].create(this);
 active_missions.push_back(tmp);
 u.active_missions.push_back(tmp.uid);
 u.active_mission = u.active_missions.size() - 1;
 mission *miss = find_mission(tmp.uid);
 (*miss->type->start)(this, miss);
}

void game::assign_mission(int id)
{
 u.active_missions.push_back(id);
 u.active_mission = u.active_missions.size() - 1;
 mission *miss = find_mission(id);
 (*miss->type->start)(this, miss);
}
 
int game::reserve_mission(mission_id type, int npc_id)
{
 mission tmp = mission_type::types[type].create(this, npc_id);
 active_missions.push_back(tmp);
 return tmp.uid;
}

int game::reserve_random_mission(mission_origin origin, point p, int npc_id)
{
 std::vector<int> valid;
 for (int i = 0; i < mission_type::types.size(); i++) {
  const auto& miss_type = mission_type::types[i];
  for(const auto x0 : miss_type.origins) {
   if (x0 == origin && (*miss_type.place)(this, p.x, p.y)) {
    valid.push_back(i);
	break;
   }
  }
 }

 if (valid.empty()) return -1;

 int index = valid[rng(0, valid.size() - 1)];

 return reserve_mission(mission_id(index), npc_id);
}

npc* game::find_npc(int id)
{
 for (int i = 0; i < active_npc.size(); i++) {
  if (active_npc[i].id == id) return &(active_npc[i]);
 }
 for (int i = 0; i < cur_om.npcs.size(); i++) {
  if (cur_om.npcs[i].id == id) return &(cur_om.npcs[i]);
 }
 return NULL;
}

mission* game::find_mission(int id)
{
 for (int i = 0; i < active_missions.size(); i++) {
  if (active_missions[i].uid == id) return &(active_missions[i]);
 }
 return NULL;
}

mission_type* game::find_mission_type(int id)
{
 for (int i = 0; i < active_missions.size(); i++) {
  if (active_missions[i].uid == id) return active_missions[i].type;
 }
 return NULL;
}

bool game::mission_complete(int id, int npc_id)
{
 mission* miss = find_mission(id);
 if (miss == NULL) {
  debugmsg("game::mission_complete(%d) - it's NULL!", id);
  return false;
 }
 mission_type* type = miss->type;
 switch (type->goal) {
  case MGOAL_GO_TO: {
   point cur_pos(lev.x + int(MAPSIZE / 2), lev.y + int(MAPSIZE / 2));
   return rl_dist(cur_pos, miss->target) <= 1;
  } break;

  case MGOAL_FIND_ITEM:
   if (!u.has_amount(type->item_id, 1)) return false;
   if (miss->npc_id != -1 && miss->npc_id != npc_id)
    return false;
   return true;

  case MGOAL_FIND_ANY_ITEM:
   return (u.has_mission_item(miss->uid) &&
           (miss->npc_id == -1 || miss->npc_id == npc_id));

  case MGOAL_FIND_MONSTER:
   if (miss->npc_id != -1 && miss->npc_id != npc_id)
    return false;
   for (int i = 0; i < z.size(); i++) {
    if (z[i].mission_id == miss->uid) return true;
   }
   return false;

  case MGOAL_FIND_NPC:
   return (miss->npc_id == npc_id);

  case MGOAL_KILL_MONSTER:
   return (miss->step >= 1);

  default:
   return false;
 }
 return false;
}

bool game::mission_failed(int id)
{
 mission *miss = find_mission(id);
 return (miss->failed);
}

void game::wrap_up_mission(int id)
{
 mission *miss = find_mission(id);
 u.completed_missions.push_back( id );
 for (int i = 0; i < u.active_missions.size(); i++) {
  if (u.active_missions[i] == id) {
   u.active_missions.erase( u.active_missions.begin() + i );
   i--;
  }
 }
 switch (miss->type->goal) {
  case MGOAL_FIND_ITEM:
   u.use_amount(miss->type->item_id, 1);
   break;
  case MGOAL_FIND_ANY_ITEM:
   u.remove_mission_items(miss->uid);
   break;
 }
 (*miss->type->end)(this, miss);
}

void game::fail_mission(int id)
{
 mission *miss = find_mission(id);
 miss->failed = true;
 u.failed_missions.push_back( id );
 for (int i = 0; i < u.active_missions.size(); i++) {
  if (u.active_missions[i] == id) {
   u.active_missions.erase( u.active_missions.begin() + i );
   i--;
  }
 }
 (*miss->type->fail)(this, miss);
}

void game::mission_step_complete(int id, int step)
{
 mission *miss = find_mission(id);
 miss->step = step;
 switch (miss->type->goal) {
  case MGOAL_FIND_ITEM:
  case MGOAL_FIND_MONSTER:
  case MGOAL_KILL_MONSTER: {
   bool npc_found = false;
   for (int i = 0; i < cur_om.npcs.size(); i++) {
    if (cur_om.npcs[i].id == miss->npc_id) {
     miss->target = point(cur_om.npcs[i].mapx, cur_om.npcs[i].mapy);
     npc_found = true;
    }
   }
   if (!npc_found)
    miss->target = point(-1, -1);
  } break;
 }
}

void game::process_missions()
{
 for (int i = 0; i < active_missions.size(); i++) {
  if (active_missions[i].deadline > 0 &&
      int(messages.turn) > active_missions[i].deadline)
   fail_mission(active_missions[i].uid);
 }
}

void game::get_input()
{
 char ch = input(); // See keypress.h - translates keypad and arrows to vikeys

 action_id act = keys.translate(ch);
 if (!act) {
  if (ch != ' ' && ch != KEY_ESCAPE && ch != '\n') messages.add("Unknown command: '%c'", ch);
  return;
 }

// This has no action unless we're in a special game mode.
 gamemode->pre_action(this, act);

 int veh_part;
 vehicle *veh = m.veh_at(u.posx, u.posy, veh_part);
 bool veh_ctrl = veh && veh->player_in_control (&u);

 switch (act) {

  case ACTION_PAUSE:
   if (run_mode == 2) // Monsters around and we don't wanna pause
    messages.add("Monster spotted--safe mode is on! (Press '!' to turn it off.)");
   else
    u.pause(this);
   break;

  case ACTION_MOVE_N:
   if (u.in_vehicle)
    pldrive(0, -1);
   else
    plmove(0, -1);
   break;

  case ACTION_MOVE_NE:
   if (u.in_vehicle)
    pldrive(1, -1);
   else
    plmove(1, -1);
   break;

  case ACTION_MOVE_E:
   if (u.in_vehicle)
    pldrive(1, 0);
   else
    plmove(1, 0);
   break;

  case ACTION_MOVE_SE:
   if (u.in_vehicle)
    pldrive(1, 1);
   else
    plmove(1, 1);
   break;

  case ACTION_MOVE_S:
   if (u.in_vehicle)
    pldrive(0, 1);
   else
   plmove(0, 1);
   break;

  case ACTION_MOVE_SW:
   if (u.in_vehicle)
    pldrive(-1, 1);
   else
    plmove(-1, 1);
   break;

  case ACTION_MOVE_W:
   if (u.in_vehicle)
    pldrive(-1, 0);
   else
    plmove(-1, 0);
   break;

  case ACTION_MOVE_NW:
   if (u.in_vehicle)
    pldrive(-1, -1);
   else
    plmove(-1, -1);
   break;

  case ACTION_MOVE_DOWN:
   if (!u.in_vehicle)
    vertical_move(-1, false);
   break;

  case ACTION_MOVE_UP:
   if (!u.in_vehicle)
    vertical_move( 1, false);
   break;

  case ACTION_OPEN:
   open();
   break;

  case ACTION_CLOSE:
   close();
   break;

  case ACTION_SMASH:
   if (veh_ctrl)
    handbrake();
   else
    smash();
   break;

  case ACTION_EXAMINE:
   examine();
   break;

  case ACTION_PICKUP:
   pickup(u.posx, u.posy, 1);
   break;

  case ACTION_BUTCHER:
   butcher();
   break;

  case ACTION_CHAT:
   chat();
   break;

  case ACTION_LOOK:
   look_around();
   break;

  case ACTION_INVENTORY: {
   bool has = false;
   do {
    char ch = inv();
    has = u.has_item(ch);
    if (has)
     full_screen_popup(u.i_at(ch).info(true).c_str());
   } while (has);
   refresh_all();
  } break;

  case ACTION_ORGANIZE:
   reassign_item();
   break;

  case ACTION_USE:
   use_item();
   break;

  case ACTION_WEAR:
   wear();
   break;

  case ACTION_TAKE_OFF:
   takeoff();
   break;

  case ACTION_EAT:
   eat();
   break;

  case ACTION_READ:
   read();
   break;

  case ACTION_WIELD:
   wield();
   break;

  case ACTION_PICK_STYLE:
   u.pick_style(this);
   if (u.weapon.type->id == 0 || u.weapon.is_style()) {
    u.weapon = item(item::types[u.style_selected], 0);
    u.weapon.invlet = ':';
   }
   refresh_all();
   break;

  case ACTION_RELOAD:
   reload();
   break;

  case ACTION_UNLOAD:
   unload();
   break;

  case ACTION_THROW:
   plthrow();
   break;

  case ACTION_FIRE:
   plfire(false);
   break;

  case ACTION_FIRE_BURST:
   plfire(true);
   break;

  case ACTION_DROP:
   drop();
   break;

  case ACTION_DIR_DROP:
   drop_in_direction();
   break;

  case ACTION_BIONICS:
   u.power_bionics(this);
   refresh_all();
   break;

  case ACTION_WAIT:
   if (veh_ctrl) veh->turret_mode = !veh->turret_mode;
   else wait();
   break;

  case ACTION_CRAFT:
   craft();
   break;

  case ACTION_CONSTRUCT:
   if (u.in_vehicle)
    messages.add("You can't construct while in vehicle.");
   else
    construction_menu();
   break;

  case ACTION_SLEEP:
   if (veh_ctrl) {
    veh->cruise_on = !veh->cruise_on;
	messages.add("Cruise control turned %s.", veh->cruise_on? "on" : "off");
   } else if (query_yn("Are you sure you want to sleep?")) {
    u.try_to_sleep(this);
    u.moves = 0;
   }
   break;

  case ACTION_TOGGLE_SAFEMODE:
   if (run_mode == 0 ) {
    run_mode = 1;
    messages.add("Safe mode ON!");
   } else {
    turnssincelastmon = 0;
    run_mode = 0;
    if (autosafemode)
    messages.add("Safe mode OFF! (Auto safe mode still enabled!)");
    else
    messages.add("Safe mode OFF!");
   }
   break;

  case ACTION_TOGGLE_AUTOSAFE:
   if (autosafemode) {
    messages.add("Auto safe mode OFF!");
    autosafemode = false;
   } else {
    messages.add("Auto safe mode ON");
    autosafemode = true;
   }
   break;

  case ACTION_IGNORE_ENEMY:
   if (run_mode == 2) {
    messages.add("Ignoring enemy!");
    run_mode = 1;
   }
   break;

  case ACTION_SAVE:
   if (query_yn("Save and quit?")) {
    save();
    u.moves = 0;
    uquit = QUIT_SAVED;
   }
   break;

  case ACTION_QUIT:
   if (query_yn("Commit suicide?")) {
    u.moves = 0;
    std::vector<item> tmp = u.inv_dump();
    item your_body(messages.turn);
    your_body.name = u.name;
    m.add_item(u.posx, u.posy, your_body);
    for (int i = 0; i < tmp.size(); i++)
     m.add_item(u.posx, u.posy, tmp[i]);
    //m.save(&cur_om, turn, levx, levy);
    //MAPBUFFER.save();
    uquit = QUIT_SUICIDE;
   }
   break;

  case ACTION_PL_INFO:
   u.disp_info(this);
   refresh_all();
   break;

  case ACTION_MAP:
   draw_overmap();
   break;

  case ACTION_MISSIONS:
   list_missions();
   break;

  case ACTION_FACTIONS:
   list_factions();
   break;

  case ACTION_MORALE:
   u.disp_morale();
   refresh_all();
   break;

  case ACTION_MESSAGES:
   msg_buffer();
   break;

  case ACTION_HELP:
   help();
   refresh_all();
   break;

  case ACTION_DEBUG:
   debug();
   break;

  case ACTION_DISPLAY_SCENT:
   display_scent();
   break;

  case ACTION_TOGGLE_DEBUGMON:
   debugmon = !debugmon;
   messages.add("Debug messages %s!", (debugmon ? "ON" : "OFF"));
   break;
 }

 gamemode->post_action(this, act);
}

int& game::scent(int x, int y)
{
 if (x < 0 || x >= SEEX * MAPSIZE || y < 0 || y >= SEEY * MAPSIZE) {
  return discard<int>::x = 0;	// Out-of-bounds - null scent
 }
 return grscent[x][y];
}

void game::update_scent()
{
 decltype(grscent) newscent;
 grscent[u.posx][u.posy] = !u.has_active_bionic(bio_scent_mask) ? u.scent : 0;	// bionic actively erases scent, not just suppressing yours

 for (int x = u.posx - 18; x <= u.posx + 18; x++) {
  for (int y = u.posy - 18; y <= u.posy + 18; y++) {
   newscent[x][y] = 0;
   if (m.move_cost(x, y) != 0 || m.has_flag(bashable, x, y)) {
    int squares_used = 0;
    for (int i = -1; i <= 1; i++) {
     for (int j = -1; j <= 1; j++) {
      if (grscent[x][y] <= grscent[x+i][y+j]) {
       newscent[x][y] += grscent[x+i][y+j];
       squares_used++;
      }
     }
    }
    newscent[x][y] /= (squares_used + 1);
    if (m.field_at(x, y).type == fd_slime &&
        newscent[x][y] < 10 * m.field_at(x, y).density)
     newscent[x][y] = 10 * m.field_at(x, y).density;
    if (newscent[x][y] > 10000) {
     debugmsg("Wacky scent at %d, %d (%d)", x, y, newscent[x][y]);
     newscent[x][y] = 0; // Scent should never be higher
    }
   }
  }
 }
 for (int x = u.posx - 18; x <= u.posx + 18; x++) {
  for (int y = u.posy - 18; y <= u.posy + 18; y++)
   grscent[x][y] = newscent[x][y];
 }
 grscent[u.posx][u.posy] = (!u.has_active_bionic(bio_scent_mask) ? u.scent : 0);
}

bool game::is_game_over()
{
 if (uquit != QUIT_NO) return true;
 for (int i = 0; i <= hp_torso; i++) {
  if (u.hp_cur[i] < 1) {
   std::vector<item> tmp = u.inv_dump();
   item your_body(messages.turn);
   your_body.name = u.name;
   m.add_item(u.posx, u.posy, your_body);
   for (int i = 0; i < tmp.size(); i++)
    m.add_item(u.posx, u.posy, tmp[i]);
   //m.save(&cur_om, turn, levx, levy);
   //MAPBUFFER.save();
   std::stringstream playerfile;
   playerfile << "save/" << u.name << ".sav";
   unlink(playerfile.str().c_str());
   uquit = QUIT_DIED;
   return true;
  }
 }
 return false;
}

void game::death_screen()
{
 gamemode->game_over(this);
 std::stringstream playerfile;
 playerfile << "save/" << u.name << ".sav";
 unlink(playerfile.str().c_str());
 int num_kills = 0;
 for (int i = 0; i < num_monsters; i++)
  num_kills += kills[i];

 WINDOW* w_death = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 mvwprintz(w_death, 0, 35, c_red, "GAME OVER - Press Spacebar to Quit");
 mvwprintz(w_death, 2, 0, c_white, "Number of kills: %d", num_kills);
 int line = 0, mon = 0;
 while (line < 40 && mon < num_monsters) {
  if (kills[mon] > 0) {
   int y = line % 20 + 3, x = int(line / 20) * 40 + 1;
   mvwprintz(w_death, y, x, c_white, "%s: %d", mtype::types[mon]->name.c_str(), kills[mon]);
   line++;
  }
  mon++;
 }

 wrefresh(w_death);
 refresh();
 char ch;
 do ch = getch();
 while(ch != ' ' && ch != '\n' && ch != KEY_ESCAPE);
 delwin(w_death);
}

bool game::load_master()
{
 std::ifstream fin;
 std::string data;
 char junk;
 fin.open("save/master.gsav");
 if (!fin.is_open()) return false;

// First, get the next ID numbers for each of these
 fin >> next_mission_id >> next_faction_id >> next_npc_id;
 int num_missions, num_npc, num_factions, num_items;

 fin >> num_missions;
 if (fin.peek() == '\n')
  fin.get(junk); // Chomp that pesky endline
 for (int i = 0; i < num_missions; i++) {
  mission tmpmiss;
  tmpmiss.load_info(this, fin);
  active_missions.push_back(tmpmiss);
 }

 fin >> num_factions;
 if (fin.peek() == '\n')
  fin.get(junk); // Chomp that pesky endline
 for (int i = 0; i < num_factions; i++) {
  getline(fin, data);
  faction tmp;
  tmp.load_info(data);
  factions.push_back(tmp);
 }
// NPCs come next
 fin >> num_npc;
 if (fin.peek() == '\n')
  fin.get(junk); // Chomp that pesky endline
 for (int i = 0; i < num_npc; i++) {
  getline(fin, data);
  npc tmp;
  tmp.load_info(this, data);
// We need to load up all their items too
  fin >> num_items;
  std::vector<item> tmpinv;
  for (int j = 0; j < num_items; j++) {
   std::string itemdata;
   char item_place;
   fin >> item_place;
   if (!fin.eof()) {
    getline(fin, itemdata);
    if (item_place == 'I')
     tmpinv.push_back(item(itemdata, this));
    else if (item_place == 'C' && !tmpinv.empty()) {
     tmpinv[tmpinv.size() - 1].contents.push_back(item(itemdata, this));
     j--;
    } else if (item_place == 'W')
     tmp.worn.push_back(item(itemdata, this));
    else if (item_place == 'w')
     tmp.weapon = item(itemdata, this);
    else if (item_place == 'c') {
     tmp.weapon.contents.push_back(item(itemdata, this));
     j--;
    }
   }
  }
  tmp.inv.add_stack(tmpinv);
  active_npc.push_back(tmp);
  if (fin.peek() == '\n')
   fin.get(junk); // Chomp that pesky endline
 }
 
 fin.close();
 return true;
}

void game::load(std::string name)
{
 std::ifstream fin;
 std::stringstream playerfile;
 playerfile << "save/" << name << ".sav";
 fin.open(playerfile.str().c_str());
// First, read in basic game state information.
 if (!fin.is_open()) {
  debugmsg("No save game exists!");
  return;
 }
 u = player();
 u.name = name;
 u.weapon = item::null;
 int tmpturn, tmpspawn, tmpnextweather, tmprun, tmptar, tmpweather, tmptemp,
     comx, comy;
 fin >> tmpturn >> tmptar >> tmprun >> mostseen >> nextinv >> next_npc_id >>
        next_faction_id >> next_mission_id >> tmpspawn >> tmpnextweather >>
        tmpweather >> tmptemp >> lev >> comx >> comy;
 messages.turn = tmpturn;
 nextspawn = tmpspawn;
 nextweather = tmpnextweather;
 cur_om = overmap(this, comx, comy, lev.z);
// m = map(&itypes, &mapitems, &traps); // Init the root map with our vectors
 //MAPBUFFER.load();
 m.load(this, lev.x, lev.y);
 run_mode = tmprun;
 if (OPTIONS[OPT_SAFEMODE] && run_mode == 0)
  run_mode = 1;
 autosafemode = OPTIONS[OPT_AUTOSAFEMODE];
 last_target = tmptar;
 weather = weather_type(tmpweather);
 temperature = tmptemp;
// Next, the scent map.
 for (int i = 0; i < SEEX * MAPSIZE; i++) {
  for (int j = 0; j < SEEY * MAPSIZE; j++)
   fin >> grscent[i][j];
 }
// Now the number of monsters...
 int nummon;
 fin >> nummon;
// ... and the data on each one.
 std::string data;
 z.clear();
 monster montmp;
 char junk;
 if (fin.peek() == '\n')
  fin.get(junk); // Chomp that pesky endline
 for (int i = 0; i < nummon; i++) {
  getline(fin, data);
  montmp.load_info(data);
  z.push_back(montmp);
 }
// And the kill counts;
 if (fin.peek() == '\n')
  fin.get(junk); // Chomp that pesky endline
 for (int i = 0; i < num_monsters; i++)
  fin >> kills[i];
// Finally, the data on the player.
 if (fin.peek() == '\n')
  fin.get(junk); // Chomp that pesky endline
 getline(fin, data);
 u.load_info(this, data);
// And the player's inventory...
 char item_place;
 std::string itemdata;
// We need a temporary vector of items.  Otherwise, when we encounter an item
// which is contained in another item, the auto-sort/stacking behavior of the
// player's inventory may cause the contained item to be misplaced.
 std::vector<item> tmpinv;
 while (!fin.eof()) {
  fin >> item_place;
  if (!fin.eof()) {
   getline(fin, itemdata);
   if (item_place == 'I')
    tmpinv.push_back(item(itemdata, this));
   else if (item_place == 'C')
    tmpinv[tmpinv.size() - 1].contents.push_back(item(itemdata, this));
   else if (item_place == 'W')
    u.worn.push_back(item(itemdata, this));
   else if (item_place == 'w')
    u.weapon = item(itemdata, this);
   else if (item_place == 'c')
    u.weapon.contents.push_back(item(itemdata, this));
  }
 }
// Now dump tmpinv into the player's inventory
 u.inv.add_stack(tmpinv);
 fin.close();
// Now load up the master game data; factions (and more?)
 load_master();
 set_adjacent_overmaps(true);
 draw();
}

void game::save()
{
 std::stringstream playerfile, masterfile;
 std::ofstream fout;
 playerfile << "save/" << u.name << ".sav";
 masterfile << "save/master.gsav";
 fout.open(playerfile.str().c_str());
// First, write out basic game state information.
 fout << int(messages.turn) << " " << int(last_target) << " " << int(run_mode) << " " <<
         mostseen << " " << nextinv << " " << next_npc_id << " " <<
         next_faction_id << " " << next_mission_id << " " << int(nextspawn) <<
         " " << int(nextweather) << " " << weather << " " << int(temperature) <<
         " " << lev << " " << cur_om.pos.x <<
         " " << cur_om.pos.y << " " << std::endl;
// Next, the scent map.
 for (int i = 0; i < SEEX * MAPSIZE; i++) {
  for (int j = 0; j < SEEY * MAPSIZE; j++)
   fout << grscent[i][j] << " ";
 }
// Now save all monsters.
 fout << std::endl << z.size() << std::endl;
 for (const auto& mon : z) fout << mon.save_info() << std::endl;
 for (int i = 0; i < num_monsters; i++)	// Save the kill counts, too.
  fout << kills[i] << " ";
// And finally the player.
 fout << u.save_info() << std::endl;
 fout << std::endl;
 fout.close();

// Now write things that aren't player-specific: factions and NPCs
 fout.open(masterfile.str().c_str());

 fout << next_mission_id << " " << next_faction_id << " " << next_npc_id <<
         " " << active_missions.size() << " ";
 for (int i = 0; i < active_missions.size(); i++)
  fout << active_missions[i].save_info() << " ";

 fout << factions.size() << std::endl;
 for (int i = 0; i < factions.size(); i++)
  fout << factions[i].save_info() << std::endl;

 fout << active_npc.size() << std::endl;
 for (int i = 0; i < active_npc.size(); i++) {
  active_npc[i].mapx = lev.x;
  active_npc[i].mapy = lev.y;
  fout << active_npc[i].save_info() << std::endl;
 }

 fout.close();

// Finally, save artifacts.
 if (item::types.size() > num_all_items) {
  fout.open("save/artifacts.gsav");
  for (int i = num_all_items; i < item::types.size(); i++)
   fout << item::types[i]->save_data() << "\n";
  fout.close();
 }
// aaaand the overmap, and the local map.
 cur_om.save(u.name);
 //m.save(&cur_om, turn, levx, levy);
 //MAPBUFFER.save();
}

void game::advance_nextinv()
{
 if (nextinv == 'z')
  nextinv = 'A';
 else if (nextinv == 'Z')
  nextinv = 'a';
 else
  nextinv++;
}

void game::decrease_nextinv()
{
 if (nextinv == 'a')
  nextinv = 'Z';
 else if (nextinv == 'A')
  nextinv = 'z';
 else
  nextinv--;
}

void game::add_event(event_type type, int on_turn, int faction_id, int x, int y)
{
 event tmp(type, on_turn, faction_id, x, y);
 events.push_back(tmp);
}

bool game::event_queued(event_type type)
{
 for (int i = 0; i < events.size(); i++) {
  if (events[i].type == type) return true;
 }
 return false;
}

void game::debug()
{
 int action = menu("Debug Functions - Using these is CHEATING!",
                   "Wish for an item",       // 1
                   "Teleport - Short Range", // 2
                   "Teleport - Long Range",  // 3
                   "Reveal map",             // 4
                   "Spawn NPC",              // 5
                   "Spawn Monster",          // 6
                   "Check game state...",    // 7
                   "Kill NPCs",              // 8
                   "Mutate",                 // 9
                   "Spawn a vehicle",        // 10
                   "Increase all skills",    // 11
                   "Learn all melee styles", // 12
                   "Check NPC",              // 13
                   "Spawn Artifact",         // 14
                   "Cancel",                 // 15
                   NULL);
 int veh_num;
 std::vector<std::string> opts;
 switch (action) {
  case 1:
   wish();
   break;

  case 2:
   teleport();
   break;

  case 3: {
   point tmp = cur_om.choose_point(this);
   if (tmp.x != -1) {
    z.clear();
    //m.save(&cur_om, turn, levx, levy);
    lev.x = tmp.x * 2 - int(MAPSIZE / 2);
    lev.y = tmp.y * 2 - int(MAPSIZE / 2);
    m.load(this, lev.x, lev.y);
   }
  } break;

  case 4:
   debugmsg("%d radio towers", cur_om.radios.size());
   for (int i = 0; i < OMAPX; i++) {
    for (int j = 0; j < OMAPY; j++)
     cur_om.seen(i, j) = true;
   }
   break;

  case 5: {
   npc temp;
   temp.normalize(this);
   temp.randomize(this);
   temp.attitude = NPCATT_TALK;
   temp.spawn_at(&cur_om, lev.x + (1 * rng(-2, 2)), lev.y + (1 * rng(-2, 2)));
   temp.posx = u.posx - 4;
   temp.posy = u.posy - 4;
   temp.form_opinion(&u);
   temp.attitude = NPCATT_TALK;
   temp.mission = NPC_MISSION_NULL;
   int mission_index = reserve_random_mission(ORIGIN_ANY_NPC,
                                              om_location(), temp.id);
   if (mission_index != -1)
   temp.chatbin.missions.push_back(mission_index);
   active_npc.push_back(temp);
  } break;

  case 6:
   monster_wish();
   break;

  case 7:
   popup_top("\
Location %d:%d in %d:%d, %s\n\
Current turn: %d; Next spawn %d.\n\
NPCs are %s spawn.\n\
%d monsters exist.\n\
%d events planned.", u.posx, u.posy, lev.x, lev.y,
oter_t::list[cur_om.ter(lev.x / 2, lev.y / 2)].name.c_str(),
int(messages.turn), int(nextspawn), (no_npc ? "NOT going to" : "going to"),
z.size(), events.size());

   if (!active_npc.empty())
    popup_top("%s: %d:%d (you: %d:%d)", active_npc[0].name.c_str(),
              active_npc[0].posx, active_npc[0].posy, u.posx, u.posy);
   break;

  case 8:
   for (int i = 0; i < active_npc.size(); i++) {
    messages.add("%s's head implodes!", active_npc[i].name.c_str());
    active_npc[i].hp_cur[bp_head] = 0;
   }
   break;

  case 9:
   mutation_wish();
   break;

  case 10:
   if (m.veh_at(u.posx, u.posy))
    debugmsg ("There's already vehicle here");
   else {
	for(auto v_type : vehicle::vtypes) 
     opts.push_back (v_type->name);
    opts.push_back (std::string("Cancel"));
    veh_num = menu_vec ("Choose vehicle to spawn", opts) + 1;
    if (veh_num > 1 && veh_num < num_vehicles)
     m.add_vehicle (this, (vhtype_id)veh_num, u.posx, u.posy, -90);
   }
   break;

  case 11:
   for (int i = 0; i < num_skill_types; i++)
    u.sklevel[i] += 3;
   break;

  case 12:
   for (int i = itm_style_karate; i <= itm_style_zui_quan; i++)
    u.styles.push_back( itype_id(i) );
   break;

  case 13: {
   point p = look_around();
   if (npc* const _npc = nPC(p)) {
    std::stringstream data;
    data << _npc->name << " " << (_npc->male ? "Male" : "Female") << std::endl;
    data << npc_class_name(_npc->myclass) << "; " <<
            npc_attitude_name(_npc->attitude) << std::endl;
    if (_npc->has_destination())
     data << "Destination: " << _npc->goal.x << ":" << _npc->goal.y << "(" <<
		     oter_t::list[ cur_om.ter(_npc->goal.x, _npc->goal.y) ].name << ")" <<
             std::endl;
    else
     data << "No destination." << std::endl;
    data << "Trust: " << _npc->op_of_u.trust << " Fear: " << _npc->op_of_u.fear <<
            " Value: " << _npc->op_of_u.value << " Anger: " << _npc->op_of_u.anger <<
            " Owed: " << _npc->op_of_u.owed << std::endl;
    data << "Aggression: " << int(_npc->personality.aggression) << " Bravery: " <<
            int(_npc->personality.bravery) << " Collector: " <<
            int(_npc->personality.collector) << " Altruism: " <<
            int(_npc->personality.altruism) << std::endl;
    for (int i = 0; i < num_skill_types; i++) {
     data << skill_name( skill(i) ) << ": " << _npc->sklevel[i];
     if (i % 2 == 1)
      data << std::endl;
     else
      data << "\t";
    }

    full_screen_popup(data.str().c_str());
   } else {
    popup("No NPC there.");
   }
  } break;

  case 14:
   point center = look_around();
   artifact_natural_property prop =
    artifact_natural_property(rng(ARTPROP_NULL + 1, ARTPROP_MAX - 1));
   m.create_anomaly(center.x, center.y, prop);
   m.add_item(center.x, center.y, new_natural_artifact(prop), 0);
   //m.add_item(u.posx, u.posy, new_natural_artifact(), 0);
   break;
 }
 erase();
 refresh_all();
}

void game::mondebug()
{
 for (int i = 0; i < z.size(); i++) {
  z[i].debug(u);
  if (z[i].has_flag(MF_SEES) && m.sees(z[i].pos.x, z[i].pos.y, u.posx, u.posy, -1))
   debugmsg("The %s can see you.", z[i].name().c_str());
  else
   debugmsg("The %s can't see you...", z[i].name().c_str());
 }
}

void game::groupdebug()
{
 erase();
 mvprintw(0, 0, "OM %d : %d    M %d : %d", cur_om.pos.x, cur_om.pos.y, lev.x, lev.y);
 int linenum = 1;
 for (int i = 0; i < cur_om.zg.size(); i++) {
  const int dist = trig_dist(lev.x, lev.y, cur_om.zg[i].pos);
  if (dist <= cur_om.zg[i].radius) {
   mvprintw(linenum, 0, "Zgroup %d: Centered at %d:%d, radius %d, pop %d",
            i, cur_om.zg[i].pos.x, cur_om.zg[i].pos.y, cur_om.zg[i].radius,
            cur_om.zg[i].population);
   linenum++;
  }
 }
 getch();
}

void game::draw_overmap()
{
 cur_om.choose_point(this);
}

void game::disp_kills()
{
 WINDOW* w = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 std::vector<mtype *> types;
 std::vector<int> count;
 for (int i = 0; i < num_monsters; i++) {
  if (kills[i] > 0) {
   types.push_back(mtype::types[i]);
   count.push_back(kills[i]);
  }
 }

 mvwprintz(w, 0, 35, c_red, "KILL COUNTS:");

 if (types.size() == 0) {
  mvwprintz(w, 2, 2, c_white, "You haven't killed any monsters yet!");
  wrefresh(w);
  getch();
  werase(w);
  wrefresh(w);
  delwin(w);
  refresh_all();
  return;
 }

 for (int i = 0; i < types.size(); i++) {
  if (i < VIEW - 1) {
   mvwprintz(w, i + 1,  0, types[i]->color, "%c %s", types[i]->sym,
             types[i]->name.c_str());
   int hori = 25;
   if (count[i] >= 10) hori = 24;
   if (count[i] >= 100) hori = 23;
   if (count[i] >= 1000) hori = 22;
   mvwprintz(w, i + 1, hori, c_white, "%d", count[i]);
  } else {
   mvwprintz(w, i + 1, 40, types[i]->color, "%c %s", types[i]->sym,
             types[i]->name.c_str());
   int hori = 65;
   if (count[i] >= 10) hori = 64;
   if (count[i] >= 100) hori = 63;
   if (count[i] >= 1000) hori = 62;
   mvwprintz(w, i + 1, hori, c_white, "%d", count[i]);
  }
 }

 wrefresh(w);
 getch();
 werase(w);
 wrefresh(w);
 delwin(w);
 refresh_all();
}

void game::disp_NPCs()
{
 WINDOW* w = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 mvwprintz(w, 0, 0, c_white, "Your position: %d:%d", lev.x, lev.y);
 std::vector<npc*> closest;
 closest.push_back(&cur_om.npcs[0]);
 for (int i = 1; i < cur_om.npcs.size(); i++) {
  if (closest.size() < VIEW-5)
   closest.push_back(&cur_om.npcs[i]);
  else if (rl_dist(lev.x, lev.y, cur_om.npcs[i].mapx, cur_om.npcs[i].mapy) <
           rl_dist(lev.x, lev.y, closest[VIEW - 6]->mapx, closest[VIEW - 6]->mapy)) {
   for (int j = 0; j < VIEW - 5; j++) {
    if (rl_dist(lev.x, lev.y, closest[j]->mapx, closest[j]->mapy) >
        rl_dist(lev.x, lev.y, cur_om.npcs[i].mapx, cur_om.npcs[i].mapy)) {
     closest.insert(closest.begin() + j, &cur_om.npcs[i]);
     closest.erase(closest.end() - 1);
     j = VIEW - 5;
    }
   }
  }
 }
 for (int i = 0; i < VIEW - 5; i++)
  mvwprintz(w, i + 2, 0, c_white, "%s: %d:%d", closest[i]->name.c_str(),
            closest[i]->mapx, closest[i]->mapy);

 wrefresh(w);
 getch();
 werase(w);
 wrefresh(w);
 delwin(w);
}

faction* game::list_factions(std::string title)
{
 std::vector<faction> valfac;	// Factions that we know of.
 for (int i = 0; i < factions.size(); i++) {
  if (factions[i].known_by_u)
   valfac.push_back(factions[i]);
 }
 if (valfac.size() == 0) {	// We don't know of any factions!
  popup("You don't know of any factions.  Press Spacebar...");
  return NULL;
 }
 WINDOW* w_list = newwin(VIEW,      MAX_FAC_NAME_SIZE, 0, 0);
 WINDOW* w_info = newwin(VIEW, SCREEN_WIDTH - MAX_FAC_NAME_SIZE, 0, MAX_FAC_NAME_SIZE);
 int maxlength = 79 - MAX_FAC_NAME_SIZE;
 int sel = 0;

// Init w_list content
 mvwprintz(w_list, 0, 0, c_white, title.c_str());
 for (int i = 0; i < valfac.size(); i++) {
  nc_color col = (i == 0 ? h_white : c_white);
  mvwprintz(w_list, i + 1, 0, col, valfac[i].name.c_str());
 }
 wrefresh(w_list);
// Init w_info content
// fac_*_text() is in faction.cpp
 mvwprintz(w_info, 0, 0, c_white,
          "Ranking: %s", fac_ranking_text(valfac[0].likes_u).c_str());
 mvwprintz(w_info, 1, 0, c_white,
          "Respect: %s", fac_respect_text(valfac[0].respects_u).c_str());
 std::string desc = valfac[0].describe();
 int linenum = 3;
 while (desc.length() > maxlength) {
  size_t split = desc.find_last_of(' ', maxlength);
  std::string line = desc.substr(0, split);
  mvwprintz(w_info, linenum, 0, c_white, line.c_str());
  desc = desc.substr(split + 1);
  linenum++;
 }
 mvwprintz(w_info, linenum, 0, c_white, desc.c_str());
 wrefresh(w_info);
 char ch;
 do {
  ch = input();
  switch ( ch ) {
  case 'j':	// Move selection down
   mvwprintz(w_list, sel + 1, 0, c_white, valfac[sel].name.c_str());
   if (sel == valfac.size() - 1)
    sel = 0;	// Wrap around
   else
    sel++;
   break;
  case 'k':	// Move selection up
   mvwprintz(w_list, sel + 1, 0, c_white, valfac[sel].name.c_str());
   if (sel == 0)
    sel = valfac.size() - 1;	// Wrap around
   else
    sel--;
   break;
  case KEY_ESCAPE:
  case 'q':
   sel = -1;
   break;
  }
  if (ch == 'j' || ch == 'k') {	// Changed our selection... update the windows
   mvwprintz(w_list, sel + 1, 0, h_white, valfac[sel].name.c_str());
   wrefresh(w_list);
   werase(w_info);
// fac_*_text() is in faction.cpp
   mvwprintz(w_info, 0, 0, c_white,
            "Ranking: %s", fac_ranking_text(valfac[sel].likes_u).c_str());
   mvwprintz(w_info, 1, 0, c_white,
            "Respect: %s", fac_respect_text(valfac[sel].respects_u).c_str());
   std::string desc = valfac[sel].describe();
   int linenum = 3;
   while (desc.length() > maxlength) {
    size_t split = desc.find_last_of(' ', maxlength);
    std::string line = desc.substr(0, split);
    mvwprintz(w_info, linenum, 0, c_white, line.c_str());
    desc = desc.substr(split + 1);
    linenum++;
   }
   mvwprintz(w_info, linenum, 0, c_white, desc.c_str());
   wrefresh(w_info);
  }
 } while (ch != KEY_ESCAPE && ch != '\n' && ch != 'q');
 werase(w_list);
 werase(w_info);
 delwin(w_list);
 delwin(w_info);
 refresh_all();
 if (sel == -1) return NULL;
 return &(factions[valfac[sel].id]);
}

void game::list_missions()
{
 WINDOW *w_missions = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 int tab = 0, selection = 0;
 char ch;
 do {
  werase(w_missions);
  draw_tabs(w_missions, tab, "ACTIVE MISSIONS", "COMPLETED MISSIONS",
            "FAILED MISSIONS", NULL);
  std::vector<int> umissions;
  switch (tab) {
   case 0: umissions = u.active_missions;	break;
   case 1: umissions = u.completed_missions;	break;
   case 2: umissions = u.failed_missions;	break;
  }
  for (int y = 3; y < VIEW; y++)
   mvwputch(w_missions, y, 30, c_white, LINE_XOXO);
  for (int i = 0; i < umissions.size(); i++) {
   mission * const miss = find_mission(umissions[i]);
   const nc_color col = (i == u.active_mission && tab == 0) ? c_ltred : c_white;
   mvwprintz(w_missions, 3 + i, 0, ((selection == i) ? hilite(col) : col), miss->name().c_str());
  }

  if (selection >= 0 && selection < umissions.size()) {
   mission *miss = find_mission(umissions[selection]);
   mvwprintz(w_missions, 4, 31, c_white,
             miss->description.c_str());
   if (miss->deadline != 0)
    mvwprintz(w_missions, 5, 31, c_white, "Deadline: %d (%d)",
              miss->deadline, int(messages.turn));
   mvwprintz(w_missions, 6, 31, c_white, "Target: (%d, %d)   You: (%d, %d)",
             miss->target.x, miss->target.y,
             (lev.x + int (MAPSIZE / 2)) / 2, (lev.y + int (MAPSIZE / 2)) / 2);
  } else {
   std::string nope;
   switch (tab) {
    case 0: nope = "You have no active missions!"; break;
    case 1: nope = "You haven't completed any missions!"; break;
    case 2: nope = "You haven't failed any missions!"; break;
   }
   mvwprintz(w_missions, 4, 31, c_ltred, nope.c_str());
  }

  wrefresh(w_missions);
  ch = input();
  switch (ch) {
  case '>':
   tab++;
   if (tab == 3) tab = 0;
   break;
  case '<':
   tab--;
   if (tab < 0) tab = 2;
   break;
  case 'j':
   selection++;
   if (selection >= umissions.size())
    selection = 0;
   break;
  case 'k':
   selection--;
   if (selection < 0)
    selection = umissions.size() - 1;
   break;
  case '\n':
   u.active_mission = selection;
   break;
  }

 } while (ch != 'q' && ch != 'Q' && ch != KEY_ESCAPE);


 werase(w_missions);
 delwin(w_missions);
 refresh_all();
}

void game::draw()
{
 static const char* const season_name[4] = { "Spring", "Summer", "Autumn", "Winter" };

 // Draw map
 werase(w_terrain);
 draw_ter();
 draw_footsteps();
 mon_info();
 // Draw Status
 draw_HP();
 werase(w_status);
 u.disp_status(w_status, this);
// TODO: Allow for a 24-hour option--already supported by calendar turn
 mvwprintz(w_status, 1, 41, c_white, messages.turn.print_time().c_str());

 {
 oter_id cur_ter = cur_om.ter((lev.x + int(MAPSIZE / 2)) / 2,
                              (lev.y + int(MAPSIZE / 2)) / 2);
 const auto& terrain = oter_t::list[cur_ter];
 std::string tername(terrain.name);
 if (tername.length() > 14) tername = tername.substr(0, 14);
 mvwprintz(w_status, 0,  0, terrain.color, tername.c_str());
 }
 if (lev.z < 0)
  mvwprintz(w_status, 0, 18, c_ltgray, "Underground");
 else
  mvwprintz(w_status, 0, 18, weather_datum::data[weather].color,
                             weather_datum::data[weather].name.c_str());
 nc_color col_temp = c_blue;
 if (temperature >= 90)
  col_temp = c_red;
 else if (temperature >= 75)
  col_temp = c_yellow;
 else if (temperature >= 60)
  col_temp = c_ltgreen;
 else if (temperature >= 50)
  col_temp = c_cyan;
 else if (temperature >  32)
  col_temp = c_ltblue;
 if (OPTIONS[OPT_USE_CELSIUS])
  wprintz(w_status, col_temp, " %dC", int((temperature - 32) / 1.8));
 else
  wprintz(w_status, col_temp, " %dF", temperature);
 mvwprintz(w_status, 0, 41, c_white, "%s, day %d",
           season_name[messages.turn.season], messages.turn.day + 1);
 if (run_mode != 0) mvwprintz(w_status, 2, 51, c_red, "SAFE");
 wrefresh(w_status);
 // Draw messages
 write_msg();
}

bool game::isBetween(int test, int down, int up)
{
	if(test>down && test<up) return true;
	else return false;
}

void game::draw_ter(int posx, int posy)
{
// posx/posy default to -999
 if (posx == -999) posx = u.posx;
 if (posy == -999) posy = u.posy;
 m.draw(this, w_terrain, point(posx, posy));

 // Draw monsters
 for (int i = 0; i < z.size(); i++) {
  point dist(abs(z[i].pos.x - posx), abs(z[i].pos.y - posy));
  if (dist.x <= SEEX && dist.y <= SEEY && u_see(&(z[i])))
   z[i].draw(w_terrain, posx, posy, false);
  else if (z[i].has_flag(MF_WARM) && dist.x <= SEEX && dist.y <= SEEY &&
           (u.has_active_bionic(bio_infrared) || u.has_trait(PF_INFRARED)))
   mvwputch(w_terrain, SEEY + z[i].pos.y - posy, SEEX + z[i].pos.x - posx, c_red, '?');
 }
 // Draw NPCs
 for (int i = 0; i < active_npc.size(); i++) {
  point dist(abs(active_npc[i].posx - posx), abs(active_npc[i].posy - posy));
  if (dist.x <= SEEX && dist.y <= SEEY && u_see(active_npc[i].posx, active_npc[i].posy))
   active_npc[i].draw(w_terrain, posx, posy, false);
 }
 if (u.has_active_bionic(bio_scent_vision)) {
  for (int realx = posx - SEEX; realx <= posx + SEEX; realx++) {
   for (int realy = posy - SEEY; realy <= posy + SEEY; realy++) {
    if (scent(realx, realy) != 0) {
     int tempx = posx - realx, tempy = posy - realy;
     if (!(isBetween(tempx, -2, 2) && isBetween(tempy, -2, 2))) {
      if (mon_at(realx, realy) != -1)
       mvwputch(w_terrain, realy + SEEY - posy, realx + SEEX - posx, c_white, '?');
      else
       mvwputch(w_terrain, realy + SEEY - posy, realx + SEEX - posx, c_magenta, '#');
     }
    }
   }
  }
 }
 wrefresh(w_terrain);
 if (u.has_disease(DI_VISUALS))
  hallucinate();
}

void game::refresh_all()
{
 draw();
 draw_minimap();
 //wrefresh(w_HP);
 draw_HP();
 wrefresh(w_moninfo);
 wrefresh(w_messages);
 refresh();
}

void game::draw_HP()
{
 int curhp;
 nc_color col;
 for (int i = 0; i < num_hp_parts; i++) {
  curhp = u.hp_cur[i];
       if (curhp == u.hp_max[i])
   col = c_green;
  else if (curhp > u.hp_max[i] * .8)
   col = c_ltgreen;
  else if (curhp > u.hp_max[i] * .5)
   col = c_yellow;
  else if (curhp > u.hp_max[i] * .3)
   col = c_ltred;
  else
   col = c_red;
  if (u.has_trait(PF_HPIGNORANT)) {
   mvwprintz(w_HP, i * 2 + 1, 0, col, " ***  ");
  } else {
   if (curhp >= 100)
    mvwprintz(w_HP, i * 2 + 1, 0, col, "%d     ", curhp);
   else if (curhp >= 10)
    mvwprintz(w_HP, i * 2 + 1, 0, col, " %d    ", curhp);
   else
    mvwprintz(w_HP, i * 2 + 1, 0, col, "  %d    ", curhp);
  }
 }
 mvwprintz(w_HP,  0, 0, c_ltgray, "HEAD:  ");
 mvwprintz(w_HP,  2, 0, c_ltgray, "TORSO: ");
 mvwprintz(w_HP,  4, 0, c_ltgray, "L ARM: ");
 mvwprintz(w_HP,  6, 0, c_ltgray, "R ARM: ");
 mvwprintz(w_HP,  8, 0, c_ltgray, "L LEG: ");
 mvwprintz(w_HP, 10, 0, c_ltgray, "R LEG: ");
 mvwprintz(w_HP, 12, 0, c_ltgray, "POW:   ");
 if (u.max_power_level == 0)
  mvwprintz(w_HP, 13, 0, c_ltgray, " --   ");
 else {
  if (u.power_level == u.max_power_level)
   col = c_blue;
  else if (u.power_level >= u.max_power_level * .5)
   col = c_ltblue;
  else if (u.power_level > 0)
   col = c_yellow;
  else
   col = c_red;
  if (u.power_level >= 100)
   mvwprintz(w_HP, 13, 0, col, "%d     ", u.power_level);
  else if (u.power_level >= 10)
   mvwprintz(w_HP, 13, 0, col, " %d    ", u.power_level);
  else
   mvwprintz(w_HP, 13, 0, col, "  %d    ", u.power_level);
 }
 wrefresh(w_HP);
}

void game::draw_minimap()
{
 // Draw the box
 werase(w_minimap);
 mvwputch(w_minimap, 0, 0, c_white, LINE_OXXO);
 mvwputch(w_minimap, 0, 6, c_white, LINE_OOXX);
 mvwputch(w_minimap, 6, 0, c_white, LINE_XXOO);
 mvwputch(w_minimap, 6, 6, c_white, LINE_XOOX);
 for (int i = 1; i < 6; i++) {
  mvwputch(w_minimap, i, 0, c_white, LINE_XOXO);
  mvwputch(w_minimap, i, 6, c_white, LINE_XOXO);
  mvwputch(w_minimap, 0, i, c_white, LINE_OXOX);
  mvwputch(w_minimap, 6, i, c_white, LINE_OXOX);
 }

 int cursx = (lev.x + int(MAPSIZE / 2)) / 2;
 int cursy = (lev.y + int(MAPSIZE / 2)) / 2;

 bool drew_mission = false;
 point target(-1, -1);
 if (u.active_mission >= 0 && u.active_mission < u.active_missions.size())
  target = find_mission(u.active_missions[u.active_mission])->target;
 else
  drew_mission = true;

 if (target.x == -1)
  drew_mission = true;

 for (int i = -2; i <= 2; i++) {
  for (int j = -2; j <= 2; j++) {
   int omx = cursx + i;
   int omy = cursy + j;
   bool seen = false;
   oter_id cur_ter = ot_null;
   if (omx >= 0 && omx < OMAPX && omy >= 0 && omy < OMAPY) {
    cur_ter = cur_om.ter(omx, omy);
    seen    = cur_om.seen(omx, omy);
   } else if ((omx < 0 || omx >= OMAPX) && (omy < 0 || omy >= OMAPY)) {
    if (omx < 0) omx += OMAPX;
    else         omx -= OMAPX;
    if (omy < 0) omy += OMAPY;
    else         omy -= OMAPY;
    cur_ter = om_diag->ter(omx, omy);
    seen    = om_diag->seen(omx, omy);
   } else if (omx < 0 || omx >= OMAPX) {
    if (omx < 0) omx += OMAPX;
    else         omx -= OMAPX;
    cur_ter = om_hori->ter(omx, omy);
    seen    = om_hori->seen(omx, omy);
   } else if (omy < 0 || omy >= OMAPY) {
    if (omy < 0) omy += OMAPY;
    else         omy -= OMAPY;
    cur_ter = om_vert->ter(omx, omy);
    seen    = om_vert->seen(omx, omy);
   } else
    debugmsg("No data loaded! omx: %d omy: %d", omx, omy);
   nc_color ter_color = oter_t::list[cur_ter].color;
   long ter_sym = oter_t::list[cur_ter].sym;
   if (seen) {
    if (!drew_mission && target.x == omx && target.y == omy) {
     drew_mission = true;
     if (i != 0 || j != 0)
      mvwputch   (w_minimap, 3 + j, 3 + i, red_background(ter_color), ter_sym);
     else
      mvwputch_hi(w_minimap, 3,     3,     ter_color, ter_sym);
    } else if (i == 0 && j == 0)
     mvwputch_hi(w_minimap, 3,     3,     ter_color, ter_sym);
    else
     mvwputch   (w_minimap, 3 + j, 3 + i, ter_color, ter_sym);
   }
  }
 }

// Print arrow to mission if we have one!
 if (!drew_mission) {
  double slope;
  if (cursx != target.x)
   slope = double(target.y - cursy) / double(target.x - cursx);
  if (cursx == target.x || abs(slope) > 3.5 ) { // Vertical slope
   if (target.y > cursy)
    mvwputch(w_minimap, 6, 3, c_red, '*');
   else
    mvwputch(w_minimap, 0, 3, c_red, '*');
  } else {
   int arrowx = 3, arrowy = 3;
   if (abs(slope) >= 1.) { // y diff is bigger!
    arrowy = (target.y > cursy ? 6 : 0);
    arrowx = 3 + 3 * (target.y > cursy ? slope : (0 - slope));
    if (arrowx < 0)
     arrowx = 0;
    if (arrowx > 6)
     arrowx = 6;
   } else {
    arrowx = (target.x > cursx ? 6 : 0);
    arrowy = 3 + 3 * (target.x > cursx ? slope : (0 - slope));
    if (arrowy < 0)
     arrowy = 0;
    if (arrowy > 6)
     arrowy = 6;
   }
   mvwputch(w_minimap, arrowy, arrowx, c_red, '*');
  }
 }

 wrefresh(w_minimap);
}

void game::hallucinate()
{
 for (int i = 0; i <= SEEX * 2 + 1; i++) {
  for (int j = 0; j <= SEEY * 2 + 1; j++) {
   if (one_in(10)) {
    char ter_sym = ter_t::list[m.ter(i + rng(-2, 2), j + rng(-2, 2))].sym;
    nc_color ter_col = ter_t::list[m.ter(i + rng(-2, 2), j + rng(-2, 2))].color;
    mvwputch(w_terrain, j, i, ter_col, ter_sym);
   }
  }
 }
 wrefresh(w_terrain);
}

unsigned char game::light_level()
{
 int ret;
 if (lev.z < 0)	// Underground!
  ret = 1;
 else {
  ret = messages.turn.sunlight();
  ret -= weather_datum::data[weather].sight_penalty;
 }
 for (int i = 0; i < events.size(); i++) {
  if (events[i].type == EVENT_DIM) {
   int turns_left = events[i].turn - int(messages.turn);
   i = events.size();
   if (turns_left > 25)
    ret = (ret * (turns_left - 25)) / 25;
   else
    ret = (ret * (25 - turns_left)) / 25;
  }
 }
 int flashlight = u.active_item_charges(itm_flashlight_on);
 //int light = u.light_items();
 if (ret < 10 && flashlight > 0) {
/* additive so that low battery flashlights still increase the light level 
	rather than decrease it 						*/
  ret += flashlight;
  if (ret > 10)
   ret = 10;
 }
 if (ret < 8 && u.has_active_bionic(bio_flashlight))
  ret = 8;
 if (ret < 8 && event_queued(EVENT_ARTIFACT_LIGHT))
  ret = 8;
 if (ret < 4 && u.has_artifact_with(AEP_GLOW))
  ret = 4;
 if (ret < 1)
  ret = 1;
// The EVENT_DIM event slowly dims the sky, then relights it
// EVENT_DIM has an occurance date of turn + 50, so the first 25 dim it
 return ret;
}

faction* game::faction_by_id(int id)
{
 for (int i = 0; i < factions.size(); i++) {
  if (factions[i].id == id)
   return &(factions[i]);
 }
 return NULL;
}

faction* game::random_good_faction()
{
 std::vector<int> valid;
 for (int i = 0; i < factions.size(); i++) {
  if (factions[i].good >= 5)
   valid.push_back(i);
 }
 if (valid.size() > 0) {
  int index = valid[rng(0, valid.size() - 1)];
  return &(factions[index]);
 }
// No good factions exist!  So create one!
 faction newfac(assign_faction_id());
 do
  newfac.randomize();
 while (newfac.good < 5);
 newfac.id = factions.size();
 factions.push_back(newfac);
 return &(factions[factions.size() - 1]);
}

faction* game::random_evil_faction()
{
 std::vector<int> valid;
 for (int i = 0; i < factions.size(); i++) {
  if (factions[i].good <= -5)
   valid.push_back(i);
 }
 if (valid.size() > 0) {
  int index = valid[rng(0, valid.size() - 1)];
  return &(factions[index]);
 }
// No good factions exist!  So create one!
 faction newfac(assign_faction_id());
 do
  newfac.randomize();
 while (newfac.good > -5);
 newfac.id = factions.size();
 factions.push_back(newfac);
 return &(factions[factions.size() - 1]);
}

bool game::sees_u(int x, int y)
{
 return (!u.has_active_bionic(bio_cloak) &&
         !u.has_artifact_with(AEP_INVISIBLE) && 
         m.sees(x, y, u.posx, u.posy, light_level()));
}

bool game::sees_u(int x, int y, int &t)
{
	return (!u.has_active_bionic(bio_cloak) &&
		!u.has_artifact_with(AEP_INVISIBLE) &&
		m.sees(x, y, u.posx, u.posy, light_level(), t));
}

bool game::u_see(int x, int y)
{
 int range = u.sight_range(light_level());
 if (u.has_artifact_with(AEP_CLAIRVOYANCE)) {
  int crange = (range > u.clairvoyance() ? u.clairvoyance() : range);
  if (rl_dist(u.posx, u.posy, x, y) <= crange)
   return true;
 }
 return m.sees(u.posx, u.posy, x, y, range);
}

bool game::u_see(int x, int y, int &t)
{
	t = 0;	// safe default in case of clairvoyance
	int range = u.sight_range(light_level());
	if (u.has_artifact_with(AEP_CLAIRVOYANCE)) {
		int crange = (range > u.clairvoyance() ? u.clairvoyance() : range);
		if (rl_dist(u.posx, u.posy, x, y) <= crange)
			return true;
	}
	return m.sees(u.posx, u.posy, x, y, range, t);
}

bool game::u_see(const monster *mon)
{
 int dist = rl_dist(u.posx, u.posy, mon->pos.x, mon->pos.y);
 if (u.has_trait(PF_ANTENNAE) && dist <= 3) return true;
 if (mon->has_flag(MF_DIGS) && !u.has_active_bionic(bio_ground_sonar) && dist > 1)
  return false;	// Can't see digging monsters until we're right next to them
 const int range = u.sight_range(light_level());
 if (u.has_artifact_with(AEP_CLAIRVOYANCE)) {
  int crange = (range > u.clairvoyance() ? u.clairvoyance() : range);
  if (dist <= crange) return true;
 }
 return m.sees(u.posx, u.posy, mon->pos.x, mon->pos.y, range);
}

bool game::pl_sees(player *p, monster *mon)
{
 if (mon->has_flag(MF_DIGS) && !p->has_active_bionic(bio_ground_sonar) &&
     rl_dist(p->posx, p->posy, mon->pos.x, mon->pos.y) > 1)
  return false;	// Can't see digging monsters until we're right next to them
 return m.sees(p->posx, p->posy, mon->pos.x, mon->pos.y, p->sight_range(light_level()));
}

point game::find_item(item *it) const
{
 if (u.has_item(it)) return point(u.posx, u.posy);
 point ret = m.find_item(it);
 if (ret.x != -1 && ret.y != -1) return ret;
 for (int i = 0; i < active_npc.size(); i++) {
  for (size_t j = 0; j < active_npc[i].inv.size(); j++) {
   if (it == &(active_npc[i].inv[j]))
    return point(active_npc[i].posx, active_npc[i].posy);
  }
 }
 return point(-999, -999);
}

void game::remove_item(item *it)
{
 point ret;
 if (it == &u.weapon) {
  u.remove_weapon();
  return;
 }
 for (size_t i = 0; i < u.inv.size(); i++) {
  if (it == &u.inv[i]) {
   u.i_remn(i);
   return;
  }
 }
 for (int i = 0; i < u.worn.size(); i++) {
  if (it == &u.worn[i]) {
   u.worn.erase(u.worn.begin() + i);
   return;
  }
 }
 ret = m.find_item(it);
 if (ret.x != -1 && ret.y != -1) {
  auto& stack = m.i_at(ret);
  for (int i = 0; i < stack.size(); i++) {
   if (it == &stack[i]) {
    m.i_rem(ret, i);
    return;
   }
  }
 }
 for (auto& n_pc : active_npc) {
  if (it == &n_pc.weapon) {
   n_pc.remove_weapon();
   return;
  }
  for (size_t j = 0; j < n_pc.inv.size(); j++) {
   if (it == &n_pc.inv[j]) {
    n_pc.i_remn(j);
    return;
   }
  }
  for (int j = 0; j < n_pc.worn.size(); j++) {
   if (it == &n_pc.worn[j]) {
    n_pc.worn.erase(n_pc.worn.begin() + j);
    return;
   }
  }
 }
}

static bool vector_has(const std::vector<int>& vec, int test)
{
 for (int i = 0; i < vec.size(); i++) {
  if (vec[i] == test) return true;
 }
 return false;
}

void game::mon_info()
{
 werase(w_moninfo);
 int buff;
 int newseen = 0;
// 7 0 1	unique_types uses these indices;
// 6 8 2	0-7 are provide by direction_from()
// 5 4 3	8 is used for local monsters (for when we explain them below)
 std::vector<int> unique_types[9];
// dangerous_types tracks whether we should print in red to warn the player
 bool dangerous[8];
 for (int i = 0; i < 8; i++)
  dangerous[i] = false;

 direction dir_to_mon, dir_to_npc;
 for (const auto& mon : z) {
  if (u_see(&mon)) {
   bool mon_dangerous = false;
   if (mon.attitude(&u) == MATT_ATTACK || mon.attitude(&u) == MATT_FOLLOW) {
    mon_dangerous = true;
    newseen++;
   }

   dir_to_mon = direction_from(u.posx, u.posy, mon.pos.x, mon.pos.y);
   int index = (rl_dist(u.posx, u.posy, mon.pos.x, mon.pos.y) <= SEEX ? 8 : dir_to_mon);
   if (mon_dangerous && index < 8) dangerous[index] = true;

   if (!vector_has(unique_types[dir_to_mon], mon.type->id))
    unique_types[index].push_back(mon.type->id);
  }
 }
 for (int i = 0; i < active_npc.size(); i++) {
  if (u_see(active_npc[i].posx, active_npc[i].posy)) { // TODO: NPC invis
   if (active_npc[i].attitude == NPCATT_KILL) newseen++;
   point npcp(active_npc[i].posx, active_npc[i].posy);
   dir_to_npc = direction_from ( u.posx, u.posy, npcp.x, npcp.y );
   int index = (rl_dist(u.posx, u.posy, npcp.x, npcp.y) <= SEEX ? 8 : dir_to_npc);
   unique_types[index].push_back(-1 - i);
  }
 }

 if (newseen > mostseen) {
  cancel_activity_query("Monster spotted!");
  turnssincelastmon = 0;
  if (run_mode == 1) run_mode = 2;	// Stop movement!
 } else if (autosafemode) { // Auto-safemode
  turnssincelastmon++;
  if(turnssincelastmon >= 50 && run_mode == 0) run_mode = 1;
 }


 mostseen = newseen;
 int line = 0;
 nc_color tmpcol;
// Print the direction headings
// Reminder:
// 7 0 1	unique_types uses these indices;
// 6 8 2	0-7 are provide by direction_from()
// 5 4 3	8 is used for local monsters (for when we explain them below)
 mvwprintz(w_moninfo,  0,  0, (unique_types[7].empty() ?
           c_dkgray : (dangerous[7] ? c_ltred : c_ltgray)), "NW:");
 mvwprintz(w_moninfo,  0, 15, (unique_types[0].empty() ? 
           c_dkgray : (dangerous[0] ? c_ltred : c_ltgray)), "North:");
 mvwprintz(w_moninfo,  0, 33, (unique_types[1].empty() ? 
           c_dkgray : (dangerous[1] ? c_ltred : c_ltgray)), "NE:");
 mvwprintz(w_moninfo,  1,  0, (unique_types[6].empty() ?
           c_dkgray : (dangerous[6] ? c_ltred : c_ltgray)), "West:");
 mvwprintz(w_moninfo,  1, 31, (unique_types[2].empty() ?
           c_dkgray : (dangerous[2] ? c_ltred : c_ltgray)), "East:");
 mvwprintz(w_moninfo,  2,  0, (unique_types[5].empty() ?
           c_dkgray : (dangerous[5] ? c_ltred : c_ltgray)), "SW:");
 mvwprintz(w_moninfo,  2, 15, (unique_types[4].empty() ?
           c_dkgray : (dangerous[4] ? c_ltred : c_ltgray)), "South:");
 mvwprintz(w_moninfo,  2, 33, (unique_types[3].empty() ?
           c_dkgray : (dangerous[3] ? c_ltred : c_ltgray)), "SE:");

 for (int i = 0; i < 8; i++) {

  point pr;
  switch (i) {
   case 7: pr.y = 0; pr.x =  4; break;
   case 0: pr.y = 0; pr.x = 22; break;
   case 1: pr.y = 0; pr.x = 37; break;

   case 6: pr.y = 1; pr.x =  6; break;
   case 2: pr.y = 1; pr.x = 37; break;

   case 5: pr.y = 2; pr.x =  4; break;
   case 4: pr.y = 2; pr.x = 22; break;
   case 3: pr.y = 2; pr.x = 37; break;
  }

  for (int j = 0; j < unique_types[i].size() && j < 10; j++) {
   buff = unique_types[i][j];

   if (buff < 0) { // It's an NPC!
    switch (active_npc[(buff + 1) * -1].attitude) {
     case NPCATT_KILL:   tmpcol = c_red;     break;
     case NPCATT_FOLLOW: tmpcol = c_ltgreen; break;
     case NPCATT_DEFEND: tmpcol = c_green;   break;
     default:            tmpcol = c_pink;    break;
    }
    mvwputch (w_moninfo, pr.y, pr.x, tmpcol, '@');

   } else // It's a monster!  easier.
    mvwputch (w_moninfo, pr.y, pr.x, mtype::types[buff]->color, mtype::types[buff]->sym);

   pr.x++;
  }
  if (unique_types[i].size() > 10) // Couldn't print them all!
   mvwputch (w_moninfo, pr.y, pr.x - 1, c_white, '+');
 } // for (int i = 0; i < 8; i++)

// Now we print their full names!

 bool listed_it[num_monsters]; // Don't list any twice!
 for (int i = 0; i < num_monsters; i++)
  listed_it[i] = false;

 point pr(0, 4);

// Start with nearby zombies--that's the most important
// We stop if pr.y hits 10--i.e. we're out of space
 for (int i = 0; i < unique_types[8].size() && pr.y < 12; i++) {
  buff = unique_types[8][i];
// buff < 0 means an NPC!  Don't list those.
  if (buff >= 0 && !listed_it[buff]) {
   listed_it[buff] = true;
   const mtype* const type = mtype::types[buff];
   std::string name = type->name;
// + 2 for the "Z "
   if (pr.x + 2 + name.length() >= 48) { // We're too long!
    pr.y++;
    pr.x = 0;
   }
   if (pr.y < 12) { // Don't print if we've overflowed
    mvwputch (w_moninfo, pr.y, pr.x, type->color, type->sym);
    mvwprintz(w_moninfo, pr.y, pr.x + 2, type->danger(), name.c_str());
   }
// +4 for the "Z " and two trailing spaces
   pr.x += 4 + name.length();
  }
 }
// Now, if there's space, the rest of the monsters!
 for (int j = 0; j < 8 && pr.y < 12; j++) {
  for (int i = 0; i < unique_types[j].size() && pr.y < 12; i++) {
   buff = unique_types[j][i];
// buff < 0 means an NPC!  Don't list those.
   if (buff >= 0 && !listed_it[buff]) {
    listed_it[buff] = true;
	const mtype* const type = mtype::types[buff];
    std::string name = type->name;
// + 2 for the "Z "
    if (pr.x + 2 + name.length() >= 48) { // We're too long!
     pr.y++;
     pr.x = 0;
    }
    if (pr.y < 12) { // Don't print if we've overflowed
     mvwputch (w_moninfo, pr.y, pr.x, type->color, type->sym);
     mvwprintz(w_moninfo, pr.y, pr.x + 2, type->danger(), name.c_str());
    }
// +3 for the "Z " and a trailing space
    pr.x += 3 + name.length();
   }
  }
 }

 wrefresh(w_moninfo);
 refresh();
}

void game::z_erase(int z_index)
{
   z.erase(z.begin() + z_index);
   if (last_target == z_index) last_target = -1;
   else if (last_target > z_index) last_target--;
}

void game::cleanup_dead()
{
 int i = z.size();
 while(0 < i--) {
   if (z[i].dead || z[i].hp <= 0) z_erase(i);
 }

 i = active_npc.size();
 while(0 < i--) {
   if (active_npc[i].dead) active_npc.erase(active_npc.begin() + i);
 }
}

void game::monmove()
{
 cleanup_dead();
 for (int i = 0; i < z.size(); i++) {
  while (!z[i].dead && !z[i].can_move_to(m, z[i].pos.x, z[i].pos.y)) {
// If we can't move to our current position, assign us to a new one
   if (debugmon)
    debugmsg("%s can't move to its location! (%d:%d), %s", z[i].name().c_str(),
             z[i].pos.x, z[i].pos.y, m.tername(z[i].pos).c_str());
   bool okay = false;
   int xdir = rng(1, 2) * 2 - 3, ydir = rng(1, 2) * 2 - 3; // -1 or 1
   int startx = z[i].pos.x - 3 * xdir, endx = z[i].pos.x + 3 * xdir;
   int starty = z[i].pos.y - 3 * ydir, endy = z[i].pos.y + 3 * ydir;
   for (int x = startx; x != endx && !okay; x += xdir) {
    for (int y = starty; y != endy && !okay; y += ydir){
     if (z[i].can_move_to(m, x, y)) {
      z[i].pos = point(x, y);
      okay = true;
     }
    }
   }
   if (!okay) z[i].dead = true;
  }

  if (!z[i].dead) {
   z[i].process_effects(this);
   if (z[i].hurt(0)) kill_mon(i);
  }

  m.mon_in_field(z[i].pos.x, z[i].pos.y, this, &(z[i]));

  while (z[i].moves > 0 && !z[i].dead) {
   z[i].made_footstep = false;
   z[i].plan(this);	// Formulate a path to follow
   z[i].move(this);	// Move one square, possibly hit u
   z[i].process_triggers(this);
   m.mon_in_field(z[i].pos.x, z[i].pos.y, this, &(z[i]));
   if (z[i].hurt(0)) kill_mon(i);	// Maybe we died...
  }

  if (!z[i].dead) {
   if (u.has_active_bionic(bio_alarm) && u.power_level >= 1 &&
       rl_dist(u.posx, u.posy, z[i].pos.x, z[i].pos.y) <= 5) {
    u.power_level--;
	messages.add("Your motion alarm goes off!");
    cancel_activity_query("Your motion alarm goes off!");
    if (u.has_disease(DI_SLEEP) || u.has_disease(DI_LYING_DOWN)) {
     u.rem_disease(DI_SLEEP);
     u.rem_disease(DI_LYING_DOWN);
    }
   }
// We might have stumbled out of range of the player; if so, kill us
   if (z[i].pos.x < 0 - (SEEX * MAPSIZE) / 6 ||
       z[i].pos.y < 0 - (SEEY * MAPSIZE) / 6 ||
       z[i].pos.x > (SEEX * MAPSIZE * 7) / 6 ||
       z[i].pos.y > (SEEY * MAPSIZE * 7) / 6   ) {
// Re-absorb into local group, if applicable
    int group = valid_group((mon_id)(z[i].type->id), lev.x, lev.y);
    if (group != -1) {
     cur_om.zg[group].population++;
     if (cur_om.zg[group].population / pow(cur_om.zg[group].radius, 2.0) > 5)
      cur_om.zg[group].radius++;
    } else if (mongroup::to_mc((mon_id)(z[i].type->id)) != mcat_null) {
     cur_om.zg.push_back(mongroup(mongroup::to_mc((mon_id)(z[i].type->id)),
                                  lev.x, lev.y, 1, 1));
	} else {
	  const auto m_cat = mongroup::to_mc((mon_id)(z[i].type->id));
	  if (m_cat != mcat_null) cur_om.zg.push_back(mongroup(m_cat, lev.x, lev.y, 1, 1));
	}
    z[i].dead = true;
   } else z[i].receive_moves();
  }
 }

 cleanup_dead();

// Now, do active NPCs.
 for (int i = 0; i < active_npc.size(); i++) {
  int turns = 0;
  if(active_npc[i].hp_cur[hp_head] <= 0 || active_npc[i].hp_cur[hp_torso] <= 0)
   active_npc[i].die(this);
  else {
   active_npc[i].reset(this);
   active_npc[i].suffer(this);
   while (!active_npc[i].dead && active_npc[i].moves > 0 && turns < 10) {	// 10 moves in one turn is lethal; assuming this is a bug intercept
    turns++;
    active_npc[i].move(this);
   }
   if (turns == 10) {
    messages.add("%s's brain explodes!", active_npc[i].name.c_str());
    active_npc[i].die(this);
   }
  }
 }
 cleanup_dead();
}

void game::om_npcs_move()
{
/*
 for (int i = 0; i < cur_om.npcs.size(); i++) {
  cur_om.npcs[i].perform_mission(this);
  if (abs(cur_om.npcs[i].mapx - levx) <= 1 &&
      abs(cur_om.npcs[i].mapy - levy) <= 1   ) {
   cur_om.npcs[i].posx = u.posx + SEEX * 2 * (cur_om.npcs[i].mapx - levx);
   cur_om.npcs[i].posy = u.posy + SEEY * 2 * (cur_om.npcs[i].mapy - levy);
   active_npc.push_back(cur_om.npcs[i]);
   cur_om.npcs.erase(cur_om.npcs.begin() + i);
   i--;
  }
 }
*/
}

void game::check_warmth()
{
/*
 // HEAD
 int warmth = u.warmth(bp_head) + int((temperature - 65) / 10);
 if (warmth <= -6) {
  add_msg("Your head is freezing!");
  u.add_disease(DI_COLD, abs(warmth * 2), this);// Heat loss via head is bad
  u.hurt(this, bp_head, 0, rng(0, abs(warmth / 3)));
 } else if (warmth <= -3) {
  add_msg("Your head is cold.");
  u.add_disease(DI_COLD, abs(warmth * 2), this);
 } else if (warmth >= 8) {
  add_msg("Your head is overheating!");
  u.add_disease(DI_HOT, warmth * 1.5, this);
 }
 // FACE -- Mouth and eyes
 warmth = u.warmth(bp_eyes) + u.warmth(bp_mouth) + int((temperature - 65) / 10);
 if (warmth <= -6) {
  add_msg("Your face is freezing!");
  u.add_disease(DI_COLD_FACE, abs(warmth), this);
  u.hurt(this, bp_head, 0, rng(0, abs(warmth / 3)));
 } else if (warmth <= -4) {
  add_msg("Your face is cold.");
  u.add_disease(DI_COLD_FACE, abs(warmth), this);
 } else if (warmth >= 12) {
  add_msg("Your face is overheating!");
  u.add_disease(DI_HOT, warmth, this);
 }
 // TORSO
 warmth = u.warmth(bp_torso) + int((temperature - 65) / 10);
 if (warmth <= -8) {
  add_msg("Your body is freezing!");
  u.add_disease(DI_COLD, abs(warmth), this);
  u.hurt(this, bp_torso, 0, rng(0, abs(warmth / 4)));
 } else if (warmth <= -2) {
  add_msg("Your body is cold.");
  u.add_disease(DI_COLD, abs(warmth), this);
 } else if (warmth >= 12) {
  add_msg("Your body is too hot."); 
  u.add_disease(DI_HOT, warmth * 2, this);
 }
 // HANDS
 warmth = u.warmth(bp_hands) + int((temperature - 65) / 10);
 if (warmth <= -4) {
  add_msg("Your hands are freezing!");
  u.add_disease(DI_COLD_HANDS, abs(warmth), this);
 } else if (warmth >= 8) {
  add_msg("Your hands are overheating!");
  u.add_disease(DI_HOT, rng(0, warmth / 2), this);
 }
 // LEGS
 warmth = u.warmth(bp_legs) + int((temperature - 65) / 10);
 if (warmth <= -6) {
  add_msg("Your legs are freezing!");
  u.add_disease(DI_COLD_LEGS, abs(warmth), this);
 } else if (warmth <= -3) {
  add_msg("Your legs are very cold.");
  u.add_disease(DI_COLD_LEGS, abs(warmth), this);
 } else if (warmth >= 8) {
  add_msg("Your legs are overheating!");
  u.add_disease(DI_HOT, rng(0, warmth), this);
 }
 // FEET
 warmth = u.warmth(bp_feet) + int((temperature - 65) / 10);
 if (warmth <= -3) {
  add_msg("Your feet are freezing!");
  u.add_disease(DI_COLD_FEET, warmth, this);
 } else if (warmth >= 12) {
  add_msg("Your feet are overheating!");
  u.add_disease(DI_HOT, rng(0, warmth), this);
 }
*/
}

void game::sound(int x, int y, int vol, std::string description)
{
 vol *= 1.5; // Scale it a little	\todo looks like this doesn't work; try *=3 /=2 instead
// First, alert all monsters (that can hear) to the sound
 for (int i = 0; i < z.size(); i++) {
  if (z[i].can_hear()) {
   int dist = rl_dist(x, y, z[i].pos);
   int volume = vol - (z[i].has_flag(MF_GOODHEARING) ? int(dist / 2) : dist);
   z[i].wander_to(x, y, volume);
   z[i].process_trigger(MTRIG_SOUND, volume);
  }
 }
// Loud sounds make the next spawn sooner!
 int spawn_range = int(MAPSIZE / 2) * SEEX;
 if (vol >= spawn_range) {
  int max = (vol - spawn_range);
  int min = int(max / 6);
  if (max > spawn_range * 4) max = spawn_range * 4;
  if (min > spawn_range * 4) min = spawn_range * 4;
  int change = rng(min, max);
  if (nextspawn < change) nextspawn = 0;
  else nextspawn -= change;
 }
// Next, display the sound as the player hears it
 if (description == "") return;	// No description (e.g., footsteps)
 if (u.has_disease(DI_DEAF)) return;	// We're deaf, can't hear it

 if (u.has_bionic(bio_ears)) vol *= 3.5;
 if (u.has_trait(PF_BADHEARING)) vol *= .5;
 if (u.has_trait(PF_CANINE_EARS)) vol *= 1.5;
 int dist = rl_dist(x, y, u.posx, u.posy);
 if (dist > vol) return;	// Too far away, we didn't hear it!
 if (u.has_disease(DI_SLEEP) &&
     ((!u.has_trait(PF_HEAVYSLEEPER) && dice(2, 20) < vol - dist) ||
      ( u.has_trait(PF_HEAVYSLEEPER) && dice(3, 20) < vol - dist)   )) {
  u.rem_disease(DI_SLEEP);
  messages.add("You're woken up by a noise.");
  return;
 }
 if (!u.has_bionic(bio_ears) && rng( (vol - dist) / 2, (vol - dist) ) >= 150) {
  int duration = (vol - dist - 130) / 4;
  if (duration > 40) duration = 40;
  u.add_disease(DI_DEAF, duration, this);
 }
 if (x != u.posx || y != u.posy)
  cancel_activity_query("Heard %s!", (description == "" ? "a noise" : description.c_str()));
// We need to figure out where it was coming from, relative to the player
 int dx = x - u.posx;
 int dy = y - u.posy;
// If it came from us, don't print a direction
 if (dx == 0 && dy == 0) {
  if (description[0] >= 'a' && description[0] <= 'z')
   description[0] += 'A' - 'a';	// Capitalize the sound
  messages.add("%s", description.c_str());
  return;
 }
 messages.add("From the %s you hear %s", direction_name(direction_from(u.posx, u.posy, x, y)), description.c_str());
}

// add_footstep will create a list of locations to draw monster
// footsteps. these will be more or less accurate depending on the
// characters hearing and how close they are
void game::add_footstep(int x, int y, int volume, int distance)
{
 if (x == u.posx && y == u.posy) return;
 else if (u_see(x, y)) return;
 int err_offset;
 if (volume / distance < 2)
  err_offset = 3;
 else if (volume / distance < 3)
  err_offset = 2;
 else
  err_offset = 1;
 if (u.has_bionic(bio_ears)) err_offset--;
 if (u.has_trait(PF_BADHEARING)) err_offset++;

 int tries = 0, origx = x, origy = y;
 if (err_offset > 0) {
  do {
   tries++;
   x = origx + rng(-err_offset, err_offset);
   y = origy + rng(-err_offset, err_offset);
  } while (tries < 10 && ((x == u.posx && y == u.posy)) || u_see(x, y));
 }
 if (tries < 10)
  footsteps.push_back(point(x, y));
 return;
}

// draws footsteps that have been created by monsters moving about
void game::draw_footsteps()
{
 for (int i = 0; i < footsteps.size(); i++) {
  mvwputch(w_terrain, SEEY + footsteps[i].y - u.posy, 
           SEEX + footsteps[i].x - u.posx, c_yellow, '?');
 }
 footsteps.clear();
 wrefresh(w_terrain);
 return;
}

void game::explosion(int x, int y, int power, int shrapnel, bool fire)
{
 timespec ts;	// Timespec for the animation of the explosion
 ts.tv_sec = 0;
 ts.tv_nsec = EXPLOSION_SPEED;
 int radius = sqrt(double(power / 4));
 int dam;
 if (power >= 30)
  sound(x, y, power * 10, "a huge explosion!");
 else
  sound(x, y, power * 10, "an explosion!");
 for (int i = x - radius; i <= x + radius; i++) {
  for (int j = y - radius; j <= y + radius; j++) {
   if (i == x && j == y)
    dam = 3 * power;
   else
    dam = 3 * power / (rl_dist(x, y, i, j));
   if (m.has_flag(bashable, i, j)) m.bash(i, j, dam);
   if (m.has_flag(bashable, i, j)) m.bash(i, j, dam); // Double up for tough doors, etc.
   if (m.is_destructable(i, j) && rng(25, 100) < dam) m.destroy(this, i, j, false);

   monster* const m_hit = mon(i, j);
   if (m_hit && m_hit->hurt(rng(dam / 2, dam * 1.5))) {
    if (m_hit->hp < 0 - 1.5 * m_hit->type->hp)
     explode_mon(*m_hit); // Explode them if it was big overkill
    else
     kill_mon(*m_hit); // TODO: player's fault?

    int vpart;
    vehicle* const veh = m.veh_at(i, j, vpart);
    if (veh) veh->damage (vpart, dam, false);
   }

   if (npc* const _npc = nPC(i,j)) {
    _npc->hit(this, bp_torso, 0, rng(dam / 2, dam * 1.5), 0);
	_npc->hit(this, bp_head,  0, rng(dam / 3, dam),       0);
	_npc->hit(this, bp_legs,  0, rng(dam / 3, dam),       0);
	_npc->hit(this, bp_legs,  1, rng(dam / 3, dam),       0);
	_npc->hit(this, bp_arms,  0, rng(dam / 3, dam),       0);
	_npc->hit(this, bp_arms,  1, rng(dam / 3, dam),       0);
    if (_npc->hp_cur[hp_head]  <= 0 || _npc->hp_cur[hp_torso] <= 0) _npc->die(this, true);
   }
   if (u.posx == i && u.posy == j) {
    messages.add("You're caught in the explosion!");
    u.hit(this, bp_torso, 0, rng(dam / 2, dam * 1.5), 0);
    u.hit(this, bp_head,  0, rng(dam / 3, dam),       0);
    u.hit(this, bp_legs,  0, rng(dam / 3, dam),       0);
    u.hit(this, bp_legs,  1, rng(dam / 3, dam),       0);
    u.hit(this, bp_arms,  0, rng(dam / 3, dam),       0);
    u.hit(this, bp_arms,  1, rng(dam / 3, dam),       0);
   }
   if (fire) {
	auto& f = m.field_at(i, j);
    if (f.type == fd_smoke) f = field(fd_fire);
    m.add_field(this, i, j, fd_fire, dam / 10);
   }
  }
 }
// Draw the explosion
 for (int i = 1; i <= radius; i++) {
  mvwputch(w_terrain, y - i + SEEY - u.posy, x - i + SEEX - u.posx, c_red, '/');
  mvwputch(w_terrain, y - i + SEEY - u.posy, x + i + SEEX - u.posx, c_red,'\\');
  mvwputch(w_terrain, y + i + SEEY - u.posy, x - i + SEEX - u.posx, c_red,'\\');
  mvwputch(w_terrain, y + i + SEEY - u.posy, x + i + SEEX - u.posx, c_red, '/');
  for (int j = 1 - i; j < 0 + i; j++) {
   mvwputch(w_terrain, y - i + SEEY - u.posy, x + j + SEEX - u.posx, c_red,'-');
   mvwputch(w_terrain, y + i + SEEY - u.posy, x + j + SEEX - u.posx, c_red,'-');
   mvwputch(w_terrain, y + j + SEEY - u.posy, x - i + SEEX - u.posx, c_red,'|');
   mvwputch(w_terrain, y + j + SEEY - u.posy, x + i + SEEX - u.posx, c_red,'|');
  }
  wrefresh(w_terrain);
  nanosleep(&ts, NULL);
 }

// The rest of the function is shrapnel
 if (shrapnel <= 0) return;
 int sx, sy, t, tx, ty;
 std::vector<point> traj;
 ts.tv_sec = 0;
 ts.tv_nsec = BULLET_SPEED;	// Reset for animation of bullets
 for (int i = 0; i < shrapnel; i++) {
  sx = rng(x - 2 * radius, x + 2 * radius);
  sy = rng(y - 2 * radius, y + 2 * radius);
  traj = line_to(x, y, sx, sy, (m.sees(x, y, sx, sy, 50, t) ? t : 0));
  dam = rng(20, 60);
  for (int j = 0; j < traj.size(); j++) {
   if (j > 0 && u_see(traj[j - 1]))
    m.drawsq(w_terrain, u, traj[j - 1].x, traj[j - 1].y, false, true);
   if (u_see(traj[j])) {
    mvwputch(w_terrain, traj[j].y + SEEY - u.posy,
                        traj[j].x + SEEX - u.posx, c_red, '`');
    wrefresh(w_terrain);
    nanosleep(&ts, NULL);
   }
   tx = traj[j].x;
   ty = traj[j].y;
   if (monster* const m_at = mon(traj[j])) {
    dam -= m_at->armor_cut();
    if (m_at->hurt(dam)) kill_mon(*m_at);
   } else if (npc* const _npc = nPC(traj[j])) {
    body_part hit = random_body_part();
    if (hit == bp_eyes || hit == bp_mouth || hit == bp_head) dam = rng(2 * dam, 5 * dam);
    else if (hit == bp_torso) dam = rng(1.5 * dam, 3 * dam);
	_npc->hit(this, hit, rng(0, 1), 0, dam);
    if (_npc->hp_cur[hp_head] <= 0 || _npc->hp_cur[hp_torso] <= 0) _npc->die(this);
   } else if (tx == u.posx && ty == u.posy) {
    body_part hit = random_body_part();
    int side = rng(0, 1);
	messages.add("Shrapnel hits your %s!", body_part_name(hit, side).c_str());
    u.hit(this, hit, rng(0, 1), 0, dam);
   } else
    m.shoot(this, tx, ty, dam, j == traj.size() - 1, 0);
  }
 }
}

void game::flashbang(int x, int y)
{
 int dist = rl_dist(u.posx, u.posy, x, y);
 if (dist <= 8) {
  if (!u.has_bionic(bio_ears))
   u.add_disease(DI_DEAF, 40 - dist * 4, this);
  if (m.sees(u.posx, u.posy, x, y, 8))
   u.infect(DI_BLIND, bp_eyes, (12 - dist) / 2, 10 - dist, this);
 }
 // TODO: Blind/deafen NPC
 for (auto& _mon : z) {
  dist = rl_dist(_mon.pos, x, y);
  if (8 < dist) continue;
  if (dist <= 4) _mon.add_effect(ME_STUNNED, 10 - dist);
  if (_mon.has_flag(MF_SEES) && m.sees(_mon.pos, x, y, 8)) _mon.add_effect(ME_BLIND, 18 - dist);
  if (_mon.has_flag(MF_HEARS)) _mon.add_effect(ME_DEAF, 60 - dist * 4);
 }
 sound(x, y, 12, "a huge boom!");
}

void game::use_computer(int x, int y)
{
 if (u.has_trait(PF_ILLITERATE)) {
  messages.add("You can not read a computer screen!");
  return;
 }
 computer* used = m.computer_at(x, y);

 if (used == NULL) {
  debugmsg("Tried to use computer at (%d, %d) - none there", x, y);
  return;
 }
 
 used->use(this);

 refresh_all();
}

void game::resonance_cascade(int x, int y)
{
 int maxglow = 100 - 5 * trig_dist(x, y, u.posx, u.posy);
 int minglow =  60 - 5 * trig_dist(x, y, u.posx, u.posy);
 if (minglow < 0)
  minglow = 0;
 if (maxglow > 0)
  u.add_disease(DI_TELEGLOW, rng(minglow, maxglow) * 100, this);
 int startx = (x < 8 ? 0 : x - 8), endx = (x+8 >= SEEX*3 ? SEEX*3 - 1 : x + 8);
 int starty = (y < 8 ? 0 : y - 8), endy = (y+8 >= SEEY*3 ? SEEY*3 - 1 : y + 8);
 for (int i = startx; i <= endx; i++) {
  for (int j = starty; j <= endy; j++) {
   switch (rng(1, 80)) {
   case 1:
   case 2:
    emp_blast(i, j);
    break;
   case 3:
   case 4:
   case 5:
    for (int k = i - 1; k <= i + 1; k++) {
     for (int l = j - 1; l <= j + 1; l++) {
      field_id type;
      switch (rng(1, 7)) {
       case 1: type = fd_blood;
       case 2: type = fd_bile;
       case 3:
       case 4: type = fd_slime;
       case 5: type = fd_fire;
       case 6:
       case 7: type = fd_nuke_gas;
      }
	  auto& f = m.field_at(k, l);
      if (f.type == fd_null || !one_in(3)) f = field(type, 3);
     }
    }
    break;
   case  6:
   case  7:
   case  8:
   case  9:
   case 10:
    m.tr_at(i, j) = tr_portal;
    break;
   case 11:
   case 12:
    m.tr_at(i, j) = tr_goo;
    break;
   case 13:
   case 14:
   case 15:
    z.push_back(monster(mtype::types[mongroup::moncats[mcat_nether][rng(0, mongroup::moncats[mcat_nether].size() - 1)]], i, j));
    break;
   case 16:
   case 17:
   case 18:
    m.destroy(this, i, j, true);
    break;
   case 19:
    explosion(i, j, rng(1, 10), rng(0, 1) * rng(0, 6), one_in(4));
    break;
   }
  }
 }
}

void game::emp_blast(int x, int y)
{
 int rn;
 if (m.has_flag(console, x, y)) {
  messages.add("The %s is rendered non-functional!", m.tername(x, y).c_str());
  m.ter(x, y) = t_console_broken;
  return;
 }
// TODO: More terrain effects.
 switch (m.ter(x, y)) {
 case t_card_science:
 case t_card_military:
  rn = rng(1, 100);
  if (rn > 92 || rn < 40) {
   messages.add("The card reader is rendered non-functional.");
   m.ter(x, y) = t_card_reader_broken;
  }
  if (rn > 80) {
   messages.add("The nearby doors slide open!");
   for (int i = -3; i <= 3; i++) {
    for (int j = -3; j <= 3; j++) {
	 m.rewrite<t_door_metal_locked, t_floor>(x + i, y + j);
    }
   }
  }
  if (rn >= 40 && rn <= 80) messages.add("Nothing happens.");
  break;
 }
 monster* const m_at = mon(x, y);
 if (m_at) {
  if (m_at->has_flag(MF_ELECTRONIC)) {
   messages.add("The EMP blast fries the %s!", m_at->name().c_str());
   int dam = dice(10, 10);
   if (m_at->hurt(dam))
    kill_mon(*m_at); // \todo Player's fault?
   else if (one_in(6))
    m_at->make_friendly();
  } else
   messages.add("The %s is unaffected by the EMP blast.", m_at->name().c_str());
 }
 if (u.posx == x && u.posy == y) {
  if (u.power_level > 0) {
   messages.add("The EMP blast drains your power.");
   int max_drain = (u.power_level > 40 ? 40 : u.power_level);
   u.charge_power(0 - rng(1 + max_drain / 3, max_drain));
  }
// TODO: More effects?
 }
// Drain any items of their battery charge
 for(auto& it : m.i_at(x, y)) {
  if (it.is_tool() && (dynamic_cast<const it_tool*>(it.type))->ammo == AT_BATT) it.charges = 0;
 }
// TODO: Drain NPC energy reserves
}

// 2019-02-15
// C:DDA analog for this family is game::critter_at<npc> for game::nPC, game::critter_at<monster> for game::mon; this is based on tripoint rather than global location
// C:DDA has no raw index returns
// C:DDA has a new Creature subclass
// it is much clearer what is obtained from the Creature class than the Character class (cf. visitor pattern template at visitable.h)
// diagram: Creature -> monster
//						Character -> player -> npc
int game::npc_at(int x, int y) const
{
 for (int i = 0; i < active_npc.size(); i++) {
  if (active_npc[i].posx == x && active_npc[i].posy == y && !active_npc[i].dead)
   return i;
 }
 return -1;
}

npc* game::nPC(int x, int y)
{
	for (auto& m : active_npc) {
		if (m.posx == x && m.posy == y && !m.dead) return &m;
	}
	return 0;
}

npc* game::nPC(const point& pt)
{
	for (auto& m : active_npc) {
		if (m.posx == pt.x && m.posy == pt.y && !m.dead) return &m;
	}
	return 0;
}

int game::mon_at(int x, int y) const
{
 for (int i = 0; i < z.size(); i++) {
  if (z[i].pos.x == x && z[i].pos.y == y && !z[i].dead) return i;
 }
 return -1;
}

monster* game::mon(int x, int y)
{
	for (auto& m : z) {
		if (m.pos.x == x && m.pos.y == y && !m.dead) return &m;
	}
	return 0;
}

monster* game::mon(const point& pt)
{
	for (auto& m : z) {
		if (m.pos == pt && !m.dead) return &m;
	}
	return 0;
}

bool game::is_empty(int x, int y)
{
 return ((m.move_cost(x, y) > 0 || m.has_flag(liquid, x, y)) &&
         npc_at(x, y) == -1 && mon_at(x, y) == -1 &&
         (u.posx != x || u.posy != y));
}

bool game::is_in_sunlight(int x, int y)
{
 return (m.is_outside(x, y) && light_level() >= 40 &&
         (weather == WEATHER_CLEAR || weather == WEATHER_SUNNY));
}

void game::kill_mon(monster& target, bool u_did_it)
{
 if (!target.dead) {
  target.dead = true;
  if (u_did_it) {
   if (target.has_flag(MF_GUILT)) {
    mdeath tmpdeath;
    tmpdeath.guilt(this, &target);
   }
   if (target.type->species != species_hallu)
    kills[target.type->id]++;	// Increment our kill counter
  }
  for (int i = 0; i < target.inv.size(); i++)
   m.add_item(target.pos.x, target.pos.y, target.inv[i]);
  target.die(this);
 }
// z_erase(index);	// highly unsafe, do this compaction at end-of-turn
}

void game::explode_mon(monster& target)
{
 if (!target.dead) {
  target.dead = true;
// Send body parts and blood all over!
  const mtype* const corpse = target.type;
  kills[corpse->id]++;	// Increment our kill counter
  const itype* const meat = corpse->chunk_material();
  if (!meat) return;	// no chunks when return value is NULL
  const int num_chunks = corpse->chunk_count();

  point pos(target.pos);
  for (int i = 0; i < num_chunks; i++) {
   point tar(pos.x + rng(-3, 3), pos.y + rng(-3, 3));
   std::vector<point> traj = line_to(pos, tar, 0);
 
   for (int j = 0; j < traj.size(); j++) {
    tar = traj[j];
// Choose a blood type and place it
    field_id blood_type = fd_blood;
    if (corpse->dies == &mdeath::boomer) blood_type = fd_bile;
    else if (corpse->dies == &mdeath::acid) blood_type = fd_acid;
	{
	auto& f = m.field_at(tar.x, tar.y);
    if (f.type == blood_type && f.density < 3) f.density++;
    else m.add_field(this, tar.x, tar.y, blood_type, 1);
	}

    if (m.move_cost(tar) == 0) {
     std::string tmp;
     if (m.bash(tar, 3, tmp)) sound(tar, 18, tmp);
     else {
      if (j > 0) tar = traj[j - 1];
	  break;
     }
    }
   }
   m.add_item(tar.x, tar.y, meat, messages.turn);
  }
 }

// z_erase(index);	// unclear whether this is a good idea (mon_kill postpones this)
}

void game::open()
{
 u.moves -= 100;
 bool didit = false;
 mvwprintw(w_terrain, 0, 0, "Open where? (hjklyubn) ");
 wrefresh(w_terrain);
 char ch = input();
 last_action += ch;
 point open(get_direction(ch));
 if (open.x != -2)
 {
  int vpart;
  vehicle * const veh = m.veh_at(u.posx + open.x, u.posy + open.y, vpart);
  if (veh && veh->part_flag(vpart, vpf_openable)) {
   if (veh->parts[vpart].open) {
    messages.add("That door is already open.");
    u.moves += 100;
   } else {
    veh->parts[vpart].open = 1;
    veh->insides_dirty = true;
   }
   return;
  }

  didit = m.open_door(u.posx + open.x, u.posy + open.y, (m.ter(u.posx, u.posy) == t_floor));
 }
 else
  messages.add("Invalid direction.");
 if (!didit) {
  switch(m.ter(u.posx + open.x, u.posy + open.y)) {
  case t_door_locked:
  case t_door_locked_alarm:
   messages.add("The door is locked!");
   break;	// Trying to open a locked door uses the full turn's movement
  case t_door_o:
   messages.add("That door is already open.");
   u.moves += 100;
   break;
  default:
   messages.add("No door there.");
   u.moves += 100;
  }
 }
}

void game::close()
{
 bool didit = false;
 mvwprintw(w_terrain, 0, 0, "Close where? (hjklyubn) ");
 wrefresh(w_terrain);
 char ch = input();
 last_action += ch;
 point close(get_direction(ch));
 if (-2 == close.x) { messages.add("Invalid direction."); return; }
 close.x += u.posx;
 close.y += u.posy;

 if (const monster* const m_at = mon(close)) { messages.add("There's a %s in the way!", m_at->name().c_str()); return; }

 int vpart;
 vehicle * const veh = m.veh_at(close, vpart);
 if (veh && veh->part_flag(vpart, vpf_openable) && veh->parts[vpart].open) {
   veh->parts[vpart].open = 0;
   veh->insides_dirty = true;
   didit = true;
 } else {
   const auto& stack = m.i_at(close);
   if (!stack.empty()) {
     messages.add("There's %s in the way!", stack.size() == 1 ? stack[0].tname(this).c_str() : "some stuff");
	 return;
   }
   if (close.x == u.posx && close.y == u.posy) { messages.add("There's some buffoon in the way!"); return; }
   didit = m.close_door(close.x, close.y);
 }
 if (didit) u.moves -= 90;
}

void game::smash()
{
 bool didit = false;
 std::string bashsound, extra;
 int smashskill = int(u.str_cur / 2.5 + u.weapon.type->melee_dam);
 mvwprintw(w_terrain, 0, 0, "Smash what? (hjklyubn) ");
 wrefresh(w_terrain);
 char ch = input();
 last_action += ch;
 if (ch == KEY_ESCAPE) {
  messages.add("Never mind.");
  return;
 }
 point smash(get_direction(ch));
 if (smash.x == -2) messages.add("Invalid direction.");
 else {
   // TODO: Move this elsewhere.
   if (m.has_flag(alarmed, u.posx + smash.x, u.posy + smash.y) && !event_queued(EVENT_WANTED)) {
     sound(u.posx, u.posy, 30, "An alarm sounds!");
     add_event(EVENT_WANTED, int(messages.turn) + 300, 0, lev.x, lev.y);
   }
   didit = m.bash(u.posx + smash.x, u.posy + smash.y, smashskill, bashsound);
 }
 if (didit) {
  if (extra != "") messages.add(extra.c_str());
  sound(u.posx, u.posy, 18, bashsound);
  u.moves -= 80;
  if (u.sklevel[sk_melee] == 0) u.practice(sk_melee, rng(0, 1) * rng(0, 1));
  if (u.weapon.made_of(GLASS) && rng(0, u.weapon.volume() + 3) < u.weapon.volume()) {
   messages.add("Your %s shatters!", u.weapon.tname(this).c_str());
   for (int i = 0; i < u.weapon.contents.size(); i++)
    m.add_item(u.posx, u.posy, u.weapon.contents[i]);
   sound(u.posx, u.posy, 16, "");
   u.hit(this, bp_hands, 1, 0, rng(0, u.weapon.volume()));
   if (u.weapon.volume() > 20)// Hurt left arm too, if it was big
    u.hit(this, bp_hands, 0, 0, rng(0, u.weapon.volume() * .5));
   u.remove_weapon();
  }
 } else
  messages.add("There's nothing there!");
}

void game::use_item()
{
 char ch = inv("Use item:");
 if (ch == KEY_ESCAPE) {
  messages.add("Never mind.");
  return;
 }
 last_action += ch;
 u.use(this, ch);
}

bool game::pl_choose_vehicle (int &x, int &y)
{
 refresh_all();
 mvprintz(0, 0, c_red, "Choose a vehicle at direction:");
 point dir(get_direction(input()));
 if (dir.x == -2) {
  messages.add("Invalid direction!");
  return false;
 }
 x += dir.x;
 y += dir.y;
 return true;
/*
int junk;
 int range = 3;
 int x0 = x - range;
 int y0 = y - range;
 int x1 = x + range;
 int y1 = y + range;
 for (int j = x - SEEX; j <= x + SEEX; j++) {
  for (int k = y - SEEY; k <= y + SEEY; k++) {
   if (u_see(j, k, junk)) {
    if (k >= y0 && k <= y1 && j >= x0 && j <= x1)
     m.drawsq(w_terrain, u, j, k, false, true);
    else
     mvwputch(w_terrain, k + SEEY - y, j + SEEX - x, c_dkgray, '#');
   }
  }
 }

 // target() sets x and y, and returns an empty vector if we canceled (Esc)
 std::vector <point> trajectory =
    target(x, y, x0, y0, x1, y1, std::vector<monster> (), junk, 0);
 return trajectory.size() > 0;
*/
}

bool game::vehicle_near ()
{
 for (int dx = -1; dx <= 1; dx++) {
  for (int dy = -1; dy <= 1; dy++) {
   if (m.veh_at(u.posx + dx, u.posy + dy)) return true;
  }
 }
 return false;
}

void game::handbrake ()
{
 vehicle *veh = m.veh_at (u.posx, u.posy);
 if (!veh) return;
 messages.add("You pull a handbrake.");
 veh->cruise_velocity = 0;
 if (veh->last_turn != 0 && rng (15, 60) * 100 < abs(veh->velocity)) {
  veh->skidding = true;
  messages.add("You lose control of %s.", veh->name.c_str());
  veh->turn (veh->last_turn > 0? 60 : -60);
 } else if (veh->velocity < 0)
  veh->stop();
 else {
  veh->velocity = veh->velocity / 2 - 10*100;
  if (veh->velocity < 0) veh->stop();
 }
 u.moves = 0;
}

void game::exam_vehicle(vehicle &veh, int cx, int cy)
{
    veh_interact vehint(cx, cy, this, &veh);	// if this breaks try 0,0 instead
    vehint.exec();
    if (vehint.sel_cmd != ' ')
    {   // TODO: different activity times
		const point o(veh.global());
        u.activity = player_activity(ACT_VEHICLE,
                                     vehint.sel_cmd == 'f'? 200 : 20000,
                                     (int) vehint.sel_cmd);
        u.activity.values.push_back (o.x);    // values[0]
        u.activity.values.push_back (o.y);    // values[1]
        u.activity.values.push_back (vehint.c.x);   // values[2]
        u.activity.values.push_back (vehint.c.y);   // values[3]
        u.activity.values.push_back (-vehint.dd.x - vehint.c.y);   // values[4]
        u.activity.values.push_back (vehint.c.x - vehint.dd.y);   // values[5]
        u.activity.values.push_back (vehint.sel_part); // values[6]
        u.moves = 0;
    }
    refresh_all();
}

void game::examine()
{
 if (u.in_vehicle) {
  int vpart;
  vehicle *veh = m.veh_at(u.posx, u.posy, vpart);
  bool qexv = (veh && (veh->velocity != 0 ?
                       query_yn("Really exit moving vehicle?") :
                       query_yn("Exit vehicle?")));
  if (qexv) {
   m.unboard_vehicle (this, u.posx, u.posy);
   u.moves -= 200;
   if (veh->velocity) {      // TODO: move player out of harms way
    int dsgn = veh->parts[vpart].mount_d.x > 0? 1 : -1;
    fling_player_or_monster (&u, 0, veh->face.dir() + 90 * dsgn, 35);
   }
   return;
  }
 }
 mvwprintw(w_terrain, 0, 0, "Examine where? (Direction button) ");
 wrefresh(w_terrain);
 char ch = input();
 last_action += ch;
 if (ch == KEY_ESCAPE || ch == 'e' || ch == 'q') return;
 point exam(get_direction(ch));
 if (exam.x == -2) {
  messages.add("Invalid direction.");
  return;
 }
 exam.x += u.posx;
 exam.y += u.posy;
 messages.add("That is a %s.", m.tername(exam).c_str());

 auto& exam_t = m.ter(exam);
 auto& stack = m.i_at(exam);

 int veh_part = 0;
 vehicle *veh = m.veh_at(exam, veh_part);
 if (veh) {
  int vpcargo = veh->part_with_feature(veh_part, vpf_cargo, false);
  if (vpcargo >= 0 && veh->parts[vpcargo].items.size() > 0) pickup(exam.x, exam.y, 0);
  else if (u.in_vehicle) messages.add("You can't do that while onboard.");
  else if (abs(veh->velocity) > 0) messages.add("You can't do that on moving vehicle.");
  else exam_vehicle (*veh, exam.x, exam.y);
 } else if (m.has_flag(sealed, exam)) {
  if (m.trans(exam.x, exam.y)) {
   std::string buff;
   if (stack.size() <= 3 && !stack.empty()) {
    buff = "It contains ";
    for (int i = 0; i < stack.size(); i++) {
     buff += stack[i].tname(this);
     if (i + 2 < stack.size()) buff += ", ";
     else if (i + 1 < stack.size()) buff += ", and ";
    }
    buff += ",";
   } else if (!stack.empty()) buff = "It contains many items,";
   buff += " but is firmly sealed.";
   messages.add(buff.c_str());
  } else {
   messages.add("There's something in there, but you can't see what it is, and the %s is firmly sealed.", m.tername(exam).c_str());
  }
 } else {
  if (stack.empty() && m.has_flag(container, exam) && !(m.has_flag(swimmable, exam) || t_toilet == exam_t))
   messages.add("It is empty.");
  else
   pickup(exam.x, exam.y, 0);
 }
 if (m.has_flag(console, exam.x, exam.y)) {
  use_computer(exam.x, exam.y);
  return;
 }
 if (t_card_science == exam_t || t_card_military == exam_t) {
  itype_id card_type = (t_card_science == exam_t ? itm_id_science : itm_id_military);
  if (u.has_amount(card_type, 1) && query_yn("Swipe your ID card?")) {
   u.moves -= 100;
   for (int i = -3; i <= 3; i++) {
    for (int j = -3; j <= 3; j++) {
	 m.rewrite<t_door_metal_locked, t_floor>(exam.x + i, exam.y + j);
    }
   }
   {	// relies on reality bubble size to work
   int i = z.size();
   while (0 < i--) {
     if (mon_turret == z[i].type->id) z_erase(i);
   }
   }
   messages.add("You insert your ID card.");
   messages.add("The nearby doors slide into the floor.");
   u.use_amount(card_type, 1);
  }
  bool using_electrohack = (u.has_amount(itm_electrohack, 1) &&
                            query_yn("Use electrohack on the reader?"));
  bool using_fingerhack = (!using_electrohack && u.has_bionic(bio_fingerhack) &&
                           u.power_level > 0 &&
                           query_yn("Use fingerhack on the reader?"));
  if (using_electrohack || using_fingerhack) {
   u.moves -= 500;
   u.practice(sk_computer, 20);
   int success = rng(u.sklevel[sk_computer]/4 - 2, u.sklevel[sk_computer] * 2);
   success += rng(-3, 3);
   if (using_fingerhack) success++;
   if (u.int_cur < 8)
    success -= rng(0, int((8 - u.int_cur) / 2));
   else if (u.int_cur > 8)
    success += rng(0, int((u.int_cur - 8) / 2));
   if (success < 0) {
    messages.add("You cause a short circuit!");
    if (success <= -5) {
     if (using_electrohack) {
      messages.add("Your electrohack is ruined!");
      u.use_amount(itm_electrohack, 1);
     } else {
      messages.add("Your power is drained!");
      u.charge_power(0 - rng(0, u.power_level));
     }
    }
	exam_t = t_card_reader_broken;
   } else if (success < 6)
    messages.add("Nothing happens.");
   else {
    messages.add("You activate the panel!");
	messages.add("The nearby doors slide into the floor.");
	exam_t = t_card_reader_broken;
    for (int i = -3; i <= 3; i++) {
     for (int j = -3; j <= 3; j++) {
	   m.rewrite<t_door_metal_locked, t_floor>(exam.x + i, exam.y + j);
     }
    }
   }
  }
 } else if (t_elevator_control == exam_t && query_yn("Activate elevator?")) {
  int movez = (lev.z < 0 ? 2 : -2);
  lev.z += movez;
  cur_om.save(u.name);
  overmap(this, cur_om.pos.x, cur_om.pos.y, -1);
  cur_om = overmap(this, cur_om.pos.x, cur_om.pos.y, cur_om.pos.z + movez);
  m.load(this, lev.x, lev.y);
  update_map(u.posx, u.posy);
  for (int x = 0; x < SEEX * MAPSIZE; x++) {
   for (int y = 0; y < SEEY * MAPSIZE; y++) {
    if (m.ter(x, y) == t_elevator) {
     u.posx = x;
     u.posy = y;
    }
   }
  }
  refresh_all();
 } else if (t_gas_pump == exam_t && query_yn("Pump gas?")) {
  item gas(item::types[itm_gasoline], messages.turn);
  if (one_in(10 + u.dex_cur)) {
   messages.add("You accidentally spill the gasoline.");
   m.add_item(u.posx, u.posy, gas);
  } else {
   u.moves -= 300;
   handle_liquid(gas, false, true);
  }
 } else if (t_slot_machine == exam_t) {
  if (u.cash < 10) messages.add("You need $10 to play.");
  else if (query_yn("Insert $10?")) {
   do {
    if (one_in(5)) popup("Three cherries... you get your money back!");
    else if (one_in(20)) {
     popup("Three bells... you win $50!");
     u.cash += 40;	// Minus the $10 we wagered
    } else if (one_in(50)) {
     popup("Three stars... you win $200!");
     u.cash += 190;
    } else if (one_in(1000)) {
     popup("JACKPOT!  You win $5000!");
     u.cash += 4990;
    } else {
     popup("No win.");
     u.cash -= 10;
    }
   } while (u.cash >= 10 && query_yn("Play again?"));
  }
 } else if (t_bulletin == exam_t) {
// TODO: Bulletin Boards
  switch (menu("Bulletin Board", "Check jobs", "Check events",
               "Check other notices", "Post notice", "Cancel", NULL)) {
   case 1:
    break;
   case 2:
    break;
   case 3:
    break;
   case 4:
    break;
  }
 } else if (t_fault == exam_t) {
  popup("\
This wall is perfectly vertical.  Odd, twisted holes are set in it, leading\n\
as far back into the solid rock as you can see.  The holes are humanoid in\n\
shape, but with long, twisted, distended limbs.");
 } else if (t_pedestal_wyrm == exam_t && stack.empty()) {
  messages.add("The pedestal sinks into the ground...");
  exam_t = t_rock_floor;
  add_event(EVENT_SPAWN_WYRMS, int(messages.turn) + rng(5, 10));
 } else if (t_pedestal_temple == exam_t) {
  if (stack.size() == 1 && stack[0].type->id == itm_petrified_eye) {
   messages.add("The pedestal sinks into the ground...");
   exam_t = t_dirt;
   stack.clear();
   add_event(EVENT_TEMPLE_OPEN, int(messages.turn) + 4);
  } else if (u.has_amount(itm_petrified_eye, 1) &&
             query_yn("Place your petrified eye on the pedestal?")) {
   u.use_amount(itm_petrified_eye, 1);
   messages.add("The pedestal sinks into the ground...");
   exam_t = t_dirt;
   add_event(EVENT_TEMPLE_OPEN, int(messages.turn) + 4);
  } else
   messages.add("This pedestal is engraved in eye-shaped diagrams, and has a large semi-spherical indentation at the top.");
 } else if (t_switch_rg <= exam_t && t_switch_even >= exam_t && query_yn("Flip the %s?", m.tername(exam).c_str())) {
  u.moves -= 100;
  for (int y = exam.y; y <= exam.y + 5; y++) {
   for (int x = 0; x < SEEX * MAPSIZE; x++) {
	m.apply_temple_switch(exam_t,exam.y,x,y);
   }
  }
  messages.add("You hear the rumble of rock shifting.");
  add_event(EVENT_TEMPLE_SPAWN, messages.turn + 3);
 }

 const auto tr_id = m.tr_at(exam.x, exam.y);
 if (tr_id != tr_null) {
   const trap* const tr = trap::traps[tr_id];
   if (tr->difficulty < 99
       && u.per_cur-u.encumb(bp_eyes) >= tr->visibility
       && query_yn("There is a %s there.  Disarm?", tr->name.c_str()))
     m.disarm_trap(this, exam.x, exam.y);
 }
}

point game::look_around()
{
 draw_ter();
 int lx = u.posx, ly = u.posy;
 char ch;
 WINDOW* w_look = newwin(13, 48, 12, SEEX * 2 + 8);
 wborder(w_look, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                 LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
 mvwprintz(w_look, 1, 1, c_white, "Looking Around");
 mvwprintz(w_look, 2, 1, c_white, "Use directional keys to move the cursor");
 mvwprintz(w_look, 3, 1, c_white, "to a nearby square.");
 wrefresh(w_look);
 do {
  ch = input();
  if (!u_see(lx, ly))
   mvwputch(w_terrain, ly - u.posy + SEEY, lx - u.posx + SEEX, c_black, ' ');
  point dir(get_direction(ch));
  if (dir.x != -2) {	// Directional key pressed
   lx += dir.x;
   ly += dir.y;
/*
   if (lx < u.posx - SEEX)
    lx = u.posx - SEEX;
   if (lx > u.posx + SEEX)
    lx = u.posx + SEEX;
   if (ly < u.posy - SEEY)
    ly = u.posy - SEEY;
   if (ly > u.posy + SEEY)
    ly = u.posy + SEEY;
*/
  }
  draw_ter(lx, ly);
  for (int i = 1; i < 12; i++) {
   for (int j = 1; j < 47; j++)
    mvwputch(w_look, i, j, c_white, ' ');
  }
  int veh_part = 0;
  vehicle *veh = m.veh_at(lx, ly, veh_part);
  if (u_see(lx, ly)) {
   if (m.move_cost(lx, ly) == 0)
    mvwprintw(w_look, 1, 1, "%s; Impassable", m.tername(lx, ly).c_str());
   else
    mvwprintw(w_look, 1, 1, "%s; Movement cost %d", m.tername(lx, ly).c_str(),
                                                    m.move_cost(lx, ly) * 50);
   mvwprintw(w_look, 2, 1, "%s", m.features(lx, ly).c_str());
   const auto& f = m.field_at(lx, ly);
   if (f.type != fd_null)
    mvwprintz(w_look, 4, 1, fieldlist[f.type].color[f.density-1],
              "%s", f.name().c_str());
   const auto tr_id = m.tr_at(lx, ly);
   if (tr_id != tr_null) {
	 const trap* const tr = trap::traps[m.tr_at(lx, ly)];
     if (u.per_cur - u.encumb(bp_eyes) >= tr->visibility)
       mvwprintz(w_look, 5, 1, tr->color, "%s", tr->name.c_str());
   }

   monster* const m_at = mon(lx, ly);
   auto& stack = m.i_at(lx, ly);
   if (m_at && u_see(m_at)) {
    m_at->draw(w_terrain, lx, ly, true);
    m_at->print_info(this, w_look);
    if (stack.size() > 1)
     mvwprintw(w_look, 3, 1, "There are several items there.");
    else if (stack.size() == 1)
     mvwprintw(w_look, 3, 1, "There is an item there.");
   } else if (npc* const _npc = nPC(lx,ly)) {
    _npc->draw(w_terrain, lx, ly, true);
	_npc->print_info(w_look);
    if (stack.size() > 1)
     mvwprintw(w_look, 3, 1, "There are several items there.");
    else if (stack.size() == 1)
     mvwprintw(w_look, 3, 1, "There is an item there.");
   } else if (veh) {
     mvwprintw(w_look, 3, 1, "There is a %s there. Parts:", veh->name.c_str());
     veh->print_part_desc(w_look, 4, 48, veh_part);
     m.drawsq(w_terrain, u, lx, ly, true, true, lx, ly);
   } else if (!stack.empty()) {
    mvwprintw(w_look, 3, 1, "There is a %s there.", stack[0].tname(this).c_str());
    if (stack.size() > 1) mvwprintw(w_look, 4, 1, "There are other items there as well.");
    m.drawsq(w_terrain, u, lx, ly, true, true, lx, ly);
   } else
    m.drawsq(w_terrain, u, lx, ly, true, true, lx, ly);

  } else if (lx == u.posx && ly == u.posy) {
   mvwputch_inv(w_terrain, SEEX, SEEY, u.color(), '@');
   mvwprintw(w_look, 1, 1, "You (%s)", u.name.c_str());
   if (veh) {
    mvwprintw(w_look, 3, 1, "There is a %s there. Parts:", veh->name.c_str());
    veh->print_part_desc(w_look, 4, 48, veh_part);
    m.drawsq(w_terrain, u, lx, ly, true, true, lx, ly);
   }

  } else {
   mvwputch(w_terrain, SEEY, SEEX, c_white, 'x');
   mvwprintw(w_look, 1, 1, "Unseen.");
  }
  wrefresh(w_look);
  wrefresh(w_terrain);
 } while (ch != ' ' && ch != KEY_ESCAPE && ch != '\n');
 if (ch == '\n')
  return point(lx, ly);
 return point(-1, -1);
}

// Pick up items at (posx, posy).
void game::pickup(int posx, int posy, int min)
{
 write_msg();
 if (u.weapon.type->id == itm_bio_claws) {
  messages.add("You cannot pick up items with your claws out!");
  return;
 }
 bool weight_is_okay = (u.weight_carried() <= u.weight_capacity() * .25);
 bool volume_is_okay = (u.volume_carried() <= u.volume_capacity() -  2);
 bool from_veh = false;
 int veh_part = 0;
 vehicle *veh = m.veh_at (posx, posy, veh_part);
 if (veh) {
  veh_part = veh->part_with_feature(veh_part, vpf_cargo, false);
  from_veh = veh && veh_part >= 0 &&
             veh->parts[veh_part].items.size() > 0 &&
             query_yn("Get items from %s?", veh->part_info(veh_part).name);
 }
// Picking up water?
 if ((!from_veh) && m.i_at(posx, posy).size() == 0) {
  if (m.has_flag(swimmable, posx, posy) || m.ter(posx, posy) == t_toilet) {
   item water = m.water_from(posx, posy);
   if (query_yn("Drink from your hands?")) {
    u.inv.push_back(water);
    u.eat(this, u.inv.size() - 1);
    u.moves -= 350;
   } else {
    handle_liquid(water, true, false);
    u.moves -= 100;
   }
  }
  return;
// Few item here, just get it
 } else if ((from_veh ? veh->parts[veh_part].items.size() :
                        m.i_at(posx, posy).size()          ) <= min) {
  int iter = 0;
  item newit = from_veh ? veh->parts[veh_part].items[0] : m.i_at(posx, posy)[0];
  if (newit.made_of(LIQUID)) {
   messages.add("You can't pick up a liquid!");
   return;
  }
  if (newit.invlet == 0) {
   newit.invlet = nextinv;
   advance_nextinv();
  }
  while (iter < 52 && u.has_item(newit.invlet) &&
         !u.i_at(newit.invlet).stacks_with(newit)) {
   newit.invlet = nextinv;
   iter++;
   advance_nextinv();
  }
  if (iter == 52) {
   messages.add("You're carrying too many items!");
   return;
  } else if (u.weight_carried() + newit.weight() > u.weight_capacity()) {
   messages.add("The %s is too heavy!", newit.tname(this).c_str());
   decrease_nextinv();
  } else if (u.volume_carried() + newit.volume() > u.volume_capacity()) {
   if (u.is_armed()) {
    if (!u.weapon.has_flag(IF_NO_UNWIELD)) {
     if (newit.is_armor() && // Armor can be instantly worn
         query_yn("Put on the %s?", newit.tname(this).c_str())) {
      if(u.wear_item(this, &newit)){
       if (from_veh)
        veh->remove_item (veh_part, 0);
       else
        m.i_clear(posx, posy);
      }
     } else if (query_yn("Drop your %s and pick up %s?",
                u.weapon.tname(this).c_str(), newit.tname(this).c_str())) {
      if (from_veh)
       veh->remove_item (veh_part, 0);
      else
       m.i_clear(posx, posy);
      m.add_item(posx, posy, u.remove_weapon());
      u.i_add(newit, this);
      u.wield(this, u.inv.size() - 1);
      u.moves -= 100;
      messages.add("Wielding %c - %s", newit.invlet, newit.tname(this).c_str());
     } else
      decrease_nextinv();
    } else {
     messages.add("There's no room in your inventory for the %s, and you can't\
 unwield your %s.", newit.tname(this).c_str(), u.weapon.tname(this).c_str());
     decrease_nextinv();
    }
   } else {
    u.i_add(newit, this);
    u.wield(this, u.inv.size() - 1);
    if (from_veh)
     veh->remove_item (veh_part, 0);
    else
     m.i_clear(posx, posy);
    u.moves -= 100;
	messages.add("Wielding %c - %s", newit.invlet, newit.tname(this).c_str());
   }
  } else if (!u.is_armed() &&
             (u.volume_carried() + newit.volume() > u.volume_capacity() - 2 ||
              newit.is_weap() || newit.is_gun())) {
   u.weapon = newit;
   if (from_veh)
    veh->remove_item (veh_part, 0);
   else
    m.i_clear(posx, posy);
   u.moves -= 100;
   messages.add("Wielding %c - %s", newit.invlet, newit.tname(this).c_str());
  } else {
   u.i_add(newit, this);
   if (from_veh)
    veh->remove_item (veh_part, 0);
   else
    m.i_clear(posx, posy);
   u.moves -= 100;
   messages.add("%c - %s", newit.invlet, newit.tname(this).c_str());
  }
  if (weight_is_okay && u.weight_carried() >= u.weight_capacity() * .25)
   messages.add("You're overburdened!");
  if (volume_is_okay && u.volume_carried() > u.volume_capacity() - 2) {
   messages.add("You struggle to carry such a large volume!");
  }
  return;
 }
// Otherwise, we have 2 or more items and should list them, etc.
 WINDOW* w_pickup = newwin(12, 48, 0, SEEX * 2 + 8);
 WINDOW* w_item_info = newwin(12, 48, 12, SEEX * 2 + 8);
 int maxitems = 9;	 // Number of items to show at one time.
 std::vector <item> here = from_veh? veh->parts[veh_part].items : m.i_at(posx, posy);
 std::vector<bool> getitem(here.size(),false);
 char ch = ' ';
 int start = 0, cur_it, iter;
 int new_weight = u.weight_carried(), new_volume = u.volume_carried();
 bool update = true;
 mvwprintw(w_pickup, 0,  0, "PICK UP");
// Now print the two lists; those on the ground and about to be added to inv
// Continue until we hit return or space
 do {
  for (int i = 1; i < 12; i++) {
   for (int j = 0; j < 48; j++)
    mvwaddch(w_pickup, i, j, ' ');
  }
  if (ch == '<' && start > 0) {
   start -= maxitems;
   mvwprintw(w_pickup, maxitems + 2, 0, "         ");
  }
  if (ch == '>' && start + maxitems < here.size()) {
   start += maxitems;
   mvwprintw(w_pickup, maxitems + 2, 12, "            ");
  }
  if (ch >= 'a' && ch <= 'a' + here.size() - 1) {
   ch -= 'a';
   getitem[ch] = !getitem[ch];
   wclear(w_item_info);
   if (getitem[ch]) {
    mvwprintw(w_item_info, 1, 0, here[ch].info().c_str());
    wborder(w_item_info, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                         LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
    wrefresh(w_item_info);
    new_weight += here[ch].weight();
    new_volume += here[ch].volume();
    update = true;
   } else {
    wborder(w_item_info, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                         LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
    wrefresh(w_item_info);
    new_weight -= here[ch].weight();
    new_volume -= here[ch].volume();
    update = true;
   }
  }
  if (ch == ',') {
   int count = 0;
   for (int i = 0; i < here.size(); i++) {
    if (getitem[i])
     count++;
    else {
     new_weight += here[i].weight();
     new_volume += here[i].volume();
    }
    getitem[i] = true;
   }
   if (count == here.size()) {
    for (int i = 0; i < here.size(); i++)
     getitem[i] = false;
    new_weight = u.weight_carried();
    new_volume = u.volume_carried();
   }
   update = true;
  }
  for (cur_it = start; cur_it < start + maxitems; cur_it++) {
   mvwprintw(w_pickup, 1 + (cur_it % maxitems), 0,
             "                                        ");
   if (cur_it < here.size()) {
    mvwputch(w_pickup, 1 + (cur_it % maxitems), 0, here[cur_it].color(&u),
             char(cur_it + 'a'));
    if (getitem[cur_it])
     wprintw(w_pickup, " + ");
    else
     wprintw(w_pickup, " - ");
    wprintz(w_pickup, here[cur_it].color(&u), here[cur_it].tname(this).c_str());
    if (here[cur_it].charges > 0)
     wprintz(w_pickup, here[cur_it].color(&u), " (%d)", here[cur_it].charges);
   }
  }
  if (start > 0)
   mvwprintw(w_pickup, maxitems + 2, 0, "< Go Back");
  if (cur_it < here.size())
   mvwprintw(w_pickup, maxitems + 2, 12, "> More items");
  if (update) {		// Update weight & volume information
   update = false;
   mvwprintw(w_pickup, 0,  7, "                           ");
   mvwprintz(w_pickup, 0,  9,
             (new_weight >= u.weight_capacity() * .25 ? c_red : c_white),
             "Wgt %d", new_weight);
   wprintz(w_pickup, c_white, "/%d", int(u.weight_capacity() * .25));
   mvwprintz(w_pickup, 0, 22,
             (new_volume > u.volume_capacity() - 2 ? c_red : c_white),
             "Vol %d", new_volume);
   wprintz(w_pickup, c_white, "/%d", u.volume_capacity() - 2);
  }
  wrefresh(w_pickup);
  ch = getch();
 } while (ch != ' ' && ch != '\n' && ch != KEY_ESCAPE);
 if (ch != '\n') {
  werase(w_pickup);
  wrefresh(w_pickup);
  delwin(w_pickup);
  delwin(w_item_info);
  messages.add("Never mind.");
  return;
 }
// At this point we've selected our items, now we add them to our inventory
 int curmit = 0;
 bool got_water = false;	// Did we try to pick up water?
 for (int i = 0; i < here.size(); i++) {
  iter = 0;
// This while loop guarantees the inventory letter won't be a repeat. If it
// tries all 52 letters, it fails and we don't pick it up.
  if (getitem[i] && here[i].made_of(LIQUID))
   got_water = true;
  else if (getitem[i]) {
   iter = 0;
   while (iter < 52 && (here[i].invlet == 0 ||
                        (u.has_item(here[i].invlet) &&
                         !u.i_at(here[i].invlet).stacks_with(here[i]))) ) {
    here[i].invlet = nextinv;
    iter++;
    advance_nextinv();
   }
   if (iter == 52) {
    messages.add("You're carrying too many items!");
    werase(w_pickup);
    wrefresh(w_pickup);
    delwin(w_pickup);
    return;
   } else if (u.weight_carried() + here[i].weight() > u.weight_capacity()) {
    messages.add("The %s is too heavy!", here[i].tname(this).c_str());
    decrease_nextinv();
   } else if (u.volume_carried() + here[i].volume() > u.volume_capacity()) {
    if (u.is_armed()) {
     if (!u.weapon.has_flag(IF_NO_UNWIELD)) {
      if (here[i].is_armor() && // Armor can be instantly worn
          query_yn("Put on the %s?", here[i].tname(this).c_str())) {
       if(u.wear_item(this, &(here[i])))
       {
        if (from_veh)
         veh->remove_item (veh_part, curmit);
        else
         m.i_rem(posx, posy, curmit);
        curmit--;
       }
      } else if (query_yn("Drop your %s and pick up %s?",
                u.weapon.tname(this).c_str(), here[i].tname(this).c_str())) {
       if (from_veh)
        veh->remove_item (veh_part, curmit);
       else
        m.i_rem(posx, posy, curmit);
       m.add_item(posx, posy, u.remove_weapon());
       u.i_add(here[i], this);
       u.wield(this, u.inv.size() - 1);
       curmit--;
       u.moves -= 100;
	   messages.add("Wielding %c - %s", u.weapon.invlet, u.weapon.tname(this).c_str());
      } else
       decrease_nextinv();
     } else {
      messages.add("There's no room in your inventory for the %s, and you can't\
  unwield your %s.", here[i].tname(this).c_str(), u.weapon.tname(this).c_str());
      decrease_nextinv();
     }
    } else {
     u.i_add(here[i], this);
     u.wield(this, u.inv.size() - 1);
     if (from_veh)
      veh->remove_item (veh_part, curmit);
     else
      m.i_rem(posx, posy, curmit);
     curmit--;
     u.moves -= 100;
    }
   } else if (!u.is_armed() &&
            (u.volume_carried() + here[i].volume() > u.volume_capacity() - 2 ||
              here[i].is_weap() || here[i].is_gun())) {
    u.weapon = here[i];
    if (from_veh)
     veh->remove_item (veh_part, curmit);
    else
     m.i_rem(posx, posy, curmit);
    u.moves -= 100;
    curmit--;
   } else {
    u.i_add(here[i], this);
    if (from_veh)
     veh->remove_item (veh_part, curmit);
    else
     m.i_rem(posx, posy, curmit);
    u.moves -= 100;
    curmit--;
   }
  }
  curmit++;
 }
 if (got_water)
  messages.add("You can't pick up a liquid!");
 if (weight_is_okay && u.weight_carried() >= u.weight_capacity() * .25)
  messages.add("You're overburdened!");
 if (volume_is_okay && u.volume_carried() > u.volume_capacity() - 2) {
  messages.add("You struggle to carry such a large volume!");
 }
 werase(w_pickup);
 wrefresh(w_pickup);
 delwin(w_pickup);
 delwin(w_item_info);
}

// Handle_liquid returns false if we didn't handle all the liquid.
bool game::handle_liquid(item &liquid, bool from_ground, bool infinite)
{
 if (!liquid.made_of(LIQUID)) {
  debugmsg("Tried to handle_liquid a non-liquid!");
  return false;
 }
 if (liquid.type->id == itm_gasoline && vehicle_near() && query_yn ("Refill vehicle?")) {
  int vx = u.posx, vy = u.posy;
  if (pl_choose_vehicle(vx, vy)) {
   vehicle *veh = m.veh_at (vx, vy);
   if (veh) {
    int ftype = AT_GAS;
    int fuel_cap = veh->fuel_capacity(ftype);
    int fuel_amnt = veh->fuel_left(ftype);
    if (fuel_cap < 1)
     messages.add("This vehicle doesn't use %s.", veh->fuel_name(ftype).c_str());
    else if (fuel_amnt == fuel_cap)
     messages.add("Already full.");
    else if (infinite && query_yn("Pump until full?")) {
     u.assign_activity(ACT_REFILL_VEHICLE, 100 * (fuel_cap - fuel_amnt));
     u.activity.placement = point(vx, vy);
    } else { // Not infinite
     veh->refill (AT_GAS, liquid.charges);
     messages.add("You refill %s with %s%s.", veh->name.c_str(),
              veh->fuel_name(ftype).c_str(),
              veh->fuel_left(ftype) >= fuel_cap? " to its maximum" : "");
     u.moves -= 100;
     return true;
    }
   } else // if (veh)
    messages.add("There isn't any vehicle there.");
   return false;
  } // if (pl_choose_vehicle(vx, vy))

 } else if (!from_ground &&
            query_yn("Pour %s on the ground?", liquid.tname(this).c_str())) {
  m.add_item(u.posx, u.posy, liquid);
  return true;

 } else { // Not filling vehicle

  std::stringstream text;
  text << "Container for " << liquid.tname(this);
  char ch = inv(text.str().c_str());
  if (!u.has_item(ch))
   return false;
  item *cont = &(u.i_at(ch));
  if (cont == NULL || cont->is_null()) {
   messages.add("Never mind.");
   return false;

  } else if (liquid.is_ammo() && (cont->is_tool() || cont->is_gun())) {

   ammotype ammo = AT_NULL;
   int max = 0;

   if (cont->is_tool()) {
    const it_tool* const tool = dynamic_cast<const it_tool*>(cont->type);
    ammo = tool->ammo;
    max = tool->max_charges;
   } else {
    const it_gun* const gun = dynamic_cast<const it_gun*>(cont->type);
    ammo = gun->ammo;
    max = gun->clip;
   }

   ammotype liquid_type = liquid.ammo_type();

   if (ammo != liquid_type) {
    messages.add("Your %s won't hold %s.", cont->tname(this).c_str(),
                                      liquid.tname(this).c_str());
    return false;
   }

   if (max <= 0 || cont->charges >= max) {
    messages.add("Your %s can't hold any more %s.", cont->tname(this).c_str(),
                                               liquid.tname(this).c_str());
    return false;
   }

   if (cont->charges > 0 && cont->curammo->id != liquid.type->id) {
    messages.add("You can't mix loads in your %s.", cont->tname(this).c_str());
    return false;
   }

   messages.add("You pour %s into your %s.", liquid.tname(this).c_str(),
                                        cont->tname(this).c_str());
   cont->curammo = dynamic_cast<const it_ammo*>(liquid.type);
   if (infinite) cont->charges = max;
   else {
    cont->charges += liquid.charges;
    if (cont->charges > max) {
     int extra = 0 - cont->charges;
     cont->charges = max;
     liquid.charges = extra;
     messages.add("There's some left over!");
     return false;
    }
   }
   return true;

  } else if (!cont->is_container()) {
   messages.add("That %s won't hold %s.", cont->tname(this).c_str(),
                                     liquid.tname(this).c_str());
   return false;

  } else if (!cont->contents.empty()) {
   messages.add("Your %s is not empty.", cont->tname(this).c_str());
   return false;

  } else {

   const it_container* const container = dynamic_cast<const it_container*>(cont->type);

   if (!(container->flags & mfb(con_wtight))) {
    messages.add("That %s isn't water-tight.", cont->tname(this).c_str());
    return false;
   } else if (!(container->flags & mfb(con_seals))) {
    messages.add("You can't seal that %s!", cont->tname(this).c_str());
    return false;
   }

   int default_charges = 1;

   if (liquid.is_food()) {
    default_charges = dynamic_cast<const it_comest*>(liquid.type)->charges;
   } else if (liquid.is_ammo()) {
    default_charges = dynamic_cast<const it_ammo*>(liquid.type)->count;
   }

   if (liquid.charges > container->contains * default_charges) {
    messages.add("You fill your %s with some of the %s.", cont->tname(this).c_str(),
                                                    liquid.tname(this).c_str());
    u.inv_sorted = false;
    int oldcharges = liquid.charges - container->contains * default_charges;
    liquid.charges = container->contains * default_charges;
    cont->put_in(liquid);
    liquid.charges = oldcharges;
    return false;
   }

   cont->put_in(liquid);
   return true;

  }
 }
 return false;
}


void game::drop()
{
 std::vector<item> dropped = multidrop();
 if (dropped.empty()) {
  messages.add("Never mind.");
  return;
 }

 itype_id first = itype_id(dropped[0].type->id);
 bool same = true;
 for (int i = 1; i < dropped.size() && same; i++) {
  if (dropped[i].type->id != first) same = false;
 }

 int veh_part = 0;
 bool to_veh = false;
 vehicle *veh = m.veh_at(u.posx, u.posy, veh_part);
 if (veh) {
  veh_part = veh->part_with_feature (veh_part, vpf_cargo);
  to_veh = veh_part >= 0;
 }
 if (dropped.size() == 1 || same) {
  if (to_veh)
   messages.add("You put your %s%s in the %s's %s.", dropped[0].tname(this).c_str(),
          (dropped.size() == 1 ? "" : "s"), veh->name.c_str(),
          veh->part_info(veh_part).name);
  else
   messages.add("You drop your %s%s.", dropped[0].tname(this).c_str(),
          (dropped.size() == 1 ? "" : "s"));
 } else {
  if (to_veh)
   messages.add("You put several items in the %s's %s.", veh->name.c_str(),
           veh->part_info(veh_part).name);
  else
   messages.add("You drop several items.");
 }

 bool vh_overflow = false;
 int i = 0;
 if (to_veh) {
  for (i = 0; i < dropped.size(); i++)
   if (!veh->add_item (veh_part, dropped[i])) {
    vh_overflow = true;
    break;
   }
  if (vh_overflow) messages.add("The trunk is full, so some items fall on the ground.");
 }
 if (!to_veh || vh_overflow) for (const auto& it : dropped) m.add_item(u.posx, u.posy, it);
}

void game::drop_in_direction()
{
 refresh_all();
 mvprintz(0, 0, c_red, "Choose a direction:");
 point dir(get_direction(input()));
 if (dir.x == -2) {
  messages.add("Invalid direction!");
  return;
 }
 dir.x += u.posx;
 dir.y += u.posy;
 int veh_part = 0;
 bool to_veh = false;
 vehicle *veh = m.veh_at(dir, veh_part);
 if (veh) {
  veh_part = veh->part_with_feature (veh_part, vpf_cargo);
  to_veh = veh->type != veh_null && veh_part >= 0;
 }

 if (m.has_flag(noitem, dir) || m.has_flag(sealed, dir)) {
  messages.add("You can't place items there!");
  return;
 }

 const auto cost = m.move_cost(dir);
 std::string verb = (0 >= cost ? "put" : "drop");
 std::string prep = (0 >= cost ? "in"  : "on"  );

 std::vector<item> dropped = multidrop();

 if (dropped.empty()) {
  messages.add("Never mind.");
  return;
 }

 itype_id first = itype_id(dropped[0].type->id);
 bool same = true;
 for (int i = 1; i < dropped.size() && same; i++) {
  if (dropped[i].type->id != first) same = false;
 }
 if (dropped.size() == 1 || same)
 {
  if (to_veh)
   messages.add("You put your %s%s in the %s's %s.", dropped[0].tname(this).c_str(),
          (dropped.size() == 1 ? "" : "s"), veh->name.c_str(),
          veh->part_info(veh_part).name);
  else
   messages.add("You %s your %s%s %s the %s.", verb.c_str(),
           dropped[0].tname(this).c_str(),
           (dropped.size() == 1 ? "" : "s"), prep.c_str(),
           m.tername(dir).c_str());
 } else {
  if (to_veh)
   messages.add("You put several items in the %s's %s.", veh->name.c_str(),
           veh->part_info(veh_part).name);
  else
   messages.add("You %s several items %s the %s.", verb.c_str(), prep.c_str(),
           m.tername(dir).c_str());
 }
 if (to_veh) {
  bool vh_overflow = false;
  for (int i = 0; i < dropped.size(); i++) {
   vh_overflow = vh_overflow || !veh->add_item (veh_part, dropped[i]);
   if (vh_overflow) m.add_item(dir.x, dir.y, dropped[i]);
  }
  if (vh_overflow) messages.add("Trunk is full, so some items fall on the ground.");
 } else {
  for (int i = 0; i < dropped.size(); i++)
   m.add_item(dir.x, dir.y, dropped[i]);
 }
}

void game::reassign_item()
{
 char ch = inv("Reassign item:");
 if (ch == KEY_ESCAPE) {
  messages.add("Never mind.");
  return;
 }
 if (!u.has_item(ch)) {
  messages.add("You do not have that item.");
  return;
 }
 char newch = popup_getkey("%c - %s; enter new letter.", ch,
                           u.i_at(ch).tname().c_str());
 if ((newch < 'A' || (newch > 'Z' && newch < 'a') || newch > 'z')) {
  messages.add("%c is not a valid inventory letter.", newch);
  return;
 }
 item* change_from = &(u.i_at(ch));
 if (u.has_item(newch)) {
  item* change_to = &(u.i_at(newch));
  change_to->invlet = ch;
  messages.add("%c - %s", ch, change_to->tname().c_str());
 }
 change_from->invlet = newch;
 messages.add("%c - %s", newch, change_from->tname().c_str());
}


void game::plthrow()
{
 char ch = inv("Throw item:");
 int range = u.throw_range(u.lookup_item(ch));
 if (range < 0) {
  messages.add("You don't have that item.");
  return;
 } else if (range == 0) {
  messages.add("That is too heavy to throw.");
  return;
 }
 item thrown = u.i_at(ch);
 if (thrown.type->id > num_items && thrown.type->id < num_all_items) {
  messages.add("That's part of your body, you can't throw that!");
  return;
 }

 int sight_range = u.sight_range(light_level());
 if (range < sight_range)
  range = sight_range;
 int x = u.posx, y = u.posy;
 int x0 = x - range;
 int y0 = y - range;
 int x1 = x + range;
 int y1 = y + range;

 for (int j = u.posx - SEEX; j <= u.posx + SEEX; j++) {
  for (int k = u.posy - SEEY; k <= u.posy + SEEY; k++) {
   if (u_see(j, k)) {
    if (k >= y0 && k <= y1 && j >= x0 && j <= x1)
     m.drawsq(w_terrain, u, j, k, false, true);
    else
     mvwputch(w_terrain, k + SEEY - u.posy, j + SEEX - u.posx, c_dkgray, '#');
   }
  }
 }

 std::vector <monster> mon_targets;
 std::vector <int> targetindices;
 int passtarget = -1;
 for (int i = 0; i < z.size(); i++) {
  if (u_see(&(z[i])) && z[i].pos.x >= x0 && z[i].pos.x <= x1 &&
                        z[i].pos.y >= y0 && z[i].pos.y <= y1) {
   mon_targets.push_back(z[i]);
   targetindices.push_back(i);
   if (i == last_target) passtarget = mon_targets.size() - 1;
   z[i].draw(w_terrain, u.posx, u.posy, true);
  }
 }

 // target() sets x and y, or returns false if we canceled (by pressing Esc)
 std::vector <point> trajectory = target(x, y, x0, y0, x1, y1, mon_targets, passtarget, &thrown);
 if (trajectory.empty()) return;
 if (passtarget != -1) last_target = targetindices[passtarget];

 u.i_rem(ch);
 u.moves -= 125;
 u.practice(sk_throw, 10);

 throw_item(u, x, y, thrown, trajectory);
}

void game::plfire(bool burst)
{
 int reload_index = -1;
 if (!u.weapon.is_gun()) return;
 vehicle *veh = m.veh_at(u.posx, u.posy);
 if (veh && veh->player_in_control(&u) && u.weapon.is_two_handed(&u)) {
  messages.add("You need free arm to drive!");
  return;
 }
 if (u.weapon.has_flag(IF_CHARGE) && !u.weapon.active) {
  if (u.has_charges(itm_UPS_on, 1) || u.has_charges(itm_UPS_off, 1)) {
   messages.add("Your %s starts charging.", u.weapon.tname().c_str());
   u.weapon.charges = 0;
   u.weapon.curammo = dynamic_cast<it_ammo*>(item::types[itm_charge_shot]);
   u.weapon.active = true;
   return;
  } else {
   messages.add("You need a charged UPS.");
   return;
  }
 }
 if (u.weapon.has_flag(IF_RELOAD_AND_SHOOT)) {
  reload_index = u.weapon.pick_reload_ammo(u, true);
  if (reload_index == -1) {
   messages.add("Out of ammo!");
   return;
  }
 }
 if (u.weapon.has_flag(IF_RELOAD_AND_SHOOT)) {
  u.weapon.reload(u, reload_index);
  u.moves -= u.weapon.reload_time(u);
  refresh_all();
 }

 if (u.weapon.charges == 0 && !u.weapon.has_flag(IF_RELOAD_AND_SHOOT)) {
  messages.add("You need to reload!");
  return;
 }
 if (u.weapon.has_flag(IF_FIRE_100) && u.weapon.charges < 100) {
  messages.add("Your %s needs 100 charges to fire!", u.weapon.tname().c_str());
  return;
 }
 if (u.weapon.has_flag(IF_USE_UPS) && !u.has_charges(itm_UPS_off, 5) &&
     !u.has_charges(itm_UPS_on, 5)) {
  messages.add("You need a UPS with at least 5 charges to fire that!");
  return;
 }

 int range = u.weapon.range(&u);
 int sight_range = u.sight_range(light_level());
 if (range > sight_range) range = sight_range;
 int x = u.posx, y = u.posy;
 int x0 = x - range;
 int y0 = y - range;
 int x1 = x + range;
 int y1 = y + range;
 for (int j = x - SEEX; j <= x + SEEX; j++) {
  for (int k = y - SEEY; k <= y + SEEY; k++) {
   if (u_see(j, k)) {
    if (k >= y0 && k <= y1 && j >= x0 && j <= x1)
     m.drawsq(w_terrain, u, j, k, false, true);
    else
     mvwputch(w_terrain, k + SEEY - y, j + SEEX - x, c_dkgray, '#');
   }
  }
 }
// Populate a list of targets with the zombies in range and visible
 std::vector <monster> mon_targets;
 std::vector <int> targetindices;
 int passtarget = -1;
 for (int i = 0; i < z.size(); i++) {
  if (z[i].pos.x >= x0 && z[i].pos.x <= x1 &&
      z[i].pos.y >= y0 && z[i].pos.y <= y1 &&
      z[i].friendly == 0 && u_see(&(z[i]))) {
   mon_targets.push_back(z[i]);
   targetindices.push_back(i);
   if (i == last_target) passtarget = mon_targets.size() - 1;
   z[i].draw(w_terrain, u.posx, u.posy, true);
  }
 }

 // target() sets x and y, and returns an empty vector if we canceled (Esc)
 std::vector <point> trajectory = target(x, y, x0, y0, x1, y1, mon_targets,
                                         passtarget, &u.weapon);
 draw_ter(); // Recenter our view
 if (trajectory.size() == 0) return;
 if (passtarget != -1) { // We picked a real live target
  last_target = targetindices[passtarget]; // Make it our default for next time
  z[targetindices[passtarget]].add_effect(ME_HIT_BY_PLAYER, 100);
 }

 if (u.weapon.has_flag(IF_USE_UPS)) {
  if (u.has_charges(itm_UPS_off, 5)) u.use_charges(itm_UPS_off, 5);
  else if (u.has_charges(itm_UPS_on, 5)) u.use_charges(itm_UPS_on, 5);
 }

// Train up our skill
 const it_gun* const firing = dynamic_cast<const it_gun*>(u.weapon.type);
 int num_shots = 1;
 if (burst) num_shots = u.weapon.burst_size();
 if (num_shots > u.weapon.charges) num_shots = u.weapon.charges;
 if (u.sklevel[firing->skill_used] == 0 || (firing->ammo != AT_BB && firing->ammo != AT_NAIL))
  u.practice(firing->skill_used, 4 + (num_shots / 2));
 if (u.sklevel[sk_gun] == 0 || (firing->ammo != AT_BB && firing->ammo != AT_NAIL))
  u.practice(sk_gun, 5);

 fire(u, x, y, trajectory, burst);
}

void game::butcher()
{
 std::vector<int> corpses;
 {
 const auto& items = m.i_at(u.posx, u.posy);
 for (int i = 0; i < items.size(); i++) {
  if (items[i].type->id == itm_corpse) corpses.push_back(i);
 }
 }
 if (corpses.empty()) {
  messages.add("There are no corpses here to butcher.");
  return;
 }
 int factor = u.butcher_factor();
 if (factor == 999) {
  messages.add("You don't have a sharp item to butcher with.");
  return;
 }
// We do it backwards to prevent the deletion of a corpse from corrupting our
// vector of indices.
 for (int i = corpses.size() - 1; i >= 0; i--) {
  const mtype* const corpse = m.i_at(u.posx, u.posy)[corpses[i]].corpse;
  if (query_yn("Butcher the %s corpse?", corpse->name.c_str())) {
   int time_to_cut;
   switch (corpse->size) {	// Time in turns to cut up te corpse
    case MS_TINY:   time_to_cut =  2; break;
    case MS_SMALL:  time_to_cut =  5; break;
    case MS_MEDIUM: time_to_cut = 10; break;
    case MS_LARGE:  time_to_cut = 18; break;
    case MS_HUGE:   time_to_cut = 40; break;
   }
   time_to_cut *= 100;	// Convert to movement points
   time_to_cut += factor * 5;	// Penalty for poor tool
   if (time_to_cut < 250) time_to_cut = 250;
   u.assign_activity(ACT_BUTCHER, time_to_cut, corpses[i]);
   u.moves = 0;
   return;
  }
 }
}

void game::complete_butcher(int index)
{
 const mtype* const corpse = m.i_at(u.posx, u.posy)[index].corpse;
 int age = m.i_at(u.posx, u.posy)[index].bday;
 m.i_rem(u.posx, u.posy, index);
 int factor = u.butcher_factor();
 int pelts;
 double skill_shift = 0.;
 int pieces = corpse->chunk_count();
 switch (corpse->size) {
  case MS_TINY:   pelts =  1; break;
  case MS_SMALL:  pelts =  3; break;
  case MS_MEDIUM: pelts =  6; break;
  case MS_LARGE:  pelts = 10; break;
  case MS_HUGE:   pelts = 18; break;
 }
 if (u.sklevel[sk_survival] < 3)
  skill_shift -= rng(0, 8 - u.sklevel[sk_survival]);
 else
  skill_shift += rng(0, u.sklevel[sk_survival]);
 if (u.dex_cur < 8)
  skill_shift -= rng(0, 8 - u.dex_cur) / 4;
 else
  skill_shift += rng(0, u.dex_cur - 8) / 4;
 if (u.str_cur < 4) skill_shift -= rng(0, 5 * (4 - u.str_cur)) / 4;
 if (factor > 0) skill_shift -= rng(0, factor / 5);

 int practice = 4 + pieces;
 if (practice > 20) practice = 20;
 u.practice(sk_survival, practice);

 pieces += int(skill_shift);
 if (skill_shift < 5) pelts += (skill_shift - 5);	// Lose some pelts

 if ((corpse->has_flag(MF_FUR) || corpse->has_flag(MF_LEATHER)) && 0 < pelts) {
  messages.add("You manage to skin the %s!", corpse->name.c_str());
  for (int i = 0; i < pelts; i++) {
   const itype* pelt;
   if (corpse->has_flag(MF_FUR) && corpse->has_flag(MF_LEATHER)) {
	pelt = item::types[one_in(2) ? itm_fur : itm_leather];
   } else {
	pelt = item::types[corpse->has_flag(MF_FUR) ? itm_fur : itm_leather];
   }
   m.add_item(u.posx, u.posy, pelt, age);
  }
 }
 if (pieces <= 0) messages.add("Your clumsy butchering destroys the meat!");
 else {
  const itype* const meat = corpse->chunk_material();	// assumed non-NULL: precondition
  for (int i = 0; i < pieces; i++)
   m.add_item(u.posx, u.posy, meat, age);
  messages.add("You butcher the corpse.");
 }
}

void game::eat()
{
 if (u.has_trait(PF_RUMINANT) && m.ter(u.posx, u.posy) == t_underbrush &&
     query_yn("Eat underbrush?")) {
  u.moves -= 400;
  u.hunger -= 10;
  m.ter(u.posx, u.posy) = t_grass;
  messages.add("You eat the underbrush.");
  return;
 }
 char ch = inv("Consume item:");
 if (ch == KEY_ESCAPE) {
  messages.add("Never mind.");
  return;
 }
 if (!u.has_item(ch)) {
  messages.add("You don't have item '%c'!", ch);
  return;
 }
 u.eat(this, u.lookup_item(ch));
}

void game::wear()
{
 char ch = inv("Wear item:");
 if (ch == KEY_ESCAPE) {
  messages.add("Never mind.");
  return;
 }
 u.wear(this, ch);
}

void game::takeoff()
{
 if (u.takeoff(this, inv("Take off item:")))
  u.moves -= 250; // TODO: Make this variable
 else
  messages.add("Invalid selection.");
}

void game::reload()
{
 if (u.weapon.is_gun()) {
  if (u.weapon.has_flag(IF_RELOAD_AND_SHOOT)) {
   messages.add("Your %s does not need to be reloaded; it reloads and fires in a single action.", u.weapon.tname().c_str());
   return;
  }
  if (u.weapon.ammo_type() == AT_NULL) {
   messages.add("Your %s does not reload normally.", u.weapon.tname().c_str());
   return;
  }
  if (u.weapon.charges == u.weapon.clip_size()) {
   messages.add("Your %s is fully loaded!", u.weapon.tname(this).c_str());
   return;
  }
  int index = u.weapon.pick_reload_ammo(u, true);
  if (index == -1) {
   messages.add("Out of ammo!");
   return;
  }
  u.assign_activity(ACT_RELOAD, u.weapon.reload_time(u), index);
  u.moves = 0;
 } else if (u.weapon.is_tool()) {
  const it_tool* const tool = dynamic_cast<const it_tool*>(u.weapon.type);
  if (tool->ammo == AT_NULL) {
   messages.add("You can't reload a %s!", u.weapon.tname(this).c_str());
   return;
  }
  int index = u.weapon.pick_reload_ammo(u, true);
  if (index == -1) {
// Reload failed
   messages.add("Out of %s!", ammo_name(tool->ammo).c_str());
   return;
  }
  u.assign_activity(ACT_RELOAD, u.weapon.reload_time(u), index);
  u.moves = 0;
 } else if (!u.is_armed()) messages.add("You're not wielding anything.");
 else messages.add("You can't reload a %s!", u.weapon.tname(this).c_str());
 refresh_all();
}

void game::unload()
{
 if (!u.weapon.is_gun() && u.weapon.contents.size() == 0 &&
     (!u.weapon.is_tool() || u.weapon.ammo_type() == AT_NULL)) {
  messages.add("You can't unload a %s!", u.weapon.tname(this).c_str());
  return;
 } else if (u.weapon.is_container() || u.weapon.charges == 0) {
  if (u.weapon.contents.empty()) {
   if (u.weapon.is_gun())
    messages.add("Your %s isn't loaded, and is not modified.", u.weapon.tname(this).c_str());
   else
    messages.add("Your %s isn't charged." , u.weapon.tname(this).c_str());
   return;
  }
// Unloading a container!
  u.moves -= 40 * u.weapon.contents.size();
  std::vector<item> new_contents;	// In case we put stuff back
  while (!u.weapon.contents.empty()) {
   item content = u.weapon.contents[0];
   int iter = 0;
// Pick an inventory item for the contents
   while ((content.invlet == 0 || u.has_item(content.invlet)) && iter < 52) {
    content.invlet = nextinv;
    advance_nextinv();
    iter++;
   }
   if (content.made_of(LIQUID)) {
    if (!handle_liquid(content, false, false))
     new_contents.push_back(content);// Put it back in (we canceled)
   } else {
    if (u.volume_carried() + content.volume() <= u.volume_capacity() &&
        u.weight_carried() + content.weight() <= u.weight_capacity() &&
        iter < 52) {
     messages.add("You put the %s in your inventory.", content.tname(this).c_str());
     u.i_add(content, this);
    } else {
     messages.add("You drop the %s on the ground.", content.tname(this).c_str());
     m.add_item(u.posx, u.posy, content);
    }
   }
   u.weapon.contents.erase(u.weapon.contents.begin());
  }
  u.weapon.contents = new_contents;
  return;
 }
// Unloading a gun or tool!
 u.moves -= int(u.weapon.reload_time(u) / 2);
 if (u.weapon.is_gun()) {	// Gun ammo is combined with existing items
  for (size_t i = 0; i < u.inv.size() && u.weapon.charges > 0; i++) {
   if (u.inv[i].is_ammo()) {
    const it_ammo* tmpammo = dynamic_cast<const it_ammo*>(u.inv[i].type);
    if (tmpammo->id == u.weapon.curammo->id && u.inv[i].charges < tmpammo->count) {
     u.weapon.charges -= (tmpammo->count - u.inv[i].charges);
     u.inv[i].charges = tmpammo->count;
     if (u.weapon.charges < 0) {
      u.inv[i].charges += u.weapon.charges;
      u.weapon.charges = 0;
     }
    }
   }
  }
 }
 item newam;
 if (u.weapon.is_gun() && u.weapon.curammo != NULL)
  newam = item(u.weapon.curammo, messages.turn);
 else
  newam = item(item::types[default_ammo(u.weapon.ammo_type())], messages.turn);
 while (u.weapon.charges > 0) {
  int iter = 0;
  while ((newam.invlet == 0 || u.has_item(newam.invlet)) && iter < 52) {
   newam.invlet = nextinv;
   advance_nextinv();
   iter++;
  }
  if (newam.made_of(LIQUID)) newam.charges = u.weapon.charges;
  u.weapon.charges -= newam.charges;
  if (u.weapon.charges < 0) {
   newam.charges += u.weapon.charges;
   u.weapon.charges = 0;
  }
  if (u.weight_carried() + newam.weight() < u.weight_capacity() &&
      u.volume_carried() + newam.volume() < u.volume_capacity() && iter < 52) {
   if (newam.made_of(LIQUID)) {
    if (!handle_liquid(newam, false, false)) u.weapon.charges += newam.charges;	// Put it back in
   } else
    u.i_add(newam, this);
  } else
   m.add_item(u.posx, u.posy, newam);
 }
 u.weapon.curammo = NULL;
}

void game::wield()
{
 if (u.weapon.has_flag(IF_NO_UNWIELD)) {
// Bionics can't be unwielded
  messages.add("You cannot unwield your %s.", u.weapon.tname(this).c_str());
  return;
 }
 char ch = inv(u.styles.empty() ? "Wield item:" : "Wield item: Press - to choose a style");
 bool success = u.wield(this, ('-' == ch ? -3 : u.lookup_item(ch)));
 if (success) u.recoil = 5;
}

void game::read()
{
 u.read(this, inv("Read:"));
}

void game::chat()
{
 if (active_npc.empty()) {
  messages.add("You talk to yourself for a moment.");
  return;
 }
 std::vector<npc*> available;
 for (int i = 0; i < active_npc.size(); i++) {
  if (u_see(active_npc[i].posx, active_npc[i].posy) &&
      rl_dist(u.posx, u.posy, active_npc[i].posx, active_npc[i].posy) <= 24)
   available.push_back(&active_npc[i]);
 }
 if (available.empty()) {
  messages.add("There's no-one close enough to talk to.");
  return;
 } else if (available.size() == 1)
  available[0]->talk_to_u(this);
 else {
  WINDOW *w = newwin(available.size() + 3, 40, 10, 20);
  wborder(w, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
             LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
  for (int i = 0; i < available.size(); i++)
   mvwprintz(w, i + 1, 1, c_white, "%d: %s", i + 1, available[i]->name.c_str());
  mvwprintz(w, available.size() + 1, 1, c_white, "%d: Cancel",
            available.size() + 1);
  wrefresh(w);
  char ch;
  do {
   ch = getch();
  } while (ch < '1' || ch > '1' + available.size());
  ch -= '1';
  if (ch == available.size())
   return;
  delwin(w);
  available[ch]->talk_to_u(this);
 }
 u.moves -= 100;
}

void game::pldrive(int x, int y)
{
 if (run_mode == 2) { // Monsters around and we don't wanna run
  messages.add("Monster spotted--run mode is on! (Press '!' to turn it off or ' to ignore monster.)");
  return;
 }
 int part = -1;
 vehicle *veh = m.veh_at (u.posx, u.posy, part);
 if (!veh) {
  debugmsg ("game::pldrive error: can't find vehicle! Drive mode is now off.");
  u.in_vehicle = false;
  return;
 }
 if (0 > veh->part_with_feature(part, vpf_controls)) {
  messages.add("You can't drive the vehicle from here. You need controls!");
  return;
 }

 int thr_amount = 10 * 100;
 if (veh->cruise_on)
  veh->cruise_thrust (-y * thr_amount);
 else {
  veh->thrust (-y);
 }
 veh->turn (15 * x);
 if (veh->skidding && veh->valid_wheel_config()) {
  if (rng (0, 40) < u.dex_cur + u.sklevel[sk_driving] * 2) {
   messages.add("You regain control of the %s.", veh->name.c_str());
   veh->skidding = false;
   veh->move.init (veh->turn_dir);
  }
 }

 u.moves = 0;
 if (x != 0 && veh->velocity != 0 && one_in(4)) u.practice(sk_driving, 1);
}

void game::plmove(int x, int y)
{
 if (run_mode == 2) { // Monsters around and we don't wanna run
  messages.add("Monster spotted--safe mode is on! (Press '!' to turn it off or ' to ignore monster.)");
  return;
 }
 if (u.has_disease(DI_STUNNED)) {
  x = rng(u.posx - 1, u.posx + 1);
  y = rng(u.posy - 1, u.posy + 1);
 } else {
  x += u.posx;
  y += u.posy;
 }
// Check if our movement is actually an attack on a monster
 monster* const m_at = mon(x, y);
 bool displace = false;	// Are we displacing a monster?
 if (m_at) {
  if (m_at->friendly == 0) {
   int udam = u.hit_mon(this, m_at);
   if (m_at->hurt(udam)) kill_mon(*m_at, true);
   return;
  } else displace = true;
 }
// If not a monster, maybe there's an NPC there
 if (npc* const _npc = nPC(x,y)) { 
  if (!_npc->is_enemy() && !query_yn("Really attack %s?", _npc->name.c_str())) return;	// Cancel the attack
  u.hit_player(this, *_npc);
  _npc->make_angry();
  if (_npc->hp_cur[hp_head]  <= 0 || _npc->hp_cur[hp_torso] <= 0) _npc->die(this, true);
  return;
 }

// Otherwise, actual movement, zomg
 if (u.has_disease(DI_AMIGARA)) {
  int curdist = 999, newdist = 999;
  for (int cx = 0; cx < SEEX * MAPSIZE; cx++) {
   for (int cy = 0; cy < SEEY * MAPSIZE; cy++) {
    if (m.ter(cx, cy) == t_fault) {
     int dist = rl_dist(cx, cy, u.posx, u.posy);
     if (dist < curdist) curdist = dist;
     dist = rl_dist(cx, cy, x, y);
     if (dist < newdist) newdist = dist;
    }
   }
  }
  if (newdist > curdist) {
   messages.add("You cannot pull yourself away from the faultline...");
   return;
  }
 }

 if (u.has_disease(DI_IN_PIT)) {
  if (rng(0, 40) > u.str_cur + int(u.dex_cur / 2)) {
   messages.add("You try to escape the pit, but slip back in.");
   u.moves -= 100;
   return;
  } else {
   messages.add("You escape the pit!");
   u.rem_disease(DI_IN_PIT);
  }
 }
 if (u.has_disease(DI_DOWNED)) {
  if (rng(0, 40) > u.dex_cur + int(u.str_cur / 2)) {
   messages.add("You struggle to stand.");
   u.moves -= 100;
   return;
  } else {
   messages.add("You stand up.");
   u.rem_disease(DI_DOWNED);
   u.moves -= 100;
   return;
  }
 }

 int vpart = -1, dpart = -1;
 vehicle *veh = m.veh_at(x, y, vpart);
 bool veh_closed_door = false;
 if (veh) {
  dpart = veh->part_with_feature (vpart, vpf_openable);
  veh_closed_door = dpart >= 0 && !veh->parts[dpart].open;
 }

 if (m.move_cost(x, y) > 0) { // move_cost() of 0 = impassible (e.g. a wall)
  if (u.underwater) u.underwater = false;
  dpart = veh ? veh->part_with_feature (vpart, vpf_seat) : -1;
  bool can_board = dpart >= 0 && !veh->parts[dpart].passenger;
  if (can_board && query_yn("Board vehicle?")) { // empty vehicle's seat ahead
   m.board_vehicle (this, x, y, &u);
   u.moves -= 200;
   return;
  }

  if (m.field_at(x, y).is_dangerous() &&
      !query_yn("Really step into that %s?", m.field_at(x, y).name().c_str()))
   return;
  const auto tr_id = m.tr_at(x, y);
  if (tr_id != tr_null) {
	 const trap* const tr = trap::traps[tr_id];
     if (    u.per_cur - u.encumb(bp_eyes) >= tr->visibility
         && !query_yn("Really step onto that %s?", tr->name.c_str()))
     return;
  }

// Calculate cost of moving
  u.moves -= u.run_cost(m.move_cost(x, y) * 50);

// Adjust recoil down
  if (u.recoil > 0) {
   if (int(u.str_cur / 2) + u.sklevel[sk_gun] >= u.recoil)
    u.recoil = 0;
   else {
    u.recoil -= int(u.str_cur / 2) + u.sklevel[sk_gun];
    u.recoil = int(u.recoil / 2);
   }
  }
  if ((!u.has_trait(PF_PARKOUR) && m.move_cost(x, y) > 2) ||
      ( u.has_trait(PF_PARKOUR) && m.move_cost(x, y) > 4    ))
  {
   if (veh) messages.add("Moving past this %s is slow!", veh->part_info(vpart).name);
   else messages.add("Moving past this %s is slow!", m.tername(x, y).c_str());
  }
  if (m.has_flag(rough, x, y)) {
   if (one_in(5) && u.armor_bash(bp_feet) < rng(1, 5)) {
    messages.add("You hurt your feet on the %s!", m.tername(x, y).c_str());
    u.hit(this, bp_feet, 0, 0, 1);
    u.hit(this, bp_feet, 1, 0, 1);
   }
  }
  if (m.has_flag(sharp, x, y) && !one_in(3) && !one_in(40 - int(u.dex_cur/2))) {
   if (!u.has_trait(PF_PARKOUR) || one_in(4)) {
    messages.add("You cut yourself on the %s!", m.tername(x, y).c_str());
    u.hit(this, bp_torso, 0, 0, rng(1, 4));
   }
  }
  if (!u.has_artifact_with(AEP_STEALTH) && !u.has_trait(PF_LEG_TENTACLES)) {
   sound(x, y, (u.has_trait(PF_LIGHTSTEP) ? 2 : 6), "");	// Sound of footsteps may awaken nearby monsters
  }
  if (one_in(20) && u.has_artifact_with(AEP_MOVEMENT_NOISE))
   sound(x, y, 40, "You emit a rattling sound.");
// If we moved out of the nonant, we need update our map data
  if (m.has_flag(swimmable, x, y) && u.has_disease(DI_ONFIRE)) {
   messages.add("The water puts out the flames!");
   u.rem_disease(DI_ONFIRE);
  }
// displace is set at the top of this function.
  if (displace) { // We displaced a friendly monster!
// Immobile monsters can't be displaced.
   if (m_at->has_flag(MF_IMMOBILE)) {
// ...except that turrets can be picked up.
// TODO: Make there a flag, instead of hard-coded to mon_turret
    if (m_at->type->id == mon_turret) {
     if (query_yn("Deactivate the turret?")) {
      m.add_item(m_at->pos.x, m_at->pos.y, item::types[itm_bot_turret], messages.turn);
	  z_erase(mon_at(x, y));
      u.moves -= 100;
     }
     return;
    } else {
     messages.add("You can't displace your %s.", m_at->name().c_str());
     return;
    }
   }
   m_at->move_to(this, u.posx, u.posy);
   messages.add("You displace the %s.", m_at->name().c_str());
  }
  if (x < SEEX * int(MAPSIZE / 2) || y < SEEY * int(MAPSIZE / 2) ||
      x >= SEEX * (1 + int(MAPSIZE / 2)) || y >= SEEY * (1 + int(MAPSIZE / 2)))
   update_map(x, y);
  u.posx = x;
  u.posy = y;
  if (tr_id != tr_null) { // We stepped on a trap!
   const trap* const tr = trap::traps[tr_id];
   if (!u.avoid_trap(tr)) (*tr->act)(this, x, y);
  }

// Some martial art styles have special effects that trigger when we move
  switch (u.weapon.type->id) {

   case itm_style_capoeira:
    if (u.disease_level(DI_ATTACK_BOOST) < 2) u.add_disease(DI_ATTACK_BOOST, 2, this, 2, 2);
    if (u.disease_level(DI_DODGE_BOOST) < 2) u.add_disease(DI_DODGE_BOOST, 2, this, 2, 2);
    break;

   case itm_style_ninjutsu:
    u.add_disease(DI_ATTACK_BOOST, 2, this, 1, 3);
    break;

   case itm_style_crane:
    if (!u.has_disease(DI_DODGE_BOOST)) u.add_disease(DI_DODGE_BOOST, 1, this, 3, 3);
    break;

   case itm_style_leopard:
    u.add_disease(DI_ATTACK_BOOST, 2, this, 1, 4);
    break;

   case itm_style_dragon:
    if (!u.has_disease(DI_DAMAGE_BOOST)) u.add_disease(DI_DAMAGE_BOOST, 2, this, 3, 3);
    break;

   case itm_style_lizard: {
    bool wall = false;
    for (int wallx = x - 1; wallx <= x + 1 && !wall; wallx++) {
     for (int wally = y - 1; wally <= y + 1 && !wall; wally++) {
      if (m.has_flag(supports_roof, wallx, wally))
       wall = true;
     }
    }
    if (wall)
     u.add_disease(DI_ATTACK_BOOST, 2, this, 2, 8);
    else
     u.rem_disease(DI_ATTACK_BOOST);
   } break;
  }

// List items here
  auto& stack = m.i_at(x, y);
  if (!stack.empty()) {
	  if (!u.has_disease(DI_BLIND) && stack.size() <= 3) {
		  std::string buff = "You see here ";
		  int i = -1;
		  for(auto& it : stack) {
			  ++i;
			  buff += it.tname(this);
			  if (i + 2 < stack.size()) buff += ", ";
			  else if (i + 1 < stack.size()) buff += ", and ";
		  }
		  buff += ".";
		  messages.add(buff.c_str());
	  } else messages.add("There are many items here.");
  }

 } else if (veh_closed_door) { // move_cost <= 0
  veh->parts[dpart].open = 1;
  veh->insides_dirty = true;
  u.moves -= 100;
  messages.add("You open the %s's %s.", veh->name.c_str(),
                                    veh->part_info(dpart).name);

 } else if (m.has_flag(swimmable, x, y)) { // Dive into water!
// Requires confirmation if we were on dry land previously
  if ((m.has_flag(swimmable, u.posx, u.posy) &&
      m.move_cost(u.posx, u.posy) == 0) || query_yn("Dive into the water?")) {
   if (m.move_cost(u.posx, u.posy) > 0 && u.swim_speed() < 500)
    messages.add("You start swimming.  Press '>' to dive underwater.");
   plswim(x, y);
  }

 } else { // Invalid move
  if (u.has_disease(DI_BLIND) || u.has_disease(DI_STUNNED)) {
// Only lose movement if we're blind
   messages.add("You bump into a %s!", m.tername(x, y).c_str());
   u.moves -= 100;
  } else if (m.open_door(x, y, m.ter(u.posx, u.posy) == t_floor))
   u.moves -= 100;
  else if (m.ter(x, y) == t_door_locked || m.ter(x, y) == t_door_locked_alarm) {
   u.moves -= 100;
   messages.add("That door is locked!");
  }
 }
}

void game::plswim(int x, int y)
{
 if (x < SEEX * int(MAPSIZE / 2) || y < SEEY * int(MAPSIZE / 2) ||
     x >= SEEX * (1 + int(MAPSIZE / 2)) || y >= SEEY * (1 + int(MAPSIZE / 2)))
  update_map(x, y);
 u.posx = x;
 u.posy = y;
 if (!m.has_flag(swimmable, x, y)) {
  debugmsg("Tried to swim in %s!", m.tername(x, y).c_str());
  return;
 }
 if (u.has_disease(DI_ONFIRE)) {
  messages.add("The water puts out the flames!");
  u.rem_disease(DI_ONFIRE);
 }
 int movecost = u.swim_speed();
 u.practice(sk_swimming, 1);
 if (movecost >= 500) {
  if (!u.underwater) {
   messages.add("You sink%s!", (movecost >= 400 ? " like a rock" : ""));
   u.underwater = true;
   u.oxygen = 30 + 2 * u.str_cur;
  }
 }
 if (u.oxygen <= 5 && u.underwater) {
  if (movecost < 500)
   popup("You need to breathe! (Press '<' to surface.)");
  else
   popup("You need to breathe but you can't swim!  Get to dry land, quick!");
 }
 u.moves -= (movecost > 200 ? 200 : movecost);
 for (size_t i = 0; i < u.inv.size(); i++) {
  if (u.inv[i].type->m1 == IRON && u.inv[i].damage < 5 && one_in(8))
   u.inv[i].damage++;
 }
}

void game::fling_player_or_monster(player *p, monster *zz, int dir, int flvel)
{
    int steps = 0;
    bool is_u = p && (p == &u);
    int dam1, dam2;

    bool is_player;
    if (p) is_player = true;
    else if (zz) is_player = false;
    else {
        debugmsg ("game::fling neither player nor monster");
        return;
    }

    tileray tdir(dir);
    std::string sname, snd;
    if (p) {
        if (is_u) sname = std::string ("You are");
        else sname = p->name + " is";
    }
    else sname = zz->name() + " is";
    int range = flvel / 10;
    int vel1 = flvel;
    int x = (is_player? p->posx : zz->pos.x);
    int y = (is_player? p->posy : zz->pos.y);
    while (range > 0) {
        tdir.advance();
        x = (is_player? p->posx : zz->pos.x) + tdir.dx();
        y = (is_player? p->posy : zz->pos.y) + tdir.dy();
        std::string dname;
        bool thru = true;
        bool slam = false;
		monster* const m_at = mon(x, y);
        dam1 = flvel / 3 + rng (0, flvel * 1 / 3);
        if (m_at) {
            slam = true;
            dname = m_at->name();
            dam2 = flvel / 3 + rng (0, flvel * 1 / 3);
            if (m_at->hurt(dam2)) kill_mon(*m_at);
            else thru = false;
            if (is_player) p->hitall (this, dam1, 40);
            else zz->hurt(dam1);
        } else if (m.move_cost(x, y) == 0 && !m.has_flag(swimmable, x, y)) {
            slam = true;
            int vpart;
            vehicle *veh = m.veh_at(x, y, vpart);
            dname = veh ? veh->part_info(vpart).name : m.tername(x, y).c_str();
            if (m.has_flag(bashable, x, y)) thru = m.bash(x, y, flvel, snd);
            else thru = false;
            if (snd.length() > 0) messages.add("You hear a %s", snd.c_str());
            if (is_player) p->hitall (this, dam1, 40);
            else zz->hurt (dam1);
            flvel = flvel / 2;
        }
        if (slam) messages.add("%s slammed against the %s for %d damage!", sname.c_str(), dname.c_str(), dam1);
		if (!thru) break;
        if (is_player)
        {
            p->posx = x;
            p->posy = y;
        }
        else zz->pos = point(x, y);
        range--;
        steps++;
        timespec ts;   // Timespec for the animation
        ts.tv_sec = 0;
        ts.tv_nsec = 50000000;
        nanosleep (&ts, 0);
    }

    if (!m.has_flag(swimmable, x, y))
    {
        // fall on ground
        dam1 = rng (flvel / 3, flvel * 2 / 3) / 2;
        if (is_player)
        {
            int dex_reduce = p->dex_cur < 4? 4 : p->dex_cur;
            dam1 = dam1 * 8 / dex_reduce;
            if (p->has_trait(PF_PARKOUR)) dam1 /= 2;
            if (dam1 > 0) p->hitall (this, dam1, 40);
        }
        else zz->hurt (dam1);
        if (is_u)
            if (dam1 > 0)
				messages.add("You fall on the ground for %d damage.", dam1);
            else
				messages.add("You fall on the ground.");
    }
    else if (is_u) messages.add("You fall into water.");
}

void game::vertical_move(int movez, bool force)
{
// > and < are used for diving underwater.
 if (m.move_cost(u.posx, u.posy) == 0 && m.has_flag(swimmable, u.posx, u.posy)){
  if (movez == -1) {
   if (u.underwater) {
    messages.add("You are already underwater!");
    return;
   }
   u.underwater = true;
   u.oxygen = 30 + 2 * u.str_cur;
   messages.add("You dive underwater!");
  } else {
   if (u.swim_speed() < 500) {
    u.underwater = false;
    messages.add("You surface.");
   } else
    messages.add("You can't surface!");
  }
  return;
 }
// Force means we're going down, even if there's no staircase, etc.
// This happens with sinkholes and the like.
 if (!force && ((movez == -1 && !m.has_flag(goes_down, u.posx, u.posy)) ||
                (movez ==  1 && !m.has_flag(goes_up,   u.posx, u.posy))   )) {
  messages.add("You can't go %s here!", (movez == -1 ? "down" : "up"));
  return;
 }

 int original_z = cur_om.pos.z;
 cur_om.save(u.name);
 //m.save(&cur_om, turn, levx, levy);
 cur_om = overmap(this, cur_om.pos.x, cur_om.pos.y, cur_om.pos.z + movez);
 map tmpmap;
 tmpmap.load(this, lev.x, lev.y);
 cur_om = overmap(this, cur_om.pos.x, cur_om.pos.y, original_z);
// Find the corresponding staircase
 int stairx = -1, stairy = -1;
 bool rope_ladder = false;
 if (force) {
  stairx = u.posx;
  stairy = u.posy;
 } else { // We need to find the stairs.
  int best = 999;
   for (int i = u.posx - SEEX * 2; i <= u.posx + SEEX * 2; i++) {
    for (int j = u.posy - SEEY * 2; j <= u.posy + SEEY * 2; j++) {
    if (rl_dist(u.posx, u.posy, i, j) <= best &&
        ((movez == -1 && tmpmap.has_flag(goes_up, i, j)) ||
         (movez ==  1 && (tmpmap.has_flag(goes_down, i, j) ||
                          tmpmap.ter(i, j) == t_manhole_cover)))) {
     stairx = i;
     stairy = j;
     best = rl_dist(u.posx, u.posy, i, j);
    }
   }
  }

  if (stairx == -1 || stairy == -1) { // No stairs found!
   if (movez < 0) {
    if (tmpmap.move_cost(u.posx, u.posy) == 0) {
     popup("Halfway down, the way down becomes blocked off.");
     return;
    } else if (u.has_amount(itm_rope_30, 1)) {
     if (!query_yn("There is a sheer drop halfway down. Climb your rope down?")) return;
     rope_ladder = true;
     u.use_amount(itm_rope_30, 1);
    } else if (!query_yn("There is a sheer drop halfway down.  Jump?"))
     return;
   }
   stairx = u.posx;
   stairy = u.posy;
  }
 }
 
// Replace the stair monsters if we just came back
 bool replace_monsters = (abs(monstair.x - lev.x) <= 1 && abs(monstair.y - lev.y) <= 1 && monstair.z == lev.z + movez);
 
 if (!force) {
  monstair.x = lev.x;
  monstair.y = lev.y;
  monstair.z = original_z;
  for (int i = 0; i < z.size(); i++) {
   if (z[i].will_reach(this, u.posx, u.posy)) {
    int turns = z[i].turns_to_reach(this, u.posx, u.posy);
    if (turns < 999)
     coming_to_stairs.push_back( monster_and_count(z[i], 1 + turns) );
   } else if (z[i].is_static_spawn()) { // Static spawn, move them back there
    tinymap tmp;
    tmp.load(this, z[i].spawnmap.x, z[i].spawnmap.y);
    tmp.add_spawn(&(z[i]));
    tmp.save(&cur_om, messages.turn, z[i].spawnmap.x, z[i].spawnmap.y);
   } else if (z[i].friendly < 0) { // Friendly, make it into a static spawn
    tinymap tmp;
    tmp.load(this, lev.x, lev.y);
	point spawn = z[i].pos;
    while (spawn.x < 0) spawn.x += SEEX;
    while (spawn.y < 0) spawn.y += SEEY;
    tmp.add_spawn(&(z[i]));
    tmp.save(&cur_om, messages.turn, lev.x, lev.y);
   } else {
    const int group = valid_group( (mon_id)(z[i].type->id), lev.x, lev.y);
    if (group != -1) cur_om.zg[group].population++;
   }
  }
 }
 z.clear();

// Figure out where we know there are up/down connectors
 std::vector<point> discover;
 for (int x = 0; x < OMAPX; x++) {
  for (int y = 0; y < OMAPY; y++) {
   if (cur_om.seen(x, y) &&
       ((movez ==  1 && oter_t::list[ cur_om.ter(x, y) ].known_up) ||
        (movez == -1 && oter_t::list[ cur_om.ter(x, y) ].known_down) ))
    discover.push_back( point(x, y) );
  }
 }

// We moved!  Load the new map.
 cur_om = overmap(this, cur_om.pos.x, cur_om.pos.y, cur_om.pos.z + movez);

// Fill in all the tiles we know about (e.g. subway stations)
 for (int i = 0; i < discover.size(); i++) {
  int x = discover[i].x, y = discover[i].y;
  cur_om.seen(x, y) = true;
  if (movez ==  1 && !oter_t::list[ cur_om.ter(x, y) ].known_down &&
      !cur_om.has_note(x, y))
   cur_om.add_note(x, y, "AUTO: goes down");
  if (movez == -1 && !oter_t::list[ cur_om.ter(x, y) ].known_up &&
      !cur_om.has_note(x, y))
   cur_om.add_note(x, y, "AUTO: goes up");
 }

 lev.z += movez;
 u.moves -= 100;
 m.load(this, lev.x, lev.y);
 u.posx = stairx;
 u.posy = stairy;
 if (rope_ladder) m.ter(u.posx, u.posy) = t_rope_up;
 if (m.rewrite_test<t_manhole_cover, t_manhole>(stairx, stairy)) 
   m.add_item(stairx + rng(-1, 1), stairy + rng(-1, 1), item::types[itm_manhole_cover], 0);

 if (replace_monsters) replace_stair_monsters();

 m.spawn_monsters(this);

 if (force) {	// Basically, we fell.
  if (u.has_trait(PF_WINGS_BIRD))
   messages.add("You flap your wings and flutter down gracefully.");
  else {
   int dam = int((u.str_max / 4) + rng(5, 10)) * rng(1, 3);//The bigger they are
   dam -= rng(u.dodge(this), u.dodge(this) * 3);
   if (dam <= 0)
    messages.add("You fall expertly and take no damage.");
   else {
    messages.add("You fall heavily, taking %d damage.", dam);
    u.hurtall(dam);
   }
  }
 }

 if (m.tr_at(u.posx, u.posy) != tr_null) { // We stepped on a trap!
  const trap* const tr = trap::traps[m.tr_at(u.posx, u.posy)];
  if (force || !u.avoid_trap(tr)) (tr->act)(this, u.posx, u.posy);
 }

 set_adjacent_overmaps(true);
 refresh_all();
}


void game::update_map(int &x, int &y)
{
 point shift(0,0);
 int group = 0;
 point olev(0, 0);
 while (x < SEEX * int(MAPSIZE / 2)) {
  x += SEEX;
  shift.x--;
 }
 while (x >= SEEX * (1 + int(MAPSIZE / 2))) {
  x -= SEEX;
  shift.x++;
 }
 while (y < SEEY * int(MAPSIZE / 2)) {
  y += SEEY;
  shift.y--;
 }
 while (y >= SEEY * (1 + int(MAPSIZE / 2))) {
  y -= SEEY;
  shift.y++;
 }
 m.shift(this, lev.x, lev.y, shift);
 lev.x += shift.x;
 lev.y += shift.y;
 if (lev.x < 0) {
  lev.x += OMAPX * 2;
  olev.x = -1;
 } else if (lev.x > OMAPX * 2 - 1) {
  lev.x -= OMAPX * 2;
  olev.x = 1;
 }
 if (lev.y < 0) {
  lev.y += OMAPY * 2;
  olev.y = -1;
 } else if (lev.y > OMAPY * 2 - 1) {
  lev.y -= OMAPY * 2;
  olev.y = 1;
 }
 if (olev.x != 0 || olev.y != 0) {
  cur_om.save(u.name);
  cur_om = overmap(this, cur_om.pos.x + olev.x, cur_om.pos.y + olev.y, cur_om.pos.z);
 }
 set_adjacent_overmaps();

 // Shift monsters
 for (int i = 0; i < z.size(); i++) {
  z[i].shift(shift.x, shift.y);
  if (z[i].pos.x < 0 - SEEX             || z[i].pos.y < 0 - SEEX ||
      z[i].pos.x > SEEX * (MAPSIZE + 1) || z[i].pos.y > SEEY * (MAPSIZE + 1)) {
// Despawn; we're out of bounds
   if (z[i].is_static_spawn()) {	// Static spawn, move them back there
    map tmp;
    tmp.load(this, z[i].spawnmap.x, z[i].spawnmap.y);
    tmp.add_spawn(&(z[i]));
    tmp.save(&cur_om, messages.turn, z[i].spawnmap.x, z[i].spawnmap.y);
   } else {	// Absorb them back into a group
    group = valid_group((mon_id)(z[i].type->id), lev.x + shift.x, lev.y + shift.y);
    if (group != -1) {
     cur_om.zg[group].population++;
     if (cur_om.zg[group].population / pow(cur_om.zg[group].radius, 2.0) > 5)
      cur_om.zg[group].radius++;
    }
/*  Removing adding new groups for now.  Haha!
	else {
		const auto m_cat = mongroup::to_mc((mon_id)(z[i].type->id));
		if (m_cat != mcat_null) cur_om.zg.push_back(mongroup(m_cat, levx, levy, 1, 1));
	}
}*/
   }
   z_erase(i);
   i--;
  }
 }
// Shift NPCs
 for (int i = 0; i < active_npc.size(); i++) {
  active_npc[i].shift(shift);
  if (active_npc[i].posx < 0 - SEEX * 2 ||
      active_npc[i].posy < 0 - SEEX * 2 ||
      active_npc[i].posx >     SEEX * (MAPSIZE + 2) ||
      active_npc[i].posy >     SEEY * (MAPSIZE + 2)   ) {
   active_npc[i].mapx = lev.x + (active_npc[i].posx / SEEX);
   active_npc[i].mapy = lev.y + (active_npc[i].posy / SEEY);
   active_npc[i].posx %= SEEX;
   active_npc[i].posy %= SEEY;
   cur_om.npcs.push_back(active_npc[i]);
   active_npc[i].dead = true;
   //active_npc.erase(active_npc.begin() + i);
   i--;
  }
 }
// Spawn static NPCs?
 npc temp;
 for (int i = 0; i < cur_om.npcs.size(); i++) {
  if (rl_dist(lev.x + int(MAPSIZE / 2), lev.y + int(MAPSIZE / 2),
              cur_om.npcs[i].mapx, cur_om.npcs[i].mapy) <= 
              int(MAPSIZE / 2) + 1) {
   int dx = cur_om.npcs[i].mapx - lev.x, dy = cur_om.npcs[i].mapy - lev.y;
   if (debugmon)
    debugmsg("Spawning static NPC, %d:%d (%d:%d)", lev.x, lev.y,
             cur_om.npcs[i].mapx, cur_om.npcs[i].mapy);
   temp = cur_om.npcs[i];
   if (temp.posx == -1 || temp.posy == -1) {
    temp.posx = SEEX * 2 * (temp.mapx - lev.x) + rng(0 - SEEX, SEEX);
    temp.posy = SEEY * 2 * (temp.mapy - lev.y) + rng(0 - SEEY, SEEY);
   } else {
    if (debugmon)
     debugmsg("Static NPC fine location %d:%d (%d:%d)", temp.posx, temp.posy,
              temp.posx + dx * SEEX, temp.posy + dy * SEEY);
    temp.posx += dx * SEEX;
    temp.posy += dy * SEEY;
   }
   if (temp.marked_for_death)
    temp.die(this, false);
   else
    active_npc.push_back(temp);
   cur_om.npcs.erase(cur_om.npcs.begin() + i);
   i--;
  }
 }
// Spawn monsters if appropriate
 m.spawn_monsters(this);	// Static monsters
 if (messages.turn >= nextspawn) spawn_mon(shift.x, shift.y);
// Shift scent
 decltype(grscent) newscent;
 for (int i = 0; i < SEEX * MAPSIZE; i++) {
  for (int j = 0; j < SEEY * MAPSIZE; j++)
   newscent[i][j] = scent(i + (shift.x * SEEX), j + (shift.y * SEEY));
 }
 memmove(grscent, newscent, sizeof(newscent));
// Update what parts of the world map we can see
 update_overmap_seen();
 draw_minimap();
 //save(); // We autosave every time the map gets updated.
}

void game::set_adjacent_overmaps(bool from_scratch)
{
 if (lev.x == OMAPX - 1 || lev.x == 0 || (from_scratch && lev.x <= OMAPX)) {
  delete om_hori;
  om_hori = new overmap(this, cur_om.pos.x - 1, cur_om.pos.y, cur_om.pos.z);
  if (lev.y == OMAPY - 1 || lev.y == 0 || (from_scratch && lev.y <= OMAPY)) {
   delete om_diag;
   om_diag = new overmap(this, cur_om.pos.x - 1, cur_om.pos.y - 1, cur_om.pos.z);
  } else if (lev.y == OMAPY || lev.y == OMAPY * 2 - 1 || (from_scratch && lev.y > OMAPY)) {
   delete om_diag;
   om_diag = new overmap(this, cur_om.pos.x - 1, cur_om.pos.y + 1, cur_om.pos.z);
  }
 } else if (lev.x == OMAPX || lev.x == OMAPX * 2 - 1 || (from_scratch && lev.x > OMAPX)) {
  delete om_hori;
  om_hori = new overmap(this, cur_om.pos.x + 1, cur_om.pos.y, cur_om.pos.z);
  if (lev.y == OMAPY - 1 || lev.y == 0 || (from_scratch && lev.y <= OMAPY)) {
   delete om_diag;
   om_diag = new overmap(this, cur_om.pos.x + 1, cur_om.pos.y - 1, cur_om.pos.z);
  } else if (lev.y == OMAPY || lev.y == OMAPY * 2 - 1 || (from_scratch && lev.y > OMAPY)) {
   delete om_diag;
   om_diag = new overmap(this, cur_om.pos.x + 1, cur_om.pos.y + 1, cur_om.pos.z);
  }
 }

 if (lev.y == OMAPY - 1 || lev.y == 0 || (from_scratch && lev.y <= OMAPY)) {
  delete om_vert;
  om_vert = new overmap(this, cur_om.pos.x    , cur_om.pos.y - 1, cur_om.pos.z);
 } else if (lev.y == OMAPY || lev.y == OMAPY * 2 - 1 || (from_scratch && lev.y > OMAPY)) {
  delete om_vert;
  om_vert = new overmap(this, cur_om.pos.x    , cur_om.pos.y + 1, cur_om.pos.z);
 }
}

void game::update_overmap_seen()
{
 point om((lev.x + MAPSIZE / 2) / 2, (lev.y + MAPSIZE / 2) / 2);
 int dist = u.overmap_sight_range(light_level());
 cur_om.seen(om.x, om.y) = true; // We can always see where we're standing
 if (dist == 0) return; // No need to run the rest!
 bool altered_om_vert = false, altered_om_diag = false, altered_om_hori = false;
 for (int x = om.x - dist; x <= om.x + dist; x++) {
  for (int y = om.y - dist; y <= om.y + dist; y++) {
   std::vector<point> line = line_to(om, x, y, 0);
   int sight_points = dist;
   int cost = 0;
   for (int i = 0; i < line.size() && sight_points >= 0; i++) {
    int lx = line[i].x, ly = line[i].y;
	const bool x_in_bounds = (0 <= lx && OMAPX > lx);
	const bool y_in_bounds = (0 <= ly && OMAPX > ly);
    if (x_in_bounds && y_in_bounds)
     cost = oter_t::list[cur_om.ter(lx, ly)].see_cost;
    else if (!x_in_bounds && !y_in_bounds) {
     if (lx < 0) lx += OMAPX;
     else        lx -= OMAPX;
     if (ly < 0) ly += OMAPY;
     else        ly -= OMAPY;
     cost = oter_t::list[om_diag->ter(lx, ly)].see_cost;
    } else if (!x_in_bounds) {
     if (lx < 0) lx += OMAPX;
     else        lx -= OMAPX;
     cost = oter_t::list[om_hori->ter(lx, ly)].see_cost;
    } else if (!y_in_bounds) {
     if (ly < 0) ly += OMAPY;
     else        ly -= OMAPY;
     cost = oter_t::list[om_vert->ter(lx, ly)].see_cost;
    }
    sight_points -= cost;
   }
   if (sight_points >= 0) {
    int tmpx = x, tmpy = y;
	const bool x_in_bounds = (0 <= tmpx && OMAPX > tmpx);
	const bool y_in_bounds = (0 <= tmpy && OMAPX > tmpy);
	if (x_in_bounds && y_in_bounds)
     cur_om.seen(tmpx, tmpy) = true;
    else if (!x_in_bounds && !y_in_bounds) {
     if (tmpx < 0) tmpx += OMAPX;
     else          tmpx -= OMAPX;
     if (tmpy < 0) tmpy += OMAPY;
     else          tmpy -= OMAPY;
     om_diag->seen(tmpx, tmpy) = true;
     altered_om_diag = true;
    } else if (!x_in_bounds) {
     if (tmpx < 0) tmpx += OMAPX;
     else          tmpx -= OMAPX;
     om_hori->seen(tmpx, tmpy) = true;
     altered_om_hori = true;
    } else if (!y_in_bounds) {
     if (tmpy < 0) tmpy += OMAPY;
     else          tmpy -= OMAPY;
     om_vert->seen(tmpx, tmpy) = true;
     altered_om_vert = true;
    }
   }
  }
 }
 if (altered_om_vert) om_vert->save(u.name);
 if (altered_om_hori) om_hori->save(u.name);
 if (altered_om_diag) om_diag->save(u.name);
}

point game::om_location()
{
 return point((lev.x + MAPSIZE/2) / 2, (lev.y + MAPSIZE/2) / 2);
}

void game::replace_stair_monsters()
{
 for (int i = 0; i < coming_to_stairs.size(); i++)
  z.push_back(coming_to_stairs[i].mon);
 coming_to_stairs.clear();
}

void game::update_stair_monsters()
{
 if (abs(lev.x - monstair.x) > 1 || abs(lev.y - monstair.y) > 1) return;

 for (int i = 0; i < coming_to_stairs.size(); i++) {
  coming_to_stairs[i].count--;
  if (coming_to_stairs[i].count <= 0) {
   int startx = rng(0, SEEX * MAPSIZE - 1), starty = rng(0, SEEY * MAPSIZE - 1);
   bool found_stairs = false;
   for (int x = 0; x < SEEX * MAPSIZE && !found_stairs; x++) {
    for (int y = 0; y < SEEY * MAPSIZE && !found_stairs; y++) {
     int sx = (startx + x) % (SEEX * MAPSIZE),
         sy = (starty + y) % (SEEY * MAPSIZE);
     if (m.has_flag(goes_up, sx, sy) || m.has_flag(goes_down, sx, sy)) {
      found_stairs = true;
      int mposx = sx, mposy = sy;
      int tries = 0;
      while (!is_empty(mposx, mposy) && tries < 10) {
       mposx = sx + rng(-2, 2);
       mposy = sy + rng(-2, 2);
       tries++;
      }
      if (tries < 10) {
       coming_to_stairs[i].mon.pos.x = sx;
       coming_to_stairs[i].mon.pos.y = sy;
       z.push_back( coming_to_stairs[i].mon );
       if (u_see(sx, sy))
        messages.add("A %s comes %s the %s!", coming_to_stairs[i].mon.name().c_str(),
                (m.has_flag(goes_up, sx, sy) ? "down" : "up"),
                m.tername(sx, sy).c_str());
      }
     }
    }
   }
   coming_to_stairs.erase(coming_to_stairs.begin() + i);
   i--;
  }
 }
 if (coming_to_stairs.empty()) monstair = tripoint(-1,-1,999);
}

void game::spawn_mon(int shiftx, int shifty)
{
 const int nlevx = lev.x + shiftx;
 const int nlevy = lev.y + shifty;
 // Create a new NPC?
 if (!no_npc && one_in(100 + 15 * cur_om.npcs.size())) {
  npc tmp;
  tmp.normalize(this);
  tmp.randomize(this);
  //tmp.stock_missions(this);
  tmp.spawn_at(&cur_om, lev.x + (1 * rng(-5, 5)), lev.y + (1 * rng(-5, 5)));
  tmp.posx = SEEX * 2 * (tmp.mapx - lev.x) + rng(0 - SEEX, SEEX);
  tmp.posy = SEEY * 2 * (tmp.mapy - lev.y) + rng(0 - SEEY, SEEY);
  tmp.form_opinion(&u);
  tmp.attitude = NPCATT_TALK;
  tmp.mission = NPC_MISSION_NULL;
  int mission_index = reserve_random_mission(ORIGIN_ANY_NPC,
                                             om_location(), tmp.id);
  if (mission_index != -1)
  tmp.chatbin.missions.push_back(mission_index);
  active_npc.push_back(tmp);
 }

// Now, spawn monsters (perhaps)
 for (int i = 0; i < cur_om.zg.size(); i++) { // For each valid group...
  int group = 0;
  int dist = trig_dist(nlevx, nlevy, cur_om.zg[i].pos);
  int pop = cur_om.zg[i].population;
  int rad = cur_om.zg[i].radius;
  if (dist <= rad) {
// (The area of the group's territory) in (population/square at this range)
// chance of adding one monster; cap at the population OR 16
   while (long((1.0 - double(dist / rad)) * pop) > rng(0, pow(rad, 2.0)) &&
          rng(0, MAPSIZE * 4) > group && group < pop && group < MAPSIZE * 3)
    group++;

   cur_om.zg[i].population -= group;

   if (group > 0) // If we spawned some zombies, advance the timer
    nextspawn += rng(group * 4 + z.size() * 4, group * 10 + z.size() * 10);

   for (int j = 0; j < group; j++) {	// For each monster in the group...
    mon_id type = valid_monster_from(mongroup::moncats[cur_om.zg[i].type]);
	if (type == mon_null) break;	// No monsters may be spawned; not soon enough?
    monster zom = monster(mtype::types[type]);
    int iter = 0;
	int monx, mony;
	do {
      if (shiftx == 0 && shifty == 0) {
       if (one_in(2)) shiftx = 1 - 2 * rng(0, 1);
       else shifty = 1 - 2 * rng(0, 1);
      }
      if (shiftx == -1) monx = (SEEX * MAPSIZE) / 6;
      else if (shiftx == 1) monx = (SEEX * MAPSIZE * 5) / 6;
	  else monx = rng(0, SEEX * MAPSIZE - 1);
      if (shifty == -1) mony = (SEEY * MAPSIZE) / 6;
      else if (shifty == 1) mony = (SEEY * MAPSIZE * 5) / 6;
	  else mony = rng(0, SEEY * MAPSIZE - 1);
      monx += rng(-5, 5);
      mony += rng(-5, 5);
      iter++;

     } while ((!zom.can_move_to(m, monx, mony) || !is_empty(monx, mony) ||
                m.sees(u.posx, u.posy, monx, mony, SEEX) ||
                rl_dist(u.posx, u.posy, monx, mony) < 8) && iter < 50);
     if (iter < 50) {
      zom.spawn(monx, mony);
      z.push_back(zom);
     }
   }	// Placing monsters of this group is done!
   if (cur_om.zg[i].population <= 0) { // Last monster in the group spawned...
    cur_om.zg.erase(cur_om.zg.begin() + i); // ...so remove that group
    i--;	// And don't increment i.
   }
  }
 }
}

mon_id game::valid_monster_from(std::vector<mon_id> group)
{
 std::vector<mon_id> valid;
 int rntype = 0;
 for(auto m_id : group) {
  const mtype* const type = mtype::types[m_id];
  if (type->frequency > 0 &&
      int(messages.turn) + 900 >= MINUTES(STARTING_MINUTES) + HOURS(type->difficulty)){
   valid.push_back(m_id);
   rntype += type->frequency;
  }
 }
 if (valid.empty()) return mon_null;
 int curmon = -1;
 if (rntype > 0)
  rntype = rng(0, rntype - 1);	// rntype set to [0, rntype)
 do {
  curmon++;
  rntype -= mtype::types[valid[curmon]]->frequency;
 } while (rntype > 0);
 return valid[curmon];
}


int game::valid_group(mon_id type, int x, int y)
{
 std::vector <int> valid_groups;
 std::vector <int> semi_valid;	// Groups that're ALMOST big enough
 for (int i = 0; i < cur_om.zg.size(); i++) {
  const int dist = trig_dist(x, y, cur_om.zg[i].pos);
  const auto& tmp_ids = mongroup::moncats[cur_om.zg[i].type];
  if (dist < cur_om.zg[i].radius) {
   for(auto tmp_monid : tmp_ids) {
    if (type == tmp_monid) {
     valid_groups.push_back(i);
	 break;
    }
   }
  } else if (dist < cur_om.zg[i].radius + 3) {
   for(auto tmp_monid : tmp_ids) {
    if (type == tmp_monid) {
     semi_valid.push_back(i);
	 break;
	}
   }
  }
 }
 if (valid_groups.size() == 0) {
  if (semi_valid.empty()) return -1;
// If there's a group that's ALMOST big enough, expand that group's radius
// by one and absorb into that group.
  const int semi = rng(0, semi_valid.size() - 1);
  cur_om.zg[semi_valid[semi]].radius++;
  return semi_valid[semi];
 }
 return valid_groups[rng(0, valid_groups.size() - 1)];
}

void game::wait()
{
 char ch = menu("Wait for how long?", "5 Minutes", "30 Minutes", "1 hour",
                "2 hours", "3 hours", "6 hours", "Exit", NULL);
 int time;
 if (ch == 7)
  return;
 switch (ch) {
  case 1: time =   5000; break;
  case 2: time =  30000; break;
  case 3: time =  60000; break;
  case 4: time = 120000; break;
  case 5: time = 180000; break;
  case 6: time = 360000; break;
 }
 u.assign_activity(ACT_WAIT, time, 0);
 u.moves = 0;
}

void game::gameover()
{
 erase();
 gamemode->game_over(this);
 mvprintw(0, 35, "GAME OVER");
 inv();
}

void game::write_msg()
{
 messages.write(w_messages);
}

void game::msg_buffer()
{
 messages.buffer();
 refresh_all();
}

void game::teleport(player *p)
{
 if (p == NULL) p = &u;
 int newx, newy, tries = 0;
 const bool is_u = (p == &u);
 p->add_disease(DI_TELEGLOW, 300, this);
 do {
  newx = p->posx + rng(0, SEEX * 2) - SEEX;
  newy = p->posy + rng(0, SEEY * 2) - SEEY;
  tries++;
 } while (tries < 15 && !is_empty(newx, newy));
 const bool can_see = (is_u || u_see(newx, newy));
 std::string You = (is_u ? "You" : p->name);
 p->posx = newx;
 p->posy = newy;
 if (tries == 15) {
  if (m.move_cost(newx, newy) == 0) {	// TODO: If we land in water, swim
   if (can_see)
    messages.add("%s teleport%s into the middle of a %s!", You.c_str(),
            (is_u ? "" : "s"), m.tername(newx, newy).c_str());
   p->hurt(this, bp_torso, 0, 500);
  } else if (monster* const m_at = mon(newx, newy)) {
     if (can_see)
       messages.add("%s teleport%s into the middle of a %s!", You.c_str(),
              (is_u ? "" : "s"), m_at->name().c_str());
     explode_mon(*m_at);
   }
 }
 if (is_u) update_map(u.posx, u.posy);
}

void game::nuke(int x, int y)
{
 overmap tmp_om = cur_om;
 cur_om = overmap(this, tmp_om.pos.x, tmp_om.pos.y, 0);
 if (x < 0 || y < 0 || x >= OMAPX || y >= OMAPY) return;
 int mapx = x * 2, mapy = y * 2;
 map tmpmap;
 tmpmap.load(this, mapx, mapy);
 for (int i = 0; i < SEEX * 2; i++) {
  for (int j = 0; j < SEEY * 2; j++) {
   if (!one_in(10)) tmpmap.ter(i, j) = t_rubble;
   if (one_in(3)) tmpmap.add_field(NULL, i, j, fd_nuke_gas, 3);
   tmpmap.radiation(i, j) += rng(20, 80);
  }
 }
 tmpmap.save(&cur_om, messages.turn, mapx, mapy);
 cur_om.ter(x, y) = ot_crater;
 cur_om = tmp_om;
}

std::vector<faction *> game::factions_at(int x, int y)
{
 std::vector<faction *> ret;
 for (int i = 0; i < factions.size(); i++) {
  if (factions[i].om.x == cur_om.pos.x && factions[i].om.y == cur_om.pos.y &&
      trig_dist(x, y, factions[i].map) <= factions[i].size)
   ret.push_back(&(factions[i]));
 }
 return ret;
}

oter_id game::ter_at(int omx, int omy, bool& mark_as_seen)
{
 oter_id ret;
 int sx = 0, sy = 0;
 if (omx >= OMAPX) sx = 1;
 else if (omx < 0) sx = -1;
 if (omy >= OMAPY) sy = 1;
 else if (omy < 0) sy = -1;
 if (sx != 0 || sy != 0) {
  omx -= sx * OMAPX;
  omy -= sy * OMAPY;
  overmap tmp(this, cur_om.pos.x + sx, cur_om.pos.y + sy, 0);
  if (mark_as_seen) {
   tmp.seen(omx, omy) = true;
   tmp.save(u.name, tmp.pos.x, tmp.pos.y, cur_om.pos.z);
  } else {
   mark_as_seen = tmp.seen(omx, omy);
  }
  ret = tmp.ter(omx, omy);
 } else {
  ret = cur_om.ter(omx, omy);
  if (mark_as_seen)
   cur_om.seen(omx, omy) = true;
  else
   mark_as_seen = cur_om.seen(omx, omy);
 }
 return ret;
}

nc_color sev(int a)
{
 switch (a) {
  case 0: return c_cyan;
  case 1: return c_blue;
  case 2: return c_green;
  case 3: return c_yellow;
  case 4: return c_ltred;
  case 5: return c_red;
  case 6: return c_magenta;
 }
 return c_dkgray;
}

void game::display_scent()
{
 int div = query_int("Sensitivity");
 draw_ter();
 for (int x = u.posx - SEEX; x <= u.posx + SEEX; x++) {
  for (int y = u.posy - SEEY; y <= u.posy + SEEY; y++) {
   int sn = scent(x, y) / (div * 2);
   mvwprintz(w_terrain, SEEY + y - u.posy, SEEX + x - u.posx, sev(sn), "%d",
             sn % 10);
  }
 }
 wrefresh(w_terrain);
 getch();
}

void intro()
{
 int maxx, maxy;
 getmaxyx(stdscr, maxy, maxx);
 WINDOW* tmp = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 while (maxy < VIEW || maxx < SCREEN_WIDTH) {
  werase(tmp);
  wprintw(tmp, "Whoa. Whoa. Hey. This game requires a minimum terminal size of 80x25. I'm\n\
sorry if your graphical terminal emulator went with the woefully-diminuitive\n\
80x24 as its default size, but that just won't work here.  Now stretch the\n\
bottom of your window downward so you get an extra line.\n");
  wrefresh(tmp);
  refresh();
  wrefresh(tmp);
  getch();
  getmaxyx(stdscr, maxy, maxx);
 }
 werase(tmp);
 wrefresh(tmp);
 delwin(tmp);
 erase();
}
