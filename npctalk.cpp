#include "npc.h"
#include "game.h"
#include "output.h"
#include "dialogue.h"
#include "skill.h"
#include "rng.h"
#include "line.h"
#include "recent_msg.h"

#include <sstream>
#include <fstream>

// Every OWED_VAL that the NPC owes you counts as +1 towards convincing
#define OWED_VAL 250

// Some aliases to help with gen_responses
#define RESPONSE(txt)	   ret.push_back(talk_response());\
                           ret.back().text = txt

#define SELECT_MISS(txt, index)  ret.push_back(talk_response());\
                                 ret.back().text = txt;\
                                 ret.back().mission_index = index

#define SELECT_TEMP(txt, index)  ret.push_back(talk_response());\
                                 ret.back().text = txt;\
                                 ret.back().tempvalue = index

#define TRIAL(tr, diff) ret.back().trial = tr;\
                        ret.back().difficulty = diff

#define SUCCESS(topic)  ret.back().success = topic
#define FAILURE(topic)  ret.back().failure = topic

#define SUCCESS_OPINION(T, F, V, A, O)   ret.back().opinion_success =\
                                         npc_opinion(T, F, V, A, O)
#define FAILURE_OPINION(T, F, V, A, O)   ret.back().opinion_failure =\
                                         npc_opinion(T, F, V, A, O)

#define SUCCESS_ACTION(func)  ret.back().effect_success = func
#define FAILURE_ACTION(func)  ret.back().effect_failure = func

#define SUCCESS_MISSION(type) ret.back().miss = type

int topic_category(talk_topic topic);

talk_topic special_talk(char ch);

void npc::talk_to_u(game *g)
{
// This is necessary so that we don't bug the player over and over
 if (attitude == NPCATT_TALK) attitude = NPCATT_NULL;
 else if (attitude == NPCATT_FLEE) {
  messages.add("%s is fleeing you!", name.c_str());
  return;
 } else if (attitude == NPCATT_KILL) {
  messages.add("%s is hostile!", name.c_str());
  return;
 }
 {	// scoping brace
 dialogue d(g->u, *this);

 d.topic_stack.push_back(chatbin.first_topic);

 if (is_leader())
  d.topic_stack.push_back(TALK_LEADER);
 else if (is_friend())
  d.topic_stack.push_back(TALK_FRIEND);

 int most_difficult_mission = 0;
 for (int i = 0; i < chatbin.missions.size(); i++) {
  const mission_type *type = g->find_mission_type(chatbin.missions[i]);
  if (type->urgent && type->difficulty > most_difficult_mission) {
   d.topic_stack.push_back(TALK_MISSION_DESCRIBE);
   chatbin.mission_selected = i;
   most_difficult_mission = type->difficulty;
  }
 }
 most_difficult_mission = 0;
 bool chosen_urgent = false;
 for (int i = 0; i < chatbin.missions_assigned.size(); i++) {
  const mission_type *type = g->find_mission_type(chatbin.missions_assigned[i]);
  if ((type->urgent && !chosen_urgent) ||
      (type->difficulty > most_difficult_mission &&
       (type->urgent || !chosen_urgent)            )) {
   chosen_urgent = type->urgent;
   d.topic_stack.push_back(TALK_MISSION_INQUIRE);
   chatbin.mission_selected = i;
   most_difficult_mission = type->difficulty;
  }
 }

 if (d.topic_stack.back() == TALK_NONE)
  d.topic_stack.back() = pick_talk_topic(&(g->u));

 moves -= 100;
 decide_needs();

 d.win = newwin(VIEW, SCREEN_WIDTH, 0, 0);
 wborder(d.win, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
 for (int i = 1; i < VIEW-1; i++)
  mvwputch(d.win, i, SCREEN_WIDTH / 2 + 1, c_ltgray, LINE_XOXO);
 mvwputch(d.win,  0, SCREEN_WIDTH / 2 + 1, c_ltgray, LINE_OXXX);
 mvwputch(d.win, VIEW-1, SCREEN_WIDTH / 2 + 1, c_ltgray, LINE_XXOX);
 mvwprintz(d.win, 1,  1, c_white, "Dialogue with %s", name.c_str());
 mvwaddstrz(d.win, 1, SCREEN_WIDTH / 2 + 3, c_white, "Your response:");

// Main dialogue loop
 do { 
  talk_topic next = d.opt(d.topic_stack.back(), g);
  if (next == TALK_NONE) {
   int cat = topic_category(d.topic_stack.back());
   do
    d.topic_stack.pop_back();
   while (cat != -1 && topic_category(d.topic_stack.back()) == cat);
  }
  if (next == TALK_DONE || d.topic_stack.empty())
   d.done = true;
  else if (next != TALK_NONE)
   d.topic_stack.push_back(next);
 } while (!d.done);
 }	// trigger dialogue d destructor
 g->refresh_all();
}

// altered to throw on error conditions to allow the sole caller to respond more reasonably.
static std::string dynamic_line(talk_topic topic, game *g, npc& p)
{
// First, a sanity test for mission stuff
 if (topic >= TALK_MISSION_START && topic <= TALK_MISSION_END) {

  if (topic == TALK_MISSION_START) throw std::string("Used TALK_MISSION_START - not meant to be used!");
  if (topic == TALK_MISSION_END) throw std::string("Used TALK_MISSION_END - not meant to be used!");

  if (0 > p.chatbin.mission_selected) throw std::string("BUG: negative mission_selected: ")+std::to_string(p.chatbin.mission_selected);
  int id = -1;
  std::vector<int>& conversation_target = (topic == TALK_MISSION_INQUIRE || topic == TALK_MISSION_ACCEPTED ||
	  topic == TALK_MISSION_SUCCESS || topic == TALK_MISSION_ADVICE ||
	  topic == TALK_MISSION_FAILURE || topic == TALK_MISSION_SUCCESS_LIE) ? p.chatbin.missions_assigned : p.chatbin.missions;

  if (p.chatbin.mission_selected >= conversation_target.size()) throw std::string("BUG: mission_selected: ") + std::to_string(p.chatbin.mission_selected) + " exceeds strict upper bound " + std::to_string(conversation_target.size());
  id = conversation_target[p.chatbin.mission_selected];

// Mission stuff is a special case, so we'll handle it up here
  if (const mission* miss = mission::from_id(id)) {
	  const mission_type* type = miss->type;
	  std::string ret = mission_dialogue(mission_id(type->id), topic);
	  if (topic == TALK_MISSION_SUCCESS && miss->follow_up != MISSION_NULL)
		  return ret + "  And I have more I'd like you to do.";
	  return ret;
  } else {	// desired invariant failed
	  // intentionally leave the offending mission uid in place
	  throw std::string("BUG: non-existent mission uid ") + std::to_string(id);
  }

 }
  
 switch (topic) {
 case TALK_NONE:
 case TALK_DONE:
  return "";

 case TALK_MISSION_LIST:
  if (p.chatbin.missions.empty()) {
   if (p.chatbin.missions_assigned.empty())
    return "I don't have any jobs for you.";
   else
    return "I don't have any more jobs for you.";
  } else if (p.chatbin.missions.size() == 1) {
    if (p.chatbin.missions_assigned.empty())
     return "I just have one job for you.  Want to hear about it?";
    else
     return "I have other one job for you.  Want to hear about it?";
  } else if (p.chatbin.missions_assigned.empty())
    return "I have several jobs for you.  Which should I describe?";
  else
   return "I have several more jobs for you.  Which should I describe?";

 case TALK_MISSION_LIST_ASSIGNED:
  if (p.chatbin.missions_assigned.empty())
   return "You're not working on anything for me right now.";
  else if (p.chatbin.missions_assigned.size() == 1)
   return "What about it?";
  else
   return "Which job?";

 case TALK_MISSION_REWARD: return "Sure, here you go!";

 case TALK_SHELTER:
  switch (rng(1, 2)) {
   case 1: return "Well, I guess it's just us.";
   default: return "At least we've got shelter.";
  }

 case TALK_SHELTER_PLANS: return "I don't know, look for supplies and other survivors I guess.";

 case TALK_SHARE_EQUIPMENT:
  if (p.has_disease(DI_ASKED_FOR_ITEM))
   return "You just asked me for stuff; ask later.";
  return "Why should I share my equipment with you?";

 case TALK_GIVE_EQUIPMENT: return "Okay, here you go.";

 case TALK_DENY_EQUIPMENT:
  if (p.op_of_u.anger >= p.hostile_anger_level() - 4)
   return "<no>, and if you ask again, <ill_kill_you>!";
  else
   return "<no><punc> <fuck_you>!";

 case TALK_TRAIN: {
  if (g->u.backlog.type == ACT_TRAIN)
   return "Shall we resume?";
  std::vector<skill> trainable = p.skills_offered_to(g->u);
  std::vector<itype_id> styles = p.styles_offered_to(g->u);
  if (trainable.empty() && styles.empty())
   return "Sorry, but it doesn't seem I have anything to teach you.";
  else
   return "Here's what I can teach you...";
 } break;

 case TALK_TRAIN_START:
  if (overmap::is_safe(g->om_location()))
   return "Alright, let's begin.";
  else
   return "It's not safe here.  Let's get to safety first.";
 break;
  
 case TALK_TRAIN_FORCE: return "Alright, let's begin.";

 case TALK_SUGGEST_FOLLOW:
  if (p.has_disease(DI_INFECTION))
   return "Not until I get some antibiotics...";
  if (p.has_disease(DI_ASKED_TO_FOLLOW))
   return "You asked me recently; ask again later.";
  return "Why should I travel with you?";

 case TALK_AGREE_FOLLOW: return "You got it, I'm with you!";
 case TALK_DENY_FOLLOW: return "Yeah... I don't think so.";
 case TALK_LEADER: return "What is it?";
 case TALK_LEAVE: return "You're really leaving?";
 case TALK_PLAYER_LEADS: return "Alright.  You can lead now.";
 case TALK_LEADER_STAYS: return "No.  I'm the leader here.";

 case TALK_HOW_MUCH_FURTHER: {
  int dist = rl_dist(g->om_location(), *p.goal);
  std::ostringstream response;
  dist *= 100;
  if (dist >= 1300) {
   int miles = dist / 52; // *100, e.g. quarter mile is "25"
   miles -= miles % 25; // Round to nearest quarter-mile
   int fullmiles = (miles - miles % 100) / 100; // Left of the decimal point
   if (fullmiles > 0)
    response << fullmiles;
   response << "." << miles << " miles.";
  } else
   response << dist << " feet.";
  return response.str();
 }

 case TALK_FRIEND: return "What is it?";

 case TALK_COMBAT_COMMANDS: {
  std::ostringstream status;
  status << "*is "; // Prepending * makes this an action, not a phrase
  switch (p.combat_rules.engagement) {
  case ENGAGE_NONE:  status << "not engaging enemies.";         break;
  case ENGAGE_CLOSE: status << "engaging nearby enemies.";      break;
  case ENGAGE_WEAK:  status << "engaging weak enemies.";        break;
  case ENGAGE_HIT:   status << "engaging enemies you attack."; break;
  case ENGAGE_ALL:   status << "engaging all enemies.";         break;
  }
  status << " " << (p.male ? "He" : "She") << " will " <<
            (p.combat_rules.use_guns ? "" : "not ") << "use firearms.";
  status << " " << (p.male ? "He" : "She") << " will " <<
            (p.combat_rules.use_grenades ? "" : "not ") << "use grenades.";

  return status.str();
 }

 case TALK_COMBAT_ENGAGEMENT: return "What should I do?";

 case TALK_STRANGER_NEUTRAL:
  if (p.myclass == NC_TRADER)
   return "Hello!  Would you care to see my goods?";
  return "Hello there.";

 case TALK_STRANGER_WARY: return "Okay, no sudden movements...";
 case TALK_STRANGER_SCARED: return "Keep your distance!";

 case TALK_STRANGER_FRIENDLY:
  if (p.myclass == NC_TRADER)
   return "Hello!  Would you care to see my goods?";
  return "Hey there, <name_g>.";

 case TALK_STRANGER_AGGRESSIVE:
  if (!g->u.unarmed_attack())
   return "<drop_it>";
  else
   return "This is my territory, <name_b>.";

 case TALK_MUG:
  if (!g->u.unarmed_attack())
   return "<drop_it>";
  else
   return "<hands_up>";

 case TALK_DESCRIBE_MISSION:
  switch (p.mission) {
   case NPC_MISSION_RESCUE_U: return "I'm here to save you!";
   case NPC_MISSION_SHELTER: return "I'm holing up here for safety.";
   case NPC_MISSION_SHOPKEEP: return "I run the shop here.";
   case NPC_MISSION_MISSING: return "Well, I was lost, but you found me...";
   case NPC_MISSION_KIDNAPPED: return "Well, I was kidnapped, but you saved me...";
   case NPC_MISSION_NULL:
    switch (p.myclass) {
     case NC_HACKER: return "I'm looking for some choice systems to hack.";
     case NC_DOCTOR: return "I'm looking for wounded to help.";
     case NC_TRADER: return "I'm collecting gear and selling it.";
     case NC_NINJA:
      if (!p.styles.empty()) {
       std::ostringstream ret;
       ret << "I am a wandering master of " <<
		   item::types[p.styles[0]]->name.c_str() << ".";
       return ret.str();
      } else
       return "I am looking for a master to train my fighting techniques.";
     case NC_COWBOY: return "Just looking for some wrongs to right.";
     case NC_SCIENTIST: return "I'm looking for clues concerning these monsters' origins...";
     case NC_BOUNTY_HUNTER: return "I'm a killer for hire.";
     case NC_NONE: return "I'm just wandering.";
     default: return "ERROR: Someone forgot to code an npc_class text.";
    } // switch (p.myclass)
   default: return "ERROR: Someone forgot to code an npc_mission text.";
  } // switch (p.mission)
  break;

 case TALK_WEAPON_DROPPED: {
  std::ostringstream ret;
  ret << "*drops " << (p.male ? "his" : "her") << " weapon.";
  return ret.str();
 }
 
 case TALK_DEMAND_LEAVE:
  return "Now get out of here, before I kill you.";

 case TALK_SIZE_UP: {
  int ability = g->u.per_cur * 3 + g->u.int_cur;
  if (ability <= 10)
   return "&You can't make anything out.";

  std::ostringstream info;
  info << "&";
  int str_range = int(100 / ability);
  int str_min = int(p.str_max / str_range) * str_range;
  info << "Str " << str_min << " - " << str_min + str_range;

  if (ability >= 40) {
   int dex_range = int(160 / ability);
   int dex_min = int(p.dex_max / dex_range) * dex_range;
   info << "  Dex " << dex_min << " - " << dex_min + dex_range;
  }
  if (ability >= 50) {
   int int_range = int(200 / ability);
   int int_min = int(p.int_max / int_range) * int_range;
   info << "  Int " << int_min << " - " << int_min + int_range;
  }
  if (ability >= 60) {
   int per_range = int(240 / ability);
   int per_min = int(p.per_max / per_range) * per_range;
   info << "  Per " << per_min << " - " << per_min + per_range;
  }

  return info.str();
 } break;

 case TALK_LOOK_AT: {
  std::ostringstream look;
  look << "&" << p.short_description();
  return look.str();
 } break;

 case TALK_OPINION: {
  std::ostringstream opinion;
  opinion << "&" << p.op_of_u.text();
  return opinion.str();
 } break;

 }

 return "I don't know what to say. (BUG)";
}

static std::vector<talk_response> gen_responses(talk_topic topic, game *g, npc& p)
{
 std::vector<talk_response> ret;
 int selected = p.chatbin.mission_selected;
 mission *miss = nullptr;
 // agree with dynamic_line on which mission is relevant.
 if (0 <= selected)
	{
    std::vector<int>& conversation_target = (topic == TALK_MISSION_INQUIRE || topic == TALK_MISSION_ACCEPTED ||
		 topic == TALK_MISSION_SUCCESS || topic == TALK_MISSION_ADVICE ||
		 topic == TALK_MISSION_FAILURE || topic == TALK_MISSION_SUCCESS_LIE) ? p.chatbin.missions_assigned : p.chatbin.missions;

	 if (selected < conversation_target.size()) miss = mission::from_id(conversation_target[selected]);	// non-null as dynamic_line validated this
	}

 switch (topic) {
 case TALK_MISSION_LIST:
  if (p.chatbin.missions.empty()) {
   RESPONSE("Oh, okay.");
    SUCCESS(TALK_NONE);
  } else if (p.chatbin.missions.size() == 1) {
   SELECT_MISS("Tell me about it.", 0);
    SUCCESS(TALK_MISSION_OFFER);
   RESPONSE("Never mind, I'm not interested.");
    SUCCESS(TALK_NONE);
  } else {
   for (int i = 0; i < p.chatbin.missions.size(); i++) {
    SELECT_MISS(g->find_mission_type( p.chatbin.missions[i] )->name, i);
     SUCCESS(TALK_MISSION_OFFER);
   }
   RESPONSE("Never mind, I'm not interested.");
    SUCCESS(TALK_NONE);
  }
  break;

 case TALK_MISSION_LIST_ASSIGNED:
  if (p.chatbin.missions_assigned.empty()) {
   RESPONSE("Never mind then.");
    SUCCESS(TALK_NONE);
  } else if (p.chatbin.missions_assigned.size() == 1) {
   SELECT_MISS("I have news.", 0);
    SUCCESS(TALK_MISSION_INQUIRE);
   RESPONSE("Never mind.");
    SUCCESS(TALK_NONE);
  } else {
   for (int i = 0; i < p.chatbin.missions_assigned.size(); i++) {
    SELECT_MISS(g->find_mission_type( p.chatbin.missions_assigned[i] )->name,
                i);
     SUCCESS(TALK_MISSION_INQUIRE);
   }
   RESPONSE("Never mind.");
    SUCCESS(TALK_NONE);
  }
  break;

 case TALK_MISSION_DESCRIBE:
  RESPONSE("What's the matter?");
   SUCCESS(TALK_MISSION_OFFER);
  RESPONSE("I don't care.");
   SUCCESS(TALK_MISSION_REJECTED);
  break;

 case TALK_MISSION_OFFER:
  RESPONSE("I'll do it!");
   SUCCESS(TALK_MISSION_ACCEPTED);
   SUCCESS_ACTION(&talk_function::assign_mission);
  RESPONSE("Not interested.");
   SUCCESS(TALK_MISSION_REJECTED);
  break;

 case TALK_MISSION_ACCEPTED:
  RESPONSE("Not a problem.");
   SUCCESS(TALK_NONE);
  RESPONSE("Got any advice?");
   SUCCESS(TALK_MISSION_ADVICE);
  RESPONSE("Can you share some equipment?");
   SUCCESS(TALK_SHARE_EQUIPMENT);
  RESPONSE("I'll be back soon!");
   SUCCESS(TALK_DONE);
  break;

 case TALK_MISSION_ADVICE:
  RESPONSE("Sounds good, thanks.");
   SUCCESS(TALK_NONE);
  RESPONSE("Sounds good.  Bye!");
   SUCCESS(TALK_DONE);
  break;

 case TALK_MISSION_REJECTED:
  RESPONSE("I'm sorry.");
   SUCCESS(TALK_NONE);
  RESPONSE("Whatever.  Bye.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_MISSION_INQUIRE: {
  int id = p.chatbin.missions_assigned[ p.chatbin.mission_selected ];
  if (miss->failed) {
   RESPONSE("I'm sorry... I failed.");
    SUCCESS(TALK_MISSION_FAILURE);
     SUCCESS_OPINION(-1, 0, -1, 1, 0);
   RESPONSE("Not yet.");
    TRIAL(TALK_TRIAL_LIE, 10 + p.op_of_u.trust * 3);
    SUCCESS(TALK_NONE);
    FAILURE(TALK_MISSION_FAILURE);
     FAILURE_OPINION(-3, 0, -1, 2, 0);
  } else if (!miss->is_complete(g->u, p.ID())) {
   const mission_type *type = miss->type;
   RESPONSE("Not yet.");
    SUCCESS(TALK_NONE);
   if (type->goal == MGOAL_KILL_MONSTER) {
    RESPONSE("Yup, I killed it.");
    TRIAL(TALK_TRIAL_LIE, 10 + p.op_of_u.trust * 5);
    SUCCESS(TALK_MISSION_SUCCESS);
     SUCCESS_ACTION(&talk_function::mission_success);
    FAILURE(TALK_MISSION_SUCCESS_LIE);
     FAILURE_OPINION(-5, 0, -1, 5, 0);
     FAILURE_ACTION(&talk_function::mission_failure);
   }
   RESPONSE("No.  I'll get back to it, bye!");
    SUCCESS(TALK_DONE);
  } else {
// TODO: Lie about mission
   const mission_type *type = miss->type;
   switch (type->goal) {
   case MGOAL_FIND_ITEM:
   case MGOAL_FIND_ANY_ITEM:
    RESPONSE("Yup!  Here it is!");
     SUCCESS(TALK_MISSION_SUCCESS);
     SUCCESS_ACTION(&talk_function::mission_success);
    break;
   case MGOAL_GO_TO:
   case MGOAL_FIND_NPC:
    RESPONSE("Here I am.");
     SUCCESS(TALK_MISSION_SUCCESS);
     SUCCESS_ACTION(&talk_function::mission_success);
    break;
   case MGOAL_FIND_MONSTER:
    RESPONSE("Here it is!");
     SUCCESS(TALK_MISSION_SUCCESS);
     SUCCESS_ACTION(&talk_function::mission_success);
    break;
   case MGOAL_KILL_MONSTER:
    RESPONSE("I killed it.");
     SUCCESS(TALK_MISSION_SUCCESS);
     SUCCESS_ACTION(&talk_function::mission_success);
    break;
   default:
    RESPONSE("Mission success!  I don't know what else to say.");
     SUCCESS(TALK_MISSION_SUCCESS);
     SUCCESS_ACTION(&talk_function::mission_success);
    break;
   }
  }
 } break;

 case TALK_MISSION_SUCCESS:
  RESPONSE("Glad to help.  I need no payment.");
   SUCCESS(TALK_NONE);
   SUCCESS_OPINION(miss->value / (OWED_VAL * 4), -1,
                   miss->value / (OWED_VAL * 2), -1, 0 - miss->value);
   SUCCESS_ACTION(&talk_function::clear_mission);
  RESPONSE("How about some items as payment?");
   SUCCESS(TALK_MISSION_REWARD);
   SUCCESS_ACTION(&talk_function::mission_reward);
  SELECT_TEMP("Maybe you can teach me something as payment.", 0);
   SUCCESS(TALK_TRAIN);
   SUCCESS_ACTION(&talk_function::clear_mission);
  RESPONSE("Alright, well, you owe me one.");
   SUCCESS(TALK_NONE);
   SUCCESS_ACTION(&talk_function::clear_mission);
  RESPONSE("Glad to help.  I need no payment.  Bye!");
   SUCCESS(TALK_DONE);
   SUCCESS_ACTION(&talk_function::clear_mission);
   SUCCESS_OPINION(p.op_of_u.owed / (OWED_VAL * 4), -1,
                   p.op_of_u.owed / (OWED_VAL * 2), -1, 0 - miss->value);
  break;

 case TALK_MISSION_SUCCESS_LIE:
  RESPONSE("Well, um, sorry.");
   SUCCESS(TALK_NONE);
   SUCCESS_ACTION(&talk_function::clear_mission);
  break;

 case TALK_MISSION_FAILURE:
  RESPONSE("I'm sorry.  I did what I could.");
   SUCCESS(TALK_NONE);
  break;

 case TALK_MISSION_REWARD:
  RESPONSE("Thank you.");
   SUCCESS(TALK_NONE);
   SUCCESS_ACTION(&talk_function::clear_mission);
  RESPONSE("Thanks, bye.");
   SUCCESS(TALK_DONE);
   SUCCESS_ACTION(&talk_function::clear_mission);
  break;

 case TALK_SHELTER:
  RESPONSE("What should we do now?");
   SUCCESS(TALK_SHELTER_PLANS);
  RESPONSE("Can I do anything for you?");
   SUCCESS(TALK_MISSION_LIST);
  if (!p.is_following()) {
   RESPONSE("Want to travel with me?");
    SUCCESS(TALK_SUGGEST_FOLLOW);
  }
  if (p.chatbin.missions_assigned.size() == 1) {
   RESPONSE("About that job...");
    SUCCESS(TALK_MISSION_INQUIRE);
  } else if (p.chatbin.missions_assigned.size() >= 2) {
   RESPONSE("About one of those jobs...");
    SUCCESS(TALK_MISSION_LIST_ASSIGNED);
  }
  RESPONSE("I can't leave the shelter without equipment...");
   SUCCESS(TALK_SHARE_EQUIPMENT);
  RESPONSE("Well, bye.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_SHELTER_PLANS:
// TODO: Add "follow me"
  RESPONSE("Hmm, okay.  Bye.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_SHARE_EQUIPMENT:
  if (p.has_disease(DI_ASKED_FOR_ITEM)) {
   RESPONSE("Okay, fine.");
    SUCCESS(TALK_NONE);
  } else {
   int score = p.op_of_u.trust + p.op_of_u.value * 3 +
               p.personality.altruism * 2;
   int missions_value = p.assigned_missions_value();
   RESPONSE("Because I'm your friend!");
    TRIAL(TALK_TRIAL_PERSUADE, 10 + score);
    SUCCESS(TALK_GIVE_EQUIPMENT);
     SUCCESS_ACTION(&talk_function::give_equipment);
     SUCCESS_OPINION(0, 0, -1, 0, score * 300);
    FAILURE(TALK_DENY_EQUIPMENT);
     FAILURE_OPINION(0, 0, -1, 0, 0);
   if (missions_value >= 1) {
    RESPONSE("Well, I am helping you out...");
     TRIAL(TALK_TRIAL_PERSUADE, 12 + (.8 * score) + missions_value / OWED_VAL);
     SUCCESS(TALK_GIVE_EQUIPMENT);
      SUCCESS_ACTION(&talk_function::give_equipment);
     FAILURE(TALK_DENY_EQUIPMENT);
      FAILURE_OPINION(0, 0, -1, 0, 0);
   }
   RESPONSE("I'll give it back!");
    TRIAL(TALK_TRIAL_LIE, score * 1.5);
    SUCCESS(TALK_GIVE_EQUIPMENT);
     SUCCESS_ACTION(&talk_function::give_equipment);
     SUCCESS_OPINION(0, 0, -1, 0, score * 300);
    FAILURE(TALK_DENY_EQUIPMENT);
     FAILURE_OPINION(0, -1, -1, 1, 0);
   RESPONSE("Give it to me, or else!");
    TRIAL(TALK_TRIAL_INTIMIDATE, 40);
    SUCCESS(TALK_GIVE_EQUIPMENT);
     SUCCESS_ACTION(&talk_function::give_equipment);
     SUCCESS_OPINION(-3, 2, -2, 2,
                     (g->u.intimidation() + p.op_of_u.fear -
                      p.personality.bravery - p.intimidation()) * 500);
    FAILURE(TALK_DENY_EQUIPMENT);
     FAILURE_OPINION(-3, 1, -3, 5, 0);
   RESPONSE("Eh, never mind.");
    SUCCESS(TALK_NONE);
   RESPONSE("Never mind, I'll do without.  Bye.");
    SUCCESS(TALK_DONE);
  }
  break;

 case TALK_GIVE_EQUIPMENT:
  RESPONSE("Thank you!");
   SUCCESS(TALK_NONE);
  RESPONSE("Thanks!  But can I have some more?");
   SUCCESS(TALK_SHARE_EQUIPMENT);
  RESPONSE("Thanks, see you later!");
   SUCCESS(TALK_DONE);
  break;

 case TALK_DENY_EQUIPMENT:
  RESPONSE("Okay, okay, sorry.");
   SUCCESS(TALK_NONE);
  RESPONSE("Seriously, give me more stuff!");
   SUCCESS(TALK_SHARE_EQUIPMENT);
  RESPONSE("Okay, fine, bye.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_TRAIN: {
  if (g->u.backlog.type == ACT_TRAIN) {
   std::ostringstream resume;
   resume << "Yes, let's resume training " <<
             (g->u.backlog.index > 0 ?
              skill_name(skill(g->u.backlog.index)) :
              item::types[ 0 - g->u.backlog.index ]->name);
   SELECT_TEMP( resume.str(), g->u.backlog.index);
    SUCCESS(TALK_TRAIN_START);
  }
  std::vector<skill> trainable = p.skills_offered_to(g->u);
  std::vector<itype_id> styles = p.styles_offered_to(g->u);
  if (trainable.empty() && styles.empty()) {
   RESPONSE("Oh, okay."); // Nothing to learn here
    SUCCESS(TALK_NONE);
  }
  int printed = 0;
  int shift = p.chatbin.tempvalue;
  bool more = trainable.size() + styles.size() - shift > 9;
  for (int i = shift; i < trainable.size() && printed < 9; i++) {
   //shift--;
   printed++;
   std::ostringstream skilltext;
   skill trained = trainable[i];

   skilltext << skill_name(trained) << ": " << g->u.sklevel[trained] <<
                " -> " << g->u.sklevel[trained] + 1 << "(cost " <<
                200 * (g->u.sklevel[trained] + 1) << ")";
   SELECT_TEMP( skilltext.str(), trainable[i] );
    SUCCESS(TALK_TRAIN_START);
  }
  if (shift < 0)
   shift = 0;
  for (int i = 0; i < styles.size() && printed < 9; i++) {
   printed++;
   SELECT_TEMP(item::types[styles[i]]->name + " (cost 800)",
               0 - styles[i] );
    SUCCESS(TALK_TRAIN_START);
  }
  if (more) {
   SELECT_TEMP("More...", shift + 9);
    SUCCESS(TALK_TRAIN);
  }
  if (shift > 0) {
   int newshift = shift - 9;
   if (newshift < 0)
    newshift = 0;
   SELECT_TEMP("Back...", newshift);
    SUCCESS(TALK_TRAIN);
  }
  RESPONSE("Eh, never mind.");
   SUCCESS(TALK_NONE);
  } break;

 case TALK_TRAIN_START:
  if (overmap::is_safe(g->om_location())) {
   RESPONSE("Sounds good.");
    SUCCESS(TALK_DONE);
    SUCCESS_ACTION(&talk_function::start_training);
   RESPONSE("On second thought, never mind.");
    SUCCESS(TALK_NONE);
  } else {
   RESPONSE("Okay.  Lead the way.");
    SUCCESS(TALK_DONE);
    SUCCESS_ACTION(&talk_function::lead_to_safety);
   RESPONSE("No, we'll be okay here.");
    SUCCESS(TALK_TRAIN_FORCE);
   RESPONSE("On second thought, never mind.");
    SUCCESS(TALK_NONE);
  }
  break;

 case TALK_TRAIN_FORCE:
  RESPONSE("Sounds good.");
   SUCCESS(TALK_DONE);
   SUCCESS_ACTION(&talk_function::start_training);
  RESPONSE("On second thought, never mind.");
   SUCCESS(TALK_NONE);
  break;

 case TALK_SUGGEST_FOLLOW:
  if (p.has_disease(DI_INFECTION)) {
   RESPONSE("Understood.  I'll get those antibiotics.");
    SUCCESS(TALK_NONE);
  } else if (p.has_disease(DI_ASKED_TO_FOLLOW)) {
   RESPONSE("Right, right, I'll ask later.");
    SUCCESS(TALK_NONE);
  } else {
   int strength = 3 * p.op_of_u.fear + p.op_of_u.value + p.op_of_u.trust +
                  (10 - p.personality.bravery);
   int weakness = 3 * p.personality.altruism + p.personality.bravery -
                  p.op_of_u.fear + p.op_of_u.value;
   int friends = 2 * p.op_of_u.trust + 2 * p.op_of_u.value -
                 2 * p.op_of_u.anger + p.op_of_u.owed / 50;
   RESPONSE("I can keep you safe.");
    TRIAL(TALK_TRIAL_PERSUADE, strength * 2);
    SUCCESS(TALK_AGREE_FOLLOW);
     SUCCESS_ACTION(&talk_function::follow);
     SUCCESS_OPINION(1, 0, 1, 0, 0);
    FAILURE(TALK_DENY_FOLLOW);
     FAILURE_ACTION(&talk_function::deny_follow);
     FAILURE_OPINION(0, 0, -1, 1, 0);
   RESPONSE("You can keep me safe.");
    TRIAL(TALK_TRIAL_PERSUADE, weakness * 2);
    SUCCESS(TALK_AGREE_FOLLOW);
     SUCCESS_ACTION(&talk_function::follow);
     SUCCESS_OPINION(0, 0, -1, 0, 0);
    FAILURE(TALK_DENY_FOLLOW);
     FAILURE_ACTION(&talk_function::deny_follow);
     FAILURE_OPINION(0, -1, -1, 1, 0);
   RESPONSE("We're friends, aren't we?");
    TRIAL(TALK_TRIAL_PERSUADE, friends * 1.5);
    SUCCESS(TALK_AGREE_FOLLOW);
     SUCCESS_ACTION(&talk_function::follow);
     SUCCESS_OPINION(2, 0, 0, -1, 0);
    FAILURE(TALK_DENY_FOLLOW);
     FAILURE_ACTION(&talk_function::deny_follow);
     FAILURE_OPINION(-1, -2, -1, 1, 0);
   RESPONSE("!I'll kill you if you don't.");
    TRIAL(TALK_TRIAL_INTIMIDATE, strength * 2);
    SUCCESS(TALK_AGREE_FOLLOW);
     SUCCESS_ACTION(&talk_function::follow);
     SUCCESS_OPINION(-4, 3, -1, 4, 0);
    FAILURE(TALK_DENY_FOLLOW);
     FAILURE_OPINION(-4, 0, -5, 10, 0);
  }
  break;

 case TALK_AGREE_FOLLOW:
  RESPONSE("Awesome!");
   SUCCESS(TALK_NONE);
  RESPONSE("Okay, let's go!");
   SUCCESS(TALK_DONE);
  break;

 case TALK_DENY_FOLLOW:
  RESPONSE("Oh, okay.");
   SUCCESS(TALK_NONE);
  break;

 case TALK_LEADER: {
  int persuade = p.op_of_u.fear + p.op_of_u.value + p.op_of_u.trust -
                 p.personality.bravery - p.personality.aggression;
  if (p.has_destination()) {
   RESPONSE("How much further?");
    SUCCESS(TALK_HOW_MUCH_FURTHER);
  }
  RESPONSE("I'm going to go my own way for a while.");
   SUCCESS(TALK_LEAVE);
  if (!p.has_disease(DI_ASKED_TO_LEAD)) {
   RESPONSE("I'd like to lead for a while.");
    TRIAL(TALK_TRIAL_PERSUADE, persuade);
    SUCCESS(TALK_PLAYER_LEADS);
     SUCCESS_ACTION(&talk_function::follow);
    FAILURE(TALK_LEADER_STAYS);
     FAILURE_OPINION(0, 0, -1, -1, 0);
   RESPONSE("Step aside.  I'm leader now.");
    TRIAL(TALK_TRIAL_INTIMIDATE, 40);
    SUCCESS(TALK_PLAYER_LEADS);
     SUCCESS_ACTION(&talk_function::follow);
     SUCCESS_OPINION(-1, 1, -1, 1, 0);
    FAILURE(TALK_LEADER_STAYS);
     FAILURE_OPINION(-1, 0, -1, 1, 0);
  }
  RESPONSE("Can I do anything for you?");
   SUCCESS(TALK_MISSION_LIST);
  RESPONSE("Let's trade items.");
   SUCCESS(TALK_NONE);
   SUCCESS_ACTION(&talk_function::start_trade);
  RESPONSE("Let's go.");
   SUCCESS(TALK_DONE);
 } break;

 case TALK_LEAVE:
  RESPONSE("Nah, I'm just kidding.");
   SUCCESS(TALK_NONE);
  RESPONSE("Yeah, I'm sure.  Bye.");
   SUCCESS_ACTION(&talk_function::leave);
   SUCCESS(TALK_DONE);
  break;

 case TALK_PLAYER_LEADS:
  RESPONSE("Good.  Something else...");
   SUCCESS(TALK_FRIEND);
  RESPONSE("Alright, let's go.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_LEADER_STAYS:
  RESPONSE("Okay, okay.");
   SUCCESS(TALK_NONE);
  break;

 case TALK_HOW_MUCH_FURTHER:
  RESPONSE("Okay, thanks.");
   SUCCESS(TALK_NONE);
  RESPONSE("Let's keep moving.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_FRIEND:
  RESPONSE("Combat commands...");
   SUCCESS(TALK_COMBAT_COMMANDS);
  RESPONSE("Can I do anything for you?");
   SUCCESS(TALK_MISSION_LIST);
  SELECT_TEMP("Can you teach me anything?", 0);
   SUCCESS(TALK_TRAIN);
  RESPONSE("Let's trade items.");
   SUCCESS(TALK_NONE);
   SUCCESS_ACTION(&talk_function::start_trade);
  RESPONSE("Let's go.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_COMBAT_COMMANDS: {
  RESPONSE("Change your engagement rules...");
   SUCCESS(TALK_COMBAT_ENGAGEMENT);
  if (p.combat_rules.use_guns) {
   RESPONSE("Don't use guns anymore.");
    SUCCESS(TALK_COMBAT_COMMANDS);
    SUCCESS_ACTION(&talk_function::toggle_use_guns);
  } else {
   RESPONSE("You can use guns.");
    SUCCESS(TALK_COMBAT_COMMANDS);
    SUCCESS_ACTION(&talk_function::toggle_use_guns);
  }
  if (p.combat_rules.use_grenades) {
   RESPONSE("Don't use grenades anymore.");
    SUCCESS(TALK_COMBAT_COMMANDS);
    SUCCESS_ACTION(&talk_function::toggle_use_grenades);
  } else {
   RESPONSE("You can use grenades.");
    SUCCESS(TALK_COMBAT_COMMANDS);
    SUCCESS_ACTION(&talk_function::toggle_use_grenades);
  }
  RESPONSE("Never mind.");
   SUCCESS(TALK_NONE);
 } break;

 case TALK_COMBAT_ENGAGEMENT: {
  if (p.combat_rules.engagement != ENGAGE_NONE) {
   RESPONSE("Don't fight unless your life depends on it.");
    SUCCESS(TALK_NONE);
    SUCCESS_ACTION(&talk_function::set_engagement_none);
  }
  if (p.combat_rules.engagement != ENGAGE_CLOSE) {
   RESPONSE("Attack enemies that get too close.");
    SUCCESS(TALK_NONE);
    SUCCESS_ACTION(&talk_function::set_engagement_close);
  }
  if (p.combat_rules.engagement != ENGAGE_WEAK) {
   RESPONSE("Attack enemies that you can kill easily.");
    SUCCESS(TALK_NONE);
    SUCCESS_ACTION(&talk_function::set_engagement_weak);
  }
  if (p.combat_rules.engagement != ENGAGE_HIT) {
   RESPONSE("Attack only enemies that I attack first.");
    SUCCESS(TALK_NONE);
    SUCCESS_ACTION(&talk_function::set_engagement_hit);
  }
  if (p.combat_rules.engagement != ENGAGE_ALL) {
   RESPONSE("Attack anything you want.");
    SUCCESS(TALK_NONE);
    SUCCESS_ACTION(&talk_function::set_engagement_all);
  }
  RESPONSE("Never mind.");
   SUCCESS(TALK_NONE);
 } break;

 case TALK_STRANGER_NEUTRAL:
 case TALK_STRANGER_WARY:
 case TALK_STRANGER_SCARED:
 case TALK_STRANGER_FRIENDLY:
  if (topic == TALK_STRANGER_NEUTRAL || topic == TALK_STRANGER_FRIENDLY) {
   RESPONSE("Another survivor!  We should travel together.");
    SUCCESS(TALK_SUGGEST_FOLLOW);
   RESPONSE("What are you doing?");
    SUCCESS(TALK_DESCRIBE_MISSION);
   RESPONSE("Care to trade?");
    SUCCESS(TALK_NONE);
    SUCCESS_ACTION(&talk_function::start_trade);
  } else {
   if (!g->u.unarmed_attack()) {
    if (g->u.volume_carried() + g->u.weapon.volume() <= g->u.volume_capacity()){
     RESPONSE("&Put away weapon.");
      SUCCESS(TALK_STRANGER_NEUTRAL);
      SUCCESS_ACTION(&talk_function::player_weapon_away);
      SUCCESS_OPINION(2, -2, 0, 0, 0);
    }
    RESPONSE("&Drop weapon.");
     SUCCESS(TALK_STRANGER_NEUTRAL);
     SUCCESS_ACTION(&talk_function::player_weapon_drop);
     SUCCESS_OPINION(4, -3, 0, 0, 0);
   }
   int diff = 50 + p.personality.bravery - 2 * p.op_of_u.fear +
                                            2 * p.op_of_u.trust;
   RESPONSE("Don't worry, I'm not going to hurt you.");
    TRIAL(TALK_TRIAL_PERSUADE, diff);
    SUCCESS(TALK_STRANGER_NEUTRAL);
     SUCCESS_OPINION(1, -1, 0, 0, 0);
    FAILURE(TALK_DONE);
     FAILURE_ACTION(&talk_function::flee);
  }
  if (!p.unarmed_attack()) {
   RESPONSE("!Drop your weapon!");
    TRIAL(TALK_TRIAL_INTIMIDATE, 30);
    SUCCESS(TALK_WEAPON_DROPPED);
     SUCCESS_ACTION(&talk_function::drop_weapon);
    FAILURE(TALK_DONE);
     FAILURE_ACTION(&talk_function::hostile);
  }
  RESPONSE("!Get out of here or I'll kill you.");
   TRIAL(TALK_TRIAL_INTIMIDATE, 20);
   SUCCESS(TALK_DONE);
    SUCCESS_ACTION(&talk_function::flee);
   FAILURE(TALK_DONE);
    FAILURE_ACTION(&talk_function::hostile);
  break;

 case TALK_STRANGER_AGGRESSIVE:
 case TALK_MUG:
  if (!g->u.unarmed_attack()) {
   int chance = 30 + p.personality.bravery - 3 * p.personality.aggression +
                 2 * p.personality.altruism - 2 * p.op_of_u.fear +
                 3 * p.op_of_u.trust;
   RESPONSE("!Calm down.  I'm not going to hurt you.");
    TRIAL(TALK_TRIAL_PERSUADE, chance);
    SUCCESS(TALK_STRANGER_WARY);
     SUCCESS_OPINION(1, -1, 0, 0, 0);
    FAILURE(TALK_DONE);
     FAILURE_ACTION(&talk_function::hostile);
   RESPONSE("!Screw you, no.");
    TRIAL(TALK_TRIAL_INTIMIDATE, chance - 5);
    SUCCESS(TALK_STRANGER_SCARED);
     SUCCESS_OPINION(-2, 1, 0, 1, 0);
    FAILURE(TALK_DONE);
     FAILURE_ACTION(&talk_function::hostile);
   RESPONSE("&Drop weapon.");
    if (topic == TALK_MUG) {
     SUCCESS(TALK_MUG);
    } else {
     SUCCESS(TALK_DEMAND_LEAVE);
    }
    SUCCESS_ACTION(&talk_function::player_weapon_drop);
  } else if (topic == TALK_MUG) {
   int chance = 35 + p.personality.bravery - 3 * p.personality.aggression +
                 2 * p.personality.altruism - 2 * p.op_of_u.fear +
                 3 * p.op_of_u.trust;
   RESPONSE("!Calm down.  I'm not going to hurt you.");
    TRIAL(TALK_TRIAL_PERSUADE, chance);
    SUCCESS(TALK_STRANGER_WARY);
     SUCCESS_OPINION(1, -1, 0, 0, 0);
    FAILURE(TALK_DONE);
     FAILURE_ACTION(&talk_function::hostile);
   RESPONSE("!Screw you, no.");
    TRIAL(TALK_TRIAL_INTIMIDATE, chance - 5);
    SUCCESS(TALK_STRANGER_SCARED);
     SUCCESS_OPINION(-2, 1, 0, 1, 0);
    FAILURE(TALK_DONE);
     FAILURE_ACTION(&talk_function::hostile);
   RESPONSE("&Put hands up.");
    SUCCESS(TALK_DONE);
    SUCCESS_ACTION(&talk_function::start_mugging);
  }
  break;

 case TALK_DESCRIBE_MISSION:
  RESPONSE("I see.");
   SUCCESS(TALK_NONE);
  RESPONSE("Bye.");
   SUCCESS(TALK_DONE);
  break;

 case TALK_WEAPON_DROPPED:
  RESPONSE("Now get out of here.");
   SUCCESS(TALK_DONE);
   SUCCESS_OPINION(-1, -2, 0, -2, 0);
   SUCCESS_ACTION(&talk_function::flee);
  break;

 case TALK_DEMAND_LEAVE:
  RESPONSE("Okay, I'm going.");
   SUCCESS(TALK_DONE);
   SUCCESS_OPINION(0, -1, 0, 0, 0);
   SUCCESS_ACTION(&talk_function::player_leaving);
  break;

 case TALK_SIZE_UP:
 case TALK_LOOK_AT:
 case TALK_OPINION:
  RESPONSE("Okay.");
   SUCCESS(TALK_NONE);
  break;
 }

 if (ret.empty()) {
  RESPONSE("Bye.");
   SUCCESS(TALK_DONE);
 }

 return ret;
}

static int trial_chance(talk_response response, const player& u, const npc& p)
{
 talk_trial trial = response.trial;
 int chance = response.difficulty;
 switch (trial) {
  case TALK_TRIAL_LIE:
   chance += u.talk_skill() - p.talk_skill() + p.op_of_u.trust * 3;
   if (u.has_trait(PF_TRUTHTELLER)) chance -= 40;
   break;

  case TALK_TRIAL_PERSUADE:
   chance += u.talk_skill() - int(p.talk_skill() / 2) + p.op_of_u.trust * 2 + p.op_of_u.value;
   if (u.has_trait(PF_GROWL)) chance -= 25;
   if (u.has_trait(PF_SNARL)) chance -= 60;
   break;

  case TALK_TRIAL_INTIMIDATE:
   chance += u.intimidation() - p.intimidation() + p.op_of_u.fear * 2 - p.personality.bravery * 2;
   if (u.has_trait(PF_TERRIFYING)) chance += 15;
   if (p.has_trait(PF_TERRIFYING)) chance -= 15;
   if (u.has_trait(PF_GROWL)) chance += 15;
   if (u.has_trait(PF_SNARL)) chance += 30;
   break;

 }

 if (chance < 0) return 0;
 if (chance > 100) return 100;

 return chance;
}

int topic_category(talk_topic topic)
{
 switch (topic) {
  case TALK_MISSION_START:
  case TALK_MISSION_DESCRIBE:
  case TALK_MISSION_OFFER:
  case TALK_MISSION_ACCEPTED:
  case TALK_MISSION_REJECTED:
  case TALK_MISSION_ADVICE:
  case TALK_MISSION_INQUIRE:
  case TALK_MISSION_SUCCESS:
  case TALK_MISSION_SUCCESS_LIE:
  case TALK_MISSION_FAILURE:
  case TALK_MISSION_REWARD:
  case TALK_MISSION_END:
   return 1;

  case TALK_SHARE_EQUIPMENT:
  case TALK_GIVE_EQUIPMENT:
  case TALK_DENY_EQUIPMENT:
   return 2;

  case TALK_SUGGEST_FOLLOW:
  case TALK_AGREE_FOLLOW:
  case TALK_DENY_FOLLOW:
   return 3;

  case TALK_COMBAT_ENGAGEMENT:
   return 4;

  case TALK_COMBAT_COMMANDS:
   return 5;

  case TALK_TRAIN:
  case TALK_TRAIN_START:
  case TALK_TRAIN_FORCE:
   return 6;

  case TALK_SIZE_UP:
  case TALK_LOOK_AT:
  case TALK_OPINION:
   return 99;

  default:
   return -1; // Not grouped with other topics
 }
 return -1;
}

static bool trade(pc& u, npc& p, int cost, std::string deal)
{
    constexpr const int header_height = 4;
    std::unique_ptr<WINDOW, curses_full_delete> w_head(newwin(header_height, SCREEN_WIDTH, 0, 0));
    std::unique_ptr<WINDOW, curses_full_delete> w_them(newwin(VIEW - header_height, SCREEN_WIDTH / 2, header_height, 0));
    std::unique_ptr<WINDOW, curses_full_delete> w_you(newwin(VIEW - header_height, SCREEN_WIDTH / 2, header_height, SCREEN_WIDTH / 2));
    mvwprintz(w_head.get(), 0, 0, c_white, "Trading with %s\n\
Tab key to switch lists, letters to pick items, Enter to finalize, Esc to quit\n\
? to get information on an item", p.name.c_str());

    // Set up line drawings
    for (int i = 0; i < SCREEN_WIDTH; i++)
        mvwputch(w_head.get(), header_height - 1, i, c_white, LINE_OXOX);
    wrefresh(w_head.get());

    // End of line drawings

   // Populate the list of what the NPC is willing to buy, and the prices they pay
   // Note that the NPC's barter skill is factored into these prices.
    std::vector<int> theirs, their_price, yours, your_price;
    p.init_selling(theirs, their_price);
    p.init_buying(u.inv, yours, your_price);
    std::vector<bool> getting_theirs(theirs.size(), false);
    std::vector<bool> getting_yours(yours.size(), false);

    // price_adjustment(...) is already notably broken (trading loops?) so probably ok that the 
    // intelligence delta is almost always zero
    // * if player is hyper-intelligent then negative prices possible both ways
    // Adjust the prices based on your barter skill.
    const double price_scale = u.barter_price_adjustment() + (p.int_cur - u.int_cur) / 15;
    for (int& his_price : their_price) his_price *= price_scale;
    for (int& my_price : your_price) my_price /= price_scale;

    const int v_hgt = clamped_ub<26>(VIEW - 8); // view height into inventory listings; limit is due to menu implementation
    int  cash = cost;// How much cash you get in the deal (negative = losing money)
    bool focus_them = true;	// Is the focus on them?
    bool update = true;		// Re-draw the screen?
    int  them_off = 0, you_off = 0;// Offset from the start of the list
    int  ch, help;

    do {
        if (update) {	// Time to re-draw
            update = false;
            // Draw borders, one of which is highlighted
            werase(w_them.get());
            werase(w_you.get());
            for (int i = 1; i < SCREEN_WIDTH; i++)
                mvwputch(w_head.get(), header_height - 1, i, c_white, LINE_OXOX);
            auto test = (cash < 0 ? u.cash >= cash * -1 : p.cash >= cash);
            mvwprintz(w_head.get(), header_height - 1, SCREEN_WIDTH / 2 - 10, (cash < 0 ? u.cash >= cash * -1 : p.cash >= cash) ? c_green : c_red,
                "%s $%d", (cash >= 0 ? "Profit" : "Cost"), abs(cash));
            if (!deal.empty()) mvwaddstrz(w_head.get(), header_height - 1, SCREEN_WIDTH / 2 + 5, (cost < 0 ? c_ltred : c_ltgreen), deal.c_str());
            wattron((focus_them ? w_them : w_you).get(), c_yellow);
            wborder(w_them.get(), LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX);
            wborder(w_you.get(), LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX);
            wattroff(w_them.get(), c_yellow);
            wattroff(w_you.get(), c_yellow);
            mvwprintz(w_them.get(), 0, 1, (cash < 0 || p.cash >= cash ? c_green : c_red), "%s: $%d", p.name.c_str(), p.cash);
            mvwprintz(w_you.get(), 0, 2, (cash > 0 || u.cash >= cash * -1 ? c_green : c_red), "You: $%d", u.cash);
            // Draw their list of items, starting from them_off
            for (int i = them_off; i + them_off < theirs.size() && i < v_hgt; i++)
                mvwprintz(w_them.get(), i - them_off + 1, 1,
                    (getting_theirs[i] ? c_white : c_ltgray), "%c %c %s - $%d",
                    char(i + 'a'), (getting_theirs[i] ? '+' : '-'),
                    p.inv[theirs[i + them_off]].tname().substr(0, 25).c_str(),
                    their_price[i + them_off]);
            if (them_off > 0) mvwprintw(w_them.get(), v_hgt + 2, 1, "< Back");
            if (them_off + v_hgt < theirs.size()) mvwprintw(w_them.get(), v_hgt + 2, 9, "More >");
            // Draw your list of items, starting from you_off
            for (int i = you_off; i + you_off < yours.size() && i < v_hgt; i++)
                mvwprintz(w_you.get(), i - you_off + 1, 1,
                    (getting_yours[i] ? c_white : c_ltgray), "%c %c %s - $%d",
                    char(i + 'a'), (getting_yours[i] ? '+' : '-'),
                    u.inv[yours[i + you_off]].tname().substr(0, 25).c_str(),
                    your_price[i + you_off]);
            if (you_off > 0) mvwprintw(w_you.get(), v_hgt + 2, 1, "< Back");
            if (you_off + v_hgt < yours.size()) mvwprintw(w_you.get(), v_hgt + 2, 9, "More >");
            wrefresh(w_head.get());
            wrefresh(w_them.get());
            wrefresh(w_you.get());
        }	// Done updating the screen
        ch = getch();
        switch (ch) {
        case '\t':
            focus_them = !focus_them;
            update = true;
            break;
        case '<':
            if (focus_them) {
                if (them_off > 0) {
                    them_off -= v_hgt;
                    update = true;
                }
            }
            else {
                if (you_off > 0) {
                    you_off -= v_hgt;
                    update = true;
                }
            }
            break;
        case '>':
            if (focus_them) {
                if (them_off + v_hgt < theirs.size()) {
                    them_off += v_hgt;
                    update = true;
                }
            }
            else {
                if (you_off + v_hgt < yours.size()) {
                    you_off += v_hgt;
                    update = true;
                }
            }
            break;
        case '?':
            update = true;
            {
                const size_t prompt_len = sizeof("Examine which item?") - 1; // C++20: use constexpr strlen
                std::unique_ptr<WINDOW, curses_full_delete> w_tmp(newwin(3, prompt_len + 2, 1, (SCREEN_WIDTH - prompt_len) / 2));
                mvwaddstrz(w_tmp.get(), 1, 1, c_red, "Examine which item?");
                wborder(w_tmp.get(), LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                    LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX);
                wrefresh(w_tmp.get());
                help = getch();
                help -= 'a';
            }
            wrefresh(w_head.get());
            if (focus_them) {
                if (help >= 0 && them_off + help < theirs.size())
                    popup(p.inv[theirs[them_off + help]].info().c_str());
            }
            else {
                if (help >= 0 && you_off + help < yours.size())
                    popup(u.inv[yours[you_off + help]].info().c_str());
            }
            break;
        case '\n':	// Check if we have enough cash...
            // \todo major rework...how long pre-Cataclysm money is "valid" is very arguable
            if (cash < 0 && u.cash < cash * -1) {
                popup("Not enough cash!  You have $%d, price is $%d.", u.cash, cash);
                update = true;
                ch = ' ';
            }
            else if (cash > 0 && p.cash < cash)
                p.op_of_u.owed += cash;
            break;
        default:	// Letters & such
            if (ch >= 'a' && ch <= 'z') {
                ch -= 'a';
                if (focus_them) {
                    if (ch < theirs.size()) {
                        getting_theirs[ch] = !getting_theirs[ch];
                        if (getting_theirs[ch])
                            cash -= their_price[ch];
                        else
                            cash += their_price[ch];
                        update = true;
                    }
                }
                else {	// Focus is on the player's inventory
                    if (ch < yours.size()) {
                        getting_yours[ch] = !getting_yours[ch];
                        if (getting_yours[ch])
                            cash += your_price[ch];
                        else
                            cash -= your_price[ch];
                        update = true;
                    }
                }
                ch = 0;
            }
        }
    } while (ch != KEY_ESCAPE && ch != '\n');

    if (ch == '\n') {
        inventory newinv;
        int practice = 0;
        {
            std::vector<char> removing;
            for (int i = 0; i < yours.size(); i++) {
                if (getting_yours[i]) {
                    const item& it = u.inv[yours[i]];
                    newinv.push_back(it);
                    removing.push_back(it.invlet);
                    practice++;
                }
            }
            // Do it in two passes, so removing items doesn't corrupt yours[]
            for (char invlet : removing) u.i_rem(invlet);
        } // scope out removing to force its destructor to run

        for (int i = 0; i < theirs.size(); i++) {
            item tmp = p.inv[theirs[i]];
            if (getting_theirs[i]) {
                practice += 2;
                tmp.invlet = 'a';
                while (u.has_in_inventory(tmp.invlet)) {
                    tmp.invlet = pc::inc_invlet(tmp.invlet);
                    if ('Z' == tmp.invlet) return false; // \todo Do something else with these.
                }
                u.inv.push_back(std::move(tmp));
            }
            else
                newinv.push_back(std::move(tmp));
        }
        u.practice(sk_barter, practice / 2);
        p.inv = std::move(newinv);
        u.cash += cash;
        p.cash -= cash;
    }
    return '\n' == ch;
}

void talk_function::assign_mission(game *g, npc& p)
{
    int selected = p.chatbin.mission_selected;
    if (0 > selected || selected >= p.chatbin.missions.size())
        throw std::string("mission_selected = " + std::to_string(selected) + "; missions.size() = " + std::to_string(p.chatbin.missions.size()) + "!");
    if (auto miss = mission::from_id(p.chatbin.missions[selected])) {
        g->u.accept(miss);
        miss->npc_id = p.ID();
        p.chatbin.missions_assigned.push_back(p.chatbin.missions[selected]);
        EraseAt(p.chatbin.missions, selected);
    } else {
        EraseAt(p.chatbin.missions, selected);
        throw std::string("mission was AWOL");
    }
}

void talk_function::mission_success(game *g, npc& p)
{
    int selected = p.chatbin.mission_selected;
    if (0 > selected || selected >= p.chatbin.missions.size())
        throw std::string("mission_selected = " + std::to_string(selected) + "; missions.size() = " + std::to_string(p.chatbin.missions.size()) + "!");

    if (auto miss = mission::from_id(p.chatbin.missions_assigned[selected])) {
        p.op_of_u += npc_opinion(0, 0, 1 + (miss->value / 1000), -1, miss->value);
        g->u.wrap_up(miss);
    } else {
        EraseAt(p.chatbin.missions, selected);
        throw std::string("mission was AWOL");
    }
}

void talk_function::mission_failure(game *g, npc& p)
{
    int selected = p.chatbin.mission_selected;
    if (0 > selected || selected >= p.chatbin.missions.size())
        throw std::string("mission_selected = " + std::to_string(selected) + "; missions.size() = " + std::to_string(p.chatbin.missions.size()) + "!");
    p.op_of_u += npc_opinion(-1, 0, -1, 1, 0);
}

void talk_function::clear_mission(game *g, npc& p)
{
    int selected = p.chatbin.mission_selected;
    if (0 > selected || selected >= p.chatbin.missions.size())
        throw std::string("mission_selected = " + std::to_string(selected) + "; missions.size() = " + std::to_string(p.chatbin.missions.size()) + "!");

    p.chatbin.mission_selected = -1;

    if (auto miss = mission::from_id(p.chatbin.missions_assigned[selected])) {
        EraseAt(p.chatbin.missions_assigned, selected);
        if (miss->follow_up != MISSION_NULL) p.chatbin.missions.push_back(g->reserve_mission(miss->follow_up, p.ID()));
    } else {
        EraseAt(p.chatbin.missions, selected);
        throw std::string("mission was AWOL");
    }
}

void talk_function::mission_reward(game *g, npc& p)
{
 int trade_amount = p.op_of_u.owed;
 p.op_of_u.owed = 0;
 trade(g->u, p, trade_amount, "Reward");
}

void talk_function::start_trade(game *g, npc& p)
{
 int trade_amount = p.op_of_u.owed;
 p.op_of_u.owed = 0;
 trade(g->u, p, trade_amount, "Trade");
}
 
void talk_function::give_equipment(game *g, npc& p)
{
 std::vector<int> giving;
 std::vector<int> prices;
 p.init_selling(giving, prices);
 int chosen = -1;
 if (giving.empty()) {
  for (size_t i = 0; i < p.inv.size(); i++) {
   int val = p.value(p.inv[i]) - p.inv[i].price() / 50;
   giving.push_back(i);
   prices.push_back(p.inv[i].price());
  }
 }
 while (chosen == -1 && giving.size() > 1) {
  int index = rng(0, giving.size() - 1);
  if (prices[index] < p.op_of_u.owed)
   chosen = index;
  giving.erase(giving.begin() + index);
  prices.erase(prices.begin() + index);
 }
 if (giving.empty()) {
  popup("%s has nothing to give!", p.name.c_str());
  return;
 }
 if (chosen == -1) chosen = 0;
 int item_index = giving[chosen];
 popup("%s gives you a %s.", p.name.c_str(), p.inv[item_index].tname().c_str());
 g->u.i_add( p.i_remn(item_index) );
 p.op_of_u.owed -= prices[chosen];
 p.add_disease(DI_ASKED_FOR_ITEM, HOURS(3));
}

void talk_function::follow(game *g, npc& p) { p.attitude = NPCATT_FOLLOW; }
void talk_function::deny_follow(game *g, npc& p) { p.add_disease(DI_ASKED_TO_FOLLOW, HOURS(6)); }
void talk_function::deny_lead(game *g, npc& p) { p.add_disease(DI_ASKED_TO_LEAD, HOURS(6)); }
void talk_function::deny_equipment(game *g, npc& p) { p.add_disease(DI_ASKED_FOR_ITEM, HOURS(1)); }

void talk_function::hostile(game *g, npc& p)
{
 messages.add("%s turns hostile!", p.name.c_str());
 p.attitude = NPCATT_KILL;
}

void talk_function::flee(game *g, npc& p)
{
 messages.add("%s turns to flee!", p.name.c_str());
 p.attitude = NPCATT_FLEE;
}

void talk_function::leave(game *g, npc& p)
{
 messages.add("%s leaves.", p.name.c_str());
 p.attitude = NPCATT_NULL;
}

void talk_function::start_mugging(game *g, npc& p)
{
 p.attitude = NPCATT_MUG;
 messages.add("Pause to stay still.  Any movement may cause %s to attack.", p.name.c_str());
}

void talk_function::player_leaving(game *g, npc& p)
{
 p.attitude = NPCATT_WAIT_FOR_LEAVE;
 p.patience = 15 - p.personality.aggression;
}

void talk_function::drop_weapon(game *g, npc& p) { p.GPSpos.add(p.unwield()); }
void talk_function::player_weapon_away(game *g, npc& p) { g->u.i_add(g->u.unwield()); }
void talk_function::player_weapon_drop(game *g, npc& p) { g->u.GPSpos.add(g->u.unwield()); }

void talk_function::lead_to_safety(game *g, npc& p)
{
 auto& miss = g->give_mission(MISSION_REACH_SAFETY);
 p.goal = miss.target;
 p.attitude = NPCATT_LEAD;
}
 
void talk_function::toggle_use_guns(game *g, npc& p) { p.combat_rules.use_guns = !p.combat_rules.use_guns; }
void talk_function::toggle_use_grenades(game *g, npc& p) { p.combat_rules.use_grenades = !p.combat_rules.use_grenades; }
void talk_function::set_engagement_none(game *g, npc& p) { p.combat_rules.engagement = ENGAGE_NONE; }
void talk_function::set_engagement_close(game *g, npc& p) { p.combat_rules.engagement = ENGAGE_CLOSE; }
void talk_function::set_engagement_weak(game *g, npc& p) { p.combat_rules.engagement = ENGAGE_WEAK; }
void talk_function::set_engagement_hit(game *g, npc& p) { p.combat_rules.engagement = ENGAGE_HIT; }
void talk_function::set_engagement_all(game *g, npc& p) { p.combat_rules.engagement = ENGAGE_ALL; }

void talk_function::start_training(game *g, npc& p)
{
 int cost = 0, time = 0;
 skill sk_used = sk_null;
 itype_id style = itm_null;
 if (p.chatbin.tempvalue < 0) {
  cost = -800;
  style = itype_id(0 - p.chatbin.tempvalue);
  time = 30000;
 } else {
  sk_used = skill(p.chatbin.tempvalue);
  cost = -200 * (1 + g->u.sklevel[sk_used]);
  time = 10000 + 5000 * g->u.sklevel[sk_used];
 }

// Pay for it
 if (p.op_of_u.owed >= 0 - cost)
  p.op_of_u.owed += cost;
 else if (!trade(g->u, p, cost, "Pay for training:"))
  return;
// Then receive it
 g->u.assign_activity(ACT_TRAIN, time, p.chatbin.tempvalue);
}

void parse_tags(std::string &phrase, player &u, npc &me)
{
 size_t fa, fb;
 std::string tag;
 do {
  fa = phrase.find("<");
  fb = phrase.find(">");
  int l = fb - fa + 1;
  if (fa != std::string::npos && fb != std::string::npos)
   tag = phrase.substr(fa, fb - fa + 1);
  else
   tag = "";
  bool replaced = false;
  for (int i = 0; i < NUM_STATIC_TAGS && !replaced; i++) {
   if (tag == talk_tags[i].tag) {
    phrase.replace(fa, l, (*talk_tags[i].replacement)[rng(0, 9)]);
    replaced = true;
   }
  }
  if (!replaced) { // Special, dynamic tags go here
   if (tag == "<yrwp>")
    phrase.replace(fa, l, u.weapon.tname());
   else if (tag == "<mywp>") {
    if (me.weapon.type->id == 0)
     phrase.replace(fa, l, "fists");
    else
     phrase.replace(fa, l, me.weapon.tname());
   } else if (tag == "<ammo>") {
	phrase.replace(fa, l,(!me.weapon.is_gun() ? "BADAMMO" : ammo_name(dynamic_cast<const it_gun*>(me.weapon.type)->ammo)));
   } else if (tag == "<punc>") {
    switch (rng(0, 2)) {
     case 0: phrase.replace(fa, l, ".");   break;
     case 1: phrase.replace(fa, l, "..."); break;
     case 2: phrase.replace(fa, l, "!");   break;
    }
   } else if (tag != "") {
    debugmsg("Bad tag. '%s' (%d - %d)", tag.c_str(), fa, fb);
    phrase.replace(fa, fb - fa + 1, "????");
   }
  }
 } while (fa != std::string::npos && fb != std::string::npos);
}

 
talk_topic dialogue::opt(talk_topic topic, game *g)
{
 static std::string talk_trial_text[NUM_TALK_TRIALS] = { "", "LIE", "PERSUADE", "INTIMIDATE" };	// constructor disables the operator<<(void*) overload

 std::string challenge;
 try {
   challenge = dynamic_line(topic, g, beta);
 } catch (const std::string& e) {	// yes, non-standard choice.
   debugmsg(e.c_str());
   return TALK_NONE;
 }
 std::vector<talk_response> responses = gen_responses(topic, g, beta);
#if NO_OP
// Put quotes around challenge (unless it's an action)
 if (challenge[0] != '*' && challenge[0] != '&') {
  std::ostringstream tmp;
  tmp << "\"" << challenge << "\"";
 }
#endif
// Parse any tags in challenge
 parse_tags(challenge, alpha, beta);
 if (challenge[0] >= 'a' && challenge[0] <= 'z') challenge[0] += 'A' - 'a';
// Prepend "My Name: "
 if (challenge[0] == '&') challenge = challenge.substr(1); // No name prepended!
 else challenge = beta.name + (challenge[0] == '*' ? " " + challenge.substr(1)	// action
	                                                : ": " + challenge);	// speech
 history.push_back(""); // Empty line between lines of dialogue
 
// Number of lines to highlight
#ifdef MID_WIDTH
#undef MID_WIDTH
#endif
 const int MID_WIDTH = SCREEN_WIDTH/2;	// historically, 40
 int hilight_lines = 1;
 size_t split;
 while (challenge.length() > MID_WIDTH) {
  hilight_lines++;
  split = challenge.find_last_of(' ', MID_WIDTH);
  history.push_back(challenge.substr(0, split));
  challenge = challenge.substr(split);
 }
 history.push_back(challenge);

 std::vector<std::string> options;
 std::vector<nc_color>    colors;
 for (int i = 0; i < responses.size(); i++) {
  std::ostringstream text;
  text << char('a' + i) << ": ";
  if (responses[i].trial != TALK_TRIAL_NONE)
   text << "[" << talk_trial_text[responses[i].trial] << " " << trial_chance(responses[i], alpha, beta) << "%%] ";	// heading to vsprint_s
  text << responses[i].text;
  options.push_back(text.str());
  parse_tags(options.back(), alpha, beta);
  if (responses[i].text[0] == '!')
   colors.push_back(c_red);
  else if (responses[i].text[0] == '*')
   colors.push_back(c_ltred);
  else if (responses[i].text[0] == '&')
   colors.push_back(c_green);
  else
   colors.push_back(c_white);
 }
  
 for (int i = 2; i < VIEW-1; i++) {
  for (int j = 1; j < SCREEN_WIDTH-1; j++) {
   if (j != MID_WIDTH+1) mvwputch(win, i, j, c_black, ' ');
  }
 }

 int curline = VIEW - 2, curhist = 1;
 while (curhist <= history.size() && curline > 0) {
  mvwaddstrz(win, curline, 1, ((curhist <= hilight_lines) ? c_red : c_dkgray), history[history.size() - curhist].c_str());
  curline--;
  curhist++;
 }

 curline = 3;
 for (int i = 0; i < options.size(); i++) {
  while (options[i].size() > MID_WIDTH - 4) {
   split = options[i].find_last_of(' ', MID_WIDTH - 4);
   mvwaddstrz(win, curline, MID_WIDTH + 2, colors[i], options[i].substr(0, split).c_str());
   options[i] = "  " + options[i].substr(split);
   curline++;
  }
  mvwaddstrz(win, curline, MID_WIDTH + 2, colors[i], options[i].c_str());
  curline++;
 }
 mvwaddstrz(win, curline + 2, MID_WIDTH + 2, c_magenta, "L: Look at");
 mvwaddstrz(win, curline + 3, MID_WIDTH + 2, c_magenta, "S: Size up stats");

 wrefresh(win);

 int ch;
 bool okay;
 do {
  do {
   ch = getch();
   if (special_talk(ch) == TALK_NONE)
    ch -= 'a';
  } while (special_talk(ch) == TALK_NONE && (ch < 0 || ch >= options.size()));
  okay = false;
  if (special_talk(ch) != TALK_NONE)
   okay = true;
  else if (colors[ch] == c_white || colors[ch] == c_green)
   okay = true;
  else if (colors[ch] == c_red && query_yn("You may be attacked! Proceed?"))
   okay = true;
  else if (colors[ch] == c_ltred && query_yn("You'll be helpless! Proceed?"))
   okay = true;
 } while (!okay);
 history.push_back("");

 if (special_talk(ch) != TALK_NONE) return special_talk(ch);

 std::string response_printed = "You: " + responses[ch].text;
 while (response_printed.length() > MID_WIDTH) {
  hilight_lines++;
  split = response_printed.find_last_of(' ', MID_WIDTH);
  history.push_back(response_printed.substr(0, split));
  response_printed = response_printed.substr(split);
 }
 history.push_back(response_printed);

 talk_response chosen = responses[ch];
 if (chosen.mission_index != -1) beta.chatbin.mission_selected = chosen.mission_index;
 if (chosen.tempvalue != -1) beta.chatbin.tempvalue = chosen.tempvalue;

 if (chosen.trial == TALK_TRIAL_NONE || rng(0, 99) < trial_chance(chosen, alpha, beta)) {
  try {
      (chosen.effect_success)(g, beta);
  } catch (const std::string & e) {	// yes, non-standard choice.
      debugmsg(e.c_str());
      return TALK_NONE;
  }

  if (chosen.trial != TALK_TRIAL_NONE) alpha.practice(sk_speech, (100 - trial_chance(chosen, alpha, beta)) / 10);
  beta.op_of_u += chosen.opinion_success;
  if (beta.turned_hostile()) {
   beta.make_angry();
   done = true;
  }
  return chosen.success;
 } else {
  try {
      (chosen.effect_failure)(g, beta);
  } catch (const std::string & e) {	// yes, non-standard choice.
      debugmsg(e.c_str());
      return TALK_NONE;
  }
  alpha.practice(sk_speech, (100 - trial_chance(chosen, alpha, beta)) / 7);
  beta.op_of_u += chosen.opinion_failure;
  if (beta.turned_hostile()) {
   beta.make_angry();
   done = true;
  }
  return chosen.failure;
 }
 return TALK_NONE; // Shouldn't ever happen
}

talk_topic special_talk(char ch)
{
 switch (ch) {
  case 'L':
  case 'l':
   return TALK_LOOK_AT;
  case 'S':
  case 's':
   return TALK_SIZE_UP;
  case 'O':
  case 'o':
   return TALK_OPINION;
  default:
   return TALK_NONE;
 }
 return TALK_NONE;
}
