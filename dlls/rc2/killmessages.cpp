/***
*
*	Rocket Crowbar 2: the death messages. For a kill by a given weapon or entity classname the
*	multiplayer rules pick a weighted random line from this table and fill in the names, with
*	the victim's pronouns. Reconstructed from rc2.dll, the run between crowbar.cpp and
*	defaultai.cpp (0x31f30-0x32540; the table is .data 0x13bd38, 383 lines);
*	reference/dossiers/p2.md. The file and function names are ours.
*
****/

#include "extdll.h"
#include "util.h"
#include "p2shared.h"

// The line's shape: which names it takes, in order (sprintf arguments). K is the killer, V the
// victim (or "self"), W the weapon; the pronouns are the victim's.
enum killmsgtype_e
{
	KILLMSG_K_V = 0,		 // killer, victim
	KILLMSG_V_K = 1,		 // victim, killer
	KILLMSG_K_V_W = 2,		 // killer, victim, weapon
	KILLMSG_ALIAS = 3,		 // the format is another classname to use (killer forms)
	KILLMSG_V = 4,			 // victim (self forms from here on)
	KILLMSG_V_HIMSELF = 5,	 // victim, himself/herself/itself
	KILLMSG_V_HIS = 6,		 // victim, his/her/it's
	KILLMSG_V_HIM = 7,		 // victim, him/her/it
	KILLMSG_V_HIMSELF_HIS = 8, // victim, himself, his
	KILLMSG_V_HE = 9,		 // victim, he/she/it
	KILLMSG_V_W = 10,		 // victim, weapon
	KILLMSG_SELFALIAS = 11,	 // as 3, for the self forms
};

// flag bits: add a possessive to the name
#define KILLMSG_POSSESSIVE_K 1
#define KILLMSG_POSSESSIVE_V 2

struct killmsg_t
{
	const char* pszName; // weapon or classname
	const char* pszFormat;
	float flWeight;
	int iType;
	int iFlags;
};

// orig .data 0x13bd38
static killmsg_t g_KillMsgs[] =
	{
	{"generic", "%s killed %s with %s", 0.5f, 2, 0},
	{"generic", "%s killed self with %s", 0.5f, 10, 0},
	{"worldspawn", "%s died mysteriously", 0.5f, 4, 0},
	{"worldspawn", "%s just fell over and died", 0.5f, 4, 0},
	{"worldspawn", "%s seems to be dead", 0.5f, 4, 0},
	{"worldspawn", "%s has left this world", 0.5f, 4, 0},
	{"worldspawn", "%s managed to kill %s", 0.5f, 0, 0},
	{"suicide", "%s has left this world", 0.5f, 4, 0},
	{"suicide", "%s got bored with life", 0.5f, 4, 0},
	{"suicide", "%s tripped and broke %s nose", 0.5f, 6, 0},
	{"suicide", "%s fell on his crowbar", 0.1f, 4, 0},
	{"world", "suicide", 0.5f, 11, 0},
	{"world", "%s managed to kill %s", 0.5f, 0, 0},
	{"water", "%s drowned", 0.5f, 4, 0},
	{"water", "%s forgot to come up for air", 0.5f, 4, 0},
	{"water", "%s can't swim", 0.5f, 4, 0},
	{"water", "%s sleeps with the fishes", 0.5f, 4, 0},
	{"trigger_hurt", "%s was in the wrong place", 0.5f, 4, 0},
	{"trigger_hurt", "%s suffered the wrath of the level gods", 0.05f, 4, 0},
	{"trigger_changelevel", "%s tried to leave", 0.5f, 4, 0},
	{"trigger_changelevel", "%s found the exit", 0.5f, 4, 0},
	{"trigger_changelevel", "%s wanted out too much", 0.5f, 4, 0},
	{"trigger_changelevel", "%s really doesn't want to be here", 0.2f, 4, 0},
	{"func_tracktrain", "%s was run over", 0.5f, 4, 0},
	{"func_tracktrain", "%s is road kill", 0.5f, 4, 0},
	{"func_tracktrain", "%s tried to stop the train with %s bare hands", 0.5f, 6, 0},
	{"func_tracktrain", "SPLAT! %s just got run over", 0.1f, 4, 0},
	{"func_door", "%s was mooshed", 0.5f, 4, 0},
	{"func_door", "%s is flatter than a pancake", 0.5f, 4, 0},
	{"func_door", "%s was squashed", 0.5f, 4, 0},
	{"func_door", "%s was caught between a rock and a hard place", 0.5f, 4, 0},
	{"func_door_rotating", "func_door", 0.5f, 11, 0},
	{"momentary_door", "func_door", 0.5f, 11, 0},
	{"func_train", "func_door", 0.5f, 11, 0},
	{"func_rotating", "func_door", 0.5f, 11, 0},
	{"func_plat", "func_door", 0.5f, 11, 0},
	{"env_explosion", "%s blew up", 0.5f, 4, 0},
	{"env_explosion", "%s was blown up", 0.5f, 4, 0},
	{"env_explosion", "%s is now splattered all over the wall", 0.5f, 4, 0},
	{"grenade", "env_explosion", 0.5f, 11, 0},
	{"func_mortar_field", "%s blew %s up", 0.5f, 0, 0},
	{"func_mortar_field", "%s called in an air strike and got %s", 0.5f, 0, 0},
	{"func_mortar_field", "%s was hit by %s air strike", 0.5f, 1, 1},
	{"func_mortar_field", "%s blew up", 0.5f, 4, 0},
	{"func_mortar_field", "%s never heard the one that got %s", 0.5f, 7, 0},
	{"func_mortar_field", "%s failed to move out of the target zone", 0.2f, 4, 0},
	{"monster_mortar", "func_mortar_field", 0.5f, 3, 0},
	{"monster_mortar", "func_mortar_field", 0.5f, 11, 0},
	{"env_laser", "%s saw the light", 0.5f, 4, 0},
	{"env_laser", "%s was cut in two", 0.25f, 4, 0},
	{"env_beam", "env_laser", 0.5f, 11, 0},
	{"monster_alien_grunt", "%s was killed by an alien grunt", 0.5f, 4, 0},
	{"monster_alien_grunt", "%s was killed the Xen infantry", 0.25f, 4, 0},
	{"monster_apache", "%s was shot down by an apache", 0.5f, 4, 0},
	{"monster_barney", "%s pricked to death by a barney", 0.25f, 4, 0},
	{"monster_barney", "%s was killed by a barney", 0.5f, 4, 0},
	{"monster_bigmomma", "%s pissed off BigMomma", 0.5f, 4, 0},
	{"monster_barnacle", "%s head was gnawed on by a barnacle", 0.5f, 4, 2},
	{"bmortar", "monster_bigmomma", 0.5f, 11, 0},
	{"squidspit", "%s was slimed by a bullsquid", 0.5f, 4, 0},
	{"monster_bullchicken", "%s is bullsquid chow", 0.5f, 4, 0},
	{"monster_alien_controller", "%s was mentally inferior to an alien controller", 0.5f, 4, 0},
	{"controller_head_ball", "monster_alien_controller", 0.5f, 11, 0},
	{"controller_energy_ball", "monster_alien_controller", 0.5f, 11, 0},
	{"monster_gargantua", "%s didn't measure up to garg", 0.5f, 4, 0},
	{"monster_gargantua", "%s was squashed by garg", 0.5f, 4, 0},
	{"garg_stomp", "monster_gargantua", 0.5f, 11, 0},
	{"monster_human_assassin", "%s is in awe of an assassin", 0.5f, 4, 0},
	{"monster_headcrab", "%s was headcrabbed", 0.5f, 4, 0},
	{"monster_headcrab", "%s didn't look in the vents", 0.5f, 4, 0},
	{"monster_human_grunt", "%s was shot by a grunt", 0.5f, 4, 0},
	{"monster_houndeye", "%s was disintegrated by a houndeye shock wave", 0.5f, 4, 0},
	{"monster_ichthyosaur", "Cue the Jaws theme, %s just went under", 0.5f, 4, 0},
	{"monster_ichthyosaur", "%s is fish food", 0.5f, 4, 0},
	{"monster_alien_slave", "%s was zapped by an alien slave", 0.5f, 4, 0},
	{"monster_vortigaunt", "monster_alien_slave", 0.5f, 11, 0},
	{"monster_leech", "How pathetic! %s is leech food", 0.5f, 4, 0},
	{"monster_nihilanth", "%s lost the game", 0.5f, 4, 0},
	{"monster_nihilanth", "%s failed to prove %s worth", 0.5f, 6, 0},
	{"nihilanth_energy_ball", "monster_nihilanth", 0.5f, 11, 0},
	{"monster_osprey", "%s was killed by an osprey", 0.5f, 4, 0},
	{"monster_tentacle", "%s is now food for a very large and very aggressive plant", 0.5f, 4, 0},
	{"monster_turret", "%s was blasted by a turret", 0.5f, 4, 0},
	{"monster_miniturret", "monster_turret", 0.5f, 11, 0},
	{"monster_sentry", "monster_turret", 0.5f, 11, 0},
	{"xen_tree", "%s was admiring a Xen tree, when WHAM! it got %s", 0.5f, 7, 0},
	{"monster_zombie", "%s was zombified", 0.5f, 4, 0},
	{"func_tank", "%s shot %s", 0.5f, 0, 0},
	{"func_tanklaser", "%s saw %s light", 0.5f, 0, 1},
	{"rpg_rocket", "%s blew %s up", 0.5f, 0, 0},
	{"func_tankrocket", "%s blew %s up", 0.5f, 0, 0},
	{"func_tankmortar", "%s blew %s up", 0.5f, 0, 0},
	{"func_tank", "%s shot %s", 0.5f, 5, 0},
	{"func_tanklaser", "%s saw the light", 0.5f, 4, 0},
	{"rpg_rocket", "%s blew %s up", 0.5f, 5, 0},
	{"func_tankrocket", "%s blew %s up", 0.5f, 5, 0},
	{"func_tankmortar", "%s blew %s up", 0.5f, 5, 0},
	{"dmg_reflect", "%s felt %s pain", 0.6f, 1, 1},
	{"dmg_reflect", "%s helped %s to gain a sense of empathy", 0.5f, 0, 0},
	{"dmg_reflect", "%s dwelled too long on %s own pain", 0.5f, 6, 0},
	{"in_space", "%s sent %s into orbit", 0.5f, 0, 0},
	{"in_space", "%s laughed gleefully as %s explosively decompressed", 0.5f, 0, 0},
	{"in_space", "%s explosively decompressed", 0.5f, 4, 0},
	{"in_space", "%s reentered the atmosphere too fast", 0.5f, 4, 0},
	{"in_space", "%s found out exactly how much air resistance can do", 0.5f, 4, 0},
	{"in_space", "%s neglected air resistance", 0.25f, 4, 0},
	{"in_space", "%s froze in the vacuum of space", 0.5f, 4, 0},
	{"weapon_crowbar", "%s skull was bashed in by %s", 0.5f, 1, 2},
	{"weapon_crowbar", "%s bashed %s", 0.5f, 0, 0},
	{"weapon_crowbar", "%s clubbed %s", 0.5f, 0, 0},
	{"weapon_crowbar", "%s beat %s to death", 0.5f, 0, 0},
	{"weapon_crowbar", "%s managed to commit suicide with a crowbar", 0.5f, 4, 0},
	{"crowbar_rocket", "%s was hit by %s drunken rocket", 0.5f, 1, 1},
	{"crowbar_rocket", "%s never saw %s drunken rocket comming", 0.5f, 1, 1},
	{"crowbar_rocket", "%s tried to ride %s rocket, and failed", 0.5f, 1, 1},
	{"crowbar_rocket", "%s couldn't aim %s own drunken rocket", 0.5f, 6, 0},
	{"crowbar_rocket", "A drunken rocket came out of nowhere and blew %s up", 0.5f, 4, 0},
	{"crowbar_rocket", "%s blew %s up", 0.5f, 5, 0},
	{"crowbar_rocket", "%s went boom", 0.2f, 4, 0},
	{"crowbar_rocket", "BOOM!   There goes %s", 0.25f, 4, 0},
	{"crowbar_rocket", "%s didn't read the \"point away from face\" label", 0.2f, 4, 0},
	{"crowbarang", "%s remotely bashed %s", 0.5f, 0, 0},
	{"crowbarang", "%s clubbed %s from afar", 0.5f, 0, 0},
	{"crowbarang", "%s failed to catch %s crowbarang", 0.5f, 1, 1},
	{"crowbarang", "%s failed to catch %s own crowbarang", 0.5f, 6, 0},
	{"glock_gib", "%s was erroded by %s", 0.5f, 1, 0},
	{"glock_gib", "%s gave %s severe acid burns", 0.5f, 0, 0},
	{"glock_gib", "%s ate away at %s", 0.5f, 0, 0},
	{"glock_gib", "%s was melted by %s", 0.5f, 1, 0},
	{"glock_gib", "%s melted", 0.5f, 4, 0},
	{"glock_gib", "%s melted %s", 0.5f, 5, 0},
	{"glock_gib", "%s didn't read the \"point away from face\" label", 0.2f, 4, 0},
	{"glock_stickygib", "%s took a big whiff of %s gas", 0.5f, 1, 1},
	{"glock_stickygib", "%s smelled %s", 0.5f, 1, 0},
	{"glock_stickygib", "%s couldn't stand %s stench", 0.5f, 1, 1},
	{"glock_stickygib", "%s choked on %s gas", 0.5f, 1, 1},
	{"glock_stickygib", "%s choked on %s own gas", 0.5f, 6, 0},
	{"glock_stickygib", "%s couldn't stand %s own stench", 0.5f, 6, 0},
	{"glock_stickygib", "%s stinks!", 0.5f, 4, 0},
	{"transphasic_gib", "%s transphasic gib passed right thourgh %s", 0.5f, 0, 1},
	{"transphasic_gib", "%s armor was bypassed by %s tranphasic gib", 0.5f, 1, 3},
	{"transphasic_gib", "%s suffered a critical hit from %s", 0.5f, 1, 0},
	{"transphasic_gib", "%s transphasic gib missfired", 0.5f, 4, 2},
	{"transphasic_gib", "%s didn't read the \"point away from face\" label", 0.2f, 4, 0},
	{"357_discharge", "%s accepted %s discharge", 0.5f, 1, 1},
	{"357_discharge", "%s threw a hair dryer %s bathtub", 0.2f, 0, 2},
	{"357_discharge", "%s forgot electricity and water don't mix well", 0.5f, 4, 0},
	{"357_discharge", "%s electrocuted %s", 0.5f, 5, 0},
	{"357_discharge", "%s didn't get out of the water quick enough", 0.2f, 4, 0},
	{"weapon_357", "%s electrocuted %s", 0.5f, 0, 0},
	{"weapon_357", "%s cooked %s to a crisp", 0.5f, 0, 0},
	{"weapon_357", "%s fried %s", 0.5f, 0, 0},
	{"weapon_357", "%s rode %s lightning", 0.5f, 1, 1},
	{"weapon_357", "%s tried to ride %s own lightning", 0.5f, 6, 0},
	{"weapon_357", "%s cooked %s", 0.5f, 5, 0},
	{"weapon_357", "%s fried to crisp", 0.5f, 4, 0},
	{"weapon_357", "%s touched the wires together...", 0.1f, 4, 0},
	{"weapon_python", "weapon_357", 0.5f, 11, 0},
	{"weapon_python", "weapon_357", 0.5f, 3, 0},
	{"chain_lightning", "weapon_357", 0.5f, 11, 0},
	{"chain_lightning", "weapon_357", 0.5f, 3, 0},
	{"ball_lightning", "weapon_357", 0.5f, 11, 0},
	{"ball_lightning", "weapon_357", 0.5f, 3, 0},
	{"blaster_bolt", "%s blasted %s", 0.5f, 0, 0},
	{"blaster_bolt", "%s blasted %s", 0.5f, 5, 0},
	{"bouncy_bolt", "%s toasted %s", 0.5f, 0, 0},
	{"bouncy_bolt", "%s toasted %s", 0.5f, 5, 0},
	{"phase_pulse", "%s irradiated %s", 0.5f, 0, 0},
	{"phase_pulse", "%s started glowing green", 0.5f, 4, 0},
	{"phase_pulse", "%s irradiated %s", 0.5f, 5, 0},
	{"mp5_fart", "%s got a whiff of %s", 0.5f, 1, 0},
	{"mp5_fart", "%s tried to light %s fart", 0.5f, 1, 1},
	{"mp5_fart", "%s stinks, badly", 0.5f, 4, 0},
	{"monster_tripmine", "%s saw %s light", 0.5f, 1, 1},
	{"monster_tripmine", "%s was enlightened by %s", 0.5f, 1, 0},
	{"monster_tripmine", "There was a bright light, and then it was all over for %s", 0.5f, 4, 0},
	{"monster_tripmine", "%s saw the light", 0.5f, 4, 0},
	{"monster_tripmine", "%s failed to notice the small red beam", 0.2f, 4, 0},
	{"bouncy_tripmine", "%s saw %s light", 0.5f, 1, 1},
	{"bouncy_tripmine", "%s was enlightened by %s", 0.5f, 1, 0},
	{"bouncy_tripmine", "%s failed to notice the small green beam", 0.2f, 4, 0},
	{"bouncy_tripmine", "There was a bright light, and then it was all over for %s", 0.5f, 4, 0},
	{"bouncy_tripmine", "%s saw the light", 0.5f, 4, 0},
	{"punch_tripmine", "%s saw %s light", 0.5f, 1, 1},
	{"punch_tripmine", "%s was enlightened by %s", 0.5f, 1, 0},
	{"punch_tripmine", "%s failed to notice the small blue beam", 0.2f, 4, 0},
	{"punch_tripmine", "There was a bright light, and then it was all over for %s", 0.5f, 4, 0},
	{"punch_tripmine", "%s saw the light", 0.5f, 4, 0},
	{"target_tripmine", "%s saw %s light", 0.5f, 1, 1},
	{"target_tripmine", "%s was enlightened by %s", 0.5f, 1, 0},
	{"target_tripmine", "%s was entranced by %s color changing glowing ball", 0.25f, 1, 1},
	{"target_tripmine", "%s helped %s join the Disco revival", 0.1f, 0, 0},
	{"target_tripmine", "There was a bright light, and then it was all over for %s", 0.5f, 4, 0},
	{"target_tripmine", "%s saw the light", 0.5f, 4, 0},
	{"target_tripmine", "%s noticed the glowing ball an instant too late", 0.25f, 4, 0},
	{"target_tripmine", "%s joined the Disco revival", 0.1f, 4, 0},
	{"shotgun_sci", "%s tried to catch %s flying scientist", 0.5f, 1, 1},
	{"shotgun_sci", "%s caught %s flying scientist", 0.5f, 1, 1},
	{"shotgun_sci", "%s blew %s up with a flying scientist", 0.3f, 0, 0},
	{"shotgun_sci", "BLAHHHH!   There goes %s", 0.2f, 4, 0},
	{"shotgun_sci", "%s didn't read the \"point away from face\" label", 0.5f, 4, 0},
	{"shotgun_sci", "The last thing %s heard was \"BLAHHH\"", 0.2f, 4, 0},
	{"shotgun_sci", "%s blew %s up with %s own scientist", 0.5f, 8, 0},
	{"shotgun_forklift", "%s never saw %s miniature forklift comming", 0.5f, 1, 1},
	{"shotgun_forklift", "%s saw %s cute little forklift, and thought nothing of it", 0.5f, 1, 1},
	{"shotgun_forklift", "%s introduced %s to Gus", 0.5f, 0, 0},
	{"shotgun_forklift", "%s tried to talk to Gus", 0.5f, 4, 0},
	{"shotgun_forklift", "Gus didn't like %s", 0.5f, 4, 0},
	{"shotgun_forklift", "%s caught up with %s own forklift bomb", 0.5f, 6, 0},
	{"shotgun_hev", "%s couldn't fit into %s rocket propelled HEV suit", 0.5f, 1, 1},
	{"shotgun_hev", "%s tried on %s flying HEV suit", 0.5f, 1, 1},
	{"shotgun_hev", "%s thought %s needed a new HEV suit, but got it from a bad place", 0.5f, 9, 0},
	{"shotgun_hev", "%s found the HEV suit a bit too small", 0.2f, 4, 0},
	{"shotgun_crate", "%s was mesmerized by %s flying crate", 0.5f, 1, 1},
	{"shotgun_crate", "%s blew %s up with a crate", 0.5f, 0, 0},
	{"shotgun_crate", "%s blew %s up with a crate", 0.5f, 5, 0},
	{"shotgun_crate", "%s has a strange affinity for crates", 0.5f, 4, 0},
	{"weapon_crossbow", "%s aim is better than %s", 0.5f, 0, 3},
	{"weapon_crossbow", "%s didn't notice %s in the dark corner", 0.5f, 1, 0},
	{"weapon_crossbow", "%s shot %s in the foot", 0.5f, 5, 0},
	{"impailer", "%s impailed %s", 0.5f, 0, 0},
	{"impailer", "%s hung %s out to dry", 0.5f, 0, 0},
	{"impailer", "%s forgot to stay away from sharp, pointy things", 0.5f, 4, 0},
	{"impailer_bleeding", "%s experimented with blood letting on %s", 0.5f, 0, 0},
	{"impailer_bleeding", "%s drained %s of blood", 0.5f, 0, 0},
	{"impailer_bleeding", "%s denied %s a blood transfusion", 0.5f, 0, 0},
	{"impailer_bleeding", "%s bleed %s to death", 0.5f, 0, 0},
	{"impailer_bleeding", "%s bleed %s to death", 0.5f, 5, 0},
	{"xbow_snipeexplo", "%s just missed %s", 0.5f, 0, 0},
	{"xbow_snipeexplo", "%s almost dodged %s bullet", 0.2f, 1, 1},
	{"xbow_snipeexplo", "%s managed to snipe %s", 0.5f, 5, 0},
	{"xbow_snipeexplo", "%s blew %s up with %s own explosive round", 0.5f, 8, 0},
	{"xbow_teather", "%s really likes %s", 0.5f, 0, 0},
	{"xbow_teather", "%s loved %s to death", 0.5f, 0, 0},
	{"xbow_teather", "%s grabbed hold of %s", 0.5f, 0, 0},
	{"xbow_teather", "%s loves %s", 0.5f, 5, 0},
	{"explode_bolt", "%s turned %s inside-out", 0.5f, 0, 0},
	{"explode_bolt", "%s blew %s up from the inside", 0.5f, 0, 0},
	{"explode_bolt", "%s blew %s up from the inside", 0.5f, 5, 0},
	{"explode_bolt", "%s stood too close", 0.5f, 4, 0},
	{"scientist_mine", "%s shook hands with %s scientist mine", 0.5f, 1, 1},
	{"scientist_mine", "%s said hello back to %s scientist mine", 0.5f, 1, 1},
	{"scientist_mine", "%s blew %s up with a scientist mine", 0.7f, 0, 0},
	{"scientist_mine", "\"Freeman...\"  %s never saw it comming", 0.3f, 4, 0},
	{"scientist_mine", "%s tried to shake hands with %s own scientist mine", 0.5f, 6, 0},
	{"scientist_mine", "%s blew %s up with %s own scientist mine", 0.5f, 8, 0},
	{"suck_mine", "%s sucked %s in", 0.5f, 0, 0},
	{"suck_mine", "%s sucked %s in", 0.5f, 5, 0},
	{"sphere_mine", "%s amazed %s with the pretty floating sphere", 0.5f, 0, 0},
	{"sphere_mine", "%s dazzled %s", 0.5f, 0, 0},
	{"sphere_mine", "%s is seeing stars thanks to %s", 0.5f, 1, 0},
	{"sphere_mine", "%s was amazed by the pretty floating sphere", 0.5f, 4, 0},
	{"sphere_mine", "%s was dazzled", 0.5f, 4, 0},
	{"sphere_mine", "%s is seeing stars", 0.5f, 4, 0},
	{"sm_project", "sphere_mine", 0.5f, 3, 0},
	{"sm_project", "sphere_mine", 0.5f, 11, 0},
	{"rocket_mine", "%s watched %s Roman Candle right up to the end", 0.5f, 1, 1},
	{"rocket_mine", "%s Roman Candle was too much for %s", 0.5f, 0, 1},
	{"rocket_mine", "%s tried to learn about fireworks from %s", 0.5f, 1, 0},
	{"rocket_mine", "%s watched the Roman Candle right up to the end", 0.5f, 4, 0},
	{"rocket_mine", "%s tried to hold the Roman Candle while it went off", 0.5f, 4, 0},
	{"rocket_mine", "%s didn't read the \"point away from face\" label", 0.5f, 4, 0},
	{"rm_rocket", "rocket_mine", 0.5f, 3, 0},
	{"rm_rocket", "rocket_mine", 0.5f, 11, 0},
	{"gauss_telefrag", "%s telefragged %s", 0.5f, 0, 0},
	{"gauss_telefrag", "%s got in %s way", 0.5f, 1, 1},
	{"gauss_telefrag", "%s tried to invade %s personal bubble", 0.5f, 1, 1},
	{"gauss_telefrag", "%s failed to grasp the mechanics of teleportation", 0.5f, 4, 0},
	{"gauss_telefrag", "%s vanished in a white flash", 0.5f, 4, 0},
	{"gauss_telefrag", "%s discovered what the inside of %s stomach looks like", 0.5f, 6, 0},
	{"squash", "%s squashed %s", 0.5f, 0, 0},
	{"squash", "%s stepped on %s", 0.5f, 0, 0},
	{"squash", "%s turned %s into a little red splatter on the ground", 0.5f, 0, 0},
	{"squash", "%s feels a bit confined", 0.5f, 4, 0},
	{"squash", "%s is claustrophobic", 0.5f, 4, 0},
	{"squash", "%s expanded to fill a non-existant space", 0.5f, 4, 0},
	{"squash", "%s didn't make room for %s head", 0.5f, 6, 0},
	{"imploder", "%s shrank %s into a mathematical point", 0.5f, 0, 0},
	{"imploder", "%s shrank %s beyond recovery", 0.5f, 0, 0},
	{"imploder", "%s taught %s the true meaning of a mathematical point", 0.5f, 0, 0},
	{"imploder", "%s became a singularity thanks to %s", 0.5f, 1, 0},
	{"imploder", "%s imploded %s", 0.5f, 0, 0},
	{"imploder", "%s imploded %s", 0.5f, 5, 0},
	{"imploder", "%s became a singularity", 0.5f, 4, 0},
	{"nuclear_missile", "%s lectured %s about applied physics", 0.5f, 0, 0},
	{"nuclear_missile", "%s found out the power of the atom from %s", 0.5f, 1, 0},
	{"nuclear_missile", "%s was the target of %s tactical nuclear strike", 0.5f, 1, 1},
	{"nuclear_missile", "%s slept through %s physics class", 0.2f, 1, 1},
	{"nuclear_missile", "%s didn't heed the warning siren", 0.5f, 4, 0},
	{"nuclear_missile", "%s lacks a keen grasp of nuclear physics", 0.5f, 4, 0},
	{"nuclear_missile", "%s was at ground zero", 0.5f, 4, 0},
	{"nuclear_missile", "%s is now having a chat with the physics legends", 0.5f, 4, 0},
	{"nuclear_missile", "%s discovered the conservation of mass-energy", 0.5f, 4, 0},
	{"freeze", "%s froze %s", 0.5f, 0, 0},
	{"freeze", "%s attempted cryogenic freezing on %s", 0.5f, 0, 0},
	{"freeze", "%s preserved %s for posterity", 0.5f, 0, 0},
	{"freeze", "%s turned %s into a giant ice cube", 0.5f, 0, 0},
	{"freeze", "%s caught a very bad cold from %s", 0.1f, 1, 0},
	{"freeze", "%s froze %s", 0.5f, 5, 0},
	{"freeze", "%s discovered the flaws in cryogenic freezing", 0.5f, 4, 0},
	{"freeze", "%s preserved %s for posterity", 0.5f, 5, 0},
	{"freeze", "%s caught a cold", 0.1f, 4, 0},
	{"freeze_grenade", "freeze", 0.5f, 3, 0},
	{"freeze_grenade", "freeze", 0.5f, 11, 0},
	{"gravity_grenade", "%s sucked %s in", 0.5f, 0, 0},
	{"gravity_grenade", "%s found out the power of gravity from %s", 0.5f, 1, 0},
	{"gravity_grenade", "%s stretched %s to infinity", 0.5f, 0, 0},
	{"gravity_grenade", "%s promised to join %s on the other side", 0.5f, 0, 0},
	{"gravity_grenade", "%s accelerated %s beyond imagination", 0.25f, 0, 0},
	{"gravity_grenade", "%s was compressed to nothing", 0.5f, 4, 0},
	{"gravity_grenade", "%s stared into the abyss", 0.5f, 4, 0},
	{"gravity_grenade", "%s crossed the event horizon", 0.5f, 4, 0},
	{"gravity_grenade", "The weakest force got the better of %s", 0.25f, 4, 0},
	{"gravity_grenade", "%s discovered a parallel universe", 0.25f, 4, 0},
	{"gravity_grenade", "%s became one with the universe", 0.1f, 4, 0},
	{"antifriction_grenade", "%s tripped %s up", 0.5f, 0, 0},
	{"antifriction_grenade", "%s helped %s break the sound barrier", 0.5f, 0, 0},
	{"antifriction_grenade", "%s tripped on %s banana peel", 0.5f, 1, 1},
	{"antifriction_grenade", "%s tripped and broke %s nose", 0.5f, 6, 0},
	{"antifriction_grenade", "%s fell down", 0.5f, 4, 0},
	{"antifriction_grenade", "%s broke the sound barrier", 0.5f, 4, 0},
	{"antifriction_grenade", "%s discovered perpetual motion", 0.5f, 4, 0},
	{"ultima_grenade", "%s obliterated %s", 0.5f, 0, 0},
	{"ultima_grenade", "%s removed %s from this plane of existence", 0.5f, 0, 0},
	{"ultima_grenade", "%s showed %s ultimate power", 0.5f, 0, 0},
	{"ultima_grenade", "%s failed to grasp %s true power", 0.5f, 1, 1},
	{"ultima_grenade", "%s has ceased to exist", 0.5f, 4, 0},
	{"ultima_grenade", "%s left this plane of existence", 0.5f, 4, 0},
	{"ultima_grenade", "%s is no more", 0.5f, 4, 0},
	{"ultima_grenade", "%s was obliterated", 0.5f, 4, 0},
	{"ultima_grenade", "%s discovered ultimate power", 0.5f, 4, 0},
	{"ultima_grenade", "%s tried to play god", 0.25f, 4, 0},
	{"roach_charge", "%s blew %s up", 0.5f, 0, 0},
	{"roach_charge", "%s didn't notice %s pet cockroach", 0.5f, 1, 1},
	{"roach_charge", "%s tried to step on %s cockroach", 0.5f, 1, 1},
	{"roach_charge", "%s was fascinated by %s cockroach", 0.5f, 1, 1},
	{"roach_charge", "%s blew %s up", 0.5f, 5, 0},
	{"roach_charge", "The cockroaches got revenge on %s for the deaths of their breathren", 0.5f, 4, 0},
	{"roach_charge", "%s stepped on one too many cockroaches", 0.5f, 4, 0},
	{"roach_charge", "%s stepped on one too few cockroaches", 0.5f, 4, 0},
	{"roach_charge", "%s never expected a cockroach to explode like that", 0.25f, 4, 0},
	{"control_forklift", "shotgun_forklift", 0.5f, 3, 1},
	{"control_forklift", "shotgun_forklift", 0.5f, 11, 1},
	{"conc_charge", "%s helped %s break the sound barrier", 0.5f, 0, 0},
	{"conc_charge", "%s accelerated %s a little too fast", 0.5f, 0, 0},
	{"conc_charge", "%s discovered the secret of flying, with %s help", 0.5f, 1, 1},
	{"conc_charge", "%s broke the sound barrier", 0.5f, 4, 0},
	{"conc_charge", "%s discovered the secret of flying", 0.5f, 4, 0},
	{"conc_charge", "%s was accelerated a little too fast", 0.5f, 4, 0},
	{"monster_snark", "%s was eaten by %s little friends", 0.5f, 1, 1},
	{"monster_snark", "%s fed %s to the snarks", 0.5f, 0, 0},
	{"monster_snark", "%s is snarkbait", 0.5f, 4, 0},
	{"monster_snark", "%s wanted to become familiar with the snarks", 0.5f, 4, 0},
	{"snark_project", "%s snarks blasted %s", 0.5f, 0, 1},
	{"snark_project", "%s was blasted by %s own snark", 0.5f, 6, 0},
	{"mini_grunt", "%s was beaten to a pulp by %s squad of grunts", 0.5f, 1, 1},
	{"mini_grunt", "%s was overpowerd by %s munchkin grunts", 0.5f, 1, 1},
	{"mini_grunt", "%s mistreated %s little friends", 0.5f, 6, 0},
	{"mini_grunt", "%s grunts rebelled against %s", 0.5f, 7, 2},
	{"assassin_gun", "%s was the victim of %s surprise attack", 0.5f, 1, 1},
	{"assassin_gun", "%s was amazed by %s acrobatics", 0.5f, 1, 1},
	{"assassin_gun", "%s was too busy watching %s to notice the bullets flying past", 0.25f, 1, 0},
	{"assassin_gun", "%s shot %s in the foot", 0.5f, 5, 0},
	{"assassin_kick", "%s knocked %s jaw out", 0.5f, 0, 2},
	{"assassin_kick", "%s knocked %s own jaw out", 0.5f, 6, 0},
	{"houndeye_beam", "%s irradiated %s", 0.5f, 0, 0},
	{"houndeye_beam", "%s blasted %s", 0.5f, 0, 0},
	{"houndeye_beam", "%s annihilated %s", 0.5f, 0, 0},
	{"houndeye_beam", "%s disintegrated %s", 0.5f, 0, 0},
	{"houndeye_beam", "%s irradiated %s", 0.5f, 5, 0},
	{"houndeye_sonic", "%s disintegrated %s", 0.5f, 0, 0},
	{"houndeye_sonic", "%s caused %s to suffer severe internal bleeding", 0.5f, 0, 0},
	{"houndeye_sonic", "%s went into shock, thanks to %s", 0.5f, 1, 0},
	{"houndeye_sonic", "%s went into shock", 0.5f, 4, 0},
	{"tcontroller_tball", "%s proved to be mentally inferior to %s", 0.5f, 1, 0},
	{"tcontroller_tball", "%s mentally dominated %s", 0.25f, 0, 0},
	{"tcontroller_tball", "%s couldn't control %s own power", 0.5f, 6, 0},
	{"stukabat_bite", "%s gnawed on %s", 0.5f, 0, 0},
	{"stukabat_bite", "%s gnawed on %s", 0.5f, 5, 0},
	{"headcrab_leap", "%s headcrabbed %s", 0.5f, 0, 0},
	{"headcrab_leap", "%s zombified %s", 0.5f, 0, 0},
	{"headcrab_leap", "%s ate %s brain", 0.5f, 0, 2},
	{"headcrab_leap", "%s was zombified", 0.5f, 4, 0},
};

#define KILLMSG_COUNT ((int)(sizeof(g_KillMsgs) / sizeof(g_KillMsgs[0])))

// orig 0x31f30, orig client 0x9c1d0 (not in files-client.tsv: Ghidra missed it): line i is one for pszName, of the killer forms (bSelf 0) or the self forms
static bool KillMsg_Matches(int i, const char* pszName, int bSelf) // name ours
{
	int iType = g_KillMsgs[i].iType;
	if (bSelf == 0)
	{
		if (iType != KILLMSG_K_V && iType != KILLMSG_V_K && iType != KILLMSG_K_V_W && iType != KILLMSG_ALIAS)
			return false;
	}
	else
	{
		if (iType != KILLMSG_V && iType != KILLMSG_V_HIMSELF && iType != KILLMSG_V_HIS && iType != KILLMSG_V_HIM && iType != KILLMSG_V_HIMSELF_HIS && iType != KILLMSG_V_HE && iType != KILLMSG_SELFALIAS && iType != KILLMSG_V_W)
			return false;
	}
	return strcmp(pszName, g_KillMsgs[i].pszName) == 0;
}

// orig 0x31fe0, orig client 0x9c2b0 (not in files-client.tsv: Ghidra missed it): a weighted random line for pszName (iRandom 0..1000), following aliases; 0
// (the generic line) when there is none
int KillMsg_Pick(const char* pszName, int bSelf, int iRandom) // name ours
{
	while (true)
	{
		float flTotal = 0;
		for (int i = 0; i < KILLMSG_COUNT; i++)
		{
			if (KillMsg_Matches(i, pszName, bSelf))
				flTotal += g_KillMsgs[i].flWeight;
		}

		float flPick = (float)iRandom * 0.001 * flTotal;
		int i;
		for (i = 0;; i++)
		{
			if (i >= KILLMSG_COUNT)
				return 0;
			if (KillMsg_Matches(i, pszName, bSelf))
			{
				flPick -= g_KillMsgs[i].flWeight;
				if (flPick <= 0)
					break;
			}
		}

		if (g_KillMsgs[i].iType != KILLMSG_ALIAS && g_KillMsgs[i].iType != KILLMSG_SELFALIAS)
			return i;
		pszName = g_KillMsgs[i].pszFormat;
	}
}

// orig 0x320a0, orig 0x320c0, orig 0x320e0, orig 0x32100: the victim's pronouns (iGender 1 female, 2 neuter).
// client.dll has them too (orig client 0x9c3a0, orig client 0x9c3d0, orig client 0x9c400, orig client 0x9c430);
// its death notice calls KillMsg_His, so they aren't static.
const char* KillMsg_Himself(int iGender) { return iGender == 1 ? "herself" : (iGender == 2 ? "itself" : "himself"); } // name ours
const char* KillMsg_His(int iGender) { return iGender == 1 ? "her" : (iGender == 2 ? "it's" : "his"); }			  // name ours
const char* KillMsg_Him(int iGender) { return iGender == 1 ? "her" : (iGender == 2 ? "it" : "him"); }				  // name ours
const char* KillMsg_He(int iGender) { return iGender == 1 ? "she" : (iGender == 2 ? "it" : "he"); }				  // name ours

static void KillMsg_Possessive(char* pszName) // name ours
{
	size_t len = strlen(pszName);
	if (pszName[len - 1] == 's' || pszName[len - 1] == 'S')
		strcat(pszName, "'");
	else
		strcat(pszName, "'s");
}

// orig 0x32120, orig client 0x9c460: line iMsg with the names filled in, into pszOut. The killer reads "self" for a
// self form, when the two names are the same, or when bSelf.
void KillMsg_Format(int iMsg, const char* pszVictim, const char* pszKiller, const char* pszWeapon, int iGender, char* pszOut, int bSelf) // name ours
{
	char szVictim[256];
	char szKiller[256];
	char szWeapon[256];

	int iType = g_KillMsgs[iMsg].iType;
	if (iType == KILLMSG_V || iType == KILLMSG_V_HIMSELF || iType == KILLMSG_V_HIS || iType == KILLMSG_V_HIM || iType == KILLMSG_V_HIMSELF_HIS || iType == KILLMSG_V_HE || iType == KILLMSG_SELFALIAS || iType == KILLMSG_V_W || strcmp(pszVictim, pszKiller) == 0 || bSelf != 0)
	{
		strcpy(szKiller, "self");
		// Original oddity, kept: the generic killer line becomes the generic suicide
		if (iMsg == 0)
			iMsg = 1;
	}
	else
	{
		strncpy(szKiller, pszKiller, 250);
		szKiller[250] = '\0';
	}

	strncpy(szVictim, pszVictim, 250);
	szVictim[250] = '\0';
	strncpy(szWeapon, pszWeapon, 250);
	szWeapon[250] = '\0';

	int iFlags = g_KillMsgs[iMsg].iFlags;
	if ((iFlags & KILLMSG_POSSESSIVE_K) != 0)
		KillMsg_Possessive(szKiller);
	if ((iFlags & KILLMSG_POSSESSIVE_V) != 0)
		KillMsg_Possessive(szVictim); // "%s ate %s brain"

	const char* pszFormat = g_KillMsgs[iMsg].pszFormat;
	switch (g_KillMsgs[iMsg].iType)
	{
	case KILLMSG_K_V:
		sprintf(pszOut, pszFormat, szKiller, szVictim);
		return;
	case KILLMSG_V_K:
		sprintf(pszOut, pszFormat, szVictim, szKiller);
		return;
	case KILLMSG_K_V_W:
		sprintf(pszOut, pszFormat, szKiller, szVictim, szWeapon);
		return;
	case KILLMSG_V:
		sprintf(pszOut, pszFormat, szVictim);
		return;
	case KILLMSG_V_HIMSELF:
		sprintf(pszOut, pszFormat, szVictim, KillMsg_Himself(iGender));
		return;
	case KILLMSG_V_HIS:
		sprintf(pszOut, pszFormat, szVictim, KillMsg_His(iGender));
		return;
	case KILLMSG_V_HIM:
		sprintf(pszOut, pszFormat, szVictim, KillMsg_Him(iGender));
		return;
	case KILLMSG_V_HIMSELF_HIS:
		sprintf(pszOut, pszFormat, szVictim, KillMsg_Himself(iGender), KillMsg_His(iGender));
		return;
	case KILLMSG_V_HE:
		sprintf(pszOut, pszFormat, szVictim, KillMsg_He(iGender));
		return;
	case KILLMSG_V_W:
		sprintf(pszOut, pszFormat, szVictim, szWeapon);
		return;
	default: // the aliases: the generic line's shape
		sprintf(pszOut, "%s killed %s with %s", szKiller, szVictim, szWeapon);
		return;
	}
}

// orig 0x32540, orig client 0x9c990: KillMsg_Format into a static buffer
const char* KillMsg_Get(int iMsg, const char* pszVictim, const char* pszKiller, const char* pszWeapon, int iGender, int bSelf) // name ours
{
	static char szMsg[512]; // VERIFY: size; the original's static (.bss 0x18c0b4) is followed by other data
	KillMsg_Format(iMsg, pszVictim, pszKiller, pszWeapon, iGender, szMsg, bSelf);
	return szMsg;
}
