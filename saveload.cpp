// master implementation file for new-style saveload support

#include "computer.h"
#include "mapdata.h"
#include "mission.h"
#include "overmap.h"
#include "monster.h"
#include "recent_msg.h"
#include "saveload.h"
#include <istream>
#include <ostream>

#include "Zaimoni.STL/Logging.h"

// legacy implementations assume text mode streams
// this only makes a difference for ostream, and may not be correct (different API may be needed)

#ifdef BINARY_STREAMS
#define I_SEP
#else
#define I_SEP << " "
#endif

#define IO_OPS_ENUM(TYPE)	\
std::istream& operator>>(std::istream& is, TYPE& dest)	\
{	\
	int tmp;	\
	is >> tmp;	\
	dest = (TYPE)tmp;	\
	return is;	\
}	\
	\
std::ostream& operator<<(std::ostream& os, TYPE src)	\
{	\
	return os << int(src);	\
}

IO_OPS_ENUM(add_type)
IO_OPS_ENUM(art_charge)
IO_OPS_ENUM(art_effect_active)
IO_OPS_ENUM(art_effect_passive)
IO_OPS_ENUM(bionic_id)
IO_OPS_ENUM(dis_type)
IO_OPS_ENUM(faction_goal)
IO_OPS_ENUM(faction_job)
IO_OPS_ENUM(field_id)
IO_OPS_ENUM(itype_id)
IO_OPS_ENUM(material)
IO_OPS_ENUM(mission_id)
IO_OPS_ENUM(morale_type)
IO_OPS_ENUM(npc_favor_type)
IO_OPS_ENUM(skill)
IO_OPS_ENUM(talk_topic)
IO_OPS_ENUM(ter_id)
IO_OPS_ENUM(trap_id)

// stereotypical translation of pointers to/from vector indexes
// \todo in general if a loaded pointer index is "invalid" we should warn here; non-null requirements are enforced higher up
// \todo in general warn if a non-null ptr points to an invalid id
std::istream& operator>>(std::istream& is, const mission_type*& dest)
{
	int type_id;
	is >> type_id;
	dest = (0 <= type_id && type_id < mission_type::types.size()) ? &mission_type::types[type_id] : 0;
	return is;
}

std::ostream& operator<<(std::ostream& os, const mission_type* const src)
{
	return os << (src ? src->id : -1);
}

std::istream& operator>>(std::istream& is, const mtype*& dest)
{
	int type_id;
	is >> type_id;
	dest = (0 <= type_id && type_id < mtype::types.size()) ? mtype::types[type_id] : 0;
	return is;
}

std::ostream& operator<<(std::ostream& os, const mtype* const src)
{
	return os << (src ? src->id : -1);
}

std::istream& operator>>(std::istream& is, const itype*& dest)
{
	int type_id;
	is >> type_id;
	dest = (0 <= type_id && type_id < item::types.size()) ? item::types[type_id] : 0;	// XXX \todo should be itype::types?
	return is;
}

std::ostream& operator<<(std::ostream& os, const itype* const src)
{
	return os << (src ? src->id : -1);
}

std::istream& operator>>(std::istream& is, const it_ammo*& dest)
{
	int type_id;
	is >> type_id;
	dest = (0 <= type_id && type_id < item::types.size() && item::types[type_id]->is_ammo()) ? static_cast<it_ammo*>(item::types[type_id]) : 0;
	return is;
}

std::ostream& operator<<(std::ostream& os, const it_ammo* const src)
{
	return os << (src ? src->id : -1);
}

std::istream& operator>>(std::istream& is, point& dest)
{
	return is >> dest.x >> dest.y;
}

std::ostream& operator<<(std::ostream& os, const point& src)
{
	return os << src.x I_SEP << src.y;
}

std::istream& operator>>(std::istream& is, tripoint& dest)
{
	return is >> dest.x >> dest.y >> dest.z;
}

std::ostream& operator<<(std::ostream& os, const tripoint& src)
{
	return os << src.x I_SEP << src.y  I_SEP << src.z;
}

std::istream& operator>>(std::istream& is, computer& dest)
{
	dest.options.clear();
	dest.failures.clear();

	// Pull in name and security
	is >> dest.name >> dest.security >> dest.mission_id;
	size_t found = dest.name.find("_");
	while (found != std::string::npos) {
		dest.name.replace(found, 1, " ");
		found = dest.name.find("_");
	}
	// Pull in options
	int optsize;
	is >> optsize;
	for (int n = 0; n < optsize; n++) {
		std::string tmpname;
		int tmpaction, tmpsec;
		is >> tmpname >> tmpaction >> tmpsec;
		size_t found = tmpname.find("_");
		while (found != std::string::npos) {
			tmpname.replace(found, 1, " ");
			found = tmpname.find("_");
		}
		dest.add_option(tmpname, computer_action(tmpaction), tmpsec);
	}
	// Pull in failures
	int failsize, tmpfail;
	is >> failsize;
	for (int n = 0; n < failsize; n++) {
		is >> tmpfail;
		dest.failures.push_back(computer_failure(tmpfail));
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const computer& src)
{
	std::string savename = src.name; // Replace " " with "_"
	size_t found = savename.find(" ");
	while (found != std::string::npos) {
		savename.replace(found, 1, "_");
		found = savename.find(" ");
	}
	os << savename  I_SEP << src.security I_SEP << src.mission_id I_SEP << src.options.size() I_SEP;
	for(const auto& opt : src.options) {
		savename = opt.name;
		found = savename.find(" ");
		while (found != std::string::npos) {
			savename.replace(found, 1, "_");
			found = savename.find(" ");
		}
		os << savename I_SEP << int(opt.action) I_SEP << opt.security I_SEP;
	}
	os << src.failures.size() << " ";
	for (const auto& fail : src.failures) os << int(fail) I_SEP;
	return os;
}

bionic::bionic(std::istream& is)
{
	is >> id >> invlet >> powered >> charge;
}

std::ostream& operator<<(std::ostream& os, const bionic& src)
{
	return os << src.id I_SEP << src.invlet I_SEP << src.powered I_SEP << src.charge;
}

mission::mission(std::istream& is)
{
	is >> type;
	std::string tmpdesc;
	do {
		is >> tmpdesc;
		if (tmpdesc != "<>")
			description += tmpdesc + " ";
	} while (tmpdesc != "<>");
	description = description.substr(0, description.size() - 1); // Ending ' '
	is >> failed >> value >> reward.type >> reward.value >> reward.item_id >> reward.skill_id >>
		uid >> target >> item_id >> count >> deadline >> npc_id >>
		good_fac_id >> bad_fac_id >> step >> follow_up;
}


std::ostream& operator<<(std::ostream& os, const mission& src)
{
	os << src.type;
	return os << src.description << " <> " << (src.failed ? 1 : 0) I_SEP << src.value  I_SEP 
		<< src.reward.type  I_SEP << src.reward.value  I_SEP << src.reward.item_id  I_SEP 
		<< src.reward.skill_id  I_SEP << src.uid  I_SEP << src.target  I_SEP << src.item_id  I_SEP << src.count  I_SEP << src.deadline  I_SEP 
		<< src.npc_id  I_SEP << src.good_fac_id  I_SEP << src.bad_fac_id  I_SEP << src.step  I_SEP << src.follow_up;
}

mongroup::mongroup(std::istream& is)
{
  int tmp_type;
  int tmp_radius;
  is >> tmp_type >> pos >> tmp_radius >> population;	// XXX note absence of src.dying: saveload cancels that
  type = (moncat_id)tmp_type;
  radius = tmp_radius;
}

std::ostream& operator<<(std::ostream& os, const mongroup& src)
{
  return os << src.type I_SEP << src.pos I_SEP << int(src.radius) I_SEP << src.population << std::endl;	// XXX note absence of src.dying: saveload cancels that
}

std::istream& operator>>(std::istream& is, field& dest)
{
	int d;
	is >> dest.type >> d >> dest.age;
	dest.density = d;
	return is;
}

std::ostream& operator<<(std::ostream& os, const field& src)
{
	return os << src.type I_SEP << int(src.density) I_SEP << src.age;
}

city::city(std::istream& is, bool is_road)
: s(0)
{	// the difference between a city, and a road, is the radius (roads have zero radius)
	is >> x >> y;
	if (!is_road) is >> s;
}

std::ostream& operator<<(std::ostream& os, const city& src)
{
	os << src.x << src.y;
	if (0 < src.s) os << src.s;
	return os << std::endl;
}

om_note::om_note(std::istream& is)
{
	is >> x >> y >> num;
	getline(is, text);	// Chomp endl
	getline(is, text);
}

std::ostream& operator<<(std::ostream& os, const om_note& src)
{
	return os << src.x  I_SEP << src.y  I_SEP << src.num << std::endl << src.text << std::endl;
}

radio_tower::radio_tower(std::istream& is)
{
	is >> x >> y >> strength;
	getline(is, message);	// Chomp endl
	getline(is, message);
}

std::ostream& operator<<(std::ostream& os, const radio_tower& src)
{
	return os << src.x I_SEP << src.y I_SEP << src.strength I_SEP << std::endl << src.message << std::endl;
}

std::istream& operator>>(std::istream& is, player_activity& dest)
{
	int tmp, tmptype;
	is >> tmptype >> dest.moves_left >> dest.index >> dest.placement >> tmp;
	dest.type = activity_type(tmptype);
	for (int i = 0; i < tmp; i++) {
		int tmp2;
		is >> tmp2;
		dest.values.push_back(tmp2);
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const player_activity& src)
{
	os << src.type I_SEP << src.moves_left I_SEP << src.index I_SEP << src.placement I_SEP << src.values.size();
	for(const auto& val : src.values) os I_SEP << val;

	return os;
}

spawn_point::spawn_point(std::istream& is)
{
	char tmpfriend;
	int t;
	is >> t >> count >> pos >> faction_id >> mission_id >> tmpfriend >> name;
	type = (mon_id)t;
	friendly = '1' == tmpfriend;
}

std::ostream& operator<<(std::ostream& os, const spawn_point& src)
{
	return os << int(src.type) I_SEP << src.count I_SEP << src.pos I_SEP <<
		src.faction_id I_SEP << src.mission_id << (src.friendly ? " 1 " : " 0 ") <<
		src.name;
}

submap::submap(std::istream& is, game* master_game)
{
	is >> turn_last_touched;
	int turndif = int(messages.turn);
	if (turndif < 0) turndif = 0;
	// Load terrain
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++) {
			is >> ter[i][j];
			itm[i][j].clear();
			trp[i][j] = tr_null;
			fld[i][j] = field();
		}
	}
	// Load irradiation
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++) {
			int radtmp;
			is >> radtmp;
			radtmp -= int(turndif / 100);	// Radiation slowly decays	\todo V 0.2.1+ handle this as a true game time effect; no saveload-purging of radiation
			if (radtmp < 0) radtmp = 0;
			rad[i][j] = radtmp;
		}
	}
	// Load items and traps and fields and spawn points and vehicles
	item it_tmp;
	std::string string_identifier;
	int itx, ity;
	do {
		is >> string_identifier; // "----" indicates end of this submap
		int t = 0;
		if (string_identifier == "I") {
			is >> itx >> ity >> std::ws;
			itm[itx][ity].push_back(item(is));
			if (it_tmp.active) active_item_count++;
		} else if (string_identifier == "C") {
			is >> std::ws;
			itm[itx][ity].back().put_in(item(is));
			if (it_tmp.active) active_item_count++;
		} else if (string_identifier == "T") {
			is >> itx >> ity;
			is >> trp[itx][ity];;
		} else if (string_identifier == "F") {
			is >> itx >> ity;
			is >> fld[itx][ity];
			field_count++;
		}
		else if (string_identifier == "S") spawns.push_back(spawn_point(is));
		else if (string_identifier == "V") {
			vehicle veh(master_game);
			veh.load(is);
			//veh.smx = gridx;
			//veh.smy = gridy;
			vehicles.push_back(veh);
		} else if (string_identifier == "c") {
			is >> comp >> std::ws;
		} else if ("----" == string_identifier) break;
		else {
			debugmsg("Unrecognized map data key");
			std::string databuff;
			getline(is, databuff);
		}
	} while (!is.eof());
}


std::ostream& operator<<(std::ostream& os, const submap& src)
{
	os << src.turn_last_touched << std::endl;
	// Dump the terrain.
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++) os << src.ter[i][j] I_SEP;
		os << std::endl;
	}
	// Dump the radiation
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++)
			os << src.rad[i][j] I_SEP;
	}
	os << std::endl;

	// Items section; designate it with an I.  Then check itm[][] for each square
	//   in the grid and print the coords and the item's details.
	// Designate it with a C if it's contained in the prior item.
	// Also, this wastes space since we print the coords for each item, when we
	//   could be printing a list of items for each coord (except the empty ones)
	item tmp;
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++) {
			for (const auto& it : src.itm[i][j])  {
				os << "I " << i I_SEP << j << std::endl;
				os << it << std::endl;
				for(const auto& it_2 : it.contents) os << "C " << std::endl << it_2 << std::endl;
			}
		}
	}
	// Output the traps
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++) {
			if (src.trp[i][j] != tr_null)
				os << "T " << i I_SEP << j I_SEP << src.trp[i][j] << std::endl;
		}
	}

	// Output the fields
	for (int j = 0; j < SEEY; j++) {
		for (int i = 0; i < SEEX; i++) {
			const field& tmpf = src.fld[i][j];
			if (tmpf.type != fd_null) os << "F " << i << " " << j << " " << tmpf << std::endl;
		}
	}
	// Output the spawn points
	for (const auto& s : src.spawns) os << "S " << s << std::endl;

	// Output the vehicles
	for (int i = 0; i < src.vehicles.size(); i++) {
		os << "V ";
		src.vehicles[i].save(os);
	}
	// Output the computer
	if (src.comp.name != "") os << "c " << src.comp << std::endl;
	return os << "----" << std::endl;
}


std::istream& operator>>(std::istream& is, vehicle_part& dest)
{
	std::string databuff;
	int pid, pnit;

	is >> pid;
	dest.id = vpart_id(pid);

	is >> dest.mount_d >> dest.hp >> dest.amount >> dest.blood >> pnit >> std::ws;
	dest.items.clear();
	for (int j = 0; j < pnit; j++) {
		dest.items.push_back(item(is));
		int ncont;
		is >> ncont >> std::ws; // how many items inside container
		for (int k = 0; k < ncont; k++) dest.items.back().put_in(item(is));
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const vehicle_part& src)
{
	os << src.id I_SEP << src.mount_d I_SEP << src.hp I_SEP << src.amount I_SEP << src.blood I_SEP << src.items.size() << std::endl;
	for(const auto& it : src.items) {
		os << it << std::endl;     // item info
		os << it.contents.size() << std::endl; // how many items inside this item
		for(const auto& it_2 : it.contents) os << it_2 << std::endl; // contents info; blocker V 0.2.0 \todo should already be handled
	}
	return os;
}

faction::faction(std::istream& is)
{
	int valuetmp;
	is >> id >> valuetmp >> goal >> job1 >> job2 >> likes_u >>
		respects_u >> known_by_u >> strength >> sneak >> crime >> cult >>
		good >> om >> map >> size >> power;
	values = valuetmp;
	int size, tmpop;
	is >> size;
	for (int i = 0; i < size; i++) {
		is >> tmpop;
		opinion_of.push_back(tmpop);
	}
	std::getline(is, name);
}

std::ostream& operator<<(std::ostream& os, const faction& src)
{
	os << src.id << " " << src.values << " " << src.goal << " " << src.job1 << " " << src.job2 <<
		" " << src.likes_u << " " << src.respects_u << " " << src.known_by_u << " " <<
		src.strength << " " << src.sneak << " " << src.crime << " " << src.cult << " " <<
		src.good << " " << src.om << " " << src.map <<
		" " << src.size << " " << src.power << " ";
	os << src.opinion_of.size() << " ";
	for (int i = 0; i < src.opinion_of.size(); i++)
		os << src.opinion_of[i] << " ";
	return os << src.name << std::endl;
}

item::item(std::istream& is)
{
	int lettmp, damtmp, burntmp;
	is >> lettmp >> type >> charges >> damtmp >> burntmp >> poison >> curammo >>
		owned >> bday >> active >> corpse >> mission_id >> player_id;
	if (!type) type = item::types[itm_null];	// \todo warn if this kicks in
	getline(is, name);
	if (name == " ''") name = "";
	else {
		size_t pos = name.find_first_of("@@");
		while (pos != std::string::npos) {
			name.replace(pos, 2, "\n");
			pos = name.find_first_of("@@");
		}
		name = name.substr(2, name.size() - 3); // s/^ '(.*)'$/\1/
	}
	invlet = char(lettmp);
	damage = damtmp;
	burnt = burntmp;
	// XXX historically, contents are not loaded at this time; \todo blocker: V 0.2.0 final version would do so
}

std::ostream& operator<<(std::ostream& os, const item& src)
{
	os I_SEP << int(src.invlet) I_SEP << src.type I_SEP << src.charges I_SEP <<
		int(src.damage) I_SEP << int(src.burnt) I_SEP << src.poison I_SEP <<
		src.curammo I_SEP << src.owned I_SEP << src.bday I_SEP << src.active I_SEP << src.corpse;
	os I_SEP << src.mission_id I_SEP << src.player_id;

	std::string name_copy(src.name);

	size_t pos = name_copy.find_first_of("\n");
	while (pos != std::string::npos) {
		name_copy.replace(pos, 1, "@@");
		pos = name_copy.find_first_of("\n");
	}
	return os << " '" << name_copy << "'";
}

monster::monster(std::istream& is)
{
	int plansize;
	is >> type >> pos >> wand >> wandf >> moves >> speed >> hp >> sp_timeout >>
		plansize >> friendly >> faction_id >> mission_id >> dead >> anger >>
		morale;
	if (!type) type = mtype::types[mon_null];	// \todo warn if this kicks in
	point ptmp;
	for (int i = 0; i < plansize; i++) {
		is >> ptmp;
		plans.push_back(ptmp);
	}
}

std::ostream& operator<<(std::ostream& os, const monster& src)
{
	os << src.type I_SEP << src.pos I_SEP << src.wand I_SEP << src.wandf I_SEP <<
		src.moves I_SEP << src.speed I_SEP << src.hp I_SEP << src.sp_timeout I_SEP <<
		src.plans.size() I_SEP << src.friendly I_SEP << src.faction_id I_SEP <<
		src.mission_id I_SEP << src.dead I_SEP << src.anger I_SEP << src.morale;
	for (int i = 0; i < src.plans.size(); i++) {
		os I_SEP << src.plans[i];
	}
	return os;
}

itype::itype(std::istream& is)
: id(0),rarity(0),name("none"),techniques(0)
{
	int colortmp, bashtmp, cuttmp, hittmp, flagstmp;

	is >> price >> sym >> colortmp >> m1 >> m2 >> volume >> weight >> bashtmp >> cuttmp >> hittmp >> flagstmp;
	color = int_to_color(colortmp);
	melee_dam = bashtmp;
	melee_cut = cuttmp;
	m_to_hit = hittmp;
	item_flags = flagstmp;

	id = item::types.size();
}

std::ostream& operator<<(std::ostream& os, const itype& src)
{
	return os << src.price I_SEP << src.sym I_SEP << color_to_int(src.color) I_SEP <<
		src.m1 I_SEP << src.m2 I_SEP << src.volume I_SEP <<
		src.weight I_SEP << int(src.melee_dam) I_SEP << int(src.melee_cut) I_SEP <<
		int(src.m_to_hit) I_SEP << int(src.item_flags);
}

it_armor::it_armor(std::istream& is)
: itype(is)
{
	covers = 0;
	encumber = 0;
	dmg_resist = 0;
	cut_resist = 0;
	env_resist = 0;
	warmth = 0;
	storage = 0;

	int covertmp, enctmp, dmgrestmp, cutrestmp, envrestmp, warmtmp, storagetmp;
	is >> covertmp >> enctmp >> dmgrestmp >> cutrestmp >> envrestmp >> warmtmp >> storagetmp;
	covers = covertmp;
	encumber = enctmp;
	dmg_resist = dmgrestmp;
	cut_resist = cutrestmp;
	env_resist = envrestmp;
	warmth = warmtmp;
	storage = storagetmp;
}

std::ostream& operator<<(std::ostream& os, const it_armor& src)
{
	return os << static_cast<const itype&>(src) I_SEP << int(src.covers) I_SEP <<
		int(src.encumber) I_SEP << int(src.dmg_resist) I_SEP << int(src.cut_resist)I_SEP <<
		int(src.env_resist) I_SEP << int(src.warmth) I_SEP << int(src.storage);
}

it_artifact_armor::it_artifact_armor(std::istream& is)
: it_armor(is)
{
	price = 0;

	int num_effects;
	is >> num_effects;

	for (int i = 0; i < num_effects; i++) {
		art_effect_passive effect;
		is >> effect;
		effects_worn.push_back(effect);
	}

	std::string namepart;
	std::stringstream namedata;
	bool start = true;
	do {
		if (!start) namedata I_SEP;
		else start = false;
		is >> namepart;
		if (namepart != "-") namedata << namepart;
	} while (namepart.find("-") == std::string::npos);
	name = namedata.str();
	start = true;

	std::stringstream descdata;
	do {
		is >> namepart;
		if (namepart == "=") {
			descdata << "\n";
			start = true;
		} else if (namepart != "-") {
			if (!start) descdata I_SEP;
			descdata << namepart;
			start = false;
		}
	} while (namepart.find("-") == std::string::npos && !is.eof());
	description = descdata.str();
}

std::ostream& operator<<(std::ostream& os, const it_artifact_armor& src)
{
	os << static_cast<const it_armor&>(src) << " " << src.effects_worn.size();
	for (const auto& eff : src.effects_worn) os << " " << eff;

	os << " " << src.name << " - ";
	std::string desctmp = src.description;
	size_t endline;
	do {
		endline = desctmp.find("\n");
		if (endline != std::string::npos)
			desctmp.replace(endline, 1, " = ");
	} while (endline != std::string::npos);

	return os << desctmp << " -";
}

it_tool::it_tool(std::istream& is)
: itype(is)
{
	ammo = AT_NULL;
	def_charges = 0;
	charges_per_use = 0;
	turns_per_charge = 0;
	revert_to = itm_null;
	use = &iuse::none;

	is >> max_charges;
}

std::ostream& operator<<(std::ostream& os, const it_tool& src)
{
	return os << static_cast<const itype&>(src) I_SEP << src.max_charges;
}

it_artifact_tool::it_artifact_tool(std::istream& is)
: it_tool(is)
{
	ammo = AT_NULL;
	price = 0;
	def_charges = 0;
	charges_per_use = 1;
	turns_per_charge = 0;
	revert_to = itm_null;
	use = &iuse::artifact;

	int num_effects;

	is >> charge_type >> num_effects;
	for (int i = 0; i < num_effects; i++) {
		art_effect_passive effect;
		is >> effect;
		effects_wielded.push_back(effect);
	}

	is >> num_effects;
	for (int i = 0; i < num_effects; i++) {
		art_effect_active effect;
		is >> effect;
		effects_activated.push_back(effect);
	}

	is >> num_effects;
	for (int i = 0; i < num_effects; i++) {
		art_effect_passive effect;
		is >> effect;
		effects_carried.push_back(effect);
	}

	std::string namepart;
	std::stringstream namedata;
	bool start = true;
	do {
		is >> namepart;
		if (namepart != "-") {
			if (!start) namedata << " ";
			else start = false;
			namedata << namepart;
		}
	} while (namepart.find("-") == std::string::npos);
	name = namedata.str();
	start = true;

	std::stringstream descdata;
	do {
		is >> namepart;
		if (namepart == "=") {
			descdata << "\n";
			start = true;
		} else if (namepart != "-") {
			if (!start) descdata << " ";
			descdata << namepart;
			start = false;
		}
	} while (namepart.find("-") == std::string::npos && !is.eof());
	description = descdata.str();
}

std::ostream& operator<<(std::ostream& os, const it_artifact_tool& src)
{
	return os << static_cast<const it_tool&>(src) I_SEP << src.charge_type I_SEP << src.effects_wielded.size();
	for (const auto& eff : src.effects_wielded) os I_SEP << eff;

	os I_SEP << src.effects_activated.size();
	for (const auto& eff : src.effects_activated) os I_SEP << eff;

	os I_SEP << src.effects_carried.size();
	for (const auto& eff : src.effects_carried) os I_SEP << eff;

	os I_SEP << src.name << " - ";
	std::string desctmp = src.description;
	size_t endline;
	do {
		endline = desctmp.find("\n");
		if (endline != std::string::npos)
			desctmp.replace(endline, 1, " = ");
	} while (endline != std::string::npos);
	return os << desctmp << " -";
}

// staging these here
std::string it_artifact_tool::save_data()
{
	std::stringstream data;
	data << "T " << *this;
	return data.str();
}

std::string it_artifact_armor::save_data()
{
	std::stringstream data;
	data << "A " << *this;
	return data.str();
}

disease::disease(std::istream& is)	// V 0.2.0 blocker \todo savefile representation for disease intensity
: intensity(0)
{
	is >> type >> duration;
}

std::ostream& operator<<(std::ostream& os, const disease& src)
{
	return os << src.type I_SEP << src.duration;
}

addiction::addiction(std::istream& is)
{
	is >> type >> intensity >> sated;
}

std::ostream& operator<<(std::ostream& os, const addiction& src)
{
	return os << src.type I_SEP << src.intensity I_SEP << src.sated;
}

morale_point::morale_point(std::istream& is)
{
	int mortype;
	int item_id;
	is >> bonus >> type >> item_type;
	if (item_type && itm_null == item_type->id) item_type = 0;	// historically, itm_null was the encoding for null pointer
}

std::ostream& operator<<(std::ostream& os, const morale_point& src)
{
	return os << src.bonus I_SEP << src.type I_SEP << src.item_type;
}

// We have an improper inheritance player -> npc (ideal difference would be the AI controller class, cf. Rogue Survivor game family
// -- but C++ is too close to the machine for the savefile issues to be easily managed.  Rely on data structures to keep 
// the save of a non-final class to hard drive to disambiguate.

// 2019-03-24: work required to make player object a proper base object of the npc object not plausibly mechanical.
std::istream& operator>>(std::istream& is, player& dest)
{
	int styletmp;
	is >> dest.pos >> dest.str_cur >> dest.str_max >> dest.dex_cur >> dest.dex_max >>
		dest.int_cur >> dest.int_max >> dest.per_cur >> dest.per_max >> dest.power_level >>
		dest.max_power_level >> dest.hunger >> dest.thirst >> dest.fatigue >> dest.stim >>
		dest.pain >> dest.pkill >> dest.radiation >> dest.cash >> dest.recoil >> dest.driving_recoil >>
		dest.in_vehicle >> dest.scent >> dest.moves >> dest.underwater >> dest.dodges_left >> dest.blocks_left >>
		dest.oxygen >> dest.active_mission >> dest.xp_pool >> dest.male >> dest.health >> dest.style_selected >> dest.activity >> dest.backlog;

	for (int i = 0; i < PF_MAX2; i++) is >> dest.my_traits[i];
	for (int i = 0; i < PF_MAX2; i++) is >> dest.my_mutations[i];
	for (int i = 0; i < NUM_MUTATION_CATEGORIES; i++) is >> dest.mutation_category_level[i];
	for (int i = 0; i < num_hp_parts; i++) is >> dest.hp_cur[i] >> dest.hp_max[i];
	for (int i = 0; i < num_skill_types; i++) is >> dest.sklevel[i] >> dest.skexercise[i];

	int numstyles;
	is >> numstyles;
	for (int i = 0; i < numstyles; i++) {
		itype_id tmp;
		is >> tmp;
		dest.styles.push_back(tmp);
	}

	int numill, typetmp;
	is >> numill;
	for (int i = 0; i < numill; i++) dest.illness.push_back(disease(is));

	int numadd = 0;
	is >> numadd;
	for (int i = 0; i < numadd; i++) dest.addictions.push_back(addiction(is));

	int numbio = 0;
	is >> numbio;
	for (int i = 0; i < numbio; i++) dest.my_bionics.push_back(bionic(is));

	// this is not mirrored in npc save format
	int nummor;
	is >> nummor;
	for (int i = 0; i < nummor; i++) dest.morale.push_back(morale_point(is));

	int nummis = 0;
	int mistmp;
	is >> nummis;
	for (int i = 0; i < nummis; i++) {
		is >> mistmp;
		dest.active_missions.push_back(mistmp);
	}
	is >> nummis;
	for (int i = 0; i < nummis; i++) {
		is >> mistmp;
		dest.completed_missions.push_back(mistmp);
	}
	is >> nummis;
	for (int i = 0; i < nummis; i++) {
		is >> mistmp;
		dest.failed_missions.push_back(mistmp);
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const player& src)
{
	os << src.pos I_SEP << src.str_cur I_SEP << src.str_max I_SEP <<
		src.dex_cur I_SEP << src.dex_max I_SEP << src.int_cur I_SEP << src.int_max I_SEP <<
		src.per_cur I_SEP << src.per_max I_SEP << src.power_level I_SEP <<
		src.max_power_level I_SEP << src.hunger I_SEP << src.thirst I_SEP << src.fatigue I_SEP <<
		src.stim I_SEP << src.pain I_SEP << src.pkill I_SEP << src.radiation I_SEP <<
		src.cash I_SEP << src.recoil I_SEP << src.driving_recoil I_SEP <<
		src.in_vehicle I_SEP << src.scent I_SEP << src.moves I_SEP <<
		src.underwater I_SEP << src.dodges_left I_SEP << src.blocks_left I_SEP <<
		src.oxygen I_SEP << src.active_mission I_SEP << src.xp_pool I_SEP << src.male I_SEP <<
		src.health I_SEP << src.style_selected I_SEP << src.activity I_SEP << src.backlog I_SEP;

	for (int i = 0; i < PF_MAX2; i++) os << src.my_traits[i] I_SEP;
	for (int i = 0; i < PF_MAX2; i++) os << src.my_mutations[i] I_SEP;
	for (int i = 0; i < NUM_MUTATION_CATEGORIES; i++) os << src.mutation_category_level[i] I_SEP;
	for (int i = 0; i < num_hp_parts; i++) os << src.hp_cur[i] I_SEP << src.hp_max[i] I_SEP;
	for (int i = 0; i < num_skill_types; i++) os << src.sklevel[i] I_SEP << src.skexercise[i] I_SEP;

	os << src.styles.size() I_SEP;
	for (int i = 0; i < src.styles.size(); i++) os << src.styles[i] I_SEP;

	os << src.illness.size() I_SEP;
	for(const auto& ill : src.illness) os << ill I_SEP;

	os << src.addictions.size() I_SEP;
	for (const auto& add : src.addictions) os << add I_SEP;

	os << src.my_bionics.size() I_SEP;
	for (const auto& bio : src.my_bionics)  os << bio I_SEP;

	os << src.morale.size() I_SEP;
	for (const auto& mor : src.morale) os << mor I_SEP;

	os I_SEP << src.active_missions.size() I_SEP;
	for (const auto& mi : src.active_missions) os << mi I_SEP;

	os I_SEP << src.completed_missions.size() I_SEP;
	for (const auto& mi : src.completed_missions) os << mi I_SEP;

	os I_SEP << src.failed_missions.size() I_SEP;
	for (const auto& mi : src.failed_missions) os << mi I_SEP;

	os << std::endl;

	// V 0.2.0 blocker \todo asymmetric, not handled in operator >>
	for (size_t i = 0; i < src.inv.size(); i++) {
		for (const auto& it : src.inv.stack_at(i)) {
			os << "I " << it << std::endl;
			for (const auto& it_2 : it.contents) os << "C " << it_2 << std::endl;	// \todo blocker: V 0.2.0 should have been handled already
		}
	}
	for (const auto& it : src.worn) os << "W " << it << std::endl;
	if (!src.weapon.is_null()) os << "w " << src.weapon << std::endl;
	for (const auto& it : src.weapon.contents) os << "c " << it << std::endl;	// \todo blocker: V 0.2.0 should have been handled already

	return os;
}

std::istream& operator>>(std::istream& is, npc_opinion& dest)
{
	int tmpsize;
	is >> dest.trust >> dest.fear >> dest.value >> dest.anger >> dest.owed >> tmpsize;
	for (int i = 0; i < tmpsize; i++) {
		int tmptype, tmpitem, tmpskill;
		npc_favor tmpfavor;
		is >> tmptype >> tmpfavor.value >> tmpitem >> tmpskill;
		tmpfavor.type = npc_favor_type(tmptype);
		tmpfavor.item_id = itype_id(tmpitem);
		tmpfavor.skill_id = skill(tmpskill);
		dest.favors.push_back(tmpfavor);
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const npc_opinion& src)
{
	os << src.trust I_SEP << src.fear I_SEP << src.value I_SEP << src.anger I_SEP << src.owed I_SEP << src.favors.size();
	for (const auto& fav : src.favors) os I_SEP << int(fav.type) I_SEP << fav.value I_SEP << fav.item_id I_SEP << fav.skill_id;	// V 0.2.0 blocker \todo iostreams support: npc_favor
	return os;
}

std::istream& operator>>(std::istream& is, npc_chatbin& dest)
{
	int tmpsize_miss, tmpsize_assigned;
	is >> dest.first_topic >> dest.mission_selected >> dest.tempvalue >> tmpsize_miss >> tmpsize_assigned;
	for (int i = 0; i < tmpsize_miss; i++) {
		int tmpmiss;
		is >> tmpmiss;
		dest.missions.push_back(tmpmiss);
	}
	for (int i = 0; i < tmpsize_assigned; i++) {
		int tmpmiss;
		is >> tmpmiss;
		dest.missions_assigned.push_back(tmpmiss);
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const npc_chatbin& src)
{
	os << src.first_topic I_SEP << src.mission_selected I_SEP << src.tempvalue I_SEP <<
		src.missions.size() I_SEP << src.missions_assigned.size();
	for (const auto& mi : src.missions) os I_SEP << mi;
	for (const auto& mi : src.missions_assigned) os I_SEP << mi;
	return os;
}
