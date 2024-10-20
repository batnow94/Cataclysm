#include "mutation.h"
#include <string.h>

mutation_branch mutation_branch::data[PF_MAX2]; // Mutation data

const trait mutation_branch::traits[PF_MAX2] = {
	{ "NULL trait!", 0, 0, 0, "This is a bug.  Weird." },
{ "Fleet-Footed", 3, 0, 0, "\
You can run more quickly than most, resulting in a 15% speed bonus on sure\n\
footing." },
{ "Parkour Expert", 2, 0, 0, "\
You're skilled at clearing obstacles; terrain like railings or counters are\n\
as easy for you to move on as solid ground." },
{ "Quick", 3, 0, 0, "\
You're just generally quick!  You get a 10% bonus to action points." },
{ "Optimist", 2, 0, 0, "\
Nothing gets you down!  Your morale will generally be higher than average." },
{ "Fast Healer", 2, 0, 0, "\
You heal a little faster than most; sleeping will heal more lost HP." },
{ "Light Eater", 3, 0, 0, "\
Your metabolism is a little slower, and you require less food than most." },
{ "Pain Resistant", 2, 0, 0, "\
You have a high tolerance for pain." },
{ "Night Vision", 1, 0, 0, "\
You possess natural night vision, and can see two squares instead of one in\n\
pitch blackness." },
{ "Poison Resistant", 1, 0, 0, "\
Your system is rather tolerant of poisons and toxins, and most will affect\n\
you less." },
{ "Fast Reader", 1, 0, 0, "\
You're a quick reader, and can get through books a lot faster than most." },
{ "Tough", 5, 0, 0, "\
It takes a lot to bring you down!  You get a 20% bonus to all hit points." },
{ "Thick-Skinned", 2, 0, 0, "\
Your skin is tough.  Cutting damage is slightly reduced for you." },
{ "Packmule", 3, 0, 0, "\
You can manage to find space for anything!  You can carry 40% more volume." },
{ "Fast Learner", 3, 0, 0, "\
Your skill comprehension is 50% higher, allowing you to learn skills much\n\
faster than others.  Note that this only applies to real-world experience,\n\
not to skill gain from other sources like books." },
{ "Deft", 2, 0, 0, "\
While you're not any better at melee combat, you are better at recovering\n\
from a miss, and will be able to attempt another strike faster." },
{ "Drunken Master", 2, 0, 0, "\
The martial art technique of Zui Quan, or Drunken Fist, comes naturally to\n\
you.  While under the influence of alcohol, your melee skill will rise\n\
considerably, especially unarmed combat." },
{ "Gourmand", 2, 0, 0, "\
You eat faster, and can eat and drink more, than anyone else!  You also enjoy\n\
food more; delicious food is better for your morale, and you don't mind some\n\
unsavory meals." },
{ "Animal Empathy", 1, 0, 0, "\
Peaceful animals will not run away from you, and even aggressive animals are\n\
less likely to attack.  This only applies to natural animals such as woodland\n\
creatures." },
{ "Terrifying", 2, 0, 0, "\
There's something about you that creatures find frightening, and they are\n\
more likely to try to flee." },
{ "Disease Resistant", 1, 0, 0, "\
It's very unlikely that you will catch ambient diseases like a cold or the\n\
flu." },
{ "High Adrenaline", 3, 0, 0, "\
If you are in a very dangerous situation, you may experience a temporary rush\n\
which increases your speed and strength significantly." },
{ "Inconspicuous", 2, 0, 0, "\
While sleeping or staying still, it is less likely that monsters will wander\n\
close to you." },
{ "Masochist", 2, 0, 0, "\
Although you still suffer the negative effects of pain, it also brings a\n\
unique pleasure to you." },
{ "Light Step", 1, 0, 0, "\
You make less noise while walking.  You're also less likely to set off traps." },
{ "Heartless", 2, 0, 0, "\
You have few qualms, and no capacity for pity. Killing the helpless, the\n\
young, and your friends will not affect your morale at all." },
{ "Android", 4, 0, 0, "\
At some point in the past you had a bionic upgrade installed in your body.\n\
You start the game with a power system, and one random bionic enhancement." },
{ "Robust Genetics", 2, 0, 0, "\
You have a very strong genetic base.  If you mutate, the odds that the\n\
mutation will be beneficial are greatly increased." },
{ "Martial Arts Training", 3, 0, 0, "\
You have receives some martial arts training at a local dojo.  You will start\n\
with your choice of karate, judo, aikido, tai chi, or taekwando." },

{ "NULL", 0, 0, 0, " -------------------------------------------------- " },

{ "Near-Sighted", -2, 0, 0, "\
Without your glasses, your seeing radius is severely reduced!  However, while\n\
wearing glasses this trait has no effect, and you are guaranteed to start\n\
with a pair." },
{ "Heavy Sleeper", -1, 0, 0, "\
You're quite the heavy sleeper.  Noises are unlikely to wake you up." },
{ "Asthmatic", -4, 0, 0, "\
You will occasionally need to use an inhaler, or else suffer severe physical\n\
limitations.  However, you are guaranteed to start with an inhaler." },
{ "Bad Back", -3, 0, 0, "\
You simply can not carry as much as people with a similar strength could.\n\
Your maximum weight carried is reduced by 35%." },
{ "Illiterate", -5, 0, 0, "\
You never learned to read!  Books and computers are off-limits to you." },
{ "Poor Hearing", -2, 0, 0, "\
Your hearing is poor, and you may not hear quiet or far-off noises." },
{ "Insomniac", -2, 0, 0, "\
You have a hard time falling asleep, even under the best circumstances!" },
{ "Vegetarian", -3, 0, 0, "\
You have moral objections to eating meat.  You may consume it, but doing so\n\
will hurt your morale." },
{ "Glass Jaw", -3, 0, 0, "\
Your head can't take much abuse.  Its maximum HP is 15% lower than usual." },
{ "Forgetful", -3, 0, 0, "\
You have a hard time remembering things.  Your skills will erode slightly\n\
faster than usual." },
{ "Lightweight", -1, 0, 0, "\
Alcohol and drugs go straight to your head.  You suffer the negative effects\n\
of these for longer." },
{ "Addictive Personality", -3, 0, 0, "\
It's easier for you to become addicted to substances, and harder to rid\n\
yourself of these addictions." },
{ "Trigger Happy", -2, 0, 0, "\
On rare occasion, you will go full-auto when you intended to fire a single\n\
shot.  This has no effect when firing handguns or other semi-automatic\n\
firearms." },
{ "Smelly", -1, 0, 0, "\
Your scent is particularly strong.  It's not offensive to humans, but animals\n\
that track your scent will do so more easily." },
{ "Chemical Imbalance", -2, 0, 0, "\
You suffer from a minor chemical imbalance, whether mental or physical. Minor\n\
changes to your internal chemistry will manifest themselves on occasion,\n\
such as hunger, sleepiness, narcotic effects, etc." },
{ "Schizophrenic", -5, 0, 0, "\
You will periodically suffer from delusions, ranging from minor effects to\n\
full visual hallucinations.  Some of these effects may be controlled through\n\
the use of Thorazine." },
{ "Jittery", -3, 0, 0, "\
During moments of great stress or under the effects of stimulants, you may\n\
find your hands shaking uncontrollably, severely reducing your dexterity." },
{ "Hoarder", -4, 0, 0, "\
You don't feel right unless you're carrying as much as you can.  You suffer\n\
morale penalties for carrying less than maximum volume (weight is ignored).\n\
Xanax can help control this anxiety." },
{ "Savant", -4, 0, 0, "\
You tend to specialize in one skill and be poor at all others.  You advance\n\
at half speed in all skills except your best one. Note that combining this\n\
with Fast Learner will come out to a slower rate of learning for all skills." },
{ "Mood Swings", -1, 0, 0, "\
Your morale will shift up and down at random, often dramatically." },
{ "Weak Stomach", -1, 0, 0, "\
You are more likely to throw up from food poisoning, alcohol, etc." },
{ "Wool Allergy", -1, 0, 0, "\
You are badly allergic to wool, and can not wear any clothing made of the\n\
substance." },
{ "HP Ignorant", -2, 0, 0, "\
You do not get to see your exact amount of HP remaining, but do have a vague\n\
idea of whether you're in good condition or not." },
{ "Truth Teller", -2, 0, 0, "\
When you try to tell a lie, you blush, stammer, and get all shifty-eyed.\n\
Telling lies and otherwise bluffing will be much more difficult for you." },
{ "Ugly", -1, 0, 2, "\
You're not much to look at.  NPCs who care about such things will react\n\
poorly to you." },

{ "Bug - PF_MAX", 0, 0, 0, "\
This shouldn't be here!  You have the trait PF_MAX toggled.  Weird." },

/* From here down are mutations.
* In addition to a points value, mutations have a visibility value and an
*  ugliness value.
* Positive visibility means that the mutation is prominent.  This will visibly
*  identify the player as a mutant, resulting in discrimination from mutant-
*  haters and trust with mutants/mutant-lovers.
* Positive ugliness means that the mutation is grotesque.  This will result in
*  a negative reaction from NPCs, even those who are themselves mutated, unless
*  the NPC is a mutant-lover.
*/

{ "Rough Skin", 0, 2, 1, "\
Your skin is slightly rough.  This has no gameplay effect." },
{ "High Night Vision", 3, 0, 0, "\
You can see incredibly well in the dark!" },
{ "Full Night Vision", 5, 0, 0, "\
You can see in pitch blackness as if you were wearing night-vision goggles." },
{ "Infrared Vision", 5, 0, 0, "\
Your eyes have mutated to pick up radiation in the infrared spectrum." },
{ "Very Fast Healer", 5, 0, 0, "\
Your flesh regenerates slowly, and you will regain HP even when not sleeping." },
{ "Regeneration", 10, 0, 0, "\
Your flesh regenerates from wounds incredibly quickly." },
{ "Fangs", 2, 2, 2, "\
Your teeth have grown into two-inch-long fangs, allowing you to make an extra\n\
attack when conditions favor it." },
{ "Nictitating Membrane", 1, 1, 2, "\
You have a second set of clear eyelids which lower while underwater, allowing\n\
you to see as though you were wearing goggles." },
{ "Gills", 3, 5, 3, "\
You've grown a set of gills in your neck, allowing you to breathe underwater." },
{ "Scales", 6, 10, 3, "\
A set of flexible green scales have grown to cover your body, acting as a\n\
natural armor." },
{ "Thick Scales", 6, 10, 4, "\
A set of heavy green scales have grown to cover your body, acting as a\n\
natural armor.  It is very difficult to penetrate, but also limits your\n\
flexibility, resulting in a -2 penalty to Dexterity." },
{ "Sleek Scales", 6, 10, 4, "\
A set of very flexible and slick scales have grown to cover your body.  These\n\
act as a weak set of armor, improve your ability to swim, and make you\n\
difficult to grab." },
{ "Light Bones", 2, 0, 0, std::string("Your bones are very light.  This enables you to run and attack ") + std::to_string(100 - rational(100) * mutation_branch::light_bones_attack_tus[PF_LIGHT_BONES - PF_LIGHT_BONES]) +
"% faster,\n\
but also reduces your carrying weight by " + std::to_string(rational(100) * mutation_branch::light_bones_carrying[PF_LIGHT_BONES - PF_LIGHT_BONES]) +
"% and makes bashing attacks hurt\n\
a little more." },
{ "Hollow Bones", -6, 0, 0, std::string("You have Avian Bone Syndrome--your bones are nearly hollow.  Your body is\n\
very light as a result, enabling you to run and attack ") + std::to_string(100 - rational(100) * mutation_branch::light_bones_attack_tus[PF_HOLLOW_BONES - PF_LIGHT_BONES]) +
"% faster, but\n\
also frail; you can carry " + std::to_string(rational(100) * mutation_branch::light_bones_carrying[PF_HOLLOW_BONES - PF_LIGHT_BONES]) +
"% less, and bashing attacks injure you more." },
{ "Feathers", 2, 10, 3, "\
Iridescent feathers have grown to cover your entire body, providing a\n\
marginal protection against attacks and minor protection from cold. They\n\
also provide a natural waterproofing." },
{ "Lightly Furred", 1, 6, 2, "\
Light fur has grown to coveryour entire body, providing slight protection\n\
from cold." },
{ "Furry", 2, 10, 3, "\
Thick black fur has grown to cover your entire body, providing a marginal\n\
protection against attacks, and considerable protection from cold." },
{ "Chitinous Skin", 2, 3, 2, "\
Your epidermis has turned into a thin, flexible layer of chitin.  It provides\n\
minor protection from cutting wounds." },
{ "Chitinous Armor", 2, 6, 3, "\
You've grown a chitin exoskeleton, much like that of an insect.  It provides\n\
considerable physical protection, but reduces your dexterity by 1." },
{ "Chitinous Plate", 2, 8, 5, "\
You've grown a chitin exoskeleton made of thick, stiff plates, like that of\n\
a beetle.  It provides excellent physical protection, but reduces your\n\
dexterity by 1 and encumbers all body parts but your eyes and mouth." },
{ "Spines", 1, 0, 0, "\
Your skin is covered with fine spines.  Whenever an unarmed opponent strikes\n\
a part of your body that is not covered by clothing, they will receive\n\
moderate damage." },
{ "Quills", 3, 0, 0, "\
Your body is covered with large quills.  Whenever an unarmed opponent strikes\n\
a part of your body that is not covered by clothing, they will receive\n\
significant damage." },
{ "Phelloderm", 3, 3, 2, "\
Your skin is light green and has a slightly woody quality to it.  This\n\
provides a weak armor, and helps you retain moisture, resulting in less\n\
thirst." },
{ "Bark", 5, 10, 3, "\
Your skin is coated in a light bark, like that of a tree.  This provides\n\
resistance to bashing and cutting damage and minor protection from fire." },
{ "Thorns", 6, 8, 4, "\
Your skin is covered in small, woody thorns.  Whenever an unarmed opponent\n\
strikes a part of your body that is not covered by clothing, they will\n\
receive minor damage.  Your punches may also deal extra damage." },
{ "Leaves", 6, 8, 3, "\
All the hair on your body has turned to long, grass-like leaves.  Apart from\n\
being physically striking, these provide you with a minor amount of nutrition\n\
while in sunlight." },
{ "Long Fingernails", 1, 1, 0, "\
Your fingernails are long and sharp.  If you aren't wearing gloves, your\n\
unarmed attacks deal a minor amount of cutting damage." },
{ "Claws", 2, 3, 2, "\
You have claws on the ends of your fingers.  If you aren't wearing gloves,\n\
your unarmed attacks deal a minor amount of cutting damage." },
{ "Large Talons", 2, 4, 3, "\
Your index fingers have grown into huge talons.  After a bit of practice, you\n\
find that this does not affect your dexterity, but allows for a deadly\n\
unarmed attack.  They also prevent you from wearing gloves." },
{ "Radiogenic", 3, 0, 0, "\
Your system has adapted to radiation.  While irradiated, you will actually\n\
heal slowly, converting the radiation into hit points." },
{ "Marloss Carrier", 4, 0, 0, "\
Ever since you ate that Marloss berry, you can't get its scent out of your\n\
nose, and you have a strong desire to eat more." },
{ "Insect Pheromones", 8, 0, 0, "\
Your body produces low-level pheromones, identifying you as a friend to many\n\
species of insects.  Insects will attack you much less." },
{ "Mammal Pheromones", 8, 0, 0, "\
Your body produces low-level pheromones which puts mammals at ease.  They\n\
will be less likely to attack or flee from you." },
{ "Disease Immune", 6, 0, 0, "\
Your body is simply immune to diseases.  You will never catch an ambient\n\
disease." },
{ "Poisonous", 8, 0, 0, "\
Your body produces a potent venom.  Any special attacks from mutations\n\
have a chance to poison your target." },
{ "Slime Hands", 4, 5, 4, "\
The skin on your hands is a mucous membrane and produces a thick, acrid\n\
slime.  Attacks using your hand will cause minor acid damage." },
{ "Compound Eyes", 2, 9, 5, "\
Your eyes are compound, like those of an insect.  This increases your\n\
perception by 2 so long as you aren't wearing eyewear." },
{ "Padded Feet", 1, 1, 0, "\
The bottoms of your feet are strongly padded.  You receive no movement\n\
penalty for not wearing shoes, and even receive a 10% bonus when running\n\
barefoot." },
{ "Hooves", -4, 2, 2, "\
Your feet have fused into hooves.  This allows kicking attacks to do much\n\
more damage, provides natural armor, and removes the need to wear shoes;\n\
however, you can not wear shoes of any kind." },
{ "Saprovore", 4, 0, 0, "\
Your digestive system is specialized to allow you to consume decaying\n\
material.  You can eat rotten food, albeit for less nutrition than\n\
usual." },
{ "Ruminant", 5, 0, 0, "\
Your digestive system is capable of digesting cellulose and other rough\n\
plant material.  You can eat underbrush by standing over it and pressing\n\
E." },
{ "Horns", 2, 3, 1, "\
You have a pair of small horns on your head.  They allow you to make a weak\n\
piercing headbutt attack." },
{ "Curled Horns", 1, 8, 2, "\
You have a pair of large curled horns, like those of a ram.  They allow you\n\
to make a strong bashing headbutt attack, but prevent you from wearing any\n\
headwear." },
{ "Pointed Horns", 2, 8, 2, "\
You have a pair of long, pointed horns, like those of an antelope.  They\n\
allow you to make a strong piercing headbutt attack, but prevent you from\n\
wearing any headwear the is not made of fabric." },
{ "Antennae", 1, 9, 4, "\
You have a pair of antennae.  They allow you to detect the presence of\n\
monsters up to a few tiles away, even if you can't see or hear them, but\n\
prevent you from wearing headwear that is not made of fabric." },
{ "Road-Runner", 4, 0, 0, "\
Your legs are extremely limber and fast-moving.  You run 30% faster on\n\
flat surfaces." },
{ "Stubby Tail", 0, 1, 2, "\
You have a short, stubby tail, like a rabbit's.  It serves no purpose." },
{ "Tail Fin", 1, 4, 2, "\
You have a fin-like tail.  It allows you to swim more quickly." },
{ "Long Tail", 2, 6, 2, "\
You have a long, graceful tail, like that of a big cat.  It improves your\n\
balance, making your ability to dodge higher." },
{ "Fluffy Tail", 2, 7, 0, "\
You have a long, fluffy-furred tail.  It greatly improves your balance,\n\
making your ability to dodge much higher." },
{ "Spiked Tail", 2, 6, 3, "\
You have a long tail that ends in a vicious stinger, like that of a\n\
scorpion.  It does not improve your balance at all, but allows for a\n\
powerful piercing attack." },
{ "Club Tail", 2, 7, 2, "\
You have a long tail that ends in a heavy, bony club.  It does not improve\n\
your balance at all, but allows for a powerful bashing attack." },
{ "Pain Recovery", 3, 0, 0, "\
You recover from pain slightly faster than normal." },
{ "Quick Pain Recovery", 5, 0, 0, "\
You recover from pain faster than normal." },
{ "Very Quick Pain Recovery", 8, 0, 0, "\
You recover from pain much faster than normal." },
{ "Bird Wings", 2, 4, 2, "\
You have a pair of large, feathered wings.  Your body is too heavy to be able\n\
to fly, but you can use them to slow your descent during a fall, and will not\n\
take falling damage under any circumstances." },
{ "Insect Wings", 3, 4, 4, "\
You have a pair of large, translucent wings.  You buzz them as you run,\n\
enabling you to run faster." },
{ "Mouth Tentacles", 1, 8, 5, "\
A set of tentacles surrounds your mouth.  They allow you to eat twice as\n\
fast." },
{ "Mandibles", 2, 8, 6, "\
A set of insect-like mandibles have grown around your mouth.  They allow you\n\
to eat faster and provide a slicing unarmed attack, but prevent you from\n\
wearing mouthwear." },
{ "Canine Ears", 2, 4, 1, "\
Your ears have extended into long, pointed ones, like those of a canine.\n\
They enhance your hearing, allowing you to hear at greater distances." },
{ "Web Walker", 3, 0, 0, "\
Your body excretes very fine amounts of a chemical which prevents you from\n\
sticking to webs.  Walking through webs does not affect you at all." },
{ "Web Weaver", 3, 0, 0, "\
Your body produces webs.  As you move, there is a chance that you will\n\
leave webs in your wake." },
{ "Whiskers", 1, 3, 1, "\
You have a set of prominent rodent-like whiskers around your mouth.  These\n\
make you more aware of vibrations in the air, and improve your ability to\n\
dodge very slightly." },
{ "Strong", 1, 0, 0, "\
Your muscles are a little stronger.  Strength + 1" },
{ "Very Strong", 2, 0, 0, "\
Your muscles are stronger.  Strength + 2" },
{ "Extremely Strong", 4, 1, 0, "\
Your muscles are much stronger.  Strength + 4" },
{ "Insanely Strong", 7, 2, 2, "\
Your muscles are noticeably bulging.  Strength + 7" },
{ "Dextrous", 1, 0, 0, "\
You are a little nimbler.  Dexterity + 1" },
{ "Very Dextrous", 2, 0, 0, "\
You are nimbler.  Dexterity + 2" },
{ "Extremely Dextrous", 3, 0, 0, "\
You are nimble and quick.  Dexterity + 4" },
{ "Insanely Dextrous", 4, 0, 0, "\
You are much nimbler than before.  Dexterity + 7" },
{ "Smart", 1, 0, 0, "\
You are a little smarter.  Intelligence + 1" },
{ "Very Smart", 2, 0, 0, "\
You are smarter.  Intelligence + 2" },
{ "Extremely Smart", 3, 1, 1, "\
You are much smarter, and your skull bulges slightly.  Intelligence + 4" },
{ "Insanely Smart", 4, 3, 3, "\
Your skull bulges noticeably with your impressive brain.  Intelligence + 7" },
{ "Perceptive", 1, 0, 0, "\
Your senses are a little keener.  Perception + 1" },
{ "Very Perceptive", 2, 0, 0, "\
Your senses are keener.  Perception + 2" },
{ "Extremely Perceptive", 3, 0, 0, "\
Your senses are much keener.  Perception + 4" },
{ "Insanely Perceptive", 4, 0, 0, "\
You can sense things you never imagined.  Perception + 7" },

{ "Head Bumps", 0, 3, 3, "\
You have a pair of bumps on your skull." },
{ "Antlers", -2, 10, 3, "\
You have a huge rack of antlers, like those of a moose.  They prevent you\n\
from hearing headwear that is not made of fabric, but provide a weak\n\
headbutt attack." },
{ "Slit Nostrils", -2, 7, 4, "\
You have a flattened nose and thin slits for nostrils, giving you a lizard-\n\
like appearance.  This makes breathing slightly difficult and increases\n\
mouth encumbrance by 1." },
{ "Forked Tongue", 0, 1, 3, "\
Your tongue is forked, like that of a reptile.  This has no effect." },
{ "Bulging Eyes", 0, 8, 4, "\
Your eyes bulge out several inches from your skull.  This does not affect\n\
your vision in any way." },
{ "Mouth Flaps", -1, 7, 6, "\
Skin tabs and odd flaps of skin surround your mouth.  They don't affect your\n\
eating, but are unpleasant to look at." },
{ "Wing Stubs", 0, 2, 2, "\
You have a pair of stubby little wings projecting from your shoulderblades.\n\
They can be wiggled at will, but are useless." },
{ "Bat Wings", -1, 9, 4, "\
You have a pair of large, leathery wings.  You can move them a little, but\n\
they are useless, and in fact put you off balance, reducing your ability to\n\
dodge slightly." },
{ "Pale Skin", 0, 3, 1, "\
Your skin is rather pale." },
{ "Spots", 0, 6, 2, "\
Your skin is covered in a pattern of red spots." },
{ "Very Smelly", -4, 4, 5, "\
You smell awful.  Monsters that track scent will find you very easily, and\n\
humans will react poorly." },
{ "Deformed", -2, 4, 4, "\
You're minorly deformed.  Some people will react badly to your appearance." },
{ "Badly Deformed", -4, 7, 7, "\
You're hideously deformed.  Some people will have a strong negative reaction\n\
to your appearance." },
{ "Grotesque", -7, 10, 10, "\
Your visage is disgusting and liable to induce vomiting.  People will not\n\
want to interact with you unless they have a very good reason to." },
{ "Nausea", -3, 0, 0, "\
You feel nauseous almost constantly, and are more liable to throw up from\n\
food poisoning, alcohol, etc." },
{ "Vomitous", -8, 0, 0, "\
You have a major digestive disorder, which causes you to vomit frequently." },
{ "Fast Metabolism", -2, 0, 0, "\
You require more food than most people." },
{ "High Thirst", -3, 0, 0, "\
Your body dries out easily; you need to drink a lot more water." },
{ "Weakening", -6, 0, 0, "\
You feel as though you are slowly weakening, but it's so slight a feeling\n\
that it does not affect you at all." },
{ "Deterioration", -8, 0, 0, "\
Your body is very slowly wasting away." },
{ "Disintegration", -10, 0, 0, "\
Your body is slowly wasting away!" },
{ "Albino", -2, 0, 0, "\
Your skin lacks pigment, and is nearly transparent.  You suffer serious burns\n\
in direct sunlight." },
{ "Sores", -2, 5, 6, "\
Your body is covered in painful sores.  The pain is worse when they are\n\
covered in clothing." },
{ "Light Sensitive", -2, 0, 0, "\
Sunlight makes you uncomfortable.  If you are outdoors and the weather is\n\
Sunny, you suffer -1 to all stats." },
{ "Very Light Sensitive", -3, 0, 0, "\
Sunlight makes you very uncomfortable.  If you are outdoors during the day,\n\
you suffer -1 to all stats; -2 if the weather is Sunny." },
{ "Troglobite", -5, 0, 0, "\
Sunlight makes you extremely uncomfortable, resulting in large penalties to\n\
all stats." },
{ "Webbed Hands", -1, 3, 2, "\
Your hands and feet are heavily webbed, reducing your dexterity by 1 and\n\
preventing you from wearing gloves.  However, you can swim much faster." },
{ "Beak", -1, 8, 4, "\
You have a beak for a mouth.  You can occasionally use it to peck at your\n\
enemies, but it is impossible for you to wear mouthgear." },
{ "Genetically Unstable", -4, 0, 0, "\
Your DNA has been damaged in a way that causes you to continually develop\n\
more mutations." },
{ "Minor Radioactivity", -4, 0, 0, "\
Your body has become radioactive!  You continuously emit low levels of\n\
radiation, some of which will be absorbed by you, and some of which will\n\
contaminate the world around you." },
{ "Radioactivity", -4, 0, 0, "\
Your body has become radioactive!  You continuously emit moderate levels of\n\
radiation, some of which will be absorbed by you, and some of which will\n\
contaminate the world around you." },
{ "Severe Radioactivity", -4, 0, 0, "\
Your body has become radioactive!  You continuously emit heavy levels of\n\
radiation, some of which will be absorbed by you, and some of which will\n\
contaminate the world around you." },
{ "Slimy", -1, 7, 6, "\
Your body is coated with a fine slime, which oozes off of you, leaving a\n\
trail." },
{ "Herbivore", -3, 0, 0, "\
Your body's ability to digest meat is severely hampered.  Eating meat has a\n\
good chance of making you vomit it back up; even if you manage to keep it\n\
down, its nutritional value is greatly reduced." },
{ "Carnivore", -3, 0, 0, "\
Your body's ability to digest fruits, vegetables and grains is severely\n\
hampered.  You cannot eat anything besides meat." },
{ "Ponderous", -3, 0, 0, "\
Your muscles are generally slow to move.  You run 10% slower." },
{ "Very Ponderous", -5, 0, 0, "\
Your muscles are quite slow to move.  You run 20% slower." },
{ "Extremely Ponderous", -8, 0, 0, "\
Your muscles are very slow to move.  You run 30% slower." },
{ "Sunlight dependent", -5, 0, 0, "\
You feel very sluggish when not in direct sunlight.  You suffer a 5% drop in\n\
speed when in shade, and a 10% drop in speed when in the dark." },
{ "Heat dependent", -2, 0, 0, "\
Your muscle response is dependent on ambient temperatures.  You lose 1% of\n\
your speed for every "+std::to_string(mutation_branch::cold_blooded_severity[0])+" degrees below 65 F." },
{ "Very Heat dependent", -3, 0, 0, "\
Your muscle response is highly dependent on ambient temperatures.  You lose\n\
1% of your speed for every " + std::to_string(mutation_branch::cold_blooded_severity[1]) + " degrees below 65 F." },
{ "Cold Blooded", -5, 0, 0, "\
You are cold-blooded and rely on heat to keep moving.  Your lose 1% of your\n\
speed for every " + std::to_string(mutation_branch::cold_blooded_severity[2]) + " degrees below 65 F." },
{ "Growling Voice", -1, 0, 0, "\
You have a growling, rough voice.  Persuading NPCs will be more difficult,\n\
but threatening them will be easier." },
{ "Snarling Voice", -2, 0, 0, "\
You have a threatening snarl in your voice.  Persuading NPCs will be near\n\
impossible, but threatening them will be much easier." },
{ "Shouter", -2, 0, 0, "\
You occasionally shout uncontrollably." },
{ "Screamer", -3, 0, 0, "\
You sometimes scream uncontrollably." },
{ "Howler", -5, 0, 0, "\
You frequently let out a piercing howl." },
{ "Tentacle Arms", -5, 7, 4, "\
Your arms have transformed into tentacles.  Though they are flexible and\n\
increase your dexterity by 1, the lack of fingers results in a permanent\n\
hand encumbrance of 3, and prevents the wearing of gloves." },
{ "4 Tentacles", -3, 8, 5, "\
Your arms have transformed into four tentacles, resulting in a bonus of 1 to\n\
dexterity, permanent hand encumbrance of 3, and preventing you from wearing\n\
gloves.  You can make up to 3 extra attacks with them." },
{ "8 Tentacles", -2, 9, 6, "\
Your arms have transformed into eight tentacles, resulting in a bonus of 1 to\n\
dexterity, permanent hand encumbrance of 3, and preventing you from wearing\n\
gloves.  You can make up to 7 extra attacks with them." },
{ "Shell", -6, 8, 3, "\
You have grown a thick shell over your torso, providing excellent armor.  You\n\
find you can use the empty space as 16 storage space, but cannot wear\n\
anything on your torso." },
{ "Leg Tentacles", -3, 8, 4, "\
Your legs have transformed into six tentacles.  This decreases your speed on\n\
land by 20%, but makes your movement silent.  However, they also increase\n\
your swimming speed." }
};

static const char* const JSON_transcode_mutation_categories[] = {
	"NULL",
	"LIZARD",
	"BIRD",
	"FISH",
	"BEAST",
	"CATTLE",
	"INSECT",
	"PLANT",
	"SLIME",
	"TROGLO",
	"CEPHALOPOD",
	"SPIDER",
	"RAT"
};

DEFINE_JSON_ENUM_SUPPORT_TYPICAL(mutation_category, JSON_transcode_mutation_categories)

#define MUTATION(mut) id = mut; data[id].valid = true

#define PREREQS(...) data[id].prereqs = { __VA_ARGS__ }
#define CANCELS(...) data[id].cancels = { __VA_ARGS__ }
#define CHANGES_TO(...) data[id].replacements = { __VA_ARGS__ }
#define LEADS_TO(...) data[id].additions = { __VA_ARGS__ }

void mutation_branch::init()
{
 int id = 0;
// Set all post-PF_MAX to valid
 for (int i = PF_MAX + 1; i < PF_MAX2; i++) data[i].valid = true;

 data[PF_MARLOSS].valid = false; // Never valid!

 MUTATION(PF_FLEET);
  CANCELS (PF_PONDEROUS1, PF_PONDEROUS2, PF_PONDEROUS3);
  CHANGES_TO (PF_FLEET2);

 MUTATION(PF_FLEET2);
  PREREQS (PF_FLEET);
  CANCELS (PF_PONDEROUS1, PF_PONDEROUS2, PF_PONDEROUS3);
  
 MUTATION(PF_FASTHEALER);
  CANCELS (PF_ROT1, PF_ROT2, PF_ROT3);
  CHANGES_TO (PF_FASTHEALER2);

 MUTATION(PF_LIGHTEATER);

 MUTATION(PF_NIGHTVISION);
  CHANGES_TO (PF_NIGHTVISION2);

 MUTATION(PF_POISRESIST);

 MUTATION(PF_THICKSKIN);

 MUTATION(PF_DEFT);

 MUTATION(PF_ANIMALEMPATH);

 MUTATION(PF_TERRIFYING);

 MUTATION(PF_DISRESISTANT);
  CHANGES_TO (PF_DISIMMUNE);

 MUTATION(PF_ROBUST);

 MUTATION(PF_ADRENALINE);

 MUTATION(PF_MYOPIC);

 MUTATION(PF_HEAVYSLEEPER);

 MUTATION(PF_LIGHTWEIGHT);

 MUTATION(PF_SMELLY);
  CHANGES_TO (PF_SMELLY2);

 MUTATION(PF_WEAKSTOMACH);
  CHANGES_TO (PF_NAUSEA);

 MUTATION(PF_UGLY);
  CHANGES_TO (PF_DEFORMED);
 

// Mutation-only traits start here

 MUTATION(PF_HOOVES);
  CANCELS (PF_PADDED_FEET, PF_LEG_TENTACLES);

 MUTATION(PF_SKIN_ROUGH);
  CHANGES_TO (PF_SCALES, PF_FEATHERS, PF_LIGHTFUR, PF_CHITIN, PF_PLANTSKIN);

 MUTATION(PF_NIGHTVISION2);
  PREREQS (PF_NIGHTVISION);
  CHANGES_TO (PF_NIGHTVISION3);

 MUTATION(PF_NIGHTVISION3);
  PREREQS (PF_NIGHTVISION2);
  LEADS_TO (PF_INFRARED);

 MUTATION(PF_FASTHEALER2);
  CANCELS (PF_ROT1, PF_ROT2, PF_ROT3);
  PREREQS (PF_FASTHEALER);
  CHANGES_TO (PF_REGEN);

 MUTATION(PF_REGEN);
  CANCELS (PF_ROT1, PF_ROT2, PF_ROT3);
  PREREQS (PF_FASTHEALER2);

 MUTATION(PF_FANGS);
  CANCELS (PF_BEAK);

 MUTATION(PF_MEMBRANE);
  PREREQS (PF_EYEBULGE);

 MUTATION(PF_SCALES);
  PREREQS (PF_SKIN_ROUGH);
  CHANGES_TO (PF_THICK_SCALES, PF_SLEEK_SCALES);

 MUTATION(PF_THICK_SCALES);
  PREREQS (PF_SCALES);
  CANCELS (PF_SLEEK_SCALES, PF_FEATHERS);

 MUTATION(PF_SLEEK_SCALES);
  PREREQS (PF_SCALES);
  CANCELS (PF_THICK_SCALES, PF_FEATHERS);

 MUTATION(PF_LIGHT_BONES);
  CHANGES_TO (PF_HOLLOW_BONES);

 MUTATION(PF_FEATHERS);
  PREREQS (PF_SKIN_ROUGH);
  CANCELS(PF_THICK_SCALES, PF_SLEEK_SCALES);

 MUTATION(PF_LIGHTFUR);
  PREREQS (PF_SKIN_ROUGH);
  CANCELS (PF_SCALES, PF_FEATHERS, PF_CHITIN, PF_PLANTSKIN);
  CHANGES_TO (PF_FUR);

 MUTATION(PF_FUR);
  CANCELS (PF_SCALES, PF_FEATHERS, PF_CHITIN, PF_PLANTSKIN);
  PREREQS (PF_LIGHTFUR);

 MUTATION(PF_CHITIN);
  CANCELS (PF_SCALES, PF_FEATHERS, PF_LIGHTFUR, PF_PLANTSKIN);
  CHANGES_TO (PF_CHITIN2);

 MUTATION(PF_CHITIN2);
  PREREQS (PF_CHITIN);
  CHANGES_TO (PF_CHITIN3);

 MUTATION(PF_CHITIN3);
  PREREQS (PF_CHITIN2);

 MUTATION(PF_SPINES);
  CHANGES_TO (PF_QUILLS);

 MUTATION (PF_QUILLS);
  PREREQS (PF_SPINES);

 MUTATION (PF_PLANTSKIN);
  CANCELS (PF_FEATHERS, PF_LIGHTFUR, PF_FUR, PF_CHITIN, PF_CHITIN2, PF_CHITIN3,
           PF_SCALES);
  CHANGES_TO (PF_BARK);
  LEADS_TO (PF_THORNS, PF_LEAVES);

 MUTATION(PF_BARK);
  PREREQS (PF_PLANTSKIN);

 MUTATION(PF_THORNS);
  PREREQS (PF_PLANTSKIN, PF_BARK);

 MUTATION(PF_LEAVES);
  PREREQS (PF_PLANTSKIN, PF_BARK);

 MUTATION(PF_NAILS);
  CHANGES_TO (PF_CLAWS, PF_TALONS);

 MUTATION(PF_CLAWS);
  PREREQS (PF_NAILS);
  CANCELS (PF_TALONS);

 MUTATION(PF_TALONS);
  PREREQS (PF_NAILS);
  CANCELS (PF_CLAWS);

 MUTATION(PF_PHEROMONE_INSECT);
  PREREQS (PF_SMELLY2);
  CANCELS (PF_PHEROMONE_MAMMAL);

 MUTATION(PF_PHEROMONE_MAMMAL);
  PREREQS (PF_SMELLY2);
  CANCELS (PF_PHEROMONE_INSECT);

 MUTATION(PF_DISIMMUNE);
  PREREQS (PF_DISRESISTANT);

 MUTATION(PF_POISONOUS);
  PREREQS (PF_POISRESIST);

 MUTATION(PF_SLIME_HANDS);
  PREREQS (PF_SLIMY);

 MUTATION(PF_COMPOUND_EYES);
  PREREQS (PF_EYEBULGE);

 MUTATION(PF_PADDED_FEET);
  CANCELS (PF_HOOVES, PF_LEG_TENTACLES);

 MUTATION(PF_HOOVES);
  CANCELS (PF_PADDED_FEET);

 MUTATION(PF_SAPROVORE);
  PREREQS (PF_CARNIVORE);
  CANCELS (PF_HERBIVORE, PF_RUMINANT);

 MUTATION(PF_RUMINANT);
  PREREQS (PF_HERBIVORE);
  CANCELS (PF_CARNIVORE, PF_SAPROVORE);

 MUTATION(PF_HORNS);
  PREREQS (PF_HEADBUMPS);
  CANCELS (PF_ANTENNAE);
  CHANGES_TO (PF_HORNS_CURLED, PF_HORNS_POINTED, PF_ANTLERS);

 MUTATION(PF_HORNS_CURLED);
  PREREQS (PF_HORNS);
  CANCELS (PF_ANTENNAE, PF_HORNS_POINTED, PF_ANTLERS);

 MUTATION(PF_HORNS_POINTED);
  PREREQS (PF_HORNS);
  CANCELS (PF_ANTENNAE, PF_HORNS_CURLED, PF_ANTLERS);

 MUTATION(PF_ANTLERS);
  PREREQS (PF_HORNS);
  CANCELS (PF_ANTENNAE, PF_HORNS_CURLED, PF_HORNS_POINTED);

 MUTATION(PF_ANTENNAE);
  PREREQS (PF_HEADBUMPS);
  CANCELS (PF_HORNS, PF_HORNS_CURLED, PF_HORNS_POINTED, PF_ANTLERS);

 MUTATION(PF_TAIL_STUB);
  CHANGES_TO (PF_TAIL_LONG, PF_TAIL_FIN);

 MUTATION(PF_TAIL_FIN);
  PREREQS (PF_TAIL_STUB);
  CANCELS (PF_TAIL_LONG, PF_TAIL_FLUFFY, PF_TAIL_STING, PF_TAIL_CLUB);

 MUTATION(PF_TAIL_LONG);
  PREREQS (PF_TAIL_STUB);
  CANCELS (PF_TAIL_FIN);
  CHANGES_TO (PF_TAIL_FLUFFY, PF_TAIL_STING, PF_TAIL_CLUB);

 MUTATION(PF_TAIL_FLUFFY);
  PREREQS (PF_TAIL_LONG);
  CANCELS (PF_TAIL_STING, PF_TAIL_CLUB, PF_TAIL_FIN);

 MUTATION(PF_TAIL_STING);
  PREREQS (PF_TAIL_LONG);
  CANCELS (PF_TAIL_FLUFFY, PF_TAIL_CLUB, PF_TAIL_FIN);

 MUTATION(PF_TAIL_CLUB);
  PREREQS (PF_TAIL_LONG);
  CANCELS (PF_TAIL_FLUFFY, PF_TAIL_STING, PF_TAIL_FIN);

 MUTATION(PF_PAINREC1);
  CHANGES_TO (PF_PAINREC2);

 MUTATION(PF_PAINREC2);
  PREREQS (PF_PAINREC1);
  CHANGES_TO (PF_PAINREC3);

 MUTATION(PF_PAINREC3);
  PREREQS (PF_PAINREC2);

 MUTATION(PF_MOUTH_TENTACLES);
  PREREQS (PF_MOUTH_FLAPS);
  CANCELS (PF_MANDIBLES);

 MUTATION(PF_MANDIBLES);
  PREREQS (PF_MOUTH_FLAPS);
  CANCELS (PF_BEAK, PF_FANGS, PF_MOUTH_TENTACLES);

 MUTATION(PF_WEB_WALKER);
  LEADS_TO (PF_WEB_WEAVER);

 MUTATION(PF_WEB_WEAVER);
  PREREQS (PF_WEB_WALKER);
  CANCELS (PF_SLIMY);

 MUTATION(PF_STR_UP);
  CHANGES_TO (PF_STR_UP_2);

 MUTATION(PF_STR_UP_2);
  PREREQS (PF_STR_UP);
  CHANGES_TO (PF_STR_UP_3);

 MUTATION(PF_STR_UP_3);
  PREREQS (PF_STR_UP_2);
  CHANGES_TO (PF_STR_UP_4);

 MUTATION(PF_STR_UP_4);
  PREREQS (PF_STR_UP_3);

 MUTATION(PF_DEX_UP);
  CHANGES_TO (PF_DEX_UP_2);

 MUTATION(PF_DEX_UP_2);
  PREREQS (PF_DEX_UP);
  CHANGES_TO (PF_DEX_UP_3);

 MUTATION(PF_DEX_UP_3);
  PREREQS (PF_DEX_UP_2);
  CHANGES_TO (PF_DEX_UP_4);

 MUTATION(PF_DEX_UP_4);
  PREREQS (PF_DEX_UP_3);

 MUTATION(PF_INT_UP);
  CHANGES_TO (PF_INT_UP_2);

 MUTATION(PF_INT_UP_2);
  PREREQS (PF_INT_UP);
  CHANGES_TO (PF_INT_UP_3);

 MUTATION(PF_INT_UP_3);
  PREREQS (PF_INT_UP_2);
  CHANGES_TO (PF_INT_UP_4);

 MUTATION(PF_INT_UP_4);
  PREREQS (PF_INT_UP_3);

 MUTATION(PF_PER_UP);
  CHANGES_TO (PF_PER_UP_2);

 MUTATION(PF_PER_UP_2);
  PREREQS (PF_PER_UP);
  CHANGES_TO (PF_PER_UP_3);

 MUTATION(PF_PER_UP_3);
  PREREQS (PF_PER_UP_2);
  CHANGES_TO (PF_PER_UP_4);

 MUTATION(PF_PER_UP_4);
  PREREQS (PF_PER_UP_3);


// Bad mutations below this point

 MUTATION(PF_HEADBUMPS);
  CHANGES_TO (PF_HORNS, PF_ANTENNAE);

 MUTATION(PF_EYEBULGE);
  LEADS_TO (PF_MEMBRANE);
  CHANGES_TO (PF_COMPOUND_EYES);

 MUTATION(PF_PALE);
  CHANGES_TO (PF_ALBINO);
  LEADS_TO (PF_TROGLO);

 MUTATION(PF_SPOTS);
  CHANGES_TO (PF_SORES);

 MUTATION(PF_SMELLY2);
  PREREQS (PF_SMELLY);
  LEADS_TO (PF_PHEROMONE_INSECT, PF_PHEROMONE_MAMMAL);

 MUTATION(PF_DEFORMED);
  PREREQS (PF_UGLY);
  CHANGES_TO (PF_DEFORMED2);

 MUTATION(PF_DEFORMED2);
  PREREQS (PF_DEFORMED);
  CHANGES_TO (PF_DEFORMED3);

 MUTATION(PF_DEFORMED3);
  PREREQS (PF_DEFORMED2);

 MUTATION(PF_HOLLOW_BONES);
  PREREQS (PF_LIGHT_BONES);

 MUTATION(PF_NAUSEA);
  PREREQS (PF_WEAKSTOMACH);
  CHANGES_TO (PF_VOMITOUS);

 MUTATION(PF_VOMITOUS);
  PREREQS (PF_NAUSEA);

 MUTATION(PF_ROT1);
  CANCELS (PF_FASTHEALER, PF_FASTHEALER2, PF_REGEN);
  CHANGES_TO (PF_ROT2);

 MUTATION(PF_ROT2);
  CANCELS (PF_FASTHEALER, PF_FASTHEALER2, PF_REGEN);
  PREREQS (PF_ROT1);
  CHANGES_TO (PF_ROT3);

 MUTATION(PF_ROT3);
  CANCELS (PF_FASTHEALER, PF_FASTHEALER2, PF_REGEN);
  PREREQS (PF_ROT2);

 MUTATION(PF_ALBINO);
  PREREQS (PF_PALE);

 MUTATION(PF_SORES);
  PREREQS (PF_SPOTS);

 MUTATION(PF_TROGLO);
  CANCELS (PF_SUNLIGHT_DEPENDENT);
  CHANGES_TO (PF_TROGLO2);

 MUTATION(PF_TROGLO2);
  CANCELS (PF_SUNLIGHT_DEPENDENT);
  PREREQS (PF_TROGLO);
  CHANGES_TO (PF_TROGLO3);

 MUTATION(PF_TROGLO3);
  CANCELS (PF_SUNLIGHT_DEPENDENT);
  PREREQS (PF_TROGLO2);

 MUTATION(PF_BEAK);
  CANCELS (PF_FANGS, PF_MANDIBLES);

 MUTATION(PF_RADIOACTIVE1);
  CHANGES_TO (PF_RADIOACTIVE2);

 MUTATION(PF_RADIOACTIVE2);
  PREREQS (PF_RADIOACTIVE1);
  CHANGES_TO (PF_RADIOACTIVE3);

 MUTATION(PF_RADIOACTIVE3);
  PREREQS (PF_RADIOACTIVE2);

 MUTATION(PF_SLIMY);
  LEADS_TO (PF_SLIME_HANDS);

 MUTATION(PF_CARNIVORE);
  CANCELS (PF_HERBIVORE, PF_RUMINANT);
  LEADS_TO (PF_SAPROVORE);

 MUTATION(PF_HERBIVORE);
  CANCELS (PF_CARNIVORE, PF_SAPROVORE);
  LEADS_TO (PF_RUMINANT);

 MUTATION(PF_PONDEROUS1);
  CANCELS (PF_FLEET, PF_FLEET2);
  CHANGES_TO (PF_PONDEROUS2);

 MUTATION(PF_PONDEROUS2);
  CANCELS (PF_FLEET, PF_FLEET2);
  PREREQS (PF_PONDEROUS1);
  CHANGES_TO (PF_PONDEROUS3);

 MUTATION(PF_PONDEROUS3);
  CANCELS (PF_FLEET, PF_FLEET2);
  PREREQS (PF_PONDEROUS2);

 MUTATION(PF_SUNLIGHT_DEPENDENT);
  CANCELS (PF_TROGLO, PF_TROGLO2, PF_TROGLO3);

 MUTATION(PF_GROWL);
  CHANGES_TO (PF_SNARL);

 MUTATION(PF_SNARL);
  PREREQS (PF_GROWL);

 MUTATION(PF_COLDBLOOD);
  CHANGES_TO (PF_COLDBLOOD2);

 MUTATION(PF_COLDBLOOD2);
  PREREQS (PF_COLDBLOOD);
  CHANGES_TO (PF_COLDBLOOD3);

 MUTATION(PF_COLDBLOOD3);
  PREREQS (PF_COLDBLOOD2);

 MUTATION(PF_SHOUT1);
  CHANGES_TO (PF_SHOUT2);

 MUTATION(PF_SHOUT2);
  PREREQS (PF_SHOUT1);
  CHANGES_TO (PF_SHOUT3);

 MUTATION(PF_SHOUT3);
  PREREQS (PF_SHOUT2);

 MUTATION(PF_WINGS_STUB);
  CHANGES_TO (PF_WINGS_BIRD, PF_WINGS_BAT, PF_WINGS_INSECT);

 MUTATION(PF_WINGS_BIRD);
  PREREQS (PF_WINGS_STUB);
  CANCELS (PF_WINGS_BAT, PF_WINGS_INSECT);

 MUTATION(PF_WINGS_BAT);
  PREREQS (PF_WINGS_STUB);
  CANCELS (PF_WINGS_BIRD, PF_WINGS_INSECT);

 MUTATION(PF_WINGS_INSECT);
  PREREQS (PF_WINGS_STUB);
  CANCELS (PF_WINGS_BIRD, PF_WINGS_BAT);

 MUTATION(PF_ARM_TENTACLES);
  CHANGES_TO (PF_ARM_TENTACLES_4);

 MUTATION(PF_ARM_TENTACLES_4);
  PREREQS (PF_ARM_TENTACLES);
  CHANGES_TO (PF_ARM_TENTACLES_8);

 MUTATION(PF_ARM_TENTACLES_8);
  PREREQS (PF_ARM_TENTACLES_4);

 MUTATION(PF_LEG_TENTACLES);
  CANCELS (PF_PADDED_FEET, PF_HOOVES);

 MUTATION(PF_SHELL);
  PREREQS (PF_CHITIN);
  CANCELS (PF_CHITIN3);
}

std::vector<pl_flag> mutations_from_category(mutation_category cat)
{
 std::vector<pl_flag> ret;
 switch (cat) {
 case MUTCAT_LIZARD:
  ret = {
PF_THICKSKIN, PF_INFRARED, PF_FANGS, PF_MEMBRANE, PF_SCALES, PF_TALONS,
PF_SLIT_NOSTRILS, PF_FORKED_TONGUE, PF_TROGLO, PF_WEBBED, PF_CARNIVORE,
PF_COLDBLOOD2, PF_STR_UP_2, PF_DEX_UP_2
  };
  break;

 case MUTCAT_BIRD:
  ret = {
PF_QUICK, PF_LIGHTEATER, PF_DEFT, PF_LIGHTSTEP, PF_BADBACK, PF_GLASSJAW,
PF_NIGHTVISION, PF_HOLLOW_BONES, PF_FEATHERS, PF_TALONS, PF_BEAK, PF_FLEET2,
PF_WINGS_BIRD, PF_COLDBLOOD, PF_DEX_UP_3
  };
  break;

 case MUTCAT_FISH:
  ret = {
PF_SMELLY2, PF_NIGHTVISION2, PF_FANGS, PF_MEMBRANE, PF_GILLS, PF_SLEEK_SCALES,
PF_TAIL_FIN, PF_DEFORMED, PF_THIRST, PF_WEBBED, PF_SLIMY, PF_COLDBLOOD2
  };
  break;

 case MUTCAT_BEAST:
  ret = {
PF_DEFT, PF_ANIMALEMPATH, PF_TERRIFYING, PF_ADRENALINE, PF_MYOPIC, PF_FORGETFUL,
PF_NIGHTVISION2, PF_FANGS, PF_FUR, PF_CLAWS, PF_PHEROMONE_MAMMAL,
PF_PADDED_FEET, PF_TAIL_FLUFFY, PF_SMELLY2, PF_DEFORMED2, PF_HUNGER, PF_TROGLO,
PF_CARNIVORE, PF_SNARL, PF_SHOUT3, PF_CANINE_EARS, PF_STR_UP_4
  };
  break;

 case MUTCAT_CATTLE:
  ret = {
PF_THICKSKIN, PF_DISRESISTANT, PF_NIGHTVISION, PF_FUR, PF_PHEROMONE_MAMMAL,
PF_HOOVES, PF_RUMINANT, PF_HORNS, PF_TAIL_LONG, PF_DEFORMED, PF_PONDEROUS2,
PF_CANINE_EARS, PF_STR_UP_2
  };
  break;

 case MUTCAT_INSECT:
  ret = {
PF_QUICK, PF_LIGHTEATER, PF_POISRESIST, PF_NIGHTVISION, PF_TERRIFYING,
PF_HEAVYSLEEPER, PF_NIGHTVISION2, PF_INFRARED, PF_CHITIN2, PF_PHEROMONE_INSECT,
PF_ANTENNAE, PF_WINGS_INSECT, PF_TAIL_STING, PF_MANDIBLES, PF_DEFORMED,
PF_TROGLO, PF_COLDBLOOD3, PF_STR_UP, PF_DEX_UP
  };
  break;

 case MUTCAT_PLANT:
  ret = {
PF_DISIMMUNE, PF_HEAVYSLEEPER, PF_BADHEARING, PF_FASTHEALER2, PF_BARK,
PF_THORNS, PF_LEAVES, PF_DEFORMED2, PF_PONDEROUS3, PF_STR_UP_2
  };
  break;

 case MUTCAT_SLIME:
  ret = {
PF_POISRESIST, PF_ROBUST, PF_CHEMIMBALANCE, PF_REGEN, PF_RADIOGENIC,
PF_DISIMMUNE, PF_POISONOUS, PF_SLIME_HANDS, PF_SMELLY2, PF_DEFORMED3,
PF_HOLLOW_BONES, PF_VOMITOUS, PF_HUNGER, PF_THIRST, PF_SORES, PF_TROGLO,
PF_WEBBED, PF_UNSTABLE, PF_RADIOACTIVE1, PF_SLIMY, PF_DEX_UP, PF_INT_UP
  };
  break;

 case MUTCAT_TROGLO:
  ret = {
PF_QUICK, PF_LIGHTEATER, PF_MYOPIC, PF_NIGHTVISION3, PF_INFRARED, PF_REGEN,
PF_DISIMMUNE, PF_POISONOUS, PF_SAPROVORE, PF_SLIT_NOSTRILS, PF_ALBINO,
PF_TROGLO3, PF_SLIMY
  };
  break;

 case MUTCAT_CEPHALOPOD:
  ret = {
PF_GILLS, PF_MOUTH_TENTACLES, PF_SLIT_NOSTRILS, PF_DEFORMED, PF_THIRST,
PF_BEAK, PF_SLIMY, PF_COLDBLOOD, PF_ARM_TENTACLES_8, PF_SHELL, PF_LEG_TENTACLES
  };
  break;

 case MUTCAT_SPIDER:
  ret = {
PF_FLEET, PF_POISRESIST, PF_NIGHTVISION3, PF_INFRARED, PF_FUR, PF_CHITIN3,
PF_POISONOUS, PF_COMPOUND_EYES, PF_MANDIBLES, PF_WEB_WEAVER, PF_TROGLO,
PF_CARNIVORE, PF_COLDBLOOD, PF_DEX_UP_2
  };
  break;

 case MUTCAT_RAT:
  ret = {
PF_DISRESISTANT, PF_NIGHTVISION2, PF_FANGS, PF_FUR, PF_CLAWS, PF_TAIL_LONG,
PF_WHISKERS, PF_DEFORMED3, PF_VOMITOUS, PF_HUNGER, PF_TROGLO2, PF_GROWL
  };
  break;
 }

 return ret;
}
