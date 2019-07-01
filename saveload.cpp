// master implementation file for new-style saveload support

#include "computer.h"
#include "mapdata.h"
#include "mission.h"
#include "overmap.h"
#include "monster.h"
#include "recent_msg.h"
#include "saveload.h"
#include "json.h"

#include <istream>
#include <ostream>

#include "Zaimoni.STL/Logging.h"

using cataclysm::JSON;

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

IO_OPS_ENUM(activity_type)
IO_OPS_ENUM(add_type)
IO_OPS_ENUM(art_charge)
IO_OPS_ENUM(art_effect_active)
IO_OPS_ENUM(art_effect_passive)
IO_OPS_ENUM(bionic_id)
IO_OPS_ENUM(combat_engagement)
IO_OPS_ENUM(computer_action)
IO_OPS_ENUM(dis_type)
IO_OPS_ENUM(faction_goal)
IO_OPS_ENUM(faction_job)
IO_OPS_ENUM(field_id)
IO_OPS_ENUM(itype_id)
IO_OPS_ENUM(material)
IO_OPS_ENUM(mission_id)
IO_OPS_ENUM(moncat_id)
IO_OPS_ENUM(mon_id)
IO_OPS_ENUM(morale_type)
IO_OPS_ENUM(npc_attitude)
IO_OPS_ENUM(npc_class)
IO_OPS_ENUM(npc_favor_type)
IO_OPS_ENUM(npc_mission)
IO_OPS_ENUM(skill)
IO_OPS_ENUM(talk_topic)
IO_OPS_ENUM(ter_id)
IO_OPS_ENUM(trap_id)

#define JSON_ENUM(TYPE)	\
JSON toJSON(TYPE src) {	\
	auto x = JSON_key(src);	\
	if (x) return JSON(x);	\
	throw std::runtime_error(std::string("encoding failure: " #TYPE " value ")+std::to_string((int)src));	\
}	\
	\
bool fromJSON(const JSON& src, TYPE& dest)	\
{	\
	if (!src.is_scalar()) return false;	\
	cataclysm::JSON_parse<TYPE> parse;	\
	dest = parse(src.scalar());	\
	return true;	\
}

JSON_ENUM(activity_type)
JSON_ENUM(add_type)
JSON_ENUM(bionic_id)
JSON_ENUM(combat_engagement)
JSON_ENUM(computer_action)
JSON_ENUM(computer_failure)
JSON_ENUM(dis_type)
JSON_ENUM(faction_goal)
JSON_ENUM(faction_job)
JSON_ENUM(field_id)
JSON_ENUM(itype_id)	// breaks if we need a randart as a morale_point item
JSON_ENUM(mission_id)
JSON_ENUM(mon_id)
JSON_ENUM(moncat_id)
JSON_ENUM(morale_type)
JSON_ENUM(npc_favor_type)
JSON_ENUM(skill)
JSON_ENUM(talk_topic)

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

JSON toJSON(const mission_type* const src) {
	auto x = JSON_key((mission_id)src->id);
	if (x) return JSON(x);
	throw std::runtime_error(std::string("encoding failure: mission_id value ") + std::to_string(src->id));
}

bool fromJSON(const JSON& src, const mission_type*& dest)
{
	if (!src.is_scalar()) return false;
	cataclysm::JSON_parse<mission_id> parse;
	auto type_id = parse(src.scalar());
	dest = type_id ? &mission_type::types[type_id] : 0;
	return true;
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

bool fromJSON(const JSON& src, const itype*& dest)
{
	if (!src.is_scalar()) return false;
	itype_id type_id;
	bool ret = fromJSON(src, type_id);
	if (ret) dest = item::types[type_id];	// XXX \todo should be itype::types?
	return ret;
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
	if ('[' == (is >> std::ws).peek()) {
		JSON pt(is);
		if (2 != pt.size() || JSON::array!=pt.mode()) throw std::runtime_error("point expected to be a length 2 array");
		if (!fromJSON(pt[0],dest.x) || !fromJSON(pt[1], dest.y)) throw std::runtime_error("point wants integer coordinates");
		return is;
	}
	return is >> dest.x >> dest.y;	// \todo release block: remove legacy reading
}

std::ostream& operator<<(std::ostream& os, const point& src)
{
	return os << '[' << std::to_string(src.x) << ',' << std::to_string(src.y) << ']';
}

bool fromJSON(const cataclysm::JSON& src, point& dest)
{
	if (2 != src.size() || JSON::array != src.mode()) return false;
	point staging;
	bool ret = fromJSON(src[0], staging.x) && fromJSON(src[1], staging.y);
	if (ret) dest = staging;
	return ret;
}

JSON toJSON(const point& src)
{
	JSON ret;
	ret.push(std::to_string(src.x));
	ret.push(std::to_string(src.y));
	return ret;
}

std::istream& operator>>(std::istream& is, tripoint& dest)
{
	if ('[' == (is >> std::ws).peek()) {
		JSON pt(is);
		if (3 != pt.size() || JSON::array != pt.mode()) throw std::runtime_error("tripoint expected to be a length 3 array");
		if (!fromJSON(pt[0], dest.x) || !fromJSON(pt[1], dest.y) || !fromJSON(pt[2], dest.z)) throw std::runtime_error("tripoint wants integer coordinates");
		return is;
	}
	return is >> dest.x >> dest.y >> dest.z;	// \todo release block: remove legacy reading
}

std::ostream& operator<<(std::ostream& os, const tripoint& src)
{
	return os << '[' << std::to_string(src.x) << ',' << std::to_string(src.y) << ',' << std::to_string(src.z) << ']';
}

template<char src, char dest>
void xform(std::string& x)
{
	size_t found;
	while ((found = x.find(src)) != std::string::npos) x.replace(found, 1, 1, dest);
}

JSON toJSON(const computer_option& src)
{
	JSON opt;
	opt.set("name", src.name);
	opt.set("action", JSON_key(src.action));
	if (0 < src.security) opt.set("security", std::to_string(src.security));
	return opt;
}

bool fromJSON(const JSON& src, computer_option& dest)
{
	bool ok = true;
	if (!src.has_key("name") || !fromJSON(src["name"], dest.name)) ok = false;
	if (!src.has_key("action") || !fromJSON(src["action"], dest.action)) ok = false;
	if (!ok) return false;
	if (src.has_key("security") && fromJSON(src["security"],dest.security) && 0 > dest.security) dest.security = 0;
	return true;
}

computer_option::computer_option(std::istream& is)
: name("Unknown"),action(COMPACT_NULL),security(0)
{
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
		if (!fromJSON(_in,*this)) throw std::runtime_error("invalid computer option");
		return;
	}
	is >> name >> action >> security;	// \todo release block: remove legacy reading
	xform<'_', ' '>(name);
}

std::ostream& operator<<(std::ostream& os, const computer_option& src)
{
	// mandatory keys: name:string, action:computer_action
	// optional key: security (default to 0 if absent, suppress if non-positive)
	JSON opt;
	opt.set("name", src.name);
	opt.set("action", JSON_key(src.action));
	if (0 < src.security) opt.set("security", std::to_string(src.security));
	return os << opt;
}

std::istream& operator>>(std::istream& is, computer& dest)
{
	dest.options.clear();
	dest.failures.clear();
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
		if (!_in.has_key("name") || !fromJSON(_in["name"], dest.name)) throw std::runtime_error("computer should be named");
		// \todo error out if name not set
		if (_in.has_key("security") && fromJSON(_in["security"], dest.security) && 0 > dest.security) dest.security = 0;
		if (_in.has_key("mission_id")) fromJSON(_in["mission_id"], dest.mission_id);
		if (_in.has_key("options")) _in["options"].decode(dest.options);
		if (_in.has_key("failures")) _in["failures"].decode(dest.failures);
		return is;
	}

	// Pull in name and security
	is >> dest.name >> dest.security >> dest.mission_id;	// \todo release block: remove legacy reading
	xform<'_',' '>(dest.name);
	// Pull in options
	int optsize;
	is >> optsize;
	for (int n = 0; n < optsize; n++) dest.options.push_back(computer_option(is));
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
	// mandatory keys: name
	// optional keys: security, mission_id, options, failures
	JSON comp;
	comp.set("name", src.name);
	if (0 < src.security) comp.set("security", std::to_string(src.security));
	if (0 <= src.mission_id) comp.set("mission_id", std::to_string(src.mission_id));
	if (!src.options.empty()) comp.set("options", JSON::encode(src.options));
	if (!src.failures.empty()) comp.set("failures", JSON::encode(src.failures));
	return os << comp;
}

bionic::bionic(std::istream& is)
: id(bio_batteries), invlet('a'), powered(false), charge(0)
{
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
		if (!_in.has_key("id") || !fromJSON(_in["id"], id)) throw std::runtime_error("unrecognized bionic");
		if (_in.has_key("invlet")) fromJSON(_in["invlet"], invlet);
		if (_in.has_key("powered")) fromJSON(_in["powered"], powered);
		if (_in.has_key("charge")) fromJSON(_in["charge"], charge);
		return;
	}
	is >> id >> invlet >> powered >> charge;	// \todo release block: remove legacy reading
}

std::ostream& operator<<(std::ostream& os, const bionic& src)
{
	JSON _bionic;
	_bionic.set("id", toJSON(src.id));
	if ('a' != src.invlet) {
		const char str[] = { src.invlet, '\x00' };
		_bionic.set("invlet", str);
	}
	if (src.powered) _bionic.set("powered", "true");
	if (0 < src.charge) _bionic.set("charge", std::to_string(src.charge));
	return os << _bionic;
}

npc_favor::npc_favor(std::istream& is)
: type(FAVOR_NULL),value(0),item_id(itm_null),skill_id(sk_null)
{
	is >> type >> value >> item_id >> skill_id;
}

std::istream& operator>>(std::istream& is, npc_favor& dest)
{
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
//		if (!_in.has_key("type") || !fromJSON(_in["type"], dest.type)) throw std::runtime_error("unrecognized favor");
//		if (!_in.has_key("favor")) throw std::runtime_error("favor reward AWOL");
		if (JSON::object != _in.mode()) return is;
		if (!_in.has_key("type") || !fromJSON(_in["type"], dest.type)) return is;
		if (!_in.has_key("favor")) return is;
		auto& x = _in["favor"];
		switch (dest.type)
		{
		case FAVOR_CASH: if (!fromJSON(x,dest.value)) throw std::runtime_error("unparsed favor reward");
			break;
		case FAVOR_ITEM: if (!fromJSON(x, dest.item_id)) throw std::runtime_error("unparsed favor reward");
			break;
		case FAVOR_TRAINING: if (!fromJSON(x, dest.skill_id)) throw std::runtime_error("unparsed favor reward");
			break;
		default: throw std::runtime_error("unhandled favor type");
		}
		return is;
	}

	return is >> dest.type >> dest.value >> dest.item_id >> dest.skill_id;
}

std::ostream& operator<<(std::ostream& os, const npc_favor& src)
{
	if (!src.type) return os << "{}"; // temporary
	JSON _favor;
	_favor.set("type", toJSON(src.type));
	switch (src.type)
	{
	case FAVOR_CASH: _favor.set("favor", std::to_string(src.value)); break;
	case FAVOR_ITEM: _favor.set("favor", JSON_key(src.item_id)); break;
	case FAVOR_TRAINING: _favor.set("favor", JSON_key(src.skill_id)); break;
//	default: throw std::runtime_error("unhandled favor type");
	}
	return os << _favor;
}

JSON toJSON(const npc_favor& src) {
	JSON _favor;
	_favor.set("type", toJSON(src.type));
	switch (src.type)
	{
	case FAVOR_CASH: _favor.set("favor", std::to_string(src.value)); break;
	case FAVOR_ITEM: _favor.set("favor", JSON_key(src.item_id)); break;
	case FAVOR_TRAINING: _favor.set("favor", JSON_key(src.skill_id)); break;
	default: throw std::runtime_error("unhandled favor type");
	}
	return _favor;
}

bool fromJSON(const JSON& src, npc_favor& dest)
{
	if (JSON::object != src.mode()) return false;
	npc_favor working;
	if (!src.has_key("type") || !fromJSON(src["type"], working.type)) return false;
	if (!src.has_key("favor")) return false;
	auto& x = src["favor"];
	switch (working.type)
	{
	case FAVOR_CASH: if (!fromJSON(x, working.value)) return false;
		break;
	case FAVOR_ITEM: if (!fromJSON(x, working.item_id)) return false;
		break;
	case FAVOR_TRAINING: if (!fromJSON(x, working.skill_id)) return false;
		break;
	default: return false;
	}
	dest = working;
	return true;
}

mission::mission(std::istream& is)
 : type(0),description(""),failed(false),value(0),uid(-1),target(-1,-1),
   item_id(itm_null),count(0),deadline(0),npc_id(-1),good_fac_id(-1),
   bad_fac_id(-1),step(0),follow_up(MISSION_NULL)
{
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
		//		if (!_in.has_key("type") || !fromJSON(_in["type"], dest.type)) throw std::runtime_error("unrecognized mission");
		//		if (!_in.has_key("favor")) throw std::runtime_error("favor reward AWOL");
		if (!_in.has_key("type") || !fromJSON(_in["type"], type)) return;
		if (!_in.has_key("uid") || !fromJSON(_in["uid"], uid) || 0>=uid) return;
		if (_in.has_key("description")) fromJSON(_in["description"],description);
		if (_in.has_key("failed")) fromJSON(_in["failed"], failed);
		if (_in.has_key("reward")) fromJSON(_in["reward"], reward);
		if (_in.has_key("target")) fromJSON(_in["target"], target);
		if (_in.has_key("item")) fromJSON(_in["item"], item_id);
		if (_in.has_key("count")) fromJSON(_in["count"], count);
		if (_in.has_key("deadline")) fromJSON(_in["deadline"], count);
		if (_in.has_key("npc")) fromJSON(_in["npc"], npc_id);
		if (_in.has_key("by_faction")) fromJSON(_in["by_faction"], good_fac_id);
		if (_in.has_key("vs_faction")) fromJSON(_in["vs_faction"], bad_fac_id);
		if (_in.has_key("step")) fromJSON(_in["step"], step);
		if (_in.has_key("next")) fromJSON(_in["next"], follow_up);
		return;
	}
	// \todo release block: remove legacy reading
	is >> type;
	std::string tmpdesc;
	do {
		is >> tmpdesc;
		if (tmpdesc != "<>") description += tmpdesc + " ";
	} while (tmpdesc != "<>");
	description = description.substr(0, description.size() - 1); // Ending ' '
	is >> failed >> value >> reward >> uid >> target >> item_id >> count >> deadline 
	   >> npc_id >> good_fac_id >> bad_fac_id >> step >> follow_up;
}


std::ostream& operator<<(std::ostream& os, const mission& src)
{
	if (!src.type || 0>=src.type->id || NUM_MISSION_IDS<=src.type->id) return os << "{}"; // temporary
	if (0 >= src.uid) return os << "{}"; // temporary
	JSON _mission;
	_mission.set("type", toJSON(src.type));
	_mission.set("uid", std::to_string(src.uid));
	if (!src.description.empty()) _mission.set("description",src.description);
	if (src.failed) _mission.set("failed", "true");
	if (src.value) _mission.set("value", std::to_string(src.value));	// for now allow negative mission values \todo audit for negative mission values
	if (JSON_key(src.reward.type)) _mission.set("value", toJSON(src.reward));
	// src.reward
	if (point(-1, -1) != src.target) _mission.set("target", toJSON(src.target));
	if (src.count) {
		if (auto json = JSON_key(src.item_id)) {
			_mission.set("item", json);
			_mission.set("count", std::to_string(src.count));
		}
	}
	if (0 < src.deadline) _mission.set("deadline", std::to_string(src.deadline));
	if (0 < src.npc_id) _mission.set("npc", std::to_string(src.npc_id));	// \todo release block: is this always-valid?
	if (0 <= src.good_fac_id) _mission.set("by_faction", std::to_string(src.good_fac_id));
	if (0 <= src.bad_fac_id) _mission.set("vs_faction", std::to_string(src.bad_fac_id));
	if (src.step) _mission.set("step", std::to_string(src.step));
	{
	if (auto json = JSON_key(src.follow_up)) _mission.set("next", json);
	}

	return os << _mission;
}

mongroup::mongroup(std::istream& is)
: type(mcat_null), pos(-1,-1), radius(0), population(0), dying(false)
{
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
		int tmp;
		if (!_in.has_key("type") || !fromJSON(_in["type"], type)) return;
		if (!_in.has_key("pos") || !fromJSON(_in["pos"], pos)) return;
		if (!_in.has_key("radius") || !fromJSON(_in["radius"], tmp)) return;
		if (!_in.has_key("population") || !fromJSON(_in["population"], population)) return;
		radius = tmp;
		if (_in.has_key("dying")) fromJSON(_in["dying"], dying);
		return;
	}
	// \todo release block: remove legacy reading
  int tmp_radius;
  is >> type >> pos >> tmp_radius >> population;	// XXX note absence of src.dying: saveload cancels that
  radius = tmp_radius;
}

std::ostream& operator<<(std::ostream& os, const mongroup& src)
{
	JSON _mongroup;
	_mongroup.set("type", toJSON(src.type));
	_mongroup.set("pos", toJSON(src.pos));
	_mongroup.set("radius", std::to_string(src.radius));
	_mongroup.set("population", std::to_string(src.population));
	if (src.dying) _mongroup.set("dying", "true");
	return os << _mongroup;
}

std::istream& operator>>(std::istream& is, field& dest)
{
	if ('{' == (is >> std::ws).peek()) {
		const JSON _in(is);
		if (_in.has_key("type")) {
			if (!fromJSON(_in["type"], dest.type)) return is;
			if (_in.has_key("density")) {
				int tmp;
				fromJSON(_in["density"], tmp);
				dest.density = tmp;
			}
			if (_in.has_key("age")) fromJSON(_in["age"], dest.age);
		} else dest = field();
		return is;
	}
	// \todo release block: remove legacy reading
	int d;
	is >> dest.type >> d >> dest.age;
	dest.density = d;
	return is;
}

std::ostream& operator<<(std::ostream& os, const field& src)
{
	if (const auto json = JSON_key(src.type)) {
		JSON _field;
		_field.set("type", json);
		_field.set("density", std::to_string((int)src.density));
		_field.set("age", std::to_string(src.age));
		return os << _field;
	} else return os << "{}";
}

// Arrays are not plausible for the final format for the overmap data classes, but
// they are easily distinguished from objects so we have a clear upgrade path.
// Plan is to reserve the object form for when the absolute coordinate system is understood.
city::city(std::istream& is, bool is_road)
: s(0)
{	// the difference between a city, and a road, is the radius (roads have zero radius)
	if ('[' == (is >> std::ws).peek()) {
		JSON _in(is);
		const size_t _size = _in.size();

		if ((2 != _size && 3!= _size) || JSON::array != _in.mode()) throw std::runtime_error("point expected to be a length 2 array");
		if (!fromJSON(_in[0], x) || !fromJSON(_in[1], y)) throw std::runtime_error("point wants integer coordinates");
		if (3 == _size && !fromJSON(_in[2], s)) throw std::runtime_error("point wants integer coordinates");
		return;
	}
	// \todo release block: remove legacy reading
	is >> x >> y;
	if (!is_road) is >> s;
}

std::ostream& operator<<(std::ostream& os, const city& src)
{
	os << '[' << std::to_string(src.x) << ',' << std::to_string(src.y);
	if (0 < src.s) os << ',' << std::to_string(src.s);
	return os << ']';
}

om_note::om_note(std::istream& is)
: x(-1), y(-1), num(-1), text("")
{
	if ('[' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (JSON::array != _in.mode() || 4 != _in.size()) throw std::runtime_error("om_note expected to be a length 4 array");
		fromJSON(_in[0], x);
		fromJSON(_in[1], y);
		fromJSON(_in[2], num);
		fromJSON(_in[3], text);
		return;
	}
	// \todo release block: remove legacy reading
	is >> x >> y >> num;
	getline(is, text);	// Chomp endl
	getline(is, text);
}

std::ostream& operator<<(std::ostream& os, const om_note& src)
{
	return os << '[' << std::to_string(src.x) <<  ',' << std::to_string(src.y) <<  ',' << std::to_string(src.num) << ',' << JSON(src.text) << ']';
}

radio_tower::radio_tower(std::istream& is)
{
	if ('[' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (JSON::array != _in.mode() || 4 != _in.size()) throw std::runtime_error("radio_tower expected to be a length 4 array");
		fromJSON(_in[0], x);
		fromJSON(_in[1], y);
		fromJSON(_in[2], strength);
		fromJSON(_in[3], message);
		return;
	}
	// \todo release block: remove legacy reading
	is >> x >> y >> strength;
	getline(is, message);	// Chomp endl
	getline(is, message);
}

std::ostream& operator<<(std::ostream& os, const radio_tower& src)
{
	return os << '[' << std::to_string(src.x) << ',' << std::to_string(src.y) << ',' << std::to_string(src.strength) << ',' << JSON(src.message) << ']';
}

std::istream& operator>>(std::istream& is, player_activity& dest)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (!_in.has_key("type") || !fromJSON(_in["type"], dest.type)) return is;
		if (_in.has_key("moves_left")) fromJSON(_in["moves_left"], dest.moves_left);
		if (_in.has_key("index")) fromJSON(_in["index"], dest.index);
		if (_in.has_key("placement")) fromJSON(_in["placement"], dest.placement);
		if (_in.has_key("values")) _in["values"].decode(dest.values);
		return is;
	}
	// \todo release block: remove legacy reading
	int tmp;
	is >> dest.type >> dest.moves_left >> dest.index >> dest.placement >> tmp;
	for (int i = 0; i < tmp; i++) {
		int tmp2;
		is >> tmp2;
		dest.values.push_back(tmp2);
	}
	return is;
}

std::ostream& operator<<(std::ostream& os, const player_activity& src)
{
	if (const auto json = JSON_key(src.type)) {
		JSON _act;
		_act.set("type", json);
		if (0 != src.moves_left) _act.set("moves_left", std::to_string(src.moves_left));
		if (-1 != src.index) _act.set("index", std::to_string(src.index));
		if (point(-1,-1) != src.placement) _act.set("placement", toJSON(src.placement));
		if (!src.values.empty()) _act.set("values", JSON::encode(src.values));
		return os << _act;
	} else return os << "{}"; // temporary
}

spawn_point::spawn_point(std::istream& is)
: pos(-1,-1), count(0), type(mon_null), faction_id(-1), mission_id(-1), friendly(false), name("NONE")
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (!_in.has_key("type") || !fromJSON(_in["type"], type)) return;
		if (_in.has_key("name")) fromJSON(_in["name"], name);
		if (_in.has_key("count")) fromJSON(_in["count"], count);
		if (_in.has_key("pos")) fromJSON(_in["pos"], pos);
		if (_in.has_key("faction_id")) fromJSON(_in["faction_id"], faction_id);
		if (_in.has_key("mission_id")) fromJSON(_in["mission_id"], mission_id);
		if (_in.has_key("friendly")) fromJSON(_in["friendly"], friendly);
		return;
	}
	// \todo release block: remove legacy reading
	char tmpfriend;
	is >> type >> count >> pos >> faction_id >> mission_id >> tmpfriend >> name;
	friendly = '1' == tmpfriend;
}

std::ostream& operator<<(std::ostream& os, const spawn_point& src)
{
	if (const auto json = JSON_key(src.type)) {
		JSON _spawn;
		_spawn.set("type", json);
		_spawn.set("name", src.name);
		if (0 != src.count) _spawn.set("count", std::to_string(src.count));
		if (point(-1, -1) != src.pos) _spawn.set("pos", toJSON(src.pos));
		if (0 <= src.faction_id) _spawn.set("faction_id", std::to_string(src.faction_id));
		if (0 < src.mission_id) _spawn.set("mission_id", std::to_string(src.mission_id));
		if (src.friendly) _spawn.set("friendly", "true");
		return os << _spawn;
	}
	else return os << "{}"; // temporary
}

// \todo release block: submap::submap,operator<< need at least partial JSON conversion
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
			is >> trp[itx][ity];
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
		} else if ("----" == string_identifier) {
			is >> std::ws;	// to ensure we don't warn on trailing whitespace at end of file
			break;
		} else {
			debugmsg((std::string("Unrecognized map data key '")+ string_identifier+"'").c_str());
			std::string databuff;
			getline(is, databuff);
			debugmsg((std::string("discarding  '") + databuff + "'").c_str());
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

// \todo release block: vehicle_part: operator>>,operator<< JSON conversion blocked by items
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
: name(""), values(0), goal(FACGOAL_NULL), job1(FACJOB_NULL), job2(FACJOB_NULL),
  likes_u(0), respects_u(0), known_by_u(false), id(-1),
  strength(0), sneak(0), crime(0), cult(0), good(0),
  om(0, 0), map(0, 0), size(0), power(0)
{
	if ('{' == (is >> std::ws).peek()) {
		cataclysm::JSON_parse<faction_value> _parse;
		JSON _in(is);
		if (!_in.has_key("id") || !fromJSON(_in["id"], id)) return;	// \todo do we want to interpolate this key?
		if (_in.has_key("name")) fromJSON(_in["name"], name);
		if (_in.has_key("values")) {
			std::vector<const char*> relay;
			_in["values"].decode(relay);
			if (!relay.empty()) values = _parse(relay);
		}
		if (_in.has_key("goal")) fromJSON(_in["goal"], goal);
		if (_in.has_key("job1")) fromJSON(_in["job1"], job1);
		if (_in.has_key("job2")) fromJSON(_in["job2"], job2);
		if (_in.has_key("u")) {
			const auto& tmp = _in["u"];
			if (tmp.has_key("likes")) fromJSON(tmp["likes"], likes_u);
			if (tmp.has_key("respects")) fromJSON(tmp["respects"], respects_u);
			if (tmp.has_key("known_by")) fromJSON(tmp["known_by"], known_by_u);
		}
		if (_in.has_key("ethics")) {
			const auto& tmp = _in["ethics"];
			if (tmp.has_key("strength")) fromJSON(tmp["strength"], strength);
			if (tmp.has_key("sneak")) fromJSON(tmp["sneak"], sneak);
			if (tmp.has_key("crime")) fromJSON(tmp["crime"], crime);
			if (tmp.has_key("cult")) fromJSON(tmp["cult"], cult);
			if (tmp.has_key("good")) fromJSON(tmp["good"], good);
		}
		if (_in.has_key("om")) fromJSON(_in["om"], om);
		if (_in.has_key("map")) fromJSON(_in["map"], job2);
		if (_in.has_key("size")) fromJSON(_in["size"], job2);
		if (_in.has_key("power")) fromJSON(_in["power"], job2);
		return;
	}
	// \todo release block: remove legacy reading
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
	std::getline(is >> std::ws, name);
}

std::ostream& operator<<(std::ostream& os, const faction& src)
{
	cataclysm::JSON_parse<faction_value> _parse;
	JSON _faction;
	_faction.set("id", std::to_string(src.id));
	_faction.set("name", src.name);
	if (src.values) {
		auto tmp = _parse(src.values);
		if (!tmp.empty()) _faction.set("values", JSON::encode(tmp));
	}
	if (auto json = JSON_key(src.goal)) _faction.set("goal",json);
	if (auto json = JSON_key(src.job1)) _faction.set("job1", json);
	if (auto json = JSON_key(src.job2)) _faction.set("job2", json);
	{
	JSON tmp;
	tmp.set("likes", std::to_string(src.likes_u));
	tmp.set("respects", std::to_string(src.respects_u));
	if (src.known_by_u) tmp.set("known_by", "true");
	_faction.set("u", std::move(tmp));	// ultimately we'd like the same format for the PC, and NPCs
	tmp.set("strength", std::to_string(src.strength));
	tmp.set("sneak", std::to_string(src.sneak));
	tmp.set("crime", std::to_string(src.crime));
	tmp.set("cult", std::to_string(src.cult));
	tmp.set("good", std::to_string(src.good));
	_faction.set("ethics", tmp);
	}
	_faction.set("om", toJSON(src.om));
	_faction.set("map", toJSON(src.map));
	_faction.set("size", std::to_string(src.size));
	_faction.set("power", std::to_string(src.power));
	return os << _faction;
}

// \todo release block JSON support for items (blocks other classes)
item::item(std::istream& is)
: type(0),corpse(0),curammo(0),name(""),invlet(0),charges(-1),active(false),
  damage(0),burnt(0),bday(0),owned(-1),poison(0),mission_id(-1),player_id(-1)
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

// \todo release block: JSON save/load support (repairs monster inventories, effects being dropped in save/load cycle
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

// \todo release block JSON support for artifact armors
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
	while((endline = desctmp.find('\n')) != std::string::npos) desctmp.replace(endline, 1, " = ");

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

// \todo release block JSON support for artifact tools
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
	while ((endline = desctmp.find('\n')) != std::string::npos) desctmp.replace(endline, 1, " = ");
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
: type(DI_NULL), intensity(0), duration(0)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (_in.has_key("type")) fromJSON(_in["type"], type);
		if (_in.has_key("duration")) fromJSON(_in["duration"], duration);
		if (_in.has_key("intensity")) fromJSON(_in["intensity"], intensity);
		return;
	}
	// \todo release block: remove legacy reading
	is >> type >> duration;
}

std::ostream& operator<<(std::ostream& os, const disease& src)
{
	if (auto json = JSON_key(src.type)) {
		JSON _disease;
		_disease.set("type", json);
		_disease.set("duration", std::to_string(src.duration));
		_disease.set("intensity", std::to_string(src.intensity));
		return os << _disease;
	}
	else return os << "{}";
}

addiction::addiction(std::istream& is)
: type(ADD_NULL), intensity(0), sated(600)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (_in.has_key("type")) fromJSON(_in["type"], type);
		if (_in.has_key("intensity")) fromJSON(_in["intensity"], intensity);
		if (_in.has_key("sated")) fromJSON(_in["sated"], sated);
		return;
	}
	// \todo release block: remove legacy reading
	is >> type >> intensity >> sated;
}

std::ostream& operator<<(std::ostream& os, const addiction& src)
{
	if (auto json = JSON_key(src.type)) {
		JSON _addiction;
		_addiction.set("type", json);
		_addiction.set("intensity", std::to_string(src.intensity));
		_addiction.set("sated", std::to_string(src.sated));
		return os << _addiction;
	} else return os << "{}";
}

morale_point::morale_point(std::istream& is)
: type(MORALE_NULL), item_type(0), bonus(0)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (!_in.has_key("type") || !fromJSON(_in["type"], type)) return;
		if (_in.has_key("bonus")) fromJSON(_in["bonus"], bonus);
		if (_in.has_key("item")) fromJSON(_in["item"], item_type);
		return;
	}
	// \todo release block: remove legacy reading
	is >> bonus >> type >> item_type;
	if (item_type && itm_null == item_type->id) item_type = 0;	// historically, itm_null was the encoding for null pointer
}

std::ostream& operator<<(std::ostream& os, const morale_point& src)
{
	if (auto json = JSON_key(src.type)) {
		JSON _morale;
		_morale.set("type", json);
		if (0 != src.bonus) _morale.set("bonus", std::to_string(src.bonus));
		if (src.item_type) {
			if (auto json2 = JSON_key((itype_id)(src.item_type->id))) _morale.set("item", json2);	// \todo eliminate this cast?
		}
		return os << _morale;
	} else return os << "{}";
}

// We have an improper inheritance player -> npc (ideal difference would be the AI controller class, cf. Rogue Survivor game family
// -- but C++ is too close to the machine for the savefile issues to be easily managed.  Rely on data structures to keep 
// the save of a non-final class to hard drive to disambiguate.

// 2019-03-24: work required to make player object a proper base object of the npc object not plausibly mechanical.
std::istream& operator>>(std::istream& is, player& dest)
{
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

	int numill;
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
	for (int i = 0; i < PF_MAX2; i++) os << src.my_mutations[i] I_SEP;	// XXX mutation info not save/loaded by NPC in C:Whales
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
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (_in.has_key("trust")) fromJSON(_in["trust"], dest.trust);
		if (_in.has_key("fear")) fromJSON(_in["fear"], dest.fear);
		if (_in.has_key("value")) fromJSON(_in["value"], dest.value);
		if (_in.has_key("anger")) fromJSON(_in["anger"], dest.anger);
		if (_in.has_key("owed")) fromJSON(_in["owed"], dest.owed);
		if (_in.has_key("favors")) _in["favors"].decode(dest.favors);
		return is;
	}
	// \todo release block: remove legacy reading
	int tmpsize;
	is >> dest.trust >> dest.fear >> dest.value >> dest.anger >> dest.owed >> tmpsize;
	for (int i = 0; i < tmpsize; i++) dest.favors.push_back(npc_favor(is));
	return is;
}

std::ostream& operator<<(std::ostream& os, const npc_opinion& src)
{
	JSON _opinion;
	_opinion.set("trust", std::to_string(src.trust));
	_opinion.set("fear", std::to_string(src.fear));
	_opinion.set("value", std::to_string(src.value));
	_opinion.set("anger", std::to_string(src.anger));
	_opinion.set("owed", std::to_string(src.owed));
	if (!src.favors.empty()) _opinion.set("favors", JSON::encode(src.favors));
	return os << _opinion;
}

std::istream& operator>>(std::istream& is, npc_chatbin& dest)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		if (!_in.has_key("first_topic") || !fromJSON(_in["first_topic"], dest.first_topic)) return is;

		// \todo: verify whether missions are fully loaded before the chatbins are loaded
		// \todo: verify whether mission_selected and tempvalue are UI-local, thus not needed in the savefile
		if (_in.has_key("mission_selected")) fromJSON(_in["mission_selected"], dest.mission_selected);
		if (_in.has_key("tempvalue")) fromJSON(_in["tempvalue"], dest.tempvalue);
		if (_in.has_key("missions")) _in["missions"].decode(dest.missions);
		if (_in.has_key("missions_assigned")) _in["missions_assigned"].decode(dest.missions_assigned);
		return is;
	}
	// \todo release block: remove legacy reading
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
	if (auto json = JSON_key(src.first_topic)) {
		JSON _chatbin;
		_chatbin.set("first_topic", json);
		// \todo: try to purge invalid mission ids before checking size, etc.
		if (0 <= src.mission_selected && (src.missions.size() > src.mission_selected || src.missions_assigned.size() > src.mission_selected)) {
			_chatbin.set("mission_selected", std::to_string(src.mission_selected));
		}
		_chatbin.set("tempvalue", std::to_string(src.tempvalue));
		if (!src.missions.empty()) _chatbin.set("missions", JSON::encode(src.missions));
		if (!src.missions_assigned.empty()) _chatbin.set("missions_assigned", JSON::encode(src.missions_assigned));
		return os << _chatbin;
	} else return os << "{}";
}

std::istream& operator>>(std::istream& is, npc_personality& dest)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		int tmp;
		if (_in.has_key("aggression") && fromJSON(_in["aggression"], tmp)) dest.aggression = (signed char)tmp;
		if (_in.has_key("bravery") && fromJSON(_in["bravery"], tmp)) dest.bravery = (signed char)tmp;
		if (_in.has_key("collector") && fromJSON(_in["collector"], tmp)) dest.collector = (signed char)tmp;
		if (_in.has_key("altruism") && fromJSON(_in["altruism"], tmp)) dest.altruism = (signed char)tmp;
		return is;
	}
	// \todo release block: remove legacy reading
	int agg, bra, col, alt;
	is >> agg >> bra >> col >> alt;
	dest.aggression = agg;
	dest.bravery = bra;
	dest.collector = col;
	dest.altruism = alt;
	return is;
}

std::ostream& operator<<(std::ostream& os, const npc_personality& src)
{
	JSON _personality;
	_personality.set("aggression", std::to_string((int)src.aggression));
	_personality.set("bravery", std::to_string((int)src.bravery));
	_personality.set("collector", std::to_string((int)src.collector));
	_personality.set("altruism", std::to_string((int)src.altruism));
	return os << _personality;
}

std::istream& operator>>(std::istream& is, npc_combat_rules& dest)
{
	if ('{' == (is >> std::ws).peek()) {
		JSON _in(is);
		int tmp;
		if (!_in.has_key("engagement") || !fromJSON(_in["engagement"], dest.engagement)) return is;
		if (_in.has_key("use_guns")) fromJSON(_in["use_guns"], dest.use_guns);
		if (_in.has_key("use_grenades")) fromJSON(_in["use_grenades"], dest.use_grenades);
		return is;
	}
	// \todo release block: remove legacy reading.  This unblocks re-ordering enum combat_engagement so that the default (ENGAGE_ALL) has value zero
	return is >> dest.engagement >> dest.use_guns >> dest.use_grenades;
}

std::ostream& operator<<(std::ostream& os, const npc_combat_rules& src)
{
	if (auto json = JSON_key(src.engagement)) {
		JSON _engage;
		_engage.set("engagement", json);
		if (!src.use_guns) _engage.set("use_guns", "false");
		if (!src.use_grenades) _engage.set("use_grenades", "false");
		return os << _engage;
	} else return os << "{}";
}

npc::npc(std::istream& is)
{
	std::string tmpname;
	is >> id;
	// Standard player stuff
	do {
		is >> tmpname;
		if (tmpname != "||") name += tmpname + " ";
	} while (tmpname != "||");
	name = name.substr(0, name.size() - 1); // Strip off trailing " "
	is >> pos >> str_cur >> str_max >> dex_cur >> dex_max >>
		int_cur >> int_max >> per_cur >> per_max >> hunger >> thirst >>
		fatigue >> stim >> pain >> pkill >> radiation >> cash >> recoil >>
		scent >> moves >> underwater >> dodges_left >> oxygen >> marked_for_death >>
		dead >> myclass >> patience;

	for (int i = 0; i < PF_MAX2; i++) is >> my_traits[i];
	for (int i = 0; i < num_hp_parts; i++) is >> hp_cur[i] >> hp_max[i];
	for (int i = 0; i < num_skill_types; i++) is >> sklevel[i] >> skexercise[i];

	int numstyles;
	is >> numstyles;
	for (int i = 0; i < numstyles; i++) {
		itype_id tmp;
		is >> tmp;
		styles.push_back(tmp);
	}

	int numill;
	is >> numill;
	for (int i = 0; i < numill; i++) illness.push_back(disease(is));

	int numadd = 0;
	is >> numadd;
	for (int i = 0; i < numadd; i++) addictions.push_back(addiction(is));

	int numbio = 0;
	is >> numbio;
	for (int i = 0; i < numbio; i++) my_bionics.push_back(bionic(is));

	// Special NPC stuff
	int flagstmp;
	is >> personality >> wand >> wandf >> om >>
		mapx >> mapy >> pl >> goal >> mission >>
		flagstmp >> fac_id >> attitude;	// V 0.2.0 blocker \todo reconstitute faction* my_fac
	flags = flagstmp;

	is >> op_of_u;
	is >> chatbin;
	is >> combat_rules;
}

std::ostream& operator<<(std::ostream& os, const npc& src)
{
	// The " || " is what tells npc::load_info() that it's down reading the name
	os << src.id I_SEP << src.name << " || " << src.pos I_SEP << src.str_cur I_SEP <<
		src.str_max I_SEP << src.dex_cur I_SEP << src.dex_max I_SEP << src.int_cur I_SEP <<
		src.int_max I_SEP << src.per_cur I_SEP << src.per_max I_SEP << src.hunger I_SEP <<
		src.thirst I_SEP << src.fatigue I_SEP << src.stim I_SEP << src.pain I_SEP <<
		src.pkill I_SEP << src.radiation I_SEP << src.cash I_SEP << src.recoil I_SEP <<
		src.scent I_SEP << src.moves I_SEP << src.underwater I_SEP << src.dodges_left I_SEP <<
		src.oxygen I_SEP << src.marked_for_death I_SEP <<
		src.dead I_SEP << src.myclass I_SEP << src.patience I_SEP;

	for (int i = 0; i < PF_MAX2; i++) os << src.my_traits[i] I_SEP;
	for (int i = 0; i < num_hp_parts; i++) os << src.hp_cur[i] I_SEP << src.hp_max[i] I_SEP;
	for (int i = 0; i < num_skill_types; i++) os << src.sklevel[i] I_SEP << src.skexercise[i] I_SEP;

	os << src.styles.size() I_SEP;
	for (int i = 0; i < src.styles.size(); i++) os << src.styles[i] I_SEP;

	os << src.illness.size() I_SEP;
	for (const auto& ill : src.illness) os << ill I_SEP;

	os << src.addictions.size() I_SEP;
	for (const auto& add : src.addictions) os << add I_SEP;

	os << src.my_bionics.size() I_SEP;
	for (const auto& bio : src.my_bionics)  os << bio I_SEP;

	// NPC-specific stuff
	os << src.personality I_SEP << src.wand I_SEP << src.wandf I_SEP <<
		src.om I_SEP << src.mapx I_SEP << src.mapy I_SEP << src.pl I_SEP <<
		src.goal I_SEP << src.mission I_SEP << int(src.flags) I_SEP;	// blocker V 0.2.0 \todo npc::it missing here
	os << (src.my_fac == NULL ? -1 : src.my_fac->id);
	os I_SEP << src.attitude I_SEP << src.op_of_u I_SEP << src.chatbin I_SEP << src.combat_rules I_SEP;

	// Inventory size, plus armor size, plus 1 for the weapon
	os << std::endl;

	// V 0.2.0 blocker \todo asymmetric, not handled in istream constructor
	os << src.inv.num_items() + src.worn.size() + 1 << std::endl;
	for (size_t i = 0; i < src.inv.size(); i++) {
		for (const auto& it : src.inv.stack_at(i)) {
			os << "I " << it << std::endl;
			for (const auto& it_2 : it.contents) os << "C " << it_2 << std::endl;	// blocker V 0.2.0 \todo should already be handled
		}
	}
	os << "w " << src.weapon << std::endl;
	for (const auto& it : src.worn) os << "W " << it << std::endl;
	for (const auto& it : src.weapon.contents) os << "c " << it << std::endl;	// blocker V 0.2.0 \todo should already be handled

	return os;
}
