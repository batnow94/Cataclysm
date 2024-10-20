#ifndef _ITYPE_ENUM_H_
#define _ITYPE_ENUM_H_

enum itype_id : int {
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

enum technique_id {
	TEC_NULL,				// \todo redefine TEC_NULL as -1 (implementation-defined operator << but we should not be doing that anyway)
	// Offensive Techniques
	TEC_SWEEP,	// Crits may make your enemy fall & miss a turn
	TEC_PRECISE,	// Crits are painful and stun
	TEC_BRUTAL,	// Crits knock the target back
	TEC_GRAB,	// Hit may allow a second unarmed attack attempt
	TEC_WIDE,	// Attacks adjacent opponents
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

#endif
