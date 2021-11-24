#ifndef _ITYPE_H_
#define _ITYPE_H_

#include "color.h"
#include "pldata_enum.h"
#include "skill.h"
#include "bionics_enum.h"
#include "artifact.h"
#include "c_bitmap.h"
#include "material_enum.h"
#include "enum_json.h"
#include <vector>
#include <initializer_list>
#ifndef SOCRATES_DAIMON
#include <optional>
#include <any>
#endif

// ideally this would be in a vector header extension
template<class T>
void EraseAt(std::vector<T>& x, size_t i) {
    x.erase(x.begin() + i);
    if (x.empty()) std::vector<T>().swap(x);    // handles problem with range-based for loops in MSVC++
}

#ifndef SOCRATES_DAIMON
class game;
class item;
class player;
class npc;
class pc;
#endif

namespace cataclysm {

	class JSON;

}

enum itype_id {
itm_null = 0,
itm_corpse,
// Special pseudoitems
itm_fire, itm_toolset,
// Drinks
itm_water, itm_sewage, itm_salt_water, itm_oj, itm_apple_cider,
 itm_energy_drink, itm_cola, itm_rootbeer, itm_milk, itm_V8, itm_broth,
 itm_soup, itm_whiskey, itm_vodka, itm_rum, itm_tequila, itm_beer, itm_bleach,
 itm_ammonia, itm_mutagen, itm_purifier, itm_tea, itm_coffee, itm_blood,
// Monster Meats
itm_meat, itm_veggy, itm_meat_tainted, itm_veggy_tainted, itm_meat_cooked,
 itm_veggy_cooked,
// Food
itm_apple, itm_orange, itm_lemon, itm_chips, itm_pretzels, itm_chocolate,
 itm_jerky, itm_sandwich_t, itm_candy, itm_mushroom, itm_mushroom_poison,
 itm_mushroom_magic, itm_blueberries, itm_strawberries, itm_tomato,
 itm_broccoli, itm_zucchini, itm_frozen_dinner, itm_cooked_dinner,
 itm_spaghetti_raw, itm_spaghetti_cooked, itm_macaroni_raw, itm_macaroni_cooked,
 itm_ravioli, itm_sauce_red, itm_sauce_pesto, itm_can_beans, itm_can_corn,
 itm_can_spam, itm_can_pineapple, itm_can_coconut, itm_can_sardine,
 itm_can_tuna, itm_can_catfood, itm_honeycomb, itm_royal_jelly, itm_fetus,
 itm_arm, itm_leg, itm_ant_egg, itm_marloss_berry, itm_flour, itm_sugar,
 itm_salt, itm_potato_raw, itm_potato_baked, itm_bread, itm_pie, itm_pizza,
 itm_mre_beef, itm_mre_veggy, itm_tea_raw, itm_coffee_raw,
// Medication
itm_bandages, itm_1st_aid, itm_vitamins, itm_aspirin, itm_caffeine,
 itm_pills_sleep, itm_iodine, itm_dayquil, itm_nyquil, itm_inhaler, itm_codeine,
 itm_oxycodone, itm_tramadol, itm_xanax, itm_adderall, itm_thorazine,
 itm_prozac, itm_cig, itm_weed, itm_coke, itm_meth, itm_heroin, itm_cigar,
 itm_antibiotics,
// Do-nothing / Melee weapons
itm_wrapper, itm_syringe, itm_rag, itm_fur, itm_leather, itm_superglue,
 itm_id_science, itm_id_military, itm_electrohack, itm_string_6, itm_string_36,
 itm_rope_6, itm_rope_30, itm_chain, itm_processor, itm_RAM, itm_power_supply,
 itm_amplifier, itm_transponder, itm_receiver, itm_antenna, itm_steel_chunk,
 itm_steel_lump, itm_hose, itm_glass_sheet, itm_manhole_cover, itm_rock,
 itm_stick, itm_broom, itm_mop, itm_screwdriver, itm_wrench, itm_saw,
 itm_hacksaw, itm_hammer_sledge, itm_hatchet, itm_ax, itm_nailboard, itm_xacto,
 itm_scalpel, itm_pot, itm_pan, itm_knife_butter, itm_knife_steak,
 itm_knife_butcher, itm_knife_combat, itm_2x4, itm_muffler, itm_pipe, itm_bat,
 itm_machete, itm_katana, itm_spear_wood, itm_spear_knife, itm_baton,
 itm_bee_sting, itm_wasp_sting, itm_chitin_piece, itm_biollante_bud,
 itm_canister_empty, itm_gold, itm_coal, itm_petrified_eye, itm_spiral_stone,
 itm_rapier, itm_cane, itm_binoculars, itm_usb_drive, itm_pike, itm_broadsword,
 itm_mace, itm_morningstar, itm_pool_cue, itm_pool_ball, itm_candlestick,
// Vehicle parts
itm_frame, itm_wheel, itm_big_wheel, itm_seat, itm_vehicle_controls,
 itm_combustion_small, itm_combustion, itm_combustion_large,
 itm_motor, itm_motor_large, itm_plasma_engine,
 itm_metal_tank, itm_storage_battery, itm_minireactor, itm_solar_panel,
 itm_steel_plate, itm_alloy_plate, itm_spiked_plate, itm_hard_plate,
// Footwear
itm_sneakers, itm_boots, itm_boots_steel, itm_boots_winter, itm_mocassins,
 itm_flip_flops, itm_dress_shoes, itm_heels, 
// Legwear
itm_jeans, itm_pants, itm_pants_leather, itm_pants_cargo, itm_pants_army,
 itm_skirt,
// Full-body clothing
itm_jumpsuit, itm_dress, itm_armor_chitin, itm_suit, itm_hazmat_suit,
 itm_armor_plate,
// Torso clothing
itm_tshirt, itm_polo_shirt, itm_dress_shirt, itm_tank_top, itm_sweatshirt,
 itm_sweater, itm_hoodie, itm_jacket_light, itm_jacket_jean, itm_blazer,
 itm_jacket_leather, itm_kevlar, itm_coat_rain, itm_poncho, itm_trenchcoat,
 itm_coat_winter, itm_coat_fur, itm_peacoat, itm_vest, itm_coat_lab,
// Gloves
itm_gloves_light, itm_mittens, itm_gloves_wool, itm_gloves_winter,
 itm_gloves_leather, itm_gloves_fingerless, itm_gloves_rubber,
 itm_gloves_medical, itm_fire_gauntlets,
// Masks
itm_mask_dust, itm_bandana, itm_scarf, itm_mask_filter, itm_mask_gas,
// Eyewear
itm_glasses_eye, itm_glasses_reading, itm_glasses_safety, itm_goggles_swim,
 itm_goggles_ski, itm_goggles_welding, itm_goggles_nv, itm_glasses_monocle,
// Headwear
itm_hat_ball, itm_hat_boonie, itm_hat_cotton, itm_hat_knit, itm_hat_hunting,
 itm_hat_fur, itm_hat_hard, itm_helmet_bike, itm_helmet_skid, itm_helmet_ball,
 itm_helmet_army, itm_helmet_riot, itm_helmet_motor, itm_helmet_chitin,
 itm_helmet_plate, itm_tophat,
// High-storage
itm_backpack, itm_purse, itm_mbag, itm_fanny, itm_holster, itm_bootstrap,
// Decorative
itm_ring, itm_necklace,
// Ammunition
itm_battery, itm_plut_cell, itm_nail, itm_bb, itm_arrow_wood, itm_arrow_cf,
 itm_bolt_wood, itm_bolt_steel, itm_shot_bird, itm_shot_00, itm_shot_slug,
 itm_shot_he, itm_22_lr, itm_22_cb, itm_22_ratshot, itm_9mm, itm_9mmP,
 itm_9mmP2, itm_38_special, itm_38_super, itm_10mm, itm_40sw, itm_44magnum,
 itm_45_acp, itm_45_jhp, itm_45_super, itm_57mm, itm_46mm, itm_762_m43,
 itm_762_m87, itm_223, itm_556, itm_556_incendiary, itm_270, itm_3006,
 itm_3006_incendiary, itm_308, itm_762_51, itm_762_51_incendiary,
 itm_laser_pack, itm_40mm_concussive, itm_40mm_frag, itm_40mm_incendiary,
 itm_40mm_teargas, itm_40mm_smoke, itm_40mm_flashbang, itm_12mm, itm_plasma,
 itm_charge_shot,
 itm_gasoline,
// Guns
itm_nailgun, itm_bbgun, itm_crossbow, itm_compbow, itm_longbow, itm_rifle_22,
 itm_rifle_9mm, itm_smg_9mm, itm_smg_45, itm_sig_mosquito, itm_sw_22,
 itm_glock_19, itm_usp_9mm, itm_sw_619, itm_taurus_38, itm_sig_40, itm_sw_610,
 itm_ruger_redhawk, itm_deagle_44, itm_usp_45, itm_m1911, itm_fn57, itm_hk_ucp,
 itm_shotgun_sawn, itm_shotgun_s, itm_shotgun_d,  itm_remington_870,
 itm_mossberg_500, itm_saiga_12, itm_american_180, itm_uzi, itm_tec9,
 itm_calico, itm_hk_mp5, itm_mac_10, itm_hk_ump45, itm_TDI, itm_fn_p90,
 itm_hk_mp7, itm_marlin_9a, itm_ruger_1022, itm_browning_blr,
 itm_remington_700, itm_sks, itm_ruger_mini, itm_savage_111f, itm_hk_g3,
 itm_hk_g36, itm_ak47, itm_fn_fal, itm_acr, itm_ar15, itm_m4a1, itm_scar_l,
 itm_scar_h, itm_steyr_aug, itm_m249, itm_v29, itm_ftk93, itm_nx17,
 itm_flamethrower_simple, itm_flamethrower, itm_launcher_simple, itm_m79,
 itm_m320, itm_mgl, itm_coilgun, itm_hk_g80, itm_plasma_rifle,
// Gun modifications
itm_silencer, itm_grip, itm_barrel_big, itm_barrel_small, itm_barrel_rifled,
 itm_clip, itm_clip2, itm_stablizer, itm_blowback, itm_autofire, itm_retool_45,
 itm_retool_9mm, itm_retool_22, itm_retool_57, itm_retool_46, itm_retool_308,
 itm_retool_223, itm_conversion_battle, itm_conversion_sniper, itm_m203,
 itm_bayonet,
// Books
itm_mag_porn, itm_mag_tv, itm_mag_news, itm_mag_cars, itm_mag_cooking,
 itm_mag_guns, itm_novel_romance, itm_novel_spy, itm_novel_scifi,
 itm_novel_drama, itm_manual_brawl, itm_manual_knives, itm_manual_mechanics,
 itm_manual_speech, itm_manual_business, itm_manual_first_aid,
 itm_manual_computers, itm_cookbook, itm_manual_electronics,
 itm_manual_tailor, itm_manual_traps, itm_manual_carpentry,
 itm_textbook_computers, itm_textbook_electronics, itm_textbook_business,
 itm_textbook_chemistry, itm_textbook_carpentry, itm_SICP, itm_textbook_robots,
// Containers
itm_bag_plastic, itm_bottle_plastic, itm_bottle_glass,
 itm_can_drink, itm_can_food, itm_box_small,
// Tools
itm_lighter, itm_sewing_kit, itm_scissors, itm_hammer, itm_extinguisher,
 itm_flashlight, itm_flashlight_on, itm_hotplate, itm_soldering_iron,
 itm_water_purifier, itm_two_way_radio, itm_radio, itm_radio_on, itm_crowbar,
 itm_hoe, itm_shovel, itm_chainsaw_off, itm_chainsaw_on, itm_jackhammer,
 itm_bubblewrap, itm_beartrap, itm_board_trap, itm_tripwire, itm_crossbow_trap,
 itm_shotgun_trap, itm_blade_trap, itm_landmine, itm_geiger_off, itm_geiger_on,
 itm_teleporter, itm_canister_goo, itm_pipebomb, itm_pipebomb_act, itm_grenade,
 itm_grenade_act, itm_flashbang, itm_flashbang_act, itm_EMPbomb,
 itm_EMPbomb_act, itm_gasbomb, itm_gasbomb_act, itm_smokebomb,
 itm_smokebomb_act, itm_molotov, itm_molotov_lit, itm_acidbomb,
 itm_acidbomb_act, itm_dynamite, itm_dynamite_act, itm_mininuke,
 itm_mininuke_act, itm_pheromone, itm_portal, itm_bot_manhack, itm_bot_turret,
 itm_UPS_off, itm_UPS_on, itm_tazer, itm_mp3, itm_mp3_on, itm_vortex_stone,
 itm_dogfood, itm_boobytrap, itm_c4, itm_c4armed, itm_dog_whistle,
 itm_vacutainer, itm_welder,
// Bionics containers
itm_bionics_battery,       itm_bionics_power,   itm_bionics_tools,
 itm_bionics_neuro,        itm_bionics_sensory, itm_bionics_aquatic,
 itm_bionics_combat_aug,   itm_bionics_hazmat,  itm_bionics_nutritional,
 itm_bionics_desert,       itm_bionics_melee,   itm_bionics_armor,
 itm_bionics_espionage,    itm_bionics_defense, itm_bionics_medical,
 itm_bionics_construction, itm_bionics_super,   itm_bionics_ranged,
// Software
itm_software_useless, itm_software_hacking, itm_software_medical,
 itm_software_math, itm_software_blood_data,
// MacGuffins!
itm_note,
// Static (non-random) artifacts should go here.
num_items,
// These shouldn't be counted among "normal" items; thus, they are outside the
// bounds of num_items
itm_bio_claws, itm_bio_fusion, itm_bio_blaster,
// Unarmed Combat Styles
itm_style_karate, itm_style_aikido, itm_style_judo, itm_style_tai_chi,
 itm_style_capoeira, itm_style_krav_maga, itm_style_muay_thai,
 itm_style_ninjutsu, itm_style_taekwando, itm_style_tiger, itm_style_crane,
 itm_style_leopard, itm_style_snake, itm_style_dragon, itm_style_centipede,
 itm_style_venom_snake, itm_style_scorpion, itm_style_lizard, itm_style_toad,
 itm_style_zui_quan,
num_all_items
};

// styles from the 1978 movie "The Five Venoms":
// itm_style_centipede, itm_style_venom_snake, itm_style_scorpion, itm_style_lizard, itm_style_toad
// others exist in our reality

DECLARE_JSON_ENUM_SUPPORT(itype_id)

struct item_drop_spec final
{
	itype_id what;
	int qty;
//	int mode; // will need this to specify formula type; for now implicitly requests one_in processing.  Credible scoped enumeration.
	int modifier;

	item_drop_spec(itype_id what, int qty = 1, int modifier = 0) noexcept : what(what), qty(qty), modifier(modifier) {}
	item_drop_spec(const item_drop_spec&) = default;
	~item_drop_spec() = default;
	item_drop_spec& operator=(const item_drop_spec&) = default;

	std::string to_s() const;
};

// IMPORTANT: If adding a new AT_*** ammotype, add it to the ammo_name function
//  at the end of itypedef.cpp
enum ammotype {
AT_NULL,
AT_BATT, AT_PLUT,
AT_NAIL, AT_BB, AT_BOLT, AT_ARROW,
AT_SHOT,
AT_22, AT_9MM, AT_38, AT_40, AT_44, AT_45,
AT_57, AT_46,
AT_762, AT_223, AT_3006, AT_308,
AT_40MM,
AT_GAS,
AT_FUSION,
AT_12MM,
AT_PLASMA,
NUM_AMMO_TYPES
};

DECLARE_JSON_ENUM_SUPPORT(ammotype)

enum software_type {
SW_NULL,
SW_USELESS,
SW_HACKING,
SW_MEDICAL,
SW_SCIENCE,
SW_DATA,
NUM_SOFTWARE_TYPES
};

enum item_flag {
IF_LIGHT_4,	// Provides 4 tiles of light
IF_LIGHT_8,	// Provides 8 tiles of light

IF_SPEAR,	// Cutting damage is actually a piercing attack
IF_STAB,	// This weapon *can* pierce, but also has normal cutting
IF_WRAP,	// Can wrap around your target, costing you and them movement
IF_MESSY,	// Splatters blood, etc.
IF_RELOAD_ONE,	// Reload cartridge by cartridge (e.g. most shotguns)
IF_STR_RELOAD,  // Reloading time is reduced by Strength * 20
IF_STR8_DRAW,   // Requires strength 8 to draw
IF_STR10_DRAW,  // Requires strength 10 to draw
IF_USE_UPS,	// Draws power from a UPS
IF_RELOAD_AND_SHOOT, // Reloading and shooting is one action
IF_FIRE_100,	// Fires 100 rounds at once! (e.g. flamethrower)
IF_GRENADE,	// NPCs treat this as a grenade
IF_CHARGE,	// For guns; charges up slowly

IF_UNARMED_WEAPON, // Counts as an unarmed weapon
IF_NO_UNWIELD, // Impossible to unwield, e.g. bionic claws

IF_AMMO_FLAME,		// Sets fire to terrain and monsters
IF_AMMO_INCENDIARY,	// Sparks explosive terrain
IF_AMMO_EXPLOSIVE,	// Small explosion
IF_AMMO_FRAG,		// Frag explosion
IF_AMMO_NAPALM,		// Firey explosion
IF_AMMO_EXPLOSIVE_BIG,	// Big explosion!
IF_AMMO_TEARGAS,	// Teargas burst
IF_AMMO_SMOKE,  	// Smoke burst
IF_AMMO_TRAIL,		// Leaves a trail of smoke
IF_AMMO_FLASHBANG,	// Disorients and blinds
IF_AMMO_STREAM,		// Doesn't stop once it hits a monster
NUM_ITEM_FLAGS
};

DECLARE_JSON_ENUM_BITFLAG_SUPPORT(item_flag)

enum technique_id {
TEC_NULL,				// \todo redefine TEC_NULL as -1 (implementation-defined operator << but we should not be doing that anyway)
// Offensive Techniques
TEC_SWEEP,	// Crits may make your enemy fall & miss a turn
TEC_PRECISE,	// Crits are painful and stun
TEC_BRUTAL,	// Crits knock the target back
TEC_GRAB,	// Hit may allow a second unarmed attack attempt
TEC_WIDE,	// Attacks adjacent oppoents
TEC_RAPID,	// Hits faster
TEC_FEINT,	// Misses take less time
TEC_THROW,	// Attacks may throw your opponent
TEC_DISARM,	// Remove an NPC's weapon
// Defensive Techniques
TEC_BLOCK,	// Block attacks, reducing them to 25% damage
TEC_BLOCK_LEGS, // Block attacks, but with your legs
TEC_WBLOCK_1,	// Weapon block, poor chance -- e.g. pole
TEC_WBLOCK_2,	// Weapon block, moderate chance -- weapon made for blocking
TEC_WBLOCK_3,	// Weapon block, good chance -- shield
TEC_COUNTER,	// Counter-attack on a block or dodge
TEC_BREAK,	// Break from a grab
TEC_DEF_THROW,	// Throw an enemy that attacks you
TEC_DEF_DISARM, // Disarm an enemy

NUM_TECHNIQUES
};

DECLARE_JSON_ENUM_BITFLAG_SUPPORT(technique_id)

struct style_move
{
 std::string name;
 technique_id tech;
 int level;
};

// Returns the name of a category of ammo (e.g. "shot")
std::string ammo_name(ammotype t);
// Returns the default ammo for a category of ammo (e.g. "itm_00_shot")
itype_id default_ammo(ammotype guntype);

// forward-declares
struct it_ammo;
struct it_armor;
struct it_artifact_armor;
struct it_artifact_tool;
struct it_bionic;
struct it_book;
struct it_comest;
struct it_container;
struct it_gun;
struct it_gunmod;
struct it_macguffin;
struct it_software;
struct it_style;
struct it_tool;

struct itype
{
 int id;		// ID # that matches its place in master itype list
 				// Used for save files; aligns to itype_id above.
			    // plausibly should be int rather than itype_id due to random artifacts
 unsigned char rarity;	// How often it's found
 unsigned int  price;	// Its value
 
 std::string name;	// Proper name
 std::string description;// Flavor text
 
 char sym;		// Symbol on the map
 nc_color color;	// Color on the map (color.h)
 
 material m1;		// Main material
 material m2;		// Secondary material -- MNULL if made of just 1 thing
 
 unsigned int volume;	// Space taken up by this item
 unsigned int weight;	// Weight in quarter-pounds; is 64 lbs max ok?
 			// Also assumes positive weight.  No helium, guys!
 
 signed char melee_dam;	// Bonus for melee damage; may be a penalty
 signed char melee_cut;	// Cutting damage in melee
 signed char m_to_hit;	// To-hit bonus for melee combat; -5 to 5 is reasonable

 typename cataclysm::bitmap<NUM_ITEM_FLAGS>::type item_flags;
 typename cataclysm::bitmap<NUM_TECHNIQUES>::type techniques;
 
 virtual const it_comest* is_food() const    { return nullptr; }
 virtual const it_ammo* is_ammo() const    { return nullptr; }
 virtual const it_gun* is_gun() const     { return nullptr; }
 virtual const it_gunmod* is_gunmod() const  { return nullptr; }
 virtual const it_bionic* is_bionic() const  { return nullptr; }
 virtual const it_armor* is_armor() const   { return nullptr; }
 virtual const it_book* is_book() const    { return nullptr; }
 virtual const it_tool* is_tool() const    { return nullptr; }
 virtual const it_container* is_container() const { return nullptr; }
 virtual const it_software* is_software() const { return nullptr; }
 virtual const it_macguffin* is_macguffin() const { return nullptr; }
 virtual const it_style* is_style() const   { return nullptr; }
 virtual bool is_artifact() const { return false; }
 virtual const it_artifact_armor* is_artifact_armor() const { return nullptr; }
 virtual const it_artifact_tool* is_artifact_tool() const { return nullptr; }
 virtual bool count_by_charges() const { return false; }
 virtual std::string save_data() { return std::string(); }

 itype();
 itype(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
	 unsigned pitem_flags, unsigned ptechniques = 0);

 virtual ~itype() = default;

 virtual int melee_value(const int skills[num_skill_types]) const;
 virtual bool expires() const { return false; } // does it expire? C:Whales comestibles, but things like Zylon also expire
 virtual bool is_expired(int age) const { return false; }

 static std::string force_sign(int src);
 virtual void info(std::ostream& dest) const {}
protected:	// this is not a final type so these aren't public
 itype(const cataclysm::JSON& src);
 virtual void toJSON(cataclysm::JSON& dest) const;
};

// Includes food drink and drugs
struct it_comest : public itype
{
 signed char quench;	// Many things make you thirstier!
 unsigned char nutr;	// Nutrition imparted
 unsigned char spoils;	// How long it takes to spoil (hours / 600 turns)
 unsigned char addict;	// Addictiveness potential
 unsigned char charges;	// Defaults # of charges (drugs, loaf of bread? etc)
 signed char stim;
 signed char healthy;

 signed char fun;	// How fun its use is

 itype_id container;	// The container it comes in
 itype_id tool;		// Tool needed to consume (e.g. lighter for cigarettes)

#ifndef SOCRATES_DAIMON
private:
 void (*use)(player*, item*, bool);// Special effects of use
 void (*use_npc)(npc&, item&);	// waterfall/SSADM software lifecycle for these three
 void (*use_pc)(pc&, item&);
 void (*use_player)(player&, item&);
 void (*use_npc_none)(npc&);
 void (*use_pc_none)(pc&);
 void (*use_player_none)(player&);
 void (*use_npc_type)(npc&, const it_comest& food);
 void (*use_pc_type)(pc&, const it_comest& food);
 void (*use_player_type)(player&, const it_comest& food);

public:
#endif
 add_type add;				// Effects of addiction

 it_comest(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1,
	 unsigned short pvolume, unsigned short pweight,

	 signed char pquench, unsigned char pnutr, unsigned char pspoils,
	 signed char pstim, signed char phealthy, unsigned char paddict,
	 unsigned char pcharges, signed char pfun, itype_id pcontainer,
	 itype_id ptool,
	 add_type padd
#ifndef SOCRATES_DAIMON
	 , decltype(use) puse = nullptr
#endif
	 );

#ifndef SOCRATES_DAIMON
 it_comest(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1,
	 unsigned short pvolume, unsigned short pweight,

	 signed char pquench, unsigned char pnutr, unsigned char pspoils,
	 signed char pstim, signed char phealthy, unsigned char paddict,
	 unsigned char pcharges, signed char pfun, itype_id pcontainer,
	 itype_id ptool,
	 add_type padd,
	 decltype(use_pc) puse
 );

 it_comest(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1,
	 unsigned short pvolume, unsigned short pweight,

	 signed char pquench, unsigned char pnutr, unsigned char pspoils,
	 signed char pstim, signed char phealthy, unsigned char paddict,
	 unsigned char pcharges, signed char pfun, itype_id pcontainer,
	 itype_id ptool,
	 add_type padd,
	 decltype(use_player) puse
 );
#endif

 const it_comest* is_food() const override { return this; }
 bool count_by_charges() const override { return charges > 1; }
 bool expires() const override { return 0 != spoils; }
 bool is_expired(int age) const override { return 0 != spoils && age > spoils * 600 /* HOURS(1) */; }

#ifndef SOCRATES_DAIMON
 void consumed_by(item& it, npc& u) const;
 void consumed_by(item& it, pc& u) const;
#endif

 void info(std::ostream& dest) const override;
};

struct it_ammo : public itype
{
 ammotype type;		// Enum of varieties (e.g. 9mm, shot, etc)
 unsigned char damage;	// Average damage done
 unsigned char pierce;	// Armor piercing; static reduction in armor
 unsigned char range;	// Maximum range
 signed char accuracy;	// Accuracy (low is good)
 unsigned char recoil;	// Recoil; modified by strength
 unsigned char count;	// Default charges

 it_ammo(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, unsigned pitem_flags,

	 ammotype ptype, unsigned char pdamage, unsigned char ppierce,
	 signed char paccuracy, unsigned char precoil, unsigned char prange,
	 unsigned char pcount);

 const it_ammo* is_ammo() const override { return this; }
 bool count_by_charges() const override { return id != itm_gasoline; }

 void info(std::ostream& dest) const override;
};

struct it_gun final : public itype
{
 ammotype ammo;
 skill skill_used;
 signed char dmg_bonus;
 signed char accuracy;
 signed char recoil;
 signed char durability;
 unsigned char burst;
 int clip;
 int reload_time;

 it_gun(int pid, unsigned char prarity, unsigned int pprice,
        std::string pname, std::string pdes,
        char psym, nc_color pcolor, material pm1, material pm2,
        unsigned short pvolume, unsigned short pweight,
        signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
        unsigned pitem_flags,

	skill pskill_used, ammotype pammo, signed char pdmg_bonus,
	signed char paccuracy, signed char precoil, unsigned char pdurability,
        unsigned char pburst, int pclip, int preload_time);

 const it_gun* is_gun() const override { return this; }
};

struct it_gunmod final : public itype
{
 signed char accuracy, damage, loudness, clip, recoil, burst;
 ammotype newtype;
 unsigned acceptible_ammo_types : NUM_AMMO_TYPES;
 bool used_on_pistol;
 bool used_on_shotgun;
 bool used_on_smg;
 bool used_on_rifle;

 it_gunmod(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut,
	 signed char pm_to_hit, unsigned pitem_flags,

	 signed char paccuracy, signed char pdamage, signed char ploudness,
	 signed char pclip, signed char precoil, signed char pburst,
	 ammotype pnewtype, long a_a_t, bool pistol,
	 bool shotgun, bool smg, bool rifle);

 const it_gunmod* is_gunmod() const override { return this; }

 void info(std::ostream& dest) const override;
};

struct it_armor : public itype
{
 unsigned char covers; // Bitfield of enum body_part
 signed char encumber;
 unsigned char dmg_resist;
 unsigned char cut_resist;
 unsigned char env_resist; // Resistance to environmental effects
 signed char warmth;
 unsigned char storage;

 it_armor();
 it_armor(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
	 unsigned pitem_flags,

	 unsigned char pcovers, signed char pencumber,
	 unsigned char pdmg_resist, unsigned char pcut_resist,
	 unsigned char penv_resist, signed char pwarmth,
	 unsigned char pstorage);

 const it_armor* is_armor() const override final { return this; }

 void info(std::ostream& dest) const override;
protected:	// this is not a final type so these aren't public
 it_armor(const cataclysm::JSON& src);
 void toJSON(cataclysm::JSON& dest) const override;
};

struct it_book final : public itype
{
 skill type;		// Which skill it upgrades
 unsigned char level;	// The value it takes the skill to
 unsigned char req;	// The skill level required to understand it
 signed char fun;	// How fun reading this is
 unsigned char intel;	// Intelligence required to read, at all
 unsigned char time;	// How long, in 10-turns (aka minutes), it takes to read
			// "To read" means getting 1 skill point, not all of em

 it_book(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
	 unsigned pitem_flags,

	 skill ptype, unsigned char plevel, unsigned char preq,
	 signed char pfun, unsigned char pintel, unsigned char ptime);

 const it_book* is_book() const override { return this; }

 void info(std::ostream& dest) const override;
};
 
enum container_flags {
 con_rigid,
 con_wtight,
 con_seals,
 num_con_flags
};

struct it_container : public itype
{
 unsigned char contains;	// Internal volume
 typename cataclysm::bitmap<num_con_flags>::type flags;

 it_container(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut,
	 signed char pm_to_hit, unsigned pitem_flags,

	 unsigned char pcontains, decltype(flags) pflags);

 const it_container* is_container() const override { return this; }
};

struct it_tool : public itype
{
 ammotype ammo;
 unsigned int max_charges;
 unsigned int def_charges;
 unsigned char charges_per_use;
 unsigned char turns_per_charge;
 itype_id revert_to;
#ifndef SOCRATES_DAIMON
private:
 void (*use_npc)(npc&, item&);	// waterfall/SSADM sofware lifecycle for these nine
 void (*use_pc)(pc&, item&);
 void (*use_player)(player&, item&);
 void (*use_item)(item&);
 void (*off_npc)(npc&, item&);
 void (*off_pc)(pc&, item&);
 void (*off_player)(player&, item&);
 void (*off_item)(item&);
 std::optional<std::any> (*can_use_npc)(const npc&);

public:
#endif

 // The constructors' buildout policy is YAGNI.  We have a combinatoric explosion here;
 // policy is that we want constructors to do their job (i.e., a fluent interface is a non-starter).
 it_tool();
#ifndef SOCRATES_DAIMON
protected:
	it_tool(decltype(use_pc) puse); // assistant for it_artifact_tool default constructor

public:
	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_pc) puse
	);

	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_player) puse
	);

	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_item) puse
	);

	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_pc) puse, decltype(off_pc) poff
	);

	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_player) puse, decltype(off_player) poff
	);

	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_item) puse, decltype(off_item) poff
	);

	it_tool(int pid, unsigned char prarity, unsigned int pprice,
		std::string pname, std::string pdes,
		char psym, nc_color pcolor, material pm1, material pm2,
		unsigned short pvolume, unsigned short pweight,
		signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
		unsigned pitem_flags,

		unsigned int pmax_charges, unsigned int pdef_charges,
		unsigned char pcharges_per_use, unsigned char pturns_per_charge,
		ammotype pammo, itype_id prevert_to,
		decltype(use_pc) use_pc, decltype(use_npc) use_npc, decltype(can_use_npc) can_use_npc
	);

#endif
 it_tool(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
	 unsigned pitem_flags,

	 unsigned int pmax_charges, unsigned int pdef_charges,
	 unsigned char pcharges_per_use, unsigned char pturns_per_charge,
	 ammotype pammo, itype_id prevert_to
 );

 const it_tool* is_tool() const override final { return this; }

#ifndef SOCRATES_DAIMON
 void used_by(item& it, npc& u) const;
 void used_by(item& it, pc& u) const;
 void turned_off_by(item& it, npc& u) const;
 void turned_off_by(item& it, pc& u) const;
 std::optional<std::any> is_relevant(const item& it, const npc& _npc) const;
 std::optional<std::string> cannot_use(const item& it, const player& u) const;
#endif

 void info(std::ostream& dest) const override;
protected:	// this is not a final type so these aren't public
	it_tool(const cataclysm::JSON& src);
#ifndef SOCRATES_DAIMON
	it_tool(const cataclysm::JSON& src, decltype(use_pc) puse);
#endif
	void toJSON(cataclysm::JSON& dest) const override;
};
        
struct it_bionic final : public itype
{
 std::vector<bionic_id> options;
 int difficulty;

 it_bionic(int pid, std::string pname, unsigned char prarity, unsigned int pprice,
	 char psym, nc_color pcolor, int pdifficulty,
	 std::string pdes, const std::initializer_list<bionic_id>& _opts);

 const it_bionic* is_bionic() const override { return this; }
};

struct it_macguffin final : public itype
{
 bool readable; // If true, activated with 'R'
#ifndef SOCRATES_DAIMON
private:
 void (*use_pc)(pc&, item&);

public:
#endif

 it_macguffin(int pid, unsigned char prarity, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor, material pm1, material pm2,
	 unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut,
	 signed char pm_to_hit, unsigned pitem_flags,

	 bool preadable
#ifndef SOCRATES_DAIMON
	 , decltype(use_pc) puse
#endif
 );

#ifndef SOCRATES_DAIMON
 void used_by(item& it, pc& u) const;
#endif

 const it_macguffin* is_macguffin() const override { return this; }
};

struct it_software : public itype
{
 software_type swtype; // 2020-04-03 not wired in
 int power; // 2020-04-03 not wired in

 it_software(int pid, unsigned int pprice,
	 std::string pname, std::string pdes,
	 char psym, nc_color pcolor,
	 software_type pswtype, int ppower);

 const it_software* is_software() const override { return this; }
};

struct it_style final : public itype
{
 using base = itype; // Cf. Objective-C

 std::vector<style_move> moves;
 
 it_style(int pid, std::string pname, std::string pdes, char psym, nc_color pcolor,
     signed char pmelee_dam, signed char pmelee_cut, signed char pm_to_hit,
     unsigned pitem_flags);

 const it_style* is_style() const override { return this; }

 int melee_value(const int skills[num_skill_types]) const override;

 void info(std::ostream& dest) const override;
};

struct it_artifact_tool final : public it_tool
{
 art_charge charge_type;
 std::vector<art_effect_passive> effects_wielded;
 std::vector<art_effect_active>  effects_activated;
 std::vector<art_effect_passive> effects_carried;

 it_artifact_tool(const cataclysm::JSON& src);
 friend std::ostream& operator<<(std::ostream& os, const it_artifact_tool& src);
 void toJSON(cataclysm::JSON& dest) const override;

 it_artifact_tool();
 it_artifact_tool(int pid, unsigned int pprice, std::string pname,
	 std::string pdes, char psym, nc_color pcolor, material pm1,
	 material pm2, unsigned short pvolume, unsigned short pweight,
	 signed char pmelee_dam, signed char pmelee_cut,
	 signed char pm_to_hit, unsigned pitem_flags);

 bool is_artifact() const override { return true; }
 const it_artifact_tool* is_artifact_tool() const override { return this; }
 std::string save_data() override;
};

struct it_artifact_armor final : public it_armor
{
 std::vector<art_effect_passive> effects_worn;

 it_artifact_armor() = default;
 it_artifact_armor(int pid, unsigned int pprice, std::string pname,
                   std::string pdes, char psym, nc_color pcolor, material pm1,
                   material pm2, unsigned short pvolume, unsigned short pweight,
                   signed char pmelee_dam, signed char pmelee_cut,
                   signed char pm_to_hit, unsigned pitem_flags,

                   unsigned char pcovers, signed char pencumber,
                   unsigned char pdmg_resist, unsigned char pcut_resist,
                   unsigned char penv_resist, signed char pwarmth,
                   unsigned char pstorage);
 it_artifact_armor(const cataclysm::JSON& src);
 friend std::ostream& operator<<(std::ostream& os, const it_artifact_armor& src);
 void toJSON(cataclysm::JSON& dest) const override;

 bool is_artifact() const override { return true; }
 const it_artifact_armor* is_artifact_armor() const override { return this; }
 std::string save_data() override;
};

// artifact.h is a zero-dependency header so catch these here
DECLARE_JSON_ENUM_SUPPORT(art_charge)
DECLARE_JSON_ENUM_SUPPORT(art_effect_active)
DECLARE_JSON_ENUM_SUPPORT(art_effect_passive)

#endif
