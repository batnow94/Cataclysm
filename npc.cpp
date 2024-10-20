#include "npc.h"
#include "game.h"
#include "vehicle.h"
#include "om_cache.hpp"
#include "rng.h"
#include "skill.h"
#include "output.h"
#include "line.h"
#include "json.h"
#include "recent_msg.h"
#include "stl_typetraits.h"
#include "stl_limits.h"
#include "saveload.h"

#include <fstream>
#include <sstream>
#include <cstdarg>

#define NPC_LOW_VALUE       5
#define NPC_HI_VALUE        8
#define NPC_VERY_HI_VALUE  15

int npc::next_id = npc::MIN_ID;

void npc::global_reset() {
    next_id = MIN_ID;
}

void npc::global_fromJSON(const cataclysm::JSON& src)
{
    if (src.has_key("next_id")) {
        const auto& _next = src["next_id"];
        if (_next.has_key("npc") && fromJSON(_next["npc"], next_id) && MIN_ID <= next_id) return;
    }
    next_id = MIN_ID;
}

void npc::global_toJSON(cataclysm::JSON& dest)
{
    if (MIN_ID < next_id) dest.set("npc", std::to_string(next_id));
}

static const char* const JSON_transcode_favors[] = {
	"CASH",
	"ITEM",
	"TRAINING"
};

static const char* const JSON_transcode_npc_attitude[] = {
	"NULL",
	"TALK",
	"TRADE",
	"FOLLOW",
	"FOLLOW_RUN",
	"LEAD",
	"WAIT",
	"DEFEND",
	"MUG",
	"WAIT_FOR_LEAVE",
	"KILL",
	"FLEE",
	"SLAVE",
	"HEAL",
	"MISSING",
	"KIDNAPPED"
};

static const char* const JSON_transcode_npc_class[] = {
	"NONE",
	"SHOPKEEP",
	"HACKER",
	"DOCTOR",
	"TRADER",
	"NINJA",
	"COWBOY",
	"SCIENTIST",
	"BOUNTY_HUNTER"
};

static const char* const JSON_transcode_npc_mission[] = {
	"NULL",
	"RESCUE_U",
	"SHELTER",
	"SHOPKEEP",
	"MISSING",
	"KIDNAPPED"
};

static const char* const JSON_transcode_talk[] = {
	"DONE",
	"MISSION_LIST",
	"MISSION_LIST_ASSIGNED",
	"",	// TALK_MISSION_START
	"MISSION_DESCRIBE",
	"MISSION_OFFER",
	"MISSION_ACCEPTED",
	"MISSION_REJECTED",
	"MISSION_ADVICE",
	"MISSION_INQUIRE",
	"MISSION_SUCCESS",
	"MISSION_SUCCESS_LIE",
	"MISSION_FAILURE",
	"",	// TALK_MISSION_END
	"MISSION_REWARD",
	"SHELTER",
	"SHELTER_PLANS",
	"SHARE_EQUIPMENT",
	"GIVE_EQUIPMENT",
	"DENY_EQUIPMENT",
	"TRAIN",
	"TRAIN_START",
	"TRAIN_FORCE",
	"SUGGEST_FOLLOW",
	"AGREE_FOLLOW",
	"DENY_FOLLOW",
	"SHOPKEEP",
	"LEADER",
	"LEAVE",
	"PLAYER_LEADS",
	"LEADER_STAYS",
	"HOW_MUCH_FURTHER",
	"FRIEND",
	"COMBAT_COMMANDS",
	"COMBAT_ENGAGEMENT",
	"STRANGER_NEUTRAL",
	"STRANGER_WARY",
	"STRANGER_SCARED",
	"STRANGER_FRIENDLY",
	"STRANGER_AGGRESSIVE",
	"MUG",
	"DESCRIBE_MISSION",
	"WEAPON_DROPPED",
	"DEMAND_LEAVE",
	"SIZE_UP",
	"LOOK_AT",
	"OPINION"
};

static const char* const JSON_transcode_engage[] = {
	"NONE",
	"CLOSE",
	"WEAK",
	"HIT",
	"ALL"
};

static const char* const JSON_transcode_npc_needs[] = {
	"none",
	"ammo",
	"weapon",
	"gun",
	"food",
	"drink"
};

static const std::pair<typename cataclysm::bitmap<NF_MAX>::type, const char*> JSON_transcode_npc_flags[] = {
	{mfb(NF_NULL),"NULL"},
	{mfb(NF_FOOD_HOARDER),"FOOD_HOARDER"},
	{mfb(NF_DRUGGIE),"DRUGGIE"},
	{mfb(NF_TECHNOPHILE),"TECHNOPHILE"},
	{mfb(NF_BOOKWORM),"BOOKWORM"},
};

DEFINE_JSON_ENUM_SUPPORT_TYPICAL(combat_engagement, JSON_transcode_engage)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(npc_attitude, JSON_transcode_npc_attitude)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(npc_class, JSON_transcode_npc_class)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(npc_favor_type, JSON_transcode_favors)
DEFINE_JSON_ENUM_BITFLAG_SUPPORT(npc_flag, JSON_transcode_npc_flags)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(npc_mission, JSON_transcode_npc_mission)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(npc_need, JSON_transcode_npc_needs)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(talk_topic, JSON_transcode_talk)

npc::npc()
: attitude(NPCATT_NULL), myclass(NC_NONE), wand(point(0,0),0), pl(point(-1,-1),0), it(-1,-1),
  fetching_item(false), has_new_items(false), my_fac(nullptr), mission(NPC_MISSION_NULL),
  patience(0), marked_for_death(false), dead(false), flags(0), _id(-1)
{
}

DEFINE_ACID_ASSIGN_W_MOVE(npc_chatbin)
DEFINE_ACID_ASSIGN_W_MOVE(npc)

std::string npc::possessive() const
{
	auto ret(name);
	regular_possessive(ret);
	return ret;
}

std::function<bool(const npc&)> npc::find_me(const int id)
{
	return [=](const npc& _npc) { return id == _npc._id; };
}

npc* npc::find(const int id) {
	auto wanted = find_me(id);

	npc* ret = game::active()->find_first(wanted);
	if (ret) return ret;

	std::function<std::optional<bool>(overmap&)> look_for = [&](overmap& om) {
		ret = om.find(wanted);
		if (ret) return std::optional<bool>(true);
		return std::optional<bool>();
	};

	// \todo check other overmaps...first those already loaded, then those *not* loaded
	om_cache::get().scan(look_for);
	return ret;
}

const npc* npc::find_r(const int id) {
	auto wanted = find_me(id);

	const npc* ret = game::active_const()->find_first(wanted);
	if (ret) return ret;

	std::function<std::optional<bool>(const overmap&)> look_for = [&](const overmap& om) {
		ret = om.find_r(wanted);
		if (ret) return std::optional<bool>(true);
		return std::optional<bool>();
	};

	// \todo check other overmaps...first those already loaded, then those *not* loaded
	om_cache::get().scan_r(look_for);
	return ret;
}

// Intentionally thin wrapper to force correct type for handler call.
void npc::consume(item& food) {
	if (const auto comest = food.is_food()) comest->consumed_by(food, *this);
	else throw std::logic_error("called player::consume on non-food");
}

void npc::die(const int id)
{
    auto g = game::active();

	static auto local_gone = [&id](npc& _npc) {
		if (id == _npc._id) {
			_npc.die();
			return std::optional<bool>(true);
		}
		return std::optional<bool>();
	};

	g->exec_first(local_gone);

	static auto gone = [&id,&g](npc& _npc) {
		if (id == _npc._id) {
			if (g->m.chunk(_npc.GPSpos)) {
				_npc.die();
				return std::optional<bool>(true);
			}
			_npc.marked_for_death = true;
			return std::optional<bool>(false);
		}
		return std::optional<bool>();
	};

	g->cur_om.exec_first(gone);

    // \todo check other overmaps...first those already loaded, then those *not* loaded
}

static std::vector<item> starting_clothes(npc_class type, bool male)
{
	static constexpr const itype_id ref_pants[] = {
		itm_jeans,
		itm_pants,
		itm_pants_leather,
		itm_pants_cargo,
		itm_skirt
	};

	static constexpr const itype_id ref_shirt[] = {
		itm_tshirt,
		itm_polo_shirt,
		itm_dress_shirt,
		itm_tank_top
	};

	static constexpr const itype_id ref_gloves[] = {
		itm_gloves_leather,
		itm_gloves_fingerless,
		itm_fire_gauntlets	// \todo? lock this down to fireman profession
	};

	std::vector<item> ret;

	itype_id pants = ref_pants[rng(0, (std::end(ref_pants) - std::begin(ref_pants)) - (male ? 2 : 1))];
	itype_id shoes = itm_sneakers;
	itype_id shirt = ref_shirt[rng(0, std::end(ref_shirt) - std::begin(ref_shirt))];

	itype_id gloves = itm_null, coat = itm_null, mask = itm_null,
		glasses = itm_null, hat = itm_null;

	auto r = rng(0, (std::end(ref_gloves) - std::begin(ref_gloves)) + 7);
	if (std::end(ref_gloves) - std::begin(ref_gloves) > r) gloves = ref_gloves[r];

	switch (rng(0, 6)) {
	case 2: coat = itm_hoodie; break;
	case 3: coat = itm_jacket_light; break;
	case 4: coat = itm_jacket_jean; break;
	case 5: coat = itm_jacket_leather; break;
	case 6: coat = itm_trenchcoat; break;
	}
	if (one_in(30)) coat = itm_kevlar;

	if (one_in(8)) {
		switch (rng(0, 2)) {
		case 0: mask = itm_mask_dust; break;
		case 1: mask = itm_bandana; break;
		case 2: mask = itm_mask_filter; break;
		}
	}

	if (one_in(8)) glasses = itm_glasses_safety;

	if (one_in(6)) {
		switch (rng(0, 5)) {
		case 0: hat = itm_hat_ball; break;
		case 1: hat = itm_hat_hunting; break;
		case 2: hat = itm_hat_hard; break;
		case 3: hat = itm_helmet_bike; break;
		case 4: hat = itm_helmet_riot; break;
		case 5: hat = itm_helmet_motor; break;
		}
	}

	// Now, more specific stuff for certain classes.
	switch (type) {
	case NC_DOCTOR:
		if (one_in(2)) pants = itm_pants;
		if (one_in(3)) shirt = (one_in(2) ? itm_polo_shirt : itm_dress_shirt);
		if (!one_in(8)) coat = itm_coat_lab;
		if (one_in(3)) mask = itm_mask_dust;
		if (one_in(4)) glasses = itm_glasses_safety;
		if (gloves != itm_null || one_in(3)) gloves = itm_gloves_medical;
		break;

	case NC_TRADER:
		if (one_in(2)) pants = itm_pants_cargo;
		switch (rng(0, 8)) {
		case 1: coat = itm_hoodie; break;
		case 2: coat = itm_jacket_jean; break;
		case 3: case 4: coat = itm_vest; break;
		case 5: case 6: case 7: case 8: coat = itm_trenchcoat; break;
		}
		break;

	case NC_NINJA:
		if (one_in(4)) shirt = itm_null;
		else if (one_in(3)) shirt = itm_tank_top;
		if (one_in(5)) gloves = itm_gloves_leather;
		if (one_in(2)) mask = itm_bandana;
		if (one_in(3)) hat = itm_null;
		break;

	case NC_COWBOY:
		if (one_in(2)) shoes = itm_boots;
		if (one_in(2)) pants = itm_jeans;
		if (one_in(3)) shirt = itm_tshirt;
		if (one_in(4)) gloves = itm_gloves_leather;
		if (one_in(4)) coat = itm_jacket_jean;
		if (one_in(3)) hat = itm_hat_boonie;
		break;

	case NC_SCIENTIST:
		if (one_in(4)) glasses = itm_glasses_eye;
		else if (one_in(2)) glasses = itm_glasses_safety;
		if (one_in(5)) coat = itm_coat_lab;
		break;

	case NC_BOUNTY_HUNTER:
		if (one_in(3)) pants = itm_pants_cargo;
		if (one_in(2)) shoes = itm_boots_steel;
		if (one_in(4)) coat = itm_jacket_leather;
		if (one_in(4)) mask = itm_mask_filter;
		if (one_in(5)) glasses = itm_goggles_ski;
		if (one_in(3)) {
			mask = itm_null;
			hat = itm_helmet_motor;
		}
		break;
	}
	// Fill in the standard things we wear
	if (shoes != itm_null) ret.push_back(item(item::types[shoes], 0));
	if (pants != itm_null) ret.push_back(item(item::types[pants], 0));
	if (shirt != itm_null) ret.push_back(item(item::types[shirt], 0));
	if (coat != itm_null) ret.push_back(item(item::types[coat], 0));
	if (gloves != itm_null) ret.push_back(item(item::types[gloves], 0));
	// Bad to wear a mask under a motorcycle helmet
	if (mask != itm_null && hat != itm_helmet_motor) ret.push_back(item(item::types[mask], 0));
	if (glasses != itm_null) ret.push_back(item(item::types[glasses], 0));
	if (hat != itm_null) ret.push_back(item(item::types[hat], 0));

	// Second pass--for extra stuff like backpacks, etc
	switch (type) {
	case NC_NONE:
	case NC_DOCTOR:
	case NC_SCIENTIST:
		if (one_in(10)) ret.push_back(item(item::types[itm_backpack], 0));
		break;
	case NC_COWBOY:
	case NC_BOUNTY_HUNTER:
		if (one_in(2)) ret.push_back(item(item::types[itm_backpack], 0));
		break;
	case NC_TRADER:
		if (!one_in(15)) ret.push_back(item(item::types[itm_backpack], 0));
		break;
	}

	return ret;
}

// XXX really should be a template choose function, but doesn't belong in rng.h, which is a thin header
static itype_id _get_itype(const std::vector<itype_id>& src)
{
	return src[rng(0, src.size() - 1)];
}

static std::vector<item> starting_inv(npc* me, npc_class type)
{
	int total_space = me->volume_capacity() - 2;
	std::vector<item> ret;
	ret.push_back(item(item::types[itm_lighter], 0));

	// First, if we're wielding a gun, get some ammo for it
	if (const auto gun = me->weapon.is_gun()) {
		const itype* const i_type = item::types[default_ammo(gun->ammo)];
		if (total_space >= i_type->volume) {
			ret.push_back(item(i_type, 0));
			total_space -= ret[ret.size() - 1].volume();
		}
		while ((type == NC_COWBOY || type == NC_BOUNTY_HUNTER || !one_in(3)) &&
			!one_in(4) && total_space >= i_type->volume) {
			ret.push_back(item(i_type, 0));
			total_space -= ret[ret.size() - 1].volume();
		}
	}
	if (type == NC_TRADER) {	// Traders just have tons of random junk
		while (total_space > 0 && !one_in(50)) {
			const itype* const i_type = item::types[rng(2, num_items - 1)];
			if (total_space >= i_type->volume) {
				const auto n = ret.size();
				ret.push_back(item(i_type, 0));
				ret[n] = ret[n].in_its_container();
				total_space -= ret[n].volume();
			}
		}
	}

	if (type == NC_HACKER) {
		while (total_space > 0 && !one_in(10)) {
			item tmpit(item::types[_get_itype(map::items[mi_npc_hacker])], 0);
			tmpit = tmpit.in_its_container();
			if (total_space >= tmpit.volume()) {
				ret.push_back(tmpit);
				total_space -= tmpit.volume();
			}
		}
	}
	if (type == NC_DOCTOR) {
		while (total_space > 0 && !one_in(10)) {
			item tmpit(item::types[_get_itype(map::items[one_in(3) ? mi_softdrugs : mi_harddrugs])], 0);
			tmpit = tmpit.in_its_container();
			if (total_space >= tmpit.volume()) {
				ret.push_back(tmpit);
				total_space -= tmpit.volume();
			}
		}
	}
	// TODO: More specifics.

	while (total_space > 0 && !one_in(8)) {
		const itype* const i_type = item::types[rng(4, num_items - 1)];
		if (total_space >= i_type->volume) {
			const auto n = ret.size();
			ret.push_back(item(i_type, 0));
			ret[n] = ret[n].in_its_container();
			total_space -= ret[n].volume();
		}
	}

	{
		int i = ret.size();
		while (0 < i--) {
			for (const auto type : map::items[mi_trader_avoid]) {
				if (ret[i].type->id == type) {
					EraseAt(ret, i);
					break;
				}
			}
		}
	}

	return ret;
}

void npc::randomize(npc_class type)
{
 assign_id();
 str_max = dice(4, 3);
 dex_max = dice(4, 3);
 int_max = dice(4, 3);
 per_max = dice(4, 3);
 weapon   = item::null;
 inv.clear();
 personality.aggression = rng(-10, 10);
 personality.bravery =    rng( -3, 10);
 personality.collector =  rng( -1, 10);
 personality.altruism =   rng(-10, 10);
 //cash = 100 * rng(0, 20) + 10 * rng(0, 30) + rng(0, 50);
 cash = 0;
 moves = 100;
 mission = NPC_MISSION_NULL;
 male = one_in(2);
 pick_name();

 if (type == NC_NONE) type = npc_class(rng(0, NC_MAX - 1));
 if (one_in(5)) type = NC_NONE;

 myclass = type;
 switch (type) {	// Type of character
 case NC_NONE:	// Untyped; no particular specialization
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(4, 2) - rng(1, 4);
   if (!one_in(3)) sklevel[i] = 0;
  }
  break;

 case NC_HACKER:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(2, 2) - rng(1, 2);
   if (!one_in(3))
    sklevel[i] = 0;
  }
  sklevel[sk_electronics] += rng(1, 4);
  sklevel[sk_computer] += rng(3, 6);
  str_max -= rng(0, 4);
  dex_max -= rng(0, 2);
  int_max += rng(1, 5);
  per_max -= rng(0, 2);
  personality.bravery -= rng(1, 3);
  personality.aggression -= rng(0, 2);
  break;

 case NC_DOCTOR:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(3, 2) - rng(1, 3);
   if (!one_in(3))
    sklevel[i] = 0;
  }
  sklevel[sk_firstaid] += rng(2, 6);
  str_max -= rng(0, 2);
  int_max += rng(0, 2);
  per_max += rng(0, 1) * rng(0, 1);
  personality.aggression -= rng(0, 4);
  if (one_in(4))
   flags |= mfb(NF_DRUGGIE);
  cash += 100 * rng(0, 3) * rng(0, 3);
  break;

 case NC_TRADER:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(2, 2) - 2 + (rng(0, 1) * rng(0, 1));
   if (!one_in(3))
    sklevel[i] = 0;
  }
  sklevel[sk_mechanics] += rng(0, 2);
  sklevel[sk_electronics] +=  rng(0, 2);
  sklevel[sk_speech] += rng(0, 3);
  sklevel[sk_barter] += rng(2, 5);
  int_max += rng(0, 1) * rng(0, 1);
  per_max += rng(0, 1) * rng(0, 1);
  personality.collector += rng(1, 5);
  cash += 250 * rng(1, 10);
  break;

 case NC_NINJA:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(2, 2) - rng(1, 2);
   if (!one_in(3))
    sklevel[i] = 0;
  }
  sklevel[sk_dodge] += rng(2, 4);
  sklevel[sk_melee] += rng(1, 4);
  sklevel[sk_unarmed] += rng(4, 6);
  sklevel[sk_throw] += rng(0, 2);
  str_max -= rng(0, 1);
  dex_max += rng(0, 2);
  per_max += rng(0, 2);
  personality.bravery += rng(0, 3);
  personality.collector -= rng(1, 6);
  do
   styles.push_back( itype_id( rng(itm_style_karate, itm_style_zui_quan) ) );
  while (one_in(2));
  break;

 case NC_COWBOY:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(3, 2) - rng(0, 4);
   if (sklevel[i] < 0)
    sklevel[i] = 0;
  }
  sklevel[sk_gun] += rng(1, 3);
  sklevel[sk_pistol] += rng(1, 3);
  sklevel[sk_rifle] += rng(0, 2);
  int_max -= rng(0, 2);
  str_max += rng(0, 1);
  per_max += rng(0, 2);
  personality.aggression += rng(0, 2);
  personality.bravery += rng(1, 5);
  break;

 case NC_SCIENTIST:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(3, 2) - 4;
   if (sklevel[i] < 0)
    sklevel[i] = 0;
  }
  sklevel[sk_computer] += rng(0, 3);
  sklevel[sk_electronics] += rng(0, 3);
  sklevel[sk_firstaid] += rng(0, 1);
  switch (rng(1, 3)) { // pick a specialty
   case 1: sklevel[sk_computer] += rng(2, 6); break;
   case 2: sklevel[sk_electronics] += rng(2, 6); break;
   case 3: sklevel[sk_firstaid] += rng(2, 6); break;
  }
  if (one_in(4))
   flags |= mfb(NF_TECHNOPHILE);
  if (one_in(3))
   flags |= mfb(NF_BOOKWORM);
  str_max -= rng(1, 3);
  dex_max -= rng(0, 1);
  int_max += rng(2, 5);
  personality.aggression -= rng(1, 5);
  personality.bravery -= rng(2, 8);
  personality.collector += rng (0, 2);
  break;

 case NC_BOUNTY_HUNTER:
  for (int i = 1; i < num_skill_types; i++) {
   sklevel[i] = dice(3, 2) - 3;
   if (sklevel[i] > 0 && one_in(3))
    sklevel[i]--;
  }
  sklevel[sk_gun] += rng(2, 4);
  sklevel[rng(sk_pistol, sk_rifle)] += rng(3, 5);
  personality.aggression += rng(1, 6);
  personality.bravery += rng(0, 5);
  break;
 }
 normalize();
 starting_weapon();
 worn = starting_clothes(type, male);
 inv.clear();
 inv.add_stack(starting_inv(this, type));
}

void npc::randomize_from_faction(faction *fac)
{
// Personality = aggression, bravery, altruism, collector
 my_fac = fac;
 randomize();
 if (!fac) return;

 switch (fac->goal) {
  case FACGOAL_DOMINANCE:
   personality.aggression += rng(0, 3);
   personality.bravery += rng(1, 4);
   personality.altruism -= rng(0, 2);
   break;
  case FACGOAL_CLEANSE:
   personality.aggression -= rng(0, 3);
   personality.bravery += rng(2, 4);
   personality.altruism += rng(0, 3);
   personality.collector -= rng(0, 2);
   break;
  case FACGOAL_SHADOW:
   personality.bravery += rng(4, 7);
   personality.collector -= rng(0, 3);
   int_max += rng(0, 2);
   per_max += rng(0, 2);
   break;
  case FACGOAL_APOCALYPSE:
   personality.aggression += rng(2, 5);
   personality.bravery += rng(4, 8);
   personality.altruism -= rng(1, 4);
   personality.collector -= rng(2, 5);
   break;
  case FACGOAL_ANARCHY:
   personality.aggression += rng(3, 6);
   personality.bravery += rng(0, 4);
   personality.altruism -= rng(3, 8);
   personality.collector -= rng(3, 6);
   int_max -= rng(1, 3);
   per_max -= rng(0, 2);
   str_max += rng(0, 3);
   break;
  case FACGOAL_KNOWLEDGE:
   if (one_in(2)) randomize(NC_SCIENTIST);
   personality.aggression -= rng(2, 5);
   personality.bravery -= rng(1, 4);
   personality.collector += rng(2, 4);
   int_max += rng(2, 5);
   str_max -= rng(1, 4);
   per_max -= rng(0, 2);
   dex_max -= rng(0, 2);
   break;
  case FACGOAL_NATURE:
   personality.aggression -= rng(0, 3);
   personality.collector -= rng(1, 4);
   break;
  case FACGOAL_CIVILIZATION:
   personality.aggression -= rng(2, 4);
   personality.altruism += rng(1, 5);
   personality.collector += rng(1, 5);
   break;
 }
// Jobs
 if (fac->has_job(FACJOB_EXTORTION)) {
  personality.aggression += rng(0, 3);
  personality.bravery -= rng(0, 2);
  personality.altruism -= rng(2, 6);
 }
 if (fac->has_job(FACJOB_INFORMATION)) {
  int_max += rng(0, 4);
  per_max += rng(0, 4);
  personality.aggression -= rng(0, 4);
  personality.collector += rng(1, 3);
 }
 if (fac->has_job(FACJOB_TRADE) || fac->has_job(FACJOB_CARAVANS)) {
  if (!one_in(3)) randomize(NC_TRADER);
  personality.aggression -= rng(1, 5);
  personality.collector += rng(1, 4);
  personality.altruism -= rng(0, 3);
 }
 if (fac->has_job(FACJOB_SCAVENGE))
  personality.collector += rng(4, 8);
 if (fac->has_job(FACJOB_MERCENARIES)) {
  if (!one_in(3)) {
   switch (rng(1, 3)) {
    case 1: randomize(NC_NINJA);		break;
    case 2: randomize(NC_COWBOY);		break;
    case 3: randomize(NC_BOUNTY_HUNTER);	break;
   }
  }
  personality.aggression += rng(0, 2);
  personality.bravery += rng(2, 4);
  personality.altruism -= rng(2, 4);
  str_max += rng(0, 2);
  per_max += rng(0, 2);
  dex_max += rng(0, 2);
 }
 if (fac->has_job(FACJOB_ASSASSINS)) {
  personality.bravery -= rng(0, 2);
  personality.altruism -= rng(1, 3);
  per_max += rng(1, 3);
  dex_max += rng(0, 2);
 }
 if (fac->has_job(FACJOB_RAIDERS)) {
  if (one_in(3)) randomize(NC_COWBOY);
  personality.aggression += rng(3, 5);
  personality.bravery += rng(0, 2);
  personality.altruism -= rng(3, 6);
  str_max += rng(0, 3);
  int_max -= rng(0, 2);
 }
 if (fac->has_job(FACJOB_THIEVES)) {
  if (one_in(3)) randomize(NC_NINJA);
  personality.aggression -= rng(2, 5);
  personality.bravery -= rng(1, 3);
  personality.altruism -= rng(1, 4);
  str_max -= rng(0, 2);
  per_max += rng(1, 4);
  dex_max += rng(1, 3);
 }
 if (fac->has_job(FACJOB_DOCTORS)) {
  if (!one_in(4)) randomize(NC_DOCTOR);
  personality.aggression -= rng(3, 6);
  personality.bravery += rng(0, 4);
  personality.altruism += rng(0, 4);
  int_max += rng(2, 4);
  per_max += rng(0, 2);
  sklevel[sk_firstaid] += rng(1, 5);
 }
 if (fac->has_job(FACJOB_FARMERS)) {
  personality.aggression -= rng(2, 4);
  personality.altruism += rng(0, 3);
  str_max += rng(1, 3);
 }
 if (fac->has_job(FACJOB_DRUGS)) {
  personality.aggression -= rng(0, 2);
  personality.bravery -= rng(0, 3);
  personality.altruism -= rng(1, 4);
 }
 if (fac->has_job(FACJOB_MANUFACTURE)) {
  personality.aggression -= rng(0, 2);
  personality.bravery -= rng(0, 2);
  switch (rng(1, 4)) {
   case 1: sklevel[sk_mechanics] += dice(2, 4);		break;
   case 2: sklevel[sk_electronics] += dice(2, 4);	break;
   case 3: sklevel[sk_cooking] += dice(2, 4);		break;
   case 4: sklevel[sk_tailor] += dice(2,  4);		break;
  }
 }
   
 if (fac->has_value(FACVAL_CHARITABLE)) {
  personality.aggression -= rng(2, 5);
  personality.bravery += rng(0, 4);
  personality.altruism += rng(2, 5);
 }
 if (fac->has_value(FACVAL_LONERS)) {
  personality.aggression -= rng(1, 3);
  personality.altruism -= rng(1, 4);
 }
 if (fac->has_value(FACVAL_EXPLORATION)) {
  per_max += rng(0, 4);
  personality.aggression -= rng(0, 2);
 }
 if (fac->has_value(FACVAL_ARTIFACTS)) {
  personality.collector += rng(2, 5);
  personality.altruism -= rng(0, 2);
 }
 if (fac->has_value(FACVAL_BIONICS)) {
  str_max += rng(0, 2);
  dex_max += rng(0, 2);
  per_max += rng(0, 2);
  int_max += rng(0, 4);
  if (one_in(3)) {
   sklevel[sk_mechanics] += dice(2, 3);
   sklevel[sk_electronics] += dice(2, 3);
   sklevel[sk_firstaid] += dice(2, 3);
  }
 }
 if (fac->has_value(FACVAL_BOOKS)) {
  str_max -= rng(0, 2);
  per_max -= rng(0, 3);
  int_max += rng(0, 4);
  personality.aggression -= rng(1, 4);
  personality.bravery -= rng(0, 3);
  personality.collector += rng(0, 3);
 }
 if (fac->has_value(FACVAL_TRAINING)) {
  str_max += rng(0, 3);
  dex_max += rng(0, 3);
  per_max += rng(0, 2);
  int_max += rng(0, 2);
  for (int i = 1; i < num_skill_types; i++) {
   if (one_in(3))
    sklevel[i] += rng(2, 4);
  }
 }
 if (fac->has_value(FACVAL_ROBOTS)) {
  int_max += rng(0, 3);
  personality.aggression -= rng(0, 3);
  personality.collector += rng(0, 3);
 }
 if (fac->has_value(FACVAL_TREACHERY)) {
  personality.aggression += rng(0, 3);
  personality.altruism -= rng(2, 5);
 }
 if (fac->has_value(FACVAL_STRAIGHTEDGE)) {
  personality.collector -= rng(0, 2);
  str_max += rng(0, 1);
  per_max += rng(0, 2);
  int_max += rng(0, 3);
 }
 if (fac->has_value(FACVAL_LAWFUL)) {
  personality.aggression -= rng(3, 7);
  personality.altruism += rng(1, 5);
  int_max += rng(0, 2);
 }
 if (fac->has_value(FACVAL_CRUELTY)) {
  personality.aggression += rng(3, 6);
  personality.bravery -= rng(1, 4);
  personality.altruism -= rng(2, 5);
 }
}

#if DEAD_FUNC
void npc::make_shopkeep(game *g, oter_id type)  // some other enum was intended
{
 randomize(g, NC_TRADER);
 item tmp;
 std::vector<items_location> pool;
 // these options feel like Gamma World, much like the factions
 /*
 switch (type) {
 case ot_set_food:
  pool.push_back(mi_snacks);
  for (int i = 0; i < 4; i++)	// Weighted to be more likely
   pool.push_back(mi_cannedfood);
  pool.push_back(mi_pasta);
  pool.push_back(mi_produce);
  pool.push_back(mi_alcohol);
  break;
  break;
 case ot_set_weapons:
  pool.push_back(mi_weapons);
  break;
 case ot_set_guns:
  pool.push_back(mi_allguns);
  break;
 case ot_set_clinic:
  pool.push_back(mi_softdrugs);
  pool.push_back(mi_harddrugs);
  break;
 case ot_set_clothing:
  pool.push_back(mi_allclothes);
  break;
 case ot_set_general:
  pool.push_back(mi_cleaning);
  pool.push_back(mi_hardware);
  pool.push_back(mi_tools);
  pool.push_back(mi_bigtools);
  pool.push_back(mi_mischw);
  pool.push_back(mi_gunxtras);
  pool.push_back(mi_electronics);
  for (int i = 0; i < 4; i++)
   pool.push_back(mi_survival_tools);
  break;
 case ot_set_library:
  pool.push_back(mi_magazines);
  pool.push_back(mi_novels);
  pool.push_back(mi_novels);
  pool.push_back(mi_manuals);
  pool.push_back(mi_manuals);
  pool.push_back(mi_textbooks);
  break;
 case ot_set_bionics:
  pool.push_back(mi_electronics);
  pool.push_back(mi_bionics);
  break;
 }
 */

 if (pool.size() > 0) {
  do {
   items_location place = pool[rng(0, pool.size() - 1)];
   item it(item::types[_get_itype(map::items[place])], messages.turn);
   if (volume_carried() + it.volume() > volume_capacity() ||
       weight_carried() + it.weight() > weight_capacity()   )
    break;
   inv.push_back(it);
  } while(true);
 }
 mission = NPC_MISSION_SHOPKEEP;
}
#endif

void npc::spawn_at(const GPS_loc& _GPSpos)
{
    GPSpos = _GPSpos;
    landing_zone_ok();
	if (auto dest = game::active()->toScreen(GPSpos)) pos = *dest;
}

skill npc::best_skill() const
{
 std::vector<int> best_skills;
 int highest = 0;
 for (int i = sk_unarmed; i <= sk_rifle; i++) {
  if (sklevel[i] > highest && i != sk_gun) {
   highest = sklevel[i];
   best_skills.clear();
  }
  if (sklevel[i] == highest && i != sk_gun)
   best_skills.push_back(i);
 }
 int index = rng(0, best_skills.size() - 1);
 return skill(best_skills[index]);
}

void npc::starting_weapon()
{
 if (!styles.empty()) {
  weapon.make(item::types[styles[rng(0, styles.size() - 1)]]);
  return;
 }
 static const itype_id weapons_bash[6] = { itm_hammer, itm_wrench, itm_hammer_sledge, itm_pipe, itm_bat, itm_crowbar };
 static const itype_id weapons_cut[6] = { itm_knife_butcher, itm_hatchet, itm_ax, itm_machete, itm_knife_combat,itm_katana };
 switch (best_skill()) {
 case sk_bashing:
  weapon.make(item::types[weapons_bash[rng(0, 5)]]);
  break;
 case sk_cutting:
  weapon.make(item::types[weapons_cut[rng(0, 5)]]);
  break;
 case sk_throw:
// TODO: Some throwing weapons... grenades?
  break;
 case sk_pistol:
  weapon.make(item::types[_get_itype(map::items[mi_pistols])]);
  break;
 case sk_shotgun:
  weapon.make(item::types[_get_itype(map::items[mi_shotguns])]);
  break;
 case sk_smg:
  weapon.make(item::types[_get_itype(map::items[mi_smg])]);
  break;
 case sk_rifle:
  weapon.make(item::types[_get_itype(map::items[mi_rifles])]);
  break;
 }
 if (const auto gun = weapon.is_gun()) {
  weapon.charges = gun->clip;
  weapon.curammo = dynamic_cast<const it_ammo*>(item::types[default_ammo(gun->ammo)]);
 }
}

#if DEAD_FUNC
bool npc::wear_if_wanted(item it)
{
 static const int max_encumb[num_bp] = {2, 3, 3, 4, 3, 3, 3, 2};

 const auto armor = it.is_armor();
 if (!armor) return false;

 bool encumb_ok = true;
 for (int i = 0; i < num_bp && encumb_ok; i++) {
  if (armor->covers & mfb(i) && encumb(body_part(i)) + armor->encumber > max_encumb[i])	// \todo V0.2.1+ consider micro-optimizing against the armor->covers test
   encumb_ok = false;
 }
 if (encumb_ok) {
  worn.push_back(it);
  return true;
 }
// Otherwise, maybe we should take off one or more items and replace them
 std::vector<int> removal;
 for (int i = 0; i < worn.size(); i++) {
  for (int j = 0; j < num_bp; j++) {
   if (armor->covers & mfb(j) && dynamic_cast<const it_armor*>(worn[i].type)->covers & mfb(j)) {
    removal.push_back(i);
    j = num_bp;
   }
  }
 }
 for (int i = 0; i < removal.size(); i++) {
  if (true) {
//  if (worn[removal[i]].value_to(this) < it.value_to(this)) {
   inv.push_back(worn[removal[i]]);
   worn.push_back(it);
   return true;
  }
 }
 return false;
}
#endif

bool npc::wield(int index)
{
 if (index < 0) { // Wielding a style
  index = 0 - index - 1;
  if (index >= styles.size()) {
   debugmsg("npc::wield(%d) [styles.size() = %d]", index, styles.size());
   return false;
  }
  if (volume_carried() + weapon.volume() <= volume_capacity()) {
   i_add(unwield());
   moves -= 3 * (mobile::mp_turn / 20); // Extra penalty for putting weapon away
  } else // No room for weapon, so we drop it
   GPSpos.add(unwield());
  moves -= 3 * (mobile::mp_turn / 20);
  weapon.make(item::types[styles[index]] );
  if (game::active()->u.see(pos)) messages.add("%s assumes a %s stance.", name.c_str(), weapon.tname().c_str());
  return true;
 }

 if (index >= inv.size()) {
  debugmsg("npc::wield(%d) [inv.size() = %d]", index, inv.size());
  return false;
 }
 if (volume_carried() + weapon.volume() <= volume_capacity()) {
  i_add(unwield());
  moves -= 3 * (mobile::mp_turn / 20);
 } else // No room for weapon, so we drop it
  GPSpos.add(unwield());
 moves -= 3 * (mobile::mp_turn / 20);
 weapon = i_remn(index);
 if (game::active()->u.see(pos)) messages.add("%s wields a %s.", name.c_str(), weapon.tname().c_str());
 return true;
}

// only valid to call on NPCs outside of the reality bubble
void npc::perform_mission(game *g)
{
 switch (mission) {
 case NPC_MISSION_RESCUE_U:
  if (int(messages.turn) % 24 == 0) {
   auto delta = g->u.GPSpos.first - GPSpos.first;
   if (2 < delta.x) delta.x = 2;
   else if (-2 > delta.x) delta.x = -2;
   if (2 < delta.y) delta.y = 2;
   else if (-2 > delta.y) delta.y = -2;
   GPSpos.first.x += delta.x;
   GPSpos.first.y += delta.y;
   attitude = NPCATT_DEFEND;
  }
  break;
 case NPC_MISSION_SHOPKEEP:
  break;	// Just stay where we are
 default:	// Random Walk
  if (int(messages.turn) % 24 == 0) {
   GPSpos.first.x += 2 * rng(-1, 1);
   GPSpos.first.y += 2 * rng(-1, 1);
  }
 }
}

void npc::form_opinion(player *u)
{
// FEAR
 if (u->weapon.is_gun()) {
  op_of_u.fear += weapon.is_gun() ? 2 : 6;
 } else if (u->weapon.type->melee_dam >= 12 || u->weapon.type->melee_cut >= 12)
  op_of_u.fear += 2;
 else if (u->unarmed_attack())	// Unarmed
  op_of_u.fear -= 3;

 if (u->str_max >= 16)
  op_of_u.fear += 2;
 else if (u->str_max >= 12)
  op_of_u.fear += 1;
 else if (u->str_max <= 5)
  op_of_u.fear -= 1;
 else if (u->str_max <= 3)
  op_of_u.fear -= 3;

 for (int i = 0; i < num_hp_parts; i++) {
  if (u->hp_cur[i] <= u->hp_max[i] / 2)
   op_of_u.fear--;
  if (hp_cur[i] <= hp_max[i] / 2)
   op_of_u.fear++;
 }

 if (u->has_trait(PF_DEFORMED2))
  op_of_u.fear += 1;
 if (u->has_trait(PF_TERRIFYING))
  op_of_u.fear += 2;

 if (u->stim > 20)
  op_of_u.fear++;

 if (u->has_disease(DI_DRUNK))
  op_of_u.fear -= 2;

// TRUST
 if (op_of_u.fear > 0)
  op_of_u.trust -= 3;
 else
  op_of_u.trust += 1;

 if (u->weapon.is_gun())
  op_of_u.trust -= 2;
 else if (u->unarmed_attack())
  op_of_u.trust += 2;

 if (u->has_disease(DI_HIGH))
  op_of_u.trust -= 1;
 if (u->has_disease(DI_THC))
  op_of_u.trust -= 1;
 if (u->has_disease(DI_DRUNK))
  op_of_u.trust -= 2;
 if (u->stim > 20 || u->stim < -20)
  op_of_u.trust -= 1;
 if (u->pkill > 30)
  op_of_u.trust -= 1;

 if (u->has_trait(PF_DEFORMED))
  op_of_u.trust -= 1;
 if (u->has_trait(PF_DEFORMED2))
  op_of_u.trust -= 3;

// VALUE
 op_of_u.value = 0;
 for (int i = 0; i < num_hp_parts; i++) {
  if (hp_cur[i] < hp_max[i] * .8)
   op_of_u.value++;
 }
 decide_needs();
 for (int i = 0; i < needs.size(); i++) {
  if (needs[i] == need_food || needs[i] == need_drink)
   op_of_u.value += 2;
 }

 if (op_of_u.fear < personality.bravery + 10 &&
     op_of_u.fear - personality.aggression > -10 && op_of_u.trust > -8)
  attitude = NPCATT_TALK;
 else if (op_of_u.fear - 2 * personality.aggression - personality.bravery < -30)
  attitude = NPCATT_KILL;
 else
  attitude = NPCATT_FLEE;
}

talk_topic npc::pick_talk_topic(player *u)
{
 //form_opinion(u);
 if (personality.aggression > 0) {
  if (op_of_u.fear * 2 < personality.bravery && personality.altruism < 0)
   return TALK_MUG;
  if (personality.aggression + personality.bravery - op_of_u.fear > 0)
   return TALK_STRANGER_AGGRESSIVE;
 }
 if (op_of_u.fear * 2 > personality.altruism + personality.bravery)
  return TALK_STRANGER_SCARED;
 if (op_of_u.fear * 2 > personality.bravery + op_of_u.trust)
  return TALK_STRANGER_WARY;
 if (op_of_u.trust - op_of_u.fear +
     (personality.bravery + personality.altruism) / 2 > 0)
  return TALK_STRANGER_FRIENDLY;

 return TALK_STRANGER_NEUTRAL;
}

int npc::player_danger(player *u)
{
 int ret = 0;
 if (u->weapon.is_gun()) {
  if (weapon.is_gun())
   ret += 4;
  else
   ret += 8;
 } else if (u->weapon.type->melee_dam >= 12 || u->weapon.type->melee_cut >= 12)
  ret++;
 else if (u->weapon.type->id == 0)	// Unarmed
  ret -= 3;

 if (u->str_cur > 20)	// Superhuman strength!
  ret += 4;
 if (u->str_max >= 16)
  ret += 2;
 else if (u->str_max >= 12)
  ret += 1;
 else if (u->str_max <= 5)
  ret -= 2;
 else if (u->str_max <= 3)
  ret -= 4;

 for (int i = 0; i < num_hp_parts; i++) {
  if (u->hp_cur[i] <= u->hp_max[i] / 2)
   ret--;
  if (hp_cur[i] <= hp_max[i] / 2)
   ret++;
 }

 if (u->has_trait(PF_TERRIFYING))
  ret += 2;

 if (u->stim > 20)
  ret++;

 if (u->has_disease(DI_DRUNK))
  ret -= 2;

 return ret;
}

bool npc::turned_hostile()
{
 return (op_of_u.anger >= hostile_anger_level());
}

int npc::hostile_anger_level()
{
 return (20 + op_of_u.fear - personality.aggression);
}

void npc::make_angry()
{
 if (is_enemy())
  return; // We're already angry!
 if (op_of_u.fear > 10 + personality.aggression + personality.bravery)
  attitude = NPCATT_FLEE; // We don't want to take u on!
 else
  attitude = NPCATT_KILL; // Yeah, we think we could take you!
}

bool npc::wants_to_travel_with(player *p)
{
/*
 int target = 8 + personality.bravery * 3 - personality.altruism * 2 -
              personality.collector * .5;
 int total = op_of_u.value * 3 + p->convince_score();
 return (total >= target);
*/
 return true;
}

int npc::assigned_missions_value() const
{
 int ret = 0;
 for (auto m_id : chatbin.missions_assigned) ret += mission::from_id(m_id)->value;
 return ret;
}

std::vector<skill> npc::skills_offered_to(const player& p) const
{
 std::vector<skill> ret;
 for (int i = 0; i < num_skill_types; i++) {
  if (sklevel[i] > p.sklevel[i]) ret.push_back( skill(i) );
 }
 return ret;
}

std::vector<itype_id> npc::styles_offered_to(const player& p) const
{
 std::vector<itype_id> ret;
 for (int i = 0; i < styles.size(); i++) {
  if (!cataclysm::any(p.styles, styles[i])) ret.push_back(styles[i]);
 }
 return ret;
}

int npc::minutes_to_u(const player& u) const
{
 int ret = cataclysm::rational_scaled<24,10>(rl_dist(overmap::toOvermap(GPSpos), overmap::toOvermap(u.GPSpos)));
 while (ret % 5 != 0) ret++; // Round up to nearest five-minute interval
 return ret;
}

bool npc::fac_has_value(faction_value value) const
{
 if (my_fac == nullptr) return false;
 return my_fac->has_value(value);
}

bool npc::fac_has_job(faction_job job) const
{
 if (my_fac == nullptr) return false;
 return my_fac->has_job(job);
}

void npc::decide_needs()
{
 int needrank[num_needs];
 for (int i = 0; i < num_needs; i++) needrank[i] = 20;
 if (weapon.is_gun()) needrank[need_ammo] = 5 * have_ammo(dynamic_cast<const it_gun*>(weapon.type)->ammo).size();
 if (weapon.type->id == 0 && sklevel[sk_unarmed] < 4)
  needrank[need_weapon] = 1;
 else
  needrank[need_weapon] = weapon.type->melee_dam + weapon.type->melee_cut +
                          weapon.type->m_to_hit;
 if (!weapon.is_gun())
  needrank[need_gun] = sklevel[sk_unarmed] + sklevel[sk_melee] +
                       sklevel[sk_bashing] + sklevel[sk_cutting] -
                       sklevel[sk_gun] * 2 + 5;
 needrank[need_food] = 15 - hunger;
 needrank[need_drink] = 15 - thirst;
 for (size_t i = 0; i < inv.size(); i++) {
  const it_comest* food = inv[i].is_food();
  if (!food) food = inv[i].is_food_container();
  if (food != nullptr) {
   needrank[need_food] += food->nutr / 4;
   needrank[need_drink] += food->quench / 4;
  }
 }
 needs.clear();
 int j;
 bool serious = false;
 for (int i = 1; i < num_needs; i++) {
  if (needrank[i] < 10) serious = true;
 }
 if (!serious) {
  needs.push_back(need_none);
  needrank[0] = 10;
 }
 for (int i = 1; i < num_needs; i++) {
  if (needrank[i] < 20) {
   for (j = 0; j < needs.size(); j++) {
    if (needrank[i] < needrank[needs[j]]) {
     needs.insert(needs.begin() + j, npc_need(i));
     j = needs.size() + 1;
    }
   }
   if (j == needs.size())
    needs.push_back(npc_need(i));
  }
 }
}

void npc::say(game *g, std::string line, ...)
{
 if (reject_not_whitelisted_printf(line.c_str())) return;
 va_list ap;
 va_start(ap, line);
 char buff[8192];
#ifdef _MSC_VER
 vsprintf_s<sizeof(buff)>(buff, line.c_str(), ap);
#else
 vsnprintf(buff, sizeof(buff), line.c_str(), ap);
#endif
 va_end(ap);
 line = buff;
 parse_tags(line, g->u, *this);
 if (g->u.see(pos)) {
  messages.add("%s says, \"%s\"", name.c_str(), line.c_str());
  g->sound(pos, 16, "");
 } else {
  std::string sound = name + " saying, \"" + line + "\"";
  g->sound(pos, 16, sound);
 }
}

void npc::init_selling(std::vector<int> &indices, std::vector<int> &prices) const
{
 bool found_lighter = false;
 for (size_t i = 0; i < inv.size(); i++) {
  const item& it = inv[i];
  if (it.type->id == itm_lighter && !found_lighter) found_lighter = true;
  else {
   const int pr = it.price();
   int val = value(it) - (pr / 50);
   if (val <= NPC_LOW_VALUE || mission == NPC_MISSION_SHOPKEEP) {
    indices.push_back(i);
    int price = pr / barter_price_adjustment();
    prices.push_back(price);
   }
  }
 }
}

void npc::init_buying(const inventory& you, std::vector<int> &indices, std::vector<int> &prices) const
{
 for (size_t i = 0; i < you.size(); i++) {
  decltype(auto) it = you[i];
  if (const int val = value(it);  val >= NPC_HI_VALUE) {
   indices.push_back(i);
   int price = it.price();
   if (val >= NPC_VERY_HI_VALUE) price *= 2;
   price *= barter_price_adjustment();
   prices.push_back(price);
  }
 }
}

int npc::minimum_item_value() const
{
 int ret = 20;
 ret -= personality.collector;
 return ret;
}

int npc::worst_item_value() const
{
 int ret = INT_MAX;
 for (size_t i = 0; i < inv.size(); i++) {
  int itval = value(inv[i]);
  if (itval < ret) ret = itval;
 }
 return ret;
}

int npc::value(const item &it) const
{
 int ret = it.price() / 50;
 {
 const skill best = best_skill();
 if (best != sk_unarmed) {
  const int weapon_val = it.weapon_value(sklevel) - weapon.weapon_value(sklevel);
  if (weapon_val > 0) ret += weapon_val;
 }
 }

 if (const auto comest = it.is_food()) {
  if (comest->nutr > 0 || comest->quench > 0) ret++;
  if (hunger > 40) ret += (comest->nutr + hunger - 40) / 6;
  if (thirst > 40) ret += (comest->quench + thirst - 40) / 4;
 }

 if (const auto ammo = it.is_ammo()) {
  const it_gun* gun;
  if (weapon.is_gun()) {
   gun = dynamic_cast<const it_gun*>(weapon.type);
   if (ammo->type == gun->ammo) ret += 14;
  }
  for (size_t i = 0; i < inv.size(); i++) {
   if (inv[i].is_gun()) {
    gun = dynamic_cast<const it_gun*>(inv[i].type);
    if (ammo->type == gun->ammo) ret += 14;
   }
  }
 }

 if (const auto book = it.is_book()) {
  if (book->intel <= int_cur) {
   ret += book->fun;
   if (sklevel[book->type] < book->level && sklevel[book->type] >= book->req)
    ret += book->level * 3;
  }
 }

// TODO: Sometimes we want more than one tool?  Also we don't want EVERY tool.
 if (it.is_tool() && !has_amount(itype_id(it.type->id), 1)) {
  ret += 8;
 }

// TODO: Artifact hunting from relevant factions
// ALSO TODO: Bionics hunting from relevant factions
 if (fac_has_job(FACJOB_DRUGS)) {
	 if (const auto food = it.is_food()) {
		 if (5 <= food->addict) ret += 10;
	 }
 }
 if (fac_has_job(FACJOB_DOCTORS) && it.type->id >= itm_bandages && it.type->id <= itm_prozac)
  ret += 10;
 if (fac_has_value(FACVAL_BOOKS) && it.is_book())
  ret += 14;
 if (fac_has_job(FACJOB_SCAVENGE)) { // Computed last for _reasons_.
  ret += 6;
  ret *= 1.3;
 }
 return ret;
}

bool npc::has_healing_item() const
{
 for (size_t i = 0; i < inv.size(); i++) {
  if (inv[i].type->id == itm_bandages || inv[i].type->id == itm_1st_aid)
   return true;
 }
 return false;
}

bool npc::took_painkiller() const
{
 return (has_disease(DI_PKILL1) || has_disease(DI_PKILL2) ||
         has_disease(DI_PKILL3) || has_disease(DI_PKILL_L));
}

// don't worry about CPU cost (yet), need something that fully works first 2020-11-22 zaimoni
bool npc::is_friend(const player* survivor) const
{
 const bool pc_friend = attitude == NPCATT_FOLLOW || attitude == NPCATT_DEFEND || attitude == NPCATT_LEAD;
 if (!survivor) return pc_friend;
 if (auto _npc = dynamic_cast<const npc*>(survivor)) {
	 // \todo NPC-NPC relationss independent of the player
	 return pc_friend == _npc->is_friend();
 }
 return pc_friend;
}

bool npc::is_following() const
{
 switch (attitude) {
 case NPCATT_FOLLOW:
 case NPCATT_FOLLOW_RUN:
 case NPCATT_DEFEND:
 case NPCATT_SLAVE:
 case NPCATT_WAIT:
  return true;
 default:
  return false;
 }
}

bool npc::is_leader() const
{
 return (attitude == NPCATT_LEAD);
}

// don't worry about CPU cost (yet), need something that fully works first 2020-11-22 zaimoni
bool npc::is_enemy(const player* survivor) const
{
 const bool pc_hostile = attitude == NPCATT_KILL || attitude == NPCATT_MUG || attitude == NPCATT_FLEE;
 if (!survivor) return pc_hostile;
 if (auto _npc = dynamic_cast<const npc*>(survivor)) {
	 // \todo NPC-NPC relations independent of the player
	 return pc_hostile == _npc->is_enemy();
 }
 return pc_hostile;
}

std::optional<std::variant<monster*, npc*, pc*> > npc::is_defending() const
{
	if (NPCATT_DEFEND == attitude) return &game::active()->u;
	return std::nullopt;
}

int npc::danger_assessment() const
{
	struct _danger {
		const npc& me;
		int total = 0;
		const game* g;
		int sightdist;

		_danger() = delete;
		_danger(const npc& me, int total) : me(me), total(total), g(game::active()), sightdist(g->light_level(me.GPSpos)) {}
		_danger(const _danger& src) = default;
		_danger(_danger&& src) = default;
		_danger& operator=(const _danger& src) = default;
		_danger& operator=(_danger&& src) = default;
		~_danger() = default;

		void operator()(const monster& _mon) {
			if (g->m.sees(me.pos, _mon.pos, sightdist)) total += _mon.type->difficulty;
		}

		// \todo should take range into account https://github.com/zaimoni/Cataclysm/issues/106
		void operator()(const player& u) {
			if (me.is_enemy(&u)) {
				const int dist = rl_dist(me.GPSpos, u.GPSpos);
				if (dist < 10) {
					if (u.weapon.is_gun()) total += 10;
					else total += 10 - dist;
				}
			}
			else if (me.is_friend(&u)) {
				const int dist = rl_dist(me.GPSpos, u.GPSpos);
				if (dist < 8) {
					if (u.weapon.is_gun()) total -= 8;
					else total -= 8 - dist;
				}
			}
		}
	};

	_danger danger = { *this, 0 };
	const auto g = game::active_const();
	auto wrap_danger = function_relay(danger);

	g->forall_do(std::function<void(const monster&)>(wrap_danger));

	danger.total /= 10;
	if (danger.total <= 2) danger.total = -10 + 5 * danger.total;	// Low danger if no monsters around

	g->forall_do(std::function<void(const player&)>(wrap_danger));

 for (int i = 0; i < num_hp_parts; i++) {
	 if (i == hp_head || i == hp_torso) {
		 if (hp_cur[i] < hp_max[i] / 4)
			 danger.total += 5;
		 else if (hp_cur[i] < hp_max[i] / 2)
			 danger.total += 3;
		 else if (hp_cur[i] < hp_max[i] * .9)
			 danger.total += 1;
	 } else {
		 if (hp_cur[i] < hp_max[i] / 4)
			 danger.total += 2;
		 else if (hp_cur[i] < hp_max[i] / 2)
			 danger.total += 1;
	 }
 }
 return danger.total;
}

int npc::average_damage_dealt() const
{
 int ret = base_damage();
 ret += weapon.damage_cut() + weapon.damage_bash() / 2;
 ret *= (base_to_hit() + weapon.type->m_to_hit);
 ret /= 15;
 return ret;
}

bool npc::bravery_check(int diff) const
{
 return (dice(10 + personality.bravery, 6) >= dice(diff, 4));
}

bool npc::emergency(int danger) const
{
 return (danger > (personality.bravery * 3 * hp_percentage()) / 100);
}

void npc::told_to_help(game *g)
{
 if (!is_following() && personality.altruism < 0) {
  say(g, "Screw you!");
  return;
 }
 if (is_following()) {
  if (personality.altruism + 4 * op_of_u.value + personality.bravery > danger_assessment()) {
   say(g, "I've got your back!");
   attitude = NPCATT_DEFEND;
  }
  return;
 }
 if (int((personality.altruism + personality.bravery) / 4) > danger_assessment()) {
  say(g, "Alright, I got you covered!");
  attitude = NPCATT_DEFEND;
 }
}

void npc::told_to_wait(game *g)
{
 if (!is_following()) {
  debugmsg("%s told to wait, but isn't following", name.c_str());
  return;
 }
 if (5 + op_of_u.value + op_of_u.trust + personality.bravery * 2 > danger_assessment()) {
  say(g, "Alright, I'll wait here.");
  if (one_in(3))
   op_of_u.trust--;
  attitude = NPCATT_WAIT;
 } else {
  if (one_in(2))
   op_of_u.trust--;
  say(g, "No way, man!");
 }
}
 
void npc::told_to_leave(game *g)
{
 if (!is_following()) {
  debugmsg("%s told to leave, but isn't following", name.c_str());
  return;
 }
 if (danger_assessment() - personality.bravery > op_of_u.value) {
  say(g, "No way, I need you!");
  op_of_u.trust -= 2;
 } else {
  say(g, "Alright, see you later.");
  op_of_u.trust -= 2;
  op_of_u.value -= 1;
 }
}

int npc::follow_distance() const
{
 return 4; // TODO: Modify based on bravery, weapon wielded, etc.
}

int npc::speed_estimate(int speed) const
{
 if (0 == per_cur) return rng(0, speed * 2);
// Up to 80% deviation if per_cur is 1;
// Up to 10% deviation if per_cur is 8;
// Up to 4% deviation if per_cur is 20;
 int deviation = speed / (double)(per_cur * 1.25);
 int low = speed - deviation, high = speed + deviation;
 return rng(low, high);
}

void npc::draw(WINDOW* w, const point& pt, bool inv) const
{
 const point dest = point(VIEW_CENTER)+pos-pt;
 nc_color col = c_pink;
 if (attitude == NPCATT_KILL) col = c_red;
 if (is_friend())             col = c_green;
 else if (is_following())     col = c_ltgreen;
 if (inv) mvwputch_inv(w, dest.y, dest.x, col, '@');
 else     mvwputch    (w, dest.y, dest.x, col, '@');
}

void npc::print_info(WINDOW* w)
{
// First line of w is the border; the next 4 are terrain info, and after that
// is a blank line. w is historically 13 characters tall, and we can't use the last one
// because it's a border as well; so we have lines 6 through 11.
// w is also historically 48 characters wide - 2 characters for border = 46 characters for us
 mvwprintz(w, 6, 1, c_white, "NPC: %s", name.c_str());
 mvwprintz(w, 7, 1, c_red, "Wielding %s%s", (weapon.type->id == 0 ? "" : "a "),
                                     weapon.tname().c_str());
 std::string wearing;
 std::ostringstream wstr;
 wstr << "Wearing: ";
 for (int i = 0; i < worn.size(); i++) {
  if (i > 0)
   wstr << ", ";
  wstr << worn[i].tname();
 }
 wearing = wstr.str();

 const int line_ub = getmaxy(w) - 1;
 const int width_ub = getmaxx(w) - 2;
 int line = 8;
 size_t split;
 do {
  split = (wearing.length() <= width_ub) ? std::string::npos :
                                     wearing.find_last_of(' ', width_ub);
  if (split == std::string::npos) {
   mvwaddstrz(w, line, 1, c_blue, wearing.c_str());
   break;
  }
  mvwaddstrz(w, line++, 1, c_blue, wearing.substr(0, split).c_str());
  wearing = wearing.substr(split + 1);
 } while (line < line_ub);
}

std::string npc::short_description() const
{
 std::ostringstream ret;
 ret << "Wielding " << weapon.tname() << ";   " << "Wearing: ";
 for (int i = 0; i < worn.size(); i++) {
  if (i > 0)
   ret << ", ";
  ret << worn[i].tname();
 }

 return ret.str();
}

std::string npc_opinion::text() const
{
 std::ostringstream ret;
 if (trust <= -10)
  ret << "Completely untrusting";
 else if (trust <= -6)
  ret << "Very untrusting";
 else if (trust <= -3)
  ret << "Untrusting";
 else if (trust <= 2)
  ret << "Uneasy";
 else if (trust <= 5)
  ret << "Trusting";
 else if (trust < 10)
  ret << "Very trusting";
 else
  ret << "Completely trusting";

 ret << " (Trust " << trust << "); ";

 if (fear <= -10)
  ret << "Thinks you're laughably harmless";
 else if (fear <= -6)
  ret << "Thinks you're harmless";
 else if (fear <= -3)
  ret << "Unafraid";
 else if (fear <= 2)
  ret << "Wary";
 else if (fear <= 5)
  ret << "Afraid";
 else if (fear < 10)
  ret << "Very afraid";
 else
  ret << "Terrified";

 ret << " (Fear " << fear << "); ";

 if (value <= -10)
  ret << "Considers you a major liability";
 else if (value <= -6)
  ret << "Considers you a burden";
 else if (value <= -3)
  ret << "Considers you an annoyance";
 else if (value <= 2)
  ret << "Doesn't care about you";
 else if (value <= 5)
  ret << "Values your presence";
 else if (value < 10)
  ret << "Treasures you";
 else
  ret << "Best Friends Forever!";

 ret << " (Value " << value << "); ";

 if (anger <= -10)
  ret << "You can do no wrong!";
 else if (anger <= -6)
  ret << "You're good people";
 else if (anger <= -3)
  ret << "Thinks well of you";
 else if (anger <= 2)
  ret << "Ambivalent";
 else if (anger <= 5)
  ret << "Pissed off";
 else if (anger < 10)
  ret << "Angry";
 else
  ret << "About to kill you";

 ret << " (Anger " << anger << ")";

 return ret.str();
}

npc_opinion& npc_opinion::operator+=(const npc_opinion& rhs)
{
	trust += rhs.trust;
	fear += rhs.fear;
	value += rhs.value;
	anger += rhs.anger;
	owed += rhs.owed;
	return *this;
}

npc_opinion operator+(const npc_opinion& lhs, const npc_opinion& rhs)
{
	npc_opinion ret(lhs);
	ret += rhs;
	return ret;
}

void npc::shift(const point delta)
{
 const point block_delta(delta*SEE);
 pos -= block_delta;
 it -= block_delta;
 pl.x -= block_delta;
 path.clear();
}

void npc::die(player* your_fault)
{
 if (dead) return;
 dead = true;
 const auto g = game::active();

 g->if_visible_message(name+" dies!", *this);

 // currently morale is PC-only; typing as player* is to avoid an external declaration
 if (your_fault) {
	 if (!your_fault->has_trait(PF_HEARTLESS)) {
		 if (is_friend(your_fault)) your_fault->add_morale(MORALE_KILLED_FRIEND, -500);
		 else if (!is_enemy(your_fault)) your_fault->add_morale(MORALE_KILLED_INNOCENT, -100);
	 }
 }

 player::die();

 for (auto& miss : g->active_missions) if (_id == miss.npc_id) miss.fail();
}

npc npc::get_proxy(std::string&& name, const point& origin, const it_gun& gun, unsigned int recoil, int charges)
{
	npc tmp;
	tmp.name = std::move(name);
	if (gun.skill_used) tmp.sklevel[gun.skill_used] = 1;
	tmp.sklevel[sk_gun] = 0;
	tmp.recoil = recoil;
	tmp.screenpos_set(origin);
	tmp.str_cur = 16;
	tmp.dex_cur = 6;
	tmp.per_cur = 8;
	tmp.weapon = item(&gun, 0);
	if (gun.ammo) tmp.weapon.curammo = dynamic_cast<const it_ammo*>(item::types[gun.ammo]);
	tmp.weapon.charges = charges;
	return tmp;
}

void npc::swim(const GPS_loc& loc)
{
	DEBUG_FAIL_OR_LEAVE(!is<swimmable>(GPSpos.ter()), return);
	auto g = game::active();
	const bool u_see = g->u.see(loc) || g->u.see(*this);

	if (rem_disease(DI_ONFIRE)) {	// VAPORWARE: not for phosphorus or lithium ...
		if (u_see) messages.add("The water puts out the flames!");
	}
	int movecost = swim_speed();
	practice(sk_swimming, 1);
	if (movecost >= 5 * mobile::mp_turn) {
		if (!underwater) {
			if (u_see) messages.add("%s sink%s!", grammar::capitalize(desc(grammar::noun::role::subject)).c_str(),
				                    (movecost >= 6 * mobile::mp_turn ? " like a rock" : ""));
			swimming_dive(); // Involuntarily.
		}
	}

	moves -= (movecost > 2 * mobile::mp_turn ? 2 * mobile::mp_turn : movecost);
	for (size_t i = 0; i < inv.size(); i++) {
		decltype(auto) it = inv[i];
		if (IRON == it.type->m1 && it.damage < 5 && one_in(8)) it.damage++; // \todo this is way too fast; also, item::damage invariant not checked for properly
	}
}

bool npc::if_visible_message(std::function<std::string()> msg) const
{
	if (msg) return game::active()->if_visible_message(msg, *this);
	return false;
}

bool npc::if_visible_message(const char* msg) const
{
	if (msg) return game::active()->if_visible_message(msg, *this);
	return false;
}

std::vector<item>* npc::use_stack_at(const point& pt) const
{
	std::vector<item>* ret = nullptr;
	auto g = game::active();
	decltype(auto) stack = g->m.i_at(pt);
	if (!stack.empty()) ret = &stack;

	// NPCs only check for vehicle inventory if the ground inventory is unsuitable
	if (!ret) {
		const auto v = g->m._veh_at(pt);
		vehicle* const veh = v ? v->first : nullptr; // backward compatibility
		if (veh) {
			int veh_part = v ? v->second : 0;
			veh_part = veh->part_with_feature(veh_part, vpf_cargo, false);
			if (veh_part >= 0 && !veh->parts[veh_part].items.empty())
				ret = &veh->parts[veh_part].items;
		}
	}
	return ret;
}


std::optional<player::item_spec> npc::choose(const char* prompt, std::function<std::optional<std::string>(const item_spec&)> fail)
{
	auto candidates = reject(fail);
	// \todo we want to eliminate dominated candidates
	auto ub = candidates.size();
	if (0 >= ub) return std::nullopt;
	return candidates[rng(0, ub - 1)];
}

// NPCs don't use the text UI, so override the player version
void npc::cancel_activity_query(const char* message, ...)
{
	bool doit = false;

	// cf player version.  The commented-out cases represent where specific AI buildout
	// would be needed to know when not to cancel.
	switch (activity.type) {
	case ACT_NULL: return;
/*
	case ACT_READ:
	case ACT_RELOAD:
	case ACT_CRAFT:
	case ACT_BUTCHER:
	case ACT_BUILD:
	case ACT_VEHICLE:
	case ACT_TRAIN:
*/
	default:
		doit = true;
	}

	if (doit) cancel_activity();
}

std::string npc_attitude_name(npc_attitude att)
{
 switch (att) {
 case NPCATT_NULL:	// Don't care/ignoring player
  return "Ignoring";
 case NPCATT_TALK:		// Move to and talk to player
  return "Wants to talk";
 case NPCATT_TRADE:		// Move to and trade with player
  return "Wants to trade";
 case NPCATT_FOLLOW:		// Follow the player
  return "Following";
 case NPCATT_FOLLOW_RUN:	// Follow the player, don't shoot monsters
  return "Following & ignoring monsters";
 case NPCATT_LEAD:		// Lead the player, wait for them if they're behind
  return "Leading";
 case NPCATT_WAIT:		// Waiting for the player
  return "Waiting for you";
 case NPCATT_DEFEND:		// Kill monsters that threaten the player
  return "Defending you";
 case NPCATT_MUG:		// Mug the player
  return "Mugging you";
 case NPCATT_WAIT_FOR_LEAVE:	// Attack the player if our patience runs out
  return "Waiting for you to leave";
 case NPCATT_KILL:		// Kill the player
  return "Attacking to kill";
 case NPCATT_FLEE:		// Get away from the player
  return "Fleeing";
 case NPCATT_SLAVE:		// Following the player under duress
  return "Enslaved";
 case NPCATT_HEAL:		// Get to the player and heal them
  return "Healing you";

 case NPCATT_MISSING:	// Special; missing NPC as part of mission
  return "Missing NPC";
 case NPCATT_KIDNAPPED:	// Special; kidnapped NPC as part of mission
  return "Kidnapped";
 default:
  return "Unknown";
 }
 return "Unknown";
}

std::string npc_class_name(npc_class classtype)
{
 switch(classtype) {
 case NC_NONE:
  return "No class";
 case NC_SHOPKEEP:	// Found in towns.  Stays in his shop mostly.
  return "Shopkeep";
 case NC_HACKER:	// Weak in combat but has hacking skills and equipment
  return "Hacker";
 case NC_DOCTOR:	// Found in towns, or roaming.  Stays in the clinic.
  return "Doctor";
 case NC_TRADER:	// Roaming trader, journeying between towns.
  return "Trader";
 case NC_NINJA:	// Specializes in unarmed combat, carries few items
  return "Ninja";
 case NC_COWBOY:	// Gunslinger and survivalist
  return "Cowboy";
 case NC_SCIENTIST:	// Uses intelligence-based skills and high-tech items
  return "Scientist";
 case NC_BOUNTY_HUNTER: // Resourceful and well-armored
  return "Bounty Hunter";
 }
 return "Unknown class";
}
