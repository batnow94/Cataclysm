#include "map.h"
#include "output.h"
#include "game.h"
#include "rng.h"
#include "line.h"
#include "recent_msg.h"
#include "om_cache.hpp"
#include "Zaimoni.STL/Logging.h"
#include "zero.h"
#include "stl_typetraits_late.h"

ter_id grass_or_dirt()
{
 if (one_in(4))
  return t_grass;
 return t_dirt;
}

enum room_type {
 room_null,
 room_closet,
 room_lobby,
 room_chemistry,
 room_teleport,
 room_goo,
 room_cloning,
 room_vivisect,
 room_bionics,
 room_dorm,
 room_living,
 room_bathroom,
 room_kitchen,
 room_bedroom,
 room_mine_shaft,
 room_mine_office,
 room_mine_storage,
 room_mine_fuel,
 room_mine_housing,
 room_bunker_bots,
 room_bunker_launcher,
 room_bunker_rifles,
 room_bunker_grenades,
 room_bunker_armor,
 room_mansion_courtyard,
 room_mansion_entry,
 room_mansion_bedroom,
 room_mansion_library,
 room_mansion_kitchen,
 room_mansion_dining,
 room_mansion_game,
 room_mansion_pool,
 room_mansion_bathroom,
 room_mansion_gallery,
 room_split
};

bool connects_to(oter_id there, int dir_from_here);
void house_room(map *m, room_type type, int x1, int y1, int x2, int y2);
void science_room(map *m, int x1, int y1, int x2, int y2, int rotate);
void set_science_room(map *m, int x1, int y1, bool faces_right, int turn);
void silo_rooms(map *m);
void build_mine_room(map *m, room_type type, int x1, int y1, int x2, int y2);

void build_mansion_room(map *m, room_type type, int x1, int y1, int x2, int y2);
void mansion_room(map *m, int x1, int y1, int x2, int y2); // pick & build

// \todo these functions may need to clear inventory as well
static void line(map *m, ter_id type, int x1, int y1, int x2, int y2)
{
	for (const auto& pt : line_to(x1, y1, x2, y2, 0)) m->ter(pt) = type;
	m->ter(x1, y1) = type;
}

static void line(map *m, ter_id type, const point& pt1, const point& pt2)
{
	for (const auto& pt : line_to(pt1, pt2, 0)) m->ter(pt) = type;
	m->ter(pt1) = type;
}

static void square(map *m, ter_id type, int x1, int y1, int x2, int y2)
{
	for (int x = x1; x <= x2; x++) {
		for (int y = y1; y <= y2; y++)
			m->ter(x, y) = type;
	}
}

static void square(map *m, ter_id type, const point& pt1, const point& pt2)
{
	for (int x = pt1.x; x <= pt2.x; x++) {
		for (int y = pt1.y; y <= pt2.y; y++)
			m->ter(x, y) = type;
	}
}

static void rough_circle(map *m, ter_id type, int x, int y, int rad)
{
	for (int i = x - rad; i <= x + rad; i++) {
		for (int j = y - rad; j <= y + rad; j++) {
			if (rl_dist(x, y, i, j) + rng(0, 3) <= rad)
				m->ter(i, j) = type;
		}
	}
}

static map_extra random_map_extra(const map_extras& embellishments)
{
	int pick = 0;
	// Set pick to the total of all the chances for map extras
	for (const auto it : embellishments.chances) pick += it;
	// Set pick to a number between 0 and the total
	pick = rng(0, pick - 1);
	int choice = -1;
	while (pick >= 0) pick -= embellishments.chances[++choice];
	return map_extra(choice);
}

submap::submap()
: active_item_count(0), field_count(0)	// turn_last_touched omitted, will be caught later
{
	memset(ter, 0, sizeof(ter));
	memset(trp, 0, sizeof(trp));
	memset(rad, 0, sizeof(rad));
}

static std::pair<point, point> overmap_delta(int x, int y)
{
    point over(x / 2, y / 2);
    point shift(0, 0);
    if (OMAPX * 2 <= x) {
        shift.x = 1;
        over.x = (x - 2 * OMAPX) / 2;
    } else if (0 > x) {
        shift.x = -1;
        over.x = (x + 2 * OMAPX) / 2;
    }
    if (OMAPY * 2 <= y) {
        shift.y = 1;
        over.y = (y - 2 * OMAPY) / 2;
    } else if (0 > y) {
        shift.y = -1;
        over.y = (y + 2 * OMAPY) / 2;
    }
    assert(0 <= over.x && OMAPX > over.x);
    assert(0 <= over.y && OMAPY > over.y);
    return std::pair(over, shift);
}

void map::generate(game *g, overmap *om, int x, int y)
{
  const int turn = int(messages.turn);
// First we have to create new submaps and initialize them to 0 all over
// We create all the submaps, even if we're not a tinymap, so that map
//  generation which overflows won't cause a crash.  At the bottom of this
//  function, we save the upper-left 4 submaps, and delete the rest.
 for (int i = 0; i < my_MAPSIZE * my_MAPSIZE; i++) {
  (grid[i] = new submap)->turn_last_touched = turn;
 }

 unsigned zones = 0;
 const auto physical = overmap_delta(x, y);
 overmap& om_actual = (0 == physical.second.x && 0 == physical.second.y) ? *om : om_cache::get().create(tripoint(om->pos.x+physical.second.x,om->pos.y + physical.second.y,om->pos.z));
 const oter_id t_above = (om_actual.pos.z < 0 || om_actual.pos.z == TUTORIAL_Z - 1)
     ? overmap::ter_c(OM_loc(tripoint(om_actual.pos.x, om_actual.pos.y, om_actual.pos.z + 1), physical.first))
     : ot_null;
 const oter_id terrain_type = om_actual.ter(physical.first);
 const oter_id t_north = overmap::ter_c(OM_loc(om_actual.pos, physical.first + Direction::N));
 const oter_id t_east = overmap::ter_c(OM_loc(om_actual.pos, physical.first + Direction::E));
 const oter_id t_south = overmap::ter_c(OM_loc(om_actual.pos, physical.first + Direction::S));
 const oter_id t_west = overmap::ter_c(OM_loc(om_actual.pos, physical.first + Direction::W));

// Okay, we know who are neighbors are.  Let's draw!
 draw_map(terrain_type, t_north, t_east, t_south, t_west, t_above, turn, g);
 //auto zones = om_actual.zones(physical.first);
 decltype(auto) embellish = oter_t::list[terrain_type].embellishments;
 if (one_in(embellish.chance)) add_extra(random_map_extra(embellish), g);
 else if (   _force_map_extra && 0 < embellish.chances[_force_map_extra]
          && (0 > _force_map_extra_pos.x || _force_map_extra_pos==point(x,y))) {
     debugmsg("map extra forced: (%d,%d)", x, y);
     add_extra(_force_map_extra, g);
     _force_map_extra = mx_null;
     _force_map_extra_pos = point(-1, -1);
 }

 post_process(g, zones);

// And finally save used submaps and delete the rest.
 for (int i = 0; i < my_MAPSIZE; i++) {
  for (int j = 0; j < my_MAPSIZE; j++) {
   if (i <= 1 && j <= 1)
    saven(om->pos, turn, point(x, y), i, j); // should be ok w/out of bounds x,y
   else
    delete grid[i + j * my_MAPSIZE];
  }
 }
}

// policy: make the caller responsible for the correct y range (historically non-strict upper bound is y0+5)
void map::apply_temple_switch(ter_id trigger, int y0, int x, int y)
{
	auto& t = ter(x, y);
	switch (trigger) {
	case t_switch_rg:
		if (t_rock_red == t) t = t_floor_red;
		else if (t_floor_red == t) t = t_rock_red;
		else if (t_rock_green == t) t = t_floor_green;
		else if (t_floor_green == t) t = t_rock_green;
		break;
	case t_switch_gb:
		if (t_rock_blue == t) t = t_floor_blue;
		else if (t_floor_blue == t) t = t_rock_blue;
		else if (t_rock_green == t) t = t_floor_green;
		else if (t_floor_green == t) t = t_rock_green;
		break;
	case t_switch_rb:
		if (t_rock_blue == t) t = t_floor_blue;
		else if (t_floor_blue == t) t = t_rock_blue;
		else if (t_rock_red == t) t = t_floor_red;
		else if (t_floor_red == t) t = t_rock_red;
		break;
	case t_switch_even:
		if ((y - y0) % 2 == 1) {
			if (t_rock_red == t) t = t_floor_red;
			else if (t_floor_red == t) t = t_rock_red;
			else if (t_rock_green == t) t = t_floor_green;
			else if (t_floor_green == t) t = t_rock_green;
			else if (t_rock_blue == t) t = t_floor_blue;
			else if (t_floor_blue == t) t = t_rock_blue;
		}
		break;
	default: assert(0 && "unhandled temple switch type");
	}
}

static int forest_penetration(oter_id x)
{
    switch (x)
    {
    case ot_forest:
    case ot_forest_water: return 14;
    case ot_forest_thick: return 18;
    default: return 0;
    }
}

// for military bases
// results will be strange for diagonals
static void install_military_base_turret(map& m, point dest, direction wall_dir, ter_id v_wall)
{
    // Seal up the entrances if there's walls there
    if (m.ter(dest) != t_dirt) {
        m.ter(dest + direction_vector(wall_dir)) = (ter_id)(v_wall + any<NORTH,SOUTH>(wall_dir));  // horizontal wall when moving vertically i.e. north-south
    }
    m.add_spawn(mon_turret, 1, dest.x, dest.y);
}

// example model is iterator pointing to std::pair<item_location, int>
template<class T>
static void _place_items(map& m, T begin, const T end, point ul, point rb, bool grass = false, int turn = 0)
{
    static_assert(std::is_same_v<std::remove_cv_t<decltype(begin->first)>, items_location>);
    //  assert(begin && end && begin != end);   // \todo need mode that throws std::logic error
    do m.place_items(begin->first, begin->second, ul.x, ul.y, rb.x, rb.y, grass, turn);
    while (++begin != end);
}

static void _stock_line(map& m, ter_id dest, items_location type, int rate, point ul, point rb, bool grass = false, int turn = 0)
{
    line(&m, dest, ul, rb);
    m.place_items(type, rate, ul.x, ul.y, rb.x, rb.y, grass, turn);
}

static void _stock_square(map& m, ter_id dest, items_location type, int rate, point ul, point rb, bool grass = false, int turn = 0)
{
    square(&m, dest, ul, rb);
    m.place_items(type, rate, ul.x, ul.y, rb.x, rb.y, grass, turn);
}

static void _stock(map& m, ter_id dest, items_location type, int rate, const point& pt, bool grass = false, int turn = 0)
{
    m.ter(pt.x, pt.y) = dest;
    m.place_items(type, rate, pt.x, pt.y, pt.x, pt.y, grass, turn);
}

void map::draw_map(oter_id terrain_type, oter_id t_north, oter_id t_east,
                   oter_id t_south, oter_id t_west, oter_id t_above, int turn,
                   game *g)
{
// Big old switch statement with a case for each overmap terrain type.
// Many of these can be copied from another type, then rotated; for instance,
//  ot_house_east is identical to ot_house_north, just rotated 90 degrees to
//  the right.  The rotate(int) function is at the bottom of this file.

// The place_items() function takes a mapitems type (see mapitems.h and
//  mapitemsdef.cpp), an "odds" int giving the chance for a single item to be
//  placed, four ints (x1, y1, x2, y2) corresponding to the upper left corner
//  and lower right corner of a square where the items are placed, a boolean
//  that indicates whether items may spawn on grass & dirt, and finally an
//  integer that indicates on which turn the items were created.  This final
//  integer should be 0, unless the items are "fresh-grown" like wild fruit.

 int rn, lw, rw, mw, tw, bw, cw, x, y;
 int n_fac = 0, e_fac = 0, s_fac = 0, w_fac = 0;
 computer *tmpcomp = NULL;

 switch (terrain_type) {
 case ot_null:  // historical.  Duplicates submap constructor effects explicitly
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    ter(i, j) = t_null;
    radiation(i, j) = 0;
   }
  }
  break;

 case ot_crater:
  if (t_north != ot_crater) n_fac = 6;
  if (t_east  != ot_crater) e_fac = 6;
  if (t_south != ot_crater) s_fac = 6;
  if (t_west  != ot_crater) w_fac = 6;

  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (rng_lte(0, w_fac, i) && rng_lte(0, e_fac, SEEX * 2 - 1 - i) &&
        rng_lte(0, n_fac, j) && rng_lte(0, s_fac, SEEX * 2 - 1 - j)   ) {
     ter(i, j) = t_rubble;
     radiation(i, j) = rng(0, 4) * rng(0, 2);
    } else {
     ter(i, j) = t_dirt;
     radiation(i, j) = rng(0, 2) * rng(0, 2) * rng(0, 2);
    }
   }
  }
  place_items(mi_wreckage, 83, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  break;

 case ot_field:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  place_items(mi_field, 60, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, turn);
  break;

 case ot_forest:
 case ot_forest_thick:
 case ot_forest_water:
  n_fac = forest_penetration(t_north);
  e_fac = forest_penetration(t_east);
  s_fac = forest_penetration(t_south);
  w_fac = forest_penetration(t_west);
  switch (terrain_type) {
  case ot_forest_thick:
   n_fac += 8;
   e_fac += 8;
   s_fac += 8;
   w_fac += 8;
   break;
  case ot_forest_water:
   n_fac += 4;
   e_fac += 4;
   s_fac += 4;
   w_fac += 4;
   break;
  }
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    int forest_chance = 0, num = 0;
    if (const auto test = n_fac - j; 0 < test) {
        forest_chance += test;
        num++;
    }
    if (const auto test = e_fac - (SEEX * 2 - 1 - i); 0 < test) {
        forest_chance += test;
        num++;
    }
    if (const auto test = s_fac - (SEEY * 2 - 1 - j); 0 < test) {
        forest_chance += test;
        num++;
    }
    if (const auto test = w_fac - i; 0 < test) {
        forest_chance += test;
        num++;
    }
    if (num > 1) forest_chance /= num;
    rn = rng(0, forest_chance);
    if ((forest_chance > 0 && rn > 13) || one_in(100 - forest_chance))
     ter(i, j) = t_tree;
    else if ((forest_chance > 0 && rn > 10) || one_in(100 - forest_chance))
     ter(i, j) = t_tree_young;
    else if ((forest_chance > 0 && rn >  9) || one_in(100 - forest_chance))
     ter(i, j) = t_underbrush;
    else
     ter(i, j) = t_dirt;
   }
  }
  place_items(mi_forest, 60, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, turn);

  if (terrain_type == ot_forest_water) {
// Reset *_fac to handle where to place water
   if (t_north == ot_forest_water) n_fac = 2;
   else if (t_north >= ot_river_center && t_north <= ot_river_nw) n_fac = 3;
   else if (t_north == ot_forest || t_north == ot_forest_thick) n_fac = 1;
   else n_fac = 0;
   if (t_east == ot_forest_water) e_fac = 2;
   else if (t_east >= ot_river_center && t_east <= ot_river_nw) e_fac = 3;
   else if (t_east == ot_forest || t_east == ot_forest_thick) e_fac = 1;
   else e_fac = 0;
   if (t_south == ot_forest_water) s_fac = 2;
   else if (t_south >= ot_river_center && t_south <= ot_river_nw) s_fac = 3;
   else if (t_south == ot_forest || t_south == ot_forest_thick) s_fac = 1;
   else s_fac = 0;
   if (t_west == ot_forest_water) w_fac = 2;
   else if (t_west >= ot_river_center && t_west <= ot_river_nw) w_fac = 3;
   else if (t_west == ot_forest || t_west == ot_forest_thick) w_fac = 1;
   else w_fac = 0;
   x = SEEX / 2 + rng(0, SEEX), y = SEEY / 2 + rng(0, SEEY);
   for (int i = 0; i < 20; i++) {
    if (x >= 0 && x < SEEX * 2 && y >= 0 && y < SEEY * 2) {
	 auto& t = ter(x, y);
     if (t_water_sh == t) t = t_water_dp;
     else if (t_dirt == t || t_underbrush == t) t = t_water_sh;
    } else break;

    x += rng(-2, 2);
    y += rng(-2, 2);
    if (x < 0 || x >= SEEX * 2) x = SEEX / 2 + rng(0, SEEX);
    if (y < 0 || y >= SEEY * 2) y = SEEY / 2 + rng(0, SEEY);
    for (int j = 0; j < n_fac; j++) {
     int wx = rng(0, SEEX * 2 -1), wy = rng(0, SEEY - 1);
	 auto& t = ter(wx, wy);
	 if (t_dirt == t || t_underbrush == t) t = t_water_sh;
	}
    for (int j = 0; j < e_fac; j++) {
     int wx = rng(SEEX, SEEX * 2 - 1), wy = rng(0, SEEY * 2 - 1);
	 auto& t = ter(wx, wy);
	 if (t_dirt == t || t_underbrush == t) t = t_water_sh;
	}
    for (int j = 0; j < s_fac; j++) {
     int wx = rng(0, SEEX * 2 - 1), wy = rng(SEEY, SEEY * 2 - 1);
	 auto& t = ter(wx, wy);
	 if (t_dirt == t || t_underbrush == t) t = t_water_sh;
	}
    for (int j = 0; j < w_fac; j++) {
     int wx = rng(0, SEEX - 1), wy = rng(0, SEEY * 2 - 1);
	 auto& t = ter(wx, wy);
	 if (t_dirt == t ||  t_underbrush == t) t = t_water_sh;
    }
   }
   rn = rng(0, 2) * rng(0, 1) * (rng(0, 1) + rng(0, 1));    // 1/24 1, 1/24 2, 11/12 0
   for (int i = 0; i < rn; i++) {
    x = rng(0, SEEX * 2 - 1);
    y = rng(0, SEEY * 2 - 1);
    add_trap(x, y, tr_sinkhole);
	auto& t = ter(x, y);
	if (t_water_sh != t) t = t_dirt;
   }
  }

  if (one_in(100)) { // One in 100 forests has a spider living in it :o
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEX * 2; j++) {
	 auto& t = ter(i, j);
     if ((t_dirt == t || t_underbrush == t) && !one_in(3))
      field_at(i, j) = field(fd_web, rng(1, 3));
    }
   }
   add_spawn(mon_spider_web, rng(1, 2), SEEX, SEEY);
  }
  break;

 case ot_hive:
// Start with a basic forest pattern
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    rn = rng(0, 14);
    if (rn > 13) {
     ter(i, j) = t_tree;
    } else if (rn > 11) {
     ter(i, j) = t_tree_young;
    } else if (rn > 10) {
     ter(i, j) = t_underbrush;
    } else {
     ter(i, j) = t_dirt;
    }
   }
  }
  
// j and i loop through appropriate hive-cell center squares
  for (int j = 5; j < SEEY * 2 - 5; j += 6) {
   for (int i = (j == 5 || j == 17 ? 3 : 6); i < SEEX * 2 - 5; i += 6) {
    if (!one_in(8)) {
// Caps are always there
     ter(i    , j - 5) = t_wax;
     ter(i    , j + 5) = t_wax;
     for (int k = -2; k <= 2; k++) {
      for (int l = -1; l <= 1; l++)
       ter(i + k, j + l) = t_floor_wax;
     }
     add_spawn(mon_bee, 2, i, j);
     ter(i    , j - 3) = t_floor_wax;
     ter(i    , j + 3) = t_floor_wax;
     ter(i - 1, j - 2) = t_floor_wax;
     ter(i    , j - 2) = t_floor_wax;
     ter(i + 1, j - 2) = t_floor_wax;
     ter(i - 1, j + 2) = t_floor_wax;
     ter(i    , j + 2) = t_floor_wax;
     ter(i + 1, j + 2) = t_floor_wax;

// Up to two of these get skipped; an entrance to the cell
     int skip1 = rng(0, 23);
     int skip2 = rng(0, 23);

     ter(i - 1, j - 4) = t_wax;
     ter(i    , j - 4) = t_wax;
     ter(i + 1, j - 4) = t_wax;
     ter(i - 2, j - 3) = t_wax;
     ter(i - 1, j - 3) = t_wax;
     ter(i + 1, j - 3) = t_wax;
     ter(i + 2, j - 3) = t_wax;
     ter(i - 3, j - 2) = t_wax;
     ter(i - 2, j - 2) = t_wax;
     ter(i + 2, j - 2) = t_wax;
     ter(i + 3, j - 2) = t_wax;
     ter(i - 3, j - 1) = t_wax;
     ter(i - 3, j    ) = t_wax;
     ter(i - 3, j - 1) = t_wax;
     ter(i - 3, j + 1) = t_wax;
     ter(i - 3, j    ) = t_wax;
     ter(i - 3, j + 1) = t_wax;
     ter(i - 2, j + 3) = t_wax;
     ter(i - 1, j + 3) = t_wax;
     ter(i + 1, j + 3) = t_wax;
     ter(i + 2, j + 3) = t_wax;
     ter(i - 1, j + 4) = t_wax;
     ter(i    , j + 4) = t_wax;
     ter(i + 1, j + 4) = t_wax;

     if (skip1 ==  0 || skip2 ==  0)
      ter(i - 1, j - 4) = t_floor_wax;
     if (skip1 ==  1 || skip2 ==  1)
      ter(i    , j - 4) = t_floor_wax;
     if (skip1 ==  2 || skip2 ==  2)
      ter(i + 1, j - 4) = t_floor_wax;
     if (skip1 ==  3 || skip2 ==  3)
      ter(i - 2, j - 3) = t_floor_wax;
     if (skip1 ==  4 || skip2 ==  4)
      ter(i - 1, j - 3) = t_floor_wax;
     if (skip1 ==  5 || skip2 ==  5)
      ter(i + 1, j - 3) = t_floor_wax;
     if (skip1 ==  6 || skip2 ==  6)
      ter(i + 2, j - 3) = t_floor_wax;
     if (skip1 ==  7 || skip2 ==  7)
      ter(i - 3, j - 2) = t_floor_wax;
     if (skip1 ==  8 || skip2 ==  8)
      ter(i - 2, j - 2) = t_floor_wax;
     if (skip1 ==  9 || skip2 ==  9)
      ter(i + 2, j - 2) = t_floor_wax;
     if (skip1 == 10 || skip2 == 10)
      ter(i + 3, j - 2) = t_floor_wax;
     if (skip1 == 11 || skip2 == 11)
      ter(i - 3, j - 1) = t_floor_wax;
     if (skip1 == 12 || skip2 == 12)
      ter(i - 3, j    ) = t_floor_wax;
     if (skip1 == 13 || skip2 == 13)
      ter(i - 3, j - 1) = t_floor_wax;
     if (skip1 == 14 || skip2 == 14)
      ter(i - 3, j + 1) = t_floor_wax;
     if (skip1 == 15 || skip2 == 15)
      ter(i - 3, j    ) = t_floor_wax;
     if (skip1 == 16 || skip2 == 16)
      ter(i - 3, j + 1) = t_floor_wax;
     if (skip1 == 17 || skip2 == 17)
      ter(i - 2, j + 3) = t_floor_wax;
     if (skip1 == 18 || skip2 == 18)
      ter(i - 1, j + 3) = t_floor_wax;
     if (skip1 == 19 || skip2 == 19)
      ter(i + 1, j + 3) = t_floor_wax;
     if (skip1 == 20 || skip2 == 20)
      ter(i + 2, j + 3) = t_floor_wax;
     if (skip1 == 21 || skip2 == 21)
      ter(i - 1, j + 4) = t_floor_wax;
     if (skip1 == 22 || skip2 == 22)
      ter(i    , j + 4) = t_floor_wax;
     if (skip1 == 23 || skip2 == 23)
      ter(i + 1, j + 4) = t_floor_wax;

     if (t_north == ot_hive && t_east == ot_hive && t_south == ot_hive && t_west == ot_hive)
      place_items(mi_hive_center, 90, i - 2, j - 2, i + 2, j + 2, false, turn);
     else
      place_items(mi_hive, 80, i - 2, j - 2, i + 2, j + 2, false, turn);
    }
   }
  }
  break;

 case ot_spider_pit:
// First generate a forest
  n_fac = forest_penetration(t_north);
  e_fac = forest_penetration(t_east);
  s_fac = forest_penetration(t_south);
  w_fac = forest_penetration(t_west);
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    int forest_chance = 0, num = 0;
    if (const auto test = n_fac - j; 0 < test) {
        forest_chance += test;
        num++;
    }
    if (const auto test = e_fac - (SEEX * 2 - 1 - i); 0 < test) {
        forest_chance += test;
        num++;
    }
    if (const auto test = s_fac - (SEEY * 2 - 1 - j); 0 < test) {
        forest_chance += test;
        num++;
    }
    if (const auto test = w_fac - i; 0 < test) {
        forest_chance += test;
        num++;
    }
    if (num > 1) forest_chance /= num;
    rn = rng(0, forest_chance);
    if ((forest_chance > 0 && rn > 13) || one_in(100 - forest_chance))
     ter(i, j) = t_tree;
    else if ((forest_chance > 0 && rn > 10) || one_in(100 - forest_chance))
     ter(i, j) = t_tree_young;
    else if ((forest_chance > 0 && rn >  9) || one_in(100 - forest_chance))
     ter(i, j) = t_underbrush;
    else
     ter(i, j) = t_dirt;
   }
  }
  place_items(mi_forest, 60, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, turn);
// Next, place webs and sinkholes
  for (int i = 0; i < 4; i++) {
   int x = rng(3, SEEX * 2 - 4), y = rng(3, SEEY * 2 - 4);
   if (i == 0)
    ter(x, y) = t_slope_down;
   else {
    ter(x, y) = t_dirt;
    add_trap(x, y, tr_sinkhole);
   }
   for (int x1 = x - 3; x1 <= x + 3; x1++) {
    for (int y1 = y - 3; y1 <= y + 3; y1++) {
     field_at(x1, y1) = field(fd_web, rng(2, 3));
	 rewrite_inv<t_slope_down, t_dirt>(x1, y1);
    }
   }
  }
  break;

 case ot_fungal_bloom:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (one_in(10)) ter(i, j) = t_tree_fungal;
    else if (one_in(300)) {
     ter(i, j) = t_marloss;
     add_item(i, j, item::types[itm_marloss_berry], turn);
    } else ter(i, j) = one_in(3) ? t_dirt : t_fungus;
   }
  }
  square(this, t_fungus, point(SEE - 3), point(SEE + 3));
  add_spawn(mon_fungaloid_queen, 1, 12, 12);
  break;

 case ot_road_ns:
 case ot_road_ew:
  if ((t_west  >= ot_house_north && t_west  <= ot_sub_station_west) ||
      (t_east  >= ot_house_north && t_east  <= ot_sub_station_west) ||
      (t_north >= ot_house_north && t_north <= ot_sub_station_west) ||
      (t_south >= ot_house_north && t_south <= ot_sub_station_west)   )
   rn = 1;	// rn = 1 if this road has sidewalks
  else
   rn = 0;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < 4 || i >= SEEX * 2 - 4)
     ter(i, j) = (1 == rn) ? t_sidewalk : grass_or_dirt();
    else
     ter(i, j) = ((i == SEEX - 1 || i == SEEX) && j % 4 != 0) ? t_pavement_y : t_pavement;
   }
  }
  if (terrain_type == ot_road_ew) rotate(1);
  place_items(mi_road, 5, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1, false, turn);
  break;

 case ot_road_ne:
 case ot_road_es:
 case ot_road_sw:
 case ot_road_wn:
  if ((t_west  >= ot_house_north && t_west  <= ot_sub_station_west) ||
      (t_east  >= ot_house_north && t_east  <= ot_sub_station_west) ||
      (t_north >= ot_house_north && t_north <= ot_sub_station_west) ||
      (t_south >= ot_house_north && t_south <= ot_sub_station_west)   )
   rn = 1;	// rn = 1 if this road has sidewalks
  else
   rn = 0;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i >= SEEX * 2 - 4 && j < 4) || i < 4 || j >= SEEY * 2 - 4) {
     if (rn == 1)
      ter(i, j) = t_sidewalk;
     else
      ter(i, j) = grass_or_dirt();
    } else {
     if (((i == SEEX - 1 || i == SEEX) && j % 4 != 0 && j < SEEY - 1) ||
         ((j == SEEY - 1 || j == SEEY) && i % 4 != 0 && i > SEEX))
      ter(i, j) = t_pavement_y;
     else
      ter(i, j) = t_pavement;
    }
   }
  }
  if (terrain_type == ot_road_es) rotate(1);    // could enforce subtraction w/static_assert
  if (terrain_type == ot_road_sw) rotate(2);
  if (terrain_type == ot_road_wn) rotate(3);
  place_items(mi_road, 5, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1, false, turn);
  break;

 case ot_road_nes:
 case ot_road_new:
 case ot_road_nsw:
 case ot_road_esw:
  if ((t_west  >= ot_house_north && t_west  <= ot_sub_station_west) ||
      (t_east  >= ot_house_north && t_east  <= ot_sub_station_west) ||
      (t_north >= ot_house_north && t_north <= ot_sub_station_west) ||
      (t_south >= ot_house_north && t_south <= ot_sub_station_west)   )
   rn = 1;	// rn = 1 if this road has sidewalks
  else
   rn = 0;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < 4 || (i >= SEEX * 2 - 4 && (j < 4 || j >= SEEY * 2 - 4)))
     ter(i, j) = (1 == rn) ? t_sidewalk : grass_or_dirt();
    else {
     if (((i == SEEX - 1 || i == SEEX) && j % 4 != 0) ||
         ((j == SEEY - 1 || j == SEEY) && i % 4 != 0 && i > SEEX))
      ter(i, j) = t_pavement_y;
     else
      ter(i, j) = t_pavement;
    }
   }
  }
  if (terrain_type == ot_road_esw) rotate(1);    // could enforce subtraction w/static_assert
  if (terrain_type == ot_road_nsw) rotate(2);
  if (terrain_type == ot_road_new) rotate(3);
  place_items(mi_road, 5, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1, false, turn);
  break;

 case ot_road_nesw:
 case ot_road_nesw_manhole:
  if ((t_west  == ot_road_nesw || t_west  == ot_road_nesw_manhole) &&
      (t_east  == ot_road_nesw || t_east  == ot_road_nesw_manhole) &&
      (t_north == ot_road_nesw || t_north == ot_road_nesw_manhole) &&
      (t_south == ot_road_nesw || t_south == ot_road_nesw_manhole)   )
   rn = 2;	// rn = 2 if this is actually a plaza
  else
   rn = 1;	// rn = 1 if this road has sidewalks
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (rn == 2)
     ter(i, j) = t_sidewalk;
    else if ((i < 4 || i >= SEEX * 2 - 4) && (j < 4 || j >= SEEY * 2 - 4)) {
     if (rn == 1)
      ter(i, j) = t_sidewalk;
     else
      ter(i, j) = grass_or_dirt();
    } else {
     if (((i == SEEX - 1 || i == SEEX) && j % 4 != 0) ||
         ((j == SEEY - 1 || j == SEEY) && i % 4 != 0))
      ter(i, j) = t_pavement_y;
     else
      ter(i, j) = t_pavement;
    }
   }
  }
  if (rn == 2) {	// Special embellishments for a plaza
   if (one_in(10)) {	// Fountain
    for (int i = SEEX - 2; i <= SEEX + 2; i++) {
     ter(i, i) = t_water_sh;
     ter(i, SEEX * 2 - i) = t_water_sh;
    }
   }
   if (one_in(10)) {	// Small trees in center
    ter(SEEX - 1, SEEY - 2) = t_tree_young;
    ter(SEEX    , SEEY - 2) = t_tree_young;
    ter(SEEX - 1, SEEY + 2) = t_tree_young;
    ter(SEEX    , SEEY + 2) = t_tree_young;
    ter(SEEX - 2, SEEY - 1) = t_tree_young;
    ter(SEEX - 2, SEEY    ) = t_tree_young;
    ter(SEEX + 2, SEEY - 1) = t_tree_young;
    ter(SEEX + 2, SEEY    ) = t_tree_young;
   }
   if (one_in(14)) {	// Rows of small trees
    int gap = rng(2, 4);
    int start = rng(0, 4);
    for (int i = 2; i < SEEX * 2 - start; i += gap) {
     ter(i               , start) = t_tree_young;
     ter(SEEX * 2 - 1 - i, start) = t_tree_young;
     ter(start, i               ) = t_tree_young;
     ter(start, SEEY * 2 - 1 - i) = t_tree_young;
    }
   }
   place_items(mi_trash, 5, 0, 0, SEEX * 2 -1, SEEX * 2 - 1, true, 0);
  } else
   place_items(mi_road,  5, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1, false, turn);
  if (terrain_type == ot_road_nesw_manhole)
   ter(rng(6, SEEX * 2 - 6), rng(6, SEEX * 2 - 6)) = t_manhole_cover;
  break;

 case ot_bridge_ns:
 case ot_bridge_ew:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < 4 || i >= SEEX * 2 - 4)
     ter(i, j) = t_water_dp;
    else if (i == 4 || i == SEEX * 2 - 5)
     ter(i, j) = t_railing_v;
    else
     ter(i, j) = ((i == SEEX - 1 || i == SEEX) && j % 4 != 0) ? t_pavement_y : t_pavement;
   }
  }
  if (terrain_type == ot_bridge_ew) rotate(1);
  place_items(mi_road, 5, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1, false, turn);
  break;
 
 case ot_hiway_ns:
 case ot_hiway_ew:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < 3 || i >= SEEX * 2 - 3)
     ter(i, j) = grass_or_dirt();
    else if (i == 3 || i == SEEX * 2 - 4)
     ter(i, j) = t_railing_v;
    else
     ter(i, j) = ((i == SEEX - 1 || i == SEEX) && j % 4 != 0) ? t_pavement_y : t_pavement;
   }
  }
  if (terrain_type == ot_hiway_ew) rotate(1);
  place_items(mi_road, 8, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1, false, turn);
  break;

 case ot_river_center:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_water_dp;
  }
  break;

 case ot_river_c_not_ne:
 case ot_river_c_not_se:
 case ot_river_c_not_sw:
 case ot_river_c_not_nw:
  for (int i = SEEX * 2 - 1; i >= 0; i--) {
   for (int j = 0; j < SEEY * 2; j++) {
    ter(i, j) = (4 > j && SEEX * 2 - 4 <= i) ? t_water_sh : t_water_dp;
   }
  }
  if (terrain_type == ot_river_c_not_se) rotate(1); // could enforce subtraction w/static_assert
  if (terrain_type == ot_river_c_not_sw) rotate(2);
  if (terrain_type == ot_river_c_not_nw) rotate(3);
  break;

 case ot_river_north:
 case ot_river_east:
 case ot_river_south:
 case ot_river_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j < 4)
      ter(i, j) = t_water_sh;
    else
     ter(i, j) = t_water_dp;
   }
  }
  if (terrain_type == ot_river_east) rotate(1); // could enforce subtraction w/static_assert
  if (terrain_type == ot_river_south) rotate(2);
  if (terrain_type == ot_river_west) rotate(3);
  break;

 case ot_river_ne:
 case ot_river_se:
 case ot_river_sw:
 case ot_river_nw:
  for (int i = SEEX * 2 - 1; i >= 0; i--) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i >= SEEX * 2 - 4 || j < 4)
     ter(i, j) = t_water_sh;
    else
     ter(i, j) = t_water_dp;
   }
  }
  if (terrain_type == ot_river_se) rotate(1); // could enforce subtraction w/static_assert
  if (terrain_type == ot_river_sw) rotate(2);
  if (terrain_type == ot_river_nw) rotate(3);
  break;

 case ot_house_base_north:
 case ot_house_base_east:
 case ot_house_base_south:
 case ot_house_base_west:
 case ot_house_north:
 case ot_house_east:
 case ot_house_south:
 case ot_house_west:
  lw = rng(0, 4);		// West external wall
  mw = lw + rng(7, 10);		// Middle wall between bedroom & kitchen/bath
  rw = SEEX * 2 - rng(1, 5);	// East external wall
  tw = rng(1, 6);		// North external wall
  bw = SEEX * 2 - rng(2, 5);	// South external wall
  cw = tw + rng(4, 7);		// Middle wall between living room & kitchen/bed
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i > lw && i < rw && j > tw && j < bw)
     ter(i, j) = t_floor;
    else
     ter(i, j) = grass_or_dirt();
    if (i >= lw && i <= rw && (j == tw || j == bw))
     ter(i, j) = t_wall_h;
    if ((i == lw || i == rw) && j > tw && j < bw)
     ter(i, j) = t_wall_v;
   }
  }
  switch(rng(1, 3)) {
  case 1:	// Quadrants, essentially
   mw = rng(lw + 5, rw - 5);
   cw = tw + rng(4, 7);
   house_room(this, room_living,	mw, tw, rw, cw);
   house_room(this, room_kitchen,	lw, tw, mw, cw);
   ter(mw, rng(tw + 2, cw - 2)) = (one_in(3) ? t_door_c : t_floor);
   rn = rng(lw + 1, cw - 2);
   ter(rn    , tw) = t_window;
   ter(rn + 1, tw) = t_window;
   rn = rng(cw + 1, rw - 2);
   ter(rn    , tw) = t_window;
   ter(rn + 1, tw) = t_window;
   mw = rng(lw + 3, rw - 3);
   if (mw <= lw + 5) {	// Bedroom on right, bathroom on left
    rn = rng(cw + 2, rw - 2);
    if (bw - cw >= 10 && mw - lw >= 6) {
     house_room(this, room_bathroom, lw, bw - 5, mw, bw);
     house_room(this, room_bedroom, lw, cw, mw, bw - 5);
     ter(mw - 1, cw) = t_door_c;
    } else {
     if (bw - cw > 4) {	// Too big for a bathroom, not big enough for 2nd bedrm
      house_room(this, room_bathroom, lw, bw - 4, mw, bw);
      for (int i = lw + 1; i <= mw - 1; i++)
       ter(i, cw    ) = t_floor;
     } else
      house_room(this, room_bathroom, lw, cw, mw, bw);
    }
    house_room(this, room_bedroom, mw, cw, rw, bw);
    ter(mw, rng(bw - 4, bw - 1)) = t_door_c;
   } else {	// Bedroom on left, bathroom on right
    rn = rng(lw + 2, cw - 2);
    if (bw - cw >= 10 && rw - mw >= 6) {
     house_room(this, room_bathroom, mw, bw - 5, rw, bw);
     house_room(this, room_bedroom, mw, cw, rw, bw - 5);
     ter(rw - 1, cw) = t_door_c;
    } else {
     if (bw - cw > 4) {	// Too big for a bathroom, not big enough for 2nd bedrm
      house_room(this, room_bathroom, mw, bw - 4, rw, bw);
      for (int i = mw + 1; i <= rw - 1; i++)
       ter(i, cw    ) = t_floor;
     } else
      house_room(this, room_bathroom, mw, cw, rw, bw);
    }
    house_room(this, room_bedroom, lw, cw, mw, bw);
    ter(mw, rng(bw - 4, bw - 1)) = t_door_c;
   }
   ter(rn    , bw) = t_window;
   ter(rn + 1, bw) = t_window;
   if (!one_in(3)) {	// Potential side windows
    rn = rng(tw + 2, bw - 5);
    ter(rw, rn    ) = t_window;
    ter(rw, rn + 4) = t_window;
   }
   if (!one_in(3)) {	// Potential side windows
    rn = rng(tw + 2, bw - 5);
    ter(lw, rn    ) = t_window;
    ter(lw, rn + 4) = t_window;
   }
   ter(rng(lw + 1, lw + 2), cw) = t_door_c;
   if (one_in(4))
    ter(rw - 2, cw) = t_door_c;
   else
    ter(mw, rng(cw + 1, bw - 1)) = t_door_c;
   if (one_in(2)) {	// Placement of the main door
    ter(rng(lw + 2, cw - 1), tw) = (one_in(6) ? t_door_c : t_door_locked);
    if (one_in(5))
     ter(rw, rng(tw + 2, cw - 2)) = (one_in(6) ? t_door_c : t_door_locked);
   } else {
    ter(rng(cw + 1, rw - 2), tw) = (one_in(6) ? t_door_c : t_door_locked);
    if (one_in(5))
     ter(lw, rng(tw + 2, cw - 2)) = (one_in(6) ? t_door_c : t_door_locked);
   }
   break;

  case 2:	// Old-style; simple
   cw = tw + rng(3, 6);
   mw = rng(lw + 7, rw - 4);
// Plop down the rooms
   house_room(this, room_living, lw, tw, rw, cw);
   house_room(this, room_kitchen, mw, cw, rw, bw - 3);
   house_room(this, room_bedroom, lw, cw, mw, bw);
   house_room(this, room_bathroom, mw, bw - 3, rw, bw);
// Space between kitchen & living room:
   rn = rng(mw + 1, rw - 3);
   ter(rn    , cw) = t_floor;
   ter(rn + 1, cw) = t_floor;
// Front windows
   rn = rng(2, 5);
   ter(lw + rn    , tw) = t_window;
   ter(lw + rn + 1, tw) = t_window;
   ter(rw - rn    , tw) = t_window;
   ter(rw - rn + 1, tw) = t_window;
// Front door
   ter(rng(lw + 4, rw - 4), tw) = (one_in(6) ? t_door_c : t_door_locked);
   if (one_in(3)) {	// Kitchen windows
    rn = rng(cw + 1, bw - 5);
    ter(rw, rn    ) = t_window;
    ter(rw, rn + 1) = t_window;
   }
   if (one_in(3)) {	// Bedroom windows
    rn = rng(cw + 1, bw - 2);
    ter(lw, rn    ) = t_window;
    ter(lw, rn + 1) = t_window;
   }
// Door to bedroom
   if (one_in(4))
    ter(rng(lw + 1, mw - 1), cw) = t_door_c;
   else
    ter(mw, rng(cw + 3, bw - 4)) = t_door_c;
// Door to bathrom
   if (one_in(4))
    ter(mw, bw - 1) = t_door_c;
   else
    ter(rng(mw + 2, rw - 2), bw - 3) = t_door_c;
// Back windows
   rn = rng(lw + 1, mw - 2);
   ter(rn    , bw) = t_window;
   ter(rn + 1, bw) = t_window;
   rn = rng(mw + 1, rw - 1);
   ter(rn, bw) = t_window;
   break;

  case 3:	// Long center hallway
   mw = int((lw + rw) / 2);
   cw = bw - rng(5, 7);
// Hallway doors and windows
   ter(mw    , tw) = (one_in(6) ? t_door_c : t_door_locked);
   if (one_in(4)) {
    ter(mw - 1, tw) = t_window;
    ter(mw + 1, tw) = t_window;
   }
   for (int i = tw + 1; i < cw; i++) {	// Hallway walls
    ter(mw - 2, i) = t_wall_v;
    ter(mw + 2, i) = t_wall_v;
   }
   if (one_in(2)) {	// Front rooms are kitchen or living room
    house_room(this, room_living, lw, tw, mw - 2, cw);
    house_room(this, room_kitchen, mw + 2, tw, rw, cw);
   } else {
    house_room(this, room_kitchen, lw, tw, mw - 2, cw);
    house_room(this, room_living, mw + 2, tw, rw, cw);
   }
// Front windows
   rn = rng(lw + 1, mw - 4);
   ter(rn    , tw) = t_window;
   ter(rn + 1, tw) = t_window;
   rn = rng(mw + 3, rw - 2);
   ter(rn    , tw) = t_window;
   ter(rn + 1, tw) = t_window;
   if (one_in(4)) {	// Side windows?
    rn = rng(tw + 1, cw - 2);
    ter(lw, rn    ) = t_window;
    ter(lw, rn + 1) = t_window;
   }
   if (one_in(4)) {	// Side windows?
    rn = rng(tw + 1, cw - 2);
    ter(rw, rn    ) = t_window;
    ter(rw, rn + 1) = t_window;
   }
   if (one_in(2)) {	// Bottom rooms are bedroom or bathroom
    house_room(this, room_bedroom, lw, cw, rw - 3, bw);
    house_room(this, room_bathroom, rw - 3, cw, rw, bw);
    ter(rng(lw + 2, mw - 3), cw) = t_door_c;
    if (one_in(4))
     ter(rng(rw - 2, rw - 1), cw) = t_door_c;
    else
     ter(rw - 3, rng(cw + 2, bw - 2)) = t_door_c;
    rn = rng(lw + 1, rw - 5);
    ter(rn    , bw) = t_window;
    ter(rn + 1, bw) = t_window;
    if (one_in(4))
     ter(rng(rw - 2, rw - 1), bw) = t_window;
    else
     ter(rw, rng(cw + 1, bw - 1));
   } else {
    house_room(this, room_bathroom, lw, cw, lw + 3, bw);
    house_room(this, room_bedroom, lw + 3, cw, rw, bw);
    if (one_in(4))
     ter(rng(lw + 1, lw + 2), cw) = t_door_c;
    else
     ter(lw + 3, rng(cw + 2, bw - 2)) = t_door_c;
    rn = rng(lw + 4, rw - 2);
    ter(rn    , bw) = t_window;
    ter(rn + 1, bw) = t_window;
    if (one_in(4))
     ter(rng(lw + 1, lw + 2), bw) = t_window;
    else
     ter(lw, rng(cw + 1, bw - 1));
   }
// Doors off the sides of the hallway
   ter(mw - 2, rng(tw + 3, cw - 3)) = t_door_c;
   ter(mw + 2, rng(tw + 3, cw - 3)) = t_door_c;
   ter(mw, cw) = t_door_c;
   break;
  }	// Done with the various house structures

  if (rng(2, 7) < tw) {	// Big front yard has a chance for a fence
   for (int i = lw; i <= rw; i++)
    ter(i, 0) = t_fence_h;
   for (int i = 1; i < tw; i++) {
    ter(lw, i) = t_fence_v;
    ter(rw, i) = t_fence_v;
   }
   int hole = rng(SEEX - 3, SEEX + 2);
   ter(hole, 0) = t_dirt;
   ter(hole + 1, 0) = t_dirt;
   if (one_in(tw)) {
    ter(hole - 1, 1) = t_tree_young;
    ter(hole + 2, 1) = t_tree_young;
   }
  }

  if (terrain_type >= ot_house_base_north && terrain_type <= ot_house_base_west) {
   do rn = rng(lw + 1, rw - 1);
   while (ter(rn, bw - 1) != t_floor);
   ter(rn, bw - 1) = t_stairs_down;
  }
  if (one_in(100)) { // Houses have a 1 in 100 chance of wasps!
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
	 auto& t = ter(i, j);
     if (t_door_c == t || t_door_locked == t) t = t_door_frame;
     if (t_window == t && !one_in(3)) t = t_window_frame;
     if ((t_wall_h == t || t_wall_v == t) && one_in(8)) t = t_paper;
    }
   }
   int num_pods = rng(8, 12);
   for (int i = 0; i < num_pods; i++) {
    point pod(rng(1, SEEX * 2 - 2), rng(1, SEEY * 2 - 2));
    direction non = (direction)(rng(NORTH, NORTHWEST));
    for (int dir = NORTH; dir <= NORTHWEST; ++dir) {
        if (non != dir) ter(pod + direction_vector((direction)dir)) = t_paper;
    }
    add_spawn(mon_wasp, 1, pod.x, pod.y);
   }
   place_items(mi_rare, 70, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, false, turn);
    
  } else if (one_in(150)) { // No wasps; black widows?
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
     if (ter(i, j) == t_floor) {
      if (one_in(15)) {
       add_spawn(mon_spider_widow, rng(1, 2), i, j);
       for (int x = i - 1; x <= i + 1; x++) {
        for (int y = j - 1; y <= j + 1; y++) {
         if (ter(x, y) == t_floor) field_at(x, y) = field(fd_web, rng(2, 3));
        }
       }
      } else if (move_cost(i, j) > 0 && field_at(i, j).is_null() && one_in(5))
       field_at(i, j) = field(fd_web);
     }
    }
   }
   place_items(mi_rare, 60, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, false, turn);
  }

  if (terrain_type == ot_house_east  || terrain_type == ot_house_base_east) rotate(1);
  if (terrain_type == ot_house_south || terrain_type == ot_house_base_south) rotate(2);
  if (terrain_type == ot_house_west  || terrain_type == ot_house_base_west) rotate(3);
  break;

 case ot_s_lot:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((j == 5 || j == 9 || j == 13 || j == 17 || j == 21) &&
        ((i > 1 && i < 8) || (i > 14 && i < SEEX * 2 - 2)))
     ter(i, j) = t_pavement_y;
    else if ((j < 2 && i > 7 && i < 17) ||
             (j >= 2 && j < SEEY * 2 - 2 && i > 1 && i < SEEX * 2 - 2))
     ter(i, j) = t_pavement;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  if (one_in(3)) {
      int vx = rng (0, 3) * 4 + 5;
      int vy = 4;
      vhtype_id vt = (one_in(10)? veh_sandbike :
                     (one_in(8)? veh_truck :
                     (one_in(3)? veh_car : veh_motorcycle)));
      add_vehicle(vt, vx, vy, one_in(2)? 90 : 270);
  }
  place_items(mi_road, 8, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, false, turn);
  if (t_east  >= ot_road_null && t_east  <= ot_road_nesw_manhole) rotate(1);
  if (t_south >= ot_road_null && t_south <= ot_road_nesw_manhole) rotate(2);
  if (t_west  >= ot_road_null && t_west  <= ot_road_nesw_manhole) rotate(3);
  break;

 case ot_park: {
  if (one_in(3)) { // Playground
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++)
     ter(i, j) = t_grass;
   }
   square(this, t_sandbox,     16,  4, 17,  5);
   square(this, t_monkey_bars,  4,  7,  6,  9);
   line(this, t_slide, 11,  8, 11, 11);
   line(this, t_bench,  6, 14,  6, 15);
   ter( 3,  9) = t_tree;
   ter( 5, 15) = t_tree;
   ter( 6,  4) = t_tree;
   ter( 9, 17) = t_tree;
   ter(13,  3) = t_tree;
   ter(15, 16) = t_tree;
   ter(19, 14) = t_tree;
   ter(20,  8) = t_tree;
   rotate(rng(0, 3));
  } else { // Basketball court
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++)
     ter(i, j) = t_pavement;
   }
   line(this, t_pavement_y,  1, 11, 22, 11);
   line(this, t_pavement_y,  6,  2,  6,  8);
   line(this, t_pavement_y, 16,  2, 16,  8);
   line(this, t_pavement_y,  6, 14,  6, 20);
   line(this, t_pavement_y, 16, 14, 16, 20);

   square(this, t_pavement_y,  9,  2, 13,  4);
   square(this, t_pavement_y,  9, 18, 13, 20);
   square(this, t_pavement,   10,  2, 12,  3);
   square(this, t_pavement,   10, 19, 12, 20);
   ter( 7,  9) = t_pavement_y;
   ter( 8, 10) = t_pavement_y;
   ter(15,  9) = t_pavement_y;
   ter(14, 10) = t_pavement_y;
   ter( 8, 12) = t_pavement_y;
   ter( 7, 13) = t_pavement_y;
   ter(14, 12) = t_pavement_y;
   ter(15, 13) = t_pavement_y;
 
   line(this, t_bench,  1,  4,  1, 10);
   line(this, t_bench,  1, 12,  1, 18);
   line(this, t_bench, 22,  4, 22, 10);
   line(this, t_bench, 22, 12, 22, 18);
 
   ter(11,  2) = t_backboard;
   ter(11, 20) = t_backboard;
 
   line(this, t_fence_v,  0,  1,  0, 21);
   line(this, t_fence_v, 23,  1, 23, 21);
   line(this, t_fence_h,  1,  1, 22,  1);
   line(this, t_fence_h,  1, 21, 22, 21);
 
   ter( 2,  1) = t_pavement;
   ter(21,  1) = t_pavement;
   ter( 2, 21) = t_pavement;
   ter(21, 21) = t_pavement;

   rotate(rng(0, 3));
  }
 } break;

 case ot_s_gas_north:
 case ot_s_gas_east:
 case ot_s_gas_south:
 case ot_s_gas_west:
  tw = rng(5, 14);
  bw = SEEY * 2 - rng(1, 2);
  mw = rng(tw + 5, bw - 3);
  if (mw < bw - 5) mw = bw - 5;
  lw = rng(0, 3);
  rw = SEEX * 2 - rng(1, 4);
  cw = rng(lw + 4, rw - 5);
  rn = rng(3, 6);	// Frequency of gas pumps
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEX * 2; j++) {
    if (j < tw && (tw - j) % 4 == 0 && i > lw && i < rw &&
        (i - (1 + lw)) % rn == 0)
     ter(i, j) = t_gas_pump;
    else if ((j < 2 && i > 7 && i < 16) || (j < tw && i > lw && i < rw))
     ter(i, j) = t_pavement;
    else if (j == tw && (i == lw+6 || i == lw+7 || i == rw-7 || i == rw-6))
     ter(i, j) = t_window;
    else if (((j == tw || j == bw) && i >= lw && i <= rw) ||
             (j == mw && (i >= cw && i < rw)))
     ter(i, j) = t_wall_h;
    else if (((i == lw || i == rw) && j > tw && j < bw) ||
             (j > mw && j < bw && (i == cw || i == rw - 2)))
     ter(i, j) = t_wall_v;
    else if ((i == rw - 5 && j > tw + 1 && j < tw + 4) ||
             (j == tw + 3 && i > rw - 5 && i < rw))
     ter(i, j) = t_counter;
    else if (i > lw && i < rw && j > tw && j < bw)
     ter(i, j) = t_floor;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  ter(cw, rng(mw + 1, bw - 1)) = t_door_c;
  ter(rw - 1, mw) = t_door_c;
  ter(rw - 1, bw - 1) = t_toilet;
  ter(rng(10, 13), tw) = t_door_c;
  if (one_in(5))
   ter(rng(lw + 1, cw - 1), bw) = (one_in(4) ? t_door_c : t_door_locked);
  for (int i = lw + (lw % 2 == 0 ? 3 : 4); i < cw && i < lw + 12; i += 2) {
   _stock_line(*this, t_rack, one_in(3) ? mi_magazines : mi_snacks, 74, point(i, tw + 2), point(i, mw - 2));
  }
  _stock_line(*this, t_fridge, mi_fridgesnacks, 82, point(lw + 1, tw + 1), point(lw + 1, bw - 1));
  place_items(mi_road,		12, 0,      0,  SEEX*2 - 1, tw - 1, false, 0);
  place_items(mi_behindcounter,	70, rw - 4, tw + 1, rw - 1, tw + 2, false, 0);
  place_items(mi_softdrugs,	12, rw - 1, bw - 2, rw - 1, bw - 2, false, 0);
  static_assert(1 == ot_s_gas_east - ot_s_gas_north);
  static_assert(2 == ot_s_gas_south - ot_s_gas_north);
  static_assert(3 == ot_s_gas_west - ot_s_gas_north);
  rotate(terrain_type - ot_s_gas_north);
  break;

 case ot_s_pharm_north:
 case ot_s_pharm_east:
 case ot_s_pharm_south:
 case ot_s_pharm_west:
  tw = rng(0, 4);
  bw = SEEY * 2 - rng(1, 5);
  mw = bw - rng(3, 4);	// Top of the storage room
  lw = rng(0, 4);
  rw = SEEX * 2 - rng(1, 5);
  cw = rng(13, rw - 5);	// Left side of the storage room
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == tw && ((i > lw + 2 && i < lw + 6) || (i > rw - 6 && i < rw - 2)))
     ter(i, j) = t_window;
    else if ((j == tw && (i == lw + 8 || i == lw + 9)) ||
             (i == cw && j == mw + 1))
     ter(i, j) = t_door_c;
    else if (((j == tw || j == bw) && i >= lw && i <= rw) ||
             (j == mw && i >= cw && i < rw))
     ter(i, j) = t_wall_h;
    else if (((i == lw || i == rw) && j > tw && j < bw) ||
             (i == cw && j > mw && j < bw))
     ter(i, j) = t_wall_v;
    else if (((i == lw + 9 || i == rw - 4 || i == rw - 3) &&
              j > tw + 3 && j < mw - 2) ||
             (j == bw - 1 && i > lw + 1 && i < cw - 1))
     ter(i, j) = t_rack;
    else if ((j == mw     && i > lw + 1 && i < cw) ||
             (j == tw + 6 && i > lw + 1 && i < lw + 6) ||
             (i == lw + 5 && j > tw     && j < tw + 7))
     ter(i, j) = t_counter;
    else if (i > lw && i < rw && j > tw && j < bw)
     ter(i, j) = t_floor;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  _stock_line(*this, t_rack, one_in(3) ? mi_snacks : (one_in(4) ? mi_cleaning : mi_magazines), 74, point(lw + 8, tw + 4), point(lw + 8, mw - 3));
  if (one_in(5))
   place_items(mi_softdrugs,	84, lw + 9, tw + 4, lw + 9, mw - 3, false, 0);
  else if (one_in(4))
   place_items(mi_cleaning,	74, lw + 9, tw + 4, lw + 9, mw - 3, false, 0);
  else
   place_items(mi_snacks,	74, lw + 9, tw + 4, lw + 9, mw - 3, false, 0);
  if (one_in(5))
   place_items(mi_softdrugs,	84, rw - 4, tw + 4, rw - 4, mw - 3, false, 0);
  else
   place_items(mi_snacks,	74, rw - 4, tw + 4, rw - 4, mw - 3, false, 0);
  if (one_in(3))
   place_items(mi_snacks,	70, rw - 3, tw + 4, rw - 3, mw - 3, false, 0);
  else
   place_items(mi_softdrugs,	80, rw - 3, tw + 4, rw - 3, mw - 3, false, 0);
  _stock_line(*this, t_fridge, mi_fridgesnacks, 74, point(lw + 1, tw + 9), point(lw + 1, mw - 2));
  _stock_line(*this, t_fridge, mi_fridgesnacks, 74, point(cw + 2, mw - 1), point(rw - 1, mw - 1));
  place_items(mi_harddrugs,	88, lw + 2, bw - 1, cw - 2, bw - 1, false, 0);
  place_items(mi_behindcounter,	78, lw + 1, tw + 1, lw + 4, tw + 5, false, 0);
  static_assert(1 == ot_s_pharm_east - ot_s_pharm_north);
  static_assert(2 == ot_s_pharm_south - ot_s_pharm_north);
  static_assert(3 == ot_s_pharm_west - ot_s_pharm_north);
  rotate(terrain_type - ot_s_pharm_north);
  break;

 case ot_s_grocery_north:
 case ot_s_grocery_east:
 case ot_s_grocery_south:
 case ot_s_grocery_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == 2 && ((i > 4 && i < 8) || (i > 15 && i < 19)))
     ter(i, j) = t_window;
    else if ((j == 2 && (i == 11 || i == 12)) || (i == 6 && j == 20))
     ter(i, j) = t_door_c;
    else if (((j == 2 || j == SEEY * 2 - 3) && i > 1 && i < SEEX * 2 - 2) ||
               (j == 18 && i > 2 && i < 7))
     ter(i, j) = t_wall_h;
    else if (((i == 2 || i == SEEX * 2 - 3) && j > 2 && j < SEEY * 2 - 3) ||
               (i == 6 && j == 19))
     ter(i, j) = t_wall_v;
    else if (j > 4 && j < 8) {
     if (i == 5 || i == 9 || i == 13 || i == 17)
      ter(i, j) = t_counter;
     else if (i > 2 && i < SEEX * 2 - 3)
      ter(i, j) = t_floor;
     else
      ter(i, j) = grass_or_dirt();
    } else if ((j == 7 && (i == 3 || i == 4)) || 
               (j == 14 && (i == 18 || i == 19)))
     ter(i, j) = t_rack;
    else if ((j == 18 && i > 15 && i < 21) || (j == 19 && i == 16))
     ter(i, j) = t_counter;
    else if (i > 2 && i < SEEX * 2 - 3 && j > 2 && j < SEEY * 2 - 3)
     ter(i, j) = t_floor;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  _stock_line(*this, t_fridge, mi_fridgesnacks, 65, point(3, 10), point(3, 15));
  _stock_line(*this, t_fridge, mi_fridge, 70, point(8, 20), point(14, 20));
  _stock_line(*this, t_fridge, mi_fridge, 50, point(19, 20), point(20, 20));
  _stock_line(*this, t_rack, mi_softdrugs, 55, point(6, 10), point(6, 15));
  _stock_line(*this, t_rack, mi_cleaning, 88, point(7, 10), point(7, 15));
  _stock_line(*this, t_rack, mi_kitchen, 75, point(10, 10), point(10, 15));
  _stock_line(*this, t_rack, mi_snacks, 78, point(11, 10), point(11, 15));
  _stock_line(*this, t_rack, mi_cannedfood, 80, point(14, 10), point(14, 15));
  _stock_line(*this, t_rack, mi_pasta, 74, point(15, 10), point(15, 15));
  _stock_line(*this, t_rack, mi_produce, 60, point(20, 10), point(20, 15));
  _stock_line(*this, t_rack, mi_produce, 50, point(18, 11), point(19, 11));
  place_items(mi_produce,	50, 18, 10, 20, 15, false, 0);
  for (int i = 8; i < 21; i +=4) {	// Checkout snacks & magazines
   line(this, t_rack, point(i, 5), point(i, 7));
   place_items(mi_snacks,    50, i, 5, i, 6, false, 0);
   place_items(mi_magazines, 70, i, 7, i, 7, false, 0);
  }
  static_assert(1 == ot_s_grocery_east - ot_s_grocery_north);
  static_assert(2 == ot_s_grocery_south - ot_s_grocery_north);
  static_assert(3 == ot_s_grocery_west - ot_s_grocery_north);
  rotate(terrain_type - ot_s_grocery_north);
  break;

 case ot_s_hardware_north:
 case ot_s_hardware_east:
 case ot_s_hardware_south:
 case ot_s_hardware_west:
  rn = 0;	// No back door
  if (!one_in(3)) rn = 1;	// Old-style back door
  else if (one_in(3)) rn = 2;	// Paved back area
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == 3 && ((i > 5 && i < 9) || (i > 14 && i < 18)))
     ter(i, j) = t_window;
    else if ((j == 3 && i > 1 && i < SEEX * 2 - 2) || 
             (j == 15 && i > 1 && i < 14) ||
             (j == SEEY * 2 - 3 && i > 12 && i < SEEX * 2 - 2))
     ter(i, j) = t_wall_h;
    else if ((i == 2 && j > 3 && j < 15) ||
             (i == SEEX * 2 - 3 && j > 3 && j < SEEY * 2 - 3) ||
             (i == 13 && j > 15 && j < SEEY * 2 - 3))
     ter(i, j) = t_wall_v;
    else if ((i > 3 && i < 10 && j == 6) || (i == 9 && j > 3 && j < 7))
     ter(i, j) = t_counter;
    else if ((i > 14 && i < 18 && j == 13) ||
              (j == SEEY * 2 - 4 && i > 13 && i < SEEX * 2 - 4) ||
              (i > 15 && i < 18 && j > 15 && j < 18))
     ter(i, j) = t_rack;
    else if ((i > 2 && i < SEEX * 2 - 3 && j > 3 && j < 15) ||
             (i > 13 && i < SEEX * 2 - 3 && j > 14 && j < SEEY * 2 - 3))
     ter(i, j) = t_floor;
    else if (rn == 2 && i > 1 && i < 13 && j > 15 && j < SEEY * 2 - 3)
     ter(i, j) = t_pavement;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  ter(rng(10, 13), 3) = t_door_c;
  if (rn > 0)
   ter(13, rng(16, 19)) = (one_in(3) ? t_door_c : t_door_locked);
  if (rn == 2) {
   if (one_in(5))
    ter(rng(4, 10), 16) = t_gas_pump;
   if (one_in(3)) {	// Place a dumpster
    const int s_x = rng(2, 11);
    const point start(s_x, 11 == s_x ? 18 : rng(18, 19));
    const point br = start + (18 == start.y ? point(2,3) : point(3,2)); // favor N/S orientation but if at bottom then E/W orientation
    _stock_square(*this, t_dumpster, mi_trash, 30, start, br);
   }
   place_items(mi_road, 30, 2, 16, 12, SEEY * 2 - 3, false, 0);
  }

  // release block V0.2.3 \todo put in some audits so that the hardware store is guaranteed to have a balanced stock
  _stock(*this, t_rack, mi_magazines, 70, point(9, 7));
  if (one_in(4))
   place_items(mi_snacks,	70,  9,  7,  9,  7, false, 0);

  _stock_line(*this, t_rack, one_in(3) ? (one_in(3) ? mi_bigtools : mi_tools) : mi_hardware, 80, point(3, 9), point(3, 14));
  _stock_line(*this, t_rack, one_in(3) ? (one_in(3) ? mi_bigtools : mi_tools) : mi_hardware, 80, point(6, 9), point(6, 14));
  _stock_line(*this, t_rack, one_in(4) ? (one_in(4) ? mi_mischw : mi_hardware) : mi_tools, 80, point(7, 9), point(7, 14));
  _stock_line(*this, t_rack, one_in(4) ? (one_in(4) ? mi_mischw : mi_hardware) : mi_tools, 80, point(10, 9), point(10, 14));
  _stock_line(*this, t_rack, one_in(3) ? (one_in(2) ? mi_cleaning : mi_tools) : mi_bigtools, 75, point(11, 9), point(11, 14));
  _stock_line(*this, t_rack, one_in(2) ? mi_cleaning : mi_snacks, 65, point(15, 8), point(17, 8));
  _stock_line(*this, t_rack, one_in(4) ? mi_hardware : mi_cleaning, 74, point(15, 9), point(17, 9));
  _stock_line(*this, t_rack, one_in(4) ? mi_hardware : mi_cleaning, 74, point(15, 12), point(17, 12));
  _stock_line(*this, t_rack, mi_mischw, 90, point(20, 4), point(20, 19));
  static_assert(1 == ot_s_hardware_east - ot_s_hardware_north);
  static_assert(2 == ot_s_hardware_south - ot_s_hardware_north);
  static_assert(3 == ot_s_hardware_west - ot_s_hardware_north);
  rotate(terrain_type - ot_s_hardware_north);
  break;

 case ot_s_electronics_north:
 case ot_s_electronics_east:
 case ot_s_electronics_south:
 case ot_s_electronics_west:
     /*
     -------""-+--"""------
     |{{{{{           #   |
     |{               #   |
     |{               #   |
     |{               ### |
     |{                   |
     |{     {{            "
     |{     {{            "
     |{                   "
     |{                   |
     |{                   |
     |{  {{     {{        "
     |{  {{     {{        "
     |{                   "
     |{                   |
     |#     {      {     #|
     |#     {      {     #|
     |#     {      {     #|
     |#     {      {     #|
     |####################|
     ----------------------

     */

  square(this, grass_or_dirt(), point(0), point(2 * SEE - 1));
  square(this, t_floor, point(4), point(2 * SEE - 4));
  line(this, t_wall_v, 3, 4, 3, SEEY * 2 - 4);
  line(this, t_wall_v, SEEX * 2 - 3, 4, SEEX * 2 - 3, SEEY * 2 - 4);
  line(this, t_wall_h, 3, 3, SEEX * 2 - 3, 3);
  line(this, t_wall_h, 3, SEEY * 2 - 3, SEEX * 2 - 3, SEEY * 2 - 3);
  ter(13, 3) = t_door_c;
  line(this, t_window, 10, 3, 11, 3);
  line(this, t_window, 16, 3, 18, 3);
  line(this, t_window, SEEX * 2 - 3, 9,  SEEX * 2 - 3, 11);
  line(this, t_window, SEEX * 2 - 3, 14,  SEEX * 2 - 3, 16);
  line(this, t_counter, SEEX * 2 - 7, 4, SEEX * 2 - 7, 6);
  line(this, t_counter, SEEX * 2 - 7, 7, SEEX * 2 - 5, 7);
  // C:Whales non-default turn
  _stock_line(*this, t_counter, mi_consumer_electronics, 85, point(4, SEEY * 2 - 4), point(SEEX * 2 - 4, SEEY * 2 - 4), false, turn - 50);
  _stock_line(*this, t_counter, mi_consumer_electronics, 85, point(4, SEEY * 2 - 5), point(4, SEEY * 2 - 9), false, turn - 50);
  _stock_line(*this, t_counter, mi_consumer_electronics, 85, point(SEEX * 2 - 4, SEEY * 2 - 5), point(SEEX * 2 - 4, SEEY * 2 - 9), false, turn - 50);
  _stock_line(*this, t_rack, mi_consumer_electronics, 85, point(9, SEEY * 2 - 5), point(9, SEEY * 2 - 9), false, turn - 50);
  _stock_line(*this, t_rack, mi_consumer_electronics, 85, point(SEEX * 2 - 9, SEEY * 2 - 5), point(SEEX * 2 - 9, SEEY * 2 - 9), false, turn - 50);
  _stock_line(*this, t_rack, mi_consumer_electronics, 85, point(4, 4), point(4, SEEY * 2 - 10), false, turn - 50);
  _stock_line(*this, t_rack, mi_consumer_electronics, 85, point(5, 4), point(8, 4), false, turn - 50);
  static_assert(1 == ot_s_electronics_east - ot_s_electronics_north);
  static_assert(2 == ot_s_electronics_south - ot_s_electronics_north);
  static_assert(3 == ot_s_electronics_west - ot_s_electronics_north);
  rotate(terrain_type - ot_s_electronics_north);
  break;

 case ot_s_sports_north:
 case ot_s_sports_east:
 case ot_s_sports_south:
 case ot_s_sports_west:
  lw = rng(0, 3);
  rw = SEEX * 2 - 1 - rng(0, 3);
  tw = rng(3, 10);
  bw = SEEY * 2 - 1 - rng(0, 3);
  cw = bw - rng(3, 5);
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (((j == tw || j == bw) && i >= lw && i <= rw) ||
        (j == cw && i > lw && i < rw))
     ter(i, j) = t_wall_h;
    else if ((i == lw || i == rw) && j > tw && j < bw)
     ter(i, j) = t_wall_v;
    else if (j == cw - 3 && i > lw && i < rw - 4)
     ter(i, j) = t_counter;
    else if (j > tw && j < bw && i > lw && i < rw)
     ter(i, j) = t_floor;
    else if (tw >= 6 && j >= tw - 6 && j < tw && i >= lw && i <= rw) {
     ter(i, j) = ((i - lw) % 4 == 0) ? t_pavement_y : t_pavement;
    } else
     ter(i, j) = grass_or_dirt();
   }
  }
  rn = rng(tw + 2, cw - 6);
  for (int i = lw + 3; i <= rw - 5; i += 4) {
   if (cw - 6 > tw + 1) {
    const point origin(i,rn);
    _stock_square(*this, t_rack, mi_camping, 86, origin, origin + Direction::SE);
   } else if (cw - 5 > tw + 1) {
    const point origin(i, cw - 5);
    _stock_line(*this, t_rack, mi_camping, 80, origin, origin + Direction::E);
   }
  }
  ter(rw - rng(2, 3), cw) = t_door_c;
  rn = rng(2, 4);
  for (int i = lw + 2; i <= lw + 2 + rn; i++)
   ter(i, tw) = t_window;
  for (int i = rw - 2; i >= rw - 2 - rn; i--)
   ter(i, tw) = t_window;
  ter(rng(lw + 3 + rn, rw - 3 - rn), tw) = t_door_c;
  if (one_in(4))
   ter(rng(lw + 2, rw - 2), bw) = t_door_locked;
  _stock_line(*this, t_rack, mi_allsporting, 90, point(lw + 1, cw - 1), point(rw - 5, cw - 1));
  _stock_line(*this, t_rack, mi_sports, 82, point(lw + 1, tw + 1), point(lw + 1, cw - 4));
  _stock_line(*this, t_rack, mi_sports, 82, point(rw - 1, tw + 1), point(rw - 1, cw - 4));
  if (!one_in(4))
   place_items(mi_allsporting,	92, lw + 1, cw + 1, rw - 1, bw - 1, false, 0);

  static_assert(1 == ot_s_sports_east - ot_s_sports_north);
  static_assert(2 == ot_s_sports_south - ot_s_sports_north);
  static_assert(3 == ot_s_sports_west - ot_s_sports_north);
  rotate(terrain_type - ot_s_sports_north);
  break;

 case ot_s_liquor_north:
 case ot_s_liquor_east:
 case ot_s_liquor_south:
 case ot_s_liquor_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == 2 && (i == 5 || i == 18))
     ter(i, j) = t_window;
    else if (((j == 2 || j == 12) && i > 2 && i < SEEX * 2 - 3) ||
             (j == 9 && i > 3 && i < 8))
     ter(i, j) = t_wall_h;
    else if (((i == 3 || i == SEEX * 2 - 4) && j > 2 && j < 12) ||
             (i == 7 && j > 9 && j < 12))
     ter(i, j) = t_wall_v;
    else if ((i == 16 && j > 2 && j < 6) || (j == 5 && i > 16 && i < 19))
     ter(i, j) = t_counter;
    else if (i > 2 && i < SEEX * 2 - 3) {
     if (j > 2 && j < 12)
      ter(i, j) = t_floor;
     else if (j > 12 && j < SEEY * 2 - 1)
      ter(i, j) = t_pavement;
     else
      ter(i, j) = grass_or_dirt();
    } else
     ter(i, j) = grass_or_dirt();
   }
  }
  ter(rng(13, 15), 2) = t_door_c;
  ter(rng(4, 6), 9) = t_door_c;
  ter(rng(9, 16), 12) = t_door_c;

  _stock_line(*this, t_rack, mi_alcohol, 96, point(4, 3), point(4, 7));
  _stock_line(*this, t_rack, mi_alcohol, 96, point(9, 3), point(11, 3));
  _stock_square(*this, t_rack, mi_alcohol, 96, point(7, 3), point(8, 7));
  _stock_square(*this, t_rack, mi_alcohol, 96, point(11, 8), point(15, 9));
  _stock_square(*this, t_rack, mi_snacks, 85, point(11, 5), point(12, 6));
  _stock_line(*this, t_fridge, mi_fridgesnacks, 90, point(19, 7), point(19, 11));
  _stock_line(*this, t_fridge, mi_fridgesnacks, 90, point(17, 11), point(18, 11));
  place_items(mi_behindcounter,	80, 17,  3, 19,  4, false, 0);

  // trash hauling service pre-apocalypse was better here than other stores
  line(this, t_dumpster, point(5, 13), point(7, 13));
  _stock_line(*this, t_dumpster, mi_trash, 30, point(5, 14), point(7, 14));
  _stock_line(*this, t_dumpster, mi_trash, 30, point(18, 15), point(18, 17));
  line(this, t_dumpster, point(19, 15), point(19, 17));

  static_assert(1 == ot_s_liquor_east - ot_s_liquor_north);
  static_assert(2 == ot_s_liquor_south - ot_s_liquor_north);
  static_assert(3 == ot_s_liquor_west - ot_s_liquor_north);
  rotate(terrain_type - ot_s_liquor_north);
  break;

 case ot_s_gun_north:
 case ot_s_gun_east:
 case ot_s_gun_south:
 case ot_s_gun_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i == 2 || i == SEEX * 2 - 3) && j > 6 && j < SEEY * 2 - 1)
     ter(i, j) = t_wall_v;
    else if ((i == 8 && j > 6 && j < 13) ||
             (j == 16 && (i == 5 || i == 8 || i == 11 || i == 14 || i == 17)))
     ter(i, j) = t_counter;
    else if ((j == 6 && ((i > 4 && i < 8) || (i > 15 && i < 19))))
     ter(i, j) = t_window;
    else if ((j == 14 && i > 3 && i < 15))
     ter(i, j) = t_wall_glass_h;
    else if (j == 16 && i == SEEX * 2 - 4)
     ter(i, j) = t_door_c;
    else if (((j == 6 || j == SEEY * 2 - 1) && i > 1 && i < SEEX * 2 - 2) ||
             ((j == 16 || j == 14) && i > 2 && i < SEEX * 2 - 3))
     ter(i, j) = t_wall_h;
    else if (((i == 3 || i == SEEX * 2 - 4) && j > 6 && j < 14) ||
             ((j > 8 && j < 12) && (i == 12 || i == 13 || i == 16)) ||
             (j == 13 && i > 15 && i < SEEX * 2 - 4))
     ter(i, j) = t_rack;
    else if (i > 2 && i < SEEX * 2 - 3 && j > 6 && j < SEEY * 2 - 1)
     ter(i, j) = t_floor;
    else if ((j > 0 && j < 6 &&
             (i == 2 || i == 6 || i == 10 || i == 17 || i == SEEX * 2 - 3)))
     ter(i, j) = t_pavement_y;
    else if (j < 6 && i > 1 && i < SEEX * 2 - 2)
     ter(i, j) = t_pavement;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  ter(rng(11, 14), 6) = t_door_c;
  ter(rng(5, 14), 14) = t_door_c;
  place_items(mi_pistols,	70, 12,  9, 13, 11, false, 0);
  place_items(mi_shotguns,	60, 16,  9, 16, 11, false, 0);
  place_items(mi_rifles,	80, 20,  7, 20, 12, false, 0);
  place_items(mi_smg,		25,  3,  7,  3,  8, false, 0);
  place_items(mi_assault,	18,  3,  9,  3, 10, false, 0);
  place_items(mi_ammo,		93,  3, 11,  3, 13, false, 0);
  place_items(mi_allguns,	12,  5, 16, 17, 16, false, 0);
  place_items(mi_gunxtras,	67, 16, 13, 19, 13, false, 0);
  if (terrain_type == ot_s_gun_east) rotate(1);
  if (terrain_type == ot_s_gun_south) rotate(2);
  if (terrain_type == ot_s_gun_west) rotate(3);
  break;

 case ot_s_clothes_north:
 case ot_s_clothes_east:
 case ot_s_clothes_south:
 case ot_s_clothes_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == 2 && (i == 11 || i == 12))
     ter(i, j) = t_door_glass_c;
    else if (j == 2 && i > 3 && i < SEEX * 2 - 4)
     ter(i, j) = t_wall_glass_h;
    else if (((j == 2 || j == SEEY * 2 - 2) && i > 1 && i < SEEX * 2 - 2) ||
             (j == 4 && i > 12 && i < SEEX * 2 - 3) ||
             (j == 17 && i > 2 && i < 12) ||
             (j == 20 && i > 2 && i < 11))
     ter(i, j) = t_wall_h;
    else if (((i == 2 || i == SEEX * 2 - 3) && j > 1 && j < SEEY * 2 - 1) ||
             (i == 11 && (j == 18 || j == 20 || j == 21)) ||
             (j == 21 && (i == 5 || i == 8)))
     ter(i, j) = t_wall_v;
    else if ((i == 16 && j > 4 && j < 9) ||
             (j == 8 && (i == 17 || i == 18)) ||
             (j == 18 && i > 2 && i < 11))
     ter(i, j) = t_counter;
    else if ((i == 3 && j > 4 && j < 13) ||
             (i == SEEX * 2 - 4 && j > 9 && j < 20) ||
             ((j == 10 || j == 11) && i > 6 && i < 13) ||
             ((j == 14 || j == 15) && i > 4 && i < 13) ||
             ((i == 15 || i == 16) && j > 10 && j < 18) ||
             (j == SEEY * 2 - 3 && i > 11 && i < 18))
     ter(i, j) = t_rack;
    else if (i > 2 && i < SEEX * 2 - 3 && j > 2 && j < SEEY * 2 - 2)
     ter(i, j) = t_floor;
    else
     ter(i, j) = grass_or_dirt();
   }
  }

  for (int i = 3; i <= 9; i += 3) {
   if (one_in(2))
    ter(i, SEEY * 2 - 4) = t_door_c;
   else
    ter(i + 1, SEEY * 2 - 4) = t_door_c;
  }

  place_items(mi_shoes,		70,  7, 10, 12, 10, false, 0);
  place_items(mi_pants,		88,  5, 14, 12, 14, false, 0);
  place_items(mi_shirts,	88,  7, 11, 12, 11, false, 0);
  place_items(mi_jackets,	80,  3,  5,  3, 12, false, 0);
  place_items(mi_winter,	60,  5, 15, 12, 15, false, 0);
  place_items(mi_bags,		70, 15, 11, 15, 17, false, 0);
  place_items(mi_dresser,	50, 12, 21, 17, 21, false, 0);
  place_items(mi_allclothes,	20,  3, 21, 10, 21, false, 0);
  place_items(mi_allclothes,	20,  3, 18, 10, 18, false, 0);

  { // for emphasis; these are covered above as well
  static const constexpr items_location clothes_stock[] = { mi_pants , mi_shirts, mi_bags };
  static const constexpr items_location clothes_stock_2[] = { mi_pants , mi_shirts, mi_jackets };

  place_items(clothes_stock[rng(0, 2)], 70, 16, 11, 16, 17, false, 0);
  place_items(clothes_stock_2[rng(0, 2)], 75, 20, 10, 20, 19, false, 0);
  }

  if (terrain_type == ot_s_clothes_east) rotate(1);
  if (terrain_type == ot_s_clothes_south) rotate(2);
  if (terrain_type == ot_s_clothes_west) rotate(3);
  break;

 case ot_s_library_north:
 case ot_s_library_east:
 case ot_s_library_south:
 case ot_s_library_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == 2) {
     if (i == 5 || i == 6 || i == 17 || i == 18)
      ter(i, j) = t_window;
     else if (i == 11 || i == 12)
      ter(i, j) = t_door_c;
     else if (i > 1 && i < SEEX * 2 - 2)
      ter(i, j) = t_wall_h;
     else
      ter(i, j) = grass_or_dirt();
    } else if (j == 17 && i > 1 && i < SEEX * 2 - 2)
      ter(i, j) = t_wall_h;
    else if (i == 2) {
     if (j == 6 || j == 7 || j == 10 || j == 11 || j == 14 || j == 15)
      ter(i, j) = t_window;
     else if (j > 1 && j < 17)
      ter(i, j) = t_wall_v;
     else
      ter(i, j) = grass_or_dirt();
    } else if (i == SEEX * 2 - 3) {
     if (j == 6 || j == 7)
      ter(i, j) = t_window;
     else if (j > 1 && j < 17)
      ter(i, j) = t_wall_v;
     else
      ter(i, j) = grass_or_dirt();
    } else if (((j == 4 || j == 5) && i > 2 && i < 10) ||
               ((j == 8 || j == 9 || j == 12 || j == 13 || j == 16) &&
                i > 2 && i < 16) || (i == 20 && j > 7 && j < 17))
     ter(i, j) = t_bookcase;
    else if ((i == 14 && j < 6 && j > 2) || (j == 5 && i > 14 && i < 19))
     ter(i, j) = t_counter;
    else if (i > 2 && i < SEEX * 2 - 3 && j > 2 && j < 17)
     ter(i, j) = t_floor;
    else
     ter(i, j) = grass_or_dirt();
   }
  }
  if (!one_in(3))
   ter(18, 17) = t_door_c;
  place_items(mi_magazines, 	70,  3,  4,  9,  4, false, 0);
  place_items(mi_magazines,	70, 20,  8, 20, 16, false, 0);
  place_items(mi_novels, 	96,  3,  5,  9,  5, false, 0);
  place_items(mi_novels,	96,  3,  8, 15,  9, false, 0);
  place_items(mi_manuals,	92,  3, 12, 15, 13, false, 0);
  place_items(mi_textbooks,	88,  3, 16, 15, 16, false, 0);
  if (terrain_type == ot_s_library_east) rotate(1);
  if (terrain_type == ot_s_library_south) rotate(2);
  if (terrain_type == ot_s_library_west) rotate(3);
  break;

 case ot_s_restaurant_north:
 case ot_s_restaurant_east:
 case ot_s_restaurant_south:
 case ot_s_restaurant_west: {
// Init to grass/dirt
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  ter_id doortype = (one_in(4) ? t_door_c : t_door_glass_c);
  lw = rng(0, 4);
  rw = rng(19, 23);
  tw = rng(0, 4);
  bw = rng(17, 23);
// Fill in with floor
  square(this, t_floor, lw + 1, tw + 1, rw - 1, bw - 1);
// Draw the walls
  line(this, t_wall_h, lw, tw, rw, tw);
  line(this, t_wall_h, lw, bw, rw, bw);
  line(this, t_wall_v, lw, tw + 1, lw, bw - 1);
  line(this, t_wall_v, rw, tw + 1, rw, bw - 1);
  
// What's the front wall look like?
  if (one_in(3)) {
   line(this, doortype, (lw + rw) / 2, tw, 1 + ((lw + rw) / 2), tw); // Double-door in center
  } else {
   ter(one_in(2) ? lw + 2 : rw - 2, tw) = doortype; // // Door to one side; Mirror it?
  }
// What type of windows?
  switch (rng(1, 6)) {
  case 1: // None!
   break;
  case 2:
  case 3: // Glass walls everywhere
   for (int i = lw + 1; i <= rw - 1; i++) {
	rewrite<t_wall_h, t_wall_glass_h>(i, tw);
   }
   while (!one_in(3)) { // 2 in 3 chance of having some walls too
    rn = rng(1, 3);
	rewrite<t_wall_glass_h, t_wall_h>(lw + rn, tw);
	rewrite<t_wall_glass_h, t_wall_h>(rw - rn, tw);
   }
   break;
  case 4:
  case 5:
  case 6: { // Just some windows
   rn = rng(1, 3);
   int win_width = rng(1, 3);
   for (int i = rn; i <= rn + win_width; i++) {
	rewrite<t_wall_h, t_window>(lw + i, tw);
	rewrite<t_wall_h, t_window>(rw - i, tw);
   }
   } break;
  } // Done building windows
// Build a kitchen
  mw = rng(bw - 6, bw - 3);
  cw = (one_in(3) ? rw - 3 : rw - 1); // 1 in 3 chance for corridor to back
  line(this, t_wall_h, lw + 1, mw, cw, mw);
  line(this, t_wall_v, cw, mw + 1, cw, bw - 1);
  _stock_line(*this, t_fridge, mi_fridge, 80, point(lw + 1, mw + 1), point(lw + 2, mw + 1));
  _stock_line(*this, t_counter, mi_kitchen, 70, point(lw + 3, mw + 1), point(cw - 1, mw + 1));
  // Place a door to the kitchen
  if (cw != rw - 1 && one_in(2)) // side door
   ter(cw, rng(mw + 2, bw - 1)) = t_door_c;
  else { // north-facing door
   rn = rng(lw + 4, cw - 2);
// Clear the counters around the door
   line(this, t_floor, rn - 1, mw + 1, rn + 1, mw + 1);
   ter(rn, mw) = t_door_c;
  }
// Back door?
  if (bw <= 19 || one_in(3)) {
// If we have a corridor, put it over there
   if (cw == rw - 3) {
// One in two chance of a double-door
    if (one_in(2))
     line(this, t_door_locked, cw + 1, bw, rw - 1, bw);
    else
     ter( rng(cw + 1, rw - 1), bw) = t_door_locked;
   } else // No corridor
    ter( rng(lw + 1, rw - 1), bw) = t_door_locked;
  }
// Build a dining area
  int table_spacing = rng(2, 4);
  for (int i = lw + table_spacing + 1; i <= rw - 2 - table_spacing;
           i += table_spacing + 2) {
   for (int j = tw + table_spacing + 1; j <= mw - 1 - table_spacing;
            j += table_spacing + 2) {
    _stock_square(*this, t_table, mi_dining, 70, point(i, j), point(i + 1, j + 1));
   }
  }
// Dumpster out back?
  if (rng(18, 21) > bw) {
   square(this, t_pavement, lw, bw + 1, rw, 24);
   rn = rng(lw + 1, rw - 4);
   square(this, t_dumpster, rn, 22, rn + 2, 23);
   place_items(mi_trash,  40, rn, 22, rn + 3, 23, false, 0);
   place_items(mi_fridge, 50, rn, 22, rn + 3, 23, false, 0);
  }

  if (terrain_type == ot_s_restaurant_east) rotate(1);
  if (terrain_type == ot_s_restaurant_south) rotate(2);
  if (terrain_type == ot_s_restaurant_west) rotate(3);
  } break;

 case ot_shelter:
// Init to grass & dirt;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  square(this, t_floor, point(5), point(2 * SEE - 6));
  square(this, t_stairs_down, point(SEE - 1), point(SEE));
  line(this, t_wall_h, 4, 4, SEEX * 2 - 5, 4);
  line(this, t_door_c, SEEX - 1, 4, SEEX, 4);
  line(this, t_wall_h, 4, SEEY * 2 - 5, SEEX * 2 - 5, SEEY * 2 - 5);
  line(this, t_door_c, SEEX - 1, SEEY * 2 - 5, SEEX, SEEY * 2 - 5);
  line(this, t_wall_v, 4, 5, 4, SEEY * 2 - 6);
  line(this, t_door_c, 4, SEEY - 1, 4, SEEY);
  line(this, t_wall_v, SEEX * 2 - 5, 5, SEEX * 2 - 5, SEEY * 2 - 6);
  line(this, t_door_c, SEEX * 2 - 5, SEEY - 1, SEEX * 2 - 5, SEEY);
  break;

 case ot_shelter_under:
  square(this, t_rock, point(0), point(2 * SEE - 1));
  square(this, t_floor, point(8), point(2 * SEE - 9));
  line(this, t_stairs_up, SEEX - 1, SEEY * 2 - 8, SEEX, SEEY * 2 - 8);
  place_items(mi_shelter, 80, 8, 8, SEEX * 2 - 9, SEEY * 2 - 9, false, 0);
  break;

 case ot_lab:
 case ot_lab_stairs:
 case ot_lab_core:
// Check for adjacent sewers; used below
  tw = 0;
  rw = 0;
  bw = 0;
  lw = 0;
  if (t_north>=ot_sewer_ns && t_north<=ot_sewer_nesw && connects_to(t_north,2))
   tw = SEEY * 2;
  if (t_east >=ot_sewer_ns && t_east <=ot_sewer_nesw && connects_to(t_east, 3))
   rw = SEEX * 2;
  if (t_south>=ot_sewer_ns && t_south<=ot_sewer_nesw && connects_to(t_south,0))
   bw = SEEY * 2;
  if (t_west >=ot_sewer_ns && t_west <=ot_sewer_nesw && connects_to(t_west, 1))
   lw = SEEX * 2;
  if (t_above == ot_null) {	// We're on ground level
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
     if (i <= 1 || i >= SEEX * 2 - 2 ||
         (j > 1 && j < SEEY * 2 - 2 && (i == SEEX - 2 || i == SEEX + 1)))
      ter(i, j) = t_wall_metal_v;
     else if (j <= 1 || j >= SEEY * 2 - 2)
      ter(i, j) = t_wall_metal_h;
     else
      ter(i, j) = t_floor;
    }
   }
   ter(SEEX - 1, 0) = t_dirt;
   ter(SEEX - 1, 1) = t_door_metal_locked;
   ter(SEEX    , 0) = t_dirt;
   ter(SEEX    , 1) = t_door_metal_locked;
   ter(SEEX - 2 + rng(0, 1) * 4, 0) = t_card_science;
   ter(SEEX - 2, SEEY    ) = t_door_metal_c;
   ter(SEEX + 1, SEEY    ) = t_door_metal_c;
   ter(SEEX - 2, SEEY - 1) = t_door_metal_c;
   ter(SEEX + 1, SEEY - 1) = t_door_metal_c;
   ter(SEEX - 1, SEEY * 2 - 3) = t_stairs_down;
   ter(SEEX    , SEEY * 2 - 3) = t_stairs_down;
   science_room(this, 2       , 2, SEEX - 3    , SEEY * 2 - 3, 1);
   science_room(this, SEEX + 2, 2, SEEX * 2 - 3, SEEY * 2 - 3, 3);

   add_spawn(mon_turret, 1, SEEX, 5);

   if (t_east > ot_road_null && t_east <= ot_road_nesw_manhole) rotate(1);
   else if (t_south > ot_road_null && t_south <= ot_road_nesw_manhole) rotate(2);
   else if (t_west > ot_road_null && t_west <= ot_road_nesw_manhole) rotate(3);
  } else if (tw != 0 || rw != 0 || lw != 0 || bw != 0) {	// Sewers!
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
	 auto& t = ter(i, j);
     t = t_floor;
     if (((i < lw || i > SEEX * 2 - 1 - rw) && j > SEEY - 3 && j < SEEY + 2) ||
         ((j < tw || j > SEEY * 2 - 1 - bw) && i > SEEX - 3 && i < SEEX + 2))
      t = t_sewage;
     if ((i == 0 && t_east >= ot_lab && t_east <= ot_lab_core) || i == SEEX * 2 - 1) {
      if (t_sewage == t) t = t_bars;
      else if (j == SEEY - 1 || j == SEEY) t = t_door_metal_c;
      else t = t_wall_v;
     } else if ((j == 0 && t_north >= ot_lab && t_north <= ot_lab_core) || j == SEEY * 2 - 1) {
      if (t_sewage == t) t = t_bars;
      else if (i == SEEX - 1 || i == SEEX) t = t_door_metal_c;
      else t = t_wall_h;
     }
    }
   }
  } else { // We're below ground, and no sewers
// Set up the boudaries of walls (connect to adjacent lab squares)
   tw = (t_north >= ot_lab && t_north <= ot_lab_finale) ? 0 : 2;
   rw = (t_east  >= ot_lab && t_east  <= ot_lab_finale) ? 1 : 2;
   bw = (t_south >= ot_lab && t_south <= ot_lab_finale) ? 1 : 2;
   lw = (t_west  >= ot_lab && t_west  <= ot_lab_finale) ? 0 : 2;
   switch (rng(1, 3)) {	// Pick a random lab layout
   case 1:	// Cross shaped
    for (int i = 0; i < SEEX * 2; i++) {
     for (int j = 0; j < SEEY * 2; j++) {
      if ((i < lw || i > SEEX * 2 - 1 - rw) ||
          ((j < SEEY - 1 || j > SEEY) && (i == SEEX - 2 || i == SEEX + 1)))
       ter(i, j) = t_wall_v;
      else if ((j < tw || j > SEEY * 2 - 1 - bw) ||
               ((i < SEEX - 1 || i > SEEX) && (j == SEEY - 2 || j == SEEY + 1)))
       ter(i, j) = t_wall_h;
      else
       ter(i, j) = t_floor;
     }
    }
    if (t_above == ot_lab_stairs)
     ter(rng(SEEX - 1, SEEX), rng(SEEY - 1, SEEY)) = t_stairs_up;
// Top left
    if (one_in(2)) {
     ter(SEEX - 2, int(SEEY / 2)) = t_door_metal_c;
     science_room(this, lw, tw, SEEX - 3, SEEY - 3, 1);
    } else {
     ter(int(SEEX / 2), SEEY - 2) = t_door_metal_c;
     science_room(this, lw, tw, SEEX - 3, SEEY - 3, 2);
    }
// Top right
    if (one_in(2)) {
     ter(SEEX + 1, int(SEEY / 2)) = t_door_metal_c;
     science_room(this, SEEX + 2, tw, SEEX * 2 - 1 - rw, SEEY - 3, 3);
    } else {
     ter(SEEX + int(SEEX / 2), SEEY - 2) = t_door_metal_c;
     science_room(this, SEEX + 2, tw, SEEX * 2 - 1 - rw, SEEY - 3, 2);
    }
// Bottom left
    if (one_in(2)) {
     ter(int(SEEX / 2), SEEY + 1) = t_door_metal_c;
     science_room(this, lw, SEEY + 2, SEEX - 3, SEEY * 2 - 1 - bw, 0);
    } else {
     ter(SEEX - 2, SEEY + int(SEEY / 2)) = t_door_metal_c;
     science_room(this, lw, SEEY + 2, SEEX - 3, SEEY * 2 - 1 - bw, 1);
    }
// Bottom right
    if (one_in(2)) {
     ter(SEEX + int(SEEX / 2), SEEY + 1) = t_door_metal_c;
     science_room(this, SEEX +2, SEEY + 2, SEEX*2 - 1 - rw, SEEY*2 - 1 - bw, 0);
    } else {
     ter(SEEX + 1, SEEY + int(SEEY / 2)) = t_door_metal_c;
     science_room(this, SEEX +2, SEEY + 2, SEEX*2 - 1 - rw, SEEY*2 - 1 - bw, 3);
    }
    if (rw == 1) {
     ter(SEEX * 2 - 1, SEEY - 1) = t_door_metal_c;
     ter(SEEX * 2 - 1, SEEY    ) = t_door_metal_c;
    }
    if (bw == 1) {
     ter(SEEX - 1, SEEY * 2 - 1) = t_door_metal_c;
     ter(SEEX    , SEEY * 2 - 1) = t_door_metal_c;
    }
    if (terrain_type == ot_lab_stairs) {	// Stairs going down
     std::vector<point> stair_points;
     if (tw != 0) {
      stair_points.push_back(point(SEEX - 1, 2));
      stair_points.push_back(point(SEEX - 1, 2));
      stair_points.push_back(point(SEEX    , 2));
      stair_points.push_back(point(SEEX    , 2));
     }
     if (rw != 1) {
      stair_points.push_back(point(SEEX * 2 - 3, SEEY - 1));
      stair_points.push_back(point(SEEX * 2 - 3, SEEY - 1));
      stair_points.push_back(point(SEEX * 2 - 3, SEEY    ));
      stair_points.push_back(point(SEEX * 2 - 3, SEEY    ));
     }
     if (bw != 1) {
      stair_points.push_back(point(SEEX - 1, SEEY * 2 - 3));
      stair_points.push_back(point(SEEX - 1, SEEY * 2 - 3));
      stair_points.push_back(point(SEEX    , SEEY * 2 - 3));
      stair_points.push_back(point(SEEX    , SEEY * 2 - 3));
     }
     if (lw != 0) {
      stair_points.push_back(point(2, SEEY - 1));
      stair_points.push_back(point(2, SEEY - 1));
      stair_points.push_back(point(2, SEEY    ));
      stair_points.push_back(point(2, SEEY    ));
     }
     stair_points.push_back(point(int(SEEX / 2)       , SEEY    ));
     stair_points.push_back(point(int(SEEX / 2)       , SEEY - 1));
     stair_points.push_back(point(int(SEEX / 2) + SEEX, SEEY    ));
     stair_points.push_back(point(int(SEEX / 2) + SEEX, SEEY - 1));
     stair_points.push_back(point(SEEX    , int(SEEY / 2)       ));
     stair_points.push_back(point(SEEX + 2, int(SEEY / 2)       ));
     stair_points.push_back(point(SEEX    , int(SEEY / 2) + SEEY));
     stair_points.push_back(point(SEEX + 2, int(SEEY / 2) + SEEY));
     rn = rng(0, stair_points.size() - 1);
     ter(stair_points[rn]) = t_stairs_down;
    }
      
    break;

   case 2:	// tic-tac-toe # layout
    for (int i = 0; i < SEEX * 2; i++) {
     for (int j = 0; j < SEEY * 2; j++) {
      if (i < lw || i > SEEX * 2 - 1 - rw || i == SEEX - 4 || i == SEEX + 3)
       ter(i, j) = t_wall_v;
      else if (j < lw || j > SEEY*2 - 1 - bw || j == SEEY - 4 || j == SEEY + 3)
       ter(i, j) = t_wall_h;
      else
       ter(i, j) = t_floor;
     }
    }
    if (t_above == ot_lab_stairs) {
     ter(SEEX - 1, SEEY - 1) = t_stairs_up;
     ter(SEEX    , SEEY - 1) = t_stairs_up;
     ter(SEEX - 1, SEEY    ) = t_stairs_up;
     ter(SEEX    , SEEY    ) = t_stairs_up;
    }
    ter(SEEX - rng(0, 1), SEEY - 4) = t_door_metal_c;
    ter(SEEX - rng(0, 1), SEEY + 3) = t_door_metal_c;
    ter(SEEX - 4, SEEY + rng(0, 1)) = t_door_metal_c;
    ter(SEEX + 3, SEEY + rng(0, 1)) = t_door_metal_c;
    ter(SEEX - 4, int(SEEY / 2)) = t_door_metal_c;
    ter(SEEX + 3, int(SEEY / 2)) = t_door_metal_c;
    ter(int(SEEX / 2), SEEY - 4) = t_door_metal_c;
    ter(int(SEEX / 2), SEEY + 3) = t_door_metal_c;
    ter(SEEX + int(SEEX / 2), SEEY - 4) = t_door_metal_c;
    ter(SEEX + int(SEEX / 2), SEEY + 3) = t_door_metal_c;
    ter(SEEX - 4, SEEY + int(SEEY / 2)) = t_door_metal_c;
    ter(SEEX + 3, SEEY + int(SEEY / 2)) = t_door_metal_c;
    science_room(this, lw, tw, SEEX - 5, SEEY - 5, rng(1, 2));
    science_room(this, SEEX - 3, tw, SEEX + 2, SEEY - 5, 2);
    science_room(this, SEEX + 4, tw, SEEX * 2 - 1 - rw, SEEY - 5, rng(2, 3));
    science_room(this, lw, SEEY - 3, SEEX - 5, SEEY + 2, 1);
    science_room(this, SEEX + 4, SEEY - 3, SEEX * 2 - 1 - rw, SEEY + 2, 3);
    science_room(this, lw, SEEY + 4, SEEX - 5, SEEY * 2 - 1 - bw, rng(0, 1));
    science_room(this, SEEX - 3, SEEY + 4, SEEX + 2, SEEY * 2 - 1 - bw, 0);
    science_room(this, SEEX+4, SEEX+4, SEEX*2-1-rw, SEEY*2-1-bw, 3 * rng(0, 1));
    if (rw == 1) {
     ter(SEEX * 2 - 1, SEEY - 1) = t_door_metal_c;
     ter(SEEX * 2 - 1, SEEY    ) = t_door_metal_c;
    }
    if (bw == 1) {
     ter(SEEX - 1, SEEY * 2 - 1) = t_door_metal_c;
     ter(SEEX    , SEEY * 2 - 1) = t_door_metal_c;
    }
    if (terrain_type == ot_lab_stairs)
     ter(SEEX - 3 + 5 * rng(0, 1), SEEY - 3 + 5 * rng(0, 1)) = t_stairs_down;
    break;
   
   case 3:	// Big room
    for (int i = 0; i < SEEX * 2; i++) {
     for (int j = 0; j < SEEY * 2; j++) {
      if (i < lw || i >= SEEX * 2 - 1 - rw)
       ter(i, j) = t_wall_v;
      else if (j < tw || j >= SEEY * 2 - 1 - bw)
       ter(i, j) = t_wall_h;
      else
       ter(i, j) = t_floor;
     }
    }
    science_room(this, lw, tw, SEEX * 2 - 1 - rw, SEEY * 2 - 1 - bw, rng(0, 3));
    if (t_above == ot_lab_stairs) {
     int sx, sy;
     do {
      sx = rng(lw, SEEX * 2 - 1 - rw);
      sy = rng(tw, SEEY * 2 - 1 - bw);
     } while (!rewrite_test<t_floor, t_stairs_up>(sx, sy));
    }
    if (rw == 1) {
     ter(SEEX * 2 - 1, SEEY - 1) = t_door_metal_c;
     ter(SEEX * 2 - 1, SEEY    ) = t_door_metal_c;
    }
    if (bw == 1) {
     ter(SEEX - 1, SEEY * 2 - 1) = t_door_metal_c;
     ter(SEEX    , SEEY * 2 - 1) = t_door_metal_c;
    }
    if (terrain_type == ot_lab_stairs) {
     int sx, sy;
     do {
      sx = rng(lw, SEEX * 2 - 1 - rw);
      sy = rng(tw, SEEY * 2 - 1 - bw);
     } while (!rewrite_test<t_floor, t_stairs_down>(sx,sy));
    }
    break;
   }
  }
// Ants will totally wreck up the place
  tw = 0;
  rw = 0;
  bw = 0;
  lw = 0;
  if (t_north >= ot_ants_ns && t_north <=ot_ants_nesw && connects_to(t_north,2))
   tw = SEEY;
  if (t_east  >= ot_ants_ns && t_east <= ot_ants_nesw && connects_to(t_east, 3))
   rw = SEEX;
  if (t_south >= ot_ants_ns && t_south <=ot_ants_nesw && connects_to(t_south,0))
   bw = SEEY + 1;
  if (t_west  >= ot_ants_ns && t_west <= ot_ants_nesw && connects_to(t_west, 1))
   lw = SEEX + 1;
  if (tw != 0 || rw != 0 || bw != 0 || lw != 0) {
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
     if ((i < SEEX*2 - lw && (!one_in(3) || (j > SEEY - 6 && j < SEEY + 5))) ||
         (i > rw &&          (!one_in(3) || (j > SEEY - 6 && j < SEEY + 5))) ||
         (j > tw &&          (!one_in(3) || (i > SEEX - 6 && i < SEEX + 5))) ||
         (j < SEEY*2 - bw && (!one_in(3) || (i > SEEX - 6 && i < SEEX + 5)))) {
      if (one_in(5))
       ter(i, j) = t_rubble;
      else
       ter(i, j) = t_rock_floor;
     }
    }
   }
  }

// Slimes pretty much wreck up the place, too, but only underground
  tw = (t_north == ot_slimepit ? SEEY     : 0);
  rw = (t_east  == ot_slimepit ? SEEX + 1 : 0);
  bw = (t_south == ot_slimepit ? SEEY + 1 : 0);
  lw = (t_west  == ot_slimepit ? SEEX     : 0);
  if (tw != 0 || rw != 0 || bw != 0 || lw != 0) {
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
     if (((j <= tw || i >= rw) && i >= j && (SEEX * 2 - 1 - i) <= j) ||
         ((j >= bw || i <= lw) && i <= j && (SEEY * 2 - 1 - j) <= i)   ) {
      if (one_in(5))
       ter(i, j) = t_rubble;
      else if (!one_in(5))
       ter(i, j) = t_slime;
     }
    }
   }
  }

 break;

 case ot_lab_finale:
  tw = (t_north >= ot_lab && t_north <= ot_lab_finale) ? 0 : 2;
  rw = (t_east  >= ot_lab && t_east  <= ot_lab_finale) ? 1 : 2;
  bw = (t_south >= ot_lab && t_south <= ot_lab_finale) ? 1 : 2;
  lw = (t_west  >= ot_lab && t_west  <= ot_lab_finale) ? 0 : 2;
// Start by setting up a large, empty room.
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < lw || i > SEEX * 2 - 1 - rw)
     ter(i, j) = t_wall_v;
    else if (j < tw || j > SEEY * 2 - 1 - bw)
     ter(i, j) = t_wall_h;
    else
     ter(i, j) = t_floor;
   }
  }
  if (rw == 1) {
   ter(SEEX * 2 - 1, SEEY - 1) = t_door_metal_c;
   ter(SEEX * 2 - 1, SEEY    ) = t_door_metal_c;
  }
  if (bw == 1) {
   ter(SEEX - 1, SEEY * 2 - 1) = t_door_metal_c;
   ter(SEEX    , SEEY * 2 - 1) = t_door_metal_c;
  }

  switch (rng(1, 3)) {
  case 1:	// Weapons testing
   add_spawn(mon_secubot, 1,            6,            6);
   add_spawn(mon_secubot, 1, SEEX * 2 - 7,            6);
   add_spawn(mon_secubot, 1,            6, SEEY * 2 - 7);
   add_spawn(mon_secubot, 1, SEEX * 2 - 7, SEEY * 2 - 7);
   add_trap(SEEX - 2, SEEY - 2, tr_dissector);
   add_trap(SEEX + 1, SEEY - 2, tr_dissector);
   add_trap(SEEX - 2, SEEY + 1, tr_dissector);
   add_trap(SEEX + 1, SEEY + 1, tr_dissector);
   if (!one_in(3)) {
    rn = dice(4, 3);
    for (int i = 0; i < rn; i++) {
     add_item(SEEX - 1, SEEY - 1, item::types[itm_laser_pack], 0);
     add_item(SEEX + 1, SEEY - 1, item::types[itm_laser_pack], 0);
    }
    add_item(SEEX - 1, SEEY    , item::types[itm_v29], 0);
    add_item(SEEX + 1, SEEY    , item::types[itm_ftk93], 0);
   } else if (!one_in(3)) {
    rn = dice(3, 6);
    for (int i = 0; i < rn; i++) {
     add_item(SEEX - 1, SEEY - 1, item::types[itm_mininuke], 0);
     add_item(SEEX    , SEEY - 1, item::types[itm_mininuke], 0);
     add_item(SEEX - 1, SEEY    , item::types[itm_mininuke], 0);
     add_item(SEEX    , SEEY    , item::types[itm_mininuke], 0);
    }
   } else {
    ter(SEEX - 2, SEEY - 1) = t_rack;
    ter(SEEX - 1, SEEY - 1) = t_rack;
    ter(SEEX    , SEEY - 1) = t_rack;
    ter(SEEX + 1, SEEY - 1) = t_rack;
    ter(SEEX - 2, SEEY    ) = t_rack;
    ter(SEEX - 1, SEEY    ) = t_rack;
    ter(SEEX    , SEEY    ) = t_rack;
    ter(SEEX + 1, SEEY    ) = t_rack;
    place_items(mi_ammo, 96, SEEX - 2, SEEY - 1, SEEX + 1, SEEY - 1, false, 0);
    place_items(mi_allguns, 96, SEEX - 2, SEEY, SEEX + 1, SEEY, false, 0);
   }
   break;

  case 2: {	// Netherworld access
   if (!one_in(4)) {	// Trapped netherworld monsters
    tw = rng(SEEY + 3, SEEY + 5);
    bw = tw + 4;
    lw = rng(SEEX - 6, SEEX - 2);
    rw = lw + 6;
    for (int i = lw; i <= rw; i++) {
     for (int j = tw; j <= bw; j++) {
      if (j == tw || j == bw) {
       if ((i - lw) % 2 == 0)
        ter(i, j) = t_wall_h;
       else
        ter(i, j) = t_reinforced_glass_h;
      } else if ((i - lw) % 2 == 0)
       ter(i, j) = t_wall_v;
      else if (j == tw + 2)
       ter(i, j) = t_wall_h;
      else {	// Empty space holds monsters!
       mon_id type = mon_id(rng(mon_flying_polyp, mon_gozu));
       add_spawn(type, 1, i, j);
      }
     }
    }
   }
   tmpcomp = add_computer(SEEX, 8, "Sub-prime contact console", 7);
   tmpcomp->add_option("Terminate Specimens", COMPACT_TERMINATE, 2);
   tmpcomp->add_option("Release Specimens", COMPACT_RELEASE, 3);
   tmpcomp->add_option("Toggle Portal", COMPACT_PORTAL, 8);
   tmpcomp->add_option("Activate Resonance Cascade", COMPACT_CASCADE, 10);
   tmpcomp->add_failure(COMPFAIL_MANHACKS);
   tmpcomp->add_failure(COMPFAIL_SECUBOTS);
   ter(SEEX - 2, 4) = t_radio_tower;
   ter(SEEX + 1, 4) = t_radio_tower;
   ter(SEEX - 2, 7) = t_radio_tower;
   ter(SEEX + 1, 7) = t_radio_tower;
   } break;

  case 3: // Bionics
   add_spawn(mon_secubot, 1,            6,            6);
   add_spawn(mon_secubot, 1, SEEX * 2 - 7,            6);
   add_spawn(mon_secubot, 1,            6, SEEY * 2 - 7);
   add_spawn(mon_secubot, 1, SEEX * 2 - 7, SEEY * 2 - 7);
   add_trap(SEEX - 2, SEEY - 2, tr_dissector);
   add_trap(SEEX + 1, SEEY - 2, tr_dissector);
   add_trap(SEEX - 2, SEEY + 1, tr_dissector);
   add_trap(SEEX + 1, SEEY + 1, tr_dissector);
   _stock_square(*this, t_counter, mi_bionics, 75, point(SEE - 1), point(SEE));
   line(this, t_reinforced_glass_h, SEEX - 2, SEEY - 2, SEEX + 1, SEEY - 2);
   line(this, t_reinforced_glass_h, SEEX - 2, SEEY + 1, SEEX + 1, SEEY + 1);
   line(this, t_reinforced_glass_v, SEEX - 2, SEEY - 1, SEEX - 2, SEEY);
   line(this, t_reinforced_glass_v, SEEX + 1, SEEY - 1, SEEX + 1, SEEY);
   ter(SEEX - 3, SEEY - 3) = t_console;
   tmpcomp = add_computer(SEEX - 3, SEEY - 3, "Bionic access", 3);
   tmpcomp->add_option("Manifest", COMPACT_LIST_BIONICS, 0);
   tmpcomp->add_option("Open Chambers", COMPACT_RELEASE, 5);
   tmpcomp->add_failure(COMPFAIL_MANHACKS);
   tmpcomp->add_failure(COMPFAIL_SECUBOTS);
   break;
  }
  break;

 case ot_bunker:
  if (t_above == ot_null) {	// We're on ground level
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++)
     ter(i, j) = grass_or_dirt();
   }
   line(this, t_wall_metal_h,  7,  7, 16,  7);
   line(this, t_wall_metal_h,  8,  8, 15,  8);
   line(this, t_wall_metal_h,  8, 15, 15, 15);
   line(this, t_wall_metal_h,  7, 16, 10, 16);
   line(this, t_wall_metal_h, 14, 16, 16, 16);
   line(this, t_wall_metal_v,  7,  8,  7, 15);
   line(this, t_wall_metal_v, 16,  8, 16, 15);
   line(this, t_wall_metal_v,  8,  9,  8, 14);
   line(this, t_wall_metal_v, 15,  9, 15, 14);
   square(this, t_floor, 9, 10, 14, 14);
   line(this, t_stairs_down,  11,  9, 12,  9);
   line(this, t_door_metal_locked, 11, 15, 12, 15);
   for (int i = 9; i <= 13; i += 2) {
    line(this, t_wall_metal_h,  9, i, 10, i);
    line(this, t_wall_metal_h, 13, i, 14, i);
    add_spawn(mon_turret, 1, 9, i + 1);
    add_spawn(mon_turret, 1, 14, i + 1);
   }
   ter(13, 16) = t_card_military;
  } else { // Below ground!
   square(this, t_rock,  point(0), point(2 * SEE - 1));
   square(this, t_floor, point(1), point(2 * SEE - 2));
   line(this, t_wall_metal_h,  2,  8,  8,  8);
   line(this, t_wall_metal_h, 15,  8, 21,  8);
   line(this, t_wall_metal_h,  2, 15,  8, 15);
   line(this, t_wall_metal_h, 15, 15, 21, 15);
   for (int j = 2; j <= 16; j += 7) {
    ter( 9, j    ) = t_card_military;
    ter(14, j    ) = t_card_military;
    ter( 9, j + 1) = t_door_metal_locked;
    ter(14, j + 1) = t_door_metal_locked;
    line(this, t_reinforced_glass_v,  9, j + 2,  9, j + 4);
    line(this, t_reinforced_glass_v, 14, j + 2, 14, j + 4);
    line(this, t_wall_metal_v,  9, j + 5,  9, j + 6);
    line(this, t_wall_metal_v, 14, j + 5, 14, j + 6);

// Fill rooms with items!
    for (int i = 2; i <= 15; i += 13) {
     static const constexpr std::pair<std::pair<items_location,int>,int> bunker_stock[] = {
         std::pair(std::pair(mi_bots, 85), 2),
         std::pair(std::pair(mi_launchers, 83), 2),
         std::pair(std::pair(mi_mil_rifles, 87), 2),
         std::pair(std::pair(mi_grenades, 88), 2),
         std::pair(std::pair(mi_mil_armor, 85), 2),
         std::pair(std::pair(mi_mil_food, 90), 3),
         std::pair(std::pair(mi_bionics_mil, 78), 1)
     };

     const auto stock = use_rarity_table(rng(0,13), std::begin(bunker_stock), std::end(bunker_stock));
     place_items(stock.first, stock.second, i, j, i + 6, j + 5, false, 0);
    }
   }
   line(this, t_wall_metal_h, 1, 1, SEEX * 2 - 2, 1);
   line(this, t_wall_metal_h, 1, SEEY * 2 - 2, SEEX * 2 - 2, SEEY * 2 - 2);
   line(this, t_wall_metal_v, 1, 2, 1, SEEY * 2 - 3);
   line(this, t_wall_metal_v, SEEX * 2 - 2, 2, SEEX * 2 - 2, SEEY * 2 - 3);
   ter(SEEX - 1, 21) = t_stairs_up;
   ter(SEEX,     21) = t_stairs_up;
  }
  break;

 case ot_outpost: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  square(this, t_dirt, point(3), point(20));
  line(this, t_wall_metal_h,  2,  2, 10,  2);
  line(this, t_wall_metal_h, 13,  2, 21,  2);
  line(this, t_wall_metal_h,  2, 21, 10, 21);
  line(this, t_wall_metal_h, 13, 21, 21, 21);
  line(this, t_wall_metal_v,  2,  3,  2, 10);
  line(this, t_wall_metal_v, 21,  3, 21, 10);
  line(this, t_wall_metal_v,  2, 13,  2, 20);
  line(this, t_wall_metal_v, 21, 13, 21, 20);
// Place some random buildings

  bool okay = true;
  while (okay) {
   int buildx = rng(6, 17), buildy = rng(6, 17);
   int buildwidthmax  = (buildx <= 11 ? buildx - 3 : 20 - buildx),
       buildheightmax = (buildy <= 11 ? buildy - 3 : 20 - buildy);
   int buildwidth = rng(3, buildwidthmax), buildheight = rng(3, buildheightmax);
   if (ter(buildx, buildy) != t_dirt)
    okay = false;
   else {
    int bx1 = buildx - buildwidth, bx2 = buildx + buildwidth,
        by1 = buildy - buildheight, by2 = buildy + buildheight;
    square(this, t_floor, bx1, by1, bx2, by2);
    line(this, t_wall_metal_h, bx1, by1, bx2, by1);
    line(this, t_wall_metal_h, bx1, by2, bx2, by2);
    line(this, t_wall_metal_v, bx1, by1, bx1, by2);
    line(this, t_wall_metal_v, bx2, by1, bx2, by2);
    switch (rng(1, 3)) {  // What type of building?
    case 1: // Barracks
     for (int i = by1 + 1; i <= by2 - 1; i += 2) {
      line(this, t_bed, bx1 + 1, i, bx1 + 2, i);
      line(this, t_bed, bx2 - 2, i, bx2 - 1, i);
     }
     place_items(mi_bedroom, 84, bx1 + 1, by1 + 1, bx2 - 1, by2 - 1, false, 0);
     break;
    case 2: // Armory
     _stock_line(*this, t_counter, mi_mil_rifles, 40, point(bx1 + 1, by1 + 1), point(bx2 - 1, by1 + 1));
     _stock_line(*this, t_counter, mi_launchers, 40, point(bx1 + 1, by2 - 1), point(bx2 - 1, by2 - 1));
     _stock_line(*this, t_counter, mi_grenades, 40, point(bx1 + 1, by1 + 2), point(bx1 + 1, by2 - 2));
     _stock_line(*this, t_counter, mi_mil_armor, 40, point(bx2 - 1, by1 + 2), point(bx2 - 1, by2 - 2));
     break;
    case 3: // Supplies
     for (int i = by1 + 1; i <= by2 - 1; i += 3) {
     _stock_line(*this, t_rack, mi_mil_food, 78, point(bx1 + 2, i), point(bx2 - 2, i));
     }
     break;
    }
    std::vector<direction> doorsides;
    if (bx1 > 3) doorsides.push_back(WEST);
    if (bx2 < 20) doorsides.push_back(EAST);
    if (by1 > 3) doorsides.push_back(NORTH);
    if (by2 < 20) doorsides.push_back(SOUTH);
    int doorx, doory;
    switch (doorsides[rng(0, doorsides.size() - 1)]) {
     case WEST:
      doorx = bx1;
      doory = rng(by1 + 1, by2 - 1);
      break;
     case EAST:
      doorx = bx2;
      doory = rng(by1 + 1, by2 - 1);
      break;
     case NORTH:
      doorx = rng(bx1 + 1, bx2 - 1);
      doory = by1;
      break;
     case SOUTH:
      doorx = rng(bx1 + 1, bx2 - 1);
      doory = by2;
      break;
    }
    for (int i = doorx - 1; i <= doorx + 1; i++) {
     for (int j = doory - 1; j <= doory + 1; j++) {
      i_clear(i, j);
	  rewrite<t_bed, t_rack, t_counter, t_floor>(i, j);
     }
    }
    ter(doorx, doory) = t_door_c;
   }
  }

  install_military_base_turret(*this, point(SEE - 1, 3),           NORTH, t_wall_metal_v);
  install_military_base_turret(*this, point(SEE,     3),           NORTH, t_wall_metal_v);
  install_military_base_turret(*this, point(SEE - 1, 2 * SEE - 4), SOUTH, t_wall_metal_v);
  install_military_base_turret(*this, point(SEE,     2 * SEE - 4), SOUTH, t_wall_metal_v);
  install_military_base_turret(*this, point(3, SEE - 1),           WEST,  t_wall_metal_v);
  install_military_base_turret(*this, point(3, SEE),               WEST,  t_wall_metal_v);
  install_military_base_turret(*this, point(2 * SEE - 4, SEE - 1), EAST,  t_wall_metal_v);
  install_military_base_turret(*this, point(2 * SEE - 4, SEE),     EAST,  t_wall_metal_v);

// Finally, scatter dead bodies / mil zombies
  for (int i = 0; i < 20; i++) {
   point dest(rng(3, 20), rng(3, 20));
   int rnx = rng(3, 20), rny = rng(3, 20);
   if (move_cost(dest) != 0) {
    if (one_in(5)) // Military zombie
     add_spawn(mon_zombie_soldier, 1, dest.x, dest.y);
    else if (one_in(2)) {
     static const constexpr std::pair<items_location, int> deceased_stock[] = {
         std::pair(mi_launchers, 10),
         std::pair(mi_mil_rifles, 30),
         std::pair(mi_mil_armor, 70),
         std::pair(mi_mil_food, 40)
     };

     item body(0);  // \todo? something more specific
     add_item(dest, body);
     _place_items(*this, std::begin(deceased_stock), std::end(deceased_stock), dest, dest, true);
     add_item(dest, item::types[itm_id_military], 0);
    } else if (one_in(20))
     rough_circle(this, t_rubble, dest.x, dest.y, rng(3, 6));
   }
  }
// Oh wait--let's also put radiation in any rubble
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
	auto& rad = radiation(i,j);
	rad += (one_in(5) ? rng(1, 2) : 0);
    if (ter(i, j) == t_rubble) rad += rng(1, 3);
   }
  }
 } break;
    
 case ot_silo:
  if (t_above == ot_null) {	// We're on ground level
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
	 ter(i, j) = (6 >= trig_dist(i, j, SEEX, SEEY)) ? t_metal_floor : grass_or_dirt();
    }
   }
   switch (rng(1, 4)) {	// Placement of stairs
   case 1:
    lw = 3;
    mw = 5;
    tw = 3;
    break;
   case 2:
    lw = 3;
    mw = 5;
    tw = SEEY * 2 - 4;
    break;
   case 3:
    lw = SEEX * 2 - 7;
    mw = lw;
    tw = 3;
    break;
   case 4:
    lw = SEEX * 2 - 7;
    mw = lw;
    tw = SEEY * 2 - 4;
    break;
   }
   for (int i = lw; i <= lw + 2; i++) {
    ter(i, tw    ) = t_wall_metal_h;
    ter(i, tw + 2) = t_wall_metal_h;
   }
   ter(lw    , tw + 1) = t_wall_metal_v;
   ter(lw + 1, tw + 1) = t_stairs_down;
   ter(lw + 2, tw + 1) = t_wall_metal_v;
   ter(mw    , tw + 1) = t_door_metal_locked;
   ter(mw    , tw + 2) = t_card_military;

  } else {	// We are NOT above ground.
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
     if (trig_dist(i, j, SEEX, SEEY) > 7)
      ter(i, j) = t_rock;
     else if (trig_dist(i, j, SEEX, SEEY) > 5) {
      ter(i, j) = t_metal_floor;
      if (one_in(30))
       add_field(NULL, i, j, fd_nuke_gas, 2);	// NULL game; no messages
     } else if (trig_dist(i, j, SEEX, SEEY) == 5) {
      ter(i, j) = t_hole;
      add_trap(i, j, tr_ledge);
     } else
      ter(i, j) = t_missile;
    }
   }
   silo_rooms(this);
  }
  break;

 case ot_silo_finale: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i == 5) {
     if (j > 4 && j < SEEY)
      ter(i, j) = t_reinforced_glass_v;
     else if (j == SEEY * 2 - 4)
      ter(i, j) = t_door_metal_c;
     else
      ter(i, j) = t_rock;
    } else
     ter(i, j) = t_rock_floor;
   }
  }
  ter(0, 0) = t_stairs_up;
  tmpcomp = add_computer(4, 5, "Missile Controls", 8);
  tmpcomp->add_option("Launch Missile", COMPACT_MISS_LAUNCH, 10);
  tmpcomp->add_option("Disarm Missile", COMPACT_MISS_DISARM,  8);
  tmpcomp->add_failure(COMPFAIL_SECUBOTS);
  tmpcomp->add_failure(COMPFAIL_DAMAGE);
  } break;

 case ot_temple:
 case ot_temple_stairs:
  if (t_above == ot_null) { // Ground floor
// TODO: More varieties?
   square(this, t_dirt, point(0), point(2 * SEE - 1));
   square(this, t_grate, point(SEE - 1), point(SEE));
   ter(SEEX + 1, SEEY + 1) = t_pedestal_temple;
  } else { // Underground!  Shit's about to get interesting!
// Start with all rock floor
   square(this, t_rock_floor, point(0), point(2 * SEE - 1));
// We always start at the south and go north.
// We use (g->levx / 2 + g->levz) % 5 to guarantee that rooms don't repeat.
   switch (1 + int(g->lev.y / 2 + g->lev.z) % 4) {// TODO: More varieties!
    case 1: // Flame bursts
     square(this, t_rock, 0, 0, SEEX - 1, SEEY * 2 - 1);
     square(this, t_rock, SEEX + 2, 0, SEEX * 2 - 1, SEEY * 2 - 1);
     for (int i = 2; i < SEEY * 2 - 4; i++) {
      add_field(g, SEEX    , i, fd_fire_vent, rng(1, 3));
      add_field(g, SEEX + 1, i, fd_fire_vent, rng(1, 3));
     }
     break;

    case 2: // Spreading water
     square(this, t_water_dp, point(4), point(5));
     add_spawn(mon_sewer_snake, 1, 4, 4);

     square(this, t_water_dp, SEEX * 2 - 5, 4, SEEX * 2 - 4, 6);
     add_spawn(mon_sewer_snake, 1, SEEX * 2 - 5, 4);

     square(this, t_water_dp, 4, SEEY * 2 - 5, 6, SEEY * 2 - 4);

     square(this, t_water_dp, point(2 * SEE - 5), point(2 * SEE - 4));

     square(this, t_rock, 0, SEEY * 2 - 2, SEEX - 1, SEEY * 2 - 1);
     square(this, t_rock, SEEX + 2, SEEY * 2 - 2, SEEX * 2 - 1, SEEY * 2 - 1);
     line(this, t_grate, SEEX, 1, SEEX + 1, 1); // To drain the water
     add_trap(SEEX, SEEY * 2 - 2, tr_temple_flood);
     add_trap(SEEX + 1, SEEY * 2 - 2, tr_temple_flood);
     for (int y = 2; y < SEEY * 2 - 2; y++) {
      for (int x = 2; x < SEEX * 2 - 2; x++) {
       if (ter(x, y) == t_rock_floor && one_in(4))
        add_trap(x, y, tr_temple_flood);
      }
     }
     break;

    case 3: { // Flipping walls puzzle
     line(this, t_rock, 0, 0, SEEX - 1, 0);
     line(this, t_rock, SEEX + 2, 0, SEEX * 2 - 1, 0);
     line(this, t_rock, SEEX - 1, 1, SEEX - 1, 6);
     line(this, t_bars, SEEX + 2, 1, SEEX + 2, 6);
     ter(14, 1) = t_switch_rg;
     ter(15, 1) = t_switch_gb;
     ter(16, 1) = t_switch_rb;
     ter(17, 1) = t_switch_even;
// Start with clear floors--then work backwards to the starting state
     line(this, t_floor_red,   SEEX, 1, SEEX + 1, 1);
     line(this, t_floor_green, SEEX, 2, SEEX + 1, 2);
     line(this, t_floor_blue,  SEEX, 3, SEEX + 1, 3);
     line(this, t_floor_red,   SEEX, 4, SEEX + 1, 4);
     line(this, t_floor_green, SEEX, 5, SEEX + 1, 5);
     line(this, t_floor_blue,  SEEX, 6, SEEX + 1, 6);
// Now, randomly choose actions
// Set up an actions vector so that there's not undue repetion
     std::vector<int> actions;
     actions.push_back(1);	// offset from ter_id values is t_switch_rg-1
     actions.push_back(2);
     actions.push_back(3);
     actions.push_back(4);
     actions.push_back(rng(1, 3));
     while (!actions.empty()) {
      int index = rng(0, actions.size() - 1);
      int action = actions[index];
      actions.erase(actions.begin() + index);
      for (int y = 1; y < 7; y++) {
       for (int x = SEEX; x <= SEEX + 1; x++) {
		apply_temple_switch((ter_id)(action + (t_switch_rg-1)),1,x,y);
       }
      }
     }
    } break;

    case 4: { // Toggling walls maze
     square(this, t_rock,        0,            0, SEEX     - 1,            1);
     square(this, t_rock,        0, SEEY * 2 - 2, SEEX     - 1, SEEY * 2 - 1);
     square(this, t_rock,        0,            2, SEEX     - 4, SEEY * 2 - 3);
     square(this, t_rock, SEEX + 2,            0, SEEX * 2 - 1,            1);
     square(this, t_rock, SEEX + 2, SEEY * 2 - 2, SEEX * 2 - 1, SEEY * 2 - 1);
     square(this, t_rock, SEEX + 5,            2, SEEX * 2 - 1, SEEY * 2 - 3);
     int x = rng(SEEX - 1, SEEX + 2), y = 2;
     std::vector<point> path; // Path, from end to start
     while (x < SEEX - 1 || x > SEEX + 2 || y < SEEY * 2 - 2) {
      path.push_back( point(x, y) );
      ter(x, y) = ter_id( rng(t_floor_red, t_floor_blue) );
      if (y == SEEY * 2 - 2) {
       if (x < SEEX - 1)
        x++;
       else if (x > SEEX + 2)
        x--;
      } else {
       std::vector<point> next;
       for (int nx = x - 1; nx <= x + 1; nx++ ) {
        for (int ny = y; ny <= y + 1; ny++) {
         if (ter(nx, ny) == t_rock_floor)
          next.push_back( point(nx, ny) );
        }
       }
       int index = rng(0, next.size() - 1);
       x = next[index].x;
       y = next[index].y;
      }
     }
// Now go backwards through path (start to finish), toggling any tiles that need
     bool toggle_red = false, toggle_green = false, toggle_blue = false;
     for (int i = path.size() - 1; i >= 0; i--) {
	  auto& t = ter(path[i]);
      if (t_floor_red == t) {
       toggle_green = !toggle_green;
       if (toggle_red) t = t_rock_red;
      } else if (t_floor_green == t) {
       toggle_blue = !toggle_blue;
       if (toggle_green) t = t_rock_green;
      } else if (t_floor_blue == t) {
       toggle_red = !toggle_red;
       if (toggle_blue) t = t_rock_blue;
      }
     }
// Finally, fill in the rest with random tiles, and place toggle traps
     for (int i = SEEX - 3; i <= SEEX + 4; i++) {
      for (int j = 2; j <= SEEY * 2 - 2; j++) {
       add_trap(i, j, tr_temple_toggle);
	   rewrite<t_rock_floor>(i,j, ter_id(rng(t_rock_red, t_floor_blue)));
      }
     }
    } break;
   } // Done with room type switch
// Stairs down if we need them
   if (terrain_type == ot_temple_stairs)
    line(this, t_stairs_down, SEEX, 0, SEEX + 1, 0);
// Stairs at the south if t_above has stairs down.
   if (t_above == ot_temple_stairs)
    line(this, t_stairs_up, SEEX, SEEY * 2 - 1, SEEX + 1, SEEY * 2 - 1);

  } // Done with underground-only stuff
  break;

 case ot_temple_finale:
  square(this, t_rock, 0, 0, SEEX * 2 - 1, SEEX * 2 - 1);
  square(this, t_rock_floor, SEEX - 1, 1, SEEX + 2, 4);
  square(this, t_rock_floor, SEEX, 5, SEEX + 1, SEEY * 2 - 1);
  line(this, t_stairs_up, SEEX, SEEY * 2 - 1, SEEX + 1, SEEY * 2 - 1);
  add_item(rng(SEEX, SEEX + 1), rng(2, 3), g->new_artifact(), 0);
  add_item(rng(SEEX, SEEX + 1), rng(2, 3), g->new_artifact(), 0);
  return;

 case ot_sewage_treatment:
  square(this, t_floor, point(0), point(2 * SEE - 1)); // Set all to floor
  line(this, t_wall_h,  0,  0, 23,  0); // Top wall
  line(this, t_window,  1,  0,  6,  0); // Its windows
  line(this, t_wall_h,  0, 23, 23, 23); // Bottom wall
  line(this, t_wall_h,  1,  5,  6,  5); // Interior wall (front office)
  line(this, t_wall_h,  1, 14,  6, 14); // Interior wall (equipment)
  line(this, t_wall_h,  1, 20,  7, 20); // Interior wall (stairs)
  line(this, t_wall_h, 14, 15, 22, 15); // Interior wall (tank)
  line(this, t_wall_v,  0,  1,  0, 22); // Left wall
  line(this, t_wall_v, 23,  1, 23, 22); // Right wall
  line(this, t_wall_v,  7,  1,  7,  5); // Interior wall (front office)
  line(this, t_wall_v,  7, 14,  7, 19); // Interior wall (stairs)
  line(this, t_wall_v,  4, 15,  4, 19); // Interior wall (mid-stairs)
  line(this, t_wall_v, 14, 15, 14, 20); // Interior wall (tank)
  line(this, t_wall_glass_v,  7,  6,  7, 13); // Interior glass (equipment)
  line(this, t_wall_glass_h,  8, 20, 13, 20); // Interior glass (flow)
  line(this, t_counter,  1,  3,  3,  3); // Desk (front office);
  line(this, t_counter,  1,  6,  1, 13); // Counter (equipment);
// Central tanks:
  square(this, t_sewage, 10,  3, 13,  6);
  square(this, t_sewage, 17,  3, 20,  6);
  square(this, t_sewage, 10, 10, 13, 13);
  square(this, t_sewage, 17, 10, 20, 13);
// Drainage tank
  square(this, t_sewage, 16, 16, 21, 18);
  square(this, t_grate,  18, 16, 19, 17);
  line(this, t_sewage, 17, 19, 20, 19);
  line(this, t_sewage, 18, 20, 19, 20);
  line(this, t_sewage,  2, 21, 19, 21);
  line(this, t_sewage,  2, 22, 19, 22);
// Pipes and pumps
  line(this, t_sewage_pipe,  1, 15,  1, 19);
  line(this, t_sewage_pump,  1, 21,  1, 22);
// Stairs down
  ter(2, 15) = t_stairs_down;
// Now place doors
  ter(rng(2, 5), 0) = t_door_c;
  ter(rng(3, 5), 5) = t_door_c;
  ter(5, 14) = t_door_c;
  ter(7, rng(15, 17)) = t_door_c;
  ter(14, rng(17, 19)) = t_door_c;
  if (one_in(3)) // back door
   ter(23, rng(19, 22)) = t_door_locked;
  ter(4, 19) = t_door_metal_locked;
  ter(2, 19) = t_console;
  ter(6, 19) = t_console;
// Computers to unlock stair room, and items
  tmpcomp = add_computer(2, 19, "EnviroCom OS v2.03", 1);
  tmpcomp->add_option("Unlock stairs", COMPACT_OPEN, 0);
  tmpcomp->add_failure(COMPFAIL_SHUTDOWN);

  tmpcomp = add_computer(6, 19, "EnviroCom OS v2.03", 1);
  tmpcomp->add_option("Unlock stairs", COMPACT_OPEN, 0);
  tmpcomp->add_failure(COMPFAIL_SHUTDOWN);
  place_items(mi_sewage_plant, 80, 1, 6, 1, 13, false, 0);
  break;

 case ot_sewage_treatment_hub: // Stairs up, center of 3x3 of treatment_below
  square(this, t_floor, point(0), point(2 * SEE - 1));
// Top & left walls; right & bottom are handled by adjacent terrain
  line(this, t_wall_h,  0,  0, 23,  0);
  line(this, t_wall_v,  0,  1,  0, 23);
// Top-left room
  line(this, t_wall_v,  8,  1,  8,  8);
  line(this, t_wall_h,  1,  9,  9,  9);
  line(this, t_wall_glass_h, rng(1, 3), 9, rng(4, 7), 9);
  ter(2, 15) = t_stairs_up;
  ter(8, 8) = t_door_c;
  ter(3, 0) = t_door_c;

// Bottom-left room - stairs and equipment
  line(this, t_wall_h,  1, 14,  8, 14);
  line(this, t_wall_glass_h, rng(1, 3), 14, rng(5, 8), 14);
  line(this, t_wall_v,  9, 14,  9, 23);
  line(this, t_wall_glass_v, 9, 16, 9, 19);
  _stock_square(*this, t_counter, mi_sewage_plant, 80, point(5, 16), point(6, 20));
  ter(0, 20) = t_door_c;
  ter(9, 20) = t_door_c;

// Bottom-right room
  line(this, t_wall_v, 14, 19, 14, 23);
  line(this, t_wall_h, 14, 18, 19, 18);
  line(this, t_wall_h, 21, 14, 23, 14);
  ter(14, 18) = t_wall_h;
  ter(14, 20) = t_door_c;
  ter(15, 18) = t_door_c;
  line(this, t_wall_v, 20, 15, 20, 18);

// Tanks and their content
  for (int i = 9; i <= 16; i += 7) {
   for (int j = 2; j <= 9; j += 7) {
    point nw_corner(i,j);
    square(this, t_rock, nw_corner, nw_corner + 5*Direction::SE);
    square(this, t_sewage, nw_corner + Direction::SE, nw_corner + 4 * Direction::SE);
   }
  }
  square(this, t_rock, 16, 15, 19, 17); // Wall around sewage from above
  square(this, t_rock, 10, 15, 14, 17); // Extra walls for southward flow
// Flow in from north, east, and west always connects to the corresponding tank
  square(this, t_sewage, 10,  0, 13,  2); // North -> NE tank
  square(this, t_sewage, 21, 10, 23, 13); // East  -> SE tank
  square(this, t_sewage,  0, 10,  9, 13); // West  -> SW tank
// Flow from south may go to SW tank or SE tank
  square(this, t_sewage, 10, 16, 13, 23);
  if (one_in(2)) { // To SW tank
   square(this, t_sewage, 10, 14, 13, 17);
// Then, flow from above may be either to flow from south, to SE tank, or both
   switch (rng(1, 5)) {
   case 1:
   case 2: // To flow from south
    square(this, t_sewage, 14, 16, 19, 17);
    line(this, t_bridge, 15, 16, 15, 17);
    if (!one_in(4))
     line(this, t_wall_glass_h, 16, 18, 19, 18); // Viewing window
    break;
   case 3:
   case 4: // To SE tank
    square(this, t_sewage, 18, 14, 19, 17);
    if (!one_in(4))
     line(this, t_wall_glass_v, 20, 15, 20, 17); // Viewing window
    break;
   case 5: // Both!
    square(this, t_sewage, 14, 16, 19, 17);
    square(this, t_sewage, 18, 14, 19, 17);
   line(this, t_bridge, 15, 16, 15, 17);
    if (!one_in(4))
     line(this, t_wall_glass_h, 16, 18, 19, 18); // Viewing window
    if (!one_in(4))
     line(this, t_wall_glass_v, 20, 15, 20, 17); // Viewing window
    break;
   }
  } else { // To SE tank, via flow from above
   square(this, t_sewage, 14, 16, 19, 17);
   square(this, t_sewage, 18, 14, 19, 17);
   line(this, t_bridge, 15, 16, 15, 17);
   if (!one_in(4))
    line(this, t_wall_glass_h, 16, 18, 19, 18); // Viewing window
   if (!one_in(4))
    line(this, t_wall_glass_v, 20, 15, 20, 17); // Viewing window
  }

// Next, determine how the tanks interconnect.
  rn = rng(1, 4); // Which of the 4 possible connections is missing?
  if (rn != 1) {
   line(this, t_sewage, 14,  4, 14,  5);
   line(this, t_bridge, 15,  4, 15,  5);
   line(this, t_sewage, 16,  4, 16,  5);
  }
  if (rn != 2) {
   line(this, t_sewage, 18,  7, 19,  7);
   line(this, t_bridge, 18,  8, 19,  8);
   line(this, t_sewage, 18,  9, 19,  9);
  }
  if (rn != 3) {
   line(this, t_sewage, 14, 11, 14, 12);
   line(this, t_bridge, 15, 11, 15, 12);
   line(this, t_sewage, 16, 11, 16, 12);
  }
  if (rn != 4) {
   line(this, t_sewage, 11,  7, 12,  7);
   line(this, t_bridge, 11,  8, 12,  8);
   line(this, t_sewage, 11,  9, 12,  9);
  }
// Bridge connecting bottom two rooms
  line(this, t_bridge, 10, 20, 13, 20);
// Possibility of extra equipment shelves
  if (!one_in(3)) _stock_line(*this, t_rack, mi_sewage_plant, 60, point(23, 1), point(23, 4));

// Finally, choose what the top-left and bottom-right rooms do.
  if (one_in(2)) { // Upper left is sampling, lower right valuable finds
// Upper left...
   line(this, t_wall_h, 1, 3, 2, 3);
   line(this, t_wall_h, 1, 5, 2, 5);
   line(this, t_wall_h, 1, 7, 2, 7);
   ter(1, 4) = t_sewage_pump;
   ter(2, 4) = t_counter;
   ter(1, 6) = t_sewage_pump;
   ter(2, 6) = t_counter;
   ter(1, 2) = t_console;
   tmpcomp = add_computer(1, 2, "EnviroCom OS v2.03", 0);
   tmpcomp->add_option("Download Sewer Maps", COMPACT_MAP_SEWER, 0);
   tmpcomp->add_option("Divert sample", COMPACT_SAMPLE, 3);
   tmpcomp->add_failure(COMPFAIL_PUMP_EXPLODE);
   tmpcomp->add_failure(COMPFAIL_PUMP_LEAK);
// Lower right...
   _stock_line(*this, t_counter, mi_sewer, 65, point(15, 23), point(22, 23));
   _stock_line(*this, t_counter, mi_sewer, 65, point(23, 15), point(23, 19));
  } else { // Upper left is valuable finds, lower right is sampling
// Upper left...
   _stock_line(*this, t_counter, mi_sewer, 65, point(1, 1), point(1, 7));
   _stock_line(*this, t_counter, mi_sewer, 65, point(7, 1), point(7, 7));
// Lower right...
   line(this, t_wall_v, 17, 22, 17, 23);
   line(this, t_wall_v, 19, 22, 19, 23);
   line(this, t_wall_v, 21, 22, 21, 23);
   ter(18, 23) = t_sewage_pump;
   ter(18, 22) = t_counter;
   ter(20, 23) = t_sewage_pump;
   ter(20, 22) = t_counter;
   ter(16, 23) = t_console;
   tmpcomp = add_computer(16, 23, "EnviroCom OS v2.03", 0);
   tmpcomp->add_option("Download Sewer Maps", COMPACT_MAP_SEWER, 0);
   tmpcomp->add_option("Divert sample", COMPACT_SAMPLE, 3);
   tmpcomp->add_failure(COMPFAIL_PUMP_EXPLODE);
   tmpcomp->add_failure(COMPFAIL_PUMP_LEAK);
  }
  break;

 case ot_sewage_treatment_under:
  square(this, t_floor, point(0), point(2*SEE-1));

  if (t_north == ot_sewage_treatment_under || t_north == ot_sewage_treatment_hub ||
      (t_north >= ot_sewer_ns && t_north <= ot_sewer_nesw &&
       connects_to(t_north, 2))) {
   if (t_north == ot_sewage_treatment_under ||
       t_north == ot_sewage_treatment_hub) {
    line(this, t_wall_h,  0,  0, 23,  0);
    ter(3, 0) = t_door_c;
   }
   n_fac = 1;
   square(this, t_sewage, 10, 0, 13, 13);
  }

  if (t_east == ot_sewage_treatment_under ||
      t_east == ot_sewage_treatment_hub ||
      (t_east >= ot_sewer_ns && t_east <= ot_sewer_nesw &&
       connects_to(t_east, 3))) {
   e_fac = 1;
   square(this, t_sewage, 10, 10, 23, 13);
  }

  if (t_south == ot_sewage_treatment_under ||
      t_south == ot_sewage_treatment_hub ||
      (t_south >= ot_sewer_ns && t_south <= ot_sewer_nesw &&
       connects_to(t_south, 0))) {
   s_fac = 1;
   square(this, t_sewage, 10, 10, 13, 23);
  }

  if (t_west == ot_sewage_treatment_under ||
      t_west == ot_sewage_treatment_hub ||
      (t_west >= ot_sewer_ns && t_west <= ot_sewer_nesw &&
       connects_to(t_west, 1))) {
   if (t_west == ot_sewage_treatment_under ||
       t_west == ot_sewage_treatment_hub) {
    line(this, t_wall_v,  0,  1,  0, 23);
    ter(0, 20) = t_door_c;
   }
   w_fac = 1;
   square(this, t_sewage,  0, 10, 13, 13);
  }
  break;

 case ot_mine_entrance: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  int tries = 0;
  bool build_shaft = true;
  do {
   int x1 = rng(1, SEEX * 2 - 10), y1 = rng(1, SEEY * 2 - 10);
   int x2 = x1 + rng(4, 9),        y2 = y1 + rng(4, 9);
   if (build_shaft) {
    build_mine_room(this, room_mine_shaft, x1, y1, x2, y2);
    build_shaft = false;
   } else {
    bool okay = true;
    for (int x = x1; x <= x2 && okay; x++) {
     for (int y = y1; y <= y2 && okay; y++) {
      auto& t = ter(x, y);
      if (t_grass != t && t_dirt != t) okay = false;
     }
    }
    if (okay) {
     room_type type = room_type( rng(room_mine_office, room_mine_housing) );
     build_mine_room(this, type, x1, y1, x2, y2);
     tries = 0;
    } else
     tries++;
   }
  } while (tries < 5);
  do {	// theoretically could infinite-loop
    int ladderx = rng(0, SEEX * 2 - 1), laddery = rng(0, SEEY * 2 - 1);
	if (rewrite_test<t_dirt, t_grass, t_manhole_cover>(ladderx, laddery)) break;
  } while (true); 
 } break;

 case ot_mine_shaft: // Not intended to actually be inhabited!
  square(this, t_rock, point(0), point(23));
  square(this, t_hole, point(SEE - 3), point(SEE + 2));
  line(this, t_grate, SEEX - 3, SEEY - 4, SEEX + 2, SEEY - 4);
  ter(SEEX - 3, SEEY - 5) = t_ladder_up;
  ter(SEEX + 2, SEEY - 5) = t_ladder_down;
  rotate(rng(0, 3));
  break;

 case ot_mine:
 case ot_mine_down:
  if (t_north >= ot_mine && t_north <= ot_mine_finale)
   n_fac = (one_in(10) ? 0 : -2);
  else
   n_fac = 4;
  if (t_east >= ot_mine  && t_east <= ot_mine_finale)
   e_fac = (one_in(10) ? 0 : -2);
  else
   e_fac = 4;
  if (t_south >= ot_mine && t_south <= ot_mine_finale)
   s_fac = (one_in(10) ? 0 : -2);
  else
   s_fac = 4;
  if (t_west >= ot_mine  && t_west <= ot_mine_finale)
   w_fac = (one_in(10) ? 0 : -2);
  else
   w_fac = 4;

  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i >= w_fac + rng(0, 2) && i <= SEEX * 2 - 1 - e_fac - rng(0, 2) &&
        j >= n_fac + rng(0, 2) && j <= SEEY * 2 - 1 - s_fac - rng(0, 2) &&
        i + j >= 4 && (SEEX * 2 - i) + (SEEY * 2 - j) >= 6  )
     ter(i, j) = t_rock_floor;
    else
     ter(i, j) = t_rock;
   }
  }

  if (t_above == ot_mine_shaft) { // We need the entrance room
   square(this, t_floor, point(10), point(15));
   line(this, t_wall_h,  9,  9, 16,  9);
   line(this, t_wall_h,  9, 16, 16, 16);
   line(this, t_wall_v,  9, 10,  9, 15);
   line(this, t_wall_v, 16, 10, 16, 15);
   line(this, t_wall_h, 10, 11, 12, 11);
   ter(10, 10) = t_elevator_control;
   ter(11, 10) = t_elevator;
   ter(10, 12) = t_ladder_up;
   _stock_line(*this, t_counter, mi_mine_equipment, 86, point(10, 15), point(15, 15));
   if (one_in(2))
    ter(9, 12) = t_door_c;
   else
    ter(16, 12) = t_door_c;

  } else { // Not an entrance; maybe some hazards!
   switch( rng(0, 6) ) {
    case 0: break; // Nothing!  Lucky!

    case 1: { // Toxic gas
     int cx = rng(9, 14), cy = rng(9, 14);
     ter(cx, cy) = t_rock;
     add_field(g, cx, cy, fd_gas_vent, 1);
    } break;

    case 2: { // Lava
     int x1 = rng(6, SEEX),                y1 = rng(6, SEEY),
         x2 = rng(SEEX + 1, SEEX * 2 - 7), y2 = rng(SEEY + 1, SEEY * 2 - 7);
     int num = rng(2, 4);
     for (int i = 0; i < num; i++) {
      int lx1 = x1 + rng(-1, 1), lx2 = x2 + rng(-1, 1),
          ly1 = y1 + rng(-1, 1), ly2 = y2 + rng(-1, 1);
      line(this, t_lava, lx1, ly1, lx2, ly2);
     }
    } break;

    case 3: { // Wrecked equipment
     int x = rng(9, 14), y = rng(9, 14);
     for (int i = x - 3; i < x + 3; i++) {
      for (int j = y - 3; j < y + 3; j++) {
       if (!one_in(4)) ter(i, j) = t_wreckage;
      }
     }
     place_items(mi_wreckage, 70, x - 3, y - 3, x + 2, y + 2, false, 0);
    } break;

    case 4: { // Dead miners
     int num_bodies = rng(4, 8);
     auto ok_pts = grep(point(0), point(2*SEE-1), [&](const point& pt) {
         return 2 == move_cost(pt);
     });
     while (0 < num_bodies && !ok_pts.empty()) {
         const auto n = rng(0, ok_pts.size() - 1);
         const point& body = ok_pts[n];
         item miner(0);
         add_item(body, miner);
         place_items(mi_mine_equipment, 60, body.x, body.y, body.x, body.y, false, 0);
         if (ok_pts.size() >= num_bodies) ok_pts.erase(ok_pts.begin() + n);
     };
    } break;

    case 5: { // Dark worm!
     std::vector<direction> sides;
     if (4 == n_fac) sides.push_back(NORTH);
     if (4 == e_fac) sides.push_back(EAST);
     if (4 == s_fac) sides.push_back(SOUTH);
     if (4 == w_fac) sides.push_back(WEST);
     if (sides.empty()) add_spawn(mon_dark_wyrm, 1, SEEX, SEEY);
     else {
         int num_worms = rng(1, 5);
         for (int i = 0; i < num_worms; i++) {
             direction side = sides[rng(0, sides.size() - 1)];
             point p;
             switch (side) {
             case NORTH: p = point(rng(1, SEEX * 2 - 2), rng(1, 5)); break;
             case EAST:  p = point(SEEX * 2 - rng(2, 6), rng(1, SEEY * 2 - 2)); break;
             case SOUTH: p = point(rng(1, SEEX * 2 - 2), SEEY * 2 - rng(2, 6)); break;
             case WEST:  p = point(rng(1, 5), rng(1, SEEY * 2 - 2)); break;
             }
             ter(p) = t_rock_floor;
             add_spawn(mon_dark_wyrm, 1, p.x, p.y);
         }
     }
    } break;

    case 6: { // Spiral
	 const point nw_out = point(rng(SEEX - 4, SEEX), rng(SEEY - 4, SEEY));
	 const point nw_in  = nw_out + Direction::SE;
	 const point ne_out = nw_out + 5 * Direction::E;
	 const point se_out = nw_out + 5 * Direction::SE;
	 const point miner_at = nw_out + 2 * Direction::SE + Direction::E;
	 int orx = rng(SEEX - 4, SEEX), ory = rng(SEEY - 4, SEEY);
     line(this, t_rock, nw_out, ne_out);
     line(this, t_rock, ne_out, se_out);
     line(this, t_rock, nw_in + 4*Direction::E, se_out);
     line(this, t_rock, miner_at + Direction::NW, nw_in + 3 * Direction::E);
     line(this, t_rock, miner_at + Direction::NW, miner_at + Direction::SW);
     ter(miner_at + Direction::S) = t_rock;
     item miner(0);
     add_item(miner_at, miner);
     place_items(mi_mine_equipment, 60, miner_at.x, miner_at.y, miner_at.x, miner_at.y, false, 0);
    } break;
   }
  }

  if (terrain_type == ot_mine_down) { // Don't forget to build a slope down!
   std::vector<direction> open;
   if (4 == n_fac) open.push_back(NORTH);
   if (4 == e_fac) open.push_back(EAST);
   if (4 == s_fac) open.push_back(SOUTH);
   if (4 == w_fac) open.push_back(WEST);

   if (open.empty()) { // We'll have to build it in the center
    auto ok_pts = grep(point(SEE-6), point(SEE+1), [&](const point& pt){
        for (int i = pt.x; i <= pt.x + 5; i++) {
            for (int j = pt.y; j <= pt.y + 5; j++) {
                if (t_rock_floor != ter(i, j)) return false;
            }
        }
        return true;
    });
    point p;
    if (!ok_pts.empty()) p = ok_pts[rng(0, ok_pts.size())];
    else { // Clear the area around the slope down
        p.x = rng(SEEX - 6, SEEX + 1);
        p.y = rng(SEEY - 6, SEEY + 1);
        square(this, t_rock_floor, p, p + 5 * Direction::SE);
    }
    square(this, t_slope_down, p + Direction::SE, p + 2*Direction::SE);
   } else { // We can build against a wall
    direction side = open[rng(0, open.size() - 1)];
    switch (side) {
     case NORTH:
      square(this, t_rock_floor, SEEX - 3, 6, SEEX + 2, SEEY);
      line(this, t_slope_down, SEEX - 2, 6, SEEX + 1, 6);
      break;
     case EAST:
      square(this, t_rock_floor, SEEX + 1, SEEY - 3, SEEX * 2 - 7, SEEY + 2);
      line(this, t_slope_down, SEEX * 2 - 7, SEEY - 2, SEEX * 2 - 7, SEEY + 1);
      break;
     case SOUTH:
      square(this, t_rock_floor, SEEX - 3, SEEY + 1, SEEX + 2, SEEY * 2 - 7);
      line(this, t_slope_down, SEEX - 2, SEEY * 2 - 7, SEEX + 1, SEEY * 2 - 7);
      break;
     case WEST:
      square(this, t_rock_floor, 6, SEEY - 3, SEEX, SEEY + 2);
      line(this, t_slope_down, 6, SEEY - 2, 6, SEEY + 1);
      break;
    }
   }
  } // Done building a slope down
   
  if (t_above == ot_mine_down) {  // Don't forget to build a slope up!
   std::vector<direction> open;
   if (n_fac == 4 && ter(SEEX, 6) != t_slope_down) open.push_back(NORTH);
   if (e_fac == 4 && ter(SEEX * 2 - 7, SEEY) != t_slope_down) open.push_back(EAST);
   if (s_fac == 4 && ter(SEEX, SEEY * 2 - 7) != t_slope_down) open.push_back(SOUTH);
   if (w_fac == 4 && ter(6, SEEY) != t_slope_down) open.push_back(WEST);

   if (open.empty()) { // We'll have to build it in the center
    auto ok_pts = grep(point(SEE-6), point(SEE+1), [&](const point& pt){
        for (int i = pt.x; i <= pt.x + 5; i++) {
            for (int j = pt.y; j <= pt.y + 5; j++) {
                if (t_rock_floor != ter(i, j)) return false;
            }
        }
        return true;
    });
    point p;
    if (!ok_pts.empty()) p = ok_pts[rng(0, ok_pts.size())];
    else { // Clear the area around the slope up
        p.x = rng(SEEX - 6, SEEX + 1);
        p.y = rng(SEEY - 6, SEEY + 1);
        square(this, t_rock_floor, p, p + 5 * Direction::SE);
    }
    square(this, t_slope_up, p + Direction::SE, p + 2*Direction::SE);

   } else { // We can build against a wall
    direction side = open[rng(0, open.size() - 1)];
    switch (side) {
     case NORTH:
      line(this, t_slope_up, SEEX - 2, 6, SEEX + 1, 6);
      break;
     case EAST:
      line(this, t_slope_up, SEEX * 2 - 7, SEEY - 2, SEEX * 2 - 7, SEEY + 1);
      break;
     case SOUTH:
      line(this, t_slope_up, SEEX - 2, SEEY * 2 - 7, SEEX + 1, SEEY * 2 - 7);
      break;
     case WEST:
      line(this, t_slope_up, 6, SEEY - 2, 6, SEEY + 1);
      break;
    }
   }
  } // Done building a slope up
  break;

 case ot_mine_finale: {
// Set up the basic chamber
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i > rng(1, 3) && i < SEEX * 2 - rng(2, 4) &&
        j > rng(1, 3) && j < SEEY * 2 - rng(2, 4)   )
     ter(i, j) = t_rock_floor;
    else
     ter(i, j) = t_rock;
   }
  }
  std::vector<direction> face; // Which walls are solid, and can be a facing?
// Now draw the entrance(s)
  if (t_north == ot_mine) square(this, t_rock_floor, SEEX, 0, SEEX + 1, 3);
  else face.push_back(NORTH);

  if (t_east  == ot_mine) square(this, t_rock_floor, SEEX * 2 - 4, SEEY, SEEX * 2 - 1, SEEY + 1);
  else face.push_back(EAST);

  if (t_south == ot_mine) square(this, t_rock_floor, SEEX, SEEY * 2 - 4, SEEX + 1, SEEY * 2 - 1);
  else face.push_back(SOUTH);

  if (t_west  == ot_mine) square(this, t_rock_floor, 0, SEEY, 3, SEEY + 1);
  else face.push_back(WEST);

// Now, pick and generate a type of finale!
  if (face.empty()) rn = rng(1, 3); // Amigara fault is not valid
  else rn = rng(1, 4);

  switch (rn) {
   case 1: { // Wyrms
    int x = rng(SEEX, SEEX + 1), y = rng(SEEY, SEEY + 1);
    ter(x, y) = t_pedestal_wyrm;
    add_item(x, y, item::types[itm_petrified_eye], 0);
   } break; // That's it!  game::examine handles the pedestal/wyrm spawns

   case 2: { // The Thing dog
    item miner(0);
    int num_bodies = rng(4, 8);
    for (int i = 0; i < num_bodies; i++) {
     int x = rng(4, SEEX * 2 - 5), y = rng(4, SEEX * 2 - 5);
     add_item(x, y, miner);
     place_items(mi_mine_equipment, 60, x, y, x, y, false, 0);
    }
    add_spawn(mon_dog_thing, 1, rng(SEEX, SEEX + 1), rng(SEEX, SEEX + 1), true);
    add_item(rng(SEEX, SEEX + 1), rng(SEEY, SEEY + 1), g->new_artifact(), 0);
   } break;

   case 3: { // Spiral down
    line(this, t_rock,  5,  5,  5, 18);
    line(this, t_rock,  5,  5, 18,  5);
    line(this, t_rock, 18,  5, 18, 18);
    line(this, t_rock,  8, 18, 18, 18);
    line(this, t_rock,  8,  8,  8, 18);
    line(this, t_rock,  8,  8, 15,  8);
    line(this, t_rock, 15,  8, 15, 15);
    line(this, t_rock, 10, 15, 15, 15);
    line(this, t_rock, 10, 10, 10, 15);
    line(this, t_rock, 10, 10, 13, 10);
    line(this, t_rock, 13, 10, 13, 13);
    ter(12, 13) = t_rock;
    ter(12, 12) = t_slope_down;
    ter(12, 11) = t_slope_down;
   } break;

   case 4: { // Amigara fault
    direction fault = face[rng(0, face.size() - 1)];
// Construct the fault on the appropriate face
    switch (fault) {
     case NORTH:
      square(this, t_rock, 0, 0, SEEX * 2 - 1, 4);
      line(this, t_fault, 4, 4, SEEX * 2 - 5, 4);
      break;
     case EAST:
      square(this, t_rock, SEEX * 2 - 5, 0, SEEY * 2 - 1, SEEX * 2 - 1);
      line(this, t_fault, SEEX * 2 - 5, 4, SEEX * 2 - 5, SEEY * 2 - 5);
      break;
     case SOUTH:
      square(this, t_rock, 0, SEEY * 2 - 5, SEEX * 2 - 1, SEEY * 2 - 1);
      line(this, t_fault, 4, SEEY * 2 - 5, SEEX * 2 - 5, SEEY * 2 - 5);
      break;
     case WEST:
      square(this, t_rock, 0, 0, 4, SEEY * 2 - 1);
      line(this, t_fault, 4, 4, 4, SEEY * 2 - 5);
      break;
    }

    ter(SEEX, SEEY) = t_console;
    tmpcomp = add_computer(SEEX, SEEY, "NEPowerOS", 0);
    tmpcomp->add_option("Read Logs", COMPACT_AMIGARA_LOG, 0);
    tmpcomp->add_option("Initiate Tremors", COMPACT_AMIGARA_START, 4);
    tmpcomp->add_failure(COMPFAIL_AMIGARA);
   } break;
  }
  } break;

 case ot_spiral_hub:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_rock_floor;
  }
  line(this, t_rock, 23,  0, 23, 23);
  line(this, t_rock,  2, 23, 23, 23);
  line(this, t_rock,  2,  4,  2, 23);
  line(this, t_rock,  2,  4, 18,  4);
  line(this, t_rock, 18,  4, 18, 18); // bad
  line(this, t_rock,  6, 18, 18, 18);
  line(this, t_rock,  6,  7,  6, 18);
  line(this, t_rock,  6,  7, 15,  7);
  line(this, t_rock, 15,  7, 15, 15);
  line(this, t_rock,  8, 15, 15, 15);
  line(this, t_rock,  8,  9,  8, 15);
  line(this, t_rock,  8,  9, 13,  9);
  line(this, t_rock, 13,  9, 13, 13);
  line(this, t_rock, 10, 13, 13, 13);
  line(this, t_rock, 10, 11, 10, 13);
  square(this, t_slope_up, 11, 11, 12, 12);
  rotate(rng(0, 3));
  break;

 case ot_spiral: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_rock_floor;
  }
  int num_spiral = rng(1, 4);
  for (int i = 0; i < num_spiral; i++) {
   int orx = rng(SEEX - 4, SEEX), ory = rng(SEEY - 4, SEEY);
   line(this, t_rock, orx    , ory    , orx + 5, ory    );
   line(this, t_rock, orx + 5, ory    , orx + 5, ory + 5);
   line(this, t_rock, orx + 1, ory + 5, orx + 5, ory + 5);
   line(this, t_rock, orx + 1, ory + 2, orx + 1, ory + 4);
   line(this, t_rock, orx + 1, ory + 2, orx + 3, ory + 2);
   ter(orx + 3, ory + 3) = t_rock;
   ter(orx + 2, ory + 3) = t_rock_floor;
   place_items(mi_spiral, 60, orx + 2, ory + 3, orx + 2, ory + 3, false, 0);
  }
 } break;

 case ot_radio_tower:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  lw = rng(1, SEEX * 2 - 2);
  tw = rng(1, SEEY * 2 - 2);
  for (int i = lw; i < lw + 4; i++) {
   for (int j = tw; j < tw + 4; j++)
    ter(i, j) = t_radio_tower;
  }
  rw = -1;
  bw = -1;
  if (lw <= 4)
   rw = rng(lw + 5, 10);
  else if (lw >= 16)
   rw = rng(3, lw - 13);
  if (tw <= 3)
   bw = rng(tw + 5, 10);
  else if (tw >= 16)
   bw = rng(3, tw - 7);
  if (rw != -1 && bw != -1) {
   for (int i = rw; i < rw + 12; i++) {
    for (int j = bw; j < bw + 6; j++) {
     if (j == bw || j == bw + 5)
      ter(i, j) = t_wall_h;
     else if (i == rw || i == rw + 11)
      ter(i, j) = t_wall_v;
     else if (j == bw + 1)
      ter(i, j) = t_counter;
     else
      ter(i, j) = t_floor;
    }
   }
   cw = rng(rw + 2, rw + 8);
   ter(cw, bw + 5) = t_window;
   ter(cw + 1, bw + 5) = t_window;
   ter(rng(rw + 2, rw + 8), bw + 5) = t_door_c;
   ter(rng(rw + 2, rw + 8), bw + 1) = t_radio_controls;
   place_items(mi_radio, 60, rw + 1, bw + 2, rw + 10, bw + 4, true, 0);
  } else	// No control room... simple controls near the tower
   ter(rng(lw, lw + 3), tw + 4) = t_radio_controls;
  break;

 case ot_toxic_dump: {
  square(this, t_dirt, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1);
  for (int n = 0; n < 6; n++) {
   int poolx = rng(4, SEEX * 2 - 5), pooly = rng(4, SEEY * 2 - 5);
   for (int i = poolx - 3; i <= poolx + 3; i++) {
    for (int j = pooly - 3; j <= pooly + 3; j++) {
     if (rng(2, 5) > rl_dist(poolx, pooly, i, j)) {
      ter(i, j) = t_sewage;
      radiation(i, j) += rng(20, 60);
     }
    }
   }
  }
  const point build(rng(6, SEEX * 2 - 7), rng(6, SEEY * 2 - 7));
  const point nw_in = build + 3 * Direction::NW;
  const point nw_out = build + 4 * Direction::NW;
  const point ne_out = build + 4 * Direction::NE;
  const point se_out = build + 4 * Direction::SE;
  const point sw_in = build + 3 * Direction::SW;
  const point sw_out = build + 4 * Direction::SW;
  square(this, t_floor, nw_in, build+3*Direction::SE);
  line(this, t_wall_h, nw_out, sw_out);
  line(this, t_wall_h, ne_out, se_out);
  line(this, t_wall_v, nw_out, ne_out);
  line(this, t_wall_v, sw_out, se_out);
  _stock_line(*this, t_counter, mi_toxic_dump_equipment, 80, nw_in, sw_in);
  add_item(build, item::types[itm_id_military], 0);
  ter(build+4*Direction::E) = t_door_locked;

  rotate(rng(0, 3));
 } break;


 case ot_cave:
  if (t_above == ot_cave) { // We're underground!
   for (int i = 0; i < SEEX * 2; i++) {
    for (int j = 0; j < SEEY * 2; j++) {
     if (rng(0, 6) < i || SEEX * 2 - rng(1, 7) > i ||
         rng(0, 6) < j || SEEY * 2 - rng(1, 7) > j   )
      ter(i, j) = t_rock_floor;
     else
      ter(i, j) = t_rock;
    }
   }
   square(this, t_slope_up, SEEX - 1, SEEY - 1, SEEX, SEEY);

   switch (rng(1, 3)) { // What type of cave is it?
   case 1: // Bear cave
    add_spawn(mon_bear, 1, rng(SEEX - 6, SEEX + 5), rng(SEEY - 6, SEEY + 5));
    if (one_in(4))
     add_spawn(mon_bear, 1, rng(SEEX - 6, SEEX + 5), rng(SEEY - 6, SEEY + 5));
    place_items(mi_ant_food, 80, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
    break;
   case 2: // Wolf cave!
    do
     add_spawn(mon_wolf, 1, rng(SEEX - 6, SEEX + 5), rng(SEEY - 6, SEEY + 5));
    while (one_in(2));
    place_items(mi_ant_food, 86, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
    break;
   case 3: { // Hermit cave
	point orig(rng(SEEX - 1, SEEX), rng(SEEY - 1, SEEY));
	point herm(rng(SEEX - 6, SEEX + 5), rng(SEEX - 6, SEEY + 5));
	for(const auto& pt : line_to(orig, herm, 0)) add_field(g, pt, fd_blood, 2);
    add_item(herm, item(messages.turn));
    place_items(mi_rare, 25, herm.x - 1, herm.y - 1, herm.x + 1, herm.y + 1,true,0);
   } break;
   }
  } else { // We're above ground!
// First, draw a forest
   draw_map(ot_forest, t_north, t_east, t_south, t_west, t_above, turn, g);
// Clear the center with some rocks
   square(this, t_rock, SEEX - 6, SEEY - 6, SEEX + 5, SEEY + 5);
   point path((one_in(2) ? SEEX - 8 : SEEX + 7), rng(SEEY - 6, SEEY + 5));
   if (one_in(2)) std::swap(path.x, path.y);
   std::vector<point> pathline = line_to(path, SEEX - 1, SEEY - 1, 0);
   for (const point& pt : pathline) square(this, t_dirt, pt, pt + Direction::SE);
   while (!one_in(8))
    ter(rng(SEEX - 6, SEEX + 5), rng(SEEY - 6, SEEY + 5)) = t_dirt;
   square(this, t_slope_down, SEEX - 1, SEEY - 1, SEEX, SEEY);
  }
  break;

 case ot_cave_rat:
  square(this, t_rock, point(0), point(SEE * 2 - 1));

  if (t_above == ot_cave_rat) { // Finale
   rough_circle(this, t_rock_floor, SEEX, SEEY, 8);
   square(this, t_rock_floor, SEEX - 1, SEEY, SEEX, SEEY * 2 - 2);
   line(this, t_slope_up, SEEX - 1, SEEY * 2 - 3, SEEX, SEEY * 2 - 2);
   for (int i = SEEX - 4; i <= SEEX + 4; i++) {
    for (int j = SEEY - 4; j <= SEEY + 4; j++) {
     if ((i <= SEEX - 2 || i >= SEEX + 2) && (j <= SEEY - 2 || j >= SEEY + 2))
      add_spawn(mon_sewer_rat, 1, i, j);
    }
   }
   add_spawn(mon_rat_king, 1, SEEX, SEEY);
   place_items(mi_rare, 75, SEEX - 4, SEEY - 4, SEEX + 4, SEEY + 4, true, 0);
  } else { // Level 1
   int cavex = SEEX, cavey = SEEY * 2 - 3;
   point stairs(SEEX - 1, 1); // Default stairs location--may change
   int centerx;
   do {
    cavex += rng(-1, 1);
    cavey -= rng(0, 1);
    for (int cx = cavex - 1; cx <= cavex + 1; cx++) {
     for (int cy = cavey - 1; cy <= cavey + 1; cy++) {
      ter(cx, cy) = t_rock_floor;
      if (one_in(10)) add_field(g, cx, cy, fd_blood, rng(1, 3));
      if (one_in(20)) add_spawn(mon_sewer_rat, 1, cx, cy);
     }
    }
    if (cavey == SEEY - 1) centerx = cavex;
   } while (cavey > 2);
// Now draw some extra passages!
   do {
	point to((one_in(2) ? 2 : SEEX * 2 - 3), rng(2, SEEY * 2 - 3));
    std::vector<point> path = line_to(centerx, SEEY - 1, to, 0);
    for (int i = 0; i < path.size(); i++) {
     for (int cx = path[i].x - 1; cx <= path[i].x + 1; cx++) {
      for (int cy = path[i].y - 1; cy <= path[i].y + 1; cy++) {
       ter(cx, cy) = t_rock_floor;
       if (one_in(10)) add_field(g, cx, cy, fd_blood, rng(1, 3));
       if (one_in(20)) add_spawn(mon_sewer_rat, 1, cx, cy);
      }
     }
    }
    if (one_in(2)) stairs = to;
   } while (one_in(2));
// Finally, draw the stairs up and down.
   ter(SEEX - 1, SEEX * 2 - 2) = t_slope_up;
   ter(SEEX    , SEEX * 2 - 2) = t_slope_up;
   ter(stairs) = t_slope_down;
  }
  break;

 case ot_sub_station_north:
 case ot_sub_station_east:
 case ot_sub_station_south:
 case ot_sub_station_west:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j < 9 || j > 12 || i < 4 || i > 19)
     ter(i, j) = t_pavement;
    else if (j < 12 && j > 8 && (i == 4 || i == 19))
     ter(i, j) = t_wall_v;
    else if (i > 3 && i < 20 && j == 12)
     ter(i, j) = t_wall_h;
    else
     ter(i, j) = t_floor;
   }
  }
  ter(16, 10) = t_stairs_down;
  if (terrain_type == ot_sub_station_east) rotate(1);
  if (terrain_type == ot_sub_station_south) rotate(2);
  if (terrain_type == ot_sub_station_west) rotate(3);
  break;

 case ot_police_north:
 case ot_police_east:
 case ot_police_south:
 case ot_police_west: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((j ==  7 && i != 17 && i != 18) ||
        (j == 12 && i !=  0 && i != 17 && i != 18 && i != SEEX * 2 - 1) ||
        (j == 14 && ((i > 0 && i < 6) || i == 9 || i == 13 || i == 17)) ||
        (j == 15 && i > 17  && i < SEEX * 2 - 1) ||
        (j == 17 && i >  0  && i < 17) ||
        (j == 20))
     ter(i, j) = t_wall_h;
    else if (((i == 0 || i == SEEX * 2 - 1) && j > 7 && j < 20) ||
             ((i == 5 || i == 10 || i == 16 || i == 19) && j > 7 && j < 12) ||
             ((i == 5 || i ==  9 || i == 13) && j > 14 && j < 17) ||
             (i == 17 && j > 14 && j < 20))
     ter(i, j) = t_wall_v;
    else if (j == 14 && i > 5 && i < 17 && i % 2 == 0)
     ter(i, j) = t_bars;
    else if ((i > 1 && i < 4 && j > 8 && j < 11) ||
             (j == 17 && i > 17 && i < 21))
     ter(i, j) = t_counter;
    else if ((i == 20 && j > 7 && j < 12) || (j == 8 && i > 19 && i < 23) ||
             (j == 15 && i > 0 && i < 5))
     ter(i, j) = t_rack;
    else if (j < 7)
     ter(i, j) = t_pavement;
    else if (j > 20)
     ter(i, j) = t_sidewalk;
    else
     ter(i, j) = t_floor;
   }
  }
  ter(17, 7) = t_door_locked_alarm;
  ter(18, 7) = t_door_locked_alarm;
  ter(rng( 1,  4), 12) = t_door_c;
  ter(rng( 6,  9), 12) = t_door_c;
  ter(rng(11, 15), 12) = t_door_c;
  ter(21, 12) = t_door_metal_locked;
  tmpcomp = add_computer(22, 13, "PolCom OS v1.47", 3);
  tmpcomp->add_option("Open Supply Room", COMPACT_OPEN, 3);
  tmpcomp->add_failure(COMPFAIL_SHUTDOWN);
  tmpcomp->add_failure(COMPFAIL_ALARM);
  tmpcomp->add_failure(COMPFAIL_MANHACKS);
  ter( 7, 14) = t_door_c;
  ter(11, 14) = t_door_c;
  ter(15, 14) = t_door_c;
  ter(rng(20, 22), 15) = t_door_c;
  ter(2, 17) = t_door_metal_locked;
  tmpcomp = add_computer(22, 13, "PolCom OS v1.47", 3);
  tmpcomp->add_option("Open Evidence Locker", COMPACT_OPEN, 3);
  tmpcomp->add_failure(COMPFAIL_SHUTDOWN);
  tmpcomp->add_failure(COMPFAIL_ALARM);
  tmpcomp->add_failure(COMPFAIL_MANHACKS);
  ter(17, 18) = t_door_c;
  for (int i = 18; i < SEEX * 2 - 1; i++)
   ter(i, 20) = t_window_alarm;
  if (one_in(3)) {
   for (int j = 16; j < 20; j++)
    ter(SEEX * 2 - 1, j) = t_window_alarm;
  }
  rn = rng(18, 21);
  const ter_id door = one_in(4) ? t_door_c : t_door_locked_alarm;
  ter(rn, 20) = door;
  ter(rn + 1, 20) = door;
  rn = rng(1, 5);
  ter(rn, 20) = t_window_alarm;
  ter(rn + 1, 20) = t_window_alarm;
  rn = rng(10, 14);
  ter(rn, 20) = t_window_alarm;
  ter(rn + 1, 20) = t_window_alarm;
  if (one_in(2)) {
   for (int i = 6; i < 10; i++)
    ter(i, 8) = t_counter;
  }
  if (one_in(3)) {
   for (int j = 8; j < 12; j++)
    ter(6, j) = t_counter;
  }
  if (one_in(3)) {
   for (int j = 8; j < 12; j++)
    ter(9, j) = t_counter;
  }
  
  place_items(mi_kitchen,      40,  6,  8,  9, 11,    false, 0);
  place_items(mi_cop_weapons,  70, 20,  8, 22,  8,    false, 0);
  place_items(mi_cop_weapons,  70, 20,  8, 20, 11,    false, 0);
  place_items(mi_cop_evidence, 60,  1, 15,  4, 15,    false, 0);

  if (terrain_type == ot_police_west) rotate(1);
  if (terrain_type == ot_police_north) rotate(2);
  if (terrain_type == ot_police_east) rotate(3);
  } break;

 case ot_bank_north:
 case ot_bank_east:
 case ot_bank_south:
 case ot_bank_west: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  square(this, t_floor, 1,  1, 22, 22);
  line(this, t_wall_h,  1,  1, 22,  1);
  line(this, t_wall_h,  2,  6, 19,  6);
  line(this, t_wall_h,  2, 13, 18, 13);
  line(this, t_wall_h,  1, 22, 22, 22);
  line(this, t_wall_h,  9,  9, 18,  9);
  line(this, t_wall_v,  1,  2,  1, 21);
  line(this, t_wall_v, 22,  2, 22, 21);
  line(this, t_wall_v, 19,  9, 19, 21);
  line(this, t_wall_v, 13, 14, 13, 16);
  line(this, t_wall_v, 13, 19, 13, 21);
  line(this, t_wall_v,  8,  7,  8, 12);
  line(this, t_wall_metal_h,  3, 14, 11, 14);
  line(this, t_wall_metal_h,  3, 21, 11, 21);
  line(this, t_wall_metal_v,  2, 14,  2, 21);
  line(this, t_wall_metal_v, 12, 14, 12, 16);
  line(this, t_wall_metal_v, 12, 19, 12, 21);
  line(this, t_counter,  2,  4,  14,  4);
  ter(13, 17) = t_door_metal_locked;
  ter(13, 18) = t_door_metal_locked;
  tmpcomp = add_computer(14, 16, "First United Bank", 3);
  tmpcomp->add_option("Open Vault", COMPACT_OPEN, 3);
  tmpcomp->add_failure(COMPFAIL_SHUTDOWN);
  tmpcomp->add_failure(COMPFAIL_ALARM);
// Front wall--glass or windows?
  if (!one_in(4)) {
   line(this, t_wall_glass_h_alarm, 2, 1, 21, 1);
   if (one_in(2))
    line(this, t_wall_glass_v_alarm, 1, 2, 1, 5); // Side wall for teller room
  } else {
   if (one_in(4))
    line(this, t_wall_glass_v_alarm, 1, 2, 1, 5); // Side wall for teller room
   rn = rng(3, 7);
   ter(rn    , 1) = t_window_alarm;
   ter(rn + 1, 1) = t_window_alarm;
   rn = rng(13, 18);
   ter(rn    , 1) = t_window_alarm;
   ter(rn + 1, 1) = t_window_alarm;
  }
// Doors for offices
  ter(8, rng(7, 8)) = t_door_c;
  ter(rng(10, 17), 9) = t_door_c;
  ter(19, rng(15, 20)) = t_door_c;
// Side and back windows
  ter(1, rng(7, 12)) = t_window_alarm;
  ter(1, rng(7, 12)) = t_window_alarm;
  ter(rng(14, 18), 22) = t_window_alarm;
  if (one_in(2))
   ter(rng(14, 18), 22) = t_window_alarm;
  if (one_in(10))
   line(this, t_wall_glass_v, 22, 2, 22, 21); // Right side is glass wall!
  else {
   rn = rng(7, 12);
   ter(22, rn    ) = t_window_alarm;
   ter(22, rn + 1) = t_window_alarm;
   rn = rng(13, 19);
   ter(22, rn    ) = t_window_alarm;
   ter(22, rn + 1) = t_window_alarm;
  }
// Finally, place the front doors.
  const ter_id door = one_in(4) ? t_door_c : (one_in(4) ? t_door_locked : t_door_locked_alarm);
  ter(10, 1) = door;
  ter(11, 1) = door;

  place_items(mi_office,       60,  2,  7,  7, 12,    false, 0);
  place_items(mi_office,       60,  9, 10, 18, 12,    false, 0);
  place_items(mi_office,       70, 14, 14, 18, 21,    false, 0);
  place_items(mi_vault,        45,  3, 15, 11, 20,    false, 0);

  if (terrain_type == ot_bank_east) rotate(1);
  if (terrain_type == ot_bank_south) rotate(2);
  if (terrain_type == ot_bank_west) rotate(3);
  } break;

 case ot_bar_north:
 case ot_bar_east:
 case ot_bar_south:
 case ot_bar_west: {
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_pavement;
  }

  square(this, t_floor, 2, 2, 21, 15);
  square(this, t_floor, 18, 17, 21, 18);
  // Main walls
  line(this, t_wall_h, 2, 1, 21, 1);
  line(this, t_wall_h, 2, 16, 21, 16);
  line(this, t_wall_h, 18, 19, 21, 19);
  line(this, t_wall_v, 1, 1, 1, 16);
  line(this, t_wall_v, 22, 1, 22, 19);
  line(this, t_wall_v, 17, 18, 17, 19);
  // Main bar counter
  line(this, t_counter, 19, 3, 19, 10);
  line(this, t_counter, 20, 3, 21, 3);
  ter(20,10) = t_counter;
   // Back room counter
  line(this, t_counter, 18, 18, 21, 18);
  // Tables
  square(this, t_table, 4, 3, 5, 4);
  square(this, t_table, 9, 3, 10, 4);
  square(this, t_table, 14, 3, 15, 4);
  square(this, t_table, 4, 8, 5, 9);
  square(this, t_table, 9, 8, 10, 9);
  square(this, t_table, 14, 8, 15, 9);
  // Pool tables
  square(this, t_pool_table,      4, 13,  8, 14);
  place_items(mi_pool_table, 50,  4, 13,  8, 14, false, 0);
  square(this, t_pool_table,     13, 13, 17, 14);
  place_items(mi_pool_table, 50, 13, 13, 17, 14, false, 0);
  // 1 in 4 chance to have glass walls in front
  if (one_in(4)) {
   line(this, t_wall_glass_h, 3, 1, 5, 1);
   line(this, t_wall_glass_h, 7, 1, 9, 1);
   line(this, t_wall_glass_h, 14, 1, 16, 1);
   line(this, t_wall_glass_h, 18, 1, 20, 1);
   line(this, t_wall_glass_v, 1, 3, 1, 5);
   line(this, t_wall_glass_v, 1, 7, 1, 9);
   line(this, t_wall_glass_v, 1, 11, 1, 13);
  } else {
   ter(3,1) = t_window;
   ter(5,1) = t_window;
   ter(7,1) = t_window;
   ter(16,1) = t_window;
   ter(18,1) = t_window;
   ter(20,1) = t_window;
   ter(1,6) = t_window;
   ter(1,11) = t_window;
  }
  // Fridges and closets
  ter(21,4) = t_fridge;
  line(this, t_rack, 21, 5, 21, 8);
  ter(21,17) = t_fridge; // Back room fridge
  // Door placement
  ter(11,1) = t_door_c;
  ter(12,1) = t_door_c;
  ter(20, 16) = t_door_locked;
  ter(17, 17) = t_door_locked;

  // Item placement
  place_items(mi_snacks, 30, 19, 3, 19, 10, false, 0);
  place_items(mi_snacks, 50, 18, 18, 21, 18, false, 0);
  place_items(mi_fridgesnacks, 60, 21, 4, 21, 4, false, 0);
  place_items(mi_fridgesnacks, 60, 21, 17, 21, 17, false, 0);
  place_items(mi_alcohol, 70, 21, 5, 21, 8, false, 0);
  place_items(mi_trash, 15, 2, 17, 16, 19, true, 0);

  if (terrain_type == ot_bar_east) rotate(1);
  if (terrain_type == ot_bar_south) rotate(2);
  if (terrain_type == ot_bar_west) rotate(3);
 } break;

 case ot_pawn_north:
 case ot_pawn_east:
 case ot_pawn_south:
 case ot_pawn_west:
// Init to plain grass/dirt
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }

  tw = rng(0, 10);
  bw = SEEY * 2 - rng(1, 2) - rng(0, 1) * rng(0, 1);
  lw = rng(0, 4);
  rw = SEEX * 2 - rng(1, 5);
  if (tw >= 6) { // Big enough for its own parking lot
   square(this, t_pavement, 0, 0, SEEX * 2 - 1, tw - 1);
   for (int i = rng(0, 1); i < SEEX * 2; i += 4)
    line(this, t_pavement_y, i, 1, i, tw - 1);
  }
// Floor and walls
  square(this, t_floor, lw, tw, rw, bw);
  line(this, t_wall_h, lw, tw, rw, tw);
  line(this, t_wall_h, lw, bw, rw, bw);
  line(this, t_wall_v, lw, tw + 1, lw, bw - 1);
  line(this, t_wall_v, rw, tw + 1, rw, bw - 1);
// Doors and windows--almost certainly alarmed
  if (one_in(15)) {
   line(this, t_window, lw + 2, tw, lw + 5, tw);
   line(this, t_window, rw - 5, tw, rw - 2, tw);
   line(this, t_door_locked, SEEX, tw, SEEX + 1, tw);
  } else {
   line(this, t_window_alarm, lw + 2, tw, lw + 5, tw);
   line(this, t_window_alarm, rw - 5, tw, rw - 2, tw);
   line(this, t_door_locked_alarm, SEEX, tw, SEEX + 1, tw);
  }
// Some display racks by the left and right walls
  _stock_line(*this, t_rack, mi_pawn, 86, point(lw + 1, tw + 1), point(lw + 1, bw - 1));
  _stock_line(*this, t_rack, mi_pawn, 86, point(rw - 1, tw + 1), point(rw - 1, bw - 1));
// Some display counters
  _stock_line(*this, t_counter, mi_pawn, 80, point(lw + 4, tw + 2), point(lw + 4, bw - 3));
  _stock_line(*this, t_counter, mi_pawn, 80, point(rw - 4, tw + 2), point(rw - 4, bw - 3));
// More display counters, if there's room for them
  if (rw - lw >= 18 && one_in(rw - lw - 17)) {
   for (int j = tw + rng(3, 5); j <= bw - 3; j += 3) {
    _stock_line(*this, t_counter, mi_pawn, 75, point(lw + 6, j), point(rw - 6, j));
   }
  }
// Finally, place an office sometimes
  if (!one_in(5)) {
   static const constexpr std::pair<items_location, int> pawn_office_stock[] = {
       std::pair(mi_office, 70),
       std::pair(mi_homeguns, 50),
       std::pair(mi_harddrugs, 20)
   };

   if (one_in(2)) { // Office on the left side
    int office_top = bw - rng(3, 5), office_right = lw + rng(4, 7);
// Clear out any items in that area!  And reset to floor.
    for (int i = lw + 1; i <= office_right; i++) {
     for (int j = office_top; j <= bw - 1; j++) {
      i_clear(i, j);
      ter(i, j) = t_floor;
     }
    }
    line(this, t_wall_h, lw + 1, office_top, office_right, office_top);
    line(this, t_wall_v, office_right, office_top + 1, office_right, bw - 1);
    ter(office_right, rng(office_top + 1, bw - 1)) = t_door_locked;
    if (one_in(4)) // Back door
     ter(rng(lw + 1, office_right - 1), bw) = t_door_locked_alarm;
// Finally, add some stuff in there
    _place_items(*this, std::begin(pawn_office_stock), std::end(pawn_office_stock), point(lw + 1, office_top + 1), point(office_right - 1, bw - 1));
   } else { // Office on the right side
    int office_top = bw - rng(3, 5), office_left = rw - rng(4, 7);
    for (int i = office_left; i <= rw - 1; i++) {
     for (int j = office_top; j <= bw - 1; j++) {
      i_clear(i, j);
      ter(i, j) = t_floor;
     }
    }
    line(this, t_wall_h, office_left, office_top, rw - 1, office_top);
    line(this, t_wall_v, office_left, office_top + 1, office_left, bw - 1);
    ter(office_left, rng(office_top + 1, bw - 1)) = t_door_locked;
    if (one_in(4)) // Back door
     ter(rng(office_left + 1, rw - 1), bw) = t_door_locked_alarm;
    // Finally, add some stuff in there
    _place_items(*this, std::begin(pawn_office_stock), std::end(pawn_office_stock), point(office_left + 1, office_top + 1), point(rw - 1, bw - 1));
   }
  }
  if (terrain_type == ot_pawn_east) rotate(1);
  if (terrain_type == ot_pawn_south) rotate(2);
  if (terrain_type == ot_pawn_west) rotate(3);
  break;

 case ot_mil_surplus_north:
 case ot_mil_surplus_east:
 case ot_mil_surplus_south:
 case ot_mil_surplus_west:
// Init to plain grass/dirt
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = grass_or_dirt();
  }
  lw = rng(0, 2);
  rw = SEEX * 2 - rng(1, 3);
  tw = rng(0, 4);
  bw = SEEY * 2 - rng(3, 8);
  square(this, t_floor, lw, tw, rw, bw);
  line(this, t_wall_h, lw, tw, rw, tw);
  line(this, t_wall_h, lw, bw, rw, bw);
  line(this, t_wall_v, lw, tw + 1, lw, bw - 1);
  line(this, t_wall_v, rw, tw + 1, rw, bw - 1);
  rn = rng(4, 7);
  line(this, t_window, lw + 2, tw, lw + rn, tw);
  line(this, t_window, rw - rn, tw, rw - 2, tw);
  line(this, t_door_c, SEEX, tw, SEEX + 1, tw);
  if (one_in(2)) // counter on left
   line(this, t_counter, lw + 2, tw + 1, lw + 2, tw + rng(3, 4));
  else // counter on right
   line(this, t_counter, rw - 2, tw + 1, rw - 2, tw + rng(3, 4));
  for (int i = lw + 1; i <= SEEX; i += 2) {
   _stock_line(*this, t_rack, one_in(3) ? mi_mil_armor : (one_in(3) ? mi_mil_surplus : mi_mil_food_nodrugs), 70, point(i, tw + 5), point(i, bw - 2));
  }
  for (int i = rw - 1; i >= SEEX + 1; i -= 2) {
   _stock_line(*this, t_rack, one_in(3) ? mi_mil_armor : (one_in(3) ? mi_mil_surplus : mi_mil_food_nodrugs), 70, point(i, tw + 5), point(i, bw - 2));
  }
  if (terrain_type == ot_mil_surplus_east) rotate(1);
  if (terrain_type == ot_mil_surplus_south) rotate(2);
  if (terrain_type == ot_mil_surplus_west) rotate(3);
  break;

 case ot_megastore_entrance: {
  square(this, t_floor, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1);
// Construct facing north; below, we'll rotate to face road
  line(this, t_wall_glass_h, 0, 0, SEEX * 2 - 1, 0);
  ter(SEEX, 0) = t_door_glass_c;
  ter(SEEX + 1, 0) = t_door_glass_c;
// Long checkout lanes
  for (int x = 2; x <= 18; x += 4) {
   line(this, t_counter, x, 4, x, 14);
   line(this, t_rack, x + 3, 4, x + 3, 14);
   place_items(mi_snacks,    80, x + 3, 4, x + 3, 14, false, 0);
   place_items(mi_magazines, 70, x + 3, 4, x + 3, 14, false, 0);
  }
  for (int i = 0; i < 10; i++) {
   int x = rng(0, SEEX * 2 - 1), y = rng(0, SEEY * 2 - 1);
   if (ter(x, y) == t_floor)
    add_spawn(mon_zombie, 1, x, y);
  }
// Finally, figure out where the road is; contruct our entrance facing that.
  if (t_east >= ot_road_null && t_east <= ot_bridge_ew) rotate(1);
  else if (t_south >= ot_road_null && t_south <= ot_bridge_ew) rotate(2);
  else if (t_west >= ot_road_null && t_west <= ot_bridge_ew) rotate(3);
 } break;

 case ot_megastore:
  square(this, t_floor, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1);

// Randomly pick contents
// \todo would be nice if we could guarantee that a megastore had all five sections in 8 overmap blocks
  switch (rng(1, 5)) {
  case 1: { // Groceries
   bool fridge = false;
   for (int x = rng(2, 3); x < SEEX * 2 - 1; x += 3) {
    for (int y = 2; y <= SEEY; y += SEEY - 2) {
     if (one_in(3))
      fridge = !fridge;
     if (fridge) {
      line(this, t_fridge, x, y, x, y + SEEY - 4);
      if (one_in(3))
       place_items(mi_fridgesnacks, 80, x, y, x, y + SEEY - 4, false, 0);
      else
       place_items(mi_fridge,       70, x, y, x, y + SEEY - 4, false, 0);
     } else {
      line(this, t_rack, x, y, x, y + SEEY - 4);
      if (one_in(3))
       place_items(mi_cannedfood, 78, x, y, x, y + SEEY - 4, false, 0);
      else if (one_in(2))
       place_items(mi_pasta,      82, x, y, x, y + SEEY - 4, false, 0);
      else if (one_in(2))
       place_items(mi_produce,    65, x, y, x, y + SEEY - 4, false, 0);
      else
       place_items(mi_snacks,     72, x, y, x, y + SEEY - 4, false, 0);
     }
    }
   }
  } break;
  case 2: // Hardware
   for (int x = 2; x <= 22; x += 4) {
    line(this, t_rack, x, 4, x, SEEY * 2 - 5);
    if (one_in(3))
     place_items(mi_tools,    70, x, 4, x, SEEY * 2 - 5, false, 0);
    else if (one_in(2))
     place_items(mi_bigtools, 70, x, 4, x, SEEY * 2 - 5, false, 0);
    else if (one_in(3))
     place_items(mi_hardware, 70, x, 4, x, SEEY * 2 - 5, false, 0);
    else
     place_items(mi_mischw,   70, x, 4, x, SEEY * 2 - 5, false, 0);
   }
   break;
  case 3: // Clothing
   for (int x = 2; x < SEEX * 2; x += 6) {
    for (int y = 3; y <= 9; y += 6) {
     square(this, t_rack, x, y, x + 1, y + 1);
     if (one_in(2))
      place_items(mi_shirts,  75, x, y, x + 1, y + 1, false, 0);
     else if (one_in(2))
      place_items(mi_pants,   72, x, y, x + 1, y + 1, false, 0);
     else if (one_in(2))
      place_items(mi_jackets, 65, x, y, x + 1, y + 1, false, 0);
     else
      place_items(mi_winter,  62, x, y, x + 1, y + 1, false, 0);
    }
   }
   for (int y = 13; y <= SEEY * 2 - 2; y += 3) {
    line(this, t_rack, 2, y, SEEX * 2 - 3, y);
    if (one_in(3))
     place_items(mi_shirts,     75, 2, y, SEEX * 2 - 3, y, false, 0);
    else if (one_in(2))
     place_items(mi_shoes,      75, 2, y, SEEX * 2 - 3, y, false, 0);
    else if (one_in(2))
     place_items(mi_bags,       75, 2, y, SEEX * 2 - 3, y, false, 0);
    else
     place_items(mi_allclothes, 75, 2, y, SEEX * 2 - 3, y, false, 0);
   }
   break;
  case 4: // Cleaning and soft drugs and novels and junk
   for (int x = rng(2, 3); x < SEEX * 2 - 1; x += 3) {
    for (int y = 2; y <= SEEY; y += SEEY - 2) {
     line(this, t_rack, x, y, x, y + SEEY - 4);
     if (one_in(3))
      place_items(mi_cleaning,  78, x, y, x, y + SEEY - 4, false, 0);
     else if (one_in(2))
      place_items(mi_softdrugs, 72, x, y, x, y + SEEY - 4, false, 0);
     else
      place_items(mi_novels,    84, x, y, x, y + SEEY - 4, false, 0);
    }
   }
   break;
  case 5: // Sporting goods
   for (int x = rng(2, 3); x < SEEX * 2 - 1; x += 3) {
    for (int y = 2; y <= SEEY; y += SEEY - 2) {
     line(this, t_rack, x, y, x, y + SEEY - 4);
     if (one_in(2))
      place_items(mi_sports,  72, x, y, x, y + SEEY - 4, false, 0);
     else if (one_in(10))
      place_items(mi_rifles,  20, x, y, x, y + SEEY - 4, false, 0);
     else
      place_items(mi_camping, 68, x, y, x, y + SEEY - 4, false, 0);
    }
   }
   break;
  }

// Add some spawns
  for (int i = 0; i < 15; i++) {
   int x = rng(0, SEEX * 2 - 1), y = rng(0, SEEY * 2 - 1);
   if (ter(x, y) == t_floor)
    add_spawn(mon_zombie, 1, x, y);
  }
// Rotate randomly...
  rotate(rng(0, 3));
// ... then place walls as needed.
  if (!any<ot_megastore_entrance, ot_megastore>(t_north)) line(this, t_wall_h, 0, 0, SEEX * 2 - 1, 0);
  if (!any<ot_megastore_entrance, ot_megastore>(t_east)) line(this, t_wall_v, SEEX * 2 - 1, 0, SEEX * 2 - 1, SEEY * 2 - 1);
  if (!any<ot_megastore_entrance, ot_megastore>(t_south)) line(this, t_wall_h, 0, SEEY * 2 - 1, SEEX * 2 - 1, SEEY * 2 - 1);
  if (!any<ot_megastore_entrance, ot_megastore>(t_west)) line(this, t_wall_v, 0, 0, 0, SEEY * 2 - 1);
  break;

 case ot_hospital_entrance:
  square(this, t_pavement, 0, 0, SEEX * 2 - 1, 5);
  square(this, t_floor, 0, 6, SEEX * 2 - 1, SEEY * 2 - 1);
// Ambulance parking place
  line(this, t_pavement_y,  5, 1, 22, 1);
  line(this, t_pavement_y,  5, 2,  5, 5);
  line(this, t_pavement_y, 22, 2, 22, 5);
// Top wall
  line(this, t_wall_h, 0, 6, 6, 6);
  line(this, t_door_glass_c, 3, 6, 4, 6);
  line(this, t_wall_glass_h, 7, 6, 19, 6);
  line(this, t_wall_h, 20, 6, SEEX * 2 - 1, 6);
// Left wall
  line(this, t_wall_v, 0, 0, 0, SEEY * 2 - 1);
  line(this, t_floor, 0, 11, 0, 12);
// Waiting area
  line(this, t_bench,  8, 7, 11,  7);
  line(this, t_bench, 13, 7, 17,  7);
  line(this, t_bench, 20, 7, 22,  7);
  _stock_line(*this, t_bench, mi_magazines, 70, point(8, 7), point(22, 10));
// Reception and examination rooms
  line(this, t_counter, 8, 13, 9, 13);
  line(this, t_wall_h, 10, 13, SEEX * 2 - 1, 13);
  line(this, t_door_c, 15, 13, 16, 13);
  line(this, t_wall_h,  8, 17, 13, 17);
  line(this, t_wall_h, 18, 17, 22, 17);
  line(this, t_wall_h,  8, 20, 13, 20);
  line(this, t_wall_h, 18, 20, 22, 20);
  line(this, t_wall_v,  7, 13,  7, 22);
  line(this, t_wall_v, 14, 15, 14, 20);
  line(this, t_wall_v, 17, 14, 17, 22);
  line(this, t_bed,  8, 19,  9, 19);
  line(this, t_bed, 21, 19, 22, 19);
  line(this, t_bed, 21, 22, 22, 22);
  _stock_line(*this, t_rack, mi_harddrugs, 80, point(18, 14), point(22, 14));
  _stock_line(*this, t_rack, mi_softdrugs, 70, point(8, 21), point(8, 22));
  ter(14, rng(18, 19)) = t_door_c;
  ter(17, rng(15, 16)) = t_door_locked; // Hard drugs room is locked
  ter(17, rng(18, 19)) = t_door_c;
  ter(17, rng(21, 22)) = t_door_c;
// ER and bottom wall
  line(this, t_wall_h, 0, 16, 6, 16);
  line(this, t_door_c, 3, 16, 4, 16);
  square(this, t_bed, 3, 19, 4, 20);
  _stock_line(*this, t_counter, mi_surgery, 78, point(1, 22), point(6, 22));
  line(this, t_wall_h, 1, 23, 22, 23);
  line(this, t_floor, 11, 23, 12, 23);
// Right side wall (needed sometimes!)
  line(this, t_wall_v, 23,  0, 23, 10);
  line(this, t_wall_v, 23, 13, 23, 23);

// Generate bodies / zombies
  rn = rng(10, 15);
  for (int i = 0; i < rn; i++) {
   item body(messages.turn);
   int zx = rng(0, SEEX * 2 - 1), zy = rng(0, SEEY * 2 - 1);
   if (ter(zx, zy) == t_bed || one_in(3))
    add_item(zx, zy, body);
   else if (move_cost(zx, zy) > 0) {
    mon_id zom = mon_zombie;
    if (one_in(6)) zom = mon_zombie_spitter;
    else if (!one_in(3)) zom = mon_boomer;
    add_spawn(zom, 1, zx, zy);
   }
  }

// Rotate to face the road
  if (t_east >= ot_road_null && t_east <= ot_bridge_ew) rotate(1);
  else if (t_south >= ot_road_null && t_south <= ot_bridge_ew) rotate(2);
  else if (t_west >= ot_road_null && t_west <= ot_bridge_ew) rotate(3);
  break;

 case ot_hospital:
  square(this, t_floor, 0, 0, 23, 23);
// We always have walls on the left and bottom
  line(this, t_wall_v, 0,  0,  0, 22);
  line(this, t_wall_h, 0, 23, 23, 23);
// These walls contain doors if they lead to more hospital
  if (any<ot_hospital_entrance, ot_hospital>(t_west)) line(this, t_door_c, 0, 11, 0, 12);
  if (any<ot_hospital_entrance, ot_hospital>(t_south)) line(this, t_door_c, 11, 23, 12, 23);

  if ((t_north == ot_hospital_entrance || t_north == ot_hospital) &&
      (t_east  == ot_hospital_entrance || t_east  == ot_hospital) &&
      (t_south == ot_hospital_entrance || t_south == ot_hospital) &&
      (t_west  == ot_hospital_entrance || t_west  == ot_hospital)   ) {
// We're in the center; center is always blood lab
// Large lab
   line(this, t_wall_h,  1,  2, 21,  2);
   line(this, t_wall_h,  1, 10, 21, 10);
   line(this, t_wall_v, 21,  3, 21,  9);
   _stock_line(*this, t_counter, mi_hospital_lab, 70, point(2, 3), point(2, 9));
   _stock_square(*this, t_counter, mi_hospital_lab, 74, point(5, 4), point(6, 8));
   _stock_square(*this, t_counter, mi_hospital_lab, 74, point(10, 4), point(11, 8));
   _stock_square(*this, t_counter, mi_hospital_lab, 74, point(15, 4), point(16, 8));
   ter(rng(3, 18),  2) = t_door_c;
   ter(rng(3, 18), 10) = t_door_c;
   if (one_in(4)) // Door on the right side
    ter(21, rng(4, 8)) = t_door_c;
   else { // Counter on the right side
    _stock_line(*this, t_counter, mi_hospital_lab, 70, point(20, 3), point(20, 9));
   }
// Blood testing facility
   line(this, t_wall_h,  1, 13, 10, 13);
   line(this, t_wall_v, 10, 14, 10, 22);
   rn = rng(1, 3);
   if (rn == 1 || rn == 3)
    ter(rng(3, 8), 13) = t_door_c;
   if (rn == 2 || rn == 3)
    ter(10, rng(15, 21)) = t_door_c;
   _stock_line(*this, t_counter, mi_hospital_lab, 60, point(2, 14), point(2, 22));
   square(this, t_counter, 4, 17, 6, 19);
   ter(4, 18) = t_centrifuge;
   line(this, t_floor, 5, 18, 6, rng(17, 19)); // Clear path to console
   tmpcomp = add_computer(5, 18, "Centrifuge", 0);
   tmpcomp->add_option("Analyze blood", COMPACT_BLOOD_ANAL, 4);
   tmpcomp->add_failure(COMPFAIL_DESTROY_BLOOD);
// Sample storage
   line(this, t_wall_h, 13, 13, 23, 13);
   line(this, t_wall_v, 13, 14, 13, 23);
   rn = rng(1, 3);
   if (rn == 1 || rn == 3)
    ter(rng(14, 22), 13) = t_door_c;
   if (rn == 2 || rn == 3)
    ter(13, rng(14, 21)) = t_door_c;
   _stock_square(*this, t_rack, mi_hospital_samples, 68, point(16, 16), point(21, 17));
   _stock_square(*this, t_rack, mi_hospital_samples, 68, point(16, 19), point(21, 20));
   _stock_line(*this, t_rack, mi_hospital_samples, 62, point(14, 22), point(23, 22));
  } else { // We're NOT in the center; a random hospital type!
   // \todo but would be nice if all four types were represented among 7 normal hospital sections
   switch (rng(1, 4)) { // What type?
   case 1: // Dorms
// Upper left rooms
    line(this, t_wall_h, 1, 5, 9, 5);
    for (int i = 1; i <= 7; i += 3) {
     line(this, t_bed, i, 1, i, 2);
     line(this, t_wall_v, i + 2, 0, i + 2, 4);
     ter(rng(i, i + 1), 5) = t_door_c;
    }
// Upper right rooms
    line(this, t_wall_h, 14, 5, 23, 5);
    line(this, t_wall_v, 14, 0, 14, 4);
    line(this, t_bed, 15, 1, 15, 2);
    ter(rng(15, 16), 5) = t_door_c;
    line(this, t_wall_v, 17, 0, 17, 4);
    line(this, t_bed, 18, 1, 18, 2);
    ter(rng(18, 19), 5) = t_door_c;
    line(this, t_wall_v, 20, 0, 20, 4);
    line(this, t_bed, 21, 1, 21, 2);
    ter(rng(21, 22), 5) = t_door_c;
// Waiting area
    for (int i = 1; i <= 9; i += 4)
     line(this, t_bench, i, 7, i, 10);
    _stock_line(*this, t_table, mi_magazines, 50, point(3, 8), point(3, 9));
    _stock_line(*this, t_table, mi_magazines, 50, point(7, 8), point(7, 9));
// Middle right rooms
    line(this, t_wall_v, 14, 7, 14, 10);
    line(this, t_wall_h, 15, 7, 23, 7);
    line(this, t_wall_h, 15, 10, 23, 10);
    line(this, t_wall_v, 19, 8, 19, 9);
    line(this, t_bed, 18, 8, 18, 9);
    line(this, t_bed, 20, 8, 20, 9);
    if (one_in(3)) { // Doors to north
     ter(rng(15, 16), 7) = t_door_c;
     ter(rng(21, 22), 7) = t_door_c;
    } else { // Doors to south
     ter(rng(15, 16), 10) = t_door_c;
     ter(rng(21, 22), 10) = t_door_c;
    }
    line(this, t_wall_v, 14, 13, 14, 16);
    line(this, t_wall_h, 15, 13, 23, 13);
    line(this, t_wall_h, 15, 16, 23, 16);
    line(this, t_wall_v, 19, 14, 19, 15);
    line(this, t_bed, 18, 14, 18, 15);
    line(this, t_bed, 20, 14, 20, 15);
    if (one_in(3)) { // Doors to south
     ter(rng(15, 16), 16) = t_door_c;
     ter(rng(21, 22), 16) = t_door_c;
    } else { // Doors to north
     ter(rng(15, 16), 13) = t_door_c;
     ter(rng(21, 22), 13) = t_door_c;
    }
// Lower left rooms
    line(this, t_wall_v, 5, 13, 5, 22);
    line(this, t_wall_h, 1, 13, 4, 13);
    line(this, t_bed, 1, 14, 1, 15);
    line(this, t_wall_h, 1, 17, 4, 17);
    line(this, t_bed, 1, 18, 1, 19);
    line(this, t_wall_h, 1, 20, 4, 20);
    line(this, t_bed, 1, 21, 1, 22);
    ter(5, rng(14, 16)) = t_door_c;
    ter(5, rng(18, 19)) = t_door_c;
    ter(5, rng(21, 22)) = t_door_c;
    line(this, t_wall_h, 7, 13, 10, 13);
    line(this, t_wall_v, 7, 14, 7, 22);
    line(this, t_wall_v, 10, 14, 10, 22);
    line(this, t_wall_h, 8, 18, 9, 18);
    line(this, t_bed, 8, 17, 9, 17);
    line(this, t_bed, 8, 22, 9, 22);
    if (one_in(3)) { // Doors to west
     ter(7, rng(14, 16)) = t_door_c;
     ter(7, rng(19, 21)) = t_door_c;
    } else { // Doors to east
     ter(10, rng(14, 16)) = t_door_c;
     ter(10, rng(19, 21)) = t_door_c;
    }
// Lower-right rooms
    line(this, t_wall_h, 14, 18, 23, 18);
    for (int i = 14; i <= 20; i += 3) {
     line(this, t_wall_v, i, 19, i, 22);
     line(this, t_bed, i + 1, 21, i + 1, 22);
     ter(rng(i + 1, i + 2), 18) = t_door_c;
    }
    break;

   case 2: // Offices and cafeteria
// Offices to north
    line(this, t_wall_v, 10, 0, 10, 8);
    line(this, t_wall_v, 13, 0, 13, 8);
    line(this, t_wall_h,  1, 5,  9, 5);
    line(this, t_wall_h, 14, 5, 23, 5);
    line(this, t_wall_h,  1, 9, 23, 9);
    line(this, t_door_c, 11, 9, 12, 9);
    line(this, t_table,  3, 3,  7, 3);
    line(this, t_table, 16, 3, 20, 3);
    line(this, t_table,  3, 8,  7, 8);
    line(this, t_table, 16, 8, 20, 8);
    ter(10, rng(2, 3)) = t_door_c;
    ter(13, rng(2, 3)) = t_door_c;
    ter(10, rng(6, 7)) = t_door_c;
    ter(13, rng(6, 7)) = t_door_c;
    place_items(mi_office, 70,  1, 1,  9, 3, false, 0);
    place_items(mi_office, 70, 14, 1, 22, 3, false, 0);
    place_items(mi_office, 70,  1, 5,  9, 8, false, 0);
    place_items(mi_office, 70, 14, 5, 22, 8, false, 0);
// Cafeteria to south
    line(this, t_wall_h,  1, 14,  8, 14);
    line(this, t_wall_h, 15, 14, 23, 14);
    for (int i = 16; i <= 19; i += 3) {
     for (int j = 17; j <= 20; j += 3) {
      square(this, t_table, i, j, i + 1, j + 1);
      place_items(mi_snacks,  60, i, j, i + 1, j + 1, false, 0);
      place_items(mi_produce, 65, i, j, i + 1, j + 1, false, 0);
     }
    }
    for (int i = 3; i <= 6; i += 3) {
     for (int j = 17; j <= 20; j += 3) {
      square(this, t_table, i, j, i + 1, j + 1);
      place_items(mi_snacks,  60, i, j, i + 1, j + 1, false, 0);
      place_items(mi_produce, 65, i, j, i + 1, j + 1, false, 0);
     }
    }
    break;

   case 3: // Operating rooms
// First, walls and doors; divide it into four big operating rooms
    line(this, t_wall_v, 10,  0, 10,  9);
    line(this, t_door_c, 10,  4, 10,  5);
    line(this, t_wall_v, 13,  0, 13,  9);
    line(this, t_door_c, 13,  4, 13,  5);
    line(this, t_wall_v, 10, 14, 10, 22);
    line(this, t_door_c, 10, 18, 10, 19);
    line(this, t_wall_v, 13, 14, 13, 22);
    line(this, t_door_c, 13, 18, 13, 19);
// Horizontal walls/doors
    line(this, t_wall_h,  1, 10, 10, 10);
    line(this, t_door_c,  5, 10,  6, 10);
    line(this, t_wall_h, 13, 10, 23, 10);
    line(this, t_door_c, 18, 10, 19, 10);
    line(this, t_wall_h,  1, 13, 10, 13);
    line(this, t_door_c,  5, 13,  6, 13);
    line(this, t_wall_h, 13, 13, 23, 13);
    line(this, t_door_c, 18, 13, 19, 13);
// Next, the contents of each operating room
    _stock_line(*this, t_counter, mi_surgery, 70, point(1, 1), point(1, 9));
    square(this, t_bed, 5, 4, 6, 5);

    _stock_line(*this, t_counter, mi_surgery, 70, point(1, 14), point(1, 22));
    square(this, t_bed, 5, 18, 6, 19);

    _stock_line(*this, t_counter, mi_surgery, 60, point(14, 6), point(14, 9));
    _stock_line(*this, t_counter, mi_surgery, 60, point(15, 9), point(17, 9));
    square(this, t_bed, 18, 4, 19, 5);

    _stock_line(*this, t_counter, mi_surgery, 60, point(14, 14), point(14, 17));
    _stock_line(*this, t_counter, mi_surgery, 60, point(15, 14), point(17, 14));
    square(this, t_bed, 18, 18, 19, 19);

    break;

   case 4: // Storage
// Soft drug storage
    line(this, t_wall_h, 3,  2, 12,  2);
    line(this, t_wall_h, 3, 10, 12, 10);
    line(this, t_wall_v, 3,  3,  3,  9);
    ter(3, 6) = t_door_c;
    _stock_line(*this, t_rack, mi_softdrugs, 90, point(4, 3), point(11, 3));
    _stock_line(*this, t_rack, mi_softdrugs, 90, point(4, 9), point(11, 9));
    _stock_line(*this, t_rack, mi_softdrugs, 80, point(6, 5), point(10, 5));
    _stock_line(*this, t_rack, mi_softdrugs, 80, point(6, 7), point(10, 7));
// Hard drug storage
    line(this, t_wall_v, 13, 0, 13, 19);
    ter(13, 6) = t_door_locked;
    _stock_line(*this, t_rack, mi_harddrugs, 78, point(14, 0), point(14, 4));   // \todo might be accessible from outside if northmost?
    _stock_line(*this, t_rack, mi_harddrugs, 85, point(17, 0), point(17, 7));   // \todo might be accessible from outside if northmost?
    _stock_line(*this, t_rack, mi_harddrugs, 85, point(20, 0), point(20, 7));   // \todo might be accessible from outside if northmost?
    line(this, t_wall_h, 20, 10, 23, 10);
    _stock_line(*this, t_rack, mi_harddrugs, 78, point(16, 10), point(19, 10));
    _stock_line(*this, t_rack, mi_harddrugs, 78, point(16, 12), point(19, 12));
    line(this, t_wall_h, 14, 14, 19, 14);
    ter(rng(14, 15), 14) = t_door_locked;
    ter(rng(16, 18), 14) = t_bars;
    line(this, t_wall_v, 20, 11, 20, 19);
    line(this, t_wall_h, 13, 20, 20, 20);
    line(this, t_door_c, 16, 20, 17, 20);
// Laundry room
    line(this, t_wall_h, 1, 13, 10, 13);
    ter(rng(3, 8), 13) = t_door_c;
    line(this, t_wall_v, 10, 14, 10, 22);
    ter(10, rng(16, 20)) = t_door_c;
    _stock_line(*this, t_counter, mi_allclothes, 70, point(1, 14), point(1, 22));
    for (int j = 15; j <= 21; j += 3) {
     line(this, t_rack, 4, j, 7, j);
     if (one_in(2))
      place_items(mi_cleaning, 92, 4, j, 7, j, false, 0);
     else if (one_in(5))
      place_items(mi_cannedfood, 75, 4, j, 7, j, false, 0);
     else
      place_items(mi_allclothes, 50, 4, j, 7, j, false, 0);
    }
    break;
   }
    

// We have walls to the north and east if they're not hospitals
   if (!any<ot_hospital_entrance, ot_hospital>(t_east)) line(this, t_wall_v, 23, 0, 23, 23);
   if (!any<ot_hospital_entrance, ot_hospital>(t_north)) line(this, t_wall_h, 0, 0, 23, 0);
  }
// Generate bodies / zombies
  rn = rng(15, 20);
  for (int i = 0; i < rn; i++) {
   item body(messages.turn);
   int zx = rng(0, SEEX * 2 - 1), zy = rng(0, SEEY * 2 - 1);
   if (ter(zx, zy) == t_bed || one_in(3))
    add_item(zx, zy, body);
   else if (move_cost(zx, zy) > 0) {
    mon_id zom = mon_zombie;
    if (one_in(6)) zom = mon_zombie_spitter;
    else if (!one_in(3)) zom = mon_boomer;
    add_spawn(zom, 1, zx, zy);
   }
  }
  break;

 case ot_mansion_entrance: {
// Left wall
  line(this, t_wall_v,  0,  0,  0, SEEY * 2 - 2);
  line(this, t_door_c,  0, SEEY - 1, 0, SEEY);
// Front wall
  line(this, t_wall_h,  1, 10,  SEEX * 2 - 1, 10);
  line(this, t_door_locked, SEEX - 1, 10, SEEX, 10);
  int winx1 = rng(2, 4);
  int winx2 = rng(4, 6);
  line(this, t_window, winx1, 10, winx2, 10);
  line(this, t_window, SEEX * 2 - 1 - winx1, 10, SEEX * 2 - 1 - winx2, 10);
  winx1 = rng(7, 10);
  winx2 = rng(10, 11);
  line(this, t_window, winx1, 10, winx2, 10);
  line(this, t_window, SEEX * 2 - 1 - winx1, 10, SEEX * 2 - 1 - winx2, 10);
  line(this, t_door_c, SEEX - 1, 10, SEEX, 10);
// Bottom wall
  line(this, t_wall_h,  0, SEEY * 2 - 1, SEEX * 2 - 1, SEEY * 2 - 1);
  line(this, t_door_c, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);

  build_mansion_room(this, room_mansion_courtyard, 1, 0, SEEX * 2 - 1, 9);
  square(this, t_floor, 1, 11, SEEX * 2 - 1, SEEY * 2 - 2);
  build_mansion_room(this, room_mansion_entry, 1, 11, SEEX * 2 - 1, SEEY*2 - 2);
// Rotate to face the road
  if (is_between<ot_road_null, ot_bridge_ew>(t_east)) rotate(1);
  else if (is_between<ot_road_null, ot_bridge_ew>(t_south)) rotate(2);
  else if (is_between<ot_road_null, ot_bridge_ew>(t_west)) rotate(3);
 } break;

 case ot_mansion:
// Start with floors all over
  square(this, t_floor, 1, 0, SEEX * 2 - 1, SEEY * 2 - 2);
// We always have a left and bottom wall
  line(this, t_wall_v, 0, 0, 0, SEEY * 2 - 2);
  line(this, t_wall_h, 0, SEEY * 2 - 1, SEEX * 2 - 1, SEEY * 2 - 1);
// tw and rw are the boundaries of the rooms inside...
  tw = 0;
  rw = SEEX * 2 - 1;
// ...if we need outside walls, adjust tw & rw and build them
// We build windows below.
  if (!any<ot_mansion_entrance, ot_mansion>(t_north)) {
   tw = 1;
   line(this, t_wall_h, 0, 0, SEEX * 2 - 1, 0);
  }
  if (!any<ot_mansion_entrance, ot_mansion>(t_east)) {
   rw = SEEX * 2 - 2;
   line(this, t_wall_v, SEEX * 2 - 1, 0, SEEX * 2 - 1, SEEX * 2 - 1);
  }
// Now pick a random layout
  switch (rng(1, 4)) {

  case 1: // Just one. big. room.
   mansion_room(this, 1, tw, rw, SEEY * 2 - 2);
   if (any<ot_mansion_entrance, ot_mansion>(t_west))
    line(this, t_door_c, 0, SEEY - 1, 0, SEEY);
   if (any<ot_mansion_entrance, ot_mansion>(t_south))
    line(this, t_door_c, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);
   break;

  case 2: // Wide hallway, two rooms.
   if (one_in(2)) { // vertical hallway
    line(this, t_wall_v,  9,  tw,  9, SEEY * 2 - 2);
    line(this, t_wall_v, 14,  tw, 14, SEEY * 2 - 2);
    line(this, t_floor, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);
    line(this, t_door_c, 0, SEEY - 1, 0, SEEY);
    mansion_room(this, 1, tw, 8, SEEY * 2 - 2);
    mansion_room(this, 15, tw, rw, SEEY * 2 - 2);
    ter( 9, rng(tw + 2, SEEX * 2 - 4)) = t_door_c;
    ter(14, rng(tw + 2, SEEX * 2 - 4)) = t_door_c;
   } else { // horizontal hallway
    line(this, t_wall_h, 1,  9, rw,  9);
    line(this, t_wall_h, 1, 14, rw, 14);
    line(this, t_door_c, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);
    line(this, t_floor, 0, SEEY - 1, 0, SEEY);
    mansion_room(this, 1, tw, rw, 8);
    mansion_room(this, 1, 15, rw, SEEY * 2 - 2);
    ter(rng(3, rw - 2),  9) = t_door_c;
    ter(rng(3, rw - 2), 14) = t_door_c;
   }
   if (any<ot_mansion_entrance, ot_mansion>(t_west))
    line(this, t_door_c, 0, SEEY - 1, 0, SEEY);
   if (any<ot_mansion_entrance, ot_mansion>(t_south))
    line(this, t_floor, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);
   break;
   
  case 3: // Four corners rooms
   line(this, t_wall_v, 10, tw, 10,  9);
   line(this, t_wall_v, 13, tw, 13,  9);
   line(this, t_wall_v, 10, 14, 10, SEEY * 2 - 2);
   line(this, t_wall_v, 13, 14, 13, SEEY * 2 - 2);
   line(this, t_wall_h,  1, 10, 10, 10);
   line(this, t_wall_h,  1, 13, 10, 13);
   line(this, t_wall_h, 13, 10, rw, 10);
   line(this, t_wall_h, 13, 13, rw, 13);
// Doors
   if (one_in(2))
    ter(10, rng(tw + 1, 8)) = t_door_c;
   else
    ter(rng(2, 8), 10) = t_door_c;

   if (one_in(2))
    ter(13, rng(tw + 1, 8)) = t_door_c;
   else
    ter(rng(15, rw - 1), 10) = t_door_c;

   if (one_in(2))
    ter(10, rng(15, SEEY * 2 - 3)) = t_door_c;
   else
    ter(rng(2, 8), 13) = t_door_c;

   if (one_in(2))
    ter(13, rng(15, SEEY * 2 - 3)) = t_door_c;
   else
    ter(rng(15, rw - 1), 13) = t_door_c;

   mansion_room(this,  1, tw,  9,  9);
   mansion_room(this, 14, tw, rw,  9);
   mansion_room(this,  1, 14,  9, SEEY * 2 - 2);
   mansion_room(this, 14, 14, rw, SEEY * 2 - 2);
   if (any<ot_mansion_entrance, ot_mansion>(t_west))
    line(this, t_floor, 0, SEEY - 1, 0, SEEY);
   if (any<ot_mansion_entrance, ot_mansion>(t_south))
    line(this, t_floor, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);
   break;

  case 4: // One large room in lower-left
   mw = rng( 4, 10);
   cw = rng(13, 19);
   x = rng(5, 10);
   y = rng(13, 18);
   line(this, t_wall_h,  1, mw, cw, mw);
   ter( rng(x + 1, cw - 1), mw) = t_door_c;
   line(this, t_wall_v, cw, mw + 1, cw, SEEY * 2 - 2);
   ter(cw, rng(y + 2, SEEY * 2 - 3) ) = t_door_c;
   mansion_room(this, 1, mw + 1, cw - 1, SEEY * 2 - 2);
// And a couple small rooms in the UL LR corners
   line(this, t_wall_v, x, tw, x, mw - 1);
   mansion_room(this, 1, tw, x - 1, mw - 1);
   if (one_in(2))
    ter(rng(2, x - 2), mw) = t_door_c;
   else
    ter(x, rng(tw + 2, mw - 2)) = t_door_c;
   line(this, t_wall_h, cw + 1, y, rw, y);
   mansion_room(this, cw + 1, y, rw, SEEY * 2 - 2);
   if (one_in(2))
    ter(rng(cw + 2, rw - 1), y) = t_door_c;
   else
    ter(cw, rng(y + 2, SEEY * 2 - 3)) = t_door_c;

   if (any<ot_mansion_entrance, ot_mansion>(t_west))
    line(this, t_floor, 0, SEEY - 1, 0, SEEY);
   if (any<ot_mansion_entrance, ot_mansion>(t_south))
    line(this, t_floor, SEEX - 1, SEEY * 2 - 1, SEEX, SEEY * 2 - 1);
   break;
  } // switch (rng(1, 4))

// Finally, place windows on outside-facing walls if necessary
  if (!any<ot_mansion_entrance, ot_mansion>(t_west)) {
   int consecutive = 0;
   for (int i = 1; i < SEEY; i++) {
    if (move_cost(1, i) != 0 && move_cost(1, SEEY * 2 - 1 - i) != 0) {
     if (consecutive == 3)
      consecutive = 0; // No really long windows
     else {
      consecutive++;
      ter(0, i) = t_window;
      ter(0, SEEY * 2 - 1 - i) = t_window;
     }
    } else
     consecutive = 0;
   }
  }
  if (!any<ot_mansion_entrance, ot_mansion>(t_south)) {
   int consecutive = 0;
   for (int i = 1; i < SEEX; i++) {
    if (move_cost(i, SEEY * 2 - 2) != 0 &&
        move_cost(SEEX * 2 - 1 - i, SEEY * 2 - 2) != 0) {
     if (consecutive == 3)
      consecutive = 0; // No really long windows
     else {
      consecutive++;
      ter(i, SEEY * 2 - 1) = t_window;
      ter(SEEX * 2 - 1 - i, SEEY * 2 - 1) = t_window;
     }
    } else
     consecutive = 0;
   }
  }
  if (any<ot_mansion_entrance, ot_mansion>(t_east)) {
   int consecutive = 0;
   for (int i = 1; i < SEEY; i++) {
    if (move_cost(SEEX * 2 - 2, i) != 0 &&
        move_cost(SEEX * 2 - 2, SEEY * 2 - 1 - i) != 0) {
     if (consecutive == 3)
      consecutive = 0; // No really long windows
     else {
      consecutive++;
      ter(SEEX * 2 - 1, i) = t_window;
      ter(SEEX * 2 - 1, SEEY * 2 - 1 - i) = t_window;
     }
    } else
     consecutive = 0;
   }
  }

  if (any<ot_mansion_entrance, ot_mansion>(t_north)) {
   int consecutive = 0;
   for (int i = 1; i < SEEX; i++) {
    if (move_cost(i, 1) != 0 && move_cost(SEEX * 2 - 1 - i, 1) != 0) {
     if (consecutive == 3)
      consecutive = 0; // No really long windows
     else {
      consecutive++;
      ter(i, 0) = t_window;
      ter(SEEX * 2 - 1 - i, 0) = t_window;
     }
    } else
     consecutive = 0;
   }
  }
  break;

 case ot_spider_pit_under:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i >= 3 && i <= SEEX * 2 - 4 && j >= 3 && j <= SEEY * 2 - 4) ||
        one_in(4)) {
     ter(i, j) = t_rock_floor;
     if (!one_in(3)) field_at(i, j) = field(fd_web, rng(1, 3));
    } else
     ter(i, j) = t_rock;
   }
  }
  ter(rng(3, SEEX * 2 - 4), rng(3, SEEY * 2 - 4)) = t_slope_up;
  place_items(mi_spider, 85, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, false, 0);
  add_spawn(mon_spider_trapdoor, 1, rng(3, SEEX * 2 - 5), rng(3, SEEY * 2 - 4));
  break;

 case ot_anthill:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < 8 || j < 8 || i > SEEX * 2 - 9 || j > SEEY * 2 - 9)
     ter(i, j) = grass_or_dirt();
    else if ((i == 11 || i == 12) && (j == 11 || j == 12))
     ter(i, j) = t_slope_down;
    else
     ter(i, j) = t_dirtmound;
   }
  }
  break;

 case ot_rock:
  n_fac = any<ot_slimepit, ot_slimepit_down, ot_cavern>(t_north) ? 6 : 0;
  e_fac = any<ot_slimepit, ot_slimepit_down, ot_cavern>(t_east) ? 6 : 0;
  s_fac = any<ot_slimepit, ot_slimepit_down, ot_cavern>(t_south) ? 6 : 0;
  w_fac = any<ot_slimepit, ot_slimepit_down, ot_cavern>(t_west) ? 6 : 0;

  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (rng(0, n_fac) > j || rng(0, e_fac) > SEEX * 2 - 1 - i ||
        rng(0, w_fac) > i || rng(0, s_fac) > SEEY * 2 - 1 - j   )
     ter(i, j) = t_rock_floor;
    else
     ter(i, j) = t_rock;
   }
  }
  break;

 case ot_rift:
  if (!any<ot_rift, ot_hellmouth>(t_north)) {
   if (connects_to(t_north, 2))
    n_fac = rng(-6, -2);
   else
    n_fac = rng(2, 6);
  }
  if (!any<ot_rift, ot_hellmouth>(t_east)) {
   if (connects_to(t_east, 3))
    e_fac = rng(-6, -2);
   else
    e_fac = rng(2, 6);
  }
  if (!any<ot_rift, ot_hellmouth>(t_south)) {
   if (connects_to(t_south, 0))
    s_fac = rng(-6, -2);
   else
    s_fac = rng(2, 6);
  }
  if (!any<ot_rift, ot_hellmouth>(t_west)) {
   if (connects_to(t_west, 1))
    w_fac = rng(-6, -2);
   else
    w_fac = rng(2, 6);
  }
// Negative *_fac values indicate rock floor connection, otherwise solid rock
// Of course, if we connect to a rift, *_fac = 0, and thus lava extends all the
//  way.
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((n_fac < 0 && j < n_fac * -1) || (s_fac < 0 && j >= SEEY * 2 - s_fac) ||
        (w_fac < 0 && i < w_fac * -1) || (e_fac < 0 && i >= SEEX * 2 - e_fac)  )
     ter(i, j) = t_rock_floor;
    else if (j < n_fac || j >= SEEY * 2 - s_fac ||
             i < w_fac || i >= SEEX * 2 - e_fac   )
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_lava;
   }
  }
  break;

 case ot_hellmouth:
  if (!any<ot_rift, ot_hellmouth>(t_north)) n_fac = 6;
  if (!any<ot_rift, ot_hellmouth>(t_east)) e_fac = 6;
  if (!any<ot_rift, ot_hellmouth>(t_south)) s_fac = 6;
  if (!any<ot_rift, ot_hellmouth>(t_west)) w_fac = 6;

  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j < n_fac || j >= SEEY*2 - s_fac || i < w_fac || i >= SEEX*2 - e_fac ||
        (i >= 6 && i < SEEX * 2 - 6 && j >= 6 && j < SEEY * 2 - 6))
     ter(i, j) = t_rock_floor;
    else
     ter(i, j) = t_lava;
    if (i >= SEEX - 1 && i <= SEEX && j >= SEEY - 1 && j <= SEEY)
     ter(i, j) = t_slope_down;
   }
  }
  switch (rng(0, 4)) {	// Randomly chosen "altar" design
  case 0:
   for (int i = 7; i <= 16; i += 3) {
    ter(i, 6) = t_rock;
    ter(i, 17) = t_rock;
    ter(6, i) = t_rock;
    ter(17, i) = t_rock;
    if (i > 7 && i < 16) {
     ter(i, 10) = t_rock;
     ter(i, 13) = t_rock;
    } else {
     ter(i - 1, 6 ) = t_rock;
     ter(i - 1, 10) = t_rock;
     ter(i - 1, 13) = t_rock;
     ter(i - 1, 17) = t_rock;
    }
   }
   break;
  case 1:
   for (int i = 6; i < 11; i++) {
    ter(i, i) = t_lava;
    ter(SEEX * 2 - 1 - i, i) = t_lava;
    ter(i, SEEY * 2 - 1 - i) = t_lava;
    ter(SEEX * 2 - 1 - i, SEEY * 2 - 1 - i) = t_lava;
    if (i < 10) {
     ter(i + 1, i) = t_lava;
     ter(SEEX * 2 - i, i) = t_lava;
     ter(i + 1, SEEY * 2 - 1 - i) = t_lava;
     ter(SEEX * 2 - i, SEEY * 2 - 1 - i) = t_lava;

     ter(i, i + 1) = t_lava;
     ter(SEEX * 2 - 1 - i, i + 1) = t_lava;
     ter(i, SEEY * 2 - i) = t_lava;
     ter(SEEX * 2 - 1 - i, SEEY * 2 - i) = t_lava;
    }
    if (i < 9) {
     ter(i + 2, i) = t_rock;
     ter(SEEX * 2 - i + 1, i) = t_rock;
     ter(i + 2, SEEY * 2 - 1 - i) = t_rock;
     ter(SEEX * 2 - i + 1, SEEY * 2 - 1 - i) = t_rock;

     ter(i, i + 2) = t_rock;
     ter(SEEX * 2 - 1 - i, i + 2) = t_rock;
     ter(i, SEEY * 2 - i + 1) = t_rock;
     ter(SEEX * 2 - 1 - i, SEEY * 2 - i + 1) = t_rock;
    }
   }
   break;
  case 2:
   for (int i = 7; i < 17; i++) {
    ter(i, 6) = t_rock;
    ter(6, i) = t_rock;
    ter(i, 17) = t_rock;
    ter(17, i) = t_rock;
    if (i != 7 && i != 16 && i != 11 && i != 12) {
     ter(i, 8) = t_rock;
     ter(8, i) = t_rock;
     ter(i, 15) = t_rock;
     ter(15, i) = t_rock;
    }
    if (i == 11 || i == 12) {
     ter(i, 10) = t_rock;
     ter(10, i) = t_rock;
     ter(i, 13) = t_rock;
     ter(13, i) = t_rock;
    }
   }
   break;
  case 3:
   for (int i = 6; i < 11; i++) {
    for (int j = 6; j < 11; j++) {
     ter(i, j) = t_lava;
     ter(SEEX * 2 - 1 - i, j) = t_lava;
     ter(i, SEEY * 2 - 1 - j) = t_lava;
     ter(SEEX * 2 - 1 - i, SEEY * 2 - 1 - j) = t_lava;
    }
   }
   break;
  }
  break;

 case ot_slimepit:
 case ot_slimepit_down:
  n_fac = any<ot_slimepit, ot_slimepit_down>(t_north) ? 1 : 0;
  e_fac = any<ot_slimepit, ot_slimepit_down>(t_east) ? 1 : 0;
  s_fac = any<ot_slimepit, ot_slimepit_down>(t_south) ? 1 : 0;
  w_fac = any<ot_slimepit, ot_slimepit_down>(t_west) ? 1 : 0;
 
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (!one_in(10) && (j < n_fac * SEEX        || i < w_fac * SEEX || 
                        j > SEEY*2 - s_fac*SEEY || i > SEEX*2 - e_fac*SEEX))
     ter(i, j) = (!one_in(10) ? t_slime : t_rock_floor);
    else if (rng(0, SEEX) > abs(i - SEEX) && rng(0, SEEY) > abs(j - SEEY))
     ter(i, j) = t_slime;
    else if (t_above == ot_null)
     ter(i, j) = t_dirt;
    else
     ter(i, j) = t_rock_floor;
   }
  }

  if (terrain_type == ot_slimepit_down)
   ter(rng(3, SEEX * 2 - 4), rng(3, SEEY * 2 - 4)) = t_slope_down;

  if (t_above == ot_slimepit_down) {
   switch (rng(1, 4)) {
    case 1: ter(rng(0, 2), rng(0, 2)) = t_slope_up;
    case 2: ter(rng(0, 2), SEEY * 2 - rng(1, 3)) = t_slope_up;
    case 3: ter(SEEX * 2 - rng(1, 3), rng(0, 2)) = t_slope_up;
    case 4: ter(SEEX * 2 - rng(1, 3), SEEY * 2 - rng(1, 3)) = t_slope_up;
   }
  }

  add_spawn(mon_blob, 8, SEEX, SEEY);
  place_items(mi_sewer, 40, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  break;

 case ot_triffid_grove:
  square(this, t_dirt, 0, 0, 2 * SEE - 1, 2 * SEE - 1);
  for (int rad = 5; rad < SEEX - 2; rad += rng(2, 3)) {
   square(this, t_tree, rad, rad, 23 - rad, 23 - rad);
   square(this, t_dirt, rad + 1, rad + 1, 22 - rad, 22 - rad);
   if (one_in(2)) { // Vertical side opening
    int x = (one_in(2) ? rad : 23 - rad), y = rng(rad + 1, 22 - rad);
    ter(x, y) = t_dirt;
   } else { // Horizontal side opening
    int x = rng(rad + 1, 22 - rad), y = (one_in(2) ? rad : 23 - rad);
    ter(x, y) = t_dirt;
   }
   add_spawn( (one_in(3) ? mon_biollante : mon_triffid), 1, rad + 1, rad + 1);
   add_spawn( (one_in(3) ? mon_biollante : mon_triffid), 1, 22 - rad, rad + 1);
   add_spawn( (one_in(3) ? mon_biollante : mon_triffid), 1, rad + 1, 22 - rad);
   add_spawn( (one_in(3) ? mon_biollante : mon_triffid), 1, 22 - rad, 22 - rad);
  }
  square(this, t_slope_down, SEEX - 1, SEEY - 1, SEEX, SEEY);
  break;

 case ot_triffid_roots: {
  square(this, t_root_wall, 0, 0, 2 * SEE - 1, 2 * SEE - 1);
  int node = 0;
  int step = 0;
  bool node_built[16];
  bool done = false;
  for (int i = 0; i < 16; i++)
   node_built[i] = false;
  do {
   node_built[node] = true;
   step++;
   int nodex = 1 + 6 * (node % 4), nodey = 1 + 6 * int(node / 4);
// Clear a 4x4 dirt square
   square(this, t_dirt, nodex, nodey, nodex + 3, nodey + 3);
// Spawn a monster in there
   if (step > 2) { // First couple of chambers are safe
    int monrng = rng(1, 20);
    int spawnx = nodex + rng(0, 3), spawny = nodey + rng(0, 3);
    if (monrng <= 5)
     add_spawn(mon_triffid, rng(1, 4), spawnx, spawny);
    else if (monrng <= 13)
     add_spawn(mon_creeper_hub, 1, spawnx, spawny);
    else if (monrng <= 19)
     add_spawn(mon_biollante, 1, spawnx, spawny);
    else {
     for (int webx = nodex; webx <= nodex + 3; webx++) {
      for (int weby = nodey; weby <= nodey + 3; weby++)
       add_field(g, webx, weby, fd_web, rng(1, 3));
     }
     add_spawn(mon_spider_web, 1, spawnx, spawny);
    }
   }
// TODO: Non-monster hazards?
// Next, pick a cell to move to
   std::vector<direction> move;
   if (node % 4 > 0 && !node_built[node - 1]) move.push_back(WEST);
   if (node % 4 < 3 && !node_built[node + 1]) move.push_back(EAST);
   if (int(node / 4) > 0 && !node_built[node - 4]) move.push_back(NORTH);
   if (int(node / 4) < 3 && !node_built[node + 4]) move.push_back(SOUTH);

   if (move.empty()) { // Nowhere to go!
    square(this, t_slope_down, nodex + 1, nodey + 1, nodex + 2, nodey + 2);
    done = true;
   } else {
    int index = rng(0, move.size() - 1);
    switch (move[index]) {
     case NORTH:
      square(this, t_dirt, nodex + 1, nodey - 2, nodex + 2, nodey - 1);
      node -= 4;
      break;
     case EAST:
      square(this, t_dirt, nodex + 4, nodey + 1, nodex + 5, nodey + 2);
      node++;
      break;
     case SOUTH:
      square(this, t_dirt, nodex + 1, nodey + 4, nodex + 2, nodey + 5);
      node += 4;
      break;
     case WEST:
      square(this, t_dirt, nodex - 2, nodey + 1, nodex - 1, nodey + 2);
      node--;
      break;
    }
   }
  } while (!done);
  square(this, t_slope_up, 2, 2, 3, 3);
  rotate(rng(0, 3));
 } break;

 case ot_triffid_finale: {
  square(this, t_root_wall, 0, 0, 23, 23);
  square(this, t_dirt, 1, 1, 4, 4);
  square(this, t_dirt, 19, 19, 22, 22);
// Drunken walk until we reach the heart (lower right, [19, 19])
// Chance increases by 1 each turn, and gives the % chance of forcing a move
// to the right or down.
  int chance = 0;
  int x = 4, y = 4;
  do {
   ter(x, y) = t_dirt;

   if (chance >= 10 && one_in(10)) { // Add a spawn
    if (one_in(2))
     add_spawn(mon_biollante, 1, x, y);
    else if (!one_in(4))
     add_spawn(mon_creeper_hub, 1, x, y);
    else
     add_spawn(mon_triffid, 1, x, y);
   }

   if (rng(0, 99) < chance) { // Force movement down or to the right
    if (x >= 19)
     y++;
    else if (y >= 19)
     x++;
    else {
     if (one_in(2))
      x++;
     else
      y++;
    }
   } else {
    chance++; // Increase chance of forced movement down/right
// Weigh movement towards directions with lots of existing walls
    int chance_west = 0, chance_east = 0, chance_north = 0, chance_south = 0;
    for (int dist = 1; dist <= 5; dist++) {
     if (ter(x - dist, y) == t_root_wall)
      chance_west++;
     if (ter(x + dist, y) == t_root_wall)
      chance_east++;
     if (ter(x, y - dist) == t_root_wall)
      chance_north++;
     if (ter(x, y + dist) == t_root_wall)
      chance_south++;
    }
    int roll = rng(0, chance_west + chance_east + chance_north + chance_south);
    if (roll < chance_west && x > 0)
     x--;
    else if (roll < chance_west + chance_east && x < 23)
     x++;
    else if (roll < chance_west + chance_east + chance_north && y > 0)
     y--;
    else if (y < 23)
     y++;
   } // Done with drunken walk
  } while (x < 19 || y < 19);
  square(this, t_slope_up, 1, 1, 2, 2);
  add_spawn(mon_triffid_heart, 1, 21, 21);
 } break;

 case ot_basement:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i == 0 || j == 0 || i == SEEX * 2 - 1 || j == SEEY * 2 - 1)
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  switch (rng(0, 4)) {	// TODO: More types!
  case 0:	// Junk!
   place_items(mi_bedroom, 60, 1, 1, SEEX * 2 - 2, SEEY * 2 - 2, false, 0);
   place_items(mi_home_hw, 80, 1, 1, SEEX * 2 - 2, SEEY * 2 - 2, false, 0);
   place_items(mi_homeguns, 10, 1, 1, SEEX * 2 - 2, SEEY * 2 - 2, false, 0);
   break;

  case 1:	// Weapons cache
   _stock_line(*this, t_rack, mi_allguns, 80, point(2, 1), point(SEEX * 2 - 3, 1));
   _stock_line(*this, t_rack, mi_ammo, 94, point(2, 5), point(SEEX * 2 - 3, 5));
   _stock_line(*this, t_rack, mi_weapons, 88, point(2, 9), point(SEEX * 2 - 3, 9));
   break;

  case 2:	// Survival Bunker
   ter(1, 1) = t_bed;
   ter(1, 2) = t_bed;
   ter(SEEX * 2 - 2, 1) = t_bed;
   ter(SEEX * 2 - 2, 2) = t_bed;
   _stock_square(*this, t_rack, mi_softdrugs, 86, point(SEEX - 1, 1), point(SEEX,  2));
   _stock_square(*this, t_rack, mi_cannedfood, 92, point(SEEX - 1, 3), point(SEEX, 6));
   _stock_line(*this, t_rack, mi_homeguns, 72, point(SEEX - 1, 7), point(SEEX, 7));
   _stock_square(*this, t_rack, mi_survival_tools, 83, point(SEEX - 1, 8), point(SEEX, 10));
   _stock_line(*this, t_rack, mi_manuals, 60, point(SEEX - 1, 11), point(SEEX, 11));
   break;

  case 3:	// Chem lab
   _stock_line(*this, t_counter, mi_chemistry, 90, point(1, 1), point(1, SEEY + 3));
   _stock_line(*this, t_counter, one_in(3) ? mi_chemistry : mi_electronics, 90, point(SEEX * 2 - 2, 1), point(SEEX * 2 - 2, SEEY + 3));
   break;

  case 4:	// Weed grow
   line(this, t_counter, 1, 1, 1, SEEY * 2 - 2);
   line(this, t_counter, SEEX * 2 - 2, 1, SEEX * 2 - 2, SEEY * 2 - 2);
   line(this, t_rock, SEEX - 2, SEEY * 2 - 4, SEEX - 2, SEEY * 2 - 2);
   line(this, t_rock, SEEX + 1, SEEY * 2 - 4, SEEX + 1, SEEY * 2 - 2);
   line(this, t_door_locked, SEEX - 1, SEEY * 2 - 4, SEEX, SEEY * 2 - 4);
   for (int i = 3; i < SEEX * 2 - 3; i += 5) {
    for (int j = 3; j < 16; j += 5) {
     square(this, t_dirt, i, j, i + 2, j + 2);
     int num_weed = rng(0, 3) * rng(0, 1);
     for (int n = 0; n < num_weed; n++) {
      int x = rng(i, i + 2), y = rng(j, j + 2);
      add_item(x, y, item::types[itm_weed], 0);
     }
    }
   }
   break;
  }
  // install stairs up here to insulate against bugs earlier
  ter(SEEX - 1, SEEY * 2 - 2) = t_stairs_up;
  ter(SEEX, SEEY * 2 - 2) = t_stairs_up;
  break;
 
// TODO: Maybe subway systems could have broken down trains in them?
 case ot_subway_station:
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_north) && connects_to(t_north, 2)) n_fac = 1;
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_east) && connects_to(t_east, 3)) e_fac = 1;
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_south) && connects_to(t_south, 0)) s_fac = 1;
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_west) && connects_to(t_west, 1)) w_fac = 1;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i < 4 && (w_fac == 0 || j < 4 || j > SEEY * 2 - 5)) ||
        (j < 4 && (n_fac == 0 || i < 4 || i > SEEX * 2 - 5)) ||
        (i > SEEX * 2 - 5 && (e_fac == 0 || j < 4 || j > SEEY * 2 - 5)) ||
        (j > SEEY * 2 - 5 && (s_fac == 0 || i < 4 || i > SEEX * 2 - 5)))
     ter(i, j) = t_floor;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  ter(2,            2           ) = t_stairs_up;
  ter(SEEX * 2 - 3, 2           ) = t_stairs_up;
  ter(2,            SEEY * 2 - 3) = t_stairs_up;
  ter(SEEX * 2 - 3, SEEY * 2 - 3) = t_stairs_up;
  if (ter(2, SEEY) == t_floor) ter(2, SEEY) = t_stairs_up;
  if (ter(SEEX * 2 - 3, SEEY) == t_floor) ter(SEEX * 2 - 3, SEEY) = t_stairs_up;
  if (ter(SEEX, 2) == t_floor) ter(SEEX, 2) = t_stairs_up;
  if (ter(SEEX, SEEY * 2 - 3) == t_floor) ter(SEEX, SEEY * 2 - 3) = t_stairs_up;
  break;
 case ot_subway_ns:
 case ot_subway_ew:
  if (terrain_type == ot_subway_ns) {
   w_fac = (t_west  == ot_cavern ? 0 : 4);
   e_fac = (t_east  == ot_cavern ? SEEX * 2 : SEEX * 2 - 5);
  } else {
   w_fac = (t_north == ot_cavern ? 0 : 4);
   e_fac = (t_south == ot_cavern ? SEEX * 2 : SEEX * 2 - 5);
  }
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < w_fac || i > e_fac)
     ter(i, j) = t_rock;
    else if (one_in(90))
     ter(i, j) = t_rubble;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  if (is_between<ot_sub_station_north, ot_sub_station_west>(t_above))
   ter(SEEX * 2 - 5, rng(SEEY - 5, SEEY + 4)) = t_stairs_up;
  place_items(mi_subway, 30, 4, 0, SEEX * 2 - 5, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_subway_ew) rotate(1);
  break;

 case ot_subway_ne:
 case ot_subway_es:
 case ot_subway_sw:
 case ot_subway_wn:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i >= SEEX * 2 - 4 && j < 4) || i < 4 || j >= SEEY * 2 - 4)
     ter(i, j) = t_rock;
    else if (one_in(30))
     ter(i, j) = t_rubble;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  if (is_between<ot_sub_station_north, ot_sub_station_west>(t_above))
   ter(SEEX * 2 - 5, rng(SEEY - 5, SEEY + 4)) = t_stairs_up;
  place_items(mi_subway, 30, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_subway_es) rotate(1);
  else if (terrain_type == ot_subway_sw) rotate(2);
  else if (terrain_type == ot_subway_wn) rotate(3);
  break;

 case ot_subway_nes:
 case ot_subway_new:
 case ot_subway_nsw:
 case ot_subway_esw:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < 4 || (i >= SEEX * 2 - 4 && (j < 4 || j >= SEEY * 2 - 4)))
     ter(i, j) = t_rock;
    else if (one_in(30))
     ter(i, j) = t_rubble;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  if (is_between<ot_sub_station_north, ot_sub_station_west>(t_above))
   ter(4, rng(SEEY - 5, SEEY + 4)) = t_stairs_up;
  place_items(mi_subway, 35, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_subway_esw) rotate(1);
  else if (terrain_type == ot_subway_nsw) rotate(2);
  else if (terrain_type == ot_subway_new) rotate(3);
  break;

 case ot_subway_nesw:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i < 4 || i >= SEEX * 2 - 4) &&
        (j < 4 || j >= SEEY * 2 - 4))
      ter(i, j) = t_rock;
    else if (one_in(30))
     ter(i, j) = t_rubble;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  if (is_between<ot_sub_station_north, ot_sub_station_west>(t_above))
   ter(4 + rng(0,1)*(SEEX * 2 - 9), 4 + rng(0,1)*(SEEY * 2 - 9)) = t_stairs_up;
  place_items(mi_subway, 40, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  break;

 case ot_sewer_ns:
 case ot_sewer_ew:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < SEEX - 2 || i > SEEX + 1)
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_sewage;
   }
  }
  place_items(mi_sewer, 10, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_sewer_ew) rotate(1);
  break;

 case ot_sewer_ne:
 case ot_sewer_es:
 case ot_sewer_sw:
 case ot_sewer_wn:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i > SEEX + 1 && j < SEEY - 2) || i < SEEX - 2 || j > SEEY + 1)
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_sewage;
   }
  }
  place_items(mi_sewer, 18, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_sewer_es) rotate(1);
  else if (terrain_type == ot_sewer_sw) rotate(2);
  else if (terrain_type == ot_sewer_wn) rotate(3);
  break;

 case ot_sewer_nes:
 case ot_sewer_new:
 case ot_sewer_nsw:
 case ot_sewer_esw:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < SEEX - 2 || (i > SEEX + 1 && (j < SEEY - 2 || j > SEEY + 1)))
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_sewage;
   }
  }
  place_items(mi_sewer, 23, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_sewer_esw) rotate(1);
  else if (terrain_type == ot_sewer_nsw) rotate(2);
  else if (terrain_type == ot_sewer_new) rotate(3);
  break;

 case ot_sewer_nesw:
  rn = rng(0, 3);
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((i < SEEX - 2 || i > SEEX + 1) && (j < SEEY - 2 || j > SEEY + 1))
      ter(i, j) = t_rock;
    else
     ter(i, j) = t_sewage;
    if (rn == 0 && (trig_dist(i, j, SEEX - 1, SEEY - 1) <= 6 ||
                    trig_dist(i, j, SEEX - 1, SEEY    ) <= 6 ||
                    trig_dist(i, j, SEEX,     SEEY - 1) <= 6 ||
                    trig_dist(i, j, SEEX,     SEEY    ) <= 6   ))
     ter(i, j) = t_sewage;
    if (rn == 0 && (i == SEEX - 1 || i == SEEX) && (j == SEEY - 1 || j == SEEY))
     ter(i, j) = t_grate;
   }
  }
  place_items(mi_sewer, 28, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  break;

 case ot_ants_ns:
 case ot_ants_ew:
  x = SEEX;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_rock;
  }
  for (int j = 0; j < SEEY * 2; j++) {
   for (int i = x - 2; i <= x + 3; i++) {
    if (i >= 1 && i < SEEX * 2 - 1)
     ter(i, j) = t_rock_floor;
   }
   x += rng(-1, 1);
   while (abs(SEEX - x) > SEEX * 2 - j - 1) {
    if (x < SEEX) x++;
    if (x > SEEX) x--;
   }
  }
  if (terrain_type == ot_ants_ew) rotate(1);
  break;

 case ot_ants_ne:
 case ot_ants_es:
 case ot_ants_sw:
 case ot_ants_wn:
  x = SEEX;
  y = 1;
  rn = 0;
// First, set it all to rock
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_rock;
  }

  for (int i = SEEX - 2; i <= SEEX + 3; i++) {
   ter(i, 0) = t_rock_floor;
   ter(i, 1) = t_rock_floor;
   ter(i, 2) = t_rock_floor;
   ter(SEEX * 2 - 1, i) = t_rock_floor;
   ter(SEEX * 2 - 2, i) = t_rock_floor;
   ter(SEEX * 2 - 3, i) = t_rock_floor;
  }
  do {
   for (int i = x - 2; i <= x + 3; i++) {
    for (int j = y - 2; j <= y + 3; j++) {
     if (i > 0 && i < SEEX * 2 - 1 && j > 0 && j < SEEY * 2 - 1)
      ter(i, j) = t_rock_floor;
    }
   }
   if (rn < SEEX) {
    x += rng(-1, 1);
    y++;
   } else {
    x++;
    if (!one_in(x - SEEX))
     y += rng(-1, 1);
    else if (y < SEEY)
     y++;
    else if (y > SEEY)
     y--;
   }
   rn++;
  } while (x < SEEX * 2 - 1 || y != SEEY);
  for (int i = x - 2; i <= x + 3; i++) {
   for (int j = y - 2; j <= y + 3; j++) {
    if (i > 0 && i < SEEX * 2 - 1 && j > 0 && j < SEEY * 2 - 1)
     ter(i, j) = t_rock_floor;
   }
  }
  if (terrain_type == ot_ants_es) rotate(1);
  else if (terrain_type == ot_ants_sw) rotate(2);
  else if (terrain_type == ot_ants_wn) rotate(3);
  break;
  
 case ot_ants_nes:
 case ot_ants_new:
 case ot_ants_nsw:
 case ot_ants_esw:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_rock;
  }
  x = SEEX;
  for (int j = 0; j < SEEY * 2; j++) {
   for (int i = x - 2; i <= x + 3; i++) {
    if (i >= 1 && i < SEEX * 2 - 1)
     ter(i, j) = t_rock_floor;
   }
   x += rng(-1, 1);
   while (abs(SEEX - x) > SEEY * 2 - j - 1) {
    if (x < SEEX) x++;
    if (x > SEEX) x--;
   }
  }
  y = SEEY;
  for (int i = SEEX; i < SEEX * 2; i++) {
   for (int j = y - 2; j <= y + 3; j++) {
    if (j >= 1 && j < SEEY * 2 - 1)
     ter(i, j) = t_rock_floor;
   }
   y += rng(-1, 1);
   while (abs(SEEY - y) > SEEX * 2 - 1 - i) {
    if (y < SEEY) y++;
    if (y > SEEY) y--;
   }
  }
  if (terrain_type == ot_ants_new) rotate(3);
  else if (terrain_type == ot_ants_nsw) rotate(2);
  else if (terrain_type == ot_ants_esw) rotate(1);
  break;

 case ot_ants_nesw:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_rock;
  }
  x = SEEX;
  for (int j = 0; j < SEEY * 2; j++) {
   for (int i = x - 2; i <= x + 3; i++) {
    if (i >= 1 && i < SEEX * 2 - 1)
     ter(i, j) = t_rock_floor;
   }
   x += rng(-1, 1);
   while (abs(SEEX - x) > SEEY * 2 - j - 1) {
    if (x < SEEX) x++;
    if (x > SEEX) x--;
   }
  }

  y = SEEY;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = y - 2; j <= y + 3; j++) {
    if (j >= 1 && j < SEEY * 2 - 1)
     ter(i, j) = t_rock_floor;
   }
   y += rng(-1, 1);
   while (abs(SEEY - y) > SEEX * 2 - i - 1) {
    if (y < SEEY) y++;
    if (y > SEEY) y--;
   }
  }
  break;

 case ot_ants_food:
 case ot_ants_larvae:
 case ot_ants_queen:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (i < SEEX - 4 || i > SEEX + 5 || j < SEEY - 4 || j > SEEY + 5)
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_rock_floor;
   }
  }
  rn = rng(10, 20);
  for (int n = 0; n < rn; n++) {
   cw = rng(1, 8);
   do {
    x = rng(1 + cw, SEEX * 2 - 2 - cw);
    y = rng(1 + cw, SEEY * 2 - 2 - cw);
   } while (ter(x, y) == t_rock);
   for (int i = x - cw; i <= x + cw; i++) {
    for (int j = y - cw; j <= y + cw; j++) {
     if (trig_dist(x, y, i, j) <= cw)
      ter(i, j) = t_rock_floor;
    }
   }
  }
  if (connects_to(t_north, 2)) square(this, t_rock_floor, point(SEEX - 2, 0), point(SEEX + 3, SEEY));
  if (connects_to(t_east, 3)) square(this, t_rock_floor, point(SEEX, SEEY - 2), point(SEEX * 2 - 1, SEEY + 3));
  if (connects_to(t_south, 0)) square(this, t_rock_floor, point(SEEX - 2, SEEY), point(SEEX + 3, SEEY * 2 - 1));
  if (connects_to(t_west, 1)) square(this, t_rock_floor, point(0, SEEY - 2), point(SEEX, SEEY + 3));
  if (terrain_type == ot_ants_food)
   place_items(mi_ant_food, 92, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  else
   place_items(mi_ant_egg,  98, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  if (terrain_type == ot_ants_queen)
   add_spawn(mon_ant_queen, 1, SEEX, SEEY);
  else if (terrain_type == ot_ants_larvae)
   add_spawn(mon_ant_larva, 10, SEEX, SEEY);
  break;

 case ot_tutorial:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if (j == 0 || j == SEEY * 2 - 1)
     ter(i, j) = t_wall_h;
    else if (i == 0 || i == SEEX * 2 - 1)
     ter(i, j) = t_wall_v;
    else if (j == SEEY) {
     if (i % 4 == 2)
      ter(i, j) = t_door_c;
     else if (i % 5 == 3)
      ter(i, j) = t_window;
     else
      ter(i, j) = t_wall_h;
    } else
     ter(i, j) = t_floor;
   }
  }
  ter(7, SEEY * 2 - 4) = t_rack;
  ter(SEEX * 2 - 2, SEEY * 2 - 4) = t_gas_pump;
  if (t_above != ot_null) {
   ter(SEEX - 2, SEEY + 2) = t_stairs_up;
   ter(2, 2) = t_water_sh;
   ter(2, 3) = t_water_sh;
   ter(3, 2) = t_water_sh;
   ter(3, 3) = t_water_sh;
  } else {
   add_item(           5, SEEY + 1, item::types[itm_helmet_bike], 0);
   add_item(           4, SEEY + 1, item::types[itm_backpack], 0);
   add_item(           3, SEEY + 1, item::types[itm_pants_cargo], 0);
   add_item(           7, SEEY * 2 - 4, item::types[itm_machete], 0);
   add_item(           7, SEEY * 2 - 4, item::types[itm_9mm], 0);
   add_item(           7, SEEY * 2 - 4, item::types[itm_9mmP], 0);
   add_item(           7, SEEY * 2 - 4, item::types[itm_uzi], 0);
   add_item(SEEX * 2 - 2, SEEY + 5, item::types[itm_bubblewrap], 0);
   add_item(SEEX * 2 - 2, SEEY + 6, item::types[itm_grenade], 0);
   add_item(SEEX * 2 - 3, SEEY + 6, item::types[itm_flashlight], 0);
   add_item(SEEX * 2 - 2, SEEY + 7, item::types[itm_cig], 0);
   add_item(SEEX * 2 - 2, SEEY + 7, item::types[itm_codeine], 0);
   add_item(SEEX * 2 - 3, SEEY + 7, item::types[itm_water], 0);
   ter(SEEX - 2, SEEY + 2) = t_stairs_down;
  }
  break;

 case ot_cavern:
  n_fac = any<ot_subway_ns, ot_subway_ew, ot_cavern>(t_north) ? 0 : 3;
  e_fac = any<ot_subway_ns, ot_subway_ew, ot_cavern>(t_east) ? SEEX * 2 - 1 : SEEX * 2 - 4;
  s_fac = any<ot_subway_ns, ot_subway_ew, ot_cavern>(t_south) ? SEEY * 2 - 1 : SEEY * 2 - 4;
  w_fac = any<ot_subway_ns, ot_subway_ew, ot_cavern>(t_west) ? 0 : 3;
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    if ((j < n_fac || j > s_fac || i < w_fac || i > e_fac) &&
        (!one_in(3) || j == 0 || j == SEEY*2 - 1 || i == 0 || i == SEEX*2 - 1))
     ter(i, j) = t_rock;
    else
     ter(i, j) = t_rock_floor;
   }
  }

  rn = rng(0, 2) * rng(0, 3) + rng(0, 1);	// Number of pillars
  for (int n = 0; n < rn; n++) {
   int px = rng(5, SEEX * 2 - 6);
   int py = rng(5, SEEY * 2 - 6);
   for (int i = px - 1; i <= px + 1; i++) {
    for (int j = py - 1; j <= py + 1; j++)
     ter(i, j) = t_rock;
   }
  }

  if (connects_to(t_north, 2)) square(this, t_rock_floor, point(SEEX - 2, 0), point(SEEX + 3, SEEY));
  if (connects_to(t_east, 3)) square(this, t_rock_floor, point(SEEX, SEEY - 2), point(SEEX * 2 - 1, SEEY + 3));
  if (connects_to(t_south, 0)) square(this, t_rock_floor, point(SEEX - 2, SEEY), point(SEEX + 3, SEEY * 2 - 1));
  if (connects_to(t_west, 1)) square(this, t_rock_floor, point(0, SEEY-2), point(SEEX, SEEY+3));
  place_items(mi_cavern, 60, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, false, 0);
  if (one_in(6)) {	// Miner remains
      const auto ok = grep(point(0), point(2 * SEE - 1), [&](point pt) {
          return 0 != move_cost(pt);
      });
      if (!ok.empty()) {
          decltype(auto) pt = ok[rng(0, ok.size() - 1)];
          if (!one_in(3)) add_item(pt, item::types[itm_jackhammer], 0);
          if (one_in(3)) add_item(pt, item::types[itm_mask_dust], 0);
          if (one_in(2)) add_item(pt, item::types[itm_hat_hard], 0);
          while (!one_in(3)) add_item(pt, item::types[rng(itm_can_beans, itm_can_tuna)], 0);
      }
  }
  break;

 default:
  debugmsg("Error: tried to generate map for omtype %d, \"%s\"", terrain_type,
           oter_t::list[terrain_type].name.c_str());
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++)
    ter(i, j) = t_floor;
  }
  break;
 }

// Now, fix sewers and subways so that they interconnect.

 if (is_between<ot_subway_ns, ot_subway_nesw>(terrain_type)) {
  if (is_between<ot_sewer_ns, ot_sewer_nesw>(t_north) && !connects_to(terrain_type, 0)) {
   if (connects_to(t_north, 2)) {
    for (int i = SEEX - 2; i < SEEX + 2; i++) {
     for (int j = 0; j < SEEY; j++)
      ter(i, j) = t_sewage;
    }
   } else {
    for (int j = 0; j < 3; j++) {
     ter(SEEX, j) = t_rock_floor;
     ter(SEEX - 1, j) = t_rock_floor;
    }
    ter(SEEX, 3) = t_door_metal_c;
    ter(SEEX - 1, 3) = t_door_metal_c;
   }
  }
  if (is_between<ot_sewer_ns, ot_sewer_nesw>(t_east) && !connects_to(terrain_type, 1)) {
   if (connects_to(t_east, 3)) {
    for (int i = SEEX; i < SEEX * 2; i++) {
     for (int j = SEEY - 2; j < SEEY + 2; j++)
      ter(i, j) = t_sewage;
    }
   } else {
    for (int i = SEEX * 2 - 3; i < SEEX * 2; i++) {
     ter(i, SEEY) = t_rock_floor;
     ter(i, SEEY - 1) = t_rock_floor;
    }
    ter(SEEX * 2 - 4, SEEY) = t_door_metal_c;
    ter(SEEX * 2 - 4, SEEY - 1) = t_door_metal_c;
   }
  }
  if (is_between<ot_sewer_ns, ot_sewer_nesw>(t_south) && !connects_to(terrain_type, 2)) {
   if (connects_to(t_south, 0)) {
    for (int i = SEEX - 2; i < SEEX + 2; i++) {
     for (int j = SEEY; j < SEEY * 2; j++)
      ter(i, j) = t_sewage;
    }
   } else {
    for (int j = SEEY * 2 - 3; j < SEEY * 2; j++) {
     ter(SEEX, j) = t_rock_floor;
     ter(SEEX - 1, j) = t_rock_floor;
    }
    ter(SEEX, SEEY * 2 - 4) = t_door_metal_c;
    ter(SEEX - 1, SEEY * 2 - 4) = t_door_metal_c;
   }
  }
  if (is_between<ot_sewer_ns, ot_sewer_nesw>(t_west) && !connects_to(terrain_type, 3)) {
   if (connects_to(t_west, 1)) {
    for (int i = 0; i < SEEX; i++) {
     for (int j = SEEY - 2; j < SEEY + 2; j++)
      ter(i, j) = t_sewage;
    }
   } else {
    for (int i = 0; i < 3; i++) {
     ter(i, SEEY) = t_rock_floor;
     ter(i, SEEY - 1) = t_rock_floor;
    }
    ter(3, SEEY) = t_door_metal_c;
    ter(3, SEEY - 1) = t_door_metal_c;
   }
  }
 } else if (is_between<ot_sewer_ns, ot_sewer_nesw>(terrain_type)) {
  if (t_above == ot_road_nesw_manhole)
   ter(rng(SEEX - 2, SEEX + 1), rng(SEEY - 2, SEEY + 1)) = t_ladder_up;
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_north) && !connects_to(terrain_type, 0)) {
   for (int j = 0; j < SEEY - 3; j++) {
    ter(SEEX, j) = t_rock_floor;
    ter(SEEX - 1, j) = t_rock_floor;
   }
   ter(SEEX, SEEY - 3) = t_door_metal_c;
   ter(SEEX - 1, SEEY - 3) = t_door_metal_c;
  }
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_east) && !connects_to(terrain_type, 1)) {
   for (int i = SEEX + 3; i < SEEX * 2; i++) {
    ter(i, SEEY) = t_rock_floor;
    ter(i, SEEY - 1) = t_rock_floor;
   }
   ter(SEEX + 2, SEEY) = t_door_metal_c;
   ter(SEEX + 2, SEEY - 1) = t_door_metal_c;
  }
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_south) && !connects_to(terrain_type, 2)) {
   for (int j = SEEY + 3; j < SEEY * 2; j++) {
    ter(SEEX, j) = t_rock_floor;
    ter(SEEX - 1, j) = t_rock_floor;
   }
   ter(SEEX, SEEY + 2) = t_door_metal_c;
   ter(SEEX - 1, SEEY + 2) = t_door_metal_c;
  }
  if (is_between<ot_subway_ns, ot_subway_nesw>(t_west) && !connects_to(terrain_type, 3)) {
   for (int i = 0; i < SEEX - 3; i++) {
    ter(i, SEEY) = t_rock_floor;
    ter(i, SEEY - 1) = t_rock_floor;
   }
   ter(SEEX - 3, SEEY) = t_door_metal_c;
   ter(SEEX - 3, SEEY - 1) = t_door_metal_c;
  }
 } else if (is_between<ot_ants_ns, ot_ants_queen>(terrain_type)) {
  if (t_above == ot_anthill) {
      const auto ok = grep(point(0), point(2 * SEE - 1), [&](point pt) {
          return t_rock_floor == ter(pt);
      });
      if (!ok.empty()) ter(ok[rng(0, ok.size() - 1)]) = t_slope_up;
  }
 }
}

static void add_corpse(game* g, map* m, int x, int y)
{
    item body(0);
    m->add_item(x, y, body);
    m->put_items_from(mi_shoes, 1, x, y);
    m->put_items_from(mi_pants, 1, x, y);
    m->put_items_from(mi_shirts, 1, x, y);
    if (one_in(6)) m->put_items_from(mi_jackets, 1, x, y);
    if (one_in(15)) m->put_items_from(mi_bags, 1, x, y);
}

// 2019-12-21: inherited from C:Whales as dead code.
void map::post_process(game *g, unsigned zones)
{
 if (zones & mfb(OMZONE_CITY)) {
  // seems to be an attempt at mocking up prior damage
  if (!one_in(10)) { // 90% chance of smashing stuff up
   for (int x = 0; x < 2 * SEEX; x++) {
    for (int y = 0; y < 2 * SEEY; y++)
     bash(x, y, 20);
   }
  }
  if (one_in(10)) { // 10% chance of corpses
   int num_corpses = rng(1, 8);
   for (int i = 0; i < num_corpses; i++) {
    int x = rng(0, 2 * SEEX), y = rng(0, 2 * SEEY);
    if (move_cost(x, y) > 0)
     add_corpse(g, this, x, y);
   }
  }
 } // OMZONE_CITY

 if (zones & mfb(OMZONE_BOMBED)) {
  // the ot_crater terrain type actually is lousy for this; might be good as a converter to ot_crater type
  while (one_in(4)) {
   point center( rng(4, 19), rng(4, 19) );
   int radius = rng(1, 4);
   for (int x = center.x - radius; x <= center.x + radius; x++) {
    for (int y = center.y - radius; y <= center.y + radius; y++) {
     if (rl_dist(x, y, center) <= rng(1, radius))
      destroy(g, x, y, false);
    }
   }
  }
 }

}

void map::place_items(items_location loc, int chance, int x1, int y1,
                      int x2, int y2, bool ongrass, int turn)
{
 const auto& eligible = map::items[loc];

 if (chance >= 100 || chance <= 0) {
  debugmsg("map::place_items() called with an invalid chance (%d)", chance);
  return;
 }
 if (eligible.size() == 0) { // No items here! (Why was it called?)
  debugmsg("map::place_items() called for an empty items list (list #%d)", loc);
  return;
 }

 int item_chance = 0;	// # of items
 for (const auto id : eligible) item_chance += item::types[id]->rarity;

 std::vector<std::pair<itype_id, point> > to_create;

 int selection;
 int px, py;
 while (rng(0, 99) < chance) {
  int randnum = rng(1, item_chance);
  selection = -1;
  while (randnum > 0 && ++selection < eligible.size()-1) randnum -= item::types[eligible[selection]]->rarity;
  int tries = 0;
  do {
   if (20 <= ++tries) {
       if (!to_create.empty()) break;
       return;
   }
   px = rng(x1, x2);
   py = rng(y1, y2);
// Only place on valid terrain
   const auto terrain = ter(px, py);
   if (!ongrass && (t_dirt == terrain || t_grass == terrain)) continue;
   const auto& t_data = ter_t::list[terrain];
   if (t_data.movecost == 0 && !(t_data.flags & mfb(container))) continue;
   to_create.push_back(std::pair(eligible[selection], point(px, py)));
  } while (false);
 }

 if (to_create.empty()) return;

 const bool will_autospawn_ammo = (mi_homeguns == loc || mi_behindcounter == loc);

 std::vector<ammotype> usable_ammo;
 for (const auto& x : to_create) item::uses_ammo_type(x.first, usable_ammo);
 if (!will_autospawn_ammo && !usable_ammo.empty()) {
     // Bay12/Robsoie : need to ensure ammo is something actually usable
     for (auto& x : to_create) {
         if (itm_battery <= x.first && itm_gasoline >= x.first && !cataclysm::any(usable_ammo, dynamic_cast<const it_ammo*>(item::types[x.first])->type)) {
             x.first = default_ammo(usable_ammo[rng(0, usable_ammo.size() - 1)]);
         }
     }
 }

 // \todo reality checks on what is about to be created


 for (auto& x : to_create) {
     const auto it = item::types[x.first];
     add_item(x.second, it, turn);
     // Guns in the home and behind counters are generated with their ammo
     // TODO: Make this less of a hack
     if (it->is_gun() && will_autospawn_ammo) {
         add_item(x.second, item::types[default_ammo(dynamic_cast<it_gun*> (it)->ammo)], turn);
     }
 }
}

void map::put_items_from(items_location loc, int num, int x, int y, int turn)
{
 const auto& eligible = map::items[loc];
 int item_chance = 0;	// # of items
 for (const auto id : eligible) item_chance += item::types[id]->rarity;

 for (int i = 0; i < num; i++) {
  int selection = -1;
  int randnum = rng(1, item_chance);
  while (randnum > 0 && ++selection < eligible.size() - 1) randnum -= item::types[eligible[selection]]->rarity;
  add_item(x, y, item::types[eligible[selection]], turn);
 }
}

void map::add_spawn(mon_id type, int count, int x, int y, bool friendly,
                    int faction_id, int mission_id, std::string name)
{
 if (x < 0 || x >= SEEX * my_MAPSIZE || y < 0 || y >= SEEY * my_MAPSIZE) {
  debugmsg("Bad add_spawn(%d, %d, %d, %d)", type, count, x, y);
  return;
 }
 int nonant = int(x / SEEX) + int(y / SEEY) * my_MAPSIZE;
 x %= SEEX;
 y %= SEEY;
 spawn_point tmp(type, count, x, y, faction_id, mission_id, friendly, name);
 grid[nonant]->spawns.push_back(tmp);
}

void map::add_spawn(monster *mon)
{
 std::string spawnname = (mon->unique_name == "" ? "NONE" : mon->unique_name);
 point spawn = mon->is_static_spawn() ? mon->spawnpos : mon->pos;
 while (spawn.x < 0) spawn.x += SEEX;
 while (spawn.y < 0) spawn.y += SEEY;
 spawn.x %= SEEX;
 spawn.y %= SEEY;
 add_spawn(mon_id(mon->type->id), 1, spawn.x, spawn.y, (mon->friendly < 0),
           mon->faction_id, mon->mission_id, spawnname);
}

vehicle* map::add_vehicle(vhtype_id type, int x, int y, int dir)
{
 if (!inbounds(x,y)) {
  debugmsg("Bad add_vehicle t=%d d=%d x=%d y=%d", type, dir, x, y);
  return 0;
 }
 int smx = x / SEEX;
 int smy = y / SEEY;
 int nonant = smx + smy * my_MAPSIZE;
 x %= SEEX;
 y %= SEEY;
 vehicle veh(type);
 veh.pos = point(x,y);
 veh.sm = point(smx,smy);
 veh.face.init(dir);
 veh.turn_dir = dir;
 veh.precalc_mounts (0, dir);
 grid[nonant]->vehicles.push_back(std::move(veh));
 return &grid[nonant]->vehicles.back();
}

computer* map::add_computer(int x, int y, std::string name, int security)
{
 ter(x, y) = t_console; // TODO: Turn this off?
 int nonant = int(x / SEEX) + int(y / SEEY) * my_MAPSIZE;
 grid[nonant]->comp = computer(name, security);
 return &(grid[nonant]->comp);
}

void map::rotate(int turns)
{
 ter_id rotated         [SEEX*2][SEEY*2];
 trap_id traprot        [SEEX*2][SEEY*2];
 std::vector<item> itrot[SEEX*2][SEEY*2];
 std::vector<std::vector<spawn_point> > sprot(my_MAPSIZE * my_MAPSIZE);
 computer tmpcomp;
 std::vector<vehicle> tmpveh;

 switch (turns) {
 case 1:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    rotated[i][j] = ter  (j, SEEX * 2 - 1 - i);
    itrot  [i][j] = i_at (j, SEEX * 2 - 1 - i);
    traprot[i][j] = tr_at(j, SEEX * 2 - 1 - i);
    i_clear(j, SEEX * 2 - 1 - i);
   }
  }
// Now, spawn points
  for (int sx = 0; sx < 2; sx++) {
   for (int sy = 0; sy < 2; sy++) {
    int gridfrom = sx + sy * my_MAPSIZE;
    int gridto = sx * my_MAPSIZE + 1 - sy;
    for (int j = 0; j < grid[gridfrom]->spawns.size(); j++) {
     spawn_point tmp = grid[gridfrom]->spawns[j];
	 tmp.pos = point(SEEY - 1 - tmp.pos.y, tmp.pos.x);	// depends on SEEX==SEEY
     sprot[gridto].push_back(tmp);
    }
   }
  }
// Finally, computers
  tmpcomp = grid[0]->comp;
  grid[0]->comp = grid[my_MAPSIZE]->comp;
  grid[my_MAPSIZE]->comp = grid[my_MAPSIZE + 1]->comp;
  grid[my_MAPSIZE + 1]->comp = grid[1]->comp;
  grid[1]->comp = tmpcomp;
// ...and vehicles
  tmpveh = grid[0]->vehicles;
  grid[0]->vehicles = grid[my_MAPSIZE]->vehicles;
  grid[my_MAPSIZE]->vehicles = grid[my_MAPSIZE + 1]->vehicles;
  grid[my_MAPSIZE + 1]->vehicles = grid[1]->vehicles;
  grid[1]->vehicles = tmpveh;
  break;
    
 case 2:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    rotated[i][j] = ter  (SEEX * 2 - 1 - i, SEEY * 2 - 1 - j);
    itrot  [i][j] = i_at (SEEX * 2 - 1 - i, SEEY * 2 - 1 - j);
    traprot[i][j] = tr_at(SEEX * 2 - 1 - i, SEEY * 2 - 1 - j);
    i_clear(SEEX * 2 - 1 - i, SEEY * 2 - 1 - j);
   }
  }
// Now, spawn points
  for (int sx = 0; sx < 2; sx++) {
   for (int sy = 0; sy < 2; sy++) {
    int gridfrom = sx + sy * my_MAPSIZE;
    int gridto = (1 - sy) * my_MAPSIZE + 1 - sx;
    for (int j = 0; j < grid[gridfrom]->spawns.size(); j++) {
     spawn_point tmp = grid[gridfrom]->spawns[j];
	 tmp.pos = point(SEEX - 1 - tmp.pos.x, SEEY - 1 - tmp.pos.y);
     sprot[gridto].push_back(tmp);
    }
   }
  }
  tmpcomp = grid[0]->comp;
  grid[0]->comp = grid[my_MAPSIZE + 1]->comp;
  grid[my_MAPSIZE + 1]->comp = tmpcomp;
  tmpcomp = grid[1]->comp;
  grid[1]->comp = grid[my_MAPSIZE]->comp;
  grid[my_MAPSIZE]->comp = tmpcomp;
// ...and vehicles
  tmpveh = grid[0]->vehicles;
  grid[0]->vehicles = grid[my_MAPSIZE + 1]->vehicles;
  grid[my_MAPSIZE + 1]->vehicles = tmpveh;
  tmpveh = grid[1]->vehicles;
  grid[1]->vehicles = grid[my_MAPSIZE]->vehicles;
  grid[my_MAPSIZE]->vehicles = tmpveh;
  break;
    
 case 3:
  for (int i = 0; i < SEEX * 2; i++) {
   for (int j = 0; j < SEEY * 2; j++) {
    rotated[i][j] = ter  (SEEY * 2 - 1 - j, i);
    itrot  [i][j] = i_at (SEEY * 2 - 1 - j, i);
    traprot[i][j] = tr_at(SEEY * 2 - 1 - j, i);
    i_clear(SEEY * 2 - 1 - j, i);
   }
  }
// Now, spawn points
  for (int sx = 0; sx < 2; sx++) {
   for (int sy = 0; sy < 2; sy++) {
    int gridfrom = sx + sy * my_MAPSIZE;
    int gridto = (1 - sx) * my_MAPSIZE + sy;
    for (int j = 0; j < grid[gridfrom]->spawns.size(); j++) {
     spawn_point tmp = grid[gridfrom]->spawns[j];
	 tmp.pos = point(tmp.pos.y, SEEX - 1 - tmp.pos.x);	// depends on SEEX==SEEY
     sprot[gridto].push_back(tmp);
    }
   }
  }
  tmpcomp = grid[0]->comp;
  grid[0]->comp = grid[1]->comp;
  grid[1]->comp = grid[my_MAPSIZE + 1]->comp;
  grid[my_MAPSIZE + 1]->comp = grid[my_MAPSIZE]->comp;
  grid[my_MAPSIZE]->comp = tmpcomp;
// ...and vehicles
  tmpveh = grid[0]->vehicles;
  grid[0]->vehicles = grid[1]->vehicles;
  grid[1]->vehicles = grid[my_MAPSIZE + 1]->vehicles;
  grid[my_MAPSIZE + 1]->vehicles = grid[my_MAPSIZE]->vehicles;
  grid[my_MAPSIZE]->vehicles = tmpveh;
  break;

 default:
  return;
 }
// assert(turns >= 1 && turns <= 3);    // true due to above switch statement

// change vehicles' directions
 for (int i = 0; i < my_MAPSIZE * my_MAPSIZE; i++)
     for (auto& veh : grid[i]->vehicles) veh.turn(turns * 90);

// Set the spawn points
 grid[0]->spawns = std::move(sprot[0]);
 grid[1]->spawns = std::move(sprot[1]);
 grid[my_MAPSIZE]->spawns = std::move(sprot[my_MAPSIZE]);
 grid[my_MAPSIZE + 1]->spawns = std::move(sprot[my_MAPSIZE + 1]);
 for (int i = 0; i < SEEX * 2; i++) {
  for (int j = 0; j < SEEY * 2; j++) {
   ter  (i, j) = rotated[i][j];
   i_at (i, j) = itrot  [i][j];
   tr_at(i, j) = traprot[i][j];
   if (turns % 2 == 1) { 	// Rotate things like walls 90 degrees
    if (ter(i, j) == t_wall_v)
     ter(i, j) = t_wall_h;
    else if (ter(i, j) == t_wall_h)
     ter(i, j) = t_wall_v;
    else if (ter(i, j) == t_wall_metal_v)
     ter(i, j) = t_wall_metal_h;
    else if (ter(i, j) == t_wall_metal_h)
     ter(i, j) = t_wall_metal_v;
    else if (ter(i, j) == t_railing_v)
     ter(i, j) = t_railing_h;
    else if (ter(i, j) == t_railing_h)
     ter(i, j) = t_railing_v;
    else if (ter(i, j) == t_wall_glass_h)
     ter(i, j) = t_wall_glass_v;
    else if (ter(i, j) == t_wall_glass_v)
     ter(i, j) = t_wall_glass_h;
    else if (ter(i, j) == t_wall_glass_h_alarm)
     ter(i, j) = t_wall_glass_v_alarm;
    else if (ter(i, j) == t_wall_glass_v_alarm)
     ter(i, j) = t_wall_glass_h_alarm;
    else if (ter(i, j) == t_reinforced_glass_h)
     ter(i, j) = t_reinforced_glass_v;
    else if (ter(i, j) == t_reinforced_glass_v)
     ter(i, j) = t_reinforced_glass_h;
    else if (ter(i, j) == t_fence_v)
     ter(i, j) = t_fence_h;
    else if (ter(i, j) == t_fence_h)
     ter(i, j) = t_fence_v;
   }
  }
 }
}

// Hideous function, I admit...
bool connects_to(oter_id there, int dir)
{
 switch (dir) {
 case 2:
  if (there == ot_subway_ns  || there == ot_subway_es || there == ot_subway_sw||
      there == ot_subway_nes || there == ot_subway_nsw ||
      there == ot_subway_esw || there == ot_subway_nesw ||
      there == ot_sewer_ns   || there == ot_sewer_es || there == ot_sewer_sw || 
      there == ot_sewer_nes  || there == ot_sewer_nsw || there == ot_sewer_esw||
      there == ot_sewer_nesw || there == ot_ants_ns || there == ot_ants_es ||
      there == ot_ants_sw    || there == ot_ants_nes ||  there == ot_ants_nsw ||
      there == ot_ants_esw   || there == ot_ants_nesw)
   return true;
  return false;
 case 3:
  if (there == ot_subway_ew  || there == ot_subway_sw || there == ot_subway_wn||
      there == ot_subway_new || there == ot_subway_nsw ||
      there == ot_subway_esw || there == ot_subway_nesw ||
      there == ot_sewer_ew   || there == ot_sewer_sw || there == ot_sewer_wn ||
      there == ot_sewer_new  || there == ot_sewer_nsw || there == ot_sewer_esw||
      there == ot_sewer_nesw || there == ot_ants_ew || there == ot_ants_sw ||
      there == ot_ants_wn    || there == ot_ants_new || there == ot_ants_nsw ||
      there == ot_ants_esw   || there == ot_ants_nesw)
   return true;
  return false;
 case 0:
  if (there == ot_subway_ns  || there == ot_subway_ne || there == ot_subway_wn||
      there == ot_subway_nes || there == ot_subway_new ||
      there == ot_subway_nsw || there == ot_subway_nesw ||
      there == ot_sewer_ns   || there == ot_sewer_ne ||  there == ot_sewer_wn ||
      there == ot_sewer_nes  || there == ot_sewer_new || there == ot_sewer_nsw||
      there == ot_sewer_nesw || there == ot_ants_ns || there == ot_ants_ne ||
      there == ot_ants_wn    || there == ot_ants_nes || there == ot_ants_new ||
      there == ot_ants_nsw   || there == ot_ants_nesw)
   return true;
  return false;
 case 1:
  if (there == ot_subway_ew  || there == ot_subway_ne || there == ot_subway_es||
      there == ot_subway_nes || there == ot_subway_new ||
      there == ot_subway_esw || there == ot_subway_nesw ||
      there == ot_sewer_ew   || there == ot_sewer_ne || there == ot_sewer_es ||
      there == ot_sewer_nes  || there == ot_sewer_new || there == ot_sewer_esw||
      there == ot_sewer_nesw || there == ot_ants_ew || there == ot_ants_ne ||
      there == ot_ants_es    || there == ot_ants_nes || there == ot_ants_new ||
      there == ot_ants_esw   || there == ot_ants_nesw)
   return true;
  return false;
 default:
  debugmsg("Connects_to with dir of %d", dir);
  return false;
 }
}

void house_room(map *m, room_type type, int x1, int y1, int x2, int y2)
{
 for (int i = x1; i <= x2; i++) {
  for (int j = y1; j <= y2; j++) {
   auto& t = m->ter(i, j);
   if (t_grass == t || t_dirt == t || t_floor == t) {
    if (j == y1 || j == y2) {
     t = t_wall_h;
    } else if (i == x1 || i == x2) {
     t = t_wall_v;
    } else
     t = t_floor;
   }
  }
 }
 for (int i = y1 + 1; i <= y2 - 1; i++) {
  m->ter(x1, i) = t_wall_v;
  m->ter(x2, i) = t_wall_v;
 }

 items_location placed = mi_none;
 int chance = 0;
 switch (type) {
 case room_living:
  placed = mi_livingroom;
  chance = 83;
  break;
 case room_kitchen:
  placed = mi_kitchen;
  chance = 75;
  m->place_items(mi_cleaning,  58, x1 + 1, y1 + 1, x2 - 1, y2 - 2, false, 0);
  m->place_items(mi_home_hw,   40, x1 + 1, y1 + 1, x2 - 1, y2 - 2, false, 0);
  switch (rng(1, 4)) {
  case 1:
   m->ter(x1 + 2, y1 + 1) = t_fridge;
   m->place_items(mi_fridge, 82, x1 + 2, y1 + 1, x1 + 2, y1 + 1, false, 0);
   break;
  case 2:
   m->ter(x2 - 2, y1 + 1) = t_fridge;
   m->place_items(mi_fridge, 82, x2 - 2, y1 + 1, x2 - 2, y1 + 1, false, 0);
   break;
  case 3:
   m->ter(x1 + 2, y2 - 1) = t_fridge;
   m->place_items(mi_fridge, 82, x1 + 2, y2 - 1, x1 + 2, y2 - 1, false, 0);
   break;
  case 4:
   m->ter(x2 - 2, y2 - 1) = t_fridge;
   m->place_items(mi_fridge, 82, x2 - 2, y2 - 1, x2 - 2, y2 - 1, false, 0);
   break;
  }
  break;
 case room_bedroom:
  placed = mi_bedroom;
  chance = 78;
  if (one_in(14))
   m->place_items(mi_homeguns, 58, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
  if (one_in(10))
   m->place_items(mi_home_hw,  40, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
  switch (rng(1, 5)) {
  case 1:
   m->ter(x1 + 1, y1 + 2) = t_bed;
   m->ter(x1 + 1, y1 + 3) = t_bed;
   break;
  case 2:
   m->ter(x1 + 2, y2 - 1) = t_bed;
   m->ter(x1 + 3, y2 - 1) = t_bed;
   break;
  case 3:
   m->ter(x2 - 1, y2 - 3) = t_bed;
   m->ter(x2 - 1, y2 - 2) = t_bed;
   break;
  case 4:
   m->ter(x2 - 3, y1 + 1) = t_bed;
   m->ter(x2 - 2, y1 + 1) = t_bed;
   break;
  case 5:
   m->ter(int((x1 + x2) / 2)    , y2 - 1) = t_bed;
   m->ter(int((x1 + x2) / 2) + 1, y2 - 1) = t_bed;
   m->ter(int((x1 + x2) / 2)    , y2 - 2) = t_bed;
   m->ter(int((x1 + x2) / 2) + 1, y2 - 2) = t_bed;
   break;
  }
  switch (rng(1, 4)) {
  case 1:
   _stock(*m, t_dresser, mi_dresser, 80, point(x1 + 2, y1 + 1));
   break;
  case 2:
   _stock(*m, t_dresser, mi_dresser, 80, point(x2 - 2, y2 - 1));
   break;
  case 3:
   _stock(*m, t_dresser, mi_dresser, 80, point((x1 + x2) / 2, y1 + 1));
   break;
  case 4:
   _stock(*m, t_dresser, mi_dresser, 80, point(x1 + 1, (y1 + y2) / 2));
   break;
  }
  break;
 case room_bathroom:
  m->ter(x2 - 1, y2 - 1) = t_toilet;
  m->place_items(mi_harddrugs, 18, x1 + 1, y1 + 1, x2 - 1, y2 - 2, false, 0);
  m->place_items(mi_cleaning,  48, x1 + 1, y1 + 1, x2 - 1, y2 - 2, false, 0);
  placed = mi_softdrugs;
  chance = 72;
  break;
 }
 m->place_items(placed, chance, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
}

void science_room(map *m, int x1, int y1, int x2, int y2, int rotate)
{
 int height = y2 - y1;
 int width  = x2 - x1;
 if (rotate % 2 == 1) {	// Swamp width & height if we're a lateral room
  int tmp = height;
  height  = width;
  width   = tmp;
 }
 for (int i = x1; i <= x2; i++) {
  for (int j = y1; j <= y2; j++)
   m->ter(i, j) = t_floor;
 }
 int area = height * width;
 std::vector<room_type> valid_rooms;
 if (height < 5 && width < 5)
 valid_rooms.push_back(room_closet);
 if (height > 6 && width > 3)
  valid_rooms.push_back(room_lobby);
 if (height > 4 || width > 4)
  valid_rooms.push_back(room_chemistry);
 if ((height > 7 || width > 7) && height > 2 && width > 2)
  valid_rooms.push_back(room_teleport);
 if (height > 4 && width > 4)
  valid_rooms.push_back(room_goo);
 if (height > 7 && width > 7)
  valid_rooms.push_back(room_bionics);
 if (height > 7 && width > 7)
  valid_rooms.push_back(room_cloning);
 if (area >= 9)
  valid_rooms.push_back(room_vivisect);
 if (height > 5 && width > 4)
  valid_rooms.push_back(room_dorm);
 if (width > 8) {
  for (int i = 8; i < width; i += rng(1, 2))
   valid_rooms.push_back(room_split);
 }

 room_type chosen = valid_rooms[rng(0, valid_rooms.size() - 1)];
 int trapx = rng(x1 + 1, x2 - 1);
 int trapy = rng(y1 + 1, y2 - 1);
 switch (chosen) {
  case room_closet:
   m->place_items(mi_cleaning, 80, x1, y1, x2, y2, false, 0);
   break;
  case room_lobby:
   if (rotate % 2 == 0)	{	// Vertical
    int desk = y1 + rng(int(height / 2) - int(height / 4), int(height / 2) + 1);
    for (int x = x1 + int(width / 4); x < x2 - int(width / 4); x++)
     m->ter(x, desk) = t_counter;
    computer* tmpcomp = m->add_computer(x2 - int(width / 4), desk,
                                        "Log Console", 3);
    tmpcomp->add_option("View Research Logs", COMPACT_RESEARCH, 0);
    tmpcomp->add_option("Download Map Data", COMPACT_MAPS, 0);
    tmpcomp->add_failure(COMPFAIL_SHUTDOWN);
    tmpcomp->add_failure(COMPFAIL_ALARM);
    tmpcomp->add_failure(COMPFAIL_DAMAGE);
    m->add_spawn(mon_turret, 1, int((x1 + x2) / 2), desk);
   } else {
    int desk = x1 + rng(int(height / 2) - int(height / 4), int(height / 2) + 1);
    for (int y = y1 + int(width / 4); y < y2 - int(width / 4); y++)
     m->ter(desk, y) = t_counter;
    computer* tmpcomp = m->add_computer(desk, y2 - int(width / 4),
                                        "Log Console", 3);
    tmpcomp->add_option("View Research Logs", COMPACT_RESEARCH, 0);
    tmpcomp->add_option("Download Map Data", COMPACT_MAPS, 0);
    tmpcomp->add_failure(COMPFAIL_SHUTDOWN);
    tmpcomp->add_failure(COMPFAIL_ALARM);
    tmpcomp->add_failure(COMPFAIL_DAMAGE);
    m->add_spawn(mon_turret, 1, desk, int((y1 + y2) / 2));
   }
   break;
  case room_chemistry:
   if (rotate % 2 == 0) {	// Vertical
    for (int x = x1; x <= x2; x++) {
     if (x % 3 == 0) {
      for (int y = y1 + 1; y <= y2 - 1; y++) {
       m->ter(x, y) = t_counter;
      }
      m->place_items(mi_chemistry, 70, x, y1 + 1, x, y2 - 1, false, 0);
     }
    }
   } else {
    for (int y = y1; y <= y2; y++) {
     if (y % 3 == 0) {
      for (int x = x1 + 1; x <= x2 - 1; x++) {
       m->ter(x, y) = t_counter;
      }
      m->place_items(mi_chemistry, 70, x1 + 1, y, x2 - 1, y, false, 0);
     }
    }
   }
   break;
  case room_teleport:
   m->ter(int((x1 + x2) / 2)    , int((y1 + y2) / 2)    ) = t_counter;
   m->ter(int((x1 + x2) / 2) + 1, int((y1 + y2) / 2)    ) = t_counter;
   m->ter(int((x1 + x2) / 2)    , int((y1 + y2) / 2) + 1) = t_counter;
   m->ter(int((x1 + x2) / 2) + 1, int((y1 + y2) / 2) + 1) = t_counter;
   m->add_trap(trapx, trapy, tr_telepad);
   m->place_items(mi_teleport, 70, int((x1 + x2) / 2),
                  int((y1 + y2) / 2), int((x1 + x2) / 2) + 1,
                  int((y1 + y2) / 2) + 1, false, 0);
   break;
  case room_goo:
   do {
    m->add_trap(trapx, trapy, tr_goo);
    trapx = rng(x1 + 1, x2 - 1);
    trapy = rng(y1 + 1, y2 - 1);
   } while(!one_in(5));
   if (rotate == 0) {
    m->tr_at(x1, y2) = tr_null;
    m->ter(x1, y2) = t_fridge;
    m->place_items(mi_goo, 60, x1, y2, x1, y2, false, 0);
   } else if (rotate == 1) {
    m->tr_at(x1, y1) = tr_null;
    m->ter(x1, y1) = t_fridge;
    m->place_items(mi_goo, 60, x1, y1, x1, y1, false, 0);
   } else if (rotate == 2) {
    m->tr_at(x2, y1) = tr_null;
    m->ter(x2, y1) = t_fridge;
    m->place_items(mi_goo, 60, x2, y1, x2, y1, false, 0);
   } else {
    m->tr_at(x2, y2) = tr_null;
    m->ter(x2, y2) = t_fridge;
    m->place_items(mi_goo, 60, x2, y2, x2, y2, false, 0);
   }
   break;
  case room_cloning:
   for (int x = x1 + 1; x <= x2 - 1; x++) {
    for (int y = y1 + 1; y <= y2 - 1; y++) {
     if (x % 3 == 0 && y % 3 == 0) {
      m->ter(x, y) = t_vat;
      m->place_items(mi_cloning_vat, 20, x, y, x, y, false, 0);
     }
    }
   }
   break;
  case room_vivisect:
   if        (rotate == 0) {
    for (int x = x1; x <= x2; x++)
     m->ter(x, y2 - 1) = t_counter;
    m->place_items(mi_dissection, 80, x1, y2 - 1, x2, y2 - 1, false, 0);
   } else if (rotate == 1) {
    for (int y = y1; y <= y2; y++)
     m->ter(x1 + 1, y) = t_counter;
    m->place_items(mi_dissection, 80, x1 + 1, y1, x1 + 1, y2, false, 0);
   } else if (rotate == 2) {
    for (int x = x1; x <= x2; x++)
     m->ter(x, y1 + 1) = t_counter;
    m->place_items(mi_dissection, 80, x1, y1 + 1, x2, y1 + 1, false, 0);
   } else if (rotate == 3) {
    for (int y = y1; y <= y2; y++)
     m->ter(x2 - 1, y) = t_counter;
    m->place_items(mi_dissection, 80, x2 - 1, y1, x2 - 1, y2, false, 0);
   }
   m->add_trap(int((x1 + x2) / 2), int((y1 + y2) / 2), tr_dissector);
   break;

  case room_bionics:
   if (rotate % 2 == 0) {
    int biox = x1 + 2, bioy = int((y1 + y2) / 2);
    m->ter(biox    , bioy - 1) = t_wall_h;
    m->ter(biox + 1, bioy - 1) = t_wall_h;
    m->ter(biox - 1, bioy - 1) = t_wall_h;
    m->ter(biox    , bioy + 1) = t_wall_h;
    m->ter(biox + 1, bioy + 1) = t_wall_h;
    m->ter(biox - 1, bioy + 1) = t_wall_h;
    m->ter(biox    , bioy    ) = t_counter;
    m->ter(biox + 1, bioy    ) = t_reinforced_glass_v;
    m->ter(biox - 1, bioy    ) = t_wall_v;
    m->place_items(mi_bionics_common, 70, biox, bioy, biox, bioy, false, 0);

    biox = x2 - 2;
    m->ter(biox    , bioy - 1) = t_wall_h;
    m->ter(biox - 1, bioy - 1) = t_wall_h;
    m->ter(biox + 1, bioy - 1) = t_wall_h;
    m->ter(biox    , bioy + 1) = t_wall_h;
    m->ter(biox - 1, bioy + 1) = t_wall_h;
    m->ter(biox + 1, bioy + 1) = t_wall_h;
    m->ter(biox    , bioy    ) = t_counter;
    m->ter(biox - 1, bioy    ) = t_reinforced_glass_v;
    m->ter(biox + 1, bioy    ) = t_wall_v;
    m->place_items(mi_bionics_common, 70, biox, bioy, biox, bioy, false, 0);

    int compx = int((x1 + x2) / 2), compy = int((y1 + y2) / 2);
    m->ter(compx, compy) = t_console;
    computer* tmpcomp = m->add_computer(compx, compy, "Bionic access", 2);
    tmpcomp->add_option("Manifest", COMPACT_LIST_BIONICS, 0);
    tmpcomp->add_option("Open Chambers", COMPACT_RELEASE, 3);
    tmpcomp->add_failure(COMPFAIL_MANHACKS);
    tmpcomp->add_failure(COMPFAIL_SECUBOTS);
   } else {
    int bioy = y1 + 2, biox = int((x1 + x2) / 2);
    m->ter(biox - 1, bioy    ) = t_wall_v;
    m->ter(biox - 1, bioy + 1) = t_wall_v;
    m->ter(biox - 1, bioy - 1) = t_wall_v;
    m->ter(biox + 1, bioy    ) = t_wall_v;
    m->ter(biox + 1, bioy + 1) = t_wall_v;
    m->ter(biox + 1, bioy - 1) = t_wall_v;
    m->ter(biox    , bioy    ) = t_counter;
    m->ter(biox    , bioy + 1) = t_reinforced_glass_h;
    m->ter(biox    , bioy - 1) = t_wall_h;
    m->place_items(mi_bionics_common, 70, biox, bioy, biox, bioy, false, 0);

    bioy = y2 - 2;
    m->ter(biox - 1, bioy    ) = t_wall_v;
    m->ter(biox - 1, bioy - 1) = t_wall_v;
    m->ter(biox - 1, bioy + 1) = t_wall_v;
    m->ter(biox + 1, bioy    ) = t_wall_v;
    m->ter(biox + 1, bioy - 1) = t_wall_v;
    m->ter(biox + 1, bioy + 1) = t_wall_v;
    m->ter(biox    , bioy    ) = t_counter;
    m->ter(biox    , bioy - 1) = t_reinforced_glass_h;
    m->ter(biox    , bioy + 1) = t_wall_h;
    m->place_items(mi_bionics_common, 70, biox, bioy, biox, bioy, false, 0);

    int compx = int((x1 + x2) / 2), compy = int((y1 + y2) / 2);
    m->ter(compx, compy) = t_console;
    computer* tmpcomp = m->add_computer(compx, compy, "Bionic access", 2);
    tmpcomp->add_option("Manifest", COMPACT_LIST_BIONICS, 0);
    tmpcomp->add_option("Open Chambers", COMPACT_RELEASE, 3);
    tmpcomp->add_failure(COMPFAIL_MANHACKS);
    tmpcomp->add_failure(COMPFAIL_SECUBOTS);
   }
   break;
  case room_dorm:
   if (rotate % 2 == 0) {
    for (int y = y1 + 1; y <= y2 - 1; y += 3) {
     m->ter(x1    , y) = t_bed;
     m->ter(x1 + 1, y) = t_bed;
     m->ter(x2    , y) = t_bed;
     m->ter(x2 - 1, y) = t_bed;
     _stock(*m, t_dresser, mi_dresser, 70, point(x1, y + 1));
     _stock(*m, t_dresser, mi_dresser, 70, point(x2, y + 1));
    }
   } else if (rotate % 2 == 1) {
    for (int x = x1 + 1; x <= x2 - 1; x += 3) {
     m->ter(x, y1    ) = t_bed;
     m->ter(x, y1 + 1) = t_bed;
     m->ter(x, y2    ) = t_bed;
     m->ter(x, y2 - 1) = t_bed;
     _stock(*m, t_dresser, mi_dresser, 70, point(x + 1, y1));
     _stock(*m, t_dresser, mi_dresser, 70, point(x + 1, y2));
    }
   }
   m->place_items(mi_bedroom, 84, x1, y1, x2, y2, false, 0);
   break;
  case room_split:
   if (rotate % 2 == 0) {
    int w1 = int((x1 + x2) / 2) - 2, w2 = int((x1 + x2) / 2) + 2;
    for (int y = y1; y <= y2; y++) {
     m->ter(w1, y) = t_wall_v;
     m->ter(w2, y) = t_wall_v;
    }
    m->ter(w1, int((y1 + y2) / 2)) = t_door_metal_c;
    m->ter(w2, int((y1 + y2) / 2)) = t_door_metal_c;
    science_room(m, x1, y1, w1 - 1, y2, 1);
    science_room(m, w2 + 1, y1, x2, y2, 3);
   } else {
    int w1 = int((y1 + y2) / 2) - 2, w2 = int((y1 + y2) / 2) + 2;
    for (int x = x1; x <= x2; x++) {
     m->ter(x, w1) = t_wall_h;
     m->ter(x, w2) = t_wall_h;
    }
    m->ter(int((x1 + x2) / 2), w1) = t_door_metal_c;
    m->ter(int((x1 + x2) / 2), w2) = t_door_metal_c;
    science_room(m, x1, y1, x2, w1 - 1, 2);
    science_room(m, x1, w2 + 1, x2, y2, 0);
   }
   break;
 }
}

void set_science_room(map *m, int x1, int y1, bool faces_right, int turn)
{
// TODO: More types!
 int type = rng(0, 4);
 int x2 = x1 + 7;
 int y2 = y1 + 4;
 switch (type) {
 case 0:	// Empty!
  return;
 case 1: // Chemistry.
// #######.
// #.......
// #.......
// #.......
// #######.
  for (int i = x1; i <= x2; i++) {
   for (int j = y1; j <= y2; j++) {
    if ((i == x1 || j == y1 || j == y2) && i != x1)
     m->ter(i, j) = t_counter;
   }
  }
  m->place_items(mi_chemistry,	85, x1 + 1, y1, x2 - 1, y1, false, 0);
  m->place_items(mi_chemistry,	85, x1 + 1, y2, x2 - 1, y2, false, 0);
  m->place_items(mi_chemistry,	85, x1, y1 + 1, x1, y2 - 1, false, 0);
  break;

 case 2: // Hydroponics.
// #.......
// #.~~~~~.
// #.......
// #.~~~~~.
// #.......
  for (int i = x1; i <= x2; i++) {
   for (int j = y1; j <= y2; j++) {
    if (i == x1)
     m->ter(i, j) = t_counter;
    else if (i > x1 + 1 && i < x2 && (j == y1 + 1 || j == y2 - 1))
     m->ter(i, j) = t_water_sh;
   }
  }
  m->place_items(mi_chemistry,	80, x1, y1, x1, y2, false, turn - 50);
  m->place_items(mi_hydro,	92, x1 + 1, y1 + 1, x2 - 1, y1 + 1, false,turn);
  m->place_items(mi_hydro,	92, x1 + 1, y2 - 1, x2 - 1, y2 - 1, false,turn);
  break;

 case 3: // Electronics.
// #######.
// #.......
// #.......
// #.......
// #######.
  for (int i = x1; i <= x2; i++) {
   for (int j = y1; j <= y2; j++) {
    if ((i == x1 || j == y1 || j == y2) && i != x1)
     m->ter(i, j) = t_counter;
   }
  }
  m->place_items(mi_electronics,85, x1 + 1, y1, x2 - 1, y1, false, turn - 50);
  m->place_items(mi_electronics,85, x1 + 1, y2, x2 - 1, y2, false, turn - 50);
  m->place_items(mi_electronics,85, x1, y1 + 1, x1, y2 - 1, false, turn - 50);
  break;

 case 4: // Monster research.
// .|.####.
// -|......
// .|......
// -|......
// .|.####.
  for (int i = x1; i <= x2; i++) {
   for (int j = y1; j <= y2; j++) {
    if (i == x1 + 1)
     m->ter(i, j) = t_wall_glass_v;
    else if (i == x1 && (j == y1 + 1 || j == y2 - 1))
     m->ter(i, j) = t_wall_glass_h;
    else if ((j == y1 || j == y2) && i >= x1 + 3 && i <= x2 - 1)
     m->ter(i, j) = t_counter;
   }
  }
// TODO: Place a monster in the sealed areas.
  m->place_items(mi_monparts,	70, x1 + 3, y1, 2 - 1, y1, false, turn - 100);
  m->place_items(mi_monparts,	70, x1 + 3, y2, 2 - 1, y2, false, turn - 100);
  break;
 }

 if (!faces_right) { // Flip it.
  ter_id rotated[SEEX*2][SEEY*2];
  std::vector<item> itrot[SEEX*2][SEEY*2];
  for (int i = x1; i <= x2; i++) {
   for (int j = y1; j <= y2; j++) {
    rotated[i][j] = m->ter(i, j);
    itrot[i][j] = m->i_at(i, j);
   }
  }
  for (int i = x1; i <= x2; i++) {
   for (int j = y1; j <= y2; j++) {
    m->ter(i, j) = rotated[x2 - (i - x1)][j];
    m->i_at(i, j) = itrot[x2 - (i - x1)][j];
   }
  }
 }
}

void silo_rooms(map *m)
{
 static constexpr const std::pair<items_location,int> primary_room_stock[] = {
   std::pair(mi_cannedfood,2),
   std::pair(mi_allguns,2),
   std::pair(mi_tools,2),
   std::pair(mi_allclothes,1),
   std::pair(mi_manuals,1),
   std::pair(mi_electronics,3),
   std::pair(mi_survival_tools,1),
   std::pair(mi_radio,2)
 };
 static constexpr const std::pair<items_location,int> secondary_room_stock[] = {
   std::pair(mi_fridge, 2),
   std::pair(mi_ammo, 2),
   std::pair(mi_none, 10),
 };

 std::vector<point> rooms;
 std::vector<point> room_sizes;
 bool okay = true;
 do {
  int x, y, height, width;
  if (one_in(2)) {	// True = top/bottom, False = left/right
   x = rng(0, SEEX * 2 - 6);
   y = rng(0, 4);
   if (one_in(2))
    y = SEEY * 2 - 2 - y;	// Bottom of the screen, not the top
   width  = rng(2, 5);
   height = 2;
   clamp_ub(width, SEEX * 2 - 2 - x);	// Make sure our room isn't too wide
  } else {
   x = rng(0, 4);
   y = rng(0, SEEY * 2 - 6);
   if (one_in(2))
    x = SEEX * 2 - 2 - x;	// Right side of the screen, not the left
   width  = 2;
   height = rng(2, 5);
   clamp_ub(height, SEEY * 2 - 2 - y);	// Make sure our room isn't too tall
  }
  if (!rooms.empty() &&	// We need at least one room!
      (m->ter(x, y) != t_rock || m->ter(x + width, y + height) != t_rock))
   okay = false;
  else {
   rooms.push_back(point(x, y));
   room_sizes.push_back(point(width, height));
   for (int i = x; i <= x + width; i++) {
    for (int j = y; j <= y + height; j++) {
	 m->rewrite<t_rock, t_floor>(i,j);
    }
   }
   const auto index = rng(0, 13);	// What type of items go here?
   if (auto used = use_rarity_table(index, std::begin(primary_room_stock), std::end(primary_room_stock)))
     m->place_items(used, 78, x, y, x + width, y + height, false, 0);
   if (auto used = use_rarity_table(index, std::begin(secondary_room_stock), std::end(secondary_room_stock)))
     m->place_items(used, 78, x, y, x + width, y + height, false, 0);
  }
 } while (okay);

 m->ter(rooms[0]) = t_stairs_up;
 int down_room = rng(0, rooms.size() - 1);
 point dp = rooms[down_room], ds = room_sizes[down_room];
 m->ter(dp + ds) = t_stairs_down;
 rooms.push_back(point(SEEX, SEEY)); // So the center circle gets connected
 room_sizes.push_back(point(5, 5));

 while (rooms.size() > 1) {
  int best_dist = 999, closest = 0;
  for (int i = 1; i < rooms.size(); i++) {
   int dist = trig_dist(rooms[0], rooms[i]);
   if (dist < best_dist) {
    best_dist = dist;
    closest = i;
   }
  }
// We chose the closest room; now draw a corridor there
  point origin = rooms[0], origsize = room_sizes[0], dest = rooms[closest];
  int x = origin.x + origsize.x, y = origin.y + origsize.y;
  bool x_first = (abs(origin.x - dest.x) > abs(origin.y - dest.y));
  while (x != dest.x || y != dest.y) {
   m->rewrite<t_rock, t_floor>(x, y);
   if ((x_first && x != dest.x) || (!x_first && y == dest.y)) {
    if (dest.x < x) x--;
    else x++;
   } else {
    if (dest.y < y) y--;
    else y++;
   }
  }
  rooms.erase(rooms.begin());
  room_sizes.erase(room_sizes.begin());
 }
}

void build_mine_room(map *m, room_type type, int x1, int y1, int x2, int y2)
{
 direction door_side;
 std::vector<direction> possibilities;
 int midx = int( (x1 + x2) / 2), midy = int( (y1 + y2) / 2);
 if (x2 < SEEX)
  possibilities.push_back(EAST);
 if (x1 > SEEX + 1)
  possibilities.push_back(WEST);
 if (y1 > SEEY + 1)
  possibilities.push_back(NORTH);
 if (y2 < SEEY)
  possibilities.push_back(SOUTH);

 if (possibilities.empty()) { // We're in the middle of the map!
  if (midx <= SEEX)
   possibilities.push_back(EAST);
  else
   possibilities.push_back(WEST);
  if (midy <= SEEY)
   possibilities.push_back(SOUTH);
  else
   possibilities.push_back(NORTH);
 }

 door_side = possibilities[rng(0, possibilities.size() - 1)];
 point door_point;
 switch (door_side) {
  case NORTH:
   door_point.x = midx;
   door_point.y = y1;
   break;
  case EAST:
   door_point.x = x2;
   door_point.y = midy;
   break;
  case SOUTH:
   door_point.x = midx;
   door_point.y = y2;
   break;
  case WEST:
   door_point.x = x1;
   door_point.y = midy;
   break;
 }
 square(m, t_floor, x1, y1, x2, y2);
 line(m, t_wall_h, x1, y1, x2, y1);
 line(m, t_wall_h, x1, y2, x2, y2);
 line(m, t_wall_v, x1, y1 + 1, x1, y2 - 1);
 line(m, t_wall_v, x2, y1 + 1, x2, y2 - 1);
// Main build switch!
 switch (type) {
  case room_mine_shaft: {
   m->ter(x1 + 1, y1 + 1) = t_console;
   line(m, t_wall_h, x2 - 2, y1 + 2, x2 - 1, y1 + 2);
   m->ter(x2 - 2, y1 + 1) = t_elevator;
   m->ter(x2 - 1, y1 + 1) = t_elevator_control_off;
   computer* tmpcomp = m->add_computer(x1 + 1, y1 + 1, "NEPowerOS", 2);
   tmpcomp->add_option("Divert power to elevator", COMPACT_ELEVATOR_ON, 0);
   tmpcomp->add_failure(COMPFAIL_ALARM);
  } break;

  case room_mine_office:
   line(m, t_counter, midx, y1 + 2, midx, y2 - 2);
   line(m, t_window, midx - 1, y1, midx + 1, y1);
   line(m, t_window, midx - 1, y2, midx + 1, y2);
   line(m, t_window, x1, midy - 1, x1, midy + 1);
   line(m, t_window, x2, midy - 1, x2, midy + 1);
   m->place_items(mi_office, 80, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
   break;

  case room_mine_storage:
   m->place_items(mi_mine_storage,85, x1 + 2, y1 + 2, x2 - 2, y2 - 2, false, 0);
   break;

  case room_mine_fuel: {
   int spacing = rng(2, 4);
   if (door_side == NORTH || door_side == SOUTH) {
    int y = (door_side == NORTH ? y1 + 2 : y2 - 2);
    for (int x = x1 + 1; x <= x2 - 1; x += spacing)
     m->ter(x, y) = t_gas_pump;
   } else {
    int x = (door_side == EAST ? x2 - 2 : x1 + 2);
    for (int y = y1 + 1; y <= y2 - 1; y += spacing)
     m->ter(x, y) = t_gas_pump;
   }
  } break;

  case room_mine_housing:
   if (door_side == NORTH || door_side == SOUTH) {
    for (int y = y1 + 2; y <= y2 - 2; y += 2) {
     m->ter(x1    , y) = t_window;
     m->ter(x1 + 1, y) = t_bed;
     m->ter(x1 + 2, y) = t_bed;
     m->ter(x2    , y) = t_window;
     m->ter(x2 - 1, y) = t_bed;
     m->ter(x2 - 2, y) = t_bed;
     _stock(*m, t_dresser, mi_dresser, 78, point(x1 + 1, y + 1));
     _stock(*m, t_dresser, mi_dresser, 78, point(x2 - 1, y + 1));
    }
   } else {
    for (int x = x1 + 2; x <= x2 - 2; x += 2) {
     m->ter(x, y1    ) = t_window;
     m->ter(x, y1 + 1) = t_bed;
     m->ter(x, y1 + 2) = t_bed;
     m->ter(x, y2    ) = t_window;
     m->ter(x, y2 - 1) = t_bed;
     m->ter(x, y2 - 2) = t_bed;
     _stock(*m, t_dresser, mi_dresser, 78, point(x + 1, y1 + 1));
     _stock(*m, t_dresser, mi_dresser, 78, point(x + 1, y2 - 1));
    }
   }
   m->place_items(mi_bedroom, 65, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
   break;
 }

 if (type == room_mine_fuel) { // Fuel stations are open on one side
  switch (door_side) {
   case NORTH: line(m, t_floor, x1, y1    , x2, y1    ); break;
   case EAST:  line(m, t_floor, x2, y1 + 1, x2, y2 - 1); break;
   case SOUTH: line(m, t_floor, x1, y2    , x2, y2    ); break;
   case WEST:  line(m, t_floor, x1, y1 + 1, x1, y2 - 1); break;
  }
 } else {
  m->ter(door_point) = (type == room_mine_storage) ? t_door_locked  // Storage has a locked door
	                                               : t_door_c;
 }
}

static room_type pick_mansion_room(int x1, int y1, int x2, int y2)
{
 int dx = abs(x1 - x2), dy = abs(y1 - y2), area = dx * dy;
 int shortest = (dx < dy ? dx : dy), longest = (dx > dy ? dx : dy);
 std::vector<room_type> valid;
 if (6 <= shortest) {
     if (area <= 60) valid.push_back(room_mansion_kitchen);
     if (longest <= 10) valid.push_back(room_mansion_game);
     if (7 <= shortest) {
         if (area >= 64 && area <= 100) valid.push_back(room_mansion_bedroom);
         if (9 <= shortest) {
             valid.push_back(room_mansion_library);
             if (10 <= shortest) {
                 valid.push_back(room_mansion_pool);
                 if (shortest >= 12) valid.push_back(room_mansion_courtyard);
             }
         }
     }
 }
 valid.push_back((longest >= 7 && shortest >= 5) ? room_mansion_dining : room_mansion_bathroom);    // forces non-empty
 if (longest >= 8 && shortest <= 6) valid.push_back(room_mansion_gallery);

 return valid[ rng(0, valid.size() - 1) ];
}

static void add_knight_suit(map& m, const point& pt)
{
    m.add_item(pt.x, pt.y, item::types[itm_helmet_plate], 0);
    m.add_item(pt.x, pt.y, item::types[itm_armor_plate], 0);
    if (one_in(2)) m.add_item(pt.x, pt.y, item::types[itm_pike], 0);
    else if (one_in(3)) m.add_item(pt.x, pt.y, item::types[itm_broadsword], 0);
    else if (one_in(6)) m.add_item(pt.x, pt.y, item::types[itm_mace], 0);
    else if (one_in(6)) m.add_item(pt.x, pt.y, item::types[itm_morningstar], 0);
}

void build_mansion_room(map *m, room_type type, int x1, int y1, int x2, int y2)
{
 int dx = abs(x1 - x2), dy = abs(y1 - y2), area = dx * dy;
 int cx_low = (x1 + x2) / 2, cx_hi = (x1 + x2 + 1) / 2,
     cy_low = (y1 + y2) / 2, cy_hi = (y1 + y2 + 1) / 2;

 bool walled_west = (x1 <= 1),            walled_north = (y1 == 0),
      walled_east = (x2 == SEEX * 2 - 1), walled_south = (y2 >= SEEY * 2 - 2);

 switch (type) {

 case room_mansion_courtyard:
  for (int x = x1; x <= x2; x++) {
   for (int y = y1; y <= y2; y++)
    m->ter(x, y) = grass_or_dirt();
  }
  if (one_in(4)) { // Tree grid
   for (int x = 1; x <= dx / 2; x += 4) {
    for (int y = 1; y <= dx / 2; y += 4) {
     m->ter(x1 + x, y1 + y) = t_tree;
     m->ter(x2 - x, y2 - y) = t_tree;
    }
   }
  }
  if (one_in(3)) { // shrub-lined
   for (int i = x1; i <= x2; i++) {
    if (m->ter(i, y2 + 1) != t_door_c)
     m->ter(i, y2) = t_shrub;
   }
   if (walled_south && x1 <= SEEX && SEEX <= x2) {
    m->ter(SEEX - 1, y2) = grass_or_dirt();
    m->ter(SEEX,     y2) = grass_or_dirt();
   }
  }
  break;

 case room_mansion_entry:
  if (!one_in(3)) { // Columns
   for (int y = y1 + 2; y <= y2; y += 3) {
    m->ter(cx_low - 3, y) = t_column;
    m->ter(cx_low + 3, y) = t_column;
   }
  }
  if (one_in(6)) { // Suits of armor
   int start = y1 + rng(2, 4), end = y2 - rng(0, 4), step = rng(3, 6);
   for (int y = start; y <= end; y += step) {
    add_knight_suit(*m, point(x1 + 1, y));
    add_knight_suit(*m, point(x2 - 1, y));
   }
  }
  break;
  
 case room_mansion_bedroom:
  if (dx > dy || (dx == dy && one_in(2))) { // horizontal
   int dressy = (one_in(2) ? cy_low - 2 : cy_low + 2);
   if (one_in(2)) { // bed on left
    square(m, t_bed, x1 + 1, cy_low - 1, x1 + 3, cy_low + 1);
    _stock(*m, t_dresser, mi_dresser, 80, point(x1 + 1, dressy));
   } else { // bed on right
    square(m, t_bed, x2 - 3, cy_low - 1, x2 - 1, cy_low + 1);
    _stock(*m, t_dresser, mi_dresser, 80, point(x2 - 1, dressy));
   }
  } else { // vertical
   int dressx = (one_in(2) ? cx_low - 2 : cx_low + 2);
   if (one_in(2)) { // bed at top
    square(m, t_bed, cx_low - 1, y1 + 1, cx_low + 1, y1 + 3);
    _stock(*m, t_dresser, mi_dresser, 80, point(dressx, y2 + 1));
   } else { // bed at bottom
    square(m, t_bed, cx_low - 1, y2 - 3, cx_low + 1, y2 - 1);
    _stock(*m, t_dresser, mi_dresser, 80, point(dressx, y2 - 1));
   }
  }
  m->place_items(mi_bedroom, 75, x1, y1, x2, y2, false, 0);
  if (one_in(10))
   m->place_items(mi_homeguns, 58, x1, y1, x2, y2, false, 0);
  break;

 case room_mansion_library:
  if (dx < dy || (dx == dy && one_in(2))) { // vertically-aligned bookshelves
   for (int x = x1 + 1; x <= cx_low - 2; x += 3) {
    for (int y = y1 + 1; y <= y2 - 3; y += 4) {
     square(m, t_bookcase, x, y, x + 1, y + 2);
     m->place_items(mi_novels,    85, x, y, x + 1, y + 2, false, 0);
     m->place_items(mi_manuals,   62, x, y, x + 1, y + 2, false, 0);
     m->place_items(mi_textbooks, 40, x, y, x + 1, y + 2, false, 0);
    }
   }
   for (int x = x2 - 1; x >= cx_low + 2; x -= 3) {
    for (int y = y1 + 1; y <= y2 - 3; y += 4) {
     square(m, t_bookcase, x - 1, y, x, y + 2);
     m->place_items(mi_novels,    85, x - 1, y, x, y + 2, false, 0);
     m->place_items(mi_manuals,   62, x - 1, y, x, y + 2, false, 0);
     m->place_items(mi_textbooks, 40, x - 1, y, x, y + 2, false, 0);
    }
   }
  } else { // horizontally-aligned bookshelves
   for (int y = y1 + 1; y <= cy_low - 2; y += 3) {
    for (int x = x1 + 1; x <= x2 - 3; x += 4) {
     square(m, t_bookcase, x, y, x + 2, y + 1);
     m->place_items(mi_novels,    85, x, y, x + 2, y + 1, false, 0);
     m->place_items(mi_manuals,   62, x, y, x + 2, y + 1, false, 0);
     m->place_items(mi_textbooks, 40, x, y, x + 2, y + 1, false, 0);
    }
   }
   for (int y = y2 - 1; y >= cy_low + 2; y -= 3) {
    for (int x = x1 + 1; x <= x2 - 3; x += 4) {
     square(m, t_bookcase, x, y - 1, x + 2, y);
     m->place_items(mi_novels,    85, x, y - 1, x + 2, y, false, 0);
     m->place_items(mi_manuals,   62, x, y - 1, x + 2, y, false, 0);
     m->place_items(mi_textbooks, 40, x, y - 1, x + 2, y, false, 0);
    }
   }
  }
  break;

 case room_mansion_kitchen:
  square(m, t_counter, cx_low, cy_low, cx_hi, cy_hi);
  m->place_items(mi_cleaning,  58, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
  if (one_in(2)) { // Fridge/racks on left/right
   line(m, t_fridge, cx_low - 1, cy_low, cx_low - 1, cy_hi);
   m->place_items(mi_fridge, 82, cx_low - 1, cy_low, cx_low - 1, cy_hi,
                  false, 0);
   line(m, t_rack, cx_hi + 1, cy_low, cx_hi + 1, cy_hi);
   m->place_items(mi_cannedfood, 50, cx_hi + 1, cy_low, cx_hi + 1, cy_hi,
                  false, 0);
   m->place_items(mi_pasta,      50, cx_hi + 1, cy_low, cx_hi + 1, cy_hi,
                  false, 0);
  } else { // Fridge/racks on top/bottom
   line(m, t_fridge, cx_low, cy_low - 1, cx_hi, cy_low - 1);
   m->place_items(mi_fridge, 82, cx_low, cy_low - 1, cx_hi, cy_low - 1,
                  false, 0);
   line(m, t_rack, cx_low, cy_hi + 1, cx_hi, cy_hi + 1);
   m->place_items(mi_cannedfood, 50, cx_low, cy_hi + 1, cx_hi, cy_hi + 1,
                  false, 0);
   m->place_items(mi_pasta,      50, cx_low, cy_hi + 1, cx_hi, cy_hi + 1,
                  false, 0);
  }
  break;
   
 case room_mansion_dining:
  if (dx < dy || (dx == dy && one_in(2))) { // vertically-aligned table
   line(m, t_table, cx_low, y1 + 2, cx_low, y2 - 2);
   line(m, t_bench, cx_low - 1, y1 + 2, cx_low - 1, y2 - 2);
   line(m, t_bench, cx_low + 1, y1 + 2, cx_low + 1, y2 - 2);
   m->place_items(mi_dining, 78, cx_low, y1 + 2, cx_low, y2 - 2, false, 0);
  } else { // horizontally-aligned table
   line(m, t_table, x1 + 2, cy_low, x2 - 2, cy_low);
   line(m, t_bench, x1 + 2, cy_low - 1, x2 - 2, cy_low - 1);
   line(m, t_bench, x1 + 2, cy_low + 1, x2 - 2, cy_low + 1);
   m->place_items(mi_dining, 78, x1 + 2, cy_low, x2 - 2, cy_low, false, 0);
  }
  break;

 case room_mansion_game:
  if (dx < dy || one_in(2)) { // vertically-aligned table
   square(m, t_pool_table, cx_low, cy_low - 1, cx_low + 1, cy_low + 1);
   m->place_items(mi_pool_table, 80, cx_low, cy_low - 1, cx_low + 1, cy_low + 1,
                  false, 0);
  } else { // horizontally-aligned table
   square(m, t_pool_table, cx_low - 1, cy_low, cx_low + 1, cy_low + 1);
   m->place_items(mi_pool_table, 80, cx_low - 1, cy_low, cx_low + 1, cy_low + 1,
                  false, 0);
  }
  break;

 case room_mansion_pool:
  square(m, t_water_sh, x1 + 2, y1 + 2, x2 - 2, y2 - 2);
  break;

 case room_mansion_bathroom:
  m->ter( rng(x1 + 1, x2 - 1), rng(y1 + 1, y2 - 1) ) = t_toilet;
  m->place_items(mi_harddrugs, 20, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
  m->place_items(mi_softdrugs, 72, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
  m->place_items(mi_cleaning,  48, x1 + 1, y1 + 1, x2 - 1, y2 - 1, false, 0);
  break;
  
 case room_mansion_gallery:
  for (int x = x1 + 1; x <= cx_low - 1; x += rng(2, 4)) {
   for (int y = y1 + 1; y <= cy_low - 1; y += rng(2, 4)) {
    if (one_in(10)) add_knight_suit(*m, point(x,y)); // Suit of armor
    else { // Objets d'art
     m->ter(x, y) = t_counter;
     m->place_items(mi_art, 70, x, y, x, y, false, 0);
    }
   }
  }
  for (int x = x2 - 1; x >= cx_hi + 1; x -= rng(2, 4)) {
   for (int y = y2 - 1; y >= cy_hi + 1; y -= rng(2, 4)) {
    if (one_in(10)) add_knight_suit(*m, point(x,y)); // Suit of armor
    else { // Objets d'art
     m->ter(x, y) = t_counter;
     m->place_items(mi_art, 70, x, y, x, y, false, 0);
    }
   }
  }
  break;

 }
}

void mansion_room(map *m, int x1, int y1, int x2, int y2)
{
 build_mansion_room(m, pick_mansion_room(x1, y1, x2, y2), x1, y1, x2, y2);
}

void map::add_extra(map_extra type, game *g)
{
 item body(messages.turn);
 
 switch (type) {

 case mx_null:
  debugmsg("Tried to generate null map extra.");
  break;

 case mx_helicopter:
 {
  int cx = rng(4, SEEX * 2 - 5), cy = rng(4, SEEY * 2 - 5);
  for (int x = 0; x < SEEX * 2; x++) {
   for (int y = 0; y < SEEY * 2; y++) {
    if (x >= cx - 4 && x <= cx + 4 && y >= cy - 4 && y <= cy + 4) {
     if (!one_in(5))
      ter(x, y) = t_wreckage;
     else if (has_flag(bashable, x, y)) {
      bash(x, y, 500);	// Smash the fuck out of it
      bash(x, y, 500);	// Smash the fuck out of it some more
     }
    } else if (one_in(10))	// 1 in 10 chance of being wreckage anyway
     ter(x, y) = t_wreckage;
   }
  }

  place_items(mi_helicopter, 90, cx - 4, cy - 4, cx + 4, cy + 4, true, 0);
  place_items(mi_helicopter, 20, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
  items_location extra_items = mi_helicopter;
  switch (rng(1, 4)) {
   case 1: extra_items = mi_military;	break;
   case 2: extra_items = mi_science;	break;
   case 3: extra_items = mi_allguns;	break;
   case 4: extra_items = mi_bionics;	break;
  }
  place_items(extra_items, 70, cx - 4, cy - 4, cx + 4, cy + 4, true, 0);
 }
 break;

 case mx_military:
 {
  int num_bodies = dice(2, 6);
  for (int i = 0; i < num_bodies; i++) {
   int x, y, tries = 0;;
   do {	// Loop until we find a valid spot to dump a body, or we give up
    x = rng(0, SEEX * 2 - 1);
    y = rng(0, SEEY * 2 - 1);
    tries++;
   } while (tries < 10 && move_cost(x, y) == 0);

   if (tries < 10) {	// We found a valid spot!
    add_item(x, y, body);
    place_items(mi_military, 86, x, y, x, y, true, 0);
    if (one_in(8)) add_item(x, y, item::types[itm_id_military], 0);
   }
  }
  place_items(mi_rare, 25, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
 }
 break;

 case mx_science:
 {
  int num_bodies = dice(2, 5);
  for (int i = 0; i < num_bodies; i++) {
   int x, y, tries = 0;
   do {	// Loop until we find a valid spot to dump a body, or we give up
    x = rng(0, SEEX * 2 - 1);
    y = rng(0, SEEY * 2 - 1);
    tries++;
   } while (tries < 10 && move_cost(x, y) == 0);

   if (tries < 10) {	// We found a valid spot!
    add_item(x, y, body);
    add_item(x, y, item::types[itm_id_science], 0);
    place_items(mi_science, 84, x, y, x, y, true, 0);
   }
  }
  place_items(mi_rare, 45, 0, 0, SEEX * 2 - 1, SEEY * 2 - 1, true, 0);
 }
 break;

 case mx_stash:
 {
  int x = rng(0, SEEX * 2 - 1), y = rng(0, SEEY * 2 - 1);
  if (move_cost(x, y) != 0) ter(x, y) = t_dirt;

  int size;
  items_location stash;
  switch (rng(1, 6)) {	// What kind of stash?
   case 1: stash = mi_stash_food;	size = 90;	break;
   case 2: stash = mi_stash_ammo;	size = 80;	break;
   case 3: stash = mi_rare;		size = 70;	break;
   case 4: stash = mi_stash_wood;	size = 90;	break;
   case 5: stash = mi_stash_drugs;	size = 85;	break;
   case 6: stash = mi_trash;		size = 92;	break;
  }

  if (move_cost(x, y) == 0) ter(x, y) = t_dirt;
  place_items(stash, size, x, y, x, y, true, 0);

// Now add traps around that stash
  for (int i = x - 4; i <= x + 4; i++) {
   for (int j = y - 4; j <= y + 4; j++) {
    if (i >= 0 && j >= 0 && i < SEEX * 2 && j < SEEY * 2 && one_in(4)) {
     trap_id placed;
     switch (rng(1, 7)) {
      case 1:
      case 2:
      case 3: placed = tr_beartrap;	break;
      case 4:
      case 5: placed = tr_nailboard;	break;
      case 6: placed = tr_crossbow;	break;
      case 7: placed = tr_shotgun_2;	break;
     }
     if (placed == tr_beartrap && has_flag(diggable, i, j)) {
	  placed = one_in(8) ? tr_landmine : tr_beartrap_buried;
     }
     add_trap(i, j,  placed);
    }
   }
  }
 }
 break;

 case mx_drugdeal: {
// Decide on a drug type
  int num_drugs;
  itype* drugtype = NULL;
  switch (rng(1, 10)) {
   case 1: // Weed
    num_drugs = rng(20, 30);
    drugtype = item::types[itm_weed];
    break;
   case 2:
   case 3:
   case 4:
   case 5: // Cocaine
    num_drugs = rng(10, 20);
    drugtype = item::types[itm_coke];
    break;
   case 6:
   case 7:
   case 8: // Meth
    num_drugs = rng(8, 14);
    drugtype = item::types[itm_meth];
    break;
   case 9:
   case 10: // Heroin
    num_drugs = rng(6, 12);
    drugtype = item::types[itm_heroin];
    break;
  }
  if (!drugtype) return;	// XXX invariant violation

  int num_bodies_a = dice(3, 3);
  int num_bodies_b = dice(3, 3);
  bool north_south = one_in(2);
  bool a_has_drugs = one_in(2);
  for (int i = 0; i < num_bodies_a; i++) {
   int x, y, tries = 0;
   do {	// Loop until we find a valid spot to dump a body, or we give up
    if (north_south) {
     x = rng(0, SEEX * 2 - 1);
     y = rng(0, SEEY - 4);
    } else {
     x = rng(0, SEEX - 4);
     y = rng(0, SEEY * 2 - 1);
    }
    tries++;
   } while (tries < 10 && move_cost(x, y) == 0);

   if (tries < 10) {	// We found a valid spot!
    add_item(x, y, body);
    place_items(mi_drugdealer, 75, x, y, x, y, true, 0);
    if (a_has_drugs && num_drugs > 0) {
     int drugs_placed = rng(2, 6);
     if (drugs_placed > num_drugs) {
      drugs_placed = num_drugs;
      num_drugs = 0;
     }
     for (int n = 0; n < drugs_placed; n++)
      add_item(x, y, drugtype, 0);
    }
   }
  }
  for (int i = 0; i < num_bodies_b; i++) {
   int x, y, tries = 0;
   do {	// Loop until we find a valid spot to dump a body, or we give up
    if (north_south) {
     x = rng(0, SEEX * 2 - 1);
     y = rng(SEEY + 3, SEEY * 2 - 1);
    } else {
     x = rng(SEEX + 3, SEEX * 2 - 1);
     y = rng(0, SEEY * 2 - 1);
    }
    tries++;
   } while (tries < 10 && move_cost(x, y) == 0);

   if (tries < 10) {	// We found a valid spot!
    add_item(x, y, body);
    place_items(mi_drugdealer, 75, x, y, x, y, true, 0);
    if (!a_has_drugs && num_drugs > 0) {
     int drugs_placed = rng(2, 6);
     if (drugs_placed > num_drugs) {
      drugs_placed = num_drugs;
      num_drugs = 0;
     }
     for (int n = 0; n < drugs_placed; n++)
      add_item(x, y, drugtype, 0);
    }
   }
  }
 } break;
  
 case mx_supplydrop: {
  int num_crates = rng(1, 5);
  for (int i = 0; i < num_crates; i++) {
   int x, y, tries = 0;
   do {	// Loop until we find a valid spot to dump a body, or we give up
    x = rng(0, SEEX * 2 - 1);
    y = rng(0, SEEY * 2 - 1);
    tries++;
   } while (tries < 10 && move_cost(x, y) == 0);
   ter(x, y) = t_crate_c;
   switch (rng(1, 10)) {
    case 1:
    case 2:
    case 3:
    case 4:
     place_items(mi_mil_food, 88, x, y, x, y, true, 0);
     break;
    case 5:
    case 6:
    case 7:
     place_items(mi_grenades, 82, x, y, x, y, true, 0);
     break;
    case 8:
    case 9:
     place_items(mi_mil_armor, 75, x, y, x, y, true, 0);
     break;
    case 10:
     place_items(mi_mil_rifles, 80, x, y, x, y, true, 0);
     break;
   }
  }
 }
 break;

 case mx_portal:
 {
  int x = rng(1, SEEX * 2 - 2), y = rng(1, SEEY * 2 - 2);
  for (int i = x - 1; i <= x + 1; i++) {
   for (int j = y - 1; j <= y + 1; j++)
    ter(i, j) = t_rubble;
  }
  add_trap(x, y, tr_portal);
  int num_monsters = rng(0, 4);
  for (int i = 0; i < num_monsters; i++) {
   mon_id type = mon_id( rng(mon_gelatin, mon_blank) );
   int mx = rng(1, SEEX * 2 - 2), my = rng(1, SEEY * 2 - 2);
   ter(mx, my) = t_rubble;
   add_spawn(type, 1, mx, my);
  }
 }
 break;

 case mx_minefield:
 {
  int num_mines = rng(6, 20);
  for (int x = 0; x < SEEX * 2; x++) {
   for (int y = 0; y < SEEY * 2; y++) {
    if (one_in(3))
     ter(x, y) = t_dirt;
   }
  }
  for (int i = 0; i < num_mines; i++) {
   int x = rng(0, SEEX * 2 - 1), y = rng(0, SEEY * 2 - 1);
   if (!has_flag(diggable, x, y) || one_in(4))
    ter(x, y) = t_dirtmound;
   add_trap(x, y, tr_landmine);
  }
 }
 break;

 case mx_wolfpack:
  add_spawn(mon_wolf, rng(3, 6), SEEX, SEEY);
  break;

 case mx_puddle:
 {
  int x = rng(6, SEEX * 2 - 7), y = rng(6, SEEY * 2 - 7);
  for (int dist = 0; dist < 6; dist++) {
   for (int px = x - dist; px <= x + dist; px++) {
    for (int py = y - dist; py <= y + dist; py++) {
     if (rng(0, 8) > dist)
      ter(px, py) = t_water_sh;
    }
   }
  }
 }
 break;

 case mx_crater:
 {
  int size = rng(2, 6);
  int x = rng(size, SEEX * 2 - 1 - size), y = rng(size, SEEY * 2 - 1 - size);
  for (int i = x - size; i <= x + size; i++) {
   for (int j = y - size; j <= y + size; j++) {
    ter(i, j) = t_rubble;
    radiation(i, j) += rng(20, 40);
   }
  }
 }
 break;

 case mx_fumarole:
 {
  int x1 = rng(0,    SEEX     - 1), y1 = rng(0,    SEEY     - 1),
      x2 = rng(SEEX, SEEX * 2 - 1), y2 = rng(SEEY, SEEY * 2 - 1);
  line(this, t_lava, x1, y1, x2, y2);
 }
 break;

 case mx_portal_in:
 {
  int x = rng(5, SEEX * 2 - 6), y = rng(5, SEEY * 2 - 6);
  add_field(g, x, y, fd_fatigue, 3);
  for (int i = x - 5; i <= x + 5; i++) {
   for (int j = y - 5; j <= y + 5; j++) {
    if (rng(0, 9) > trig_dist(x, y, i, j)) {
     marlossify(i, j);
     if (ter(i, j) == t_marloss) add_item(x, y, item::types[itm_marloss_berry], messages.turn);
     if (one_in(15)) g->z.push_back(monster(mtype::types[mon_id(rng(mon_gelatin, mon_blank))], i, j));
    }
   }
  }
 }
 break;

 case mx_anomaly: {
  point center( rng(6, SEEX * 2 - 7), rng(6, SEEY * 2 - 7) );
  artifact_natural_property prop = artifact_natural_property(rng(ARTPROP_NULL + 1, ARTPROP_MAX - 1));
  create_anomaly(center.x, center.y, prop);
  add_item(center, g->new_natural_artifact(prop), 0);
 } break;
 } // switch (prop)
}

void map::create_anomaly(int cx, int cy, artifact_natural_property prop)
{
 rough_circle(this, t_rubble, cx, cy, 5);
 switch (prop) {
  case ARTPROP_WRIGGLING:
  case ARTPROP_MOVING:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble) {
      add_field(NULL, i, j, fd_push_items, 1);
      if (one_in(3)) add_item(i, j, item::types[itm_rock], 0);
     }
    }
   }
   break;
  case ARTPROP_GLOWING:
  case ARTPROP_GLITTERING:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble && one_in(2))
      add_trap(i, j, tr_glow);
    }
   }
   break;
  case ARTPROP_HUMMING:
  case ARTPROP_RATTLING:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble && one_in(2))
      add_trap(i, j, tr_hum);
    }
   }
   break;
  case ARTPROP_WHISPERING:
  case ARTPROP_ENGRAVED:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble && one_in(3))
      add_trap(i, j, tr_shadow);
    }
   }
   break;
  case ARTPROP_BREATHING:
   for (int i = cx - 1; i <= cx + 1; i++) {
    for (int j = cy - 1; i <= cy + 1; j++)
     if (i == cx && j == cy)
      add_spawn(mon_breather_hub, 1, i, j);
     else
      add_spawn(mon_breather, 1, i, j);
   }
   break;
  case ARTPROP_DEAD:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble)
      add_trap(i, j, tr_drain);
    }
   }
   break;
  case ARTPROP_ITCHY:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble)
      radiation(i, j) = rng(0, 10);
    }
   }
   break;
  case ARTPROP_ELECTRIC:
  case ARTPROP_CRACKLING:
   add_field(NULL, cx, cy, fd_shock_vent, 3);
   break;
  case ARTPROP_SLIMY:
   add_field(NULL, cx, cy, fd_acid_vent, 3);
   break;
  case ARTPROP_WARM:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble)
      add_field(NULL, i, j, fd_fire_vent, 1 + (rl_dist(cx, cy, i, j) % 3));
    }
   }
   break;
  case ARTPROP_SCALED:
   for (int i = cx - 5; i <= cx + 5; i++) {
    for (int j = cy - 5; j <= cy + 5; j++) {
     if (ter(i, j) == t_rubble)
      add_trap(i, j, tr_snake);
    }
   }
   break;
  case ARTPROP_FRACTAL:
   create_anomaly(cx - 4, cy - 4,
               artifact_natural_property(rng(ARTPROP_NULL + 1, ARTPROP_MAX - 1)));
   create_anomaly(cx + 4, cy - 4,
               artifact_natural_property(rng(ARTPROP_NULL + 1, ARTPROP_MAX - 1)));
   create_anomaly(cx - 4, cy + 4,
               artifact_natural_property(rng(ARTPROP_NULL + 1, ARTPROP_MAX - 1)));
   create_anomaly(cx + 4, cy - 4,
               artifact_natural_property(rng(ARTPROP_NULL + 1, ARTPROP_MAX - 1)));
   break;
 }
}
