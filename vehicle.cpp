#include "vehicle.h"
#include "game.h"
#include "recent_msg.h"
#include "saveload.h"

#include <stdlib.h>
#include <math.h>
#include <sstream>
#include <fstream>

const int fuel_types[] = { AT_GAS, AT_BATT, AT_PLUT, AT_PLASMA };
#define num_fuel_types (sizeof(fuel_types)/sizeof(*fuel_types))

static const char* const JSON_transcode_vparts[] = {
	"seat",
	"frame_h",
	"frame_v",
	"frame_c",
	"frame_y",
	"frame_u",
	"frame_n",
	"frame_b",
	"frame_h2",
	"frame_v2",
	"frame_cover",
	"frame_handle",
	"board_h",
	"board_v",
	"board_y",
	"board_u",
	"board_n",
	"board_b",
	"roof",
	"door",
	"window",
	"blade_h",
	"blade_v",
	"spike_h",
	"spike_v",
	"wheel_large",
	"wheel",
	"engine_gas_small",
	"engine_gas_med",
	"engine_gas_large",
	"engine_motor",
	"engine_motor_large",
	"engine_plasma",
	"fuel_tank_gas",
	"fuel_tank_batt",
	"fuel_tank_plut",
	"fuel_tank_hydrogen",
	"cargo_trunk",
	"cargo_box",
	"controls",
	"muffler",
	"seatbelt",
	"solar_panel",
	"m249",
	"flamethrower",
	"plasmagun",
	"steel_plate",
	"superalloy_plate",
	"spiked_plate",
	"hard_plate"
};

static const char* const JSON_transcode_vtypes[] = {
	"custom",
	"motorcycle",
	"sandbike",
	"car",
	"truck"
};

DEFINE_JSON_ENUM_SUPPORT_TYPICAL(vhtype_id, JSON_transcode_vtypes)
DEFINE_JSON_ENUM_SUPPORT_TYPICAL(vpart_id, JSON_transcode_vparts)

std::vector <vehicle*> vehicle::vtypes;

vehicle::vehicle(vhtype_id type_id)
: _type(type_id), insides_dirty(true), pos(0,0), velocity(0), cruise_velocity(0), cruise_on(true),
  turn_dir(0), skidding(false), last_turn(0), moves(0), turret_mode(0)
{
    if (veh_custom < _type && vehicle::vtypes.size() > _type)
    {   // get a copy of sample vehicle of this type
        *this = *(vehicle::vtypes[_type]);
        init_state();
    }
    precalc_mounts(0, face.dir());
}

DEFINE_ACID_ASSIGN_W_MOVE(vehicle)

bool vehicle::player_in_control(player& p) const
{
    if (!_type || !p.in_vehicle) return false;
    int veh_part;
    const vehicle* const veh = game::active()->m.veh_at(p.pos, veh_part);
    if (!veh || veh != this) return false;
    return part_with_feature(veh_part, vpf_controls, false) >= 0;
}

void vehicle::init_state()
{
    for (int p = 0; p < parts.size(); p++)
    {
        if (part_flag(p, vpf_fuel_tank))   // 10% to 75% fuel for tank
            parts[p].amount = rng (part_info(p).size / 10, part_info(p).size * 3 / 4);
        if (part_flag(p, vpf_openable))    // doors are closed
            parts[p].open = 0;
        if (part_flag(p, vpf_seat))        // no passengers
            parts[p].passenger = 0;
    }
}

const vpart_info& vehicle::part_info(int index) const
{
    vpart_id id = (index < 0 || index >= parts.size()) ? vp_null : parts[index].id;
    if (id < vp_null || id >= num_vparts) id = vp_null;
    return vpart_info::list[id];
}

bool vehicle::can_mount (int dx, int dy, vpart_id id) const
{
    if (id <= 0 || id >= num_vparts) return false;
	const auto& p_info = vpart_info::list[id];
    bool n3ar = parts.empty() || (p_info.flags & (mfb(vpf_internal) | mfb(vpf_over))); // first and internal parts needs no mount point
    if (!n3ar)
        for (int i = 0; i < 4; i++)
        {
            int ndx = i < 2? (i == 0? -1 : 1) : 0;
            int ndy = i < 2? 0 : (i == 2? - 1: 1);
            std::vector<int> parts_n3ar = parts_at_relative (dx + ndx, dy + ndy);
            if (parts_n3ar.size() < 1)
                continue;
            if (part_flag(parts_n3ar[0], vpf_mount_point))
            {
                n3ar = true;
                break;
            }
        }
    if (!n3ar) return false; // no point to mount

// TODO: seatbelts must require an obstacle part n3arby

    std::vector<int> parts_here = parts_at_relative (dx, dy);
    if (parts_here.empty()) return p_info.flags & mfb(vpf_external); // can be mounted if first and external

    const int flags1 = part_info(parts_here[0]).flags;
    if (    (p_info.flags & mfb(vpf_armor))
		 && (flags1 & mfb(vpf_no_reinforce)))
		return false;   // trying to put armor plates on non-reinforcable part

    for (int vf = vpf_func_begin; vf <= vpf_func_end; vf++)
        if (    (p_info.flags & mfb(vf))
			 && 0 <= part_with_feature(parts_here[0], vf, false)) {
            return false;   // this part already has inner part with same unique feature
        }

    const bool allow_inner = flags1 & mfb(vpf_mount_inner);
	const bool allow_over  = flags1 & mfb(vpf_mount_over);
	const bool this_inner  = p_info.flags & mfb(vpf_internal);
	const bool this_over   = p_info.flags & (mfb(vpf_over) | mfb(vpf_armor));
    if (allow_inner && (this_inner || this_over)) return true; // can mount as internal part or over it
    if (allow_over && this_over) return true; // can mount as part over
    return false;
}

bool vehicle::can_unmount(int p) const
{
    int dx = parts[p].mount_d.x;
    int dy = parts[p].mount_d.y;
    if (!dx && !dy)
    { // central point
        bool is_ext = false;
        for (int ep = 0; ep < external_parts.size(); ep++)
            if (external_parts[ep] == p)
            {
                is_ext = true;
                break;
            }
        if (external_parts.size() > 1 && is_ext) return false; // unmounting 0, 0 part anly allowed as last part
    }

    if (!part_flag (p, vpf_mount_point)) return true;
    for (int i = 0; i < 4; i++)
    {
        int ndx = i < 2? (i == 0? -1 : 1) : 0;
        int ndy = i < 2? 0 : (i == 2? - 1: 1);
        if (!(dx + ndx) && !(dy + ndy)) continue; // 0, 0 point is main mount
        if (parts_at_relative (dx + ndx, dy + ndy).size() > 0)
        {
            int cnt = 0;
            for (int j = 0; j < 4; j++)
            {
                int jdx = j < 2? (j == 0? -1 : 1) : 0;
                int jdy = j < 2? 0 : (j == 2? - 1: 1);
                std::vector<int> pc = parts_at_relative (dx + ndx + jdx, dy + ndy + jdy);
                if (pc.size() > 0 && part_with_feature (pc[0], vpf_mount_point) >= 0)
                    cnt++;
            }
            if (cnt < 2) return false;
        }
    }
    return true;
}

int vehicle::install_part (int dx, int dy, vpart_id id, int hp, bool force)
{
    if (!force && !can_mount (dx, dy, id))
        return -1;  // no money -- no ski!
    // if this is first part, add this part to list of external parts
    if (parts_at_relative (dx, dy).size () < 1)
        external_parts.push_back (parts.size());
    parts.push_back(vehicle_part(id,dx,dy, hp < 0 ? vpart_info::list[id].durability : hp));
    find_exhaust ();
    precalc_mounts (0, face.dir());
    insides_dirty = true;
    return parts.size() - 1;
}

void vehicle::remove_part (int p)
{
    parts.erase(parts.begin() + p);
    find_external_parts ();
    find_exhaust ();
    precalc_mounts (0, face.dir());    
    insides_dirty = true;
}


std::vector<int> vehicle::parts_at_relative (int dx, int dy) const
{
    std::vector<int> res;
    for (int i = 0; i < parts.size(); i++)
        if (parts[i].mount_d.x == dx && parts[i].mount_d.y == dy)
            res.push_back (i);
    return res;
}

std::vector<int> vehicle::internal_parts(int p) const
{
    std::vector<int> res;
	const auto& ref_part = parts[p];
    for (int i = p + 1; i < parts.size(); i++)
        if (parts[i].mount_d == ref_part.mount_d)
            res.push_back (i);
    return res;
}

int vehicle::part_with_feature(int p, unsigned int f, bool unbroken) const
{
    if (part_flag(p, f)) return p;
    std::vector<int> parts_here = internal_parts(p);
	for (const auto n : parts_here)
        if (part_flag(n, f) && (!unbroken || parts[n].hp > 0))
            return n;
    return -1;
}

bool vehicle::part_flag(int p, unsigned int f) const
{
    if (p < 0 || p >= parts.size()) return false;
    return part_info(p).flags & mfb(f);
}

int vehicle::part_at(int dx, int dy) const
{
	for (const auto p : external_parts) {
		const auto& part = parts[p];
        if (part.precalc_d[0].x == dx &&
			part.precalc_d[0].y == dy)
            return p;
    }
    return -1;
}

char vehicle::part_sym (int p) const
{
    if (p < 0 || p >= parts.size()) return 0;
    std::vector<int> ph = internal_parts (p);
    int po = part_with_feature(p, vpf_over, false);
    int pd = po < 0? p : po;
    if (part_flag (pd, vpf_openable) && parts[pd].open) return '\''; // open door
    return parts[pd].hp <= 0? part_info(pd).sym_broken : part_info(pd).sym;
}

nc_color vehicle::part_color (int p) const
{
    if (p < 0 || p >= parts.size()) return c_black;
    int parm = part_with_feature(p, vpf_armor, false);
    int po = part_with_feature(p, vpf_over, false);
    int pd = po < 0? p : po;
    if (parts[p].blood > 200) return c_red;
    else if (parts[p].blood > 0) return c_ltred;
    
    if (parts[pd].hp <= 0) return part_info(pd).color_broken;

    // first, check if there's a part over. then, if armor here (projects its color on part)
    if (po >= 0) return part_info(po).color;
    else if (parm >= 0) return part_info(parm).color;

    return part_info(pd).color;
}

void vehicle::print_part_desc (void *w, int y1, int width, int p, int hl)
{
    WINDOW *win = (WINDOW *) w;
    if (p < 0 || p >= parts.size())
        return;
    std::vector<int> pl = internal_parts (p);
    pl.insert (pl.begin(), p);
    int y = y1;
    for (int i = 0; i < pl.size(); i++)
    {
        int dur = part_info (pl[i]).durability;
        int per_cond = parts[pl[i]].hp * 100 / (dur < 1? 1 : dur);
        nc_color col_cond = c_dkgray;
        if (parts[pl[i]].hp >= dur)
            col_cond = c_green;
        else
        if (per_cond >= 80)
            col_cond = c_ltgreen;
        else
        if (per_cond >= 50)
            col_cond = c_yellow;
        else
        if (per_cond >= 20)
            col_cond = c_ltred;
        else
        if (parts[pl[i]].hp > 0)
            col_cond = c_red;

        bool armor = part_flag(pl[i], vpf_armor);
        mvwprintz(win, y, 2, i == hl? hilite(col_cond) : col_cond, part_info(pl[i]).name);
        mvwprintz(win, y, 1, i == hl? hilite(c_ltgray) : c_ltgray, armor? "(" : (i? "-" : "["));
        mvwprintz(win, y, 2 + strlen(part_info(pl[i]).name), i == hl? hilite(c_ltgray) : c_ltgray, armor? ")" : (i? "-" : "]"));
//         mvwprintz(win, y, 3 + strlen(part_info(pl[i]).name), c_ltred, "%d", parts[pl[i]].blood);

        if (i == 0)
            mvwprintz(win, y, width-5, c_ltgray, is_inside(pl[i])? " In " : "Out ");
        y++;
    }
}

void vehicle::print_fuel_indicator (void *w, int y, int x)
{
    WINDOW *win = (WINDOW *) w;
    const nc_color fcs[num_fuel_types] = { c_ltred, c_yellow, c_ltgreen, c_ltblue };
    const char fsyms[5] = { 'E', '\\', '|', '/', 'F' };
    nc_color col_indf1 = c_ltgray;
    mvwprintz(win, y, x, col_indf1, "E...F");
    for (int i = 0; i < num_fuel_types; i++) {
        int cap = fuel_capacity(fuel_types[i]);
        if (cap > 0) {
            int amnt = cap > 0? fuel_left(fuel_types[i]) * 99 / cap : 0;
            int indf = (amnt / 20) % 5;
            mvwprintz(win, y, x + indf, fcs[i], "%c", fsyms[indf]);
        }
    }
}

point vehicle::coord_translate (point reld) const
{
    tileray tdir (face.dir());
    tdir.advance (reld.x);
	return point(tdir.dx() + tdir.ortho_dx(reld.y), tdir.dy() + tdir.ortho_dy(reld.y));
}

point vehicle::coord_translate (int dir, point reld)
{
    tileray tdir (dir);
    tdir.advance (reld.x);
	return point(tdir.dx() + tdir.ortho_dx(reld.y), tdir.dy() + tdir.ortho_dy(reld.y));
}

void vehicle::precalc_mounts (int idir, int dir)
{
    if (idir < 0 || idir > 1) idir = 0;
	for(auto& part : parts) part.precalc_d[idir] = coord_translate(dir, part.mount_d);
}

std::vector<int> vehicle::boarded_parts() const
{
    std::vector<int> res;
    for (int p = 0; p < parts.size(); p++)
        if (part_flag (p, vpf_seat) && parts[p].passenger)
            res.push_back (p);
    return res;
}

player *vehicle::get_passenger(int p) const
{
    p = part_with_feature (p, vpf_seat, false);
    if (p >= 0 && parts[p].passenger) {
		const point origin(global() + parts[p].precalc_d[0]);
		auto g = game::active();
        if (g->u.pos == origin && g->u.in_vehicle) return &g->u;
		if (npc* const nPC = g->nPC(origin)) return nPC;	// \todo V0.2.1+ why not require in_vehicle?
    }
    return 0;
}

point vehicle::global() const { return SEEX * sm + pos; }

int vehicle::total_mass() const
{
    int m = 0;
    for (int i = 0; i < parts.size(); i++)
    {
        m += item::types[part_info(i).item]->weight;
		for (const auto& it : parts[i].items) m += it.type->weight;
        if (part_flag(i,vpf_seat) && parts[i].passenger)
            m += 520; // TODO: get real weight
    }
    return m;
}

int vehicle::fuel_left(int ftype, bool for_engine) const
{
    int fl = 0;
	for (int p = 0; p < parts.size(); p++) {
		if (!part_flag(p, vpf_fuel_tank)) continue;
		const auto& p_info = part_info(p);
		if ((ftype == p_info.fuel_type || (for_engine && ftype == AT_BATT && p_info.fuel_type == AT_PLUT)))
			fl += parts[p].amount;
	}
    return fl;
}

int vehicle::fuel_capacity(int ftype) const
{
    int cap = 0;
	for (int p = 0; p < parts.size(); p++) {
		if (!part_flag(p, vpf_fuel_tank)) continue;
		const auto& p_info = part_info(p);
		if (ftype == p_info.fuel_type) cap += p_info.size;
	}
    return cap;
}

int vehicle::refill (int ftype, int amount)
{
    for (int p = 0; p < parts.size(); p++)
    {
        if (part_flag(p, vpf_fuel_tank) &&
            part_info(p).fuel_type == ftype &&
            parts[p].amount < part_info(p).size)
        {
            int need = part_info(p).size - parts[p].amount;
            if (amount < need)
            {
                parts[p].amount += amount;
                return 0;
            }
            else
            {
                parts[p].amount += need;
                amount -= need;
            }
        }
    }
    return amount;
}

std::string vehicle::fuel_name(int ftype)
{
    switch (ftype)
    {
    case AT_GAS:
        return std::string("gasoline");
    case AT_BATT:
        return std::string("batteries");
    case AT_PLUT:
        return std::string("plutonium cells");
    case AT_PLASMA:
        return std::string("hydrogen");
    default:
        return std::string("INVALID FUEL (BUG)");
    }
}

int vehicle::basic_consumption(int ftype) const
{
    if (ftype == AT_PLUT) ftype = AT_BATT;
    int cnt = 0;
    int fcon = 0;
	for (int p = 0; p < parts.size(); p++) {
		if (!part_flag(p, vpf_engine) || 0 >= parts[p].hp) continue;
		const auto& p_info = part_info(p);
        if (ftype == p_info.fuel_type) {
            fcon += p_info.power;
            cnt++;
        }
	}
    if (fcon < 100 && cnt > 0) fcon = 100;
    return fcon;
}

int vehicle::total_power(bool fueled) const
{
    int pwr = 0;
    int cnt = 0;
	for (int p = 0; p < parts.size(); p++) {
		if (!part_flag(p, vpf_engine) || 0 >= parts[p].hp) continue;
		const auto& p_info = part_info(p);
		if (fuel_left(p_info.fuel_type, true) || !fueled) {
			pwr += p_info.power;
			cnt++;
		}
	}
    if (cnt > 1) pwr = pwr * 4 / (4 + cnt -1);
    return pwr;
}

int vehicle::solar_power() const
{
    int pwr = 0;
    for (int p = 0; p < parts.size(); p++)
        if (part_flag(p, vpf_solar_panel) && parts[p].hp > 0)
            pwr += part_info(p).power;
    return pwr;
}

int vehicle::acceleration(bool fueled) const
{
    return (int) (safe_velocity (fueled) * k_mass() / (1 + strain ()) / 10);
}

int vehicle::max_velocity(bool fueled) const
{
    return total_power(fueled) * 80;
}

int vehicle::safe_velocity(bool fueled) const
{
    int pwrs = 0;
    int cnt = 0;
	for (int p = 0; p < parts.size(); p++) {
		if (!part_flag(p, vpf_engine) || 0 >= parts[p].hp) continue;
		const auto& p_info = part_info(p);
		if (fuel_left(p_info.fuel_type, true) || !fueled) {
			int m2c = 100;
			switch (p_info.fuel_type) {
			case AT_GAS:    m2c = 60; break;
			case AT_PLASMA: m2c = 75; break;
			case AT_BATT:   m2c = 90; break;
			}
			pwrs += p_info.power * m2c / 100;
			cnt++;
		}
	}
    if (cnt > 0) pwrs = pwrs * 4 / (4 + cnt -1);
    return (int) (pwrs * k_dynamics() * k_mass()) * 80;
}

int vehicle::noise(bool fueled, bool gas_only) const
{
    int pwrs = 0;
    int cnt = 0;
    int muffle = 100;
    for (int p = 0; p < parts.size(); p++)
        if (part_flag(p, vpf_muffler) && parts[p].hp > 0 && part_info(p).bonus < muffle)
            muffle = part_info(p).bonus;

	for (int p = 0; p < parts.size(); p++) {
		if (!part_flag(p, vpf_engine) || 0 >= parts[p].hp) continue;
		const auto& p_info = part_info(p);
		if (fuel_left(p_info.fuel_type, true) || !fueled) {
			int nc = 10;
			switch (p_info.fuel_type) {
			case AT_GAS:    nc = 25; break;
			case AT_PLASMA: nc = 10; break;
			case AT_BATT:   nc = 3; break;
			}
			if (!gas_only || p_info.fuel_type == AT_GAS) {
				int pwr = p_info.power * nc / 100;
				if (muffle < 100 && (p_info.fuel_type == AT_GAS ||
					p_info.fuel_type == AT_PLASMA))
					pwr = pwr * muffle / 100;
				pwrs += pwr;
				cnt++;
			}
		}
	}
    return pwrs;
}

int vehicle::wheels_area(int *cnt) const
{
    int count = 0;
    int size = 0;
    for (int i = 0; i < external_parts.size(); i++) {
        int p = external_parts[i];
        if (part_flag(p, vpf_wheel) && parts[p].hp > 0) {
            size += part_info(p).size;
            count++;
        }
    }
    if (cnt) *cnt = count;
    return size;
}

float vehicle::k_dynamics() const
{
    const int max_obst = 13;
    int obst[max_obst];
    for (int o = 0; o < max_obst; o++) obst[o] = 0;
    for (int i = 0; i < external_parts.size(); i++) {
        int p = external_parts[i];
        int frame_size = part_flag(p, vpf_obstacle)? 30 : 10;
        int pos = parts[p].mount_d.y + max_obst / 2;
        if (pos < 0) pos = 0;
        if (pos >= max_obst) pos = max_obst -1;
        if (obst[pos] < frame_size) obst[pos] = frame_size;
    }
    int frame_obst = 0;
    for (int o = 0; o < max_obst; o++) frame_obst += obst[o];
    float ae0 = 200.0;
    float fr0 = 1000.0;
    int wa = wheels_area();

    // calculate aerodynamic coefficient
    float ka = ae0 / (ae0 + frame_obst);

    // calculate safe speed reduction due to wheel friction
    float kf = fr0 / (fr0 + wa);

    return ka * kf;
}

float vehicle::k_mass() const
{
    int wa = wheels_area();
    float ma0 = 50.0;

    // calculate safe speed reduction due to mass
    float km = wa > 0? ma0 / (ma0 + total_mass() / 8 / (float) wa) : 0;

    return km;
}

float vehicle::strain() const
{
    int mv = max_velocity();
    int sv = safe_velocity();
    if (mv <= sv)
        mv = sv + 1;
    if (velocity < safe_velocity())
        return 0;
    else
        return (float) (velocity - sv) / (float) (mv - sv);
}

bool vehicle::valid_wheel_config() const
{
    int x1, y1, x2, y2;
    int count = 0;
	for (const auto p : external_parts) {
		if (!part_flag(p, vpf_wheel)) continue;
		const auto& part = parts[p];
        if (part.hp <= 0) continue;
        if (!count)
        {
            x1 = x2 = part.mount_d.x;
            y1 = y2 = part.mount_d.y;
        }
        if (part.mount_d.x < x1) x1 = part.mount_d.x;
        if (part.mount_d.x > x2) x2 = part.mount_d.x;
        if (part.mount_d.y < y1) y1 = part.mount_d.y;
        if (part.mount_d.y > y2) y2 = part.mount_d.y;
        count++;
    }
    if (count < 2) return false;
    float xo = 0, yo = 0;
    float wo = 0, w2;
    for (int p = 0; p < parts.size(); p++)
    { // lets find vehicle's center of masses
        w2 = item::types[part_info(p).item]->weight;
        if (w2 < 1) continue;
        xo = xo * wo / (wo + w2) + parts[p].mount_d.x * w2 / (wo + w2);
        yo = yo * wo / (wo + w2) + parts[p].mount_d.y * w2 / (wo + w2);
        wo += w2;
    }
//    g->add_msg("cm x=%.3f y=%.3f m=%d  x1=%d y1=%d x2=%d y2=%d", xo, yo, (int) wo, x1, y1, x2, y2);
    if ((int)xo < x1 || (int)xo > x2 || (int)yo < y1 || (int)yo > y2)
        return false; // center of masses not inside support of wheels (roughly)
    return true;
}

void vehicle::consume_fuel ()
{
    int ftypes[3] = { AT_GAS, AT_BATT, AT_PLASMA };
    for (int ft = 0; ft < 3; ft++)
    {
        float st = strain() * 10;
        int amnt = (int) (basic_consumption (ftypes[ft]) * (1.0 + st * st) / 100);
        if (!amnt)
            continue; // no engines of that type
//         g->add_msg("consume: %d of fuel%d (st:%.2f)", amnt, ft, st);
        bool elec = ftypes[ft] == AT_BATT;
        bool found = false;
        for (int j = 0; j < (elec? 2 : 1); j++)
        {
            for (int p = 0; p < parts.size(); p++)
                // if this is a fuel tank
                // and its type is same we're looking for now
                // and for electric engines:
                //  - if j is 0, then we're looking for plutonium (it's first)
                //  - otherwise we're looking for batteries (second)
                if (part_flag(p, vpf_fuel_tank) &&
                    (part_info(p).fuel_type == (elec? (j? AT_BATT : AT_PLUT) : ftypes[ft])))
                {
                    parts[p].amount -= amnt;
                    if (parts[p].amount < 0)
                        parts[p].amount = 0;
                    found = true;
                    break;
                }
            if (found) break;
        }
    }
}

void vehicle::thrust (int thd)
{
    if (velocity == 0) {
        turn_dir = face.dir();
        last_turn = 0;
        move = face;
        moves = 0;
        last_turn = 0;
        skidding = false;
    }

    if (!thd) return;

	auto g = game::active();

    bool pl_ctrl = player_in_control(g->u);

    if (!valid_wheel_config() && velocity == 0) {
        if (pl_ctrl) messages.add("The %s don't have enough wheels to move!", name.c_str());
        return;
    }

    int sgn = velocity < 0? -1 : 1;
    bool thrusting = sgn == thd;

    if (thrusting) {
        if (total_power () < 1) {
            if (pl_ctrl) {
                if (total_power (false) < 1)
					messages.add("The %s don't have engine!", name.c_str());
                else
					messages.add("The %s's engine emits sneezing sound.", name.c_str());
            }
            cruise_velocity = 0;
            return;
        }

        consume_fuel ();

        int strn = (int) (strain () * strain() * 100);

        for (int p = 0; p < parts.size(); p++)
            if (part_flag(p, vpf_engine) &&
                (fuel_left (part_info(p).fuel_type, true)) && parts[p].hp > 0 &&
                rng (1, 100) < strn)
            {
                int dmg = rng (strn * 2, strn * 4);
                damage_direct (p, dmg, 0);
            }

        // add sound and smoke
		const point origin(global());
        int smk = noise (true, true);
        if (smk > 0) {
            point rd(origin + coord_translate(exhaust_d));
            g->m.add_field(g, rd, fd_smoke, (smk / 50) + 1);
        }
        g->sound(origin, noise(), "");
    }

    if (skidding) return;

    int accel = acceleration();
    int max_vel = max_velocity();
    int brake = 30 * k_mass();
    int brk = abs(velocity) * brake / 100;
    if (brk < accel) brk = accel;
    if (brk < 10 * 100) brk = 10 * 100;
    int vel_inc = (thrusting? accel : brk) * thd;
    if ((velocity > 0 && velocity + vel_inc < 0) ||
        (velocity < 0 && velocity + vel_inc > 0))
        stop ();
    else {
        velocity += vel_inc;
        if (velocity > max_vel)
            velocity = max_vel;
        else if (velocity < -max_vel / 4)
            velocity = -max_vel / 4;
    }
}

void vehicle::cruise_thrust (int amount)
{
    if (!amount)
        return;
    int max_vel = (safe_velocity() * 11 / 10000 + 1) * 1000;
    cruise_velocity += amount;
    cruise_velocity = cruise_velocity / abs(amount) * abs(amount);
    if (cruise_velocity > max_vel)
        cruise_velocity = max_vel;
    else
    if (-cruise_velocity > max_vel / 4)
        cruise_velocity = -max_vel / 4;
}

void vehicle::turn (int deg)
{
    if (deg == 0)
        return;
    if (velocity < 0)
        deg = -deg;
    last_turn = deg;
    turn_dir += deg;
    if (turn_dir < 0)
        turn_dir += 360;
    if (turn_dir >= 360)
        turn_dir -= 360;
}

void vehicle::stop ()
{
    velocity = 0;
    skidding = false;
    move = face;
    last_turn = 0;
    moves = 0;
}

int vehicle::part_collision (int vx, int vy, int part, point dest)
{
	static const int mass_from_msize[mtype::MS_MAX] = { 15, 40, 80, 200, 800 };

	auto g = game::active();

    const bool pl_ctrl = player_in_control(g->u);
	npc* const nPC = g->nPC(dest);
    const bool u_here = dest == g->u.pos && !g->u.in_vehicle;
    monster * const z = g->mon(dest);
    player * const ph = (nPC ? nPC : (u_here? &g->u : 0));
    vehicle * const oveh = g->m.veh_at(dest);
    const bool veh_collision = oveh && oveh->pos != pos;
    bool body_collision = u_here || z || nPC;

    // 0 - nothing, 1 - monster/player/npc, 2 - vehicle,
    // 3 - thin_obstacle, 4 - bashable, 5 - destructible, 6 - other
    int collision_type = 0;
    std::string obs_name(g->m.tername(dest));

    int parm = part_with_feature (part, vpf_armor);
    if (parm < 0) parm = part;
    int dmg_mod = part_info(parm).dmg_mod;
    // let's calculate type of collision & mass of object we hit
    int mass = total_mass() / 8;
    int mass2;
    if (veh_collision)
    { // first, check if we collide with another vehicle (there shouldn't be impassable terrain below)
        collision_type = 2; // vehicle
        mass2 = oveh->total_mass() / 8;
        body_collision = false;
        obs_name = oveh->name.c_str();
    } else if (body_collision)
    { // then, check any monster/NPC/player on the way
        collision_type = 1; // body
		mass2 = mass_from_msize[z ? z->type->size : MS_MEDIUM];
    }
    else // if all above fails, go for terrain which might obstruct moving
    if (g->m.has_flag_ter_only (thin_obstacle, dest.x, dest.y)) {
        collision_type = 3; // some fence
        mass2 = 20;
    } else if (g->m.has_flag_ter_only(bashable, dest.x, dest.y)) {
        collision_type = 4; // bashable (door, window)
        mass2 = 50;    // special case: instead of calculating absorb based on mass of obstacle later, we let
                       // map::bash function deside, how much absorb is
    } else if (0 == g->m.move_cost_ter_only(dest.x, dest.y)) {
		if (g->m.is_destructable_ter_only(dest.x, dest.y)) {
			collision_type = 5; // destructible (wall)
			mass2 = 200;
		} else if (!g->m.has_flag_ter_only(swimmable, dest.x, dest.y)) {
			collision_type = 6; // not destructible
			mass2 = 1000;
		}
	}
    if (!collision_type) return 0;  // hit nothing

    int degree = rng (70, 100);
    int imp = abs(velocity) * mass / k_mvel / 100 ;
    int imp2 = imp * mass2 / (mass + mass2) * degree / 100;
    bool smashed = true;
    std::string snd;
    if (collision_type == 4 || collision_type == 2) // something bashable -- use map::bash to determine outcome
    {
        int absorb = -1;
        g->m.bash(dest, imp * dmg_mod / 100, snd, &absorb);
        if (absorb != -1) imp2 = absorb;
        smashed = imp * dmg_mod / 100 > absorb;
    } else if (collision_type >= 3) // some other terrain
    {
        smashed = imp * rng (80, 120) / 100 > mass2;
        if (smashed)
            switch (collision_type) // destroy obstacle
            {
            case 3:
                g->m.ter(dest) = t_dirt;
                break;
            case 5:
                g->m.ter(dest) = t_rubble;
                snd = "crash!";
                break;
            case 6:
                smashed = false;
                break;
            default:;
            }
        g->sound(dest, smashed? 80 : 50, "");
    }
    if (!body_collision) {
        if (pl_ctrl) {
            if (snd.length() > 0)
				messages.add("Your %s's %s rams into %s with a %s", name.c_str(), part_info(part).name, obs_name.c_str(), snd.c_str());
            else
				messages.add("Your %s's %s rams into %s.", name.c_str(), part_info(part).name, obs_name.c_str());
        }
        else if (snd.length() > 0)
			messages.add("You hear a %s", snd.c_str());
    }
    if (part_flag(part, vpf_sharp) && smashed) imp2 /= 2;
    int imp1 = imp - imp2;
    int vel1 = imp1 * k_mvel * 100 / mass;
    int vel2 = imp2 * k_mvel * 100 / mass2;

    if (collision_type == 1) {
        int dam = imp1 * dmg_mod / 100;
        if (z) {
            int z_armor = part_flag(part, vpf_sharp)? z->type->armor_cut : z->type->armor_bash;
            if (z_armor < 0) z_armor = 0;
            if (z) dam -= z_armor;
        }
        if (dam < 0) dam = 0;

        if (part_flag(part, vpf_sharp))
            parts[part].blood += (20 + dam) * 5;
        else if (dam > rng (10, 30))
            parts[part].blood += (10 + dam / 2) * 5;

        int turns_stunned = rng (0, dam) > 10? rng (1, 2) + (dam > 40? rng (1, 2) : 0) : 0;
        if (part_flag(part, vpf_sharp)) turns_stunned = 0;
        if (turns_stunned > 6) turns_stunned = 6;
        if (turns_stunned > 0 && z) z->add_effect(ME_STUNNED, turns_stunned);

        std::string dname(z ? z->name().c_str() : ph->name);
        if (pl_ctrl)
			messages.add("Your %s's %s rams into %s, inflicting %d damage%s!",
                    name.c_str(), part_info(part).name, dname.c_str(), dam,
                    turns_stunned > 0 && z? " and stunning it" : "");

        int angle = (100 - degree) * 2 * (one_in(2)? 1 : -1);
        if (z) {
            z->hurt(dam);
            if (vel2 > rng (5, 30))
                g->fling_player_or_monster (0, z, move.dir() + angle, vel2 / 100);
            if (z->hp < 1) g->kill_mon (*z, pl_ctrl);
        } else {
            ph->hitall (g, dam, 40);
            if (vel2 > rng (5, 30))
                g->fling_player_or_monster (ph, 0, move.dir() + angle, vel2 / 100);
        }

        if (part_flag(part, vpf_sharp)) {
			auto& f = g->m.field_at(dest);
			if (f.type == fd_blood) {
				if (f.density < 3) f.density++;
			} else g->m.add_field(g, dest, fd_blood, 1);
        } else
            g->sound(dest, 20, "");
    }

    if (!smashed || collision_type == 2) { // vehicles shouldn't intersect
        cruise_on = false;
        stop();
        imp2 = imp;
    } else {
        if (vel1 < 500) stop();
        else velocity = (velocity < 0) ? -vel1 : vel1;

        int turn_roll = rng (0, 100);
        int turn_amount = rng (1, 3) * sqrt (imp2);
        turn_amount /= 15;
        if (turn_amount < 1) turn_amount = 1;
        turn_amount *= 15;
        if (turn_amount > 120) turn_amount = 120;
        bool turn_veh = turn_roll < (abs(velocity) - vel1) / 100;
        if (turn_veh) {
            skidding = true;
            turn (one_in (2)? turn_amount : -turn_amount);
        }

    }
    damage (parm, imp2, 1);
    return imp2;
}

void vehicle::handle_trap (int x, int y, int part)
{
    int pwh = part_with_feature (part, vpf_wheel);
    if (pwh < 0) return;
	auto g = game::active();
    trap_id t = g->m.tr_at(x, y);
    if (t == tr_null) return;
    int noise = 0;
    int chance = 100;
    int expl = 0;
    int shrap = 0;
    bool wreckit = false;
    std::string msg ("The %s's %s runs over %s.");
    std::string snd;
    switch (t)
    {
        case tr_bubblewrap:
            noise = 18;
            snd = "Pop!";
            break;
        case tr_beartrap:
        case tr_beartrap_buried:
            noise = 8;
            snd = "SNAP!";
            wreckit = true;
            g->m.tr_at(x, y) = tr_null;
            g->m.add_item(x, y, item::types[itm_beartrap], 0);
            break;
        case tr_nailboard:
            wreckit = true;
            break;
        case tr_blade:
            noise = 1;
            snd = "Swinnng!";
            wreckit = true;
            break;
        case tr_crossbow:
            chance = 30;
            noise = 1;
            snd = "Clank!";
            wreckit = true;
            g->m.tr_at(x, y) = tr_null;
            g->m.add_item(x, y, item::types[itm_crossbow], 0);
            g->m.add_item(x, y, item::types[itm_string_6], 0);
            if (!one_in(10)) g->m.add_item(x, y, item::types[itm_bolt_steel], 0);
            break;
        case tr_shotgun_2:
        case tr_shotgun_1:
            noise = 60;
            snd = "Bang!";
            chance = 70;
            wreckit = true;
            if (t == tr_shotgun_2) g->m.tr_at(x, y) = tr_shotgun_1;
            else
            {
                g->m.tr_at(x, y) = tr_null;
                g->m.add_item(x, y, item::types[itm_shotgun_sawn], 0);
                g->m.add_item(x, y, item::types[itm_string_6], 0);
            }
            break;
        case tr_landmine:
            expl = 10;
            shrap = 8;
            break;
        case tr_boobytrap:
            expl = 18;
            shrap = 12;
            break;
        case tr_dissector:
            noise = 10;
            snd = "BRZZZAP!";
            wreckit = true;
            break;
        case tr_sinkhole:
        case tr_pit:
        case tr_spike_pit:
        case tr_ledge:
            wreckit = true;
            break;
        case tr_goo:
        case tr_portal:
        case tr_telepad:
        case tr_temple_flood:
        case tr_temple_toggle:
            msg.clear();
        default:;
    }
    if (msg.size() > 0 && g->u_see(x, y))
		messages.add(msg.c_str(), name.c_str(), part_info(part).name, trap::traps[t]->name.c_str());
    if (noise > 0) g->sound (x, y, noise, snd);
    if (wreckit && chance >= rng (1, 100)) damage (part, 500);
    if (expl > 0) g->explosion(x, y, expl, shrap, false);
}

bool vehicle::add_item (int part, item itm)
{
    if (!part_flag(part, vpf_cargo) || parts[part].items.size() >= 26) return false;
	if (part_flag(part, vpf_turret)) {
		const it_ammo* const ammo = dynamic_cast<const it_ammo*>(itm.type);
		if (!ammo || (ammo->type != part_info(part).fuel_type ||
			ammo->type == AT_GAS ||
			ammo->type == AT_PLASMA))
			return false;
	}
    parts[part].items.push_back (itm);
    return true;
}

void vehicle::remove_item (int part, int itemdex)
{
    if (itemdex < 0 || itemdex >= parts[part].items.size()) return;
    parts[part].items.erase (parts[part].items.begin() + itemdex);
}

void vehicle::gain_moves (int mp)
{
    moves += mp;
	auto g = game::active();
    // cruise control TODO: enable for NPC?
    if (player_in_control(g->u)) {
        if (cruise_on)
        if (abs(cruise_velocity - velocity) >= acceleration()/2 ||
            (cruise_velocity != 0 && velocity == 0) ||
            (cruise_velocity == 0 && velocity != 0))
            thrust (cruise_velocity > velocity? 1 : -1);
    }

	const point anchor(global());
    if (g->is_in_sunlight(anchor)) {
        if (const int spw = solar_power()) {
            int fl = spw / 100;
            if (rng (0, 100) <= (spw % 100)) fl++;
            if (fl) refill (AT_BATT, fl);
        }
    }
    // check for smoking parts
    for (int ep = 0; ep < external_parts.size(); ep++) {
        int p = external_parts[ep];
        if (parts[p].blood > 0) parts[p].blood--;
        int p_eng = part_with_feature (p, vpf_engine, false);
        if (p_eng < 0 || parts[p_eng].hp > 0 || parts[p_eng].amount < 1) continue;
        parts[p_eng].amount--;
		const point origin(anchor + parts[p_eng].precalc_d[0]);
        for (int ix = -1; ix <= 1; ix++)
            for (int iy = -1; iy <= 1; iy++)
                if (!rng(0, 2)) g->m.add_field(g, origin.x + ix, origin.y + iy, fd_smoke, rng(2, 4));
    }

    if (turret_mode) // handle turrets
        for (int p = 0; p < parts.size(); p++)
            fire_turret (p);
}

void vehicle::find_external_parts ()
{
    external_parts.clear();
    for (int p = 0; p < parts.size(); p++)
    {
        bool ex = false;
        for (int i = 0; i < external_parts.size(); i++)
            if (parts[external_parts[i]].mount_d == parts[p].mount_d)
            {
                ex = true;
                break;
            }
        if (!ex) external_parts.push_back (p);
    }
}

void vehicle::find_exhaust ()
{
    int en = -1;
    for (int p = 0; p < parts.size(); p++)
        if (part_flag(p, vpf_engine) && part_info(p).fuel_type == AT_GAS) {
            en = p;
            break;
        }
    if (en < 0) {
		exhaust_d = point(0, 0);
        return;
    }
    exhaust_d = parts[en].mount_d;
    for (int p = 0; p < parts.size(); p++)
        if (parts[p].mount_d.y == exhaust_d.y && parts[p].mount_d.x < exhaust_d.x)
            exhaust_d.x = parts[p].mount_d.x;
    exhaust_d.x--;
}

void vehicle::refresh_insides ()
{
    insides_dirty = false;
    for (int ep = 0; ep < external_parts.size(); ep++)
    {
        int p = external_parts[ep];
        if (part_with_feature(p, vpf_roof) < 0 || parts[p].hp <= 0)
        { // if there's no roof (or it's broken) -- it's outside!
/*            debugmsg ("part%d/%d(%s)%d,%d no roof=false", p, external_parts.size(),
                      part_info(p).name, parts[p].mount_dx, parts[p].mount_dy);*/
            parts[p].inside = false;
            continue;
        }

        parts[p].inside = true; // inside if not otherwise
        for (int i = 0; i < 4; i++)
        { // let's check four neighbour parts
            int ndx = i < 2? (i == 0? -1 : 1) : 0;
            int ndy = i < 2? 0 : (i == 2? - 1: 1);
            std::vector<int> parts_n3ar = parts_at_relative (parts[p].mount_d.x + ndx, parts[p].mount_d.y + ndy);
            bool cover = false; // if we aren't covered from sides, the roof at p won't save us
            for (int j = 0; j < parts_n3ar.size(); j++) {
                int pn = parts_n3ar[j];
                if (parts[pn].hp <= 0) continue;   // it's broken = can't cover
                if (part_flag(pn, vpf_roof))
                { // another roof -- cover
                    cover = true;
                    break;
                } else if (part_flag(pn, vpf_obstacle))
                { // found an obstacle, like board or windshield or door
                    if (part_flag(pn, vpf_openable) && parts[pn].open)
                        continue; // door and it's open -- can't cover
                    cover = true;
                    break;
                }
            }
            if (!cover) {
                parts[p].inside = false;
                break;
            }
        }
    }
}

bool vehicle::is_inside (int p)
{
    if (p < 0 || p >= parts.size()) return false;
    if (insides_dirty) refresh_insides ();
    return parts[p].inside;
}

void vehicle::unboard_all ()
{
	const point o(global());
	auto& m = game::active()->m;
    std::vector<int> bp(boarded_parts());
	for (int i = 0; i < bp.size(); i++) {
		const point dest(o + parts[bp[i]].precalc_d[0]);
		m.unboard_vehicle(dest);
	}
}

int vehicle::damage (int p, int dmg, int type, bool aimed)
{
    if (dmg < 1) return dmg;

    std::vector<int> pl = internal_parts (p);
    pl.insert (pl.begin(), p);
    if (!aimed)
    {
        bool found_obs = false;
        for (int i = 0; i < pl.size(); i++)
            if (part_flag (pl[i], vpf_obstacle) &&
                (!part_flag (pl[i], vpf_openable) || !parts[pl[i]].open))
            {
                found_obs = true;
                break;
            }
        if (!found_obs) // not aimed at this tile and no obstacle here -- fly through
            return dmg;
    }
    int parm = part_with_feature (p, vpf_armor);
    int pdm = pl[rng (0, pl.size()-1)];
    int dres;
    if (parm < 0)
        // not covered by armor -- damage part
        dres = damage_direct (pdm, dmg, type);
    else
    {
        // covered by armor -- damage armor first
        dres = damage_direct (parm, dmg, type);
        // half damage for internal part(over parts not covered)
        damage_direct (pdm, part_flag(pdm, vpf_over)? dmg : dmg / 2, type);
    }
    return dres;
}

void vehicle::damage_all (int dmg1, int dmg2, int type)
{
    if (dmg2 < dmg1)
    {
        int t = dmg2;
        dmg2 = dmg1;
        dmg1 = t;
    }
    if (dmg1 < 1)
        return;
    for (int p = 0; p < parts.size(); p++)
        if (!one_in(4))
            damage_direct (p, rng (dmg1, dmg2), type);
}

int vehicle::damage_direct (int p, int dmg, int type)
{
    if (parts[p].hp <= 0) return dmg;
    int tsh = part_info(p).durability / 10;
    if (tsh > 20) tsh = 20;
    int dres = dmg;
    if (dmg >= tsh || type != 1) {
        dres -= parts[p].hp;
        int last_hp = parts[p].hp;
        parts[p].hp -= dmg;
        if (parts[p].hp < 0) parts[p].hp = 0;
        if (!parts[p].hp && last_hp > 0) insides_dirty = true;
		auto g = game::active();
        if (part_flag(p, vpf_fuel_tank)) {
            int ft = part_info(p).fuel_type;
            if (ft == AT_GAS || ft == AT_PLASMA) {
                int pow = parts[p].amount / 40;
                if (parts[p].hp <= 0) leak_fuel (p);
                if (type == 2 ||
                    (one_in (ft == AT_GAS? 2 : 4) && pow > 5 && rng (75, 150) < dmg)) {
                    g->explosion(global() + parts[p].precalc_d[0], pow, 0, ft == AT_GAS);
                    parts[p].hp = 0;
                }
            }
        } else if (parts[p].hp <= 0 && part_flag(p, vpf_unmount_on_damage)) {
			const point dest(global() + parts[p].precalc_d[0]);
            g->m.add_item(dest, item::types[part_info(p).item], messages.turn);
            remove_part(p);
        }
    }
    if (dres < 0) dres = 0;
    return dres;
}

void vehicle::leak_fuel (int p)
{
    if (!part_flag(p, vpf_fuel_tank)) return;
    int ft = part_info(p).fuel_type;
    if (ft == AT_GAS) {
		const point origin(global());
		auto g = game::active();
        for (int i = origin.x - 2; i <= origin.x + 2; i++)
            for (int j = origin.y - 2; j <= origin.y + 2; j++)
                if (g->m.move_cost(i, j) > 0 && one_in(2)) {
                    if (parts[p].amount < 100) {
                        parts[p].amount = 0;
                        return;
                    }
                    g->m.add_item(i, j, item::types[itm_gasoline], 0);
                    parts[p].amount -= 100;
                }
    }
    parts[p].amount = 0;
}

bool vehicle::refill(player& u, const int part, const bool test)
{
    if (!part_flag(part, vpf_fuel_tank)) return false;
	auto& p_info = part_info(part);
	const int ftype = p_info.fuel_type;

    int i_itm = -1;
    item *p_itm = 0;
    int min_charges = -1;
    bool i_cont = false;

    itype_id itid = default_ammo((ammotype)ftype);
    if (u.weapon.is_container() && u.weapon.contents.size() > 0 && u.weapon.contents[0].type->id == itid) {
        i_itm = -2;
        p_itm = &u.weapon.contents[0];
        min_charges = u.weapon.contents[0].charges;
        i_cont = true;
    } else if (u.weapon.type->id == itid) {
        i_itm = -2;
        p_itm = &u.weapon;
        min_charges = u.weapon.charges;
    } else
     for (size_t i = 0; i < u.inv.size(); i++) {
        item *itm = &u.inv[i];
        bool cont = false;
        if (itm->is_container() && itm->contents.size() > 0) {
            cont = true;
            itm = &(itm->contents[0]);
        }
        if (itm->type->id != itid) continue;
        if (i_itm < 0 || min_charges > itm->charges) {
            i_itm = i;
            p_itm = itm;
            i_cont = cont;
            min_charges = itm->charges;
        }
     }

    if (i_itm == -1) return false;
    else if (test) return true;

    int fuel_per_charge = 1;
    switch (ftype)
    {
    case AT_PLUT:
        fuel_per_charge = 1000;
        break;
    case AT_PLASMA:
        fuel_per_charge = 100;
        break;
    default:;
    }

	const int max_fuel = p_info.size;
	auto& v_part = parts[part];
    int dch = (max_fuel - v_part.amount) / fuel_per_charge;
    if (dch < 1) dch = 1;
    const bool rem_itm = min_charges <= dch;
    const int used_charges = rem_itm? min_charges : dch;
    if (max_fuel > (v_part.amount += used_charges * fuel_per_charge)) v_part.amount = max_fuel;

	messages.add("You %s %s's %s%s.", ftype == AT_BATT? "recharge" : "refill", name.c_str(),	// XXX \todo augment to handle NPCs
             ftype == AT_BATT? "battery" : (ftype == AT_PLUT? "reactor" : "fuel tank"),
             v_part.amount == max_fuel? " to its maximum" : "");

    p_itm->charges -= used_charges;
    if (rem_itm)
    {
        if (i_itm == -2)
        {
            if (i_cont)
                u.weapon.contents.erase (u.weapon.contents.begin());
            else
                u.remove_weapon ();
        }
        else
        {
            if (i_cont)
                u.inv[i_itm].contents.erase (u.inv[i_itm].contents.begin());
            else
                u.inv.remove_item (i_itm);
        }
    }
	return true;
}

void vehicle::fire_turret (int p, bool burst)
{
    if (!part_flag (p, vpf_turret)) return;
    it_gun *gun = dynamic_cast<it_gun*> (item::types[part_info(p).item]);
    if (!gun) return;
    int charges = burst? gun->burst : 1;
    if (!charges) charges = 1;
    int amt = part_info (p).fuel_type;
    if (amt == AT_GAS || amt == AT_PLASMA)
    {
        if (amt == AT_GAS) charges = 20; // hacky
        int fleft = fuel_left (amt);
        if (fleft < 1) return;
        const it_ammo* const ammo = dynamic_cast<it_ammo*>(item::types[amt == AT_GAS? itm_gasoline : itm_plasma]);
        if (!ammo) return;
        if (fire_turret_internal (p, *gun, *ammo, charges))
        { // consume fuel
            if (amt == AT_PLASMA) charges *= 10; // hacky, too
            for (int p = 0; p < parts.size(); p++)
            {
                if (part_flag(p, vpf_fuel_tank) &&
                    part_info(p).fuel_type == amt &&
                    parts[p].amount > 0)
                {
                    parts[p].amount -= charges;
                    if (parts[p].amount < 0) parts[p].amount = 0;
                }
            }
        }
    } else {
        if (parts[p].items.size() > 0) {
            const it_ammo* const ammo = dynamic_cast<const it_ammo*> (parts[p].items[0].type);	// XXX expected to crash if not ammo \todo fix
            if (!ammo || ammo->type != amt || parts[p].items[0].charges < 1) return;
            if (charges > parts[p].items[0].charges) charges = parts[p].items[0].charges;
            if (fire_turret_internal (p, *gun, *ammo, charges))
            { // consume ammo
                if (charges >= parts[p].items[0].charges)
                    parts[p].items.erase (parts[p].items.begin());
                else
                    parts[p].items[0].charges -= charges;
            }
        }
    }
}

bool vehicle::fire_turret_internal (int p, it_gun &gun, const it_ammo &ammo, int charges)
{
	const point origin(global() + parts[p].precalc_d[0]);
    // code copied form mattack::smg, mattack::flamethrower
    int t, fire_t;
    monster *target = 0;
    int range = ammo.type == AT_GAS? 5 : 12;
    int closest = range + 1;
	auto g = game::active();
	for(auto& _mon : g->z) {
        int dist = rl_dist(origin, _mon.pos);
        if (_mon.friendly == 0 && dist < closest &&
            g->m.sees(origin, _mon.pos, range, t)) {
            target = &_mon;
            closest = dist;
            fire_t = t;
        }
    }
    if (!target) return false;

    std::vector<point> traj = line_to(origin, target->pos, fire_t);
    for (int i = 0; i < traj.size(); i++)
        if (traj[i] == g->u.pos) return false; // won't shoot at player
    if (g->u_see(origin)) messages.add("The %s fires its %s!", name.c_str(), part_info(p).name);
    player tmp;
    tmp.name = std::string("The ") + part_info(p).name;
    tmp.sklevel[gun.skill_used] = 1;
    tmp.sklevel[sk_gun] = 0;
    tmp.recoil = abs(velocity) / 100 / 4;
    tmp.screenpos_set(origin);
    tmp.str_cur = 16;
    tmp.dex_cur =  6;
    tmp.per_cur =  8;
    tmp.weapon = item(&gun, 0);
    it_ammo curam = ammo;
    tmp.weapon.curammo = &curam;
    tmp.weapon.charges = charges;
    g->fire(tmp, target->pos, traj, true);
    if (ammo.type == AT_GAS) {
		for(const auto& pt : traj) g->m.add_field(g, pt, fd_fire, 1);
    }
	return true;
}

