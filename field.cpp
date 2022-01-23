#include "rng.h"
#include "game.h"
#include "line.h"
#include "stl_limits.h"
#include "recent_msg.h"

static const char* JSON_transcode[] = {
	"blood",
	"bile",
	"web",
	"slime",
	"acid",
	"sap",
	"fire",
	"smoke",
	"toxic_gas",
	"tear_gas",
	"nuke_gas",
	"gas_vent",
	"fire_vent",
	"flame_burst",
	"electricity",
	"fatigue",
	"push_items",
	"shock_vent",
	"acid_vent"
};

DEFINE_JSON_ENUM_SUPPORT_HARDCODED_NONZERO(field_id, JSON_transcode)

bool map::process_fields(game *g)
{
 bool found_field = false;
 for (int x = 0; x < my_MAPSIZE; x++) {
  for (int y = 0; y < my_MAPSIZE; y++) {
   if (grid[x + y * my_MAPSIZE]->field_count > 0)
    found_field |= process_fields_in_submap(g, x + y * my_MAPSIZE);
  }
 }
 return found_field;
}

// \todo this belongs in its own header
template<class T, size_t N>
class inline_stack final
{
    size_t ub;
    T x[N];

public:
    inline_stack() : ub(0) {}
    // implicit defaults for now

    bool empty() const { return 0 >= ub; }
    auto size() const { return ub; }
    void push(const T& src) { x[ub++] = src; }
    void push(T&& src) { x[ub++] = src; }
    T& operator[](ptrdiff_t n) { return x[n]; }
    const T& operator[](ptrdiff_t n) const { return x[n]; }
};

static void clear_nearby_scent(GPS_loc loc)
{
    const auto g = game::active();
    auto pos = g->toScreen(loc);
    if (pos) g->scent(*pos) = 0;
    for (decltype(auto) dir : Direction::vector) {
        auto pos = g->toScreen(loc+dir);
        if (pos) g->scent(*pos) = 0;
    }
}

bool map::process_fields_in_submap(game *g, int gridn)
{
 bool found_field = false;
 for (int locx = 0; locx < SEEX; locx++) {
  for (int locy = 0; locy < SEEY; locy++) {
   field * const cur = &(grid[gridn]->fld[locx][locy]);
   int x = locx + SEEX * (gridn % my_MAPSIZE),
       y = locy + SEEY * int(gridn / my_MAPSIZE);
   auto loc = g->toGPS(point(x, y));

   const field_id curtype = cur->type;
   if (curtype != fd_null) found_field = true;
   // \todo ideally would use accessors and preclude this by construction
   if (cur->density > 3 || cur->density < 1) {
       if (const char* const json = JSON_key(curtype)) {
           debuglog("Whoooooa density of %d for %s", cur->density, json);
       } else {
           debuglog("Whoooooa density of %d", cur->density);
       }
       // repair it
       if (3 < cur->density) cur->density = 3;
       else cur->density = 1;
   }

  if (cur->age == 0) continue; // Don't process "newborn" fields

  switch (curtype) {

   case fd_null:
    break;	// Do nothing, obviously.  OBVIOUSLY.

   case fd_blood:
   case fd_bile:
    if (is<swimmable>(loc.ter())) cur->age += 250;	// Dissipate faster in water
    break;

   case fd_acid:
    if (is<swimmable>(loc.ter())) cur->age += 20;	// Dissipate faster in water
	{
	auto& stack = loc.items_at();
    int i = stack.size();
    while(0 <= --i) {
     item& melting = stack[i];
     if (melting.made_of(LIQUID) || melting.made_of(VEGGY)   ||
         melting.made_of(FLESH)  || melting.made_of(POWDER)  ||
         melting.made_of(COTTON) || melting.made_of(WOOL)    ||
         melting.made_of(PAPER)  || melting.made_of(PLASTIC) ||
         (melting.made_of(GLASS) && !one_in(3)) || one_in(4)) {
// Acid destructible objects here
      melting.damage++;
      if (5 <= melting.damage || (melting.made_of(PAPER) && 3 <= melting.damage)) {
       cur->age += melting.volume();
       decltype(melting.contents) stage;
       stage.swap(melting.contents);
	   EraseAt(stack, i);
       if (auto ub = stage.size()) {
           stack.reserve(stack.size() + ub);
           for (decltype(auto) it : stage) stack.push_back(std::move(it));
       }
      }
     }
    }
	}
    break;

   case fd_sap:
    break; // It doesn't do anything.

   case fd_fire: {
// Consume items as fuel to help us grow/last longer.
    int smoke = 0, consumed = 0;
	auto& stack = loc.items_at();
    int i = stack.size();
    while(0 <= --i && consumed < cur->density * 2) {
     bool destroyed = false;
     item& it = stack[i];
	 const int vol = it.volume();

	 // firearms ammo cooks easily in B movies
     if (ammotype am = it.provides_ammo_type(); am && am != AT_BATT && am != AT_NAIL && am != AT_BB && am != AT_BOLT && am != AT_ARROW) {
      // as charger rounds are not typically found in naked ground inventories, their new (2020-04-10) inability to burn should be a non-issue
      cur->age /= 2;
      cur->age -= 600;
      destroyed = true;
      smoke += 6;
      consumed++;

     } else if (it.made_of(PAPER)) {
      destroyed = it.burn(cur->density * 3);
      consumed++;
      if (cur->density == 1) cur->age -= vol * 10;
      if (vol >= 4) smoke++;

     } else if ((it.made_of(WOOD) || it.made_of(VEGGY))) {
      if (vol <= cur->density * 10 || 3 == cur->density) {
       cur->age -= 4;
       destroyed = it.burn(cur->density);
       smoke++;
       consumed++;
      } else if (it.burnt < cur->density) {
       destroyed = it.burn(1);
       smoke++;
      }

     } else if ((it.made_of(COTTON) || it.made_of(WOOL))) {
      if (vol <= cur->density * 5 || 3 == cur->density) {
       cur->age--;
       destroyed = it.burn(cur->density);
       smoke++;
       consumed++;
      } else if (it.burnt < cur->density) {
       destroyed = it.burn(1);
       smoke++;
      }

     } else if (it.made_of(FLESH)) {
      if (vol <= cur->density * 5 || (3 == cur->density && one_in(vol / 20))) {
       cur->age--;
       destroyed = it.burn(cur->density);
       smoke += 3;
       consumed++;
      } else if (it.burnt < cur->density * 5 || 2 <= cur->density) {
       destroyed = it.burn(1);
       smoke++;
      }

     } else if (it.made_of(LIQUID)) {
         if (is_hard_liquor(it.type->id)) { // TODO: Make this be not a hack.
             cur->age -= 300;
             smoke += 6;
         } else {
             cur->age += rng(80 * vol, 300 * vol);
             smoke++;
         }
         destroyed = true;
         consumed++;
     } else if (it.made_of(POWDER)) {
      cur->age -= vol;
      destroyed = true;
      smoke += 2;

     } else if (it.made_of(PLASTIC)) {
      smoke += 3;
      if (it.burnt <= cur->density * 2 || (3 == cur->density && one_in(vol))) {
       destroyed = it.burn(cur->density);
       if (one_in(vol + it.burnt)) cur->age--;
      }
     }

     if (destroyed) {
      decltype(it.contents) stage;
      stage.swap(it.contents);
	  EraseAt(stack, i);
      if (auto ub = stage.size()) {
          stack.reserve(stack.size() + ub);
          for (decltype(auto) obj : stage) stack.push_back(std::move(obj));
      }
     }
    }

    if (const auto veh = loc.veh_at()) veh->first->damage(veh->second, cur->density * 10, vehicle::damage_type::incendiary);
// Consume the terrain we're on
	auto& terrain = loc.ter();
    if (is<explodes>(terrain)) {
     terrain = ter_id(int(terrain) + 1);
     cur->age = 0;
     cur->density = 3;
     g->explosion(x, y, 40, 0, true);

    } else if (is<flammable>(terrain) && one_in(32 - cur->density * 10)) {
     cur->age -= cur->density * cur->density * 40;
     smoke += 15;
     if (cur->density == 3) terrain = t_rubble;

    } else if (is<l_flammable>(terrain) && one_in(62 - cur->density * 10)) {
     cur->age -= cur->density * cur->density * 30;
     smoke += 10;
     if (cur->density == 3) terrain = t_rubble;

    } else if (is<swimmable>(terrain))
     cur->age += 800;	// Flames die quickly on water

// If we consumed a lot, the flames grow higher
    while (cur->density < 3 && cur->age < 0) {
     cur->age += 300;
     cur->density++;
    }
// If the flames are in a pit, it can't spread to non-pit
    const bool in_pit = (t_pit == terrain);
// If the flames are REALLY big, they contribute to adjacent flames
    if (cur->density == 3 && cur->age < 0) {
        inline_stack<field*, std::end(Direction::vector) - std::begin(Direction::vector)> stage;
        for (decltype(auto) dir : Direction::vector) {
            auto dest = loc + dir;
            auto& fd = dest.field_at();
            if (fd_fire == fd.type && 3 > fd.density && (!in_pit || t_pit == dest.ter())) stage.push(&fd);
        }
        if (auto ub = stage.size()) {
            auto& fd = *stage[rng(0, ub - 1)];
            fd.density++;
            fd.age = 0;
            cur->age = 0;
        }
    }
// Consume adjacent fuel / terrain / webs to spread.
// Randomly offset our x/y shifts by 0-2, to randomly pick a square to spread to
    int starti = rng(0, 2);
    int startj = rng(0, 2);
    for (int i = 0; i < 3; i++) {
     for (int j = 0; j < 3; j++) {
      int fx = x + ((i + starti) % 3) - 1, fy = y + ((j + startj) % 3) - 1;
      if (inbounds(fx, fy)) {
       int spread_chance = 20 * (cur->density - 1) + 10 * smoke;
	   auto& f = field_at(fx, fy);
       if (f.type == fd_web) spread_chance = 50 + spread_chance / 2;
	   auto& t = ter(fx, fy);
       if (is<explodes>(t) && one_in(8 - cur->density)) {
        t = ter_id(t + 1);
        g->explosion(fx, fy, 40, 0, true);
       } else if ((i != 0 || j != 0) && rng(1, 100) < spread_chance &&
                  (!in_pit || t_pit == t) &&
                  ((3 == cur->density && (is<flammable>(t) || one_in(20))) ||
                   (3 == cur->density && (is<l_flammable>(t) && one_in(10))) ||
                   contains_ignitable(i_at(fx, fy)) ||
                   f.type == fd_web)) {
        if (f.type == fd_smoke || f.type == fd_web)
         f = field(fd_fire);
        else
         add_field(g, fx, fy, fd_fire, 1);
       } else {
        bool nosmoke = true;
        for (int ii = -1; ii <= 1; ii++) {
         for (int jj = -1; jj <= 1; jj++) {
		  auto& f = field_at(x + ii, y + jj);
          if (f.type == fd_fire && f.density == 3) smoke++;
          else if (f.type == fd_smoke) nosmoke = false;
         }
        }
// If we're not spreading, maybe we'll stick out some smoke, huh?
        if (move_cost(fx, fy) > 0 &&
            (!one_in(smoke) || (nosmoke && one_in(40))) && 
            rng(3, 35) < cur->density * 10 && cur->age < 1000) {
         smoke--;
         add_field(g, fx, fy, fd_smoke, rng(1, cur->density));
        }
       }
      }
     }
    }
   } break;
  
   case fd_smoke:
    clear_nearby_scent(loc);
    if (loc.is_outside()) cur->age += 50;
    if (one_in(2)) {
     std::vector <point> spread;
     for (int a = -1; a <= 1; a++) {
      for (int b = -1; b <= 1; b++) {
	   point dest(x + a, y + b);
	   auto& f = field_at(dest);
       if ((f.type == fd_smoke && f.density < 3) ||
           (f.is_null() && move_cost(dest) > 0))
        spread.push_back(dest);
      }
     }
     if (cur->age > 0 && spread.size() > 0) {
      point p = spread[rng(0, spread.size() - 1)];
	  auto& f = field_at(p);
      if (f.type == fd_smoke) {
		f.density++;
        cur->density--;
      } else if (add_field(g, p, fd_smoke, 1, cur->age)) cur->density--;
     }
    }
   break;

   case fd_tear_gas:
    clear_nearby_scent(loc);
    if (loc.is_outside()) cur->age += 30;
// One in three chance that it spreads (less than smoke!)
    if (one_in(3)) {
     std::vector <point> spread;
// Pick all eligible points to spread to
     for (int a = -1; a <= 1; a++) {
      for (int b = -1; b <= 1; b++) {
	   point dest(x + a, y + b);
	   auto& f = field_at(dest);
       if (((f.type == fd_smoke ||
		     f.type == fd_tear_gas) &&
		     f.density < 3            )      ||
           (f.is_null() && move_cost(dest) > 0))
        spread.push_back(dest);
      }
     }
// Then, spread to a nearby point
     if (cur->age > 0 && spread.size() > 0) {
      point p = spread[rng(0, spread.size() - 1)];
	  auto& f = field_at(p);
	  switch (f.type) {
	  case fd_tear_gas:	// Nearby teargas grows thicker
		  f.density++;
		  cur->density--;
		  break;
	  case fd_smoke:	// Nearby smoke is converted into teargas
		  f.type = fd_tear_gas;
		  break;
	  default:	// Or, just create a new field.
		  if (add_field(g, p, fd_tear_gas, 1, cur->age)) cur->density--;
	  }
     }
    }
    break;

   case fd_toxic_gas:
    clear_nearby_scent(loc);
    if (loc.is_outside()) cur->age += 40;
    if (cur->age > 0 && one_in(2)) {
     std::vector <point> spread;
// Pick all eligible points to spread to
     for (int a = -1; a <= 1; a++) {
      for (int b = -1; b <= 1; b++) {
	   point dest(x + a, y + b);
	   auto& f = field_at(dest);
       if (((f.type == fd_smoke ||
		     f.type == fd_tear_gas ||
		     f.type == fd_toxic_gas) &&
		     f.density < 3            )      ||
           (f.is_null() && move_cost(dest) > 0))
        spread.push_back(dest);
      }
     }
// Then, spread to a nearby point
     if (!spread.empty()) {
      point p = spread[rng(0, spread.size() - 1)];
	  auto& f = field_at(p);
	  switch (f.type) {
	  case fd_toxic_gas:	// Nearby toxic gas grows thicker
		  f.density++;
		  cur->density--;
		  break;
	  case fd_smoke:	// Nearby smoke & teargas is converted into toxic gas
	  case fd_tear_gas:
		  f.type = fd_toxic_gas;
		  break;
	  default:	// Or, just create a new field.
		  if (add_field(g, p, fd_toxic_gas, 1, cur->age)) cur->density--;
	  }
     }
    }
    break;


   case fd_nuke_gas:
    clear_nearby_scent(loc);
    if (loc.is_outside()) cur->age += 40;
// Increase long-term radiation in the land underneath
    radiation(x, y) += rng(0, cur->density);
    if (cur->age > 0 && one_in(2)) {
     std::vector <point> spread;
// Pick all eligible points to spread to
     for (int a = -1; a <= 1; a++) {
      for (int b = -1; b <= 1; b++) {
	   point dest(x + a, y + b);
	   auto& f = field_at(dest);
       if (((f.type == fd_smoke ||
		     f.type == fd_tear_gas ||
		     f.type == fd_toxic_gas ||
		     f.type == fd_nuke_gas   ) &&
		     f.density < 3            )      ||
           (f.is_null() && move_cost(dest) > 0))
        spread.push_back(dest);
      }
     }
// Then, spread to a nearby point
     if (!spread.empty()) {
      point p = spread[rng(0, spread.size() - 1)];
	  auto& f = field_at(p);
	  switch(f.type) {
	  case fd_nuke_gas:	// Nearby nukegas grows thicker
		if (f.density < 3) {
          f.density++;
          cur->density--;
        } break;
	  case fd_smoke:	// Nearby smoke, tear, and toxic gas is converted into nukegas
	  case fd_toxic_gas:
	  case fd_tear_gas:
		  f.type = fd_nuke_gas;
		  break;
	  default:	// Or, just create a new field.
		if (add_field(g, p, fd_nuke_gas, 1, cur->age)) cur->density--;
	  }
     }
    }
    break;

   case fd_gas_vent:
    { // Used only by the mine shaft; fires on entrance into reality bubble
       for (decltype(auto) dir : Direction::vector) {
           auto dest = loc + dir;
           auto& fd = loc.field_at();
           if (fd_toxic_gas == fd.type) fd.density++; // C:Whales grace (would expect 3)
           else dest.add(field(fd_toxic_gas, 3));
       }
       loc.add(field(fd_toxic_gas, 3)); // C:Whales: self-erasing
    }
    break;

   case fd_fire_vent:
    if (cur->density > 1) {
     if (one_in(3))
      cur->density--;
    } else {
     cur->type = fd_flame_burst;
     cur->density = 3;
    }
    break;

   case fd_flame_burst:
    if (cur->density > 1)
     cur->density--;
    else {
     cur->type = fd_fire_vent;
     cur->density = 3;
    }
    break;

   case fd_electricity:
    if (!one_in(5)) {	// 4 in 5 chance to spread
     if (move_cost(x, y) == 0 && cur->density > 1) { // We're grounded
         if (MINUTES(5) <= cur->age) break; // too old to spread (???)

         inline_stack<point, std::end(Direction::vector) - std::begin(Direction::vector)> stage;
         for (decltype(auto) dir : Direction::vector) {
             auto dest = point(x, y) + dir;
             if (0 < move_cost(dest) && field_at(dest).is_null()) stage.push(dest);
         };
         if (auto ub = stage.size()) {
             add_field(g, stage[rng(0, ub - 1)], fd_electricity, 1);
             cur->density--;
         }
     } else {	// We're not grounded; attempt to ground
         std::vector<point> valid;
         for (decltype(auto) dir : Direction::vector) {
             auto dest = point(x, y) + dir;
             if (move_cost(dest) == 0 && field_at(dest).is_null())	// Grounded, unelectrified tiles first
                 valid.push_back(dest);
         };
         if (int ub = valid.size()) {
             while (0 <= --ub && 0 < cur->density) {
                 int index = rng(0, ub);
                 add_field(g, valid[index], fd_electricity, 1);
                 cur->density--;
                 valid.erase(valid.begin() + index);
             }
         } else {
             point dest(x + rng(-1, 1), y + rng(-1, 1));
             if (move_cost(dest) > 0) {
                 auto& f = field_at(dest);
                 if (f.type == fd_electricity && f.density < 3) f.density++;
                 else add_field(g, dest, fd_electricity, 1);
             }
             cur->density--;
         }
     }
    }
    break;

   case fd_fatigue:
    if (cur->density < 3 && int(messages.turn) % 3600 == 0 && one_in(10))
     cur->density++;
    else if (cur->density == 3 && one_in(600)) { // Spawn nether creature!
     mon_id type = mon_id(rng(mon_flying_polyp, mon_blank));
     g->z.push_back(monster(mtype::types[type], x + rng(-3, 3), y + rng(-3, 3)));
    }
    break;

   case fd_push_items: {
       std::vector<GPS_loc> valid2(1, loc);
       for (decltype(auto) delta : Direction::vector) {
           const auto dest(delta + loc);
           if (fd_push_items == dest.field_at().type) valid2.push_back(dest);
       }
       if (1 == valid2.size() && !g->mob_at(loc)) break;    // no-op

    std::vector<item>& stack = loc.items_at();
    int i = stack.size();
    while (0 <= --i) {
        item& obj = stack[i];
        if (itm_rock != obj.type->id || int(messages.turn) - 1 <= obj.bday) continue;
        item tmp(std::move(obj));
        stack.erase(stack.begin() + i); // obj invalidated
        auto dest = valid2[rng(0, valid2.size() - 1)];
        // \todo convert to std::visit
        if (const auto whom = g->mob_at(dest)) {
            if (auto u = std::get_if<pc*>(&*whom)) {
                messages.add("A %s hits you!", tmp.tname().c_str());
                (*u)->hit(g, random_body_part(), rng(0, 1), 6, 0);
            } else if (auto p = std::get_if<npc*>(&*whom)) {
                (*p)->hit(g, random_body_part(), rng(0, 1), 6, 0);
                if (g->u.see(dest)) messages.add("A %s hits %s!", tmp.tname().c_str(), (*p)->name.c_str());
            } else {
                auto mon = std::get<monster*>(*whom);
                mon->hurt(6 - mon->armor_bash());
                if (g->u.see(dest)) messages.add("A %s hits the %s!", tmp.tname().c_str(), mon->name().c_str());
            }
        }
        dest.add(std::move(tmp));
    }
   } break;

   case fd_shock_vent:
    if (cur->density > 1) {
     if (one_in(5)) cur->density--;
    } else {
     cur->density = 3;
     int num_bolts = rng(3, 6);
     for (int i = 0; i < num_bolts; i++) {
         point dir = Direction::vector[rng(0, std::end(Direction::vector) - std::begin(Direction::vector) - 1)];
         point bolt = point(x, y);
         int n = rng(4, 12);
         while (0 <= --n) {
             bolt += dir;
             add_field(g, bolt.x, bolt.y, fd_electricity, rng(2, 3));
             if (one_in(4)) dir.x = (0 == dir.x) ? rng(0, 1) * 2 - 1 : 0;
             if (one_in(4)) dir.y = (0 == dir.y) ? rng(0, 1) * 2 - 1 : 0;
         }
     }
    }
    break;

   case fd_acid_vent:
    if (cur->density > 1) {
     if (cur->age >= MINUTES(1)) {
      cur->density--;
      cur->age = 0;
     }
    } else {
     cur->density = 3;
     for (int i = x - 5; i <= x + 5; i++) {
      for (int j = y - 5; j <= y + 5; j++) {
	   const auto& fd = field_at(i, j);
       if (fd.type == fd_null || fd.density == 0) {
        int newdens = 3 - (rl_dist(x, y, i, j) / 2) + (one_in(3) ? 1 : 0);
        if (newdens > 3) newdens = 3;
        if (newdens > 0) add_field(g, i, j, fd_acid, newdens);
       }
      }
     }
    }
    break;

   } // switch (curtype)
  
   cur->age++;
   const int half_life = field::list[cur->type].halflife;
   if (half_life > 0) {
    if (cur->age > 0 && dice(3, cur->age) > dice(3, half_life)) {
     cur->age = 0;
     cur->density--;
    }
    if (cur->density <= 0) { // Totally dissapated.
     grid[gridn]->field_count--;
     grid[gridn]->fld[locx][locy] = field();
    }
   }
  }
 }
 return found_field;
}

void map::step_in_field(game* g, player& u)
{
    assert(!u.is_npc());    // \todo V0.2.5+ eliminate this precondition
    const point pt = u.pos;

    field& cur = field_at(pt);
    switch (cur.type) {
    case fd_null:
    case fd_blood:	// It doesn't actually do anything
    case fd_bile:		// Ditto
        return;

    case fd_web: {
        if (!u.has_trait(PF_WEB_WALKER)) {
            int web = cur.density * 5 - u.disease_level(DI_WEBBED);
            if (0 < web) u.add_disease(DI_WEBBED, web);
            remove_field(pt);
        }
    } break;

    case fd_acid:
        if (cur.density == 3) {
            messages.add("The acid burns your legs and feet!");
            u.hit(g, bp_feet, 0, 0, rng(4, 10));
            u.hit(g, bp_feet, 1, 0, rng(4, 10));
            u.hit(g, bp_legs, 0, 0, rng(2, 8));
            u.hit(g, bp_legs, 1, 0, rng(2, 8));
        } else {
            messages.add("The acid burns your feet!");
            u.hit(g, bp_feet, 0, 0, rng(cur.density, 4 * cur.density));
            u.hit(g, bp_feet, 1, 0, rng(cur.density, 4 * cur.density));
        }
        break;

    case fd_sap:
        messages.add("The sap sticks to you!");
        u.add_disease(DI_SAP, cur.density * 2);
        if (1 >= cur.density)
            remove_field(pt);
        else
            cur.density--;
        break;

    case fd_fire:
        if (!u.has_active_bionic(bio_heatsink)) {
            if (cur.density == 1) {
                messages.add("You burn your legs and feet!");
                u.hit(g, bp_feet, 0, 0, rng(2, 6));
                u.hit(g, bp_feet, 1, 0, rng(2, 6));
                u.hit(g, bp_legs, 0, 0, rng(1, 4));
                u.hit(g, bp_legs, 1, 0, rng(1, 4));
            } else if (cur.density == 2) {
                messages.add("You're burning up!");
                u.hit(g, bp_legs, 0, 0, rng(2, 6));
                u.hit(g, bp_legs, 1, 0, rng(2, 6));
                u.hit(g, bp_torso, 0, 4, rng(4, 9));
            } else if (cur.density == 3) {
                messages.add("You're set ablaze!");
                u.hit(g, bp_legs, 0, 0, rng(2, 6));
                u.hit(g, bp_legs, 1, 0, rng(2, 6));
                u.hit(g, bp_torso, 0, 4, rng(4, 9));
                u.add_disease(DI_ONFIRE, 5);
            }
            if (cur.density == 2)
                u.infect(DI_SMOKE, bp_mouth, 5, 20);
            else if (cur.density == 3)
                u.infect(DI_SMOKE, bp_mouth, 7, 30);
        }
        break;

    case fd_smoke:
        if (cur.density == 3)
            u.infect(DI_SMOKE, bp_mouth, 4, 15);
        break;

    case fd_tear_gas:
        if (cur.density > 1 || !one_in(3))
            u.infect(DI_TEARGAS, bp_mouth, 5, 20);
        if (cur.density > 1)
            u.infect(DI_BLIND, bp_eyes, cur.density * 2, 10);
        break;

    case fd_toxic_gas:
        if (cur.density == 2)
            u.infect(DI_POISON, bp_mouth, 5, 30);
        else if (cur.density == 3)
            u.infect(DI_BADPOISON, bp_mouth, 5, 30);
        break;

    case fd_nuke_gas:
        u.radiation += rng(0, cur.density * (cur.density + 1));
        if (cur.density == 3) {
            messages.add("This radioactive gas burns!");
            u.hurtall(rng(1, 3));
        }
        break;

    case fd_flame_burst:
        if (!u.has_active_bionic(bio_heatsink)) {
            messages.add("You're torched by flames!");
            u.hit(g, bp_legs, 0, 0, rng(2, 6));
            u.hit(g, bp_legs, 1, 0, rng(2, 6));
            u.hit(g, bp_torso, 0, 4, rng(4, 9));
        } else
            messages.add("These flames do not burn you.");
        break;

    case fd_electricity:
        if (u.has_artifact_with(AEP_RESIST_ELECTRICITY))
            messages.add("The electricity flows around you.");
        else {
            messages.add("You're electrocuted!");
            u.hurtall(rng(1, cur.density));
            if (one_in(8 - cur.density) && !one_in(30 - u.str_cur)) {
                messages.add("You're paralyzed!");
                u.moves -= rng(cur.density * 50, cur.density * 150);
            }
        }
        break;

    case fd_fatigue:
        if (rng(0, 2) < cur.density) {
            messages.add("You're violently teleported!");
            u.hurtall(cur.density);
            g->teleport(&u);
        }
        break;

    case fd_shock_vent:
    case fd_acid_vent:
        remove_field(pt);
        break;
    }
}

void map::mon_in_field(game* g, monster& z)
{
    if (z.has_flag(MF_DIGS)) return;	// Digging monsters are immune to fields
    const point pt = z.pos;
    field& cur = field_at(pt);
    int dam = 0;
    switch (cur.type) {
    case fd_null:
    case fd_blood:	// It doesn't actually do anything
    case fd_bile:		// Ditto
        break;

    case fd_web:
        if (!z.has_flag(MF_WEBWALK)) {
            cataclysm::rational_scale<4, 5>(z.speed);
            remove_field(pt);
        }
        break;

    case fd_acid:
        if (/* !z.has_flag(MF_DIGS) && */ !z.has_flag(MF_FLIES) && !z.has_flag(MF_ACIDPROOF)) {
            if (cur.density == 3)
                dam = rng(4, 10) + rng(2, 8);
            else
                dam = rng(cur.density, cur.density * 4);
        }
        break;

    case fd_sap:
        z.speed -= cur.density * 5;
        if (1 >= cur.density)
            remove_field(pt);
        else
            cur.density--;
        break;

    case fd_fire:
        if (z.made_of(FLESH))
            dam = 3;
        if (z.made_of(VEGGY))
            dam = 12;
        if (z.made_of(PAPER) || z.made_of(LIQUID) || z.made_of(POWDER) ||
            z.made_of(WOOD) || z.made_of(COTTON) || z.made_of(WOOL))
            dam = 20;
        if (z.made_of(STONE) || z.made_of(KEVLAR) || z.made_of(STEEL))
            dam = -20;
        if (z.has_flag(MF_FLIES))
            dam -= 15;

        if (cur.density == 1)
            dam += rng(2, 6);
        else if (cur.density == 2) {
            dam += rng(6, 12);
            if (!z.has_flag(MF_FLIES)) {
                z.moves -= mobile::mp_turn / 5;
                if (!z.ignitable()) z.add_effect(ME_ONFIRE, rng(3, 8));
            }
        }
        else if (cur.density == 3) {
            dam += rng(10, 20);
            if (!z.has_flag(MF_FLIES) || one_in(3)) {
                z.moves -= (mobile::mp_turn / 5) * 2;
                if (!z.ignitable()) z.add_effect(ME_ONFIRE, rng(8, 12));
            }
        }
        [[fallthrough]]; // Drop through to smoke

    case fd_smoke:
        if (cur.density == 3)
            z.speed -= rng(10, 20);
        if (z.made_of(VEGGY))	// Plants suffer from smoke even worse
            z.speed -= rng(1, cur.density * 12);
        break;

    case fd_tear_gas:
        if (z.made_of(FLESH) || z.made_of(VEGGY)) {
            z.add_effect(ME_BLIND, cur.density * 8);
            if (cur.density == 3) {
                z.add_effect(ME_STUNNED, rng(10, 20));
                dam = rng(4, 10);
            }
            else if (cur.density == 2) {
                z.add_effect(ME_STUNNED, rng(5, 10));
                dam = rng(2, 5);
            }
            else
                z.add_effect(ME_STUNNED, rng(1, 5));
            if (z.made_of(VEGGY)) {
                z.speed -= rng(cur.density * 5, cur.density * 12);
                dam += cur.density * rng(8, 14);
            }
        }
        break;

    case fd_toxic_gas:
        dam = cur.density;
        z.speed -= cur.density;
        break;

    case fd_nuke_gas:
        if (cur.density == 3) {
            z.speed -= rng(60, 120);
            dam = rng(30, 50);
        }
        else if (cur.density == 2) {
            z.speed -= rng(20, 50);
            dam = rng(10, 25);
        }
        else {
            z.speed -= rng(0, 15);
            dam = rng(0, 12);
        }
        if (z.made_of(VEGGY)) {
            z.speed -= rng(cur.density * 5, cur.density * 12);
            dam *= cur.density;
        }
        break;

    case fd_flame_burst:
        if (z.made_of(FLESH))
            dam = 3;
        if (z.made_of(VEGGY))
            dam = 12;
        if (z.made_of(PAPER) || z.made_of(LIQUID) || z.made_of(POWDER) ||
            z.made_of(WOOD) || z.made_of(COTTON) || z.made_of(WOOL))
            dam = 50;
        if (z.made_of(STONE) || z.made_of(KEVLAR) || z.made_of(STEEL))
            dam = -25;
        dam += rng(0, 8);
        z.moves -= 20;
        break;

    case fd_electricity:
        dam = rng(1, cur.density);
        if (one_in(8 - cur.density)) z.moves -= cur.density * 150;
        break;

    case fd_fatigue:
        if (rng(0, 2) < cur.density) {
            dam = cur.density;
            g->teleport_to(z, g->teleport_destination_unsafe(z.pos, 10));
        }
        break;

 }
    if (0 < dam && z.hurt(dam)) g->kill_mon(z);
}
