/****************************************************************************
 * [S]imulated [M]edieval [A]dventure multi[U]ser [G]ame      |   \\._.//   *
 * -----------------------------------------------------------|   (0...0)   *
 * SMAUG 1.4 (C) 1994, 1995, 1996, 1998  by Derek Snider      |    ).:.(    *
 * -----------------------------------------------------------|    {o o}    *
 * SMAUG code team: Thoric, Altrag, Blodkai, Narn, Haus,      |   / ' ' \   *
 * Scryn, Rennard, Swordbearer, Gorog, Grishnakh, Nivek,      |~'~.VxvxV.~'~*
 * Tricops and Fireblade                                      |             *
 * ------------------------------------------------------------------------ *
 * Merc 2.1 Diku Mud improvments copyright (C) 1992, 1993 by Michael        *
 * Chastain, Michael Quan, and Mitchell Tse.                                *
 * Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,          *
 * Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.     *
 * ------------------------------------------------------------------------ *
 *                      Battle & death module                               *
 ****************************************************************************/

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <time.h>

#include "comm.h"
#include "mud.h"

extern char lastplayercmd[MAX_INPUT_LENGTH];
extern CHAR_DATA *gch_prev;

OBJ_DATA *used_weapon; /* Used to figure out which weapon
                        * later */

/*
 * Local functions.
 */
void new_dam_message
    args((CHAR_DATA * ch, CHAR_DATA *victim, int dam,
          int dt, OBJ_DATA *obj));
void death_cry args((CHAR_DATA * ch));
void group_gain args((CHAR_DATA * ch, CHAR_DATA *victim));
int xp_compute args((CHAR_DATA * gch, CHAR_DATA *victim));
int align_compute args((CHAR_DATA * gch, CHAR_DATA *victim));
ch_ret one_hit args((CHAR_DATA * ch, CHAR_DATA *victim, int dt));
int obj_hitroll args((OBJ_DATA * obj));
void show_condition args((CHAR_DATA * ch, CHAR_DATA *victim));
bool pkill_ok args((CHAR_DATA * ch, CHAR_DATA *victim));

bool pkill_ok(CHAR_DATA *ch, CHAR_DATA *victim) {
  bool clanbypass = false;
  OBJ_DATA *o;

  if (IS_NPC(ch) || IS_NPC(victim))
    return true;

  if (ch->truepl <= 250) {
    send_to_char("You can not fight other players until you're out of training'.\n\r",
                 ch);
    return false;
  }
  if (victim->truepl <= 250) {
    send_to_char("You can not fight other players until they're out of training.\n\r",
                 ch);
    return false;
  }
  if ((o = carrying_dball(victim)) != NULL) {
    return true;
  }
  if (ch->pcdata->clan && victim->pcdata->clan && !xIS_SET(victim->act, PLR_PK1) && !xIS_SET(victim->act, PLR_PK2)) {
    if (is_kaio(ch) && kairanked(victim) && ch->kairank <= victim->kairank)
      clanbypass = true;
    else if (is_demon(ch) && demonranked(victim))
      clanbypass = true;
    else if (kairanked(ch) && demonranked(victim))
      clanbypass = true;
    else if (demonranked(ch) && demonranked(victim))
      clanbypass = true;

    if (!clanbypass) {
      if (ch->pcdata->clan == victim->pcdata->clan) {
        send_to_char("You can't kill another member of your clan!\n\r",
                     ch);
        return false;
      }
      if (ch->pcdata->clan != victim->pcdata->clan) {
        switch (allianceStatus(ch->pcdata->clan,
                               victim->pcdata->clan)) {
          case ALLIANCE_FRIENDLY:
          case ALLIANCE_ALLIED:
            send_to_char("Your clan is at peace with theirs!.\n\r",
                         ch);
            return false;
            break;
          case ALLIANCE_NEUTRAL:
            send_to_char("Your clan is in a state of neutrality with theirs!\n\r",
                         ch);
            return false;
            break;
          case ALLIANCE_HOSTILE:
            if (xIS_SET(victim->act, PLR_WAR1)) {
              if ((float)ch->exp /
                          victim->exp >
                      5 &&
                  !IS_HC(victim)) {
                ch_printf(ch,
                          "You are more than 5 times stronger.\n\r");
                return false;
              }
              xSET_BIT(ch->act, PLR_WAR1);
              ch->pcdata->pk_timer = 60;
              return true;
            } else if (xIS_SET(victim->act, PLR_WAR2)) {
              xSET_BIT(ch->act, PLR_WAR2);
              ch->pcdata->pk_timer = 60;
              return true;
            }
            ch_printf(ch,
                      "They have to have a war flag on when you're not at full war status with that clan!\n\r");
            return false;
            break;
          case ALLIANCE_ATWAR:
            if (!xIS_SET(ch->act, PLR_WAR2))
              xSET_BIT(ch->act, PLR_WAR2);
            else if (!xIS_SET(ch->act, PLR_WAR1))
              xSET_BIT(ch->act, PLR_WAR1);
            ch->pcdata->pk_timer = 60;
            return true;
            break;
          default:
            break;
        }
      }
    }
  }
  if (!IS_HC(ch)) {
    ch->pcdata->pk_timer = 60;
  }
  ch->pcdata->gohometimer = 30;

  if (IS_HC(victim) && !IS_HC(ch)) {
    xREMOVE_BIT(ch->act, PLR_PK1);
    xSET_BIT(ch->act, PLR_PK2);
    return true;
  }
  if (xIS_SET(victim->act, PLR_BOUNTY) && victim->pcdata->bounty > 0 && !str_cmp(victim->name, ch->pcdata->hunting)) {
    if (xIS_SET(victim->in_room->room_flags, ROOM_SAFE)) {
      return false;
    }
    if (xIS_SET(victim->act, PLR_PK1) && !IS_HC(ch)) {
      xREMOVE_BIT(ch->act, PLR_PK1);
      xSET_BIT(ch->act, PLR_PK2);
    } else if (!xIS_SET(victim->act, PLR_PK2) && !IS_HC(ch)) {
      xSET_BIT(ch->act, PLR_PK2);
    } else if (!xIS_SET(victim->act, PLR_PK1) && !xIS_SET(victim->act, PLR_PK2) && !IS_HC(victim)) {
      return false;
    }
    return true;
  }
  if (xIS_SET(victim->act, PLR_PK1) || xIS_SET(victim->act, PLR_WAR1)) {
    if ((float)ch->exp / victim->exp > 5) {
      send_to_char("They have a yellow PK flag and you are more than 5 times stronger.\n\r",
                   ch);

      if (!xIS_SET(ch->act, PLR_PK2) && !IS_HC(ch))
        xSET_BIT(ch->act, PLR_PK1);
      return false;
    }
  }
  if (is_kaio(ch) && kairanked(victim) && ch->kairank <= victim->kairank) {
    if ((float)ch->exp / victim->exp > 5) {
      ch_printf(ch, "You are 5 times stronger.\n\r");
      return false;
    }
    return true;
  }
  if (is_demon(ch) && demonranked(victim)) {
    if ((float)ch->exp / victim->exp > 5) {
      ch_printf(ch, "You are 5 times stronger.\n\r");
      return false;
    }
    return true;
  }
  if (kairanked(ch) && demonranked(victim)) {
    if ((float)ch->exp / victim->exp > 5) {
      ch_printf(ch, "You are 5 times stronger.\n\r");
      return false;
    }
    return true;
  }
  if (demonranked(ch) && kairanked(victim)) {
    if ((float)ch->exp / victim->exp > 5) {
      ch_printf(ch, "You are 5 times stronger.\n\r");
      return false;
    }
    return true;
  }
  if (!xIS_SET(victim->act, PLR_PK1) && !xIS_SET(victim->act, PLR_PK2) && !xIS_SET(victim->act, PLR_WAR1) && !xIS_SET(victim->act, PLR_WAR2) && !IS_HC(victim)) {
    send_to_char("You can't kill someone without a PK flag.\n\r",
                 ch);
    return false;
  }
  return true;
}

/*
 * Check to see if player's attacks are (still?) suppressed
 * #ifdef TRI
 */
bool is_attack_supressed(CHAR_DATA *ch) {
  TIMER *timer;

  if (IS_NPC(ch))
    return false;

  timer = get_timerptr(ch, TIMER_ASUPRESSED);

  if (!timer)
    timer = get_timerptr(ch, TIMER_DELAY);

  if (!timer)
    return false;

  /*
   * perma-supression -- bard? (can be reset at end of fight, or
   * spell, etc)
   */
  if (timer->value == -1)
    return true;

  /* this is for timed supressions */
  if (timer->count >= 1)
    return true;

  return false;
}

/*
 * Check to see if weapon is poisoned.
 */
bool is_wielding_poisoned(CHAR_DATA *ch) {
  OBJ_DATA *obj;

  if (!used_weapon)
    return false;

  if ((obj = get_eq_char(ch, WEAR_WIELD)) != NULL && used_weapon == obj && IS_OBJ_STAT(obj, ITEM_POISONED))
    return true;
  if ((obj = get_eq_char(ch, WEAR_DUAL_WIELD)) != NULL && used_weapon == obj && IS_OBJ_STAT(obj, ITEM_POISONED))
    return true;

  return false;
}

/*
 * hunting, hating and fearing code				-Thoric
 */
bool is_hunting(CHAR_DATA *ch, CHAR_DATA *victim) {
  if (!ch->hunting || ch->hunting->who != victim)
    return false;

  return true;
}

bool is_hating(CHAR_DATA *ch, CHAR_DATA *victim) {
  if (!ch->hating || ch->hating->who != victim)
    return false;

  return true;
}

bool is_fearing(CHAR_DATA *ch, CHAR_DATA *victim) {
  if (!ch->fearing || ch->fearing->who != victim)
    return false;

  return true;
}

void stop_hunting(CHAR_DATA *ch) {
  if (ch->hunting) {
    STRFREE(ch->hunting->name);
    DISPOSE(ch->hunting);
    ch->hunting = NULL;
  }
  return;
}

void stop_hating(CHAR_DATA *ch) {
  if (ch->hating) {
    STRFREE(ch->hating->name);
    DISPOSE(ch->hating);
    ch->hating = NULL;
  }
  return;
}

void stop_fearing(CHAR_DATA *ch) {
  if (ch->fearing) {
    STRFREE(ch->fearing->name);
    DISPOSE(ch->fearing);
    ch->fearing = NULL;
  }
  return;
}

void start_hunting(CHAR_DATA *ch, CHAR_DATA *victim) {
  if (ch->hunting)
    stop_hunting(ch);

  CREATE(ch->hunting, HHF_DATA, 1);
  ch->hunting->name = QUICKLINK(victim->name);
  ch->hunting->who = victim;
  return;
}

void start_hating(CHAR_DATA *ch, CHAR_DATA *victim) {
  if (ch->hating)
    stop_hating(ch);

  CREATE(ch->hating, HHF_DATA, 1);
  ch->hating->name = QUICKLINK(victim->name);
  ch->hating->who = victim;
  return;
}

void start_fearing(CHAR_DATA *ch, CHAR_DATA *victim) {
  if (ch->fearing)
    stop_fearing(ch);

  CREATE(ch->fearing, HHF_DATA, 1);
  ch->fearing->name = QUICKLINK(victim->name);
  ch->fearing->who = victim;
  return;
}

int max_fight(CHAR_DATA *ch) {
  return 8;
}

/*
 * Control the fights going on.
 * Called periodically by update_handler.
 * Many hours spent fixing bugs in here by Thoric, as noted by residual
 * debugging checks.  If you never get any of these error messages again
 * in your logs... then you can comment out some of the checks without
 * worry.
 *
 * Note:  This function also handles some non-violence updates.
 */
void violence_update(void) {
  char buf[MAX_STRING_LENGTH];
  CHAR_DATA *ch;
  CHAR_DATA *lst_ch;
  CHAR_DATA *victim;
  CHAR_DATA *rch, *rch_next;
  AFFECT_DATA *paf, *paf_next;
  TIMER *timer, *timer_next;
  ch_ret retcode;
  int attacktype, cnt;
  SKILLTYPE *skill;
  static int pulse = 0;
  bool repeat = false;

  lst_ch = NULL;
  pulse = (pulse + 1) % 100;

  for (ch = last_char; ch; lst_ch = ch, ch = gch_prev) {
    set_cur_char(ch);

    if (ch == first_char && ch->prev) {
      bug("ERROR: first_char->prev != NULL, fixing...", 0);
      ch->prev = NULL;
    }
    gch_prev = ch->prev;

    if (gch_prev && gch_prev->next != ch) {
      sprintf(buf,
              "FATAL: violence_update: %s->prev->next doesn't point to ch.",
              ch->name);
      bug(buf, 0);
      bug("Short-cutting here", 0);
      ch->prev = NULL;
      gch_prev = NULL;
      do_shout(ch, "Goku says, 'Prepare for the worst!'");
    }
    /*
     * See if we got a pointer to someone who recently died...
     * if so, either the pointer is bad... or it's a player who
     * "died", and is back at the healer...
     * Since he/she's in the char_list, it's likely to be the later...
     * and should not already be in another fight already
     */
    if (char_died(ch))
      continue;

    /*
     * See if we got a pointer to some bad looking data...
     */
    if (!ch->in_room || !ch->name) {
      log_string("violence_update: bad ch record!  (Shortcutting.)");
      sprintf(buf,
              "ch: %d  ch->in_room: %d  ch->prev: %d  ch->next: %d",
              (int)ch, (int)ch->in_room, (int)ch->prev,
              (int)ch->next);
      log_string(buf);
      log_string(lastplayercmd);
      if (lst_ch)
        sprintf(buf,
                "lst_ch: %d  lst_ch->prev: %d  lst_ch->next: %d",
                (int)lst_ch, (int)lst_ch->prev,
                (int)lst_ch->next);
      else
        strcpy(buf, "lst_ch: NULL");
      log_string(buf);
      gch_prev = NULL;
      continue;
    }
    for (timer = ch->first_timer; timer; timer = timer_next) {
      timer_next = timer->next;
      if (--timer->count <= 0) {
        if (timer->type == TIMER_ASUPRESSED) {
          if (timer->value == -1) {
            timer->count = 1000;
            continue;
          }
        }
        if (timer->type == TIMER_NUISANCE) {
          DISPOSE(ch->pcdata->nuisance);
        }
        if (timer->type == TIMER_DO_FUN) {
          int tempsub;

          tempsub = ch->substate;
          ch->substate = timer->value;
          (timer->do_fun)(ch, "");
          if (char_died(ch))
            break;
          if (ch->substate != timer->value && ch->substate != SUB_NONE)
            repeat = true;
          ch->substate = tempsub;
        }
        if (timer->type == TIMER_DELAY) {
          if (ch->delay_vict)
            rage(ch, ch->delay_vict);
          if (char_died(ch))
            break;
        }
        if (!repeat)
          extract_timer(ch, timer);
      }
    }

    /* Goku's timer function */
    if (ch->timerDelay) {
      ch->timerDelay--;
      if (ch->timerDelay <= 0) {
        ch->timerDelay = 0;
        ch->timerType = 0;
        if (ch->timerDo_fun)
          (ch->timerDo_fun)(ch, "");
        if (ch->timerType == 0)
          ch->timerDo_fun = NULL;
      }
    }
    if (char_died(ch))
      continue;

    /*
     * We need spells that have shorter durations than an hour.
     * So a melee round sounds good to me... -Thoric
     */
    for (paf = ch->first_affect; paf; paf = paf_next) {
      paf_next = paf->next;
      if (paf->duration > 0)
        paf->duration--;
      else if (paf->duration < 0)
        ;
      else {
        if (!paf_next || paf_next->type != paf->type || paf_next->duration > 0) {
          skill = get_skilltype(paf->type);
          if (paf->type > 0 && skill && skill->msg_off) {
            set_char_color(AT_WEAROFF, ch);
            send_to_char(skill->msg_off,
                         ch);
            send_to_char("\n\r", ch);
          }
        }
        affect_remove(ch, paf);
      }
    }
	/*Make sure 'powerup push' doesn't run unchecked and kill people, and make sure you can't suppress to more than you could power up to*/
	if (!xIS_SET((ch)->affected_by, AFF_SAFEMAX) && !IS_NPC(ch)) {
	  ch->releasepl = ch->truepl;
	  ch->pushpowerup = 0;
	}
	if (xIS_SET((ch)->affected_by, AFF_SAFEMAX) && !IS_NPC(ch)) {
	  ch->releasepl = ch->pl;
	}
	if (!IS_NPC(ch)) {
	  /*truePL formula*/
	  long double skillfloat = 0.00000;
	  long double powerupfloat = 0.00000;
	  long double biomassfloat = 0.00000;
	  
	  skillfloat = ((float)(ch->energymastery + ch->strikemastery) / 10000);
	  powerupfloat = ((float)ch->masterypowerup / 5000);
	  biomassfloat = ((float)ch->biomass / 4000);
	  if (!is_bio(ch))
		ch->truepl = (long double)(ch->exp * (1 + (powerupfloat + skillfloat)));
	  else
		ch->truepl = (long double)(ch->exp * (1 + (biomassfloat + skillfloat)));
	  /*Saiyan Rage racial mods*/
	  if (is_saiyan(ch) || is_hb(ch)) {
		double maxrage = (((double) ch->max_hit / ch->hit) * 10);
		if (maxrage > 50)
			maxrage = 50;
		if (ch->position == POS_FIGHTING || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK || ch->position == POS_DEFENSIVE || ch->position == POS_EVASIVE) {
		  if ((ch->saiyanrage + 1) < maxrage)
		    ch->saiyanrage += 1;
		  else
			ch->saiyanrage = maxrage;
		  pager_printf(ch, "&wDEBUG: saiyanrage is now %d.\n\r", ch->saiyanrage);
		  int ragechance = number_range(1, 4);
		  int fakeout = number_range(1, 100);
		  if (ch->saiyanrage >= 10 && ch->saiyanrage < 20 && ragechance == 4) {
			act(AT_RED, "You grit your teeth, struggling to control your rage!", ch, NULL, NULL, TO_CHAR);
			act(AT_RED, "$n grits $s teeth, struggling to control $s rage!", ch, NULL, NULL, TO_NOTVICT);
			if (fakeout == 100) {
			  act(AT_GREEN, "Your eyes briefly flash a light shade of green.", ch, NULL, NULL, TO_CHAR);
			  act(AT_GREEN, "$n's eyes briefly flash a light shade of green.", ch, NULL, NULL, TO_NOTVICT);
			}
		  }
		  else if (ch->saiyanrage >= 20 && ch->saiyanrage < 40 && ragechance == 4) {
			act(AT_RED, "Your rage consumes you!", ch, NULL, NULL, TO_CHAR);
			act(AT_RED, "$n's rage consumes $m!", ch, NULL, NULL, TO_NOTVICT);
			if (fakeout == 100) {
			  act(AT_LBLUE, "A bolt of lightning erupts suddenly around you!", ch, NULL, NULL, TO_CHAR);
			  act(AT_LBLUE, "A bolt of lightning erupts suddenly around $n!", ch, NULL, NULL, TO_NOTVICT);
			}
		  }
		  else if (ch->saiyanrage >= 40 && ragechance == 4) {
			act(AT_RED, "You howl in uncontrollable rage!", ch, NULL, NULL, TO_CHAR);
			act(AT_RED, "$n howls in uncontrollable rage!", ch, NULL, NULL, TO_NOTVICT);
			if (fakeout == 100) {
			  act(AT_YELLOW, "Your hair flashes golden blonde but quickly returns to normal.", ch, NULL, NULL, TO_CHAR);
			  act(AT_YELLOW, "$n's hair flashes golden blonde but quickly returns to normal.", ch, NULL, NULL, TO_NOTVICT);
			}
		  }
		}
		else
		  ch->saiyanrage -= 5;
		if (ch->saiyanrage < 0)
		  ch->saiyanrage = 0;
	  }
	}
	if (!xIS_SET((ch)->affected_by, AFF_KAIOKEN) && !IS_NPC(ch)) {
	  if (ch->skillkaioken != 0 && ch->skillkaioken > 1)
		ch->skillkaioken = 1;
	}
    /* Transformation Update */
	if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_THIN_TRANS)) {
      int form_mastery = 0;
      int majintotal = 0;

      form_mastery = (ch->masterymajin / 90000);
      majintotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));

      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_THIN_TRANS);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting)
		ch->masterymajin += 9;
      if (ch->desc && ch->fighting)
		ch->masterymajin += 27;
	}
	if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SUPER_TRANS)) {
      int form_mastery = 0;
      int majintotal = 0;

      form_mastery = (ch->masterymajin / 90000);
      majintotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));

      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SUPER_TRANS);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting)
		ch->masterymajin += 9;
      if (ch->desc && ch->fighting)
		ch->masterymajin += 27;
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_KID_TRANS)) {
      int form_mastery = 0;

      form_mastery = (ch->masterymajin / 90000);
      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_KID_TRANS);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting)
        ch->masterymajin += 9;
      if (ch->desc && ch->fighting)
        ch->masterymajin += 27;
    }
	if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SEMIPERFECT)) {
      int form_mastery = 0;
      int biototal = 0;

      form_mastery = (ch->masterybio / 90000);
      biototal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));

      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SEMIPERFECT);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting)
        ch->masterybio += 9;
	  if (ch->desc && ch->fighting)
        ch->masterybio += 27;
    }
	if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_PERFECT)) {
      int form_mastery = 0;
      int biototal = 0;

      form_mastery = (ch->masterybio / 90000);
      biototal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));

      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_PERFECT);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting)
        ch->masterybio += 9;
	  if (ch->desc && ch->fighting)
        ch->masterybio += 27;
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT)) {
      int form_mastery = 0;

      form_mastery = (ch->masterybio / 90000);
      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_ULTRAPERFECT);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting)
        ch->masterybio += 9;
	  if (ch->desc && ch->fighting)
        ch->masterybio += 27;
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SNAMEK)) {
      int form_drain = 0;
      int form_mastery = 0;
      double plmod = 0;

      form_mastery = (ch->masterynamek / 90000);
      plmod = (ch->pl / ch->truepl);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SNAMEK);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        ch->mana = 0;
        transStatRemove(ch);
        act(AT_WHITE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_WHITE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      if (plmod > 600)
        form_drain = (800 - (form_mastery * 6));
      else if (plmod > 500)
        form_drain = (400 - (form_mastery * 6));
      else if (plmod > 350)
        form_drain = (400 - (form_mastery * 6));
      else if (plmod > 250)
        form_drain = (275 - (form_mastery * 6));
      else if (plmod > 150)
        form_drain = (200 - (form_mastery * 6));
      else if (plmod > 75)
        form_drain = (150 - (form_mastery * 6));
      else if (plmod > 50)
        form_drain = (100 - (form_mastery * 6));
      else if (plmod > 35)
        form_drain = (75 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masterynamek += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masterynamek += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_MYSTIC)) {
      int form_drain = 0;
      int form_mastery = 0;
      double plmod = 0;

      form_mastery = (ch->masterymystic / 90000);
      plmod = (ch->pl / ch->exp);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_MYSTIC);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_WHITE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_WHITE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      if (plmod > 600)
        form_drain = (800 - (form_mastery * 6));
      else if (plmod > 500)
        form_drain = (400 - (form_mastery * 6));
      else if (plmod > 350)
        form_drain = (400 - (form_mastery * 6));
      else if (plmod > 250)
        form_drain = (275 - (form_mastery * 6));
      else if (plmod > 150)
        form_drain = (200 - (form_mastery * 6));
      else if (plmod > 75)
        form_drain = (150 - (form_mastery * 6));
      else if (plmod > 50)
        form_drain = (100 - (form_mastery * 6));
      else if (plmod > 35)
        form_drain = (75 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masterymystic += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masterymystic += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_ICER2)) {
      int form_mastery = 0;
      int icertotal = 0;

      form_mastery = (ch->masteryicer / 90000);
      icertotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));

      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting) {
        if (icertotal < 1000000) {
          if (form_mastery > 15) {
            ch->masteryicer += 1;
          } else {
            ch->masteryicer += 9;
          }
        } else if (icertotal >= 1000000) {
          ch->masteryicer += 9;
        }
      }
      if (ch->desc && ch->fighting) {
        if (icertotal < 1000000) {
          if (form_mastery > 15) {
            ch->masteryicer += 1;
          } else {
            ch->masteryicer += 27;
          }
        } else if (icertotal >= 1000000) {
          ch->masteryicer += 27;
        }
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_ICER3)) {
      int form_mastery = 0;
      int icertotal = 0;

      form_mastery = (ch->masteryicer / 90000);
      icertotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));

      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting) {
        if (icertotal < 1000000) {
          if (form_mastery > 15) {
            ch->masteryicer += 1;
          } else {
            ch->masteryicer += 9;
          }
        } else if (icertotal >= 1000000) {
          ch->masteryicer += 9;
        }
      }
      if (ch->desc && ch->fighting) {
        if (icertotal < 1000000) {
          if (form_mastery > 15) {
            ch->masteryicer += 1;
          } else {
            ch->masteryicer += 27;
          }
        } else if (icertotal >= 1000000) {
          ch->masteryicer += 27;
        }
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_ICER4)) {
      int form_mastery = 0;

      form_mastery = (ch->masteryicer / 90000);
      if (form_mastery < 1)
        form_mastery = 1;

      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (ch->desc && !ch->fighting) {
        ch->masteryicer += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryicer += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_ICER5)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryicer / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (600 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryicer += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryicer += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_GOLDENFORM)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryicer / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_GOLDENFORM);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        act(AT_PURPLE, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_PURPLE, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (800 - (form_mastery * 6));
      if (form_drain < 400)
        form_drain = 400;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryicer += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryicer += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && !xIS_SET((ch)->affected_by, AFF_USSJ) && !xIS_SET((ch)->affected_by, AFF_USSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_YELLOW, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_YELLOW, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (100 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && !xIS_SET((ch)->affected_by, AFF_USSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_YELLOW, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_YELLOW, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (150 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_YELLOW, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_YELLOW, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (300 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_YELLOW, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_YELLOW, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (275 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_YELLOW, "You lose control of your ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_YELLOW, "$n loses control of $s ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (500 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ3) && xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_RED, "You lose control of your God Ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_RED, "$n loses control of $s God Ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (400 - (form_mastery * 6));
      if (form_drain < 1)
        form_drain = 1;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    if (!IS_NPC(ch) && xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ3) && xIS_SET((ch)->affected_by, AFF_SSJ4) && xIS_SET((ch)->affected_by, AFF_SGOD)) {
      int form_drain = 0;
      int form_mastery = 0;

      form_mastery = (ch->masteryssj / 90000);
      if (ch->mana <= 0) {
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
        xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
        xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
        xREMOVE_BIT((ch)->affected_by, AFF_SGOD);
        if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
          xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
          xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
        ch->pl = ch->truepl;
        ch->powerup = 0;
        transStatRemove(ch);
        ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
        act(AT_LBLUE, "You lose control of your God Ki and return to normal!", ch, NULL, NULL, TO_CHAR);
        act(AT_LBLUE, "$n loses control of $s God Ki and returns to normal!", ch, NULL, NULL, TO_NOTVICT);
      }
      if (form_mastery < 1)
        form_mastery = 1;
      form_drain = (500 - (form_mastery * 6));
      if (form_drain < 250)
        form_drain = 250;
      ch->mana -= form_drain;
      if (ch->desc && !ch->fighting) {
        ch->masteryssj += 9;
      } else if (ch->desc && ch->fighting) {
        ch->masteryssj += 27;
      }
    }
    /* Bug Guard */
    if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL) && xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      bug("POWERCHANNEL AND SAFEMAX ACTIVE TOGETHER, REMOVED POWERCHANNEL", 0);
      send_to_char("DEBUG: POWERUP AND SAFEMAX CAUGHT TOGETHER, REMOVED POWERCHANNEL\n\r", ch);
    }
    /* New Time-based Powerup */
    if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL) && !xIS_SET((ch)->affected_by, AFF_KAIOKEN) && !xIS_SET((ch)->affected_by, AFF_SAFEMAX) && ch->position >= POS_STANDING) {
      double safemaximum = 0;
      int form_mastery = 0;
      double plmod = 0;
      int auraColor = AT_WHITE;
      int powerupstr = 0;
      int powerupspd = 0;
      int powerupint = 0;
      int powerupcon = 0;

      powerupstr = ch->perm_str * 0.05;
      powerupspd = ch->perm_dex * 0.05;
      powerupint = ch->perm_int * 0.05;
      powerupcon = ch->perm_con * 0.05;
	  
      safemaximum = ((get_curr_int(ch) * 0.03) + 1);
      if (is_saiyan(ch) || is_hb(ch))
        form_mastery = (ch->masteryssj / 90000);
      if (is_icer(ch))
        form_mastery = (ch->masteryicer / 90000);
      if (is_kaio(ch) || is_human(ch))
        form_mastery = (ch->masterymystic / 90000);
      if (is_namek(ch))
        form_mastery = (ch->masterynamek / 90000);
	  if (is_bio(ch))
		form_mastery = (ch->masterybio / 90000);
	  if (is_genie(ch))
		form_mastery = (ch->masterymajin / 90000);
	  plmod = ((float)ch->pl / ch->truepl);
      if (!IS_NPC(ch) && ch->pcdata->auraColorPowerUp > 0)
        auraColor = ch->pcdata->auraColorPowerUp;
      if (form_mastery < 1)
        form_mastery = 1;
      if (ch->position < POS_STANDING) {
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        send_to_char("You must stand if you wish to power up.\n\r", ch);
      }
      if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
        send_to_char("Your energy is too unstable while using Kaioken.\n\r", ch);
      }
      if (is_saiyan(ch) || is_hb(ch)) {
        int onestr = 0;
        int twostr = 0;
        int threestr = 0;
        int fourstr = 0;
        int fivestr = 0;
        int sixstr = 0;
        int sevenstr = 0;
        int onespd = 0;
        int twospd = 0;
        int threespd = 0;
        int fourspd = 0;
        int fivespd = 0;
        int sixspd = 0;
        int sevenspd = 0;
        int oneint = 0;
        int twoint = 0;
        int threeint = 0;
        int fourint = 0;
        int fiveint = 0;
        int sixint = 0;
        int sevenint = 0;
        int onecon = 0;
        int twocon = 0;
        int threecon = 0;
        int fourcon = 0;
        int fivecon = 0;
        int sixcon = 0;
        int sevencon = 0;
		char transbuf[MAX_STRING_LENGTH];
		char transbuf2[MAX_STRING_LENGTH];

        onestr = ch->perm_str * 0.20;
        twostr = ch->perm_str * 0.30;
        threestr = ch->perm_str * 0.50;
        fourstr = ch->perm_str * 0.50;
        fivestr = ch->perm_str * 0.80;
        sixstr = ch->perm_str;
        sevenstr = ch->perm_str * 1.40;
        onespd = ch->perm_dex * 0.20;
        twospd = ch->perm_dex * 0.25;
        threespd = -250;
        fourspd = ch->perm_dex * 0.50;
        fivespd = ch->perm_dex * 0.60;
        sixspd = ch->perm_dex;
        sevenspd = ch->perm_dex * 1.25;
        oneint = ch->perm_int * 0.10;
        twoint = ch->perm_int * 0.10;
        threeint = ch->perm_int * 0.10;
        fourint = ch->perm_int * 0.25;
        fiveint = ch->perm_int * 0.30;
        sixint = ch->perm_int * 0.50;
        sevenint = ch->perm_int;
        onecon = ch->perm_con * 0.20;
        twocon = ch->perm_con * 0.30;
        threecon = ch->perm_con * 0.50;
        fourcon = ch->perm_con * 0.40;
        fivecon = ch->perm_con * 0.70;
        sixcon = ch->perm_con;
        sevencon = ch->perm_con * 1.25;
        if (!xIS_SET((ch)->affected_by, AFF_SSJ)) {
          int saiyanTotal = 0;

          safemaximum = 1 + (ch->masterypowerup / 1000) + (ch->energymastery / 10000) + (ch->strikemastery / 10000);
          saiyanTotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));
          if (ch->powerup < 10) {
			  if ((ch->pl + (long double)((float)ch->truepl / 10)) < ch->truepl) {
				ch->pl += (long double)((float)ch->truepl / 10);
				act(auraColor, "Your inner potential explodes into a display of roaring ki.", ch, NULL, NULL, TO_CHAR);
				act(auraColor, "$n's inner potential explodes into a display of roaring ki.", ch, NULL, NULL, TO_NOTVICT);
			  }
			  else {
				ch->pl = ch->truepl;
				ch->powerup = 10;
			  }
            ch->powerup += 1;
            transStatApply(ch, powerupstr, powerupspd, powerupint, powerupcon);
            if (saiyanTotal >= 1000000 && ch->powerup >= 9 && ch->notransform == 0) {
              xSET_BIT((ch)->affected_by, AFF_SSJ);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
			  if (ch->perm_str >= (ch->perm_int * 4)) {
                act(AT_YELLOW, "You howl in pure rage, an incredible rush of power filling your body.", ch, NULL, NULL, TO_CHAR);
				act(AT_YELLOW, "Your hair stands on end, and a golden aura tinged with flecks of pooling green is unleashed around you!", ch, NULL, NULL, TO_CHAR);
                act(AT_YELLOW, "$n's hair suddenly flashes yellow-green, transcending beyond $s normal limits in a monstrous display of rage!", ch, NULL, NULL, TO_NOTVICT);
				if (ch->altssj == 0) {
				  ch->altssj = 1;
				  sprintf(transbuf, "The atmosphere shifts to an unsettling greenish hue as the world trembles beneath %s's rage!", ch->name);
				  do_info(ch, transbuf);
				  sprintf(transbuf2, "%s has become a Legendary Super Saiyan!", ch->name);
				  do_info(ch, transbuf2);
				}
			  }
			  else {
				act(AT_YELLOW, "Your eyes turn blue, your hair flashes blonde and a fiery golden aura erupts around you!", ch, NULL, NULL, TO_CHAR);
                act(AT_YELLOW, "$n's hair suddenly flashes blonde, transcending beyond $s normal limits in a fiery display of golden ki!", ch, NULL, NULL, TO_NOTVICT);
			  }
              ch->powerup = 0;
              ch->pl = ch->truepl * 50;
              transStatApply(ch, onestr, onespd, oneint, onecon);
              if (!IS_NPC(ch)) {
                ch->pcdata->eyes = 0;
                ch->pcdata->haircolor = 3;
              }
            }
            /*if (plmod >= 20 && plmod < 27) {
              act(auraColor, "Your body is barely visible amidst your vortex of ki.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's body is barely visible amidst $s vortex of ki!", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 15 && plmod < 20) {
              act(auraColor, "Your aura spirals upward, nearly licking the clouds.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's aura spirals upward, nearly licking the clouds.", ch, NULL, NULL, TO_NOTVICT);
            }*/
            /*if (plmod >= 5 && plmod < 10) {
              act(auraColor, "Your aura flickers around you, only faintly visible.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's aura flickers around $m, only faintly visible.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod > 1 && plmod < 5) {
              act(auraColor, "Your body glows faintly.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's body glows faintly.", ch, NULL, NULL, TO_NOTVICT);
            }*/
			/*
            if ((plmod >= 27) && (saiyanTotal < 4000)) {
              ch->pl = (ch->exp * 30);
              act(auraColor, "The raging torrent of ki fades, but your power remains.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's raging torrent of ki fades away, but $s power remains.", ch, NULL, NULL, TO_NOTVICT);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            }*/
          }
          if (ch->powerup >= 10 && saiyanTotal < 1000000) {
            ch->powerup = 10;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "Having reached your limit, you stop powering up. Going any further would be dangerous.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up.", ch, NULL, NULL, TO_NOTVICT);
          }
		  else if (ch->powerup >= 10 && ch->notransform == 1) {
			ch->powerup = 10;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "Having reached your limit, you stop powering up. Going any further would be dangerous.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up.", ch, NULL, NULL, TO_NOTVICT);
		  }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && !xIS_SET((ch)->affected_by, AFF_USSJ) && !xIS_SET((ch)->affected_by, AFF_USSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 52) {
              act(AT_YELLOW, "Your golden aura churns with scattering rays of light.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's golden aura churns with scattering rays of light.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 65) {
              xSET_BIT((ch)->affected_by, AFF_USSJ);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_YELLOW, "Your muscles bulge, and with a sudden burst of power you ascend beyond the reaches of any mere Super Saiyan.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's muscles bulge, drawing on a power beyond that of any mere Super Saiyan.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 75;
              transStatApply(ch, twostr, twospd, twoint, twocon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "Having reached your limit, your burning aura recedes to a gentle blaze.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n's burning, golden aura recedes into a gentle blaze.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && !xIS_SET((ch)->affected_by, AFF_USSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 78) {
              act(AT_YELLOW, "Crackling bolts of energy build in your aura, impure, but powerful as your muscles swell.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "Crackling bolts of impure energy dance in $n's raging aura, $s muscles swelling to incredible sizes.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 100) {
              xSET_BIT((ch)->affected_by, AFF_USSJ2);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_YELLOW, "Your muscles expand to inhuman sizes, engorging yourself with energy!", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's muscles expand to inhuman sizes, engorging $mself with energy!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 150;
              transStatApply(ch, threestr, threespd, threeint, threecon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "You let out a deep breath, your bulging muscles retracting only slightly.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n lets out a deep breath, $s bulging muscles retracting only slightly.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 156) {
              act(AT_YELLOW, "Your giant muscles tremble, emitting waves of tumultuous energy.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's giant muscles tremble, emitting waves of tumultuous energy.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 200) {
              xSET_BIT((ch)->affected_by, AFF_SSJ2);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_YELLOW, "Your muscles shrink, but in an intense explosion of rage your power grows nonetheless, sending arcing bolts of energy from your body.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "Your hair stands further on end as you ascend to the true next level.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's muscles shrink amidst a storm of golden ki. In a sea of crackling, pure energy, $e truly ascends to the next level.", ch, NULL, NULL, TO_NOTVICT);
              act(AT_YELLOW, "$n stares straight ahead with absolute confidence.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 225;
              transStatApply(ch, fourstr, fourspd, fourint, fourcon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "Your giant muscles tense and flex, but no further power comes from it.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n's grossly oversized muscles tense and flex, but $e seems unable to progress any further.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && !xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 234) {
              act(AT_YELLOW, "You howl with fury as your energy builds, constant strikes of crackling energy suffusing the air.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n howls with fury, constant strikes of crackling energy suffusing the air.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 300) {
              xSET_BIT((ch)->affected_by, AFF_SSJ3);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_YELLOW, "An earth-shattering burst of energy expands your aura. Your eyebrows disappear and your hair lengthens, flowing down your back.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "Only the stench of ozone accompanies the countless bolts of energy wreathing your body.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "The world feel as though it could pull apart as $n's aura expands! $s eyebrows disappear slowly and $s hair lengthens, flowing down $s back.", ch, NULL, NULL, TO_NOTVICT);
              act(AT_YELLOW, "When the bright light fades, $n stands within a wreath of countless bolts of energy, unleashing the primal rage of the Saiyan race.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 350;
              transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "Your aura shrinks as you relax, only occasional bursts of energy crackling around you.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n's aura shrinks as $e relaxes, only occasional bursts of energy crackling around $m.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ3) && !xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.02;
            ch->powerup += 1;
            if (plmod > 358) {
              act(AT_YELLOW, "The colours of the world seem to fade away against the brilliant light of your aura.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "The colours of the world seem to fade away against the brilliant light of $n's aura.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 450) {
              xSET_BIT((ch)->affected_by, AFF_SSJ4);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_RED, "Your aura fades and your hair and eyes return to normal. However, in the next instant something inside you changes.", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "Godly Ki radiates from deep within, and with a mighty shout that pierces the heavens, a brilliant red and gold aura encompasses you.", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "Your hair and eyes flash red, tinted subtly with violet as you ascend beyond your mortal restrictions.", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n's hair and eyes return to normal. However, in the next instant something feels very different.", ch, NULL, NULL, TO_NOTVICT);
              act(AT_RED, "$n is encompassed in a massive aura of crimson and gold, $s hair and eyes shifting red with a subtle violet tint.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 500;
              transStatApply(ch, sixstr, sixspd, sixint, sixcon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "Your aura shrinks as you establish control over your primal energy, unable to take it any further.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n's aura shrinks as $e establishes control over $s primal energy.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ3) && xIS_SET((ch)->affected_by, AFF_SSJ4) && !xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.02;
            ch->powerup += 1;
            if (plmod > 511) {
              act(AT_RED, "You stand perfectly calm, your power increasing by the second.", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n stands perfectly calm, and yet with every second $s power grows.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 600) {
              xSET_BIT((ch)->affected_by, AFF_SGOD);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_LBLUE, "Harnessing the secrets of your God Ki, you kindle the flame deep within and surge with newfound power.", ch, NULL, NULL, TO_CHAR);
              act(AT_LBLUE, "Your hair and eyes flash blue, and a brilliant cyan aura erupts around you!", ch, NULL, NULL, TO_CHAR);
              act(AT_LBLUE, "$n's body is swallowed in an intense blue light. What emerges is no mere Super Saiyan.", ch, NULL, NULL, TO_NOTVICT);
              act(AT_LBLUE, "$n's hair and eyes shimmer a deep cyan hue, merging fully with a power beyond mortal ki.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 625;
              transStatApply(ch, sevenstr, sevenspd, sevenint, sevencon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "You shut your eyes, your whole body relaxing.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n relaxes further, emitting subtle pulses of God Ki.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SSJ) && xIS_SET((ch)->affected_by, AFF_USSJ) && xIS_SET((ch)->affected_by, AFF_USSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ2) && xIS_SET((ch)->affected_by, AFF_SSJ3) && xIS_SET((ch)->affected_by, AFF_SSJ4) && xIS_SET((ch)->affected_by, AFF_SGOD)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 644) {
              act(AT_LBLUE, "Countless particles of white light merge with your aura, sending scattering rays of energy in all directions.", ch, NULL, NULL, TO_CHAR);
              act(AT_LBLUE, "Countless particles of white light merge with $n's aura, sending scattering rays of energy in all directions.", ch, NULL, NULL, TO_NOTVICT);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_LBLUE, "Your blue aura of God Ki recedes but your incredible power remains, waiting just beneath the surface.", ch, NULL, NULL, TO_CHAR);
            act(AT_LBLUE, "$n's intense God Ki recedes.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
      }
	  else if (is_bio(ch)) {
        int biototal = 0;
        int onestr = 0;
        int twostr = 0;
        int threestr = 0;
        int fourstr = 0;
        int fivestr = 0;
        int onespd = 0;
        int twospd = 0;
        int threespd = 0;
        int fourspd = 0;
        int fivespd = 0;
        int oneint = 0;
        int twoint = 0;
        int threeint = 0;
        int fourint = 0;
        int fiveint = 0;
        int onecon = 0;
        int twocon = 0;
        int threecon = 0;
        int fourcon = 0;
        int fivecon = 0;

        onestr = ch->perm_str * 0.15;
        twostr = ch->perm_str * 0.25;
        threestr = ch->perm_str * 0.40;
        onespd = ch->perm_dex * 0.15;
        twospd = ch->perm_dex * 0.25;
        threespd = ch->perm_dex * 0.40;
        oneint = ch->perm_int * 0.15;
        twoint = ch->perm_int * 0.25;
        threeint = ch->perm_int * 0.40;
        onecon = ch->perm_con * 0.15;
        twocon = ch->perm_con * 0.25;
        threecon = ch->perm_con * 0.40;
        if (!xIS_SET((ch)->affected_by, AFF_SEMIPERFECT) && !xIS_SET((ch)->affected_by, AFF_PERFECT) && !xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT)) {
          safemaximum = 1 + (ch->gsbiomass);
          biototal = ((ch->strikemastery) + (ch->energymastery) + (ch->biomass));
          if (ch->powerup < 10) {
            if ((ch->pl + (long double)((float)ch->truepl / 10)) < ch->truepl) {
				ch->pl += (long double)((float)ch->truepl / 10);
				act(auraColor, "The essence of your harvested victims churns the air around you!", ch, NULL, NULL, TO_CHAR);
				act(auraColor, "The essence of $n's harvested victims churns the air around $s aura!", ch, NULL, NULL, TO_NOTVICT);
			}
			  else {
				ch->pl = ch->truepl;
				ch->powerup = 10;
			  }
            ch->powerup += 1;
            transStatApply(ch, powerupstr, powerupspd, powerupint, powerupcon);
            if (ch->powerup >= 9 && ch->gsbiomass >= 17) {
              xSET_BIT((ch)->affected_by, AFF_SEMIPERFECT);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_GREEN, "Your body expands in size, shifting to a larger, more humanoid form!", ch, NULL, NULL, TO_CHAR);
			  act(AT_GREEN, "Your tail compresses, shifting upward between your smaller wings.", ch, NULL, NULL, TO_CHAR);
			  act(AT_GREEN, "So close to perfection.", ch, NULL, NULL, TO_CHAR);
              act(AT_GREEN, "$n's body expands in size, shifting to a larger, more humanoid form!", ch, NULL, NULL, TO_NOTVICT);
			  act(AT_GREEN, "$n's wings and tail compress, shrinking along $s upper back.", ch, NULL, NULL, TO_NOTVICT);
			  act(AT_GREEN, "A hideous display.", ch, NULL, NULL, TO_NOTVICT);
              ch->powerup = 0;
              ch->pl = ch->truepl * 60;
              transStatApply(ch, onestr, onespd, oneint, onecon);
            }
          }
          if (ch->powerup >= 10 && ch->gsbiomass < 17) {
            ch->powerup = 10;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "The howling anguish of your victims ceases as you reach your limit. Going any further would be dangerous.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up, the howling anguish of $s victims ceasing.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SEMIPERFECT)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 62) {
              act(AT_YELLOW, "Your large form glows brilliantly, scattering rays of golden light through your aura.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's large form glows brilliantly, $s golden aura radiating with golden light.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 80 && ch->gsbiomass >= 18) {
              xREMOVE_BIT((ch)->affected_by, AFF_SEMIPERFECT);
              xSET_BIT((ch)->affected_by, AFF_PERFECT);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_GREEN, "Your body shines suddenly with a brilliant inner light.", ch, NULL, NULL, TO_CHAR);
              act(AT_GREEN, "Your bulky muscles compress and you adopt an even more humanoid appearance,", ch, NULL, NULL, TO_CHAR);
			  act(AT_GREEN, "compounding the features of your accumulated genetic material.", ch, NULL, NULL, TO_CHAR);
			  act(AT_GREEN, "True perfection.", ch, NULL, NULL, TO_CHAR);
              act(AT_GREEN, "$n erupts into a blinding white light, emerging from the chaos in a more", ch, NULL, NULL, TO_NOTVICT);
			  act(AT_GREEN, "compressed, agile form. Their features are decidedly more humanoid,", ch, NULL, NULL, TO_NOTVICT);
			  act(AT_GREEN, "exuding confidence and an unsettling inner strength.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 125;
              transStatApply(ch, twostr, twospd, twoint, twocon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "You relax, unable to push your power any further.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n relaxes, unable to push $s power any further.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_PERFECT)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 129) {
              act(AT_YELLOW, "Your golden aura churns with radiant light, sending scattering rays surging about you.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n's golden aura churns with radiant light, sending scattering rays surging about them.", ch, NULL, NULL, TO_NOTVICT);
            }
            if ((plmod >= 150) && (ch->gsbiomass >= 19)) {
              xREMOVE_BIT((ch)->affected_by, AFF_PERFECT);
              xSET_BIT((ch)->affected_by, AFF_ULTRAPERFECT);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_GREEN, "Countless bolts of energy arc through your massive aura as it swells suddenly in size!", ch, NULL, NULL, TO_CHAR);
              act(AT_GREEN, "You ascend beyond mere perfection, attaining a level of strength previously unknown.", ch, NULL, NULL, TO_CHAR);
			  act(AT_GREEN, "Absolute perfection.", ch, NULL, NULL, TO_CHAR);
              act(AT_GREEN, "$n ascends beyond mere perfection, sending crackling bolts of energy arcing throughout $s massive aura!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 225;
              transStatApply(ch, threestr, threespd, threeint, threecon);
            } else if ((plmod >= 150) && (ch->gsbiomass = 18)) {
              ch->pl = (ch->truepl * 150);
              act(AT_YELLOW, "Reaching the limits of perfection, your golden aura reduces dramatically in size.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "$n reaches the limits of perfection, unable to force out any more power.", ch, NULL, NULL, TO_NOTVICT);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "You relax your massive aura, unable to increase your perfect power any further.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n relaxes, unable to increase $s perfect power any further.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.06;
            ch->powerup += 1;
            if (plmod > 155) {
              act(AT_YELLOW, "Countless bolts of pure energy dance in scattering arcs around you.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "Countless bolts of pure energy dance in scattering arcs around $n.", ch, NULL, NULL, TO_NOTVICT);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "Reaching the limits of perfection, your golden aura reduces dramatically in size.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n reaches the limits of perfection, unable to force out any more power.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
      }
	  else if (is_genie(ch)) {
        int majintotal = 0;
        int onestr = 0;
        int twostr = 0;
        int threestr = 0;
        int fourstr = 0;
        int fivestr = 0;
        int onespd = 0;
        int twospd = 0;
        int threespd = 0;
        int fourspd = 0;
        int fivespd = 0;
        int oneint = 0;
        int twoint = 0;
        int threeint = 0;
        int fourint = 0;
        int fiveint = 0;
        int onecon = 0;
        int twocon = 0;
        int threecon = 0;
        int fourcon = 0;
        int fivecon = 0;

        onestr = ch->perm_str * 0.25;
        twostr = ch->perm_str * 0.30;
        threestr = ch->perm_str * 0.50;
        onespd = ch->perm_dex * 0.15;
        twospd = ch->perm_dex * 0.25;
        threespd = ch->perm_dex * 0.35;
        oneint = ch->perm_int * 0.15;
        twoint = ch->perm_int * 0.25;
        threeint = ch->perm_int * 0.50;
        onecon = ch->perm_con * 0.20;
        twocon = ch->perm_con * 0.30;
        threecon = ch->perm_con * 0.40;
        if (!xIS_SET((ch)->affected_by, AFF_THIN_TRANS) && !xIS_SET((ch)->affected_by, AFF_SUPER_TRANS) && !xIS_SET((ch)->affected_by, AFF_KID_TRANS)) {
          safemaximum = 1 + (ch->masterypowerup / 1000) + (ch->energymastery / 10000) + (ch->strikemastery / 10000);
          majintotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));
          if (ch->powerup < 10) {
            if ((ch->pl + (long double)((float)ch->truepl / 10)) < ch->truepl) {
				ch->pl += (long double)((float)ch->truepl / 10);
				act(auraColor, "Your power increases, sending gouts of superheated steam into the air around you!", ch, NULL, NULL, TO_CHAR);
				act(auraColor, "$n's power increases, sending gouts of superheated steam into the air around $s!", ch, NULL, NULL, TO_NOTVICT);
			}
			else {
				ch->pl = ch->truepl;
				ch->powerup = 10;
			}
            ch->powerup += 1;
            transStatApply(ch, powerupstr, powerupspd, powerupint, powerupcon);
            if (ch->powerup >= 9 && majintotal >= 1000000) {
              xSET_BIT((ch)->affected_by, AFF_THIN_TRANS);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(auraColor, "You disappear into the sea of superheated steam around you.", ch, NULL, NULL, TO_CHAR);
			  act(auraColor, "When you emerge, the fat stores in your body have been converted entirely into energy!", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n disappears into a sea of superheated steam.", ch, NULL, NULL, TO_NOTVICT);
			  act(auraColor, "When $n emerges, $s fat stores have been converted entirely into energy!", ch, NULL, NULL, TO_NOTVICT);
              ch->powerup = 0;
              ch->pl = ch->truepl * 60;
              transStatApply(ch, onestr, onespd, oneint, onecon);
            }
          }
          if (ch->powerup >= 10 && majintotal < 1000000) {
            ch->powerup = 10;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "You reach your limit, the gouts of superheated air pouring out of your body halting suddenly. Guess you've run out of steam.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up, clearly running out of steam.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_THIN_TRANS)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 62) {
              act(auraColor, "The air churns and sways in the massive sea of steam exuded from your body.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "the air churns and sways around $n, exuding a massive sea of steam from $s body.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 80) {
              xREMOVE_BIT((ch)->affected_by, AFF_THIN_TRANS);
              xSET_BIT((ch)->affected_by, AFF_SUPER_TRANS);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(auraColor, "Your body tenses, and an incredible power wells up from deep within you.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Your excess energy converts into even greater muscle mass, rippling with intensity!", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's body tenses, an incredible power welling up from deep within.", ch, NULL, NULL, TO_NOTVICT);
			  act(auraColor, "$n's excess energy converts into even greater muscle mass, rippling with intensity!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 125;
              transStatApply(ch, twostr, twospd, twoint, twocon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "The roiling steam subsides, unable to increase your power any further.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "The roiling steam around $n subsides, unable to push $s power any further.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SUPER_TRANS)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 129) {
              act(auraColor, "Massive amounts of steam fire outward from your muscular body, churning the air around you.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Massive amounts of steam fire outward from $n's muscular body, churning the air around them.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 150) {
              xREMOVE_BIT((ch)->affected_by, AFF_SUPER_TRANS);
              xSET_BIT((ch)->affected_by, AFF_KID_TRANS);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(auraColor, "A primal feeling wells up from deep within your subconscious.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "In an instant your body shrinks in size, losing several feet in height,", ch, NULL, NULL, TO_CHAR);
			  act(auraColor, "but obtaining a level of power beyond your wildest imagining!", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's body shrinks suddenly, taking an almost child-like appearance.", ch, NULL, NULL, TO_NOTVICT);
			  act(auraColor, "They'd be difficult to take seriously, if not for the tremendous power radiating from within!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 225;
              transStatApply(ch, threestr, threespd, threeint, threecon);
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "The storm of roiling steam around your body relaxes along with your bulging muscles.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "The storm of roiling steam around $n relaxes along with $s bulging muscles.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
	  }
        if (xIS_SET((ch)->affected_by, AFF_KID_TRANS)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.06;
            ch->powerup += 1;
            if (plmod > 155) {
              act(auraColor, "You harness the unsettling, primal energy lurking in the depths of your mind.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n harnesses the unsettling, primal energy lurking in the depths of $s mind.", ch, NULL, NULL, TO_NOTVICT);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "You reach the limits of your primal energy, unable to force out any more power.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches the limits of $s primal energy, unable to force out any more power.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
      }
	  else if (is_icer(ch)) {
        int icertotal = 0;
        int onestr = 0;
        int twostr = 0;
        int threestr = 0;
        int fourstr = 0;
        int fivestr = 0;
        int onespd = 0;
        int twospd = 0;
        int threespd = 0;
        int fourspd = 0;
        int fivespd = 0;
        int oneint = 0;
        int twoint = 0;
        int threeint = 0;
        int fourint = 0;
        int fiveint = 0;
        int onecon = 0;
        int twocon = 0;
        int threecon = 0;
        int fourcon = 0;
        int fivecon = 0;

        onestr = ch->perm_str * 0.10;
        twostr = ch->perm_str * 0.15;
        threestr = ch->perm_str * 0.20;
        fourstr = ch->perm_str * 0.60;
        fivestr = ch->perm_str * 1.20;
        onespd = ch->perm_dex * 0.10;
        twospd = ch->perm_dex * 0.15;
        threespd = ch->perm_dex * 0.20;
        fourspd = ch->perm_dex * 0.50;
        fivespd = ch->perm_dex;
        oneint = ch->perm_int * 0.10;
        twoint = ch->perm_int * 0.10;
        threeint = ch->perm_int * 0.15;
        fourint = ch->perm_int * 0.25;
        fiveint = ch->perm_int;
        onecon = ch->perm_con * 0.15;
        twocon = ch->perm_con * 0.20;
        threecon = ch->perm_con * 0.30;
        fourcon = ch->perm_con * 0.60;
        fivecon = ch->perm_con * 1.40;
        if (!xIS_SET((ch)->affected_by, AFF_ICER2) && !xIS_SET((ch)->affected_by, AFF_ICER3) && !xIS_SET((ch)->affected_by, AFF_ICER4) && !xIS_SET((ch)->affected_by, AFF_ICER5) && !xIS_SET((ch)->affected_by, AFF_GOLDENFORM)) {
          safemaximum = 1 + (ch->masterypowerup / 1000) + (ch->energymastery / 10000) + (ch->strikemastery / 10000);
          if (ch->powerup < 10) {
            if ((ch->pl + (long double)((float)ch->truepl / 10)) < ch->truepl) {
				ch->pl += (long double)((float)ch->truepl / 10);
				act(auraColor, "You glow brightly, hairline fractures appearing across your body.", ch, NULL, NULL, TO_CHAR);
				act(auraColor, "$n glows brightly, hairline fractures appearing across $s body.", ch, NULL, NULL, TO_NOTVICT);
			}
			else {
				ch->pl = ch->truepl;
				ch->powerup = 10;
			}
            ch->powerup += 1;
            transStatApply(ch, powerupstr, powerupspd, powerupint, powerupcon);
            if (ch->masterypowerup >= 1000 && ch->powerup >= 9) {
              xSET_BIT((ch)->affected_by, AFF_ICER2);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_PURPLE, "Your entire body expands monstrously in size, wicked horns sprouting from your head!", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n's entire body expands monstrously in size, wicked horns sprouting from $s head!", ch, NULL, NULL, TO_NOTVICT);
              ch->powerup = 0;
              ch->pl = ch->truepl * 2;
              transStatApply(ch, onestr, onespd, oneint, onecon);
            }
          }
          if (ch->powerup >= 10 && ch->masterypowerup < 1000) {
            ch->powerup = 10;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "The tiny fractures in your body seal as you stop powering up. Going any further would be dangerous.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up, the tiny fractures in $s body closing.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_ICER2)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 2.06) {
              act(AT_PURPLE, "Your giant body glows brilliantly, debris scattering in all directions.", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n's giant body glows brilliantly, debris flying past you.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 2.38) {
              xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
              xSET_BIT((ch)->affected_by, AFF_ICER3);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_PURPLE, "You double forward, chitinous chunks stripping away.", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "Spikes protrude from your back and shoulders as your head elongates, transforming you into a deformed monstrosity!", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n doubles forward, chitinous chunks stripping away from $m as $e transforms into a deformed monstrosity!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 3;
              transStatApply(ch, twostr, twospd, twoint, twocon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_PURPLE, "You relax, your prehensile tail coiling nonchalantly behind you.", ch, NULL, NULL, TO_CHAR);
            act(AT_PURPLE, "$n relaxes, $s tail coiling nonchalantly behind $m.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_ICER3)) {
		  icertotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 3.09) {
              act(AT_PURPLE, "Your chitinous body creaks ominously beneath your raging aura.", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n's chitinous body creaks ominously beneath $s raging aura.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (ch->powerup >= 17 && icertotal > 1000000) {
              xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
              xSET_BIT((ch)->affected_by, AFF_ICER4);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_PURPLE, "In an explosion of ki your body fades away, emerging from the dust in a new form!", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "Your body shrinks to normal size, wicked spikes replaced with smooth skin and patches as reflective as glass.", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n emerges from an explosion of ki, $s body shrinking into a sleek, smooth form.", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 50;
              transStatApply(ch, threestr, threespd, threeint, threecon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_PURPLE, "You hunch forward, unable to increase your power any further.", ch, NULL, NULL, TO_CHAR);
            act(AT_PURPLE, "$n hunches forward, unable to increase $s power further.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_ICER4)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 52) {
              act(AT_PURPLE, "Your arms extend at your sides, a vicious purple aura coursing around you.", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n extends $s arms to the side, a vicious purple aura coursing around $m.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 100) {
              xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
              xSET_BIT((ch)->affected_by, AFF_ICER5);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_PURPLE, "Your muscles expand massively in size, swelling with incredible energy!", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n's muscles expand massively in size, swelling with incredible energy!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 150;
              transStatApply(ch, fourstr, fourspd, fourint, fourcon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_PURPLE, "Chunks of landscape fall back into place as you stop powering up.", ch, NULL, NULL, TO_CHAR);
            act(AT_PURPLE, "Chunks of landscape fall back into place as $n stops powering up.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_ICER5)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            ch->pl *= 1.03;
            ch->powerup += 1;
            if (plmod > 156) {
              act(AT_PURPLE, "Your vicious aura rages uncontrolled, devastating the landscape.", ch, NULL, NULL, TO_CHAR);
              act(AT_PURPLE, "$n's vicious aura rages out of control, devastating the landscape.", ch, NULL, NULL, TO_NOTVICT);
            }
            if (plmod >= 275) {
              xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
              xSET_BIT((ch)->affected_by, AFF_GOLDENFORM);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_YELLOW, "A brilliant golden light overtakes you, traveling up the length of your body.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "Your skin takes on a reflective golden sheen as you ascend into the realm of God Ki.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "A brilliant golden light overtakes $n, traveling up the length of $s body.", ch, NULL, NULL, TO_NOTVICT);
              act(AT_YELLOW, "$s skin takes on a reflective golden sheen as $e ascends into the realm of God Ki!", ch, NULL, NULL, TO_NOTVICT);
              ch->pl = ch->truepl * 380;
              transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "You try to relax, but your bulging muscles ignore the command.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n tries to relax, but $s bulging muscles ignore the command.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_GOLDENFORM)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            if (plmod >= 600) {
              ch->pl *= 1.03;
            } else {
              ch->pl *= 1.04;
            }
            ch->powerup += 1;
            if (plmod > 395) {
              act(AT_YELLOW, "The air roils, an intense pressure building from your glorious golden sheen.", ch, NULL, NULL, TO_CHAR);
              act(AT_YELLOW, "The air roils, an intense pressure building from $n's glorious golden sheen.", ch, NULL, NULL, TO_NOTVICT);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_YELLOW, "Your massive golden aura recedes into your body, leaving only a lustrous glow.", ch, NULL, NULL, TO_CHAR);
            act(AT_YELLOW, "$n's massive golden aura recedes into $s body, leaving only a lustrous glow.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
      }
      if (is_kaio(ch) || is_human(ch)) {
        int onestr = 0;
        int twostr = 0;
        int threestr = 0;
        int fourstr = 0;
        int fivestr = 0;
        int sixstr = 0;
        int sevenstr = 0;
        int onespd = 0;
        int twospd = 0;
        int threespd = 0;
        int fourspd = 0;
        int fivespd = 0;
        int sixspd = 0;
        int sevenspd = 0;
        int oneint = 0;
        int twoint = 0;
        int threeint = 0;
        int fourint = 0;
        int fiveint = 0;
        int sixint = 0;
        int sevenint = 0;
        int onecon = 0;
        int twocon = 0;
        int threecon = 0;
        int fourcon = 0;
        int fivecon = 0;
        int sixcon = 0;
        int sevencon = 0;

        onestr = ch->perm_str * 0.20;
        twostr = ch->perm_str * 0.25;
        threestr = ch->perm_str * 0.30;
        fourstr = ch->perm_str * 0.40;
        fivestr = ch->perm_str * 0.60;
        sixstr = ch->perm_str;
        sevenstr = ch->perm_str * 1.20;
        onespd = ch->perm_dex * 0.10;
        twospd = ch->perm_dex * 0.15;
        threespd = ch->perm_dex * 0.20;
        fourspd = ch->perm_dex * 0.30;
        fivespd = ch->perm_dex * 0.70;
        sixspd = ch->perm_dex;
        sevenspd = ch->perm_dex * 1.20;
        oneint = ch->perm_int * 0.15;
        twoint = ch->perm_int * 0.20;
        threeint = ch->perm_int * 0.30;
        fourint = ch->perm_int * 0.40;
        fiveint = ch->perm_int * 0.70;
        sixint = ch->perm_int * 1.10;
        sevenint = ch->perm_int * 1.30;
        onecon = ch->perm_con * 0.20;
        twocon = ch->perm_con * 0.20;
        threecon = ch->perm_con * 0.30;
        fourcon = ch->perm_con * 0.40;
        fivecon = ch->perm_con * 0.60;
        sixcon = ch->perm_con;
        sevencon = ch->perm_con * 1.25;
        if (!xIS_SET((ch)->affected_by, AFF_MYSTIC)) {
          int mysticTotal = 0;

          mysticTotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));
          safemaximum = 1 + (ch->masterypowerup / 1000) + (ch->energymastery / 10000) + (ch->strikemastery / 10000);
          if (ch->powerup < 10) {
            if ((ch->pl + (long double)((float)ch->truepl / 10)) < ch->truepl) {
				ch->pl += (long double)((float)ch->truepl / 10);
				act(auraColor, "Your gentle aura explodes into a display of roaring ki.", ch, NULL, NULL, TO_CHAR);
				act(auraColor, "$n's gentle aura explodes into a display of roaring ki.", ch, NULL, NULL, TO_NOTVICT);
			}
			else {
				ch->pl = ch->truepl;
				ch->powerup = 10;
			}
            ch->powerup += 1;
            transStatApply(ch, powerupstr, powerupspd, powerupint, powerupcon);
            if (ch->powerup >= 9 && mysticTotal >= 1000000) {
              xSET_BIT((ch)->affected_by, AFF_MYSTIC);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(auraColor, "You cry out as your aura expands, pushing beyond your latent potential!", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n cries out, $s inner potential exploding to the surface!", ch, NULL, NULL, TO_NOTVICT);
              ch->powerup = 0;
              ch->pl = ch->truepl * 35;
              transStatApply(ch, onestr, onespd, oneint, onecon);
            }
          }
          if (ch->powerup >= 10 && mysticTotal < 1000000) {
            ch->powerup = 10;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "Having reached your limit, you stop powering up. Going any further would be dangerous.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_MYSTIC)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            if (plmod < 600) {
              ch->pl *= 1.05;
            } else if (plmod >= 600) {
              ch->pl *= 1.03;
            }
            ch->powerup += 1;
            if (plmod > 600) {
              act(auraColor, "Radiant light suffuses your entire body, cloaking you entirely.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Radiant light suffuses $n's entire body, cloaking $m completely.", ch, NULL, NULL, TO_NOTVICT);
            } else if (plmod > 500) {
              act(auraColor, "Pulses of God Ki emanate deep from within your core!", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Pulses of God Ki emanate deep from within $n's core!", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, sevenstr, sevenspd, sevenint, sevencon);
            } else if (plmod > 350) {
              act(auraColor, "Your aura churns violently, a mysterious ki building deep within.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's aura engulfs $m, churning violently while a mysterious ki seeps from within.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, sixstr, sixspd, sixint, sixcon);
            } else if (plmod > 250) {
              act(auraColor, "Bolts of pure white energy crackle through your body, striking random locations.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Bolts of pure white energy crackle through $n's body, striking random locations.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
            } else if (plmod > 150) {
              act(auraColor, "Your muscles swell with energy, containing power without growing in size.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's muscles swell with energy, containing power without growing in size.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, fourstr, fourspd, fourint, fourcon);
            } else if (plmod > 75) {
              act(auraColor, "Your aura swells to twice its normal size but quickly dies back down.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "$n's aura swells to twice its normal size but quickly dies back down.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, threestr, threespd, threeint, threecon);
            } else if (plmod > 50) {
              act(auraColor, "Massive chunks of rock and debris crumble beneath your aura.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Massive chunks of rock and debris crumble beneath $n's aura.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, twostr, twospd, twoint, twocon);
            } else if (plmod > 37) {
              act(auraColor, "Dust and debris swirl ominously around you.", ch, NULL, NULL, TO_CHAR);
              act(auraColor, "Dust and debris swirl ominously around $n.", ch, NULL, NULL, TO_NOTVICT);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "You reach your limit, tiny bolts of energy dancing between your fingertips.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches the limits of $s potential, tiny bolts of energy dancing between $s fingertips.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
      }
      if (is_namek(ch)) {
        int onestr = 0;
        int twostr = 0;
        int threestr = 0;
        int fourstr = 0;
        int fivestr = 0;
        int sixstr = 0;
        int sevenstr = 0;
        int onespd = 0;
        int twospd = 0;
        int threespd = 0;
        int fourspd = 0;
        int fivespd = 0;
        int sixspd = 0;
        int sevenspd = 0;
        int oneint = 0;
        int twoint = 0;
        int threeint = 0;
        int fourint = 0;
        int fiveint = 0;
        int sixint = 0;
        int sevenint = 0;
        int onecon = 0;
        int twocon = 0;
        int threecon = 0;
        int fourcon = 0;
        int fivecon = 0;
        int sixcon = 0;
        int sevencon = 0;

        onestr = ch->perm_str * 0.20;
        twostr = ch->perm_str * 0.25;
        threestr = ch->perm_str * 0.30;
        fourstr = ch->perm_str * 0.40;
        fivestr = ch->perm_str * 0.50;
        sixstr = ch->perm_str * 0.9;
        sevenstr = ch->perm_str * 1.10;
        onespd = ch->perm_dex * 0.10;
        twospd = ch->perm_dex * 0.15;
        threespd = ch->perm_dex * 0.25;
        fourspd = ch->perm_dex * 0.35;
        fivespd = ch->perm_dex * 0.60;
        sixspd = ch->perm_dex * 0.80;
        sevenspd = ch->perm_dex;
        oneint = ch->perm_int * 0.15;
        twoint = ch->perm_int * 0.25;
        threeint = ch->perm_int * 0.50;
        fourint = ch->perm_int * 0.75;
        fiveint = ch->perm_int;
        sixint = ch->perm_int * 1.25;
        sevenint = ch->perm_int * 1.75;
        onecon = ch->perm_con * 0.20;
        twocon = ch->perm_con * 0.30;
        threecon = ch->perm_con * 0.30;
        fourcon = ch->perm_con * 0.40;
        fivecon = ch->perm_con * 0.60;
        sixcon = ch->perm_con * 0.80;
        sevencon = ch->perm_con * 1.15;
        if (!xIS_SET((ch)->affected_by, AFF_SNAMEK)) {
          int namekTotal = 0;

          namekTotal = ((ch->strikemastery) + (ch->energymastery) + (ch->masterypowerup));
          safemaximum = 1 + (ch->masterypowerup / 1000) + (ch->energymastery / 10000) + (ch->strikemastery / 10000);
          if (ch->powerup < 10) {
            if ((ch->pl + (long double)((float)ch->truepl / 10)) < ch->truepl) {
				ch->pl += (long double)((float)ch->truepl / 10);
				act(auraColor, "Your gentle aura explodes into a display of roaring ki.", ch, NULL, NULL, TO_CHAR);
				act(auraColor, "$n's gentle aura explodes into a display of roaring ki.", ch, NULL, NULL, TO_NOTVICT);
			}
			  else {
				ch->pl = ch->truepl;
				ch->powerup = 10;
			  }
            ch->powerup += 1;
            transStatApply(ch, powerupstr, powerupspd, powerupint, powerupcon);
            if (ch->powerup >= 9 && namekTotal >= 1000000) {
              xSET_BIT((ch)->affected_by, AFF_SNAMEK);
              xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
              act(AT_WHITE, "Your mind opens to the secrets of the ancient Namekians, flooding you with incredible power.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "$n's mind opens to the secrets of the ancient Namekians, flooding $m with incredible power.'", ch, NULL, NULL, TO_NOTVICT);
              ch->powerup = 0;
              ch->pl = ch->truepl * 50;
              transStatApply(ch, onestr, onespd, oneint, onecon);
            }
          }
          if (ch->powerup >= 10 && namekTotal < 1000000) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(auraColor, "Having reached your limit, you stop powering up. Going any further would be dangerous.", ch, NULL, NULL, TO_CHAR);
            act(auraColor, "$n reaches $s limit and stops powering up.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
        if (xIS_SET((ch)->affected_by, AFF_SNAMEK)) {
          safemaximum = form_mastery;
          if (ch->powerup < safemaximum) {
            if (plmod >= 600) {
              ch->pl *= 1.03;
            } else if (plmod >= 400) {
              ch->pl *= 1.04;
            } else if (plmod < 400) {
              ch->pl *= 1.04;
            }
            ch->powerup += 1;
            if (plmod > 600) {
              act(AT_WHITE, "Vague images of your ancestors flash before you, conjured by your brilliant divine aura.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "Vague images of $n's ancestors flash randomly within $s blinding God Ki.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, sevenstr, sevenspd, sevenint, sevencon);
            } else if (plmod > 500) {
              act(AT_WHITE, "A burst of God Ki erupts from deep within your body.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "A burst of God Ki erupts from deep within $n's body.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, sixstr, sixspd, sixint, sixcon);
            } else if (plmod > 350) {
              act(AT_WHITE, "The heavens shake and the earth trembles at your feet.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "The heavens shake and the earth trembles at $n's feet.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
            } else if (plmod > 250) {
              act(AT_WHITE, "Bolts of energy crackle through your blinding aura.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "Bolts of energy crackle through $n's blinding aura.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, fourstr, fourspd, fourint, fourcon);
            } else if (plmod > 150) {
              act(AT_WHITE, "Your aura swirls, constant beams of light radiating from within.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "$n's aura swirls with constant beams of echoing light.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, threestr, threespd, threeint, threecon);
            } else if (plmod > 75) {
              act(AT_WHITE, "Blinding flashes escape your body, echoing through the air.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "Blinding flashes escape $n's body, echoing through the air.", ch, NULL, NULL, TO_NOTVICT);
              transStatApply(ch, twostr, twospd, twoint, twocon);
            } else if (plmod > 50) {
              act(AT_WHITE, "Brilliant white light shrouds the contours of your body.", ch, NULL, NULL, TO_CHAR);
              act(AT_WHITE, "Brilliant white light shrouds the contours of $n's body.", ch, NULL, NULL, TO_NOTVICT);
            }
          }
          if (ch->powerup >= safemaximum) {
            ch->powerup = safemaximum;
            xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
            xSET_BIT((ch)->affected_by, AFF_SAFEMAX);
            act(AT_WHITE, "Having reached your limits, your ancestors can afford you no more power.", ch, NULL, NULL, TO_CHAR);
            act(AT_WHITE, "Having reached $s limits, $n takes a deep breath and stops channeling his ancestors' wisdom.", ch, NULL, NULL, TO_NOTVICT);
          }
        }
      }
      if (!is_saiyan(ch) && !is_hb(ch) && !is_icer(ch) && !is_namek(ch) && !is_human(ch) && !is_kaio(ch) && !is_bio(ch) && !is_genie(ch)) {
        send_to_char("DEBUG: You should only be testing the non-locked races.\n\r", ch);
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL) && !xIS_SET((ch)->affected_by, AFF_KAIOKEN) && xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      double safemaximum = 0;
      int danger = 0;
      int form_mastery = 0;

      if (ch->position < POS_STANDING && ch->position > POS_DEAD) {
        xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
        send_to_char("You must stand if you wish to power up.\n\r", ch);
      }
      form_mastery = (ch->train / 90000);
      if (xIS_SET((ch)->affected_by, AFF_SSJ) || xIS_SET((ch)->affected_by, AFF_SSJ2) || xIS_SET((ch)->affected_by, AFF_SSJ3) || xIS_SET((ch)->affected_by, AFF_SSJ4) || xIS_SET((ch)->affected_by, AFF_SGOD) || xIS_SET((ch)->affected_by, AFF_HYPER) || xIS_SET((ch)->affected_by, AFF_SNAMEK) || xIS_SET((ch)->affected_by, AFF_ICER2) || xIS_SET((ch)->affected_by, AFF_ICER3) || xIS_SET((ch)->affected_by, AFF_ICER4) || xIS_SET((ch)->affected_by, AFF_ICER5) || xIS_SET((ch)->affected_by, AFF_GOLDENFORM) || xIS_SET((ch)->affected_by, AFF_SEMIPERFECT) || xIS_SET((ch)->affected_by, AFF_PERFECT) || xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT) || xIS_SET((ch)->affected_by, AFF_OOZARU) || xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU) || xIS_SET((ch)->affected_by, AFF_EXTREME) || xIS_SET((ch)->affected_by, AFF_MYSTIC) || xIS_SET((ch)->affected_by, AFF_SUPERANDROID) || xIS_SET((ch)->affected_by, AFF_MAKEOSTAR) || xIS_SET((ch)->affected_by, AFF_EVILBOOST) || xIS_SET((ch)->affected_by, AFF_EVILSURGE) || xIS_SET((ch)->affected_by, AFF_EVILOVERLOAD)) {
        danger = ((ch->pushpowerup * 1000) * ch->pushpowerup);
        /* Just in case. */
        if (danger < 1) {
          danger = 1000;
        }
        ch->pl *= 1.01;
        ch->pushpowerup += 1;
        if ((ch->mana - danger) < 0)
          ch->mana = 0;
        else {
          ch->mana -= danger;
        }

        if (danger != 0 && ch->mana == 0) {
          ch->hit -= (danger / 10);
          act(AT_RED, "Your body is ripping itself apart!", ch, NULL, NULL, TO_CHAR);
          act(AT_RED, "$n's body is ripping itself apart!'", ch, NULL, NULL, TO_NOTVICT);
          if (ch->hit - (danger / 10) < 0) {
            ch->hit -= (danger / 10);
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body gives out under the intense strain. All must succumb to their limits in the end.", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body completely spent.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s withers away, succumbing to their limits", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        } else {
          act(AT_YELLOW, "You struggle against the will of your own body, increasing your power at incredible risk.", ch, NULL, NULL, TO_CHAR);
          act(AT_YELLOW, "$n struggles against the will of $s own body, increasing $s power at incredible risk.", ch, NULL, NULL, TO_NOTVICT);
        }
      } else {
        safemaximum = 1;
        danger = ((ch->pushpowerup * 200) * ch->pushpowerup);
        /* Just in case. */
        if (danger < 1) {
          danger = 200;
        }
        ch->pl *= 1.01;
        ch->pushpowerup += 1;
        if ((ch->mana - danger) < 0)
          ch->mana = 0;

        else
          ch->mana -= danger;

        if (danger != 0 && ch->mana == 0) {
          ch->hit -= (danger / 10);
          act(AT_RED, "Your body is ripping itself apart!", ch, NULL, NULL, TO_CHAR);
          act(AT_RED, "$n's body is ripping itself apart!'", ch, NULL, NULL, TO_NOTVICT);
          if (ch->hit - (danger / 10) < 0) {
            ch->hit -= (danger / 10);
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body gives out under the intense strain. All must succumb to their limits in the end.", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n explodes, $s body completely spent!", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s's body explodes in a mass of unbound Ki! Oh the humanity!", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        } else {
          act(AT_YELLOW, "You struggle against the will of your own body, increasing your power at incredible risk.", ch, NULL, NULL, TO_CHAR);
          act(AT_YELLOW, "$n struggles against the will of $s own body, increasing $s power at incredible risk.", ch, NULL, NULL, TO_NOTVICT);
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_POWERUPTRAIN) && ch->position != POS_STANDING) {
      xREMOVE_BIT((ch)->affected_by, AFF_POWERUPTRAIN);
      send_to_char("You lose concentration.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_POWERUPTRAIN) && ch->position == POS_STANDING) {
      int message = 0;
      int drain = 0;

      message = number_range(1, 100);
      drain = number_range(1, 3);

      if (message >= 98) {
        if ((ch->mana - (ch->max_mana * (.02 * drain))) > 0) {
          act(AT_WHITE, "Your body glows faintly as you manage to contain a raging power building within.", ch, NULL, NULL, TO_CHAR);
          act(AT_WHITE, "$n glows very faintly.", ch, NULL, NULL, TO_NOTVICT);
          act(AT_WHITE, "Your Ki Mastery increases.", ch, NULL, NULL, TO_CHAR);
          if (IS_IMMORTAL(ch))
            pager_printf(ch, "&wDEBUG: Powerup Mastery is now %d.\n\r", ch->masterypowerup);
          ch->energymastery += 5;
          ch->masterypowerup += 3;
          ch->mana -= ch->max_mana * (.02 * drain);
          if (ch->energymastery >= (ch->kspgain * 100)) {
            pager_printf_color(ch,
                               "&CYou gained 5 Skill Points!\n\r");
            ch->sptotal += 5;
            ch->kspgain += 1;
          }
        } else {
          ch->mana = 0;
          send_to_char("Exhausted, you lose concentration.\n\r", ch);
          xREMOVE_BIT((ch)->affected_by, AFF_POWERUPTRAIN);
        }
      } else if (message >= 60) {
        if ((ch->mana - (ch->max_mana * (.02 * drain))) > 0) {
          act(AT_WHITE, "You struggle to maintain delicate control over your inner power.", ch, NULL, NULL, TO_CHAR);
          act(AT_WHITE, "$n struggles to maintain delicate control over $s inner power.", ch, NULL, NULL, TO_NOTVICT);
          act(AT_WHITE, "Your Ki Mastery increases slightly.", ch, NULL, NULL, TO_CHAR);
          if (IS_IMMORTAL(ch))
            pager_printf(ch, "&wDEBUG: Powerup Mastery is now %d.\n\r", ch->masterypowerup);
          ch->energymastery += 3;
          ch->masterypowerup += 2;
          ch->mana -= ch->max_mana * (.02 * drain);
          if (ch->energymastery >= (ch->kspgain * 100)) {
            pager_printf_color(ch,
                               "&CYou gained 5 Skill Points!\n\r");
            ch->sptotal += 5;
            ch->kspgain += 1;
          }
        } else {
          ch->mana = 0;
          send_to_char("Exhausted, you lose concentration.\n\r", ch);
          xREMOVE_BIT((ch)->affected_by, AFF_POWERUPTRAIN);
        }
      } else {
        if ((ch->mana - (ch->max_mana * (.02 * drain))) > 0) {
          act(AT_WHITE, "Your Ki Mastery increases slightly.", ch, NULL, NULL, TO_CHAR);
          if (IS_IMMORTAL(ch))
            pager_printf(ch, "&wDEBUG: Powerup Mastery is now %d.\n\r", ch->masterypowerup);
          ch->energymastery += 3;
          ch->masterypowerup += 2;
          ch->mana -= ch->max_mana * (.02 * drain);
          if (ch->energymastery >= (ch->kspgain * 100)) {
            pager_printf_color(ch,
                               "&CYou gained 5 Skill Points!\n\r");
            ch->sptotal += 5;
            ch->kspgain += 1;
          }
        } else {
          ch->mana = 0;
          send_to_char("Exhausted, you lose concentration.\n\r", ch);
          xREMOVE_BIT((ch)->affected_by, AFF_POWERUPTRAIN);
        }
      }
    }
    /* Gravity area/room effects and bonus combat stats, weighted clothing */
    if (!IS_NPC(ch)) {
      double totalrgrav = 0;
      double addedrweight = 0;
      double playerrweight = 0;
      double weighttrainmult = 0;
      double weightstatmult = 0;
      int weightstat = 0;
      int weighttrain = 0;
      int damrange = 0;
      int gravdam = 0;
      int vigormod = 0;
      int weightconspd = 0;

      damrange = number_range(1, 3);

      addedrweight = (double)weightedtraining(ch) / 100000;
      playerrweight = (double)1 + addedrweight;
      totalrgrav = (double)ch->gravSetting * playerrweight;
      weighttrainmult = (double)((double)totalrgrav / 50) + 1;
      weighttrain = 5 * weighttrainmult;
      weightstatmult = (double)((double)totalrgrav / 10) + 1;
      weightstat = 10 * weightstatmult;
      weightconspd = weightstat * 2;

      vigormod = (ch->vigoreffec / 4) + 2;
      if (vigormod < 2)
        vigormod = 2;

      if (totalrgrav > 1) {
        gravdam = damrange * totalrgrav;
      }
      if ((ch->workoutstrain - vigormod) < 0) {
        ch->workoutstrain = 0;
      } else {
        ch->workoutstrain -= vigormod;
        ch->bodymastery += 1;
        if (ch->bodymastery >= (ch->bspgain * 100)) {
          pager_printf_color(ch,
                             "&CYou gained 5 Skill Points!\n\r");
          ch->sptotal += 5;
          ch->bspgain += 1;
        }
      }
      if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV10) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV50) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV100) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV200) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV300) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV400) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV500) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV600) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV700) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV800) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV900) || xIS_SET((ch)->in_room->room_flags, ROOM_GRAV1000)) {
        if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV10)) {
          ch->gravSetting = 10;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV50)) {
          ch->gravSetting = 50;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV100)) {
          ch->gravSetting = 100;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV200)) {
          ch->gravSetting = 200;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV300)) {
          ch->gravSetting = 300;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV400)) {
          ch->gravSetting = 400;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV500)) {
          ch->gravSetting = 500;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV600)) {
          ch->gravSetting = 600;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV700)) {
          ch->gravSetting = 700;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV800)) {
          ch->gravSetting = 800;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV900)) {
          ch->gravSetting = 900;
        } else if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV1000)) {
          ch->gravSetting = 1000;
        }
      } else {
        if (!xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
          ch->gravSetting = 1;
        }
      }
      if (ch->position == POS_FIGHTING || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK || ch->position == POS_DEFENSIVE || ch->position == POS_EVASIVE) {
          int poweruprand = 0;

          poweruprand = number_range(1, 3);
          if (poweruprand == 3)
            ch->masterypowerup += 2;
        stat_train(ch, "str", weightstat);
        stat_train(ch, "spd", weightconspd);
        stat_train(ch, "con", weightconspd);
        stat_train(ch, "int", weightstat);
        ch->train += weighttrain;
        ch->mana -= gravdam;
        if ((ch->mana - gravdam) < 0)
          ch->mana = 0;
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_EXPUSHUPS) && ch->position != POS_RESTING && ch->position != POS_DEAD) {
      xREMOVE_BIT((ch)->affected_by, AFF_EXPUSHUPS);
      send_to_char("Your workout is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_EXSHADOWBOXING) && ch->position != POS_RESTING && ch->position != POS_DEAD) {
      xREMOVE_BIT((ch)->affected_by, AFF_EXSHADOWBOXING);
      send_to_char("Your workout is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_EXENDURING) && ch->position != POS_RESTING && ch->position != POS_DEAD) {
      xREMOVE_BIT((ch)->affected_by, AFF_EXENDURING);
      send_to_char("Your workout is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_INTEXPUSHUPS) && ch->position != POS_RESTING && ch->position != POS_DEAD) {
      xREMOVE_BIT((ch)->affected_by, AFF_INTEXPUSHUPS);
      send_to_char("Your workout is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_INTEXSHADOWBOXING) && ch->position != POS_RESTING && ch->position != POS_DEAD) {
      xREMOVE_BIT((ch)->affected_by, AFF_INTEXSHADOWBOXING);
      send_to_char("Your workout is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_INTEXENDURING) && ch->position != POS_RESTING && ch->position != POS_DEAD) {
      xREMOVE_BIT((ch)->affected_by, AFF_INTEXENDURING);
      send_to_char("Your workout is cut short.\n\r", ch);
    }
    /* New exercise system */
    if (xIS_SET((ch)->affected_by, AFF_EXPUSHUPS)) {
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_EXPUSHUPS);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int increase = 0;
      int gravdam = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double playerweight = 0;
      double addedweight = 0;
      double totalgrav = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 3);

      addedweight = (double)weightedtraining(ch) / 100000;
      playerweight = (double)1 + addedweight;
      totalgrav = (double)ch->gravSetting * playerweight;

      trainmessage = number_range(1, 100);
      gravdam = totalgrav * damrange;
	  if (totalgrav < 20)
		statbonus = (totalgrav * 5) + 15;
	  else
		statbonus = (totalgrav) + 15;
      breakbonus = statbonus * 2;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "str", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your joints pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou perform a push-up, your strength steadily building.\n\r");
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "str", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GPushing past your normal limits, you perform a series of one-armed push-ups!\n\r");
        act(AT_WHITE, "$n does a set of one-armed pushups in rapid-fire succession.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        exercise_train(ch, "str", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_INTEXPUSHUPS)) {
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_INTEXPUSHUPS);
      if (ch->workoutstrain > 1000) {
        xREMOVE_BIT((ch)->affected_by, AFF_INTEXPUSHUPS);
        pager_printf(ch, "&WRunning out of steam, you take a break from training so intensely.\n\r");
      }

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int increase = 0;
      int gravdam = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double playerweight = 0;
      double addedweight = 0;
      double totalgrav = 0;

      increase = number_range(3, 9);
      damrange = number_range(3, 9);

      addedweight = (double)weightedtraining(ch) / 100000;
      playerweight = (double)1 + addedweight;
      totalgrav = (double)ch->gravSetting * playerweight;

      trainmessage = number_range(1, 100);
      gravdam = totalgrav * damrange;
      if (totalgrav < 20)
		statbonus = (totalgrav * 5) + 15;
	  else
		statbonus = (totalgrav) + 15;
      statbonus = statbonus * 1.5;
      breakbonus = statbonus * 2;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "str", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your joints pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou perform a push-up, your strength steadily building.\n\r");
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "str", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GPushing past your normal limits, you perform a series of one-armed push-ups!\n\r");
        act(AT_WHITE, "$n does a set of one-armed pushups in rapid-fire succession.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        exercise_train(ch, "str", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      ch->workoutstrain += 20;
    }
    if (xIS_SET((ch)->affected_by, AFF_EXSHADOWBOXING)) {
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_EXSHADOWBOXING);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int increase = 0;
      int gravdam = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double playerweight = 0;
      double addedweight = 0;
      double totalgrav = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 3);

      addedweight = (double)weightedtraining(ch) / 100000;
      playerweight = (double)1 + addedweight;
      totalgrav = (double)ch->gravSetting * playerweight;

      trainmessage = number_range(1, 100);
      gravdam = totalgrav * damrange;
      if (totalgrav < 20)
		statbonus = (totalgrav * 5) + 15;
	  else
		statbonus = (totalgrav) + 15;
      breakbonus = statbonus * 2;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "spd", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your joints pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou jab repeatedly at the air, dancing from foot to foot.\n\r");
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "spd", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou explode into an elaborate combo, firing punch after punch through a rush of insight!\n\r");
        act(AT_WHITE, "$n explodes into an elaborate punch-punch combo.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        exercise_train(ch, "spd", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_INTEXSHADOWBOXING)) {
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_INTEXSHADOWBOXING);
      if (ch->workoutstrain > 1000) {
        xREMOVE_BIT((ch)->affected_by, AFF_INTEXSHADOWBOXING);
        pager_printf(ch, "&WRunning out of steam, you take a break from training so intensely.\n\r");
      }

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int increase = 0;
      int gravdam = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double playerweight = 0;
      double addedweight = 0;
      double totalgrav = 0;

      increase = number_range(3, 9);
      damrange = number_range(3, 9);

      addedweight = (double)weightedtraining(ch) / 100000;
      playerweight = (double)1 + addedweight;
      totalgrav = (double)ch->gravSetting * playerweight;

      trainmessage = number_range(1, 100);
      gravdam = totalgrav * damrange;
      if (totalgrav < 20)
		statbonus = (totalgrav * 5) + 15;
	  else
		statbonus = (totalgrav) + 15;
      statbonus = statbonus * 1.5;
      breakbonus = statbonus * 2;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "spd", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your joints pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou jab repeatedly at the air, dancing from foot to foot.\n\r");
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "spd", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou explode into an elaborate combo, firing punch after punch through a rush of insight!\n\r");
        act(AT_WHITE, "$n explodes into an elaborate punch-punch combo.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        exercise_train(ch, "spd", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      ch->workoutstrain += 20;
    }
    if (xIS_SET((ch)->affected_by, AFF_EXENDURING)) {
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_EXENDURING);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int increase = 0;
      int gravdam = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double playerweight = 0;
      double addedweight = 0;
      double totalgrav = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 3);

      addedweight = (double)weightedtraining(ch) / 100000;
      playerweight = (double)1 + addedweight;
      totalgrav = (double)ch->gravSetting * playerweight;

      trainmessage = number_range(1, 100);
      gravdam = totalgrav * damrange;
      if (totalgrav < 20)
		statbonus = (totalgrav * 5) + 15;
	  else
		statbonus = (totalgrav) + 15;
      breakbonus = statbonus * 2;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "con", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your joints pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou feel heavy, struggling to carry your own weight without ki regulation.\n\r");
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "con", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 6000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou push through exhaustion, barely carrying on.\n\r");
        act(AT_WHITE, "$n pushes through exhaustion, barely carrying on.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        exercise_train(ch, "con", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_INTEXENDURING)) {
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_INTEXENDURING);
      if (ch->workoutstrain > 1000) {
        xREMOVE_BIT((ch)->affected_by, AFF_INTEXENDURING);
        pager_printf(ch, "&WRunning out of steam, you take a break from training so intensely.\n\r");
      }

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int increase = 0;
      int gravdam = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double playerweight = 0;
      double addedweight = 0;
      double totalgrav = 0;

      increase = number_range(3, 9);
      damrange = number_range(3, 9);

      addedweight = (double)weightedtraining(ch) / 100000;
      playerweight = (double)1 + addedweight;
      totalgrav = (double)ch->gravSetting * playerweight;

      trainmessage = number_range(1, 100);
      gravdam = totalgrav * damrange;
      if (totalgrav < 20)
		statbonus = (totalgrav * 5) + 15;
	  else
		statbonus = (totalgrav) + 15;
      statbonus = statbonus * 1.5;
      breakbonus = statbonus * 2;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "con", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your joints pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou feel heavy, struggling to carry your own weight without ki regulation.\n\r");
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        exercise_train(ch, "con", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 3000 * totalgrav;
        xp_gain = (long double)base_xp;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou push through exhaustion, barely carrying on.\n\r");
        act(AT_WHITE, "$n pushes through exhaustion, barely carrying on.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        exercise_train(ch, "con", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            ch->exp -= xp_gain;
            if (ch->position == POS_DEAD) {
              if (totalgrav > 1) {
                act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              } else {
                act(AT_RED, "Your body gives out from exhaustion ...", ch, NULL, NULL, TO_CHAR);
                act(AT_RED, "$n collapses, DEAD, $s body completely exhausted.", ch, NULL, NULL, TO_NOTVICT);
                sprintf(buf, "%s's body gives out from exhaustion'", ch->name);
              }
              ch->exp -= (ch->exp * 0.1);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      ch->workoutstrain += 20;
    }
    /* New Gravity Training */
    if (xIS_SET((ch)->affected_by, AFF_PUSHUPS) && !xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      xREMOVE_BIT((ch)->affected_by, AFF_PUSHUPS);
      send_to_char("Your training is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_SHADOWBOXING) && !xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      xREMOVE_BIT((ch)->affected_by, AFF_SHADOWBOXING);
      send_to_char("Your training is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_ENDURING) && !xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      xREMOVE_BIT((ch)->affected_by, AFF_ENDURING);
      send_to_char("Your training is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_MEDITATION) && !xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      xREMOVE_BIT((ch)->affected_by, AFF_MEDITATION);
      send_to_char("Your training is cut short.\n\r", ch);
    }
    if (xIS_SET((ch)->affected_by, AFF_PUSHUPS) && xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      // check if position has changed to not standing
      if (ch->position < POS_STANDING && ch->position != POS_DEAD) {
        xREMOVE_BIT((ch)->affected_by, AFF_PUSHUPS);
        send_to_char("You cannot gravity train in such a state.\n\r", ch);
      }
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_PUSHUPS);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int gravLevel = 0;
      int increase = 0;
      int safegrav = 0;
      int gravdam = 0;
      int safediff = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double overacc_bonus = 0;
      long double xp_bonus = 0;
      int acc = 0;
      int gravres = 0;
      int resdiff = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 6);
      gravres = (get_curr_con(ch) / 500) + 1;

      acc = ((ch->gravExp / 1000) + 1);
      if (acc < 1)
        acc = 1;
      trainmessage = number_range(1, 100);
      safediff = ((ch->gravSetting - acc) + 2);
      if (safediff < 1)
        safediff = 1;
      resdiff = (safediff - gravres);
      if (resdiff < 1)
        resdiff = 1;
      gravdam = (pow(resdiff, 3) * damrange);
      gravLevel = ch->gravSetting;
      statbonus = safediff + 15;
      breakbonus = statbonus * 2;
      overacc_bonus = 0.1 * safediff;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "str", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou perform a push-up in %d times gravity, your strength steadily building.\n\r", gravLevel);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "str", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 120 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        pager_printf(ch, "&GPushing past your normal limits, you perform a series of one-armed push-ups!\n\r", gravLevel);
        act(AT_WHITE, "$n does a set of one-armed pushups in rapid-fire succession.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        stat_train(ch, "str", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_SHADOWBOXING) && xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      // check if positioning has changed to not standing
      if (ch->position < POS_STANDING && ch->position != POS_DEAD) {
        xREMOVE_BIT((ch)->affected_by, AFF_SHADOWBOXING);
        send_to_char("You cannot gravity train in such a lax state.\n\r", ch);
      }
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_SHADOWBOXING);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int gravLevel = 0;
      int increase = 0;
      int safegrav = 0;
      int gravdam = 0;
      int safediff = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double overacc_bonus = 0;
      long double xp_bonus = 0;
      int acc = 0;
      int gravres = 0;
      int resdiff = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 6);
      gravres = (get_curr_con(ch) / 500) + 1;

      acc = ((ch->gravExp / 1000) + 1);
      if (acc < 1)
        acc = 1;
      trainmessage = number_range(1, 100);
      safediff = ((ch->gravSetting - acc) + 2);
      if (safediff < 1)
        safediff = 1;
      resdiff = (safediff - gravres);
      if (resdiff < 1)
        resdiff = 1;
      gravdam = (pow(resdiff, 3) * damrange);
      gravLevel = ch->gravSetting;
      statbonus = safediff + 15;
      breakbonus = statbonus * 2;
      overacc_bonus = 0.1 * safediff;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "spd", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou perform a combo in %d times gravity while skillfully dodging from side to side.\n\r", gravLevel);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "spd", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 120 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        pager_printf(ch, "&GBreathing deeply, you throw all of your energy into a vicious jab-cross-hook-knee combination!\n\r", gravLevel);
        act(AT_WHITE, "$n drives forward with a long combo, bouncing light on $s feet.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        stat_train(ch, "spd", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_ENDURING) && xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      // check if position is no longer standing
      if (ch->position < POS_STANDING && ch->position != POS_DEAD) {
        xREMOVE_BIT((ch)->affected_by, AFF_ENDURING);
        send_to_char("You cannot gravity train in such a lax state.\n\r", ch);
      }
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_ENDURING);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int gravLevel = 0;
      int increase = 0;
      int safegrav = 0;
      int gravdam = 0;
      int safediff = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double overacc_bonus = 0;
      long double xp_bonus = 0;
      int acc = 0;
      int gravres = 0;
      int resdiff = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 6);
      gravres = (get_curr_con(ch) / 500) + 1;

      acc = ((ch->gravExp / 1000) + 1);
      if (acc < 1)
        acc = 1;
      trainmessage = number_range(1, 100);
      safediff = ((ch->gravSetting - acc) + 2);
      if (safediff < 1)
        safediff = 1;
      resdiff = (safediff - gravres);
      if (resdiff < 1)
        resdiff = 1;
      gravdam = (pow(resdiff, 3) * damrange);
      gravLevel = ch->gravSetting;
      statbonus = safediff + 15;
      breakbonus = statbonus * 2;
      overacc_bonus = 0.1 * safediff;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "con", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou crank up the dial well beyond %d times gravity, fighting just to stay on your feet.\n\r", gravLevel);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "con", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 120 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        pager_printf(ch, "&GYou push the dial even further, gritting your teeth and nearly crushing your body to the floor!\n\r", gravLevel);
        act(AT_WHITE, "$n endures a level of gravity far beyond their normal limits, fighting just to stay alive!", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        stat_train(ch, "con", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_MEDITATION) && xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
      // check if position is no longer standing
      if (ch->position < POS_STANDING && ch->position != POS_DEAD) {
        xREMOVE_BIT((ch)->affected_by, AFF_MEDITATION);
        send_to_char("You cannot gravity train in such a lax state.\n\r", ch);
      }
      if (!ch->desc)
        xREMOVE_BIT((ch)->affected_by, AFF_MEDITATION);

      char buf[MAX_STRING_LENGTH];
      int trainmessage = 0;
      long double xp_gain = 0;
      long double base_xp = 0;
      int gravLevel = 0;
      int increase = 0;
      int safegrav = 0;
      int gravdam = 0;
      int safediff = 0;
      int damrange = 0;
      int statbonus = 0;
      int breakbonus = 0;
      double overacc_bonus = 0;
      long double xp_bonus = 0;
      int acc = 0;
      int gravres = 0;
      int resdiff = 0;

      increase = number_range(1, 3);
      damrange = number_range(1, 6);
      gravres = (get_curr_con(ch) / 500) + 1;

      acc = ((ch->gravExp / 1000) + 1);
      if (acc < 1)
        acc = 1;
      trainmessage = number_range(1, 100);
      safediff = ((ch->gravSetting - acc) + 2);
      if (safediff < 1)
        safediff = 1;
      resdiff = (safediff - gravres);
      if (resdiff < 1)
        resdiff = 1;
      gravdam = (pow(resdiff, 3) * damrange);
      gravLevel = ch->gravSetting;
      statbonus = safediff + 15;
      breakbonus = statbonus * 2;
      overacc_bonus = 0.1 * safediff;

      if (trainmessage < 65) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "int", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 65 && trainmessage < 99) {
        base_xp = (long double)increase / 240 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        pager_printf(ch, "&GYou focus your mind's eye in %d times gravity, shutting all else out with extreme calm.\n\r", gravLevel);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        if (xp_gain > 1) {
          sprintf(buf, "Your power level increases by %s points.", num_punct(xp_gain));
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        } else {
          sprintf(buf, "Your power level increases very slightly.", NULL);
          act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        }
        stat_train(ch, "int", statbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
      if (trainmessage >= 99) {
        base_xp = (long double)increase / 120 * gravLevel;
        xp_bonus = (long double)base_xp * overacc_bonus;
        xp_gain = (long double)base_xp + xp_bonus;
        gain_exp(ch, xp_gain);
        if (safediff > 1)
          ch->gravExp += (increase + safediff);
        pager_printf(ch, "&GYour mind clears completely and you momentarily achieve an overwhelming sense of inner peace.\n\r", gravLevel);
        act(AT_WHITE, "$n sits in peaceful meditation, displaying absolutely no outward emotion.", ch, NULL, NULL, TO_NOTVICT);
        sprintf(buf, "Your power level suddenly increases by %s points.", num_punct(xp_gain));
        act(AT_HIT, buf, ch, NULL, NULL, TO_CHAR);
        stat_train(ch, "int", breakbonus);
        if (ch->mana - gravdam > 0)
          ch->mana -= gravdam;
        else if (ch->mana - gravdam <= 0) {
          ch->mana = 0;
          ch->hit -= (gravdam * 10);
          act(AT_RED, "Your bones pop and creak ominously.", ch, NULL, NULL, TO_CHAR);
          if (ch->hit - (gravdam * 10) < 0) {
            update_pos(ch);
            if (ch->position == POS_DEAD) {
              act(AT_RED, "Your body has been crushed!", ch, NULL, NULL, TO_CHAR);
              act(AT_RED, "$n collapses, DEAD, $s body crushed under intense gravity.", ch, NULL, NULL, TO_NOTVICT);
              sprintf(buf, "%s is crushed into a pancake under intense gravity", ch->name);
              do_info(ch, buf);
              raw_kill(ch, ch);
            }
          }
        }
      }
    }
    /* Kaioken tick effects */
    if (xIS_SET((ch)->affected_by, AFF_KAIOKEN) && !IS_NPC(ch) && ch->desc) {
      kaioken_drain(ch);
	  stat_train(ch, "con", ch->skillkaioken);
	  ch->masterypowerup += (ch->skillkaioken / 2);
    }
    if (char_died(ch))
      continue;

    /* check for exits moving players around */
    if (pullcheck(ch, pulse) == rCHAR_DIED || char_died(ch))
      continue;

    /* so people charging attacks stop fighting */
    if (ch->charge > 0)
      continue;

    /* Let the battle begin! */

    if ((victim = who_fighting(ch)) == NULL || IS_AFFECTED(ch, AFF_PARALYSIS))
      continue;

    sysdata.outBytesFlag = LOGBOUTCOMBAT;

    int foc = 0;

    foc =
        URANGE(0,
               ((number_range(1, UMAX(1, get_curr_int(ch))) / 5)),
               get_curr_int(ch));

    if (!IS_NPC(ch)) {
      int div = 0;

      if (ch->skilldodge >= 0 &&
          !IS_SET(ch->pcdata->combatFlags, CMB_NO_DODGE))
        div += 5;
      if (ch->skillblock >= 0 &&
          !IS_SET(ch->pcdata->combatFlags, CMB_NO_BLOCK))
        div += 5;
      if (ch->skilldcd >= 0 &&
          !IS_SET(ch->pcdata->combatFlags, CMB_NO_DCD))
        div += 40;

      if (div > 0) {
        foc = (foc - ((foc / 100) * div));
      }
    }
    if (foc < 0)
      foc = 0;

    ch->focus += foc;
    ch->focus = URANGE(0, ch->focus, get_curr_int(ch));

    retcode = rNONE;

    if (xIS_SET(ch->in_room->room_flags, ROOM_SAFE)) {
      sprintf(buf,
              "violence_update: %s fighting %s in a SAFE room.",
              ch->name, victim->name);
      log_string(buf);
      stop_fighting(ch, true);
    } else if (IS_AWAKE(ch) && ch->in_room == victim->in_room)
      retcode = multi_hit(ch, victim, TYPE_UNDEFINED);
    else
      stop_fighting(ch, false);

    if (char_died(ch))
      continue;

    if (retcode == rCHAR_DIED || (victim = who_fighting(ch)) == NULL)
      continue;

    /*
     *  Mob triggers
     *  -- Added some victim death checks, because it IS possible.. -- Alty
     */
    rprog_rfight_trigger(ch);
    if (char_died(ch) || char_died(victim))
      continue;
    mprog_hitprcnt_trigger(ch, victim);
    if (char_died(ch) || char_died(victim))
      continue;
    mprog_fight_trigger(ch, victim);
    if (char_died(ch) || char_died(victim))
      continue;

    /*
     * NPC special attack flags                             -Thoric
     */
    if (IS_NPC(ch)) {
      if (!xIS_EMPTY(ch->attacks)) {
        attacktype = -1;
        if (30 + (ch->level / 4) >= number_percent()) {
          cnt = 0;
          for (;;) {
            if (cnt++ > 10) {
              attacktype = -1;
              break;
            }
            attacktype =
                number_range(7,
                             MAX_ATTACK_TYPE - 1);
            if (xIS_SET(ch->attacks, attacktype))
              break;
          }
          switch (attacktype) {
            case ATCK_BASH:
              do_bash(ch, "");
              retcode = global_retcode;
              break;
            case ATCK_STUN:
              do_stun(ch, "");
              retcode = global_retcode;
              break;
            case ATCK_GOUGE:
              do_gouge(ch, "");
              retcode = global_retcode;
              break;
            case ATCK_FEED:
              do_gouge(ch, "");
              retcode = global_retcode;
              break;
          }
          if (attacktype != -1 && (retcode == rCHAR_DIED || char_died(ch)))
            continue;
        }
      }
      /*
       * NPC special defense flags                                -Thoric
       */
      if (!xIS_EMPTY(ch->defenses)) {
        attacktype = -1;
        if (50 + (ch->level / 4) > number_percent()) {
          cnt = 0;
          for (;;) {
            if (cnt++ > 10) {
              attacktype = -1;
              break;
            }
            attacktype =
                number_range(2,
                             MAX_DEFENSE_TYPE - 1);
            if (xIS_SET(ch->defenses, attacktype))
              break;
          }

          switch (attacktype) {
            case DFND_CURELIGHT:
              /*
               * A few quick checks in the
               * cure ones so that a) less
               * spam and b) we don't have
               * mobs looking stupider
               * than normal by healing
               * themselves when they
               * aren't even being hit
               * (although that doesn't
               * happen TOO often
               */
              if (ch->hit < ch->max_hit) {
                act(AT_MAGIC,
                    "$n mutters a few incantations...and looks a little better.",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("cure light"),
                                ch->level, ch, ch);
              }
              break;
            case DFND_CURESERIOUS:
              if (ch->hit < ch->max_hit) {
                act(AT_MAGIC,
                    "$n mutters a few incantations...and looks a bit better.",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("cure serious"),
                                ch->level, ch, ch);
              }
              break;
            case DFND_CURECRITICAL:
              if (ch->hit < ch->max_hit) {
                act(AT_MAGIC,
                    "$n mutters a few incantations...and looks healthier.",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("cure critical"),
                                ch->level, ch, ch);
              }
              break;
            case DFND_HEAL:
              if (ch->hit < ch->max_hit) {
                act(AT_MAGIC,
                    "$n mutters a few incantations...and looks much healthier.",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("heal"),
                                ch->level, ch, ch);
              }
              break;
              /*
               * Thanks to
               * guppy@wavecomputers.net
               * for catching this
               */
            case DFND_SHOCKSHIELD:
              if (!IS_AFFECTED(ch, AFF_SHOCKSHIELD)) {
                act(AT_MAGIC,
                    "$n utters a few incantations...",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("shockshield"),
                                ch->level, ch, ch);
              } else
                retcode = rNONE;
              break;
            case DFND_VENOMSHIELD:
              if (!IS_AFFECTED(ch, AFF_VENOMSHIELD)) {
                act(AT_MAGIC,
                    "$n utters a few incantations ...",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("venomshield"),
                                ch->level, ch, ch);
              } else
                retcode = rNONE;
              break;
            case DFND_ACIDMIST:
              if (!IS_AFFECTED(ch, AFF_ACIDMIST)) {
                act(AT_MAGIC,
                    "$n utters a few incantations ...",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("acidmist"),
                                ch->level, ch, ch);
              } else
                retcode = rNONE;
              break;
            case DFND_FIRESHIELD:
              if (!IS_AFFECTED(ch, AFF_FIRESHIELD)) {
                act(AT_MAGIC,
                    "$n utters a few incantations...",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("fireshield"),
                                ch->level, ch, ch);
              } else
                retcode = rNONE;
              break;
            case DFND_ICESHIELD:
              if (!IS_AFFECTED(ch, AFF_ICESHIELD)) {
                act(AT_MAGIC,
                    "$n utters a few incantations...",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("iceshield"),
                                ch->level, ch, ch);
              } else
                retcode = rNONE;
              break;
            case DFND_trueSIGHT:
              if (!IS_AFFECTED(ch, AFF_trueSIGHT))
                retcode =
                    spell_smaug(skill_lookup("true"),
                                ch->level, ch, ch);
              else
                retcode = rNONE;
              break;
            case DFND_SANCTUARY:
              if (!IS_AFFECTED(ch, AFF_SANCTUARY)) {
                act(AT_MAGIC,
                    "$n utters a few incantations...",
                    ch, NULL, NULL,
                    TO_ROOM);
                retcode =
                    spell_smaug(skill_lookup("sanctuary"),
                                ch->level, ch, ch);
              } else
                retcode = rNONE;
              break;
          }
          if (attacktype != -1 && (retcode == rCHAR_DIED || char_died(ch)))
            continue;
        }
      }
    }
    /*
     * Fun for the whole family!
     */
    for (rch = ch->in_room->first_person; rch; rch = rch_next) {
      rch_next = rch->next_in_room;

      /*
       *   Group Fighting Styles Support:
       *   If ch is tanking
       *   If rch is using a more aggressive style than ch
       *   Then rch is the new tank   -h
       */
      /* &&( is_same_group(ch, rch)      ) */

      if ((!IS_NPC(ch) && !IS_NPC(rch)) && (rch != ch) && (rch->fighting) && (who_fighting(rch->fighting->who) == ch) &&
          (!xIS_SET(rch->fighting->who->act, ACT_AUTONOMOUS)) && (rch->style < ch->style)) {
        rch->fighting->who->fighting->who = rch;
      }
      if (IS_AWAKE(rch) && !rch->fighting) {
        /*
         * Split forms auto-assist others in their group.
         */

        if (!IS_NPC(ch) && is_splitformed(ch)) {
          if (IS_NPC(rch) && is_split(rch) && is_same_group(ch, rch) && !is_safe(rch, victim, true))
            multi_hit(rch, victim,
                      TYPE_UNDEFINED);
          continue;
        }
        /*
         * PC's auto-assist others in their group.
         */
        if (!IS_NPC(ch) || IS_AFFECTED(ch, AFF_CHARM)) {
          if (((!IS_NPC(rch) && rch->desc) || IS_AFFECTED(rch, AFF_CHARM)) && is_same_group(ch, rch) && !is_safe(rch, victim, true)) {
            multi_hit(rch, victim,
                      TYPE_UNDEFINED);
          }
          continue;
        }
        /*
         * NPC's assist NPC's of same type or 12.5% chance regardless.
         */
        if (IS_NPC(rch) && !IS_AFFECTED(rch, AFF_CHARM) && !xIS_SET(rch->act, ACT_NOASSIST)) {
          if (char_died(ch))
            break;
          if (rch->pIndexData == ch->pIndexData || number_bits(3) == 0) {
            CHAR_DATA *vch;
            CHAR_DATA *target;
            int number;

            target = NULL;
            number = 0;
            for (vch =
                     ch->in_room->first_person;
                 vch; vch = vch->next) {
              if (can_see(rch, vch) &&
                  is_same_group(vch,
                                victim) &&
                  number_range(0,
                               number) == 0) {
                if (vch->mount && vch->mount ==
                                      rch)
                  target =
                      NULL;
                else {
                  target =
                      vch;
                  number++;
                }
              }
            }

            if (target)
              multi_hit(rch, target,
                        TYPE_UNDEFINED);
          }
        }
      }
    }
  }
  sysdata.outBytesFlag = LOGBOUTNORM;

  return;
}

/*
 * Do one group of attacks.
 */
ch_ret
multi_hit(CHAR_DATA *ch, CHAR_DATA *victim, int dt) {
  int chance;
  int dual_bonus;
  ch_ret retcode;

  if (!IS_NPC(ch)) {
    if (ch->fight_start == 0)
      ch->fight_start = ch->exp;
  }
  if (!IS_NPC(victim)) {
    if (victim->fight_start == 0)
      victim->fight_start = victim->exp;
  }
  /* add timer to pkillers */
  if (!IS_NPC(ch) && !IS_NPC(victim)) {
    if (xIS_SET(ch->act, PLR_NICE))
      return rNONE;
    add_timer(ch, TIMER_RECENTFIGHT, 11, NULL, 0);
    add_timer(victim, TIMER_RECENTFIGHT, 11, NULL, 0);
  }

  if (IS_NPC(ch) && xIS_SET(ch->act, ACT_NOATTACK))
    return rNONE;

  if ((retcode = one_hit(ch, victim, dt)) != rNONE)
    return retcode;

  if (who_fighting(ch) != victim || dt == gsn_backstab || dt == gsn_circle)
    return rNONE;

  /* Very high chance of hitting compared to chance of going berserk */
  /* 40% or higher is always hit.. don't learn anything here though. */
  /* -- Altrag */
  chance = IS_NPC(ch) ? 100 : (LEARNED(ch, gsn_berserk) * 5 / 2);
  if (IS_AFFECTED(ch, AFF_BERSERK) && number_percent() < chance)
    if ((retcode = one_hit(ch, victim, dt)) != rNONE ||
        who_fighting(ch) != victim)
      return retcode;

  if (get_eq_char(ch, WEAR_DUAL_WIELD)) {
    dual_bonus =
        IS_NPC(ch) ? (ch->level /
                      10)
                   : (LEARNED(ch, gsn_dual_wield) / 10);
    chance = IS_NPC(ch) ? ch->level : LEARNED(ch, gsn_dual_wield);
    if (number_percent() < chance) {
      learn_from_success(ch, gsn_dual_wield);
      retcode = one_hit(ch, victim, dt);
      if (retcode != rNONE || who_fighting(ch) != victim)
        return retcode;
    } else
      learn_from_failure(ch, gsn_dual_wield);
  } else
    dual_bonus = 0;
  /*
   * NPC predetermined number of attacks                      -Thoric
   */
  if (IS_NPC(ch) && ch->numattacks > 0) {
    for (chance = 0; chance < ch->numattacks; chance++) {
      retcode = one_hit(ch, victim, dt);
      if (retcode != rNONE || who_fighting(ch) != victim)
        return retcode;
    }
    return retcode;
  }
  /* PC Number of Attacks*/
  chance = 95 / 1.5;
  if (number_percent() < chance) {
    retcode = one_hit(ch, victim, dt);
    if (retcode != rNONE || who_fighting(ch) != victim)
      return retcode;
  }
  if (number_percent() < chance) {
    retcode = one_hit(ch, victim, dt);
    if (retcode != rNONE || who_fighting(ch) != victim)
      return retcode;
  }
  if (is_human(ch) && ch->humanstat == 2) {
	if (number_percent() < chance) {
      retcode = one_hit(ch, victim, dt);
    if (retcode != rNONE || who_fighting(ch) != victim)
      return retcode;
    }
  }
}

/*
 * Weapon types, haus
 */
int weapon_prof_bonus_check(CHAR_DATA *ch, OBJ_DATA *wield, int *gsn_ptr) {
  int bonus;

  bonus = 0;
  *gsn_ptr = -1;
  if (!IS_NPC(ch) && ch->level > 5 && wield) {
    switch (wield->value[3]) {
      default:
        *gsn_ptr = -1;
        break;
      case DAM_HIT:
      case DAM_SUCTION:
      case DAM_BITE:
      case DAM_BLAST:
        *gsn_ptr = gsn_pugilism;
        break;
      case DAM_SLASH:
      case DAM_SLICE:
        *gsn_ptr = gsn_long_blades;
        break;
      case DAM_PIERCE:
      case DAM_STAB:
        *gsn_ptr = gsn_short_blades;
        break;
      case DAM_WHIP:
        *gsn_ptr = gsn_flexible_arms;
        break;
      case DAM_CLAW:
        *gsn_ptr = gsn_talonous_arms;
        break;
      case DAM_POUND:
      case DAM_CRUSH:
        *gsn_ptr = gsn_bludgeons;
        break;
      case DAM_BOLT:
      case DAM_ARROW:
      case DAM_DART:
      case DAM_STONE:
      case DAM_PEA:
        *gsn_ptr = gsn_missile_weapons;
        break;
    }
    if (*gsn_ptr != -1)
      bonus = (int)((LEARNED(ch, *gsn_ptr) - 50) / 10);
    if (IS_DEVOTED(ch))
      bonus -= ch->pcdata->favor / -400;
  }
  return bonus;
}

/*
 * Calculate the tohit bonus on the object and return RIS values.
 * -- Altrag
 */
int obj_hitroll(OBJ_DATA *obj) {
  int tohit = 0;
  AFFECT_DATA *paf;

  for (paf = obj->pIndexData->first_affect; paf; paf = paf->next)
    if (paf->location == APPLY_HITROLL)
      tohit += paf->modifier;
  for (paf = obj->first_affect; paf; paf = paf->next)
    if (paf->location == APPLY_HITROLL)
      tohit += paf->modifier;
  return tohit;
}

/*
 * Offensive shield level modifier
 */
sh_int
off_shld_lvl(CHAR_DATA *ch, CHAR_DATA *victim) {
  sh_int lvl;

  if (!IS_NPC(ch)) { /* players get much less effect */
    lvl = UMAX(1, (ch->level - 10) / 2);
    if (number_percent() + (victim->level - lvl) < 40) {
      if (CAN_PKILL(ch) && CAN_PKILL(victim))
        return ch->level;
      else
        return lvl;
    } else
      return 0;
  } else {
    lvl = ch->level / 2;
    if (number_percent() + (victim->level - lvl) < 70)
      return lvl;
    else
      return 0;
  }
}

/*
 * Hit one guy once.
 */
ch_ret
one_hit(CHAR_DATA *ch, CHAR_DATA *victim, int dt) {
  OBJ_DATA *wield;
  int plusris;
  int dam;
  int diceroll;
  int attacktype, cnt;
  int prof_bonus;
  int prof_gsn = -1;
  ch_ret retcode = rNONE;
  static bool dual_flip = false;
  double attmod = 0.000;
  int baseToHit = 0;

  attmod = get_attmod(ch, victim);

  /*
   * Can't beat a dead char!
   * Guard against weird room-leavings.
   */
  if (victim->position == POS_DEAD || ch->in_room != victim->in_room)
    return rVICT_DIED;

  ch->melee = true;

  used_weapon = NULL;
  /*
   * Figure out the weapon doing the damage                   -Thoric
   * Dual wield support -- switch weapons each attack
   */
  if ((wield = get_eq_char(ch, WEAR_DUAL_WIELD)) != NULL) {
    if (dual_flip == false) {
      dual_flip = true;
      wield = get_eq_char(ch, WEAR_WIELD);
    } else
      dual_flip = false;
  } else
    wield = get_eq_char(ch, WEAR_WIELD);

  used_weapon = wield;

  if (wield)
    prof_bonus = 0;
  else
    prof_bonus = 0;

  if (ch->fighting && dt == TYPE_UNDEFINED && IS_NPC(ch) && !xIS_EMPTY(ch->attacks)) {
    cnt = 0;
    for (;;) {
      attacktype = number_range(0, 6);
      if (xIS_SET(ch->attacks, attacktype))
        break;
      if (cnt++ > 16) {
        attacktype = -1;
        break;
      }
    }
    if (attacktype == ATCK_BACKSTAB)
      attacktype = -1;
    if (wield && number_percent() > 25)
      attacktype = -1;
    if (!wield && number_percent() > 50)
      attacktype = -1;

    switch (attacktype) {
      default:
        break;
      case ATCK_BITE:
        do_bite(ch, "");
        retcode = global_retcode;
        break;
      case ATCK_CLAWS:
        do_claw(ch, "");
        retcode = global_retcode;
        break;
      case ATCK_TAIL:
        do_tail(ch, "");
        retcode = global_retcode;
        break;
      case ATCK_STING:
        do_sting(ch, "");
        retcode = global_retcode;
        break;
      case ATCK_PUNCH:
        do_punch(ch, "");
        retcode = global_retcode;
        break;
      case ATCK_KICK:
        do_kick(ch, "");
        retcode = global_retcode;
        break;
      case ATCK_TRIP:
        attacktype = 0;
        break;
    }
    if (attacktype >= 0)
      return retcode;
  }
  if (dt == TYPE_UNDEFINED) {
    dt = TYPE_HIT;
    if (wield && wield->item_type == ITEM_WEAPON)
      dt += wield->value[3];
  }
  /*
   * Calculate percent to hit.
   */

  /*
   * Base chance to hit. Change as needed
   */
  baseToHit = 60;

  if (get_curr_dex(ch) > get_curr_dex(victim))
    baseToHit *=
        (double)(get_curr_dex(ch) / get_curr_dex(victim)) / 10 + 1;
  else
    baseToHit *= (double)get_curr_dex(ch) / get_curr_dex(victim);

  baseToHit = URANGE(1, baseToHit, 100);

  if (attmod > 1 && attmod < 10)
    baseToHit *= (double)attmod / 10 + 1;
  else if (attmod >= 10)
    baseToHit = UMIN((attmod * 10), 1000000);
  else if (attmod < 1)
    baseToHit *= attmod;

  /* if you can't see what's coming... */
  if (wield && !can_see_obj(victim, wield))
    baseToHit -= 5;
  if (!can_see(ch, victim))
    baseToHit -= 20;

  baseToHit = URANGE(1, baseToHit, 100);

  /*
   * The moment of excitement!
   */
  diceroll = number_range(1, 100);
  if ((diceroll > 5 && diceroll > baseToHit) || diceroll >= 96) {
    /* Miss. */
    if (prof_gsn != -1)
      learn_from_failure(ch, prof_gsn);
    damage(ch, victim, 0, dt);
    tail_chain();
    return rNONE;
  }
  /*
   * Hit.
   * Calc damage.
   */

  if (!wield) { /* bare hand dice formula fixed by
                 * Thoric */
    if (is_android(ch) || is_superandroid(ch))
      dam =
          number_range(ch->barenumdie,
                       (ch->baresizedie) * (ch->barenumdie +
                                            1));
    else
      dam =
          number_range(ch->barenumdie,
                       ch->baresizedie * ch->barenumdie);
  } else {
    dam = number_range(wield->value[1], wield->value[2]);
  }

  /*
   * Calculate Damage Modifiers Based on strength and con
   */
  dam += get_damroll(ch);
  if (dt == TYPE_HIT)
    dam -= get_strDef(victim);
  dam -= get_conDef(victim);

  if (dam < 0)
    dam = 0;

  /*
   * Bonuses.
   */
  if (prof_bonus)
    dam += prof_bonus / 4;

  /*
   * Max Damage Possable.
   */
  if (dam * attmod > 999999999)
    dam = 999999999;
  else
    dam *= attmod;

  /*
   * Calculate Damage Modifiers from Victim's Fighting Style
   */
  if (victim->position == POS_BERSERK)
    dam = 1.2 * dam;
  else if (victim->position == POS_AGGRESSIVE)
    dam = 1.1 * dam;
  else if (victim->position == POS_DEFENSIVE) {
    dam = .85 * dam;
    learn_from_success(victim, gsn_style_defensive);
  } else if (victim->position == POS_EVASIVE) {
    dam = .75 * dam;
    learn_from_success(victim, gsn_style_evasive);
  }
  /*
   * Calculate Damage Modifiers from Attacker's Fighting Style
   */
  if (ch->position == POS_BERSERK) {
    dam = 1.2 * dam;
    learn_from_success(ch, gsn_style_berserk);
  } else if (ch->position == POS_AGGRESSIVE) {
    dam = 1.1 * dam;
    learn_from_success(ch, gsn_style_aggressive);
  } else if (ch->position == POS_DEFENSIVE)
    dam = .85 * dam;
  else if (ch->position == POS_EVASIVE)
    dam = .05 * dam;
  if (!IS_NPC(victim)) {
	  if (is_saiyan(victim) || is_hb(victim))
		victim->saiyanrage += 1;
    }

  if (!IS_NPC(ch) && ch->pcdata->learned[gsn_enhanced_damage] > 0) {
    dam += (int)(dam * LEARNED(ch, gsn_enhanced_damage) / 120);
    learn_from_success(ch, gsn_enhanced_damage);
  }
  if (!IS_AWAKE(victim))
    dam *= 2;
  if (dt == gsn_backstab)
    dam *= (2 + URANGE(2, ch->level - (victim->level / 4), 30) / 8);

  if (dt == gsn_circle)
    dam *=
        (2 + URANGE(2, ch->level - (victim->level / 4), 30) / 16);

  /*
   * For NPC's, they can be set to automaticly take a % off dammage
   */
  if (IS_NPC(victim)) {
    if ((GET_AC(victim) > 0)) {
      dam = (dam * (GET_AC(victim) * 0.01));
    }
    if (GET_AC(victim) <= 0) {
      dam = 0;
    }
  }
  if (dam < 0)
    dam = 0;

  if (dam > 999999999)
    dam = 999999999;

  plusris = 0;

  if (wield) {
    if (IS_OBJ_STAT(wield, ITEM_MAGIC))
      dam = ris_damage(victim, dam, RIS_MAGIC);
    else
      dam = ris_damage(victim, dam, RIS_NONMAGIC);

    /*
     * Handle PLUS1 - PLUS6 ris bits vs. weapon hitroll     -Thoric
     */
    plusris = obj_hitroll(wield);
  } else
    dam = ris_damage(victim, dam, RIS_NONMAGIC);

  /* check for RIS_PLUSx                                      -Thoric */
  if (dam) {
    int x, res, imm, sus, mod;

    if (plusris)
      plusris = RIS_PLUS1 << UMIN(plusris, 7);

    /* initialize values to handle a zero plusris */
    imm = res = -1;
    sus = 1;

    /* find high ris */
    for (x = RIS_PLUS1; x <= RIS_PLUS6; x <<= 1) {
      if (IS_SET(victim->immune, x))
        imm = x;
      if (IS_SET(victim->resistant, x))
        res = x;
      if (IS_SET(victim->susceptible, x))
        sus = x;
    }
    mod = 10;
    if (imm >= plusris)
      mod -= 10;
    if (res >= plusris)
      mod -= 2;
    if (sus <= plusris)
      mod += 2;

    /* check if immune */
    if (mod <= 0)
      dam = -1;
    if (mod != 10)
      dam = (dam * mod) / 10;
  }
  if (prof_gsn != -1) {
    if (dam > 0)
      learn_from_success(ch, prof_gsn);
    else
      learn_from_failure(ch, prof_gsn);
  }
  /* immune to damage */
  if (dam == -1) {
    if (dt >= 0 && dt < top_sn) {
      SKILLTYPE *skill = skill_table[dt];
      bool found = false;

      if (skill->imm_char && skill->imm_char[0] != '\0') {
        act(AT_HIT, skill->imm_char, ch, NULL, victim,
            TO_CHAR);
        found = true;
      }
      if (skill->imm_vict && skill->imm_vict[0] != '\0') {
        act(AT_HITME, skill->imm_vict, ch, NULL, victim,
            TO_VICT);
        found = true;
      }
      if (skill->imm_room && skill->imm_room[0] != '\0') {
        act(AT_ACTION, skill->imm_room, ch, NULL,
            victim, TO_NOTVICT);
        found = true;
      }
      if (found)
        return rNONE;
    }
    dam = 0;
  }
  if ((retcode = damage(ch, victim, dam, dt)) != rNONE)
    return retcode;
  if (char_died(ch))
    return rCHAR_DIED;
  if (char_died(victim))
    return rVICT_DIED;

  retcode = rNONE;
  if (dam == 0)
    return retcode;

  /*
   * Weapon spell support                             -Thoric
   * Each successful hit casts a spell
   */
  if (wield && !IS_SET(victim->immune, RIS_MAGIC) && !xIS_SET(victim->in_room->room_flags, ROOM_NO_MAGIC)) {
    AFFECT_DATA *aff;

    for (aff = wield->pIndexData->first_affect; aff;
         aff = aff->next)
      if (aff->location == APPLY_WEAPONSPELL && IS_VALID_SN(aff->modifier) && skill_table[aff->modifier]->spell_fun)
        retcode =
            (*skill_table[aff->modifier]->spell_fun)(aff->modifier, (wield->level + 3) / 3, ch,
                                                     victim);
    if (retcode != rNONE || char_died(ch) || char_died(victim))
      return retcode;
    for (aff = wield->first_affect; aff; aff = aff->next)
      if (aff->location == APPLY_WEAPONSPELL && IS_VALID_SN(aff->modifier) && skill_table[aff->modifier]->spell_fun)
        retcode =
            (*skill_table[aff->modifier]->spell_fun)(aff->modifier, (wield->level + 3) / 3, ch,
                                                     victim);
    if (retcode != rNONE || char_died(ch) || char_died(victim))
      return retcode;
  }
  /*
   * magic shields that retaliate                             -Thoric
   */
  if (IS_AFFECTED(victim, AFF_FIRESHIELD) && !IS_AFFECTED(ch, AFF_FIRESHIELD))
    retcode =
        spell_smaug(skill_lookup("flare"), off_shld_lvl(victim, ch),
                    victim, ch);
  if (retcode != rNONE || char_died(ch) || char_died(victim))
    return retcode;

  if (IS_AFFECTED(victim, AFF_ICESHIELD) && !IS_AFFECTED(ch, AFF_ICESHIELD))
    retcode =
        spell_smaug(skill_lookup("iceshard"),
                    off_shld_lvl(victim, ch), victim, ch);
  if (retcode != rNONE || char_died(ch) || char_died(victim))
    return retcode;

  if (IS_AFFECTED(victim, AFF_SHOCKSHIELD) && !IS_AFFECTED(ch, AFF_SHOCKSHIELD))
    retcode =
        spell_smaug(skill_lookup("torrent"),
                    off_shld_lvl(victim, ch), victim, ch);
  if (retcode != rNONE || char_died(ch) || char_died(victim))
    return retcode;

  if (IS_AFFECTED(victim, AFF_ACIDMIST) && !IS_AFFECTED(ch, AFF_ACIDMIST))
    retcode =
        spell_smaug(skill_lookup("acidshot"),
                    off_shld_lvl(victim, ch), victim, ch);
  if (retcode != rNONE || char_died(ch) || char_died(victim))
    return retcode;

  if (IS_AFFECTED(victim, AFF_VENOMSHIELD) && !IS_AFFECTED(ch, AFF_VENOMSHIELD))
    retcode =
        spell_smaug(skill_lookup("venomshot"),
                    off_shld_lvl(victim, ch), victim, ch);
  if (retcode != rNONE || char_died(ch) || char_died(victim))
    return retcode;

  tail_chain();
  return retcode;
}

/*
 * Hit one guy with a projectile.
 * Handles use of missile weapons (wield = missile weapon)
 * or thrown items/weapons
 */
ch_ret
projectile_hit(CHAR_DATA *ch, CHAR_DATA *victim, OBJ_DATA *wield,
               OBJ_DATA *projectile, sh_int dist) {
  int victim_ac;
  int thac0;
  int thac0_00;
  int thac0_32;
  int plusris;
  int dam;
  int diceroll;
  int prof_bonus;
  int prof_gsn = -1;
  int proj_bonus;
  int dt;
  ch_ret retcode;

  if (!projectile)
    return rNONE;

  if (projectile->item_type == ITEM_PROJECTILE || projectile->item_type == ITEM_WEAPON) {
    dt = TYPE_HIT + projectile->value[3];
    proj_bonus =
        number_range(projectile->value[1], projectile->value[2]);
  } else {
    dt = TYPE_UNDEFINED;
    proj_bonus =
        number_range(1, URANGE(2, get_obj_weight(projectile), 100));
  }

  /*
   * Can't beat a dead char!
   */
  if (victim->position == POS_DEAD || char_died(victim)) {
    extract_obj(projectile);
    return rVICT_DIED;
  }
  if (wield)
    prof_bonus = weapon_prof_bonus_check(ch, wield, &prof_gsn);
  else
    prof_bonus = 0;

  if (dt == TYPE_UNDEFINED) {
    dt = TYPE_HIT;
    if (wield && wield->item_type == ITEM_MISSILE_WEAPON)
      dt += wield->value[3];
  }
  /*
   * Calculate to-hit-armor-class-0 versus armor.
   */
  if (IS_NPC(ch)) {
    thac0_00 = ch->mobthac0;
    thac0_32 = 0;
  } else {
    thac0_00 = class_table[ch->class]->thac0_00;
    thac0_32 = class_table[ch->class]->thac0_32;
  }
  thac0 =
      interpolate(ch->level, thac0_00,
                  thac0_32) -
      GET_HITROLL(ch) + (dist * 2);
  victim_ac = UMAX(-19, (int)(GET_AC(victim) / 10));

  /* if you can't see what's coming... */
  if (!can_see_obj(victim, projectile))
    victim_ac += 1;
  if (!can_see(ch, victim))
    victim_ac -= 4;

  /* Weapon proficiency bonus */
  victim_ac += prof_bonus;

  /*
   * The moment of excitement!
   */
  while ((diceroll = number_bits(5)) >= 20)
    ;

  if (diceroll == 0 || (diceroll != 19 && diceroll < thac0 - victim_ac)) {
    /* Miss. */
    if (prof_gsn != -1)
      learn_from_failure(ch, prof_gsn);

    /* Do something with the projectile */
    if (number_percent() < 50)
      extract_obj(projectile);
    else {
      if (projectile->in_obj)
        obj_from_obj(projectile);
      if (projectile->carried_by)
        obj_from_char(projectile);
      obj_to_room(projectile, victim->in_room);
    }
    damage(ch, victim, 0, dt);
    tail_chain();
    return rNONE;
  }
  /*
   * Hit.
   * Calc damage.
   */

  if (!wield) /* dice formula fixed by Thoric */
    dam = proj_bonus;
  else
    dam =
        number_range(wield->value[1],
                     wield->value[2]) +
        (proj_bonus / 10);

  /*
   * Bonuses.
   */
  dam += GET_DAMROLL(ch);

  if (prof_bonus)
    dam += prof_bonus / 4;

  /*
   * Calculate Damage Modifiers from Victim's Fighting Style
   */
  if (victim->position == POS_BERSERK)
    dam = 1.2 * dam;
  else if (victim->position == POS_AGGRESSIVE)
    dam = 1.1 * dam;
  else if (victim->position == POS_DEFENSIVE)
    dam = .85 * dam;
  else if (victim->position == POS_EVASIVE)
    dam = .8 * dam;

  if (!IS_NPC(ch) && ch->pcdata->learned[gsn_enhanced_damage] > 0) {
    dam += (int)(dam * LEARNED(ch, gsn_enhanced_damage) / 120);
    learn_from_success(ch, gsn_enhanced_damage);
  }
  if (!IS_AWAKE(victim))
    dam *= 2;

  if (dam <= 0)
    dam = 1;

  plusris = 0;

  if (IS_OBJ_STAT(projectile, ITEM_MAGIC))
    dam = ris_damage(victim, dam, RIS_MAGIC);
  else
    dam = ris_damage(victim, dam, RIS_NONMAGIC);

  /*
   * Handle PLUS1 - PLUS6 ris bits vs. weapon hitroll -Thoric
   */
  if (wield)
    plusris = obj_hitroll(wield);

  /* check for RIS_PLUSx                                      -Thoric */
  if (dam) {
    int x, res, imm, sus, mod;

    if (plusris)
      plusris = RIS_PLUS1 << UMIN(plusris, 7);

    /* initialize values to handle a zero plusris */
    imm = res = -1;
    sus = 1;

    /* find high ris */
    for (x = RIS_PLUS1; x <= RIS_PLUS6; x <<= 1) {
      if (IS_SET(victim->immune, x))
        imm = x;
      if (IS_SET(victim->resistant, x))
        res = x;
      if (IS_SET(victim->susceptible, x))
        sus = x;
    }
    mod = 10;
    if (imm >= plusris)
      mod -= 10;
    if (res >= plusris)
      mod -= 2;
    if (sus <= plusris)
      mod += 2;

    /* check if immune */
    if (mod <= 0)
      dam = -1;
    if (mod != 10)
      dam = (dam * mod) / 10;
  }
  if (prof_gsn != -1) {
    if (dam > 0)
      learn_from_success(ch, prof_gsn);
    else
      learn_from_failure(ch, prof_gsn);
  }
  /* immune to damage */
  if (dam == -1) {
    if (dt >= 0 && dt < top_sn) {
      SKILLTYPE *skill = skill_table[dt];
      bool found = false;

      if (skill->imm_char && skill->imm_char[0] != '\0') {
        act(AT_HIT, skill->imm_char, ch, NULL, victim,
            TO_CHAR);
        found = true;
      }
      if (skill->imm_vict && skill->imm_vict[0] != '\0') {
        act(AT_HITME, skill->imm_vict, ch, NULL, victim,
            TO_VICT);
        found = true;
      }
      if (skill->imm_room && skill->imm_room[0] != '\0') {
        act(AT_ACTION, skill->imm_room, ch, NULL,
            victim, TO_NOTVICT);
        found = true;
      }
      if (found) {
        if (number_percent() < 50)
          extract_obj(projectile);
        else {
          if (projectile->in_obj)
            obj_from_obj(projectile);
          if (projectile->carried_by)
            obj_from_char(projectile);
          obj_to_room(projectile,
                      victim->in_room);
        }
        return rNONE;
      }
    }
    dam = 0;
  }
  if ((retcode = damage(ch, victim, dam, dt)) != rNONE) {
    extract_obj(projectile);
    return retcode;
  }
  if (char_died(ch)) {
    extract_obj(projectile);
    return rCHAR_DIED;
  }
  if (char_died(victim)) {
    extract_obj(projectile);
    return rVICT_DIED;
  }
  retcode = rNONE;
  if (dam == 0) {
    if (number_percent() < 50)
      extract_obj(projectile);
    else {
      if (projectile->in_obj)
        obj_from_obj(projectile);
      if (projectile->carried_by)
        obj_from_char(projectile);
      obj_to_room(projectile, victim->in_room);
    }
    return retcode;
  }
  /* weapon spells	-Thoric */
  if (wield && !IS_SET(victim->immune, RIS_MAGIC) && !xIS_SET(victim->in_room->room_flags, ROOM_NO_MAGIC)) {
    AFFECT_DATA *aff;

    for (aff = wield->pIndexData->first_affect; aff;
         aff = aff->next)
      if (aff->location == APPLY_WEAPONSPELL && IS_VALID_SN(aff->modifier) && skill_table[aff->modifier]->spell_fun)
        retcode =
            (*skill_table[aff->modifier]->spell_fun)(aff->modifier, (wield->level + 3) / 3, ch,
                                                     victim);
    if (retcode != rNONE || char_died(ch) || char_died(victim)) {
      extract_obj(projectile);
      return retcode;
    }
    for (aff = wield->first_affect; aff; aff = aff->next)
      if (aff->location == APPLY_WEAPONSPELL && IS_VALID_SN(aff->modifier) && skill_table[aff->modifier]->spell_fun)
        retcode =
            (*skill_table[aff->modifier]->spell_fun)(aff->modifier, (wield->level + 3) / 3, ch,
                                                     victim);
    if (retcode != rNONE || char_died(ch) || char_died(victim)) {
      extract_obj(projectile);
      return retcode;
    }
  }
  extract_obj(projectile);

  tail_chain();
  return retcode;
}

/*
 * Calculate damage based on resistances, immunities and suceptibilities
 *					-Thoric
 */
int ris_damage(CHAR_DATA *ch, int dam, int ris) {
  sh_int modifier;

  modifier = 10;
  if (IS_SET(ch->immune, ris) && !IS_SET(ch->no_immune, ris))
    modifier -= 10;
  if (IS_SET(ch->resistant, ris) && !IS_SET(ch->no_resistant, ris))
    modifier -= 2;
  if (IS_SET(ch->susceptible, ris) && !IS_SET(ch->no_susceptible, ris)) {
    if (IS_NPC(ch) && IS_SET(ch->immune, ris))
      modifier += 0;
    else
      modifier += 2;
  }
  if (modifier <= 0)
    return -1;
  if (modifier == 10)
    return dam;
  return (dam * modifier) / 10;
}

/*
 * fight training
 */
void stat_train(CHAR_DATA *ch, char *stat, int modifier) {
  int *tAbility;
  int *pAbility;
  int *permTstat;
  int fightIncrease = 0;
  int gainMod = 0;
  if (modifier > 0) {
    gainMod = modifier;
  } else {
    gainMod = 5;
  }

  fightIncrease = (number_range(1, 2) * gainMod);

  if (strcmp("str", stat) == 0) {
    tAbility = &ch->pcdata->tStr;
    pAbility = &ch->perm_str;
    permTstat = &ch->pcdata->permTstr;
	if (is_human(ch) && ch->humanstat == 1)
		fightIncrease *= 1.33;
  } else if (strcmp("int", stat) == 0) {
    tAbility = &ch->pcdata->tInt;
    pAbility = &ch->perm_int;
    permTstat = &ch->pcdata->permTint;
	if (is_human(ch) && ch->humanstat == 3)
		fightIncrease *= 1.20;
  } else if (strcmp("spd", stat) == 0) {
    tAbility = &ch->pcdata->tSpd;
    pAbility = &ch->perm_dex;
    permTstat = &ch->pcdata->permTspd;
	if (is_human(ch) && ch->humanstat == 2)
		fightIncrease *= 1.20;
  } else if (strcmp("con", stat) == 0) {
    tAbility = &ch->pcdata->tCon;
    pAbility = &ch->perm_con;
    permTstat = &ch->pcdata->permTcon;
	if (is_human(ch) && ch->humanstat == 1)
		fightIncrease *= 1.20;
  }

  *tAbility += fightIncrease;

  if (*tAbility >= 1000 && *permTstat < 2000000000) {
    *tAbility = 0;
    *pAbility += 1;
    *permTstat += 1;
    if (ch->exp < 50) {
      int randomgain = number_range(1, 3);

      if (randomgain == 3) {
        ch->exp += 1;
        ch->pl += 1;
      }
    }
    if (strcmp("str", stat) == 0) {
      send_to_char("&CYou feel your strength improving!&D\n\r", ch);
    }
    if (strcmp("spd", stat) == 0) {
      send_to_char("&CYou feel your speed improving!&D\n\r", ch);
    }
    if (strcmp("int", stat) == 0) {
      send_to_char("&CYou feel your intelligence improving!&D\n\r", ch);
    }
    if (strcmp("con", stat) == 0) {
      send_to_char("&CYou feel your constitution improving!&D\n\r", ch);
    }
  } else if (*permTstat >= 2000000000 && *tAbility >= 999) {
    *tAbility = 999;
  }
}

void exercise_train(CHAR_DATA *ch, char *stat, int modifier) {
  int *tAbility;
  int *pAbility;
  int *permTstat;
  int fightIncrease = 0;
  int gainMod = 0;
  if (modifier > 0) {
    gainMod = modifier;
  } else {
    gainMod = 5;
  }

  fightIncrease = (number_range(1, 3) * gainMod);

  if (strcmp("str", stat) == 0) {
    tAbility = &ch->pcdata->tStr;
    pAbility = &ch->perm_str;
    permTstat = &ch->pcdata->permTstr;
	if (is_human(ch) && ch->humanstat == 1)
		fightIncrease *= 1.33;
  } else if (strcmp("int", stat) == 0) {
    tAbility = &ch->pcdata->tInt;
    pAbility = &ch->perm_int;
    permTstat = &ch->pcdata->permTint;
	if (is_human(ch) && ch->humanstat == 3)
		fightIncrease *= 1.20;
  } else if (strcmp("spd", stat) == 0) {
    tAbility = &ch->pcdata->tSpd;
    pAbility = &ch->perm_dex;
    permTstat = &ch->pcdata->permTspd;
	if (is_human(ch) && ch->humanstat == 2)
		fightIncrease *= 1.20;
  } else if (strcmp("con", stat) == 0) {
    tAbility = &ch->pcdata->tCon;
    pAbility = &ch->perm_con;
    permTstat = &ch->pcdata->permTcon;
	if (is_human(ch) && ch->humanstat == 4)
		fightIncrease *= 1.20;
  }

  *tAbility += fightIncrease;

  if (*tAbility >= 1000 && *permTstat < 2000000000) {
    *tAbility = 0;
    *pAbility += 1;
    *permTstat += 1;
    if (ch->exp < 50) {
      int randomgain = 0;

      randomgain = number_range(1, 3);
      if (randomgain == 3) {
        ch->exp += 1;
        ch->pl += 1;
      }
    }
    if (!strcmp(stat, "str")) {
      send_to_char("&CYou feel your strength improving!&D\n\r", ch);
    }
    if (!strcmp(stat, "spd")) {
      send_to_char("&CYou feel your speed improving!&D\n\r", ch);
    }
    if (!strcmp(stat, "int")) {
      send_to_char("&CYou feel your intelligence improving!&D\n\r", ch);
    }
    if (!strcmp(stat, "con")) {
      send_to_char("&CYou feel your constitution improving!&D\n\r", ch);
    }
  } else if (*permTstat >= 2000000000 && *tAbility >= 999) {
    *tAbility = 999;
  }
}

ch_ret
damage(CHAR_DATA *ch, CHAR_DATA *victim, int dam, int dt) {
  char buf[MAX_STRING_LENGTH];
  char buf1[MAX_STRING_LENGTH];
  char kbuf[MAX_STRING_LENGTH];
  char filename[256];
  bool npcvict;
  bool kaiopres = false;
  bool demonpres = false;
  bool loot;

  long double xp_gain = 0;
  long double xp_gain_post = 0;
  ch_ret retcode;
  sh_int dampmod;
  CHAR_DATA *gch;
  int init_gold, new_gold, gold_diff;
  sh_int anopc = 0; /* # of (non-pkill) pc in a (ch) */
  sh_int bnopc = 0; /* # of (non-pkill) pc in b (victim) */
  float xp_mod;
  long double los = 0;
  bool preservation = false;
  bool biopres = false;
  bool warpres = false;
  bool immortal = false;
  ROOM_INDEX_DATA *pRoomIndex;
  double clothingPlMod = 0;
  /* fight training */

  retcode = rNONE;

  if (!ch) {
    bug("Damage: null ch!", 0);
    return rERROR;
  }
  if (!victim) {
    bug("Damage: null victim!", 0);
    return rVICT_DIED;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim))
    if (ch->pcdata->clan && victim->pcdata->clan)
      warpres = true;

  if (!IS_NPC(victim))
    if (IS_SET(victim->pcdata->flags, PCFLAG_IMMORTALITY))
      immortal = true;

  if (victim->position == POS_DEAD)
    return rVICT_DIED;

  if (xIS_SET(victim->act, ACT_PROTOTYPE) && !IS_NPC(ch) && !IS_IMMORTAL(ch))
    return rNONE;

  npcvict = IS_NPC(victim);

  /* check damage types for RIS */
  if (dam && dt != TYPE_UNDEFINED) {
    if (IS_FIRE(dt))
      dam = ris_damage(victim, dam, RIS_FIRE);
    else if (IS_COLD(dt))
      dam = ris_damage(victim, dam, RIS_COLD);
    else if (IS_ACID(dt))
      dam = ris_damage(victim, dam, RIS_ACID);
    else if (IS_ELECTRICITY(dt))
      dam = ris_damage(victim, dam, RIS_ELECTRICITY);
    else if (IS_ENERGY(dt))
      dam = ris_damage(victim, dam, RIS_ENERGY);
    else if (IS_DRAIN(dt))
      dam = ris_damage(victim, dam, RIS_DRAIN);
    else if (dt == gsn_poison || IS_POISON(dt))
      dam = ris_damage(victim, dam, RIS_POISON);
    else if (dt == (TYPE_HIT + DAM_POUND) || dt == (TYPE_HIT + DAM_CRUSH) || dt == (TYPE_HIT + DAM_STONE) || dt == (TYPE_HIT + DAM_PEA))
      dam = ris_damage(victim, dam, RIS_BLUNT);
    else if (dt == (TYPE_HIT + DAM_STAB) || dt == (TYPE_HIT + DAM_PIERCE) || dt == (TYPE_HIT + DAM_BITE) || dt == (TYPE_HIT + DAM_BOLT) || dt == (TYPE_HIT + DAM_DART) || dt == (TYPE_HIT + DAM_ARROW))
      dam = ris_damage(victim, dam, RIS_PIERCE);
    else if (dt == (TYPE_HIT + DAM_SLICE) || dt == (TYPE_HIT + DAM_SLASH) || dt == (TYPE_HIT + DAM_WHIP) || dt == (TYPE_HIT + DAM_CLAW))
      dam = ris_damage(victim, dam, RIS_SLASH);

    if (dam == -1) {
      if (dt >= 0 && dt < top_sn) {
        bool found = false;
        SKILLTYPE *skill = skill_table[dt];

        if (skill->imm_char && skill->imm_char[0] != '\0') {
          act(AT_HIT, skill->imm_char, ch, NULL,
              victim, TO_CHAR);
          found = true;
        }
        if (skill->imm_vict && skill->imm_vict[0] != '\0') {
          act(AT_HITME, skill->imm_vict, ch, NULL,
              victim, TO_VICT);
          found = true;
        }
        if (skill->imm_room && skill->imm_room[0] != '\0') {
          act(AT_ACTION, skill->imm_room, ch,
              NULL, victim, TO_NOTVICT);
          found = true;
        }
        if (found)
          return rNONE;
      }
      dam = 0;
    }
  }
  /* hell has no way out. */
  if (xIS_SET(victim->in_room->room_flags, ROOM_SAFE))
    dam = 0;

  if (dam && npcvict && ch != victim) {
    if (!xIS_SET(victim->act, ACT_SENTINEL)) {
      if (victim->hunting) {
        if (victim->hunting->who != ch) {
          STRFREE(victim->hunting->name);
          victim->hunting->name =
              QUICKLINK(ch->name);
          victim->hunting->who = ch;
        }
      }
    }
    if (victim->hating) {
      if (victim->hating->who != ch) {
        STRFREE(victim->hating->name);
        victim->hating->name = QUICKLINK(ch->name);
        victim->hating->who = ch;
      }
    } else if (!xIS_SET(victim->act, ACT_PACIFIST))
      start_hating(victim, ch);
  }
  if (victim != ch) {
    /*
     * Certain attacks are forbidden.
     * Most other attacks are returned.
     */
    if (is_safe(ch, victim, true))
      return rNONE;
    check_attacker(ch, victim);

    if (victim->position > POS_STUNNED) {
      if (!victim->fighting && victim->in_room == ch->in_room)
        set_fighting(victim, ch);
      if (IS_NPC(victim) && victim->fighting)
        victim->position = POS_FIGHTING;
      else if (victim->fighting) {
        switch (victim->style) {
          case (STYLE_EVASIVE):
            victim->position = POS_EVASIVE;
            break;
          case (STYLE_DEFENSIVE):
            victim->position = POS_DEFENSIVE;
            break;
          case (STYLE_AGGRESSIVE):
            victim->position = POS_AGGRESSIVE;
            break;
          case (STYLE_BERSERK):
            victim->position = POS_BERSERK;
            break;
          default:
            victim->position = POS_FIGHTING;
        }
      }
    }
    if (victim->position > POS_STUNNED) {
      if (!ch->fighting && victim->in_room == ch->in_room)
        set_fighting(ch, victim);

      /* if victim is charmed, ch might attack victim's master. */
      if (IS_NPC(ch) && npcvict && IS_AFFECTED(victim, AFF_CHARM) && victim->master && victim->master->in_room == ch->in_room && number_bits(3) == 0) {
        stop_fighting(ch, false);
        retcode =
            multi_hit(ch, victim->master,
                      TYPE_UNDEFINED);
        return retcode;
      }
    }
    /*
     * More charm stuff.
     */
    if (victim->master == ch)
      stop_follower(victim);

    /*
     * Pkill stuff.  If a deadly attacks another deadly or is attacked by
     * one, then ungroup any nondealies.  Disabled untill I can figure out
     * the right way to do it.
     */

    /*
     *  count the # of non-pkill pc in a ( not including == ch )
     */
    for (gch = ch->in_room->first_person; gch;
         gch = gch->next_in_room)
      if (is_same_group(ch, gch) && !IS_NPC(gch) && !IS_PKILL(gch) && (ch != gch))
        anopc++;

    /*
     *  count the # of non-pkill pc in b ( not including == victim )
     */
    for (gch = victim->in_room->first_person; gch;
         gch = gch->next_in_room)
      if (is_same_group(victim, gch) && !IS_NPC(gch) && !IS_PKILL(gch) && (victim != gch))
        bnopc++;

    /*
     *  only consider disbanding if both groups have 1(+) non-pk pc,
     *  or when one participant is pc, and the other group has 1(+)
     *  pk pc's (in the case that participant is only pk pc in group)
     */
    if ((bnopc > 0 && anopc > 0) || (bnopc > 0 && !IS_NPC(ch)) || (anopc > 0 && !IS_NPC(victim))) {
      /* disband from same group first */
      if (is_same_group(ch, victim)) {
        /* messages to char and master handled in stop_follower */
        act(AT_ACTION, "$n disbands from $N's group.",
            (ch->leader == victim) ? victim : ch, NULL,
            (ch->leader ==
             victim)
                ? victim->master
                : ch->master,
            TO_NOTVICT);
        if (ch->leader == victim)
          stop_follower(victim);
        else
          stop_follower(ch);
      }
      /* if leader isnt pkill, leave the group and disband ch */
      if (ch->leader != NULL && !IS_NPC(ch->leader) &&
          !IS_PKILL(ch->leader)) {
        act(AT_ACTION, "$n disbands from $N's group.",
            ch, NULL, ch->master, TO_NOTVICT);
        stop_follower(ch);
      } else {
        for (gch = ch->in_room->first_person; gch;
             gch = gch->next_in_room)
          if (is_same_group(gch, ch) && !IS_NPC(gch) && !IS_PKILL(gch) && gch != ch) {
            act(AT_ACTION,
                "$n disbands from $N's group.",
                ch, NULL, gch->master,
                TO_NOTVICT);
            stop_follower(gch);
          }
      }
      /* if leader isnt pkill, leave the group and disband victim */
      if (victim->leader != NULL && !IS_NPC(victim->leader) &&
          !IS_PKILL(victim->leader)) {
        act(AT_ACTION, "$n disbands from $N's group.",
            victim, NULL, victim->master, TO_NOTVICT);
        stop_follower(victim);
      } else {
        for (gch = victim->in_room->first_person; gch;
             gch = gch->next_in_room)
          if (is_same_group(gch, victim) && !IS_NPC(gch) && !IS_PKILL(gch) && gch != victim) {
            act(AT_ACTION,
                "$n disbands from $N's group.",
                gch, NULL, gch->master,
                TO_NOTVICT);
            stop_follower(gch);
          }
      }
    }
    /* inviso attacks ... not. */
    if (IS_AFFECTED(ch, AFF_INVISIBLE)) {
      affect_strip(ch, gsn_invis);
      affect_strip(ch, gsn_mass_invis);
      xREMOVE_BIT(ch->affected_by, AFF_INVISIBLE);
      act(AT_MAGIC, "$n fades into existence.", ch, NULL,
          NULL, TO_ROOM);
    }
    /* take away hide */
    if (IS_AFFECTED(ch, AFF_HIDE))
      xREMOVE_BIT(ch->affected_by, AFF_HIDE);
    /* damage modifiers. */
    if (IS_AFFECTED(victim, AFF_SANCTUARY))
      dam /= 2;
    if (IS_AFFECTED(victim, AFF_PROTECT) && IS_EVIL(ch))
      dam -= (int)(dam / 4);
    if (dam < 0)
      dam = 0;
    /* check for disarm, trip, parry, dodge and tumble. */
    if (dt >= TYPE_HIT && ch->in_room == victim->in_room) {
      if (IS_NPC(ch) && xIS_SET(ch->defenses, DFND_DISARM) && ch->level > 9 && number_percent() < ch->level / 3)
        disarm(ch, victim);
      if (IS_NPC(ch) && xIS_SET(ch->attacks, ATCK_TRIP) && ch->level > 5 && number_percent() < ch->level / 2)
        trip(ch, victim);

      if (check_dodge(ch, victim))
        return rNONE;
    }
    /* check control panel settings and modify damage */
    if (IS_NPC(ch)) {
      if (npcvict)
        dampmod = sysdata.dam_mob_vs_mob;
      else
        dampmod = sysdata.dam_mob_vs_plr;
    } else {
      if (npcvict)
        dampmod = sysdata.dam_plr_vs_mob;
      else
        dampmod = sysdata.dam_plr_vs_plr;
    }
    if (dampmod > 0 && dampmod != 100)
      dam = (dam * dampmod) / 100;

    dam = dam_armor_recalc(victim, dam);

    if (!IS_NPC(victim) && ch->melee) {
      int spd = 0;
      int check = 0;

      int att = ((get_attmod(victim, ch) / 2) * 10);
      int revatt = ((get_attmod(ch, victim) / 2) * 5);

      if (att > 100)
        att = 100;
      if (att < 0)
        att = 0;
      if (revatt > 100)
        revatt = 100;
      if (revatt < 0)
        revatt = 0;

      /* checks to see if the ch is faster */
      if (get_curr_dex(ch) > get_curr_dex(victim)) {
        if (((float)get_curr_dex(ch) /
             (float)get_curr_dex(victim)) >= 1.25)
          spd = -5;
        if (((float)get_curr_dex(ch) /
             (float)get_curr_dex(victim)) >= 1.5)
          spd = -10;
        if (((float)get_curr_dex(ch) /
             (float)get_curr_dex(victim)) >= 2)
          spd = -15;
        if (((float)get_curr_dex(ch) /
             (float)get_curr_dex(victim)) >= 3)
          spd = -20;
      } else if (get_curr_dex(ch) < get_curr_dex(victim)) {
        if (((float)get_curr_dex(victim) /
             (float)get_curr_dex(ch)) >= 1.25)
          spd = 10;
        if (((float)get_curr_dex(victim) /
             (float)get_curr_dex(ch)) >= 1.5)
          spd = 20;
        if (((float)get_curr_dex(victim) /
             (float)get_curr_dex(ch)) >= 2)
          spd = 30;
        if (((float)get_curr_dex(victim) /
             (float)get_curr_dex(ch)) >= 3)
          spd = 50;
      } else
        spd = 0;

      check = ((20 + spd) + (att - revatt));

      if (check < 1)
        check = 1;
      if (check > 100)
        check = 100;
      int hitcheck = 0;
      if (!IS_SET(victim->pcdata->combatFlags, CMB_NO_DODGE)) {
        hitcheck = number_range(1, 100);
        if (hitcheck <= 95) {
          if (number_range(1, 100) <= check) {
            dam = 0;
            victim->dodge = true;
            if (IS_NPC(ch) && !IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG)) {
              pager_printf(victim,
                           "&OYou skillfully dodge %s's attack.\n\r",
                           ch->short_descr);
            }
            if (!IS_NPC(ch) && !IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG)) {
              pager_printf(victim,
                           "&OYou skillfully dodge %s's attack.\n\r",
                           ch->name);
            }
            // train speed stat on dodge success
            if (!IS_NPC(victim)) {
              stat_train(victim, "spd", 8);
			  stat_train(victim, "con", 4);
            }
          }
        }
      }
      if (!victim->dodge && !IS_SET(victim->pcdata->combatFlags, CMB_NO_BLOCK)) {
        hitcheck = number_range(1, 100);
        if (hitcheck <= 95) {
          if (number_range(1, 100) <= check) {
            dam = 0;
            victim->block = true;
            if (IS_NPC(ch) && !IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG)) {
              pager_printf(victim,
                           "&OYou successfully block %s's attack.\n\r",
                           ch->short_descr);
            }
            if (!IS_NPC(ch) && !IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG)) {
              pager_printf(victim,
                           "&OYou successfully block %s's attack.\n\r",
                           ch->name);
            }
            // train con stat on block success
            if (!IS_NPC(victim)) {
              stat_train(victim, "con", 8);
			  stat_train(victim, "spd", 4);
            }
          }
        }
      }
      // train con when taking damage
      if (dam > 0) {
        if (dam < 10) {
          stat_train(victim, "con", 7);
		  stat_train(victim, "spd", 7);
        } else if (dam < 20) {
          stat_train(victim, "con", 10);
		  stat_train(victim, "spd", 10);
        } else if (dam > 20) {
          stat_train(victim, "con", 15);
		  stat_train(victim, "spd", 15);
        }
      }
      ch->melee = false;
    }
    dam_message(ch, victim, dam, dt);
  }
  if (dam < 0)
    dam = 0;
  else
    victim->hit -= dam;

  if (!is_android_h(victim) && !IS_NPC(victim))
    victim->mana -=
        URANGE(0, ((double)dam / 7500 * 0.01 * victim->mana),
               victim->max_mana);

  heart_calc(victim, "");

  if (!IS_NPC(victim) && !IS_IMMORTAL(victim)) {
    if ((victim->skillssj1 <= 0 && victim->exp < ch->exp) || (victim->skillssj1 >= 1 && victim->skillssj2 <= 0 && victim->exp < ch->exp) || (victim->skillssj2 >= 1 && victim->skillssj3 <= 0 && victim->exp < ch->exp) || (victim->skillssj3 >= 1 && victim->skillssgod <= 0 && victim->exp < ch->exp) || (victim->skillssgod >= 1 && victim->skillssblue <= 0 && victim->exp < ch->exp)) {
      if (IS_NPC(ch) || (!IS_NPC(ch) && !xIS_SET(ch->act, PLR_SPAR))) {
        if (!IS_NPC(victim) && is_saiyan(victim)) {
          if (dam > 40)
            victim->rage += 4;
          else if (dam > 30)
            victim->rage += 3;
          else if (dam > 20)
            victim->rage += 2;
          else if (dam > 10)
            victim->rage += 1;
          else
            victim->rage += 1;
        }
        if (!IS_NPC(victim) && is_hb(victim)) {
          if (dam > 40)
            victim->rage += 5;
          else if (dam > 30)
            victim->rage += 4;
          else if (dam > 20)
            victim->rage += 3;
          else if (dam > 10)
            victim->rage += 2;
          else
            victim->rage += 1;
        }
        if (victim->skillssj1 <= 0)
          rage(victim, ch);
        else if (victim->skillssj1 >= 1 && victim->skillssj2 <=
                                               1)
          rage2(victim, ch);
        else if (victim->skillssj2 >=
                     1 &&
                 victim->skillssj3 <=
                     0)
          rage3(victim, ch);
        else if (victim->skillssj3 >=
                     1 &&
                 victim->skillssgod <=
                     0)
          rage4(victim, ch);
        else if (victim->skillssgod >=
                     1 &&
                 victim->skillssblue <=
                     0)
          rage5(victim, ch);
      }
    }
  }
  if (dam && ch != victim && !IS_NPC(ch) && ch->fighting && ch->fighting->xp &&
      !IS_IMMORTAL(ch) && !IS_IMMORTAL(victim) && !is_split(victim)) {
    if (dam < 1)
      dam = 1;
    xp_mod = 1;
    if (!IS_NPC(victim))
      xp_gain =
          (long double)dam / 500000 * pow(victim->worth, xp_mod);
    if (IS_NPC(victim))
      xp_gain =
          (long double)dam / 500000 * pow(victim->worth, xp_mod);
    /* Sparing and deadly combat pl gain's */
    if (!IS_NPC(ch) && !IS_NPC(victim) && !xIS_SET(ch->act, PLR_SPAR) && !xIS_SET(victim->act, PLR_SPAR)) {
      xp_gain =
          (long double)dam / 25000 * pow(victim->worth, xp_mod);
    }
    if (!IS_NPC(ch) && !IS_NPC(victim) && xIS_SET(ch->act, PLR_SPAR) && xIS_SET(victim->act, PLR_SPAR)) {
      xp_gain =
          (long double)dam / 375000 * pow(victim->worth, xp_mod);
    }
	/* If damage is greater than target health, restrict PL gain*/
	if (dam > victim->max_hit)
	  xp_gain *= ((long double)victim->max_hit / dam);
	if (ch->truepl >= 9001)
	  xp_gain *= 0.1;
    /* PL Gains cut if player is stronger than opponent */
    if (!IS_NPC(victim)) {
      if ((ch->pl / victim->pl) < 4)
        xp_gain *= 0.7;
      else if ((ch->pl / victim->pl) < 5)
        xp_gain *= 0.6;
      else if ((ch->pl / victim->pl) < 6)
        xp_gain *= 0.5;
      else if ((ch->pl / victim->pl) < 7)
        xp_gain *= 0.4;
      else if ((ch->pl / victim->pl) < 8)
        xp_gain *= 0.3;
      else if ((ch->pl / victim->pl) < 9)
        xp_gain *= 0.2;
      else if ((ch->pl / victim->pl) < 10)
        xp_gain *= 0.1;
      else
        xp_gain = 0;
    }
    if (IS_NPC(victim)) {
	  if ((ch->pl < victim->exp))
		xp_gain *= 0.88;
      else if ((ch->pl / victim->exp) < 2)
        xp_gain *= 0.5;
      else if ((ch->pl / victim->exp) < 3)
        xp_gain *= 0.333;
      else if ((ch->pl / victim->exp) < 4)
        xp_gain *= 0.25;
      else if ((ch->pl / victim->exp) < 5)
        xp_gain *= 0.2;
      else
        xp_gain = 0;
    }

    if (xp_gain < 0)
      xp_gain = 0;

    if (xIS_SET(ch->in_room->room_flags, ROOM_TIME_CHAMBER)) {
      switch (number_range(1, 4)) {
        case 1:
          xp_gain *= 1;
          break;

        case 2:
          xp_gain *= 1.5;
          break;

        case 3:
          xp_gain *= 2;
          break;

        case 4:
          xp_gain *= 3;
          break;
      }
    }
    if (ch->race == 6 && !IS_NPC(ch) && !IS_NPC(victim) && xIS_SET(ch->act, PLR_SPAR) && xIS_SET(victim->act, PLR_SPAR)) {
      xp_gain = (long double)xp_gain * 0.75;
    }
    if (ch->race == 6 && !xIS_SET(ch->act, PLR_SPAR) && !xIS_SET(victim->act, PLR_SPAR)) {
      ch->pcdata->absorb_pl += floor(xp_gain) / 2;
      xp_gain /= 2;
    }
    if ((clothingPlMod = weightedClothingPlMod(ch)) > 0)
      xp_gain += xp_gain * clothingPlMod;

    if (xIS_SET(ch->act, PLR_3XPL_GAIN)) {
      xREMOVE_BIT(ch->act, PLR_3XPL_GAIN);
      xp_gain *= 3;
    } else if (xIS_SET(ch->act, PLR_2XPL_GAIN)) {
      xREMOVE_BIT(ch->act, PLR_2XPL_GAIN);
      xp_gain *= 2;
    }
    if (ch->mod == 0)
      ch->mod = 1;
    xp_gain *= ch->mod;

    xp_gain_post =
        floor(xp_gain *
              (long double)(race_table[ch->race]->exp_multiplier /
                            100.0));

    if (is_android(ch) || is_superandroid(ch)) {
      if (xp_gain_post != 1) {
        sprintf(buf1,
                "Your tl increases by %s points.",
                num_punct_ld(xp_gain_post));
        act(AT_HIT, buf1, ch, NULL, victim, TO_CHAR);
      } else {
        sprintf(buf1, "Your tl increases by %s point.",
                num_punct_ld(xp_gain_post));
        act(AT_HIT, buf1, ch, NULL, victim, TO_CHAR);
      }
    } else {
      if (xp_gain_post < 1) {
        sprintf(buf1,
                "Your pl increases very slightly.",
                NULL);

        act(AT_HIT, buf1, ch, NULL, victim, TO_CHAR);
      } else if (xp_gain_post >= 1) {
        sprintf(buf1,
                "Your pl increases very slightly.",
                NULL);
        act(AT_HIT, buf1, ch, NULL, victim, TO_CHAR);
      }
    }

    gain_exp(ch, xp_gain);

    /* fight training for strength */
    if (xIS_SET((ch)->affected_by, AFF_ENERGYFIST)) {
      stat_train(ch, "int", 10);
    } else if (xIS_SET((ch)->affected_by, AFF_HYBRIDSTYLE)) {
      stat_train(ch, "str", 5);
      stat_train(ch, "int", 5);
    } else if (xIS_SET((ch)->affected_by, AFF_BRUISERSTYLE)) {
      stat_train(ch, "str", 10);
    } else {
      stat_train(ch, "str", 3);
    }

    ch->train += 1;
  }
  if (!IS_NPC(victim) && victim->level >= LEVEL_IMMORTAL && victim->hit < 1) {
    victim->hit = 1;
    stop_fighting(victim, true);
  }

  if (dam > 0 && dt > TYPE_HIT && !IS_AFFECTED(victim, AFF_POISON) && is_wielding_poisoned(ch) && !IS_SET(victim->immune, RIS_POISON) && !saves_poison_death(ch->level, victim)) {
    AFFECT_DATA af;

    af.type = gsn_poison;
    af.duration = 20;
    af.location = APPLY_STR;
    af.modifier = -2;
    af.bitvector = meb(AFF_POISON);
    affect_join(victim, &af);
    victim->mental_state = URANGE(20, victim->mental_state + (IS_PKILL(victim) ? 1 : 2), 100);
  }
  if (!npcvict && get_trust(victim) >= LEVEL_IMMORTAL && get_trust(ch) >= LEVEL_IMMORTAL && victim->hit < 1) {
    victim->hit = 1;
    stop_fighting(victim, true);
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && xIS_SET(ch->act, PLR_SPAR) && victim->hit <= 0) {
    victim->hit = -1;
    if (victim->fighting && victim->fighting->who->hunting && victim->fighting->who->hunting->who == victim)
      stop_hunting(victim->fighting->who);

    if (victim->fighting && victim->fighting->who->hating && victim->fighting->who->hating->who == victim)
      stop_hating(victim->fighting->who);

    stop_fighting(victim, true);

    if (ch->pcdata->clan) {
      if (victim->exp < 10000) /* 10k */
        ch->pcdata->clan->spar_wins[0]++;
      else if (victim->exp < 100000) /* 100k */
        ch->pcdata->clan->spar_wins[1]++;
      else if (victim->exp < 1000000) /* 1m */
        ch->pcdata->clan->spar_wins[2]++;
      else if (victim->exp < 10000000) /* 10m */
        ch->pcdata->clan->spar_wins[3]++;
      else if (victim->exp < 100000000) /* 100m */
        ch->pcdata->clan->spar_wins[4]++;
      else if (victim->exp < 1000000000) /* 1b */
        ch->pcdata->clan->spar_wins[5]++;
      else /* +1b */
        ch->pcdata->clan->spar_wins[6]++;
    }
    if (victim->pcdata->clan) {
      if (ch->exp < 10000) /* 10k */
        victim->pcdata->clan->spar_loss[0]++;
      else if (ch->exp < 100000) /* 100k */
        victim->pcdata->clan->spar_loss[1]++;
      else if (ch->exp < 1000000) /* 1m */
        victim->pcdata->clan->spar_loss[2]++;
      else if (ch->exp < 10000000) /* 10m */
        victim->pcdata->clan->spar_loss[3]++;
      else if (ch->exp < 100000000) /* 100m */
        victim->pcdata->clan->spar_loss[4]++;
      else if (ch->exp < 1000000000) /* 1b */
        victim->pcdata->clan->spar_loss[5]++;
      else /* +1b */
        victim->pcdata->clan->spar_loss[6]++;
    }
    ch->pcdata->spar_wins += 1;
    victim->pcdata->spar_loss += 1;
    ch->pcdata->sparcount += 1;
    victim->pcdata->sparcount += 1;
    if (ch->pcdata->nextspartime <= 0) {
      struct tm *tms;

      tms = localtime(&current_time);
      tms->tm_mday += 1;
      ch->pcdata->nextspartime = mktime(tms);
    }
    if (victim->pcdata->nextspartime <= 0) {
      struct tm *tms;

      tms = localtime(&current_time);
      tms->tm_mday += 1;
      victim->pcdata->nextspartime = mktime(tms);
    }
    adjust_hiscore("sparwins", ch, ch->pcdata->spar_wins);
    adjust_hiscore("sparloss", victim, victim->pcdata->spar_loss);

    act(AT_WHITE, "You stop fighting and spare $N's life.", ch,
        NULL, victim, TO_CHAR);
    act(AT_WHITE, "$n stops fighting and spares your life.", ch,
        NULL, victim, TO_VICT);
    act(AT_WHITE, "$n stops fighting and spares $N's life.", ch,
        NULL, victim, TO_NOTVICT);
  }
  update_pos(victim);

  if (ch->race == 6 && victim->position <= POS_STUNNED && victim->hit < 1 && !xIS_SET(ch->act, PLR_SPAR)) {
    bio_absorb(ch, victim);
  }
  if (ch->race == 27 && victim->position <= POS_STUNNED && victim->hit < 1 && !xIS_SET(ch->act, PLR_SPAR)) {
    bio_absorb(ch, victim);
  }
  else
    switch (victim->position) {
      case POS_MORTAL:
        act(AT_DYING,
            "$n is mortally wounded, and will die soon, if not aided.",
            victim, NULL, NULL, TO_ROOM);
        act(AT_DANGER,
            "You are mortally wounded, and will die soon, if not aided.",
            victim, NULL, NULL, TO_CHAR);
        break;

      case POS_INCAP:
        act(AT_DYING,
            "$n is incapacitated and will slowly die, if not aided.",
            victim, NULL, NULL, TO_ROOM);
        act(AT_DANGER,
            "You are incapacitated and will slowly die, if not aided.",
            victim, NULL, NULL, TO_CHAR);
        break;

      case POS_STUNNED:
        if (!IS_AFFECTED(victim, AFF_PARALYSIS)) {
          act(AT_ACTION,
              "$n is stunned, but will probably recover.",
              victim, NULL, NULL, TO_ROOM);
          act(AT_HURT,
              "You are stunned, but will probably recover.",
              victim, NULL, NULL, TO_CHAR);
        }
        break;

      case POS_DEAD:
        if (dt >= 0 && dt < top_sn) {
          SKILLTYPE *skill = skill_table[dt];

          if (skill->die_char && skill->die_char[0] != '\0')
            act(AT_DEAD, skill->die_char, ch, NULL,
                victim, TO_CHAR);
          if (skill->die_vict && skill->die_vict[0] != '\0')
            act(AT_DEAD, skill->die_vict, ch, NULL,
                victim, TO_VICT);
          if (skill->die_room && skill->die_room[0] != '\0')
            act(AT_DEAD, skill->die_room, ch, NULL,
                victim, TO_NOTVICT);
        }
        act(AT_DEAD, "$n is DEAD!!", victim, 0, 0, TO_ROOM);
        act(AT_DEAD, "You have been KILLED!!\n\r", victim, 0, 0,
            TO_CHAR);
        break;

      default:
        if (dam > victim->max_hit / 4) {
          act(AT_HURT, "That really did HURT!", victim, 0,
              0, TO_CHAR);
          if (number_bits(3) == 0)
            worsen_mental_state(victim, 1);
        }
        if (victim->hit < victim->max_hit / 4) {
          if (is_android(victim))
            act(AT_DANGER,
                "You wish that your wounds would stop leaking oil so much!",
                victim, 0, 0, TO_CHAR);
          else
            act(AT_DANGER,
                "You wish that your wounds would stop BLEEDING so much!",
                victim, 0, 0, TO_CHAR);
          if (number_bits(2) == 0)
            worsen_mental_state(victim, 1);
        }
        break;
    }

  /* sleep spells and extremely wounded folks. */
  if ((ch)->position == POS_SLEEPING && !IS_AFFECTED(victim, AFF_PARALYSIS)) {
    if (victim->fighting && victim->fighting->who->hunting && victim->fighting->who->hunting->who == victim)
      stop_hunting(victim->fighting->who);

    if (victim->fighting && victim->fighting->who->hating && victim->fighting->who->hating->who == victim)
      stop_hating(victim->fighting->who);

    if (!npcvict && IS_NPC(ch))
      stop_fighting(victim, true);
    else
      stop_fighting(victim, false);
  }
  /* payoff for killing things. */
  if (victim->position == POS_DEAD) {
    group_gain(ch, victim);
    /* stuff for handling loss of kai and demon ranks. */
    if (!IS_NPC(ch) && !IS_NPC(victim)) {
      if (is_kaio(ch) && kairanked(victim)) {
        if (ch->kairank < victim->kairank) {
          int lower = 0;
          int higher = 0;

          sprintf(kbuf,
                  "%s has stolen %s's kaioshin title %s",
                  ch->name, victim->name,
                  get_kai(victim));
          to_channel(kbuf, CHANNEL_MONITOR,
                     "Monitor", LEVEL_IMMORTAL);
          sprintf(kbuf,
                  "%s has stolen %s's kaioshin title of %s",
                  ch->name, victim->name,
                  get_kai(victim));
          do_info(ch, kbuf);
          lower = ch->kairank;
          higher = victim->kairank;
          STRFREE(kaioshin[higher - 1]);
          kaioshin[higher - 1] =
              STRALLOC(ch->name);
          ch->kairank = higher;
          victim->kairank = lower;
          if (victim->kairank == 0) {
            sprintf(kbuf,
                    "%s is no longer a kaioshin",
                    victim->name);
            do_info(ch, kbuf);
            if (xIS_SET(victim->affected_by,
                        AFF_SANCTUARY))
              xREMOVE_BIT(victim->affected_by,
                          AFF_SANCTUARY);
          } else {
            sprintf(kbuf,
                    "%s has dropped to kaioshin title %s",
                    victim->name,
                    get_kai(victim));
            STRFREE(kaioshin[lower - 1]);
            kaioshin[lower - 1] =
                STRALLOC(victim->name);

            do_info(ch, kbuf);
          }
          save_sysdata(sysdata);
          save_char_obj(ch);
          save_char_obj(victim);
        }
        kaiopres = true;
      }
      if (is_demon(ch) && demonranked(victim)) {
        if (ch->demonrank < victim->demonrank) {
          int lower = 0;
          int higher = 0;
          bool cg = false, vg = false;

          /* greater demon */
          bool cw = false, vw = false;

          /* demon warlord */
          bool vk = false;

          /* demon king */
          int c = 0;
          int d = 0;
          int e = 0;

          if (demonranked(ch)) {
            if (ch->demonrank == 1) {
              for (c = 0; c < 6; c++) {
                if (!str_cmp(greaterdemon
                                 [c],
                             ch->name)) {
                  cg = true;
                  d = c;
                  break;
                }
              }
            } else if (ch->demonrank == 2) {
              for (c = 0; c < 3; c++) {
                if (!str_cmp(demonwarlord
                                 [c],
                             ch->name)) {
                  cw = true;
                  d = c;
                  break;
                }
              }
            }
          }
          if (demonranked(victim)) {
            if (victim->demonrank == 1) {
              for (c = 0; c < 6; c++) {
                if (!str_cmp(greaterdemon
                                 [c],
                             victim->name)) {
                  vg = true;
                  e = c;
                  break;
                }
              }
            } else if (victim->demonrank ==
                       2) {
              for (c = 0; c < 3; c++) {
                if (!str_cmp(demonwarlord
                                 [c],
                             victim->name)) {
                  vw = true;
                  e = c;
                  break;
                }
              }
            } else if (victim->demonrank ==
                       3) {
              if (!str_cmp(demonking,
                           victim->name)) {
                vk = true;
              }
            }
          }
          sprintf(kbuf,
                  "%s has stolen %s's demon title %s",
                  ch->name, victim->name,
                  get_demon(victim));
          to_channel(kbuf, CHANNEL_MONITOR,
                     "Monitor", LEVEL_IMMORTAL);
          sprintf(kbuf,
                  "%s has stolen %s's demon title of %s",
                  ch->name, victim->name,
                  get_demon(victim));
          do_info(ch, kbuf);
          lower = ch->demonrank;
          higher = victim->demonrank;
          if (vg) {
            STRFREE(greaterdemon[e]);
            greaterdemon[e] =
                STRALLOC(ch->name);
          } else if (vw) {
            STRFREE(demonwarlord[e]);
            demonwarlord[e] =
                STRALLOC(ch->name);
          } else if (vk) {
            STRFREE(demonking);
            demonking = STRALLOC(ch->name);
            if (xIS_SET(victim->affected_by,
                        AFF_SANCTUARY))
              xREMOVE_BIT(victim->affected_by,
                          AFF_SANCTUARY);
          }
          ch->demonrank = higher;
          victim->demonrank = lower;
          if (victim->demonrank == 0) {
            sprintf(kbuf,
                    "%s is no longer a demon",
                    victim->name);
            do_info(ch, kbuf);
          } else {
            sprintf(kbuf,
                    "%s has dropped to demon title %s",
                    victim->name,
                    get_demon(victim));
            if (cg) {
              STRFREE(greaterdemon
                          [d]);
              greaterdemon[d] =
                  STRALLOC(victim->name);
            } else if (cw) {
              STRFREE(demonwarlord
                          [d]);
              demonwarlord[d] =
                  STRALLOC(victim->name);
            }
            do_info(ch, kbuf);
          }
          save_sysdata(sysdata);
          save_char_obj(ch);
          save_char_obj(victim);
        }
        demonpres = true;
      }
    }
    if (!IS_NPC(ch) && (is_bio(ch) || is_android(ch) || is_superandroid(ch)) && ch->pcdata->learned[gsn_self_destruct] > 0) {
      if (ch->exp > ch->pl) {
        if (IS_NPC(victim)) {
          if (victim->exp >= ch->exp)
            ch->pcdata->sd_charge++;
        } else {
          if (victim->pl >= ch->exp)
            ch->pcdata->sd_charge++;
        }
      } else {
        if (IS_NPC(victim)) {
          if (victim->exp >= ch->pl)
            ch->pcdata->sd_charge++;
        } else {
          if (victim->pl >= ch->pl)
            ch->pcdata->sd_charge++;
        }
      }
    }

    if (!npcvict) {
      /* Bounty stuff begins - Garinan */
      if (!IS_NPC(ch) && !IS_NPC(victim)) {
        victim->pcdata->pk_timer = 0;
        if (!str_cmp(victim->name, ch->pcdata->hunting) && xIS_SET(victim->act, PLR_BOUNTY) && victim->pcdata->bounty > 0) {
          ch->pcdata->bowed +=
              victim->pcdata->bounty;
          ch->pcdata->bkills++;
          adjust_hiscore("bounty", ch,
                         ch->pcdata->bkills);
          sprintf(log_buf,
                  "You have claimed %d zeni from the head of %s!\n\r",
                  victim->pcdata->bounty,
                  victim->name);
          send_to_char(log_buf, ch);
          send_to_char("You may collect your earnings at any bounty officer.&R\n\r",
                       ch);
          xREMOVE_BIT(victim->act, PLR_BOUNTY);
          victim->pcdata->bounty = 0;
          victim->pcdata->b_timeleft = 1440;
          DISPOSE(ch->pcdata->hunting);
          ch->pcdata->hunting = str_dup("");
          sprintf(log_buf,
                  "%s has claimed the bounty from the head of %s!",
                  ch->name, victim->name);
          do_info(ch, log_buf);
        } else if (!str_cmp(ch->name, victim->pcdata->hunting) && xIS_SET(ch->act, PLR_BOUNTY) && ch->pcdata->bounty > 0) {
          pager_printf_color(victim,
                             "You have lost the right to take the bounty on %s's head",
                             ch->name);
          DISPOSE(victim->pcdata->hunting);
          victim->pcdata->hunting = str_dup("");
        }
      }
      /* Bounty stuff ends - Garinan */
      sprintf(log_buf, "%s (%d) killed by %s at %d",
              victim->name,
              victim->level,
              (IS_NPC(ch) ? ch->short_descr : ch->name),
              victim->in_room->vnum);
      log_string(log_buf);
      to_channel(log_buf, CHANNEL_MONITOR, "Monitor",
                 LEVEL_IMMORTAL);
      sprintf(buf, "%s has been killed by %s", victim->name,
              (IS_NPC(ch) ? ch->short_descr : ch->name));
      do_info(ch, buf);

      if (!IS_NPC(ch) && ch->pcdata->clan && ch->pcdata->clan->clan_type != CLAN_ORDER && ch->pcdata->clan->clan_type != CLAN_GUILD && victim != ch) {
        sprintf(filename, "%s%s.record", CLAN_DIR,
                ch->pcdata->clan->short_name);
        sprintf(log_buf,
                "&P(%2d) %-12s &wvs &P(%2d) %s &P%s ... &w%s",
                ch->level, ch->name, victim->level,
                !CAN_PKILL(victim) ? "&W<Peaceful>" : victim->pcdata->clan ? victim->pcdata->clan->badge
                                                                           : "&P(&WRonin&P)",
                victim->name, ch->in_room->area->name);
        if (victim->pcdata->clan && victim->pcdata->clan->name ==
                                        ch->pcdata->clan->name)
          ;
        else
          append_to_file(filename, log_buf);
      }
      if (!IS_IMMORTAL(ch) && !IS_IMMORTAL(victim)) {
        if (can_use_skill(victim, number_percent(),
                          gsn_preservation)) {
          learn_from_success(victim,
                             gsn_preservation);
          preservation = true;
        } else
          learn_from_failure(victim,
                             gsn_preservation);

        if (can_use_skill(victim, number_percent(),
                          gsn_regeneration)) {
          learn_from_success(victim,
                             gsn_regeneration);
          biopres = true;
        } else
          learn_from_failure(victim,
                             gsn_regeneration);

        if (IS_SET(victim->pcdata->flags, PCFLAG_IMMORTALITY))
          immortal = true;

        if (!IS_NPC(ch) && !IS_NPC(victim)) {
          if (preservation || biopres) {
            if (warpres || immortal)
              los =
                  (long double)
                      victim->exp *
                  0.01;
            else if ((kairanked(ch) &&
                      demonranked(victim)) ||
                     (demonranked(ch) &&
                      kairanked(victim)) ||
                     kaiopres || demonpres)
              los =
                  (long double)
                      victim->exp *
                  0.015;
            else
              los =
                  (long double)
                      victim->exp *
                  0.02;
          } else {
            if (warpres || immortal)
              los =
                  (long double)
                      victim->exp *
                  0.01;
            else if ((kairanked(ch) &&
                      demonranked(victim)) ||
                     (demonranked(ch) &&
                      kairanked(victim)) ||
                     kaiopres || demonpres)
              los =
                  (long double)
                      victim->exp *
                  0.025;
            else
              los =
                  (long double)
                      victim->exp *
                  0.03;
          }
        }
      } else {
        if (is_split(ch)) {
          if (immortal)
            los =
                (long double)
                    victim->exp *
                0.01;
          else if (preservation || biopres)
            los =
                (long double)
                    victim->exp *
                0.02;
          else
            los =
                (long double)
                    victim->exp *
                0.03;
        } else {
          if (immortal)
            los =
                (long double)
                    victim->exp *
                0.01;
          else if (preservation || biopres)
            los =
                (long double)
                    victim->exp *
                0.03;
          else
            los =
                (long double)
                    victim->exp *
                0.085;
        }
      }
      gain_exp(victim, 0 - los);
      if (IS_NPC(ch) && !IS_NPC(victim)) {
        if (is_hunting(ch, victim))
          stop_hunting(ch);

        if (is_hating(ch, victim))
          stop_hating(ch);
      }
    } else if (!IS_NPC(ch)) /* keep track of mob vnum killed */
      add_kill(ch, victim);

    check_killer(ch, victim);
    if (!IS_NPC(ch) && ch->pcdata->clan)
      update_member(ch);
    if (!IS_NPC(victim) && victim->pcdata->clan)
      update_member(victim);

    if (ch->in_room == victim->in_room)
      loot = legal_loot(ch, victim);
    else
      loot = false;

    if (!IS_NPC(victim) && (preservation || biopres || immortal)) {
      stop_fighting(victim, true);
      make_corpse(victim, ch);
      victim->mana = victim->max_mana;
      victim->hit = 100;
      for (;;) {
        pRoomIndex =
            get_room_index(number_range(victim->in_room->area->low_r_vnum,
                                        victim->in_room->area->hi_r_vnum));
        if (pRoomIndex) {
          if (!xIS_SET(pRoomIndex->room_flags,
                       ROOM_PRIVATE) &&
              !xIS_SET(pRoomIndex->room_flags,
                       ROOM_SOLITARY) &&
              !xIS_SET(pRoomIndex->room_flags,
                       ROOM_NO_ASTRAL) &&
              !xIS_SET(pRoomIndex->room_flags,
                       ROOM_PROTOTYPE) &&
              has_exits(pRoomIndex)) {
            break;
          }
        }
      }
      if (victim->fighting)
        stop_fighting(victim, true);
      if (preservation) {
        act(AT_RED,
            "With $n's final ounce of energy, $n coughs up an egg and launches it in to the air!",
            victim, NULL, NULL, TO_ROOM);
        send_to_char("&RWith your final ounce of energy, you cough up an egg and launch it in to the air!",
                     victim);
        char_from_room(victim);
        char_to_room(victim, pRoomIndex);
        victim->position = POS_STANDING;
        act(AT_MAGIC,
            "An egg falls from the sky creating a small crater as it hits the ground.",
            victim, NULL, NULL, TO_ROOM);
        act(AT_MAGIC,
            "The egg begins to crack open as $n pops out!",
            victim, NULL, NULL, TO_ROOM);
        act(AT_MAGIC,
            "Your egg lands with a THUD!  You break free of your egg and emerge reborn!",
            victim, NULL, NULL, TO_CHAR);
      } else if (biopres) {
        act(AT_DGREEN,
            "A single cell of $n survives. Within moments, $n begins to regenerate $s body. A shapeless mass of flesh sprouts new legs...new arms...new wings...and a new head. Suddenly, $n is made whole again.",
            victim, NULL, NULL, TO_ROOM);
        ch_printf(victim,
                  "&gA single one of your cells survives. Within moments, you begin to regenerate your body. Your shapeless mass of flesh sprouts new legs...new arms...new wings...and a new head. Suddenly, you are made whole again.");
        victim->position = POS_STANDING;
      } else if (immortal) {
        act(AT_WHITE,
            "As $n lays dead on the floor, $s blood suddenly flows back into $s wounds. $n's wounds heal back up on their own, as $s body glows with a faint, supernatural light. $*s eyes suddenly snap back open as $e is alive again.",
            victim, NULL, NULL, TO_ROOM);
        ch_printf(victim,
                  "&WAs you lay dead on the ground, your powers of immortality suddenly\n\r"
                  "kick in. Your blood flows back into your wounds, and they heal up on\n\r"
                  "their own. You begin to feel your body again as you come back to\n\r"
                  "life. You open your eyes and look out at the land of living, rather\n\r"
                  "than the land of the dead.\n\r");
        victim->position = POS_STANDING;
      }
      do_look(victim, "auto");

      if (IS_SET(sysdata.save_flags, SV_DEATH))
        save_char_obj(victim);
    } else {
      set_cur_char(victim);
      raw_kill(ch, victim);
      victim = NULL;
    }
    if (!IS_NPC(ch) && loot) {
      if (xIS_SET(ch->act, PLR_AUTOGOLD)) {
        init_gold = ch->gold;
        do_get(ch, "zeni corpse");
        new_gold = ch->gold;
        gold_diff = (new_gold - init_gold);
      }
      if (xIS_SET(ch->act, PLR_AUTOLOOT) && victim != ch)
        do_get(ch, "all corpse");
      else
        do_look(ch, "in corpse");
      if (xIS_SET(ch->act, PLR_AUTOSAC))
        do_sacrifice(ch, "corpse");
    }

    if (IS_SET(sysdata.save_flags, SV_KILL))
      save_char_obj(ch);
    return rVICT_DIED;
  }
  if (victim == ch)
    return rNONE;

  /* wimp out? */
  if (npcvict && dam > 0) {
    if ((xIS_SET(victim->act, ACT_WIMPY) && number_bits(1) == 0 && victim->hit < victim->max_hit / 2) || (IS_AFFECTED(victim, AFF_CHARM) && victim->master && victim->master->in_room != victim->in_room)) {
      start_fearing(victim, ch);
      stop_hunting(victim);
      do_flee(victim, "");
    }
  }
  if (!npcvict && victim->hit > 0 && victim->hit <= victim->wimpy && victim->wait == 0)
    do_flee(victim, "");
  else if (!npcvict && xIS_SET(victim->act, PLR_FLEE))
    do_flee(victim, "");
  tail_chain();
  return rNONE;
}

/*
 * Changed is_safe to have the show_messg boolian.  This is so if you don't
 * want to show why you can't kill someone you can't turn it off.  This is
 * useful for things like area attacks.  --Shaddai
 */
bool is_safe(CHAR_DATA *ch, CHAR_DATA *victim, bool show_messg) {
  if (char_died(victim) || char_died(ch))
    return true;

  /* Thx Josh! */
  if (who_fighting(ch) == ch)
    return false;

  if (!victim) { /* Gonna find this is_safe crash bug
                  * -Blod */
    bug("Is_safe: %s opponent does not exist!", ch->name);
    return true;
  }
  if (!victim->in_room) {
    bug("Is_safe: %s has no physical location!", victim->name);
    return true;
  }
  if (xIS_SET(victim->in_room->room_flags, ROOM_SAFE)) {
    if (show_messg) {
      set_char_color(AT_MAGIC, ch);
      send_to_char("A magical force prevents you from attacking.\n\r",
                   ch);
    }
    return true;
  }
  if (!IS_NPC(victim) && !IS_NPC(ch)) {
    if (xIS_SET(victim->act, PLR_BOUNTY) && victim->pcdata->bounty > 0 && !str_cmp(victim->name, ch->pcdata->hunting)) {
      if (!xIS_SET(victim->act, PLR_PK1) && !xIS_SET(victim->act, PLR_PK2) && !IS_HC(victim)) {
        return true;
      } else {
        return false;
      }
    }
  }
  if (IS_PACIFIST(ch)) { /* Fireblade */
    if (show_messg) {
      set_char_color(AT_MAGIC, ch);
      ch_printf(ch,
                "You are a pacifist and will not fight.\n\r");
    }
    return true;
  }
  if (IS_PACIFIST(victim)) { /* Gorog */
    char buf[MAX_STRING_LENGTH];

    if (show_messg) {
      sprintf(buf, "%s is a pacifist and will not fight.\n\r",
              capitalize(victim->short_descr));
      set_char_color(AT_MAGIC, ch);
      send_to_char(buf, ch);
    }
    return true;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && ch != victim && !in_arena(ch)) {
    if (show_messg) {
      set_char_color(AT_IMMORT, ch);
      send_to_char("The gods have forbidden player killing in this area.\n\r",
                   ch);
    }
    return true;
  }
  if (xIS_SET(ch->in_room->room_flags, ROOM_ARENA) && (!xIS_SET(ch->act, PLR_SPAR) || !xIS_SET(victim->act, PLR_SPAR))) {
    if (show_messg)
      send_to_char("You must SPAR someone in an arena.\n\r",
                   ch);
    return true;
  }
  if (IS_NPC(ch) || IS_NPC(victim))
    return false;
  if (!xIS_SET(ch->act, PLR_SPAR) && !xIS_SET(victim->act, PLR_SPAR)) {
    if (get_timer(victim, TIMER_PKILLED) > 0) {
      if (show_messg) {
        set_char_color(AT_GREEN, ch);
        send_to_char("That character has died within the last 30 minutes.\n\r",
                     ch);
      }
      return true;
    }
    if (get_timer(ch, TIMER_PKILLED) > 0) {
      if (show_messg) {
        set_char_color(AT_GREEN, ch);
        send_to_char("You have been killed within the last 30 minutes.\n\r",
                     ch);
      }
      return true;
    }
  }
  return false;
}

/*
 * just verify that a corpse looting is legal
 */
bool legal_loot(CHAR_DATA *ch, CHAR_DATA *victim) {
  /* anyone can loot mobs */
  if (IS_NPC(victim))
    return true;
  /* non-charmed mobs can loot anything */
  if (IS_NPC(ch) && !ch->master)
    return true;
  /* members of different clans can loot too! -Thoric */
  if (!IS_NPC(ch) && !IS_NPC(victim) && IS_SET(ch->pcdata->flags, PCFLAG_DEADLY) && IS_SET(victim->pcdata->flags, PCFLAG_DEADLY))
    return true;
  return false;
}

/*
 * See if an attack justifies a KILLER flag.
 */
void check_killer(CHAR_DATA *ch, CHAR_DATA *victim) {
  bool split = false;

  if (xIS_SET((ch)->affected_by, AFF_SPLIT_FORM))
    split = true;

  /*
   * NPC's are fair game.
   */
  if (IS_NPC(victim)) {
    if (!IS_NPC(ch)) {
      int level_ratio;

      level_ratio = URANGE(1, ch->level / victim->level, 50);
      if (ch->pcdata->clan)
        ch->pcdata->clan->mkills++;
      ch->pcdata->mkills++;
      ch->in_room->area->mkills++;
      if (ch->race == 6)
        update_absorb(ch, victim);
      adjust_hiscore("mkills", ch, ch->pcdata->mkills);
      if (ch->pcdata->deity) {
        if (victim->race == ch->pcdata->deity->npcrace)
          adjust_favor(ch, 3, level_ratio);
        else if (victim->race ==
                 ch->pcdata->deity->npcfoe)
          adjust_favor(ch, 17, level_ratio);
        else
          adjust_favor(ch, 2, level_ratio);
      }
    }
    return;
  }
  /*
   * If you kill yourself nothing happens.
   */

  if (ch == victim)
    return;

  /*
         * Any character in the arena is ok to kill.
         * Added pdeath and pkills here
         if ( in_arena( ch ) )
         {
         if ( !IS_NPC(ch) && !IS_NPC(victim) )
         {
         ch->pcdata->pkills++;
         victim->pcdata->pdeaths++;
         adjust_hiscore( "pkill", ch, ch->pcdata->pkills );
         }
         else if ( (IS_NPC(ch) && is_split(ch))
         && !IS_NPC(victim) )
         {
         if( !ch->master )
         {
         log("%s just killed the split form %s without the owner online",
         victim->name, ch->short_descr );
         }
         ch->master->pcdata->pkills++;
         victim->pcdata->pdeaths++;
         adjust_hiscore( "pkill", ch, ch->master->pcdata->pkills );
         }

         return;
         }
         */

  /*
   * So are killers and thieves.
   */
  if (xIS_SET(victim->act, PLR_KILLER) || xIS_SET(victim->act, PLR_THIEF)) {
    if (!IS_NPC(ch)) {
      if (ch->pcdata->clan) {
        if (victim->exp < 10000) /* 10k */
          ch->pcdata->clan->pkills[0]++;
        else if (victim->exp < 100000) /* 100k */
          ch->pcdata->clan->pkills[1]++;
        else if (victim->exp < 1000000) /* 1m */
          ch->pcdata->clan->pkills[2]++;
        else if (victim->exp < 10000000) /* 10m */
          ch->pcdata->clan->pkills[3]++;
        else if (victim->exp < 100000000) /* 100m */
          ch->pcdata->clan->pkills[4]++;
        else if (victim->exp < 1000000000) /* 1b */
          ch->pcdata->clan->pkills[5]++;
        else /* +1b */
          ch->pcdata->clan->pkills[6]++;
      }
      ch->pcdata->pkills++;
      ch->in_room->area->pkills++;
      if (ch->race == 6)
        update_absorb(ch, victim);
      adjust_hiscore("pkill", ch, ch->pcdata->pkills); /* cronel hiscore */
    }
    return;
  }
  /* clan checks                                      -Thoric */
  if (!IS_NPC(ch) && !IS_NPC(victim) && IS_SET(ch->pcdata->flags, PCFLAG_DEADLY) && IS_SET(victim->pcdata->flags, PCFLAG_DEADLY)) {
    /* not of same clan? Go ahead and kill!!! */
    if (!ch->pcdata->clan || !victim->pcdata->clan || (ch->pcdata->clan->clan_type != CLAN_NOKILL && victim->pcdata->clan->clan_type != CLAN_NOKILL && ch->pcdata->clan != victim->pcdata->clan)) {
      if (ch->pcdata->clan) {
        if (victim->exp < 10000) /* 10k */
          ch->pcdata->clan->pkills[0]++;
        else if (victim->exp < 100000) /* 100k */
          ch->pcdata->clan->pkills[1]++;
        else if (victim->exp < 1000000) /* 1m */
          ch->pcdata->clan->pkills[2]++;
        else if (victim->exp < 10000000) /* 10m */
          ch->pcdata->clan->pkills[3]++;
        else if (victim->exp < 100000000) /* 100m */
          ch->pcdata->clan->pkills[4]++;
        else if (victim->exp < 1000000000) /* 1b */
          ch->pcdata->clan->pkills[5]++;
        else /* +1b */
          ch->pcdata->clan->pkills[6]++;
      }
      ch->pcdata->pkills++;
      if (ch->race == 6)
        update_absorb(ch, victim);

      adjust_hiscore("pkill", ch, ch->pcdata->pkills); /* cronel hiscore */
      ch->hit = ch->max_hit;
      ch->mana = ch->max_mana;
      ch->move = ch->max_move;
      update_pos(victim);
      if (victim != ch) {
        act(AT_MAGIC,
            "Bolts of blue energy rise from the corpse, seeping into $n.",
            ch, victim->name, NULL, TO_ROOM);
        act(AT_MAGIC,
            "Bolts of blue energy rise from the corpse, seeping into you.",
            ch, victim->name, NULL, TO_CHAR);
      }
      if (victim->pcdata->clan) {
        if (victim->exp < 10000) /* 10k */
          victim->pcdata->clan->pdeaths[0]++;
        else if (victim->exp < 100000) /* 100k */
          victim->pcdata->clan->pdeaths[1]++;
        else if (victim->exp < 1000000) /* 1m */
          victim->pcdata->clan->pdeaths[2]++;
        else if (victim->exp < 10000000) /* 10m */
          victim->pcdata->clan->pdeaths[3]++;
        else if (victim->exp < 100000000) /* 100m */
          victim->pcdata->clan->pdeaths[4]++;
        else if (victim->exp < 1000000000) /* 1b */
          victim->pcdata->clan->pdeaths[5]++;
        else /* +1b */
          victim->pcdata->clan->pdeaths[6]++;
      }
      victim->pcdata->pdeaths++;
      adjust_hiscore("deaths", victim,
                     (victim->pcdata->pdeaths +
                      victim->pcdata->mdeaths));
      adjust_favor(victim, 11, 1);
      adjust_favor(ch, 2, 1);
      if (victim->pcdata->pk_timer > 0)
        victim->pcdata->pk_timer = 0;
      if (!IS_HC(victim))
        add_timer(victim, TIMER_PKILLED, 690, NULL, 0);
      else
        add_timer(victim, TIMER_PKILLED, 345, NULL, 0);
      WAIT_STATE(victim, 3 * PULSE_VIOLENCE);
      /* xSET_BIT(victim->act, PLR_PK); */
      return;
    }
  } else if (IS_NPC(ch) && !IS_NPC(victim) && is_split(ch)) {
    if (ch->master)
      ch->master->pcdata->pkills++;
    victim->pcdata->pdeaths++;
    adjust_hiscore("deaths", victim,
                   (victim->pcdata->pdeaths +
                    victim->pcdata->mdeaths));
    if (!IS_HC(victim))
      add_timer(victim, TIMER_PKILLED, 690, NULL, 0);
    else
      add_timer(victim, TIMER_PKILLED, 345, NULL, 0);
    if (victim->pcdata->pk_timer > 0)
      victim->pcdata->pk_timer = 0;
    WAIT_STATE(victim, 3 * PULSE_VIOLENCE);
    return;
  }
  /*
   * Charm-o-rama.
   */
  if (IS_AFFECTED(ch, AFF_CHARM)) {
    if (!ch->master) {
      char buf[MAX_STRING_LENGTH];

      sprintf(buf, "Check_killer: %s bad AFF_CHARM",
              IS_NPC(ch) ? ch->short_descr : ch->name);
      bug(buf, 0);
      affect_strip(ch, gsn_charm_person);
      xREMOVE_BIT(ch->affected_by, AFF_CHARM);
      return;
    }
    /* stop_follower( ch ); */
    if (ch->master)
      check_killer(ch->master, victim);
    return;
  }
  /*
   * NPC's are cool of course (as long as not charmed).
   * Hitting yourself is cool too (bleeding).
   * So is being immortal (Alander's idea).
   * And current killers stay as they are.
   */

  if (IS_NPC(ch)) {
    if (!IS_NPC(victim)) {
      int level_ratio;

      if (!IS_HC(victim))
        add_timer(victim, TIMER_PKILLED, 690, NULL, 0);
      else
        add_timer(victim, TIMER_PKILLED, 345, NULL, 0);
      if (victim->pcdata->clan) {
        /*
         * if( split ) {
         * victim->pcdata->clan->pdeaths++;
         * ch->master->pcdata->clan->pkills++; }
         * else
         */
        victim->pcdata->clan->mdeaths++;
      }
      if (split) {
        victim->pcdata->pdeaths++;
        ch->master->pcdata->pkills++;
        victim->in_room->area->pdeaths++;
      } else {
        victim->pcdata->mdeaths++;
        victim->in_room->area->mdeaths++;
      }
      adjust_hiscore("deaths", victim,
                     (victim->pcdata->pdeaths +
                      victim->pcdata->mdeaths));
      level_ratio = URANGE(1, ch->level / victim->level, 50);
      if (victim->pcdata->deity) {
        if (ch->race == victim->pcdata->deity->npcrace)
          adjust_favor(victim, 12, level_ratio);
        else if (ch->race ==
                 victim->pcdata->deity->npcfoe)
          adjust_favor(victim, 15, level_ratio);
        else
          adjust_favor(victim, 11, level_ratio);
      }
    }
    return;
  }
  if (!IS_NPC(ch)) {
    if (ch->pcdata->clan)
      ch->pcdata->clan->illegal_pk++;
    ch->pcdata->illegal_pk++;
    ch->in_room->area->illegal_pk++;
  }
  if (!IS_NPC(victim)) {
    if (victim->pcdata->clan) {
      if (victim->exp < 10000) /* 10k */
        victim->pcdata->clan->pdeaths[0]++;
      else if (victim->exp < 100000) /* 100k */
        victim->pcdata->clan->pdeaths[1]++;
      else if (victim->exp < 1000000) /* 1m */
        victim->pcdata->clan->pdeaths[2]++;
      else if (victim->exp < 10000000) /* 10m */
        victim->pcdata->clan->pdeaths[3]++;
      else if (victim->exp < 100000000) /* 100m */
        victim->pcdata->clan->pdeaths[4]++;
      else if (victim->exp < 1000000000) /* 1b */
        victim->pcdata->clan->pdeaths[5]++;
      else /* +1b */
        victim->pcdata->clan->pdeaths[6]++;
    }
    adjust_hiscore("deaths", victim,
                   (victim->pcdata->pdeaths +
                    victim->pcdata->mdeaths));
    victim->pcdata->pdeaths++;
    victim->in_room->area->pdeaths++;
  }
  if (xIS_SET(ch->act, PLR_KILLER))
    return;

  set_char_color(AT_WHITE, ch);
  send_to_char("A strange feeling grows deep inside you, and a tingle goes up your spine...\n\r",
               ch);
  set_char_color(AT_IMMORT, ch);
  send_to_char("A deep voice booms inside your head, 'Thou shall now be known as a deadly murderer!!!'\n\r",
               ch);
  set_char_color(AT_WHITE, ch);
  send_to_char("You feel as if your soul has been revealed for all to see.\n\r",
               ch);
  if (xIS_SET(ch->act, PLR_ATTACKER))
    xREMOVE_BIT(ch->act, PLR_ATTACKER);
  save_char_obj(ch);
  return;
}

/*
 * See if an attack justifies a ATTACKER flag.
 */
void check_attacker(CHAR_DATA *ch, CHAR_DATA *victim) {
  return;
  /*
   * Made some changes to this function Apr 6/96 to reduce the prolifiration
   * of attacker flags in the realms. -Narn
   */
  /*
   * NPC's are fair game.
   * So are killers and thieves.
   */
  if (IS_NPC(victim) || xIS_SET(victim->act, PLR_KILLER) || xIS_SET(victim->act, PLR_THIEF))
    return;

  /* deadly char check */
  if (!IS_NPC(ch) && !IS_NPC(victim) && CAN_PKILL(ch) && CAN_PKILL(victim))
    return;

  /*
   * Charm-o-rama.
   */
  if (IS_AFFECTED(ch, AFF_CHARM)) {
    if (!ch->master) {
      char buf[MAX_STRING_LENGTH];

      sprintf(buf, "Check_attacker: %s bad AFF_CHARM",
              IS_NPC(ch) ? ch->short_descr : ch->name);
      bug(buf, 0);
      affect_strip(ch, gsn_charm_person);
      xREMOVE_BIT(ch->affected_by, AFF_CHARM);
      return;
    }
    return;
  }
  /*
   * NPC's are cool of course (as long as not charmed).
   * Hitting yourself is cool too (bleeding).
   * So is being immortal (Alander's idea).
   * And current killers stay as they are.
   */
  if (IS_NPC(ch) || ch == victim || ch->level >= LEVEL_IMMORTAL || xIS_SET(ch->act, PLR_ATTACKER) || xIS_SET(ch->act, PLR_KILLER))
    return;

  xSET_BIT(ch->act, PLR_ATTACKER);
  save_char_obj(ch);
  return;
}

/*
 * Set position of a victim.
 */
void update_pos(CHAR_DATA *victim) {
  if (!victim) {
    bug("update_pos: null victim", 0);
    return;
  }
  if (victim->hit > 0) {
    if (victim->position <= POS_STUNNED)
      victim->position = POS_STANDING;
    if (IS_AFFECTED(victim, AFF_PARALYSIS))
      victim->position = POS_STUNNED;
    return;
  }
  if (IS_NPC(victim) || victim->hit <= -11) {
    if (victim->mount) {
      act(AT_ACTION, "$n falls from $N.",
          victim, NULL, victim->mount, TO_ROOM);
      xREMOVE_BIT(victim->mount->act, ACT_MOUNTED);
      victim->mount = NULL;
    }
    victim->position = POS_DEAD;
    return;
  }
  if (victim->hit <= -6)
    victim->position = POS_MORTAL;
  else if (victim->hit <= -3)
    victim->position = POS_INCAP;
  else
    victim->position = POS_STUNNED;

  if (victim->position > POS_STUNNED && IS_AFFECTED(victim, AFF_PARALYSIS))
    victim->position = POS_STUNNED;

  if (victim->mount) {
    act(AT_ACTION, "$n falls unconscious from $N.",
        victim, NULL, victim->mount, TO_ROOM);
    xREMOVE_BIT(victim->mount->act, ACT_MOUNTED);
    victim->mount = NULL;
  }
  return;
}

/*
 * Start fights.
 */
void set_fighting(CHAR_DATA *ch, CHAR_DATA *victim) {
  FIGHT_DATA *fight;

  if (ch->fighting) {
    char buf[MAX_STRING_LENGTH];

    sprintf(buf, "Set_fighting: %s -> %s (already fighting %s)",
            ch->name, victim->name, ch->fighting->who->name);
    bug(buf, 0);
    return;
  }
  if (IS_AFFECTED(ch, AFF_SLEEP))
    affect_strip(ch, gsn_sleep);

  /* Limit attackers -Thoric */
  if (victim->num_fighting > max_fight(victim)) {
    send_to_char("There are too many people fighting for you to join in.\n\r",
                 ch);
    return;
  }
  CREATE(fight, FIGHT_DATA, 1);
  fight->who = victim;
  fight->xp = (int)xp_compute(ch, victim) * 0.85;
  fight->align = align_compute(ch, victim);
  if (!IS_NPC(ch) && IS_NPC(victim))
    fight->timeskilled = times_killed(ch, victim);
  ch->num_fighting = 1;
  ch->fighting = fight;
  if (IS_NPC(ch))
    ch->position = POS_FIGHTING;
  else
    switch (ch->style) {
      case (STYLE_EVASIVE):
        ch->position = POS_EVASIVE;
        break;
      case (STYLE_DEFENSIVE):
        ch->position = POS_DEFENSIVE;
        break;
      case (STYLE_AGGRESSIVE):
        ch->position = POS_AGGRESSIVE;
        break;
      case (STYLE_BERSERK):
        ch->position = POS_BERSERK;
        break;
      default:
        ch->position = POS_FIGHTING;
    }
  victim->num_fighting++;
  if (victim->switched && IS_AFFECTED(victim->switched, AFF_POSSESS)) {
    send_to_char("You are disturbed!\n\r", victim->switched);
    do_return(victim->switched, "");
  }
  return;
}

CHAR_DATA *
who_fighting(CHAR_DATA *ch) {
  if (!ch) {
    bug("who_fighting: null ch", 0);
    return NULL;
  }
  if (!ch->fighting)
    return NULL;
  return ch->fighting->who;
}

void free_fight(CHAR_DATA *ch) {
  if (!ch) {
    bug("Free_fight: null ch!", 0);
    return;
  }
  if (ch->fighting) {
    if (!char_died(ch->fighting->who))
      --ch->fighting->who->num_fighting;
    DISPOSE(ch->fighting);
  }
  ch->fighting = NULL;
  /* Get rid of charged attacks hitting after the fight stops */
  ch->substate = SUB_NONE;
  ch->skillGsn = -1;
  /* Bug with mobs retaining prefocus -Karma */
  ch->focus = 0;
  ch->charge = 0;
  ch->timerDelay = 0;
  ch->timerType = 0;
  ch->timerDo_fun = NULL;
  /* end added code */
  if (ch->mount)
    ch->position = POS_MOUNTED;
  else
    ch->position = POS_STANDING;
  /* Berserk wears off after combat. -- Altrag */
  if (IS_AFFECTED(ch, AFF_BERSERK)) {
    affect_strip(ch, gsn_berserk);
    set_char_color(AT_WEAROFF, ch);
    send_to_char(skill_table[gsn_berserk]->msg_off, ch);
    send_to_char("\n\r", ch);
  }
  return;
}

/*
 * Stop fights.
 */
void stop_fighting(CHAR_DATA *ch, bool fBoth) {
  CHAR_DATA *fch;

  free_fight(ch);
  update_pos(ch);

  if (!fBoth) /* major short cut here by Thoric */
    return;

  for (fch = first_char; fch; fch = fch->next) {
    if (who_fighting(fch) == ch) {
      free_fight(fch);
      update_pos(fch);
    }
  }
  return;
}

/* Vnums for the various bodyparts */
int part_vnums[] = {
    12, /* Head */
    14, /* arms */
    15, /* legs */
    13, /* heart */
    44, /* brains */
    16, /* guts */
    45, /* hands */
    46, /* feet */
    47, /* fingers */
    48, /* ear */
    49, /* eye */
    50, /* long_tongue */
    51, /* eyestalks */
    52, /* tentacles */
    53, /* fins */
    54, /* wings */
    55, /* tail */
    56, /* scales */
    59, /* claws */
    87, /* fangs */
    58, /* horns */
    57, /* tusks */
    55, /* tailattack */
    85, /* sharpscales */
    84, /* beak */
    86, /* haunches */
    83, /* hooves */
    82, /* paws */
    81, /* forelegs */
    80, /* feathers */
    0,  /* r1 */
    0   /* r2 */
};

/* Messages for flinging off the various bodyparts */
char *part_messages[] = {
    "$n's severed head plops from its neck.",
    "$n's arm is sliced from $s dead body.",
    "$n's leg is sliced from $s dead body.",
    "$n's heart is torn from $s chest.",
    "$n's brains spill grotesquely from $s head.",
    "$n's guts spill grotesquely from $s torso.",
    "$n's hand is sliced from $s dead body.",
    "$n's foot is sliced from $s dead body.",
    "A finger is sliced from $n's dead body.",
    "$n's ear is sliced from $s dead body.",
    "$n's eye is gouged from its socket.",
    "$n's tongue is torn from $s mouth.",
    "An eyestalk is sliced from $n's dead body.",
    "A tentacle is severed from $n's dead body.",
    "A fin is sliced from $n's dead body.",
    "A wing is severed from $n's dead body.",
    "$n's tail is sliced from $s dead body.",
    "A scale falls from the body of $n.",
    "A claw is torn from $n's dead body.",
    "$n's fangs are torn from $s mouth.",
    "A horn is wrenched from the body of $n.",
    "$n's tusk is torn from $s dead body.",
    "$n's tail is sliced from $s dead body.",
    "A ridged scale falls from the body of $n.",
    "$n's beak is sliced from $s dead body.",
    "$n's haunches are sliced from $s dead body.",
    "A hoof is sliced from $n's dead body.",
    "A paw is sliced from $n's dead body.",
    "$n's foreleg is sliced from $s dead body.",
    "Some feathers fall from $n's dead body.",
    "r1 message.",
    "r2 message."};

/*
 * Improved Death_cry contributed by Diavolo.
 * Additional improvement by Thoric (and removal of turds... sheesh!)
 * Support for additional bodyparts by Fireblade
 */
void death_cry(CHAR_DATA *ch) {
  ROOM_INDEX_DATA *was_in_room;
  char *msg;
  EXIT_DATA *pexit;
  int vnum, shift, index, i;

  if (!ch) {
    bug("DEATH_CRY: null ch!", 0);
    return;
  }
  vnum = 0;
  msg = NULL;

  switch (number_range(0, 5)) {
    default:
      msg = "You hear $n's death cry.";
      break;
    case 0:
      msg =
          "$n screams furiously as $e falls to the ground in a heap!";
      break;
    case 1:
      msg = "$n hits the ground ... DEAD.";
      break;
    case 2:
      msg =
          "$n catches $s guts in $s hands as they pour through $s fatal"
          " wound!";
      break;
    case 3:
      msg = "$n splatters blood on your armor.";
      break;
    case 4:
      msg =
          "$n gasps $s last breath and blood spurts out of $s "
          "mouth and ears.";
      break;
    case 5:
      shift = number_range(0, 31);
      index = 1 << shift;

      for (i = 0; i < 32 && ch->xflags; i++) {
        if (HAS_BODYPART(ch, index)) {
          msg = part_messages[shift];
          vnum = part_vnums[shift];
          break;
        } else {
          shift = number_range(0, 31);
          index = 1 << shift;
        }
      }

      if (!msg)
        msg = "You hear $n's death cry.";
      break;
  }

  act(AT_CARNAGE, msg, ch, NULL, NULL, TO_ROOM);

  if (vnum) {
    char buf[MAX_STRING_LENGTH];
    OBJ_DATA *obj;
    char *name;

    if (!get_obj_index(vnum)) {
      bug("death_cry: invalid vnum", 0);
      return;
    }
    name = IS_NPC(ch) ? ch->short_descr : ch->name;
    obj = create_object(get_obj_index(vnum), 0);
    obj->timer = number_range(4, 7);
    if (IS_AFFECTED(ch, AFF_POISON))
      obj->value[3] = 10;

    sprintf(buf, obj->short_descr, name);
    STRFREE(obj->short_descr);
    obj->short_descr = STRALLOC(buf);

    sprintf(buf, obj->description, name);
    STRFREE(obj->description);
    obj->description = STRALLOC(buf);

    obj = obj_to_room(obj, ch->in_room);
  }
  if (number_range(1, 4) == 1) {
    if (IS_NPC(ch))
      msg = "You hear something's death cry.";
    else
      msg = "You hear someone's death cry.";

    was_in_room = ch->in_room;
    for (pexit = was_in_room->first_exit; pexit;
         pexit = pexit->next) {
      if (pexit->to_room && pexit->to_room != was_in_room) {
        ch->in_room = pexit->to_room;
        act(AT_CARNAGE, msg, ch, NULL, NULL, TO_ROOM);
      }
    }
    ch->in_room = was_in_room;
  }
  return;
}

void raw_kill(CHAR_DATA *ch, CHAR_DATA *victim) {
  AREA_DATA *karea;
  int lostZeni;

  if (!victim) {
    bug("raw_kill: null victim!", 0);
    return;
  }
  /* so we know which economy to drop zeni into -Goku 09.28.04 */
  karea = victim->in_room->area;

  stop_fighting(victim, true);

  mprog_death_trigger(ch, victim);
  if (char_died(victim))
    return;
  /* death_cry( victim ); */

  rprog_death_trigger(ch, victim);
  if (char_died(victim))
    return;

  make_corpse(victim, ch);
  if (victim->in_room->sector_type == SECT_OCEANFLOOR || victim->in_room->sector_type == SECT_UNDERWATER || victim->in_room->sector_type == SECT_WATER_SWIM || victim->in_room->sector_type == SECT_WATER_NOSWIM)
    act(AT_BLOOD, "$n's blood slowly clouds the surrounding water.",
        victim, NULL, NULL, TO_ROOM);
  else if (victim->in_room->sector_type == SECT_AIR)
    act(AT_BLOOD, "$n's blood sprays wildly through the air.",
        victim, NULL, NULL, TO_ROOM);
  else if (victim->in_room->sector_type == SECT_SPACE)
    act(AT_BLOOD, "$n's blood forms into floating spheres.", victim,
        NULL, NULL, TO_ROOM);

  if (IS_NPC(victim)) {
    victim->pIndexData->killed++;
    extract_char(victim, true, false);
    victim = NULL;
    return;
  }
  if (is_splitformed(victim)) {
    CHAR_DATA *och;
    CHAR_DATA *och_next;

    for (och = first_char; och; och = och_next) {
      och_next = och->next;

      if (!IS_NPC(och))
        continue;

      if ((xIS_SET(och->affected_by, AFF_SPLIT_FORM) || xIS_SET(och->affected_by, AFF_TRI_FORM) || xIS_SET(och->affected_by, AFF_MULTI_FORM) || xIS_SET(och->affected_by, AFF_BIOJR)) && och->master == victim) {
        extract_char(och, true, false);
      }
    }
    xREMOVE_BIT((victim)->affected_by, AFF_MULTI_FORM);
    xREMOVE_BIT((victim)->affected_by, AFF_TRI_FORM);
    xREMOVE_BIT((victim)->affected_by, AFF_SPLIT_FORM);
    xREMOVE_BIT((victim)->affected_by, AFF_BIOJR);
  }
  set_char_color(AT_DIEMSG, victim);
  if (is_android(victim) || is_superandroid(victim)) {
    do_help(victim, "_ANDROID_DIEMSG_");
  } else {
    do_help(victim, "_DIEMSG_");
  }

  if (victim->race == 6)
    evolveCheck(victim, ch, true);

  extract_char(victim, false, true);
  if (!victim) {
    bug("oops! raw_kill: extract_char destroyed pc char", 0);
    return;
  }
  while (victim->first_affect)
    affect_remove(victim, victim->first_affect);
  victim->affected_by = race_table[victim->race]->affected;
  victim->pl = 1;
  victim->position = POS_RESTING;
  victim->hit = 1;
  victim->mana = 1;
  victim->focus = 0;
  heart_calc(victim, "");
  victim->powerup = 0;
  victim->pcdata->tStr = 0;
  victim->pcdata->tSpd = 0;
  victim->pcdata->tInt = 0;
  victim->pcdata->tCon = 0;

  if (!IS_NPC(victim) && victim->race == 6) {
    victim->pcdata->absorb_sn = -1;
    victim->pcdata->absorb_learn = 0;
  }
  /* Shut down some of those naked spammer killers - Blodkai */

  /*
   * Pardon crimes...                                         -Thoric
   */
  if (xIS_SET(victim->act, PLR_KILLER)) {
    xREMOVE_BIT(victim->act, PLR_KILLER);
    send_to_char("The gods have pardoned you for your murderous acts.\n\r",
                 victim);
  }
  if (xIS_SET(victim->act, PLR_THIEF)) {
    xREMOVE_BIT(victim->act, PLR_THIEF);
    send_to_char("The gods have pardoned you for your thievery.\n\r",
                 victim);
  }
  victim->pcdata->condition[COND_FULL] = 12;
  victim->pcdata->condition[COND_THIRST] = 12;

  lostZeni = victim->gold * 0.07;
  victim->gold -= lostZeni;
  boost_economy(karea, lostZeni);
  pager_printf(victim, "&YYou have lost %d zeni!\n\r&D", lostZeni);

  if (!is_android(victim) && !is_superandroid(victim) && !xIS_SET(victim->affected_by, AFF_DEAD))
    xSET_BIT(victim->affected_by, AFF_DEAD);

  if (IS_SET(sysdata.save_flags, SV_DEATH))
    save_char_obj(victim);
  return;
}

void group_gain(CHAR_DATA *ch, CHAR_DATA *victim) {
  CHAR_DATA *gch;
  CHAR_DATA *lch;
  int xp;
  int members;

  /*
   * Monsters don't get kill xp's or alignment changes.
   * Dying of mortal wounds or poison doesn't give xp to anyone!
   */
  if (IS_NPC(ch) || victim == ch)
    return;

  members = 0;
  for (gch = ch->in_room->first_person; gch; gch = gch->next_in_room) {
    if (is_same_group(gch, ch))
      members++;
  }

  if (members == 0) {
    bug("Group_gain: members.", members);
    members = 1;
  }
  lch = ch->leader ? ch->leader : ch;

  for (gch = ch->in_room->first_person; gch; gch = gch->next_in_room) {
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;

    if (!is_same_group(gch, ch))
      continue;

    if (gch->pl / lch->pl > 10) {
      send_to_char("You are too high for this group.\n\r",
                   gch);
      continue;
    }
    if (gch->pl / lch->pl < 0.1) {
      send_to_char("You are too low for this group.\n\r",
                   gch);
      continue;
    }
    xp = (int)(xp_compute(gch, victim));
    if (!gch->fighting)
      xp /= 2;
    gch->alignment = align_compute(gch, victim);

    for (obj = ch->first_carrying; obj; obj = obj_next) {
      obj_next = obj->next_content;
      if (obj->wear_loc == WEAR_NONE)
        continue;

      if (((IS_OBJ_STAT(obj, ITEM_ANTI_EVIL) && IS_EVIL(ch)) || (IS_OBJ_STAT(obj, ITEM_ANTI_GOOD) && IS_GOOD(ch)) || (IS_OBJ_STAT(obj, ITEM_ANTI_NEUTRAL) && IS_NEUTRAL(ch))) && !IS_HC(ch)) {
        act(AT_MAGIC, "You are zapped by $p.", ch, obj,
            NULL, TO_CHAR);
        act(AT_MAGIC, "$n is zapped by $p.", ch, obj,
            NULL, TO_ROOM);

        obj_from_char(obj);
        obj_to_char(obj, ch);
        oprog_zap_trigger(ch, obj); /* mudprogs */
        if (char_died(ch))
          return;
      }
    }
  }

  return;
}

int align_compute(CHAR_DATA *gch, CHAR_DATA *victim) {
  int newalign;

  /* slowed movement in good & evil ranges by a factor of 5, h */
  /* Added divalign to keep neutral chars shifting faster -- Blodkai */
  /* This is obviously gonna take a lot more thought */

  if (!IS_NPC(victim) && !IS_NPC(gch)) {
    newalign = gch->alignment;

    if (is_kaio(gch) && newalign < 0)
      newalign = 0;
    if (is_demon(gch) && newalign > 0)
      newalign = 0;
    if (is_bio(gch) && newalign > 0)
      newalign = 0;

    return newalign;
  } else {
    newalign = gch->alignment - victim->alignment * 0.02;

    if (newalign > 1000)
      newalign = 1000;
    if (newalign < -1000)
      newalign = -1000;

    if (is_kaio(gch) && newalign < 0)
      newalign = 0;
    if (is_demon(gch) && newalign > 0)
      newalign = 0;
    if (is_bio(gch) && newalign > 0)
      newalign = 0;

    return newalign;
  }
}

/*
 * Calculate how much XP gch should gain for killing victim
 * Lots of redesigning for new exp system by Thoric
 *
 * More editing by Warren to remove levels
 */
int xp_compute(CHAR_DATA *gch, CHAR_DATA *victim) {
  int xp = 0;

  if (!IS_NPC(victim)) {
    if ((gch->pl / victim->pl) <= 5)
      xp = (victim->pl * (number_range(4, 6) * 0.01));
    else if ((gch->pl / victim->pl) <= 8)
      xp = (victim->pl * (number_range(3, 5) * 0.01));
    else if ((gch->pl / victim->pl) < 10)
      xp = (victim->pl * (number_range(2, 4) * 0.01));
    else
      xp = 0;
  }
  if (IS_NPC(victim)) {
    if ((gch->pl / victim->exp) <= 5)
      xp = (victim->exp * (number_range(4, 6) * 0.01));
    else if ((gch->pl / victim->exp) <= 8)
      xp = (victim->exp * (number_range(3, 5) * 0.01));
    else if ((gch->pl / victim->exp) < 10)
      xp = (victim->exp * (number_range(2, 4) * 0.01));
    else
      xp = 0;
  }
  return xp;
}

/*
 * Revamped by Thoric to be more realistic
 * Added code to produce different messages based on weapon type - FB
 * Added better bug message so you can track down the bad dt's -Shaddai
 */
void new_dam_message(CHAR_DATA *ch, CHAR_DATA *victim, int dam, int dt,
                     OBJ_DATA *obj) {
  char buf1[256], buf2[256], buf3[256];
  char bugbuf[MAX_STRING_LENGTH];
  const char *vs;
  const char *vp;
  const char *attack;
  char punct;
  struct skill_type *skill = NULL;
  bool gcflag = false;
  bool gvflag = false;
  int d_index, w_index;
  ROOM_INDEX_DATA *was_in_room;

  if (ch->in_room != victim->in_room) {
    was_in_room = ch->in_room;
    char_from_room(ch);
    char_to_room(ch, victim->in_room);
  } else
    was_in_room = NULL;

  /* Get the weapon index */

  if (dt > 0 && dt < top_sn) {
    w_index = 0;
  } else if (dt >= TYPE_HIT && dt <
                                   TYPE_HIT + sizeof(attack_table) / sizeof(attack_table[0])) {
    w_index = dt - TYPE_HIT;
  } else {
    sprintf(bugbuf, "Dam_message: bad dt %d from %s in %d.",
            dt, ch->name, ch->in_room->vnum);
    bug(bugbuf, 0);
    dt = TYPE_HIT;
    w_index = 0;
  }

  /* get the damage index  */
  if (dam == 0)
    d_index = 0;
  else if (dam < 2)
    d_index = 1;
  else if (dam < 3)
    d_index = 2;
  else if (dam < 4)
    d_index = 3;
  else if (dam < 5)
    d_index = 4;
  else if (dam < 6)
    d_index = 5;
  else if (dam < 7)
    d_index = 6;
  else if (dam < 8)
    d_index = 7;
  else if (dam < 10)
    d_index = 8;
  else if (dam < 12)
    d_index = 9;
  else if (dam < 14)
    d_index = 10;
  else if (dam < 18)
    d_index = 11;
  else if (dam < 24)
    d_index = 12;
  else if (dam < 30)
    d_index = 13;
  else if (dam < 40)
    d_index = 14;
  else if (dam < 50)
    d_index = 15;
  else if (dam < 60)
    d_index = 16;
  else if (dam < 70)
    d_index = 17;
  else if (dam < 80)
    d_index = 18;
  else if (dam < 90)
    d_index = 19;
  else if (dam < 100)
    d_index = 20;
  else if (dam < 300)
    d_index = 21;
  else if (dam < 1000)
    d_index = 22;
  else if (dam < 10000)
    d_index = 23;
  else if (dam < 100000)
    d_index = 24;
  else if (dam < 1000000)
    d_index = 25;
  else if (dam < 10000000)
    d_index = 26;
  else
    d_index = 27;

  /* Lookup the damage message */
  vs = s_1337_messages[d_index];
  vp = p_message_table[w_index][d_index];

  punct = (dam <= 30) ? '.' : '!';

  if (dam == 0 && (!IS_NPC(ch) && (IS_SET(ch->pcdata->flags, PCFLAG_GAG))))
    gcflag = true;

  if (dam == 0 && (!IS_NPC(victim) &&
                   (IS_SET(victim->pcdata->flags, PCFLAG_GAG))))
    gvflag = true;

  if (dt >= 0 && dt < top_sn)
    skill = skill_table[dt];

  if (victim->dodge) {
    if (IS_NPC(ch)) {
      sprintf(buf1, "&B%s dodges %s's attack.", victim->name,
              ch->short_descr);
      sprintf(buf2, "&B%s dodges your attack.", victim->name);
      sprintf(buf3, "&BYou dodge %s's attack.",
              ch->short_descr);
    } else {
      sprintf(buf1, "&B%s dodges %s's attack.", victim->name,
              ch->name);
      sprintf(buf2, "&B%s dodges your attack.", victim->name);
      sprintf(buf3, "&BYou dodge %s's attack.", ch->name);
    }
    victim->dodge = false;
  } else if (victim->block) {
    if (IS_NPC(ch)) {
      sprintf(buf1, "&B%s blocks %s's attack.", victim->name,
              ch->short_descr);
      sprintf(buf2, "&B%s blocks your attack.", victim->name);
      sprintf(buf3, "&BYou block %s's attack.",
              ch->short_descr);
    } else {
      sprintf(buf1, "&B%s blocks %s's attack.", victim->name,
              ch->name);
      sprintf(buf2, "&B%s blocks your attack.", victim->name);
      sprintf(buf3, "&BYou block %s's attack.", ch->name);
    }
    victim->block = false;
  } else if (victim->ki_dodge) {
    if (IS_NPC(ch)) {
      sprintf(buf1,
              "&B%s flickers and vanishes, dodging %s's attack with super-speed.",
              victim->name, ch->short_descr);
      sprintf(buf2,
              "&B%s flickers and vanishes, dodging your attack with super-speed.",
              victim->name);
      sprintf(buf3,
              "&BYou flicker and vanish, dodging %s's attack with super-speed.",
              ch->short_descr);
    } else {
      sprintf(buf1,
              "&B%s flickers and vanishes, dodging %s's attack with super-speed.",
              victim->name, ch->name);
      sprintf(buf2,
              "&B%s flickers and vanishes, dodging your attack with super-speed.",
              victim->name);
      sprintf(buf3,
              "&BYou flicker and vanish, dodging %s's attack with super-speed.",
              ch->name);
    }
    victim->ki_dodge = false;
  } else if (victim->ki_deflect) {
    if (IS_NPC(ch)) {
      sprintf(buf1, "&B%s deflects %s's attack.",
              victim->name, ch->short_descr);
      sprintf(buf2, "&B%s deflects your attack.",
              victim->name);
      sprintf(buf3, "&BYou deflect %s's attack.",
              ch->short_descr);
    } else {
      sprintf(buf1, "&B%s deflects %s's attack.",
              victim->name, ch->name);
      sprintf(buf2, "&B%s deflects your attack.",
              victim->name);
      sprintf(buf3, "&BYou deflect %s's attack.", ch->name);
    }
    victim->ki_deflect = false;
  } else if (victim->ki_cancel) {
    if (IS_NPC(ch)) {
      sprintf(buf1, "&B%s cancels out %s's attack.",
              victim->name, ch->short_descr);
      sprintf(buf2, "&B%s cancels out your attack.",
              victim->name);
      sprintf(buf3, "&BYou cancel out %s's attack.",
              ch->short_descr);
    } else {
      sprintf(buf1, "&B%s cancels out %s's attack.",
              victim->name, ch->name);
      sprintf(buf2, "&B%s cancels out your attack.",
              victim->name);
      sprintf(buf3, "&BYou cancel out %s's attack.",
              ch->name);
    }
    victim->ki_cancel = false;
  } else if (dt == TYPE_HIT) {
    if (dam > 0) {
      sprintf(buf1, "$n %s $N%c  [%s]", vp, punct,
              num_punct(dam));
      sprintf(buf2, "You %s $N%c  [%s]", vs, punct,
              num_punct(dam));
      sprintf(buf3, "$n %s you%c  [%s]", vp, punct,
              num_punct(dam));
    }
    if (dam <= 0) {
      switch (number_range(1, 15)) {
        default:
          sprintf(buf1,
                  "$n hits $N. $N just laughs at $s weak power.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You hit $N. $N just laughs at your weak power. [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n hits you. You laugh at $s weak power. [%s]",
                  num_punct(dam));
          break;
        case 1:
          sprintf(buf1,
                  "$n hits $N. $N just laughs at $s weak power.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You hit $N. $N just laughs at your weak power. [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n hits you. You laugh at $s weak power. [%s]",
                  num_punct(dam));
          break;
        case 2:
          sprintf(buf1, "$n %s $N%c  [%s]", vp, punct,
                  num_punct(dam));
          sprintf(buf2, "You %s $N%c  [%s]", vp, punct,
                  num_punct(dam));
          sprintf(buf3, "$n %s you%c  [%s]", vp, punct,
                  num_punct(dam));
          break;
        case 3:
          sprintf(buf1,
                  "$n's hit is absorbed by the ki energy surrounding $N.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "Your hit is absorbed by the ki energy surrounding $N.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n's hit is absorbed by the ki energy surrounding you. [%s]",
                  num_punct(dam));
          break;
        case 4:
          sprintf(buf1,
                  "$N dodges $n's hit with incredible speed.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "$N dodges your hit with incredible speed.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "You dodge $n's hit with incredible speed. [%s]",
                  num_punct(dam));
          break;
        case 5:
          sprintf(buf1,
                  "$N crosses $S arms and blocks $n's hit.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "$N crosses $S arms and blocks your hit.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "You cross your arms and block $n's hit. [%s]",
                  num_punct(dam));
          break;
        case 6:
          sprintf(buf1,
                  "$n aims too low and misses $N.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You aim too low and miss $N.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n aims too low and misses you. [%s]",
                  num_punct(dam));
          break;
        case 7:
          sprintf(buf1,
                  "$n aims too high and misses $N.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You aim too high and miss $N.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n aims too high and misses you. [%s]",
                  num_punct(dam));
          break;
        case 8:
          sprintf(buf1,
                  "$n lands $s hit but $N just grins at $m.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You land your hit but $N just grins at you.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n lands $s hit but you just grin at $m. [%s]",
                  num_punct(dam));
          break;
        case 9:
          sprintf(buf1,
                  "$n loses concentration for a moment and misses $N.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You lose concentration for a moment and miss $N.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n loses concentration for a moment and misses you. [%s]",
                  num_punct(dam));
          break;
        case 10:
          sprintf(buf1,
                  "$n overextends $mself and misses $N.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You overextend yourself and miss $N.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n overextends $mself and misses you. [%s]",
                  num_punct(dam));
          break;
        case 11:
          sprintf(buf1,
                  "$n hits $N but $E seems to shrug off the damage.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You hit $N but $E seems to shrug off the damage.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n hits you, but you soak the damage. [%s]",
                  num_punct(dam));
          break;
        case 12:
          sprintf(buf1,
                  "$N chuckles as $n goes off-balance by missing $M.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You get laughed at by $N for your off-balance miss.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "You chuckle as $n goes off-balance by missing you. [%s]",
                  num_punct(dam));
          break;
        case 13:
          sprintf(buf1,
                  "$N ducks under $n's attack.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "$N ducks under your attack.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "You duck under $n's attack. [%s]",
                  num_punct(dam));
          break;
        case 14:
          sprintf(buf1,
                  "$N jumps over $n's attack.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "$N jumps over your attack.  [%s]",
                  num_punct(dam));
          sprintf(buf3, "You jump over $n's attack. [%s]",
                  num_punct(dam));
          break;
        case 15:
          sprintf(buf1,
                  "$n trips on something and misses $N.  [%s]",
                  num_punct(dam));
          sprintf(buf2,
                  "You trip on something and miss $N.  [%s]",
                  num_punct(dam));
          sprintf(buf3,
                  "$n trips on something and misses you. [%s]",
                  num_punct(dam));
          break;
      }
    } else if (dt > TYPE_HIT && is_wielding_poisoned(ch)) {
      if (dt <
          TYPE_HIT +
              sizeof(attack_table) / sizeof(attack_table[0]))
        attack = attack_table[dt - TYPE_HIT];
      else {
        sprintf(bugbuf,
                "Dam_message: bad dt %d from %s in %d.",
                dt, ch->name, ch->in_room->vnum);
        bug(bugbuf, 0);
        dt = TYPE_HIT;
        attack = attack_table[0];
      }

      sprintf(buf1, "$n's poisoned %s %s $N%c", attack, vp,
              punct);
      sprintf(buf2, "Your poisoned %s %s $N%c", attack, vp,
              punct);
      sprintf(buf3, "$n's poisoned %s %s you%c", attack, vp,
              punct);
    } else {
      if (skill) {
        attack = skill->noun_damage;
        if (dam == 0) {
          bool found = false;

          if (skill->miss_char && skill->miss_char[0] != '\0') {
            act(AT_HIT, skill->miss_char,
                ch, NULL, victim, TO_CHAR);
            found = true;
          }
          if (skill->miss_vict && skill->miss_vict[0] != '\0') {
            act(AT_HITME, skill->miss_vict,
                ch, NULL, victim, TO_VICT);
            found = true;
          }
          if (skill->miss_room && skill->miss_room[0] != '\0') {
            if (strcmp(skill->miss_room,
                       "supress"))
              act(AT_ACTION,
                  skill->miss_room,
                  ch, NULL, victim,
                  TO_NOTVICT);
            found = true;
          }
          if (found) { /* miss message already
                        * sent */
            if (was_in_room) {
              char_from_room(ch);
              char_to_room(ch,
                           was_in_room);
            }
            return;
          }
        } else {
          if (skill->hit_char && skill->hit_char[0] != '\0')
            act(AT_HIT, skill->hit_char, ch,
                NULL, victim, TO_CHAR);
          if (skill->hit_vict && skill->hit_vict[0] != '\0')
            act(AT_HITME, skill->hit_vict,
                ch, NULL, victim, TO_VICT);
          if (skill->hit_room && skill->hit_room[0] != '\0')
            act(AT_ACTION, skill->hit_room,
                ch, NULL, victim,
                TO_NOTVICT);
        }
      } else if (dt >= TYPE_HIT && dt <
                                       TYPE_HIT +
                                           sizeof(attack_table) /
                                               sizeof(attack_table[0])) {
        if (obj)
          attack = obj->short_descr;
        else
          attack = attack_table[dt - TYPE_HIT];
      } else {
        sprintf(bugbuf,
                "Dam_message: bad dt %d from %s in %d.",
                dt, ch->name, ch->in_room->vnum);
        bug(bugbuf, 0);
        dt = TYPE_HIT;
        attack = attack_table[0];
      }

      sprintf(buf1, "$n's %s %s $N%c  [%s]", attack, vp,
              punct, num_punct(dam));
      sprintf(buf2, "Your %s %s $N%c  [%s]", attack, vp,
              punct, num_punct(dam));
      sprintf(buf3, "$n's %s %s you%c  [%s]", attack, vp,
              punct, num_punct(dam));
    }

    act(AT_ACTION, buf1, ch, NULL, victim, TO_NOTVICT);
    if (!gcflag)
      act(AT_HIT, buf2, ch, NULL, victim, TO_CHAR);
    if (!gvflag)
      act(AT_HITME, buf3, ch, NULL, victim, TO_VICT);

    if (was_in_room) {
      char_from_room(ch);
      char_to_room(ch, was_in_room);
    }
    return;
  }
}

#ifndef dam_message
void dam_message(CHAR_DATA *ch, CHAR_DATA *victim, int dam, int dt) {
  new_dam_message(ch, victim, dam, dt);
}

#endif

void do_stopspar(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;

  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You arn't fighting anyone.\n\r", ch);
    return;
  }
  if (IS_NPC(victim)) {
    send_to_char("You think you are sparing?!?.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && !xIS_SET(ch->act, PLR_SPAR) && !xIS_SET(victim->act, PLR_SPAR)) {
    send_to_char("You think you are sparing?!?.\n\r", ch);
    return;
  }
  stop_fighting(ch, true);
  act(AT_GREEN, "You decide to stop sparring with $N.", ch, NULL, victim,
      TO_CHAR);
  act(AT_GREEN, "$n decides to stop sparring with you.", ch, NULL, victim,
      TO_VICT);
  act(AT_GREEN, "$n decides to stop sparring with $N.", ch, NULL, victim,
      TO_NOTVICT);
  ch->pcdata->sparcount += 1;
  victim->pcdata->sparcount += 1;

  if (ch->pcdata->nextspartime <= 0) {
    struct tm *tms;

    tms = localtime(&current_time);
    tms->tm_mday += 1;
    ch->pcdata->nextspartime = mktime(tms);
  }
  if (victim->pcdata->nextspartime <= 0) {
    struct tm *tms;

    tms = localtime(&current_time);
    tms->tm_mday += 1;
    victim->pcdata->nextspartime = mktime(tms);
  }

  return;
}

void do_spar(CHAR_DATA *ch, char *argument) {
  char buf[MAX_STRING_LENGTH];
  char arg[MAX_INPUT_LENGTH];
  CHAR_DATA *victim;

  one_argument(argument, arg);

  if (arg[0] == '\0') {
    send_to_char("Spar whom?\n\r", ch);
    return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("They aren't here.\n\r", ch);
    return;
  }
  if (!IS_NPC(victim) && !victim->desc) {
    send_to_char("They are link dead, it wouldn't be right.", ch);
    return;
  }
  if (IS_NPC(victim)) {
    send_to_char("You can only spar other players.", ch);
    return;
  }
  if (victim == ch) {
    send_to_char("You can't spar your self.\n\r", ch);
    return;
  }
  if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_NPC(victim)) {
    if (!xIS_SET(ch->act, PLR_QUESTING) && xIS_SET(victim->act, PLR_QUESTING)) {
      send_to_char("You can't spar a player involved in a role playing event.\n\r",
                   ch);
      return;
    }
    if (xIS_SET(ch->act, PLR_QUESTING) && !xIS_SET(victim->act, PLR_QUESTING)) {
      send_to_char("You can't spar a player not involved in a role playing event.\n\r",
                   ch);
      return;
    }
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && victim->hit < 86) {
    send_to_char("They need to rest a bit before sparring some more.\n\r",
                 ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && ch->hit < 86) {
    send_to_char("You need to rest a bit before sparring some more.\n\r",
                 ch);
    return;
  }
  if (!xIS_SET(ch->act, PLR_SPAR))
    xSET_BIT(ch->act, PLR_SPAR);
  if (!xIS_SET(victim->act, PLR_SPAR))
    xSET_BIT(victim->act, PLR_SPAR);

  if (is_safe(ch, victim, true))
    return;

  if (ch->position == POS_RESTING || ch->position == POS_SLEEPING) {
    send_to_char("How do you propose to do that in your current state?\n\r",
                 ch);
    return;
  }
  if (ch->position == POS_FIGHTING || ch->position == POS_EVASIVE || ch->position == POS_DEFENSIVE || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK) {
    send_to_char("You do the best you can!\n\r", ch);
    return;
  }
  if (!xIS_SET(ch->in_room->room_flags, ROOM_ARENA)) {
    send_to_char("You must be in an arena to spar someone.\n\r",
                 ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && ch->exp <= 50) {
    send_to_char("You can not fight other players while you are in training.\n\r",
                 ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && victim->exp <= 50) {
    send_to_char("You can not fight other players while they are in training.\n\r",
                 ch);
    return;
  }
  if (who_fighting(victim) != NULL) {
    send_to_char("It would not be honorable to interfere with some one else's battle.\n\r",
                 ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && victim->hit < 2) {
    send_to_char("They are too hurt to fight anymore.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && ch->hit < 2) {
    send_to_char("You are too hurt to fight anymore.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && !IS_HC(ch) && !IS_HC(victim) && xIS_SET(ch->act, PLR_SPAR) && xIS_SET(victim->act, PLR_SPAR) && ((IS_GOOD(ch) && !IS_GOOD(victim)) || (IS_EVIL(ch) && !IS_EVIL(victim)) || (IS_NEUTRAL(ch) && !IS_NEUTRAL(victim)))) {
    send_to_char("You would not spar someone who is aligned that way.\n\r",
                 ch);
    return;
  }
  if (!victim->pcdata->HBTCPartner) {
    send_to_char("They are not accepting sparring partners at this time.\n\r",
                 ch);
    return;
  }
  if (strcasecmp(ch->name, victim->pcdata->HBTCPartner)) {
    send_to_char("They do not want to spar with you.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, 1 * PULSE_VIOLENCE);
  if (!xIS_SET(ch->act, PLR_SPAR)) {
    sprintf(buf, "Help!  I am being attacked by %s!",
            IS_NPC(ch) ? ch->short_descr : ch->name);
    if (IS_PKILL(victim)) {
      do_wartalk(victim, buf);
    } else {
      do_yell(victim, buf);
    }
  }
  check_illegal_pk(ch, victim);

  ch->spar_start = ch->exp;
  victim->spar_start = victim->exp;
  ch->delay = 0;
  check_attacker(ch, victim);
  multi_hit(ch, victim, TYPE_UNDEFINED);
  return;
}

void do_kill(CHAR_DATA *ch, char *argument) {
  char buf[MAX_STRING_LENGTH];
  char arg[MAX_INPUT_LENGTH];
  CHAR_DATA *victim;
  OBJ_DATA *o;

  argument = one_argument(argument, arg);

  if (arg[0] == '\0') {
    send_to_char("Kill whom?\n\r", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("They aren't here.\n\r", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (!IS_NPC(victim) && !victim->desc) {
    send_to_char("They are link dead, it wouldn't be right.", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (IS_NPC(victim) && is_split(victim)) {
    ch_printf(ch, "You are not allowed to kill splitforms.\n\r");
    return;
  }
  if (IS_NPC(victim) && victim->morph) {
    send_to_char("This creature appears strange to you.  Look upon it more closely before attempting to kill it.",
                 ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if ((float)victim->exp / ch->exp > 5 && !IS_NPC(victim)) {
    ch_printf(ch,
              "They are more than 5 times stronger than you.\n\r");
    return;
  }
  if (IS_NPC(victim) && is_splitformed(victim) && victim->master && victim->master == ch) {
    send_to_char("You can't kill your own Split Form.\n\r", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (IS_NPC(ch) && is_splitformed(ch) && !IS_NPC(victim)) {
    send_to_char("You can't do that.", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (!IS_NPC(ch) && IS_NPC(victim) && is_splitformed(victim)) {
    pager_printf_color(ch,
                       "Why don't you try attacking the real %s.",
                       victim->short_descr);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (IS_NPC(victim) && victim->master != NULL && IS_IMMORTAL(victim->master) && !IS_IMMORTAL(ch)) {
    send_to_char("Cheatz.\n\r", ch);
    return;
  }
  if (IS_GOOD(ch) && !IS_EVIL(victim) && !IS_NPC(victim) &&
      !IS_HC(ch) && (o = carrying_dball(victim)) == NULL) {
    if (ch->kairank >= victim->kairank && !is_kaio(victim)) {
      ch_printf(ch,
                "Its not in your nature to kill others that are not evil.\n\r");
      return;
    }
  }
  if (victim->position <= POS_STUNNED) {
    ch_printf(ch,
              "They have one foot in the grave already. That would be pointless.\n\r");
    return;
  }
  if (victim == ch) {
    send_to_char("You hit yourself.  Ouch!\n\r", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (is_safe(ch, victim, true)) {
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (!IS_IMMORTAL(ch) && !IS_NPC(ch) && !IS_NPC(victim)) {
    if (!xIS_SET(ch->act, PLR_QUESTING) && xIS_SET(victim->act, PLR_QUESTING)) {
      send_to_char("You can't attack a player involved in a role playing event.\n\r",
                   ch);
      return;
    }
    if (xIS_SET(ch->act, PLR_QUESTING) && !xIS_SET(victim->act, PLR_QUESTING)) {
      send_to_char("You can't attack a player not involved in a role playing event.\n\r",
                   ch);
      return;
    }
  }
  if (xIS_SET(ch->in_room->room_flags, ROOM_ARENA)) {
    send_to_char("You can only spar while in an arena.\n\r", ch);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (xIS_SET(ch->act, PLR_SPAR)) {
    xREMOVE_BIT(ch->act, PLR_SPAR);
  }
  if (xIS_SET(victim->act, PLR_SPAR)) {
    xREMOVE_BIT(victim->act, PLR_SPAR);
  }
  if (IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim) {
    act(AT_PLAIN, "$N is your beloved master.", ch, NULL, victim,
        TO_CHAR);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE / 2);
    return;
  }
  if (ch->position == POS_RESTING || ch->position == POS_SLEEPING) {
    send_to_char("How do you propose to do that in your current state?\n\r",
                 ch);
    return;
  }
  if (ch->position == POS_FIGHTING || ch->position == POS_EVASIVE || ch->position == POS_DEFENSIVE || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK) {
    send_to_char("You do the best you can!\n\r", ch);
    return;
  }
  if (who_fighting(victim) != NULL) {
    send_to_char("It would not be honorable to interfere with some one else's battle.\n\r",
                 ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim) && strcmp(argument, "now")) {
    send_to_char("You must type 'kill <person> now' if you want to kill a player.\n\r",
                 ch);
    return;
  }
  if (!IS_NPC(ch) && !IS_NPC(victim))
    if (!pkill_ok(ch, victim))
      return;

  if (!IS_NPC(victim)) {
    sprintf(log_buf, "PLAYER COMBAT: %s[%s] vs. %s[%s].",
            ch->name, !xIS_SET(ch->act, PLR_SPAR) ? "DEADLY" : "SPARING",
            victim->name, !xIS_SET(victim->act, PLR_SPAR) ? "DEADLY" : "SPARING");
    log_string_plus(log_buf, LOG_NORMAL, ch->level);
    WAIT_STATE(ch, 1 * PULSE_VIOLENCE);
    if (!xIS_SET(ch->act, PLR_SPAR)) {
      sprintf(buf,
              "Help!  I am being attacked by %s!",
              IS_NPC(ch) ? ch->short_descr : ch->name);
      if (IS_PKILL(victim)) {
        do_wartalk(victim, buf);
      } else {
        do_yell(victim, buf);
      }
      ch->focus = 0;
      victim->focus = 0;
    }
    check_illegal_pk(ch, victim);
  }
  if (!IS_NPC(ch) && ch->race == 6) {
    find_absorb_data(ch, victim);
    ch->pcdata->absorb_pl = 0;
  }
  if (!IS_NPC(victim) && victim->race == 6) {
    find_absorb_data(victim, ch);
    victim->pcdata->absorb_pl = 0;
  }
  /* Moved to here so that mobs no longer retain prefocus. -Karma */
  ch->focus = 0;
  victim->focus = 0;

  ch->delay = 0;
  ch->fight_start = 0;
  ch->fight_start = ch->exp;
  victim->fight_start = 0;
  victim->fight_start = victim->exp;
  check_attacker(ch, victim);
  multi_hit(ch, victim, TYPE_UNDEFINED);
  return;
}

void do_murde(CHAR_DATA *ch, char *argument) {
  send_to_char("If you want to MURDER, spell it out.\n\r", ch);
  return;
}

void do_murder(CHAR_DATA *ch, char *argument) {
  char buf[MAX_STRING_LENGTH];
  char arg[MAX_INPUT_LENGTH];
  CHAR_DATA *victim;

  one_argument(argument, arg);

  if (arg[0] == '\0') {
    send_to_char("Murder whom?\n\r", ch);
    return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("They aren't here.\n\r", ch);
    return;
  }
  if (victim == ch) {
    send_to_char("Suicide is a mortal sin.\n\r", ch);
    return;
  }
  if (!IS_NPC(victim)) {
    send_to_char("Use KILL to kill another player.\n\r", ch);
    return;
  }
  if (IS_NPC(victim)) {
    send_to_char("Use KILL to kill another player.\n\r", ch);
    return;
  }
  if (is_safe(ch, victim, true))
    return;

  if (IS_AFFECTED(ch, AFF_CHARM)) {
    if (ch->master == victim) {
      act(AT_PLAIN, "$N is your beloved master.", ch, NULL,
          victim, TO_CHAR);
      return;
    } else {
      if (ch->master)
        xSET_BIT(ch->master->act, PLR_ATTACKER);
    }
  }
  if (ch->position == POS_FIGHTING || ch->position == POS_EVASIVE || ch->position == POS_DEFENSIVE || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK) {
    send_to_char("You do the best you can!\n\r", ch);
    return;
  }
  if (!IS_NPC(victim) && xIS_SET(ch->act, PLR_NICE)) {
    send_to_char("You feel too nice to do that!\n\r", ch);
    return;
  }
  /*
    if ( !IS_NPC( victim ) && xIS_SET(victim->act, PLR_PK ) )
*/

  if (!IS_NPC(victim)) {
    sprintf(log_buf, "%s: murder %s.", ch->name, victim->name);
    log_string_plus(log_buf, LOG_NORMAL, ch->level);
  }
  WAIT_STATE(ch, 1 * PULSE_VIOLENCE);
  sprintf(buf, "Help!  I am being attacked by %s!",
          IS_NPC(ch) ? ch->short_descr : ch->name);
  if (IS_PKILL(victim))
    do_wartalk(victim, buf);
  else
    do_yell(victim, buf);
  check_illegal_pk(ch, victim);
  check_attacker(ch, victim);
  multi_hit(ch, victim, TYPE_UNDEFINED);
  return;
}

/*
 * Check to see if the player is in an "Arena".
 */
bool in_arena(CHAR_DATA *ch) {
  if (xIS_SET(ch->in_room->room_flags, ROOM_ARENA))
    return true;
  if (IS_SET(ch->in_room->area->flags, AFLAG_FREEKILL))
    return true;
  if (ch->in_room->vnum >= 29 && ch->in_room->vnum <= 43)
    return true;
  if (!str_cmp(ch->in_room->area->filename, "arena.are"))
    return true;

  return false;
}

bool check_illegal_pk(CHAR_DATA *ch, CHAR_DATA *victim) {
  char buf[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];

  if (!IS_NPC(victim) && !IS_NPC(ch)) {
    if ((!IS_SET(victim->pcdata->flags, PCFLAG_DEADLY) || !IS_SET(ch->pcdata->flags, PCFLAG_DEADLY)) && !in_arena(ch) && ch != victim) {
      if (IS_NPC(ch))
        sprintf(buf, " (%s)", ch->name);
      if (IS_NPC(victim))
        sprintf(buf2, " (%s)", victim->name);

      sprintf(log_buf,
              "&p%s on %s%s in &W***&rILLEGAL PKILL&W*** &pattempt at %d",
              (lastplayercmd),
              (IS_NPC(victim) ? victim->short_descr : victim->name), (IS_NPC(victim) ? buf2 : ""),
              victim->in_room->vnum);
      last_pkroom = victim->in_room->vnum;
      log_string(log_buf);
      to_channel(log_buf, CHANNEL_MONITOR, "Monitor",
                 LEVEL_IMMORTAL);
      return true;
    }
  }
  return false;
}

void do_flee(CHAR_DATA *ch, char *argument) {
  ROOM_INDEX_DATA *was_in;
  ROOM_INDEX_DATA *now_in;
  int attempt, los;
  sh_int door;
  EXIT_DATA *pexit;
  CHAR_DATA *wf;

  if (!who_fighting(ch)) {
    if (ch->position == POS_FIGHTING || ch->position == POS_EVASIVE || ch->position == POS_DEFENSIVE || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK) {
      if (ch->mount)
        ch->position = POS_MOUNTED;
      else
        ch->position = POS_STANDING;
    }
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (IS_AFFECTED(ch, AFF_BERSERK)) {
    send_to_char("Flee while berserking?  You aren't thinking very clearly...\n\r",
                 ch);
    return;
  }
  /* No fleeing while more aggressive than standard or hurt. - Haus */
  if (!IS_NPC(ch) && ch->position < POS_FIGHTING) {
    send_to_char("You can't flee in an aggressive stance...\n\r",
                 ch);
    return;
  }
  if (IS_NPC(ch) && ch->position <= POS_SLEEPING)
    return;
  if (!IS_NPC(ch) && !IS_NPC(who_fighting(ch)) && xIS_SET(ch->act, PLR_SPAR) && xIS_SET(who_fighting(ch)->act, PLR_SPAR)) {
    send_to_char("Use 'STOPSPAR' to stop fighting.\n\r", ch);
    return;
  }
  was_in = ch->in_room;

  /*
   * Decided to make fleeing harder to accomplish when in a pk fight.
   * -Karma
   */
  wf = who_fighting(ch);
  bool nochance = false;

  if (!IS_NPC(ch) && !IS_NPC(wf)) {
    if (number_range(1, 2) == 2)
      // 50 % chance
      nochance = true;
  } else {
    if (number_range(1, 5) < 2)
      // 80 % chance
      nochance = true;
  }

  if (!nochance) {
    for (attempt = 0; attempt < 8; attempt++) {
      door = number_door();
      if ((pexit = get_exit(was_in, door)) == NULL || !pexit->to_room || IS_SET(pexit->exit_info, EX_NOFLEE) || (IS_SET(pexit->exit_info, EX_CLOSED) && !IS_AFFECTED(ch, AFF_PASS_DOOR)) || (IS_NPC(ch) && xIS_SET(pexit->to_room->room_flags, ROOM_NO_MOB)))
        continue;
      affect_strip(ch, gsn_sneak);
      xREMOVE_BIT(ch->affected_by, AFF_SNEAK);
      if (ch->mount && ch->mount->fighting)
        stop_fighting(ch->mount, true);
      move_char(ch, pexit, 0);
      if ((now_in = ch->in_room) == was_in)
        continue;
      ch->in_room = was_in;
      act(AT_FLEE, "$n flees head over heels!", ch, NULL,
          NULL, TO_ROOM);
      ch->in_room = now_in;
      act(AT_FLEE, "$n glances around for signs of pursuit.",
          ch, NULL, NULL, TO_ROOM);
      if (!IS_NPC(ch) && is_bio(ch)) {
        /* Clear out any chance to absorb something */
        ch->pcdata->absorb_sn = 0;
        ch->pcdata->absorb_learn = 0;
      }
      if (!IS_NPC(ch)) {
        act(AT_FLEE,
            "You flee head over heels from combat!", ch,
            NULL, NULL, TO_CHAR);
        if (!IS_NPC(ch) && !IS_NPC(wf)) {
          if (ch->pcdata->clan && wf->pcdata->clan) {
            los = ch->exp * 0.005;
          }
        } else {
          los = ch->exp * 0.005;
        }
      } else
        los = ch->exp * 0.005;
      if (wf && ch->pcdata->deity) {
        int level_ratio =
            URANGE(1, wf->level / ch->level,
                   50);

        if (wf && wf->race ==
                      ch->pcdata->deity->npcrace)
          adjust_favor(ch, 1,
                       level_ratio);
        else if (wf && wf->race ==
                           ch->pcdata->deity->npcfoe)
          adjust_favor(ch, 16,
                       level_ratio);
        else
          adjust_favor(ch, 0,
                       level_ratio);
      }
    }
    stop_fighting(ch, true);
    return;
  }
  act(AT_FLEE, "You attempt to flee from combat but can't escape!", ch,
      NULL, NULL, TO_CHAR);
  return;
}

void do_sla(CHAR_DATA *ch, char *argument) {
  send_to_char("If you want to SLAY, spell it out.\n\r", ch);
  return;
}

void do_slay(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  char arg[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  one_argument(argument, arg2);
  if (arg[0] == '\0') {
    send_to_char("Syntax: [Char] [Type]\n\r", ch);
    send_to_char("Types: Skin, Slit, Immolate, Demon, Shatter, 9mm, Deheart, Pounce, Cookie, Fslay.\n\r",
                 ch);
    return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("They aren't here.\n\r", ch);
    return;
  }
  if (ch == victim) {
    send_to_char("Suicide is a mortal sin.\n\r", ch);
    return;
  }
  if (!IS_NPC(victim) && ch->level < sysdata.level_mset_player) {
    send_to_char("You can't do that to players.\n\r", ch);
    return;
  }
  if (!str_cmp(arg2, "immolate")) {
    act(AT_FIRE, "Your fireball turns $N into a blazing inferno.",
        ch, NULL, victim, TO_CHAR);
    act(AT_FIRE,
        "$n releases a searing fireball in your direction.", ch,
        NULL, victim, TO_VICT);
    act(AT_FIRE,
        "$n points at $N, who bursts into a flaming inferno.", ch,
        NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "skin")) {
    act(AT_BLOOD,
        "You rip the flesh from $N and send his soul to the fiery depths of hell.",
        ch, NULL, victim, TO_CHAR);
    act(AT_BLOOD,
        "Your flesh has been torn from your bones and your bodyless soul now watches your bones incenerate in the fires of hell.",
        ch, NULL, victim, TO_VICT);
    act(AT_BLOOD,
        "$n rips the flesh off of $N, releasing his soul into the fiery depths of hell.",
        ch, NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "9mm")) {
    act(AT_IMMORT,
        "You pull out your 9mm and bust a cap in $N's ass.", ch,
        NULL, victim, TO_CHAR);
    act(AT_IMMORT,
        "$n pulls out $s 9mm and busts a cap in your ass.", ch,
        NULL, victim, TO_VICT);
    act(AT_IMMORT,
        "$n pulls out $s 9mm and busts a cap in $N's ass.", ch,
        NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "deheart")) {
    act(AT_BLOOD,
        "You rip through $N's chest and pull out $M beating heart in your hand.",
        ch, NULL, victim, TO_CHAR);
    act(AT_BLOOD,
        "You feel a sharp pain as $n rips into your chest and pulls our your beating heart in $M hand.",
        ch, NULL, victim, TO_VICT);
    act(AT_BLOOD,
        "Specks of blood hit your face as $n rips through $N's chest pulling out $M's beating heart.",
        ch, NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "shatter")) {
    act(AT_LBLUE,
        "You freeze $N with a glance and shatter the frozen corpse into tiny shards.",
        ch, NULL, victim, TO_CHAR);
    act(AT_LBLUE,
        "$n freezes you with a glance and shatters your frozen body into tiny shards.",
        ch, NULL, victim, TO_VICT);
    act(AT_LBLUE,
        "$n freezes $N with a glance and shatters the frozen body into tiny shards.",
        ch, NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "demon")) {
    act(AT_IMMORT,
        "You gesture, and a slavering demon appears.  With a horrible grin, the",
        ch, NULL, victim, TO_CHAR);
    act(AT_IMMORT,
        "foul creature turns on $N, who screams in panic before being eaten alive.",
        ch, NULL, victim, TO_CHAR);
    act(AT_IMMORT,
        "$n gestures, and a slavering demon appears.  The foul creature turns on",
        ch, NULL, victim, TO_VICT);
    act(AT_IMMORT,
        "you with a horrible grin.   You scream in panic before being eaten alive.",
        ch, NULL, victim, TO_VICT);
    act(AT_IMMORT,
        "$n gestures, and a slavering demon appears.  With a horrible grin, the",
        ch, NULL, victim, TO_NOTVICT);
    act(AT_IMMORT,
        "foul creature turns on $N, who screams in panic before being eaten alive.",
        ch, NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "pounce")) {
    act(AT_BLOOD,
        "Leaping upon $N with bared fangs, you tear open $S throat and toss the corpse to the ground...",
        ch, NULL, victim, TO_CHAR);
    act(AT_BLOOD,
        "In a heartbeat, $n rips $s fangs through your throat!  Your blood sprays and pours to the ground as your life ends...",
        ch, NULL, victim, TO_VICT);
    act(AT_BLOOD,
        "Leaping suddenly, $n sinks $s fangs into $N's throat.  As blood sprays and gushes to the ground, $n tosses $N's dying body away.",
        ch, NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "slit")) {
    act(AT_BLOOD, "You calmly slit $N's throat.", ch, NULL, victim,
        TO_CHAR);
    act(AT_BLOOD,
        "$n reaches out with a clawed finger and calmly slits your throat.",
        ch, NULL, victim, TO_VICT);
    act(AT_BLOOD, "$n calmly slits $N's throat.", ch, NULL, victim,
        TO_NOTVICT);
  } else if (!str_cmp(arg2, "dog")) {
    act(AT_BLOOD, "You order your dogs to rip $N to shreds.", ch,
        NULL, victim, TO_CHAR);
    act(AT_BLOOD, "$n orders $s dogs to rip you apart.", ch, NULL,
        victim, TO_VICT);
    act(AT_BLOOD, "$n orders $s dogs to rip $N to shreds.", ch,
        NULL, victim, TO_NOTVICT);
  } else if (!str_cmp(arg2, "fslay")) {
    act(AT_IMMORT, "You point at $N and fall down laughing.", ch,
        NULL, victim, TO_CHAR);
    act(AT_IMMORT,
        "$n points at you and falls down laughing. How embaressing!.",
        ch, NULL, victim, TO_VICT);
    act(AT_IMMORT, "$n points at $N and falls down laughing.", ch,
        NULL, victim, TO_NOTVICT);
    return;
  } else if (!str_cmp(arg2, "cookie")) {
    act(AT_BLOOD,
        "You point a finger at $N and $e turns into a cookie!", ch,
        NULL, victim, TO_CHAR);
    act(AT_BLOOD,
        "$n points $s finger at you and you turn into a cookie!",
        ch, NULL, victim, TO_VICT);
    act(AT_BLOOD,
        "$n points $s finger at $N and $e turns into a cookie!", ch,
        NULL, victim, TO_NOTVICT);
  } else {
    act(AT_IMMORT, "You slay $N in cold blood!", ch, NULL, victim,
        TO_CHAR);
    act(AT_IMMORT, "$n slays you in cold blood!", ch, NULL, victim,
        TO_VICT);
    act(AT_IMMORT, "$n slays $N in cold blood!", ch, NULL, victim,
        TO_NOTVICT);
  }

  set_cur_char(victim);
  raw_kill(ch, victim);
  return;
}

void do_fslay(CHAR_DATA *ch, char *argument) {
  {
    CHAR_DATA *victim;
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];

    /* char buf[MAX_STRING_LENGTH]; */

    argument = one_argument(argument, arg);
    one_argument(argument, arg2);
    if (arg[0] == '\0') {
      send_to_char("Syntax: [Char] [Type]\n\r", ch);
      send_to_char("Types: Skin, Slit, Immolate, Demon, Shatter, 9mm, Deheart, Pounce, Cookie, Fslay, Lightning.\n\r",
                   ch);
      return;
    }
    if ((victim = get_char_room(ch, arg)) == NULL) {
      send_to_char("They aren't here.\n\r", ch);
      return;
    }
    if (ch == victim) {
      send_to_char("Suicide is a mortal sin.\n\r", ch);
      return;
    }
    if (!IS_NPC(victim) && get_trust(victim) >= get_trust(ch)) {
      send_to_char("You failed.\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "immolate")) {
      act(AT_FIRE,
          "Your fireball turns $N into a blazing inferno.",
          ch, NULL, victim, TO_CHAR);
      act(AT_FIRE,
          "$n releases a searing fireball in your direction.",
          ch, NULL, victim, TO_VICT);
      act(AT_FIRE,
          "$n points at $N, who bursts into a flaming inferno.",
          ch, NULL, victim, TO_NOTVICT);
    }
    if (!str_cmp(arg2, "lightning")) {
      act(AT_RED,
          "You throw a lightning bolt at $N, reducing them to cinders.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "A holy lightning bolt from the skies above reduces you to blasphemous cinders.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$N is reduced to blasphemous cinders by a lightning bolt.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "skin")) {
      act(AT_BLOOD,
          "You rip the flesh from $N and send his soul to the fiery depths of hell.",
          ch, NULL, victim, TO_CHAR);
      act(AT_BLOOD,
          "Your flesh has been torn from your bones and your bodyless soul now watches your bones incenerate in the fires of hell.",
          ch, NULL, victim, TO_VICT);
      act(AT_BLOOD,
          "$n rips the flesh off of $N, releasing his soul into the fiery depths of hell.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "9mm")) {
      act(AT_IMMORT,
          "You pull out your 9mm and bust a cap in $N's ass.",
          ch, NULL, victim, TO_CHAR);
      act(AT_IMMORT,
          "$n pulls out $s 9mm and busts a cap in your ass.",
          ch, NULL, victim, TO_VICT);
      act(AT_IMMORT,
          "$n pulls out $s 9mm and busts a cap in $N's ass.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "deheart")) {
      act(AT_BLOOD,
          "You rip through $N's chest and pull out $M beating heart in your hand.",
          ch, NULL, victim, TO_CHAR);
      act(AT_BLOOD,
          "You feel a sharp pain as $n rips into your chest and pulls our your beating heart in $M hand.",
          ch, NULL, victim, TO_VICT);
      act(AT_BLOOD,
          "Specks of blood hit your face as $n rips through $N's chest pulling out $M's beating heart.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "shatter")) {
      act(AT_LBLUE,
          "You freeze $N with a glance and shatter the frozen corpse into tiny shards.",
          ch, NULL, victim, TO_CHAR);
      act(AT_LBLUE,
          "$n freezes you with a glance and shatters your frozen body into tiny shards.",
          ch, NULL, victim, TO_VICT);
      act(AT_LBLUE,
          "$n freezes $N with a glance and shatters the frozen body into tiny shards.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "demon")) {
      act(AT_IMMORT,
          "You gesture, and a slavering demon appears.  With a horrible grin, the",
          ch, NULL, victim, TO_CHAR);
      act(AT_IMMORT,
          "foul creature turns on $N, who screams in panic before being eaten alive.",
          ch, NULL, victim, TO_CHAR);
      act(AT_IMMORT,
          "$n gestures, and a slavering demon appears.  The foul creature turns on",
          ch, NULL, victim, TO_VICT);
      act(AT_IMMORT,
          "you with a horrible grin.   You scream in panic before being eaten alive.",
          ch, NULL, victim, TO_VICT);
      act(AT_IMMORT,
          "$n gestures, and a slavering demon appears.  With a horrible grin, the",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_IMMORT,
          "foul creature turns on $N, who screams in panic before being eaten alive.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "pounce")) {
      act(AT_BLOOD,
          "Leaping upon $N with bared fangs, you tear open $S throat and toss the corpse to the ground...",
          ch, NULL, victim, TO_CHAR);
      act(AT_BLOOD,
          "In a heartbeat, $n rips $s fangs through your throat!  Your blood sprays and pours to the ground as your life ends...",
          ch, NULL, victim, TO_VICT);
      act(AT_BLOOD,
          "Leaping suddenly, $n sinks $s fangs into $N's throat.  As blood sprays and gushes to the ground, $n tosses $N's dying body away.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "slit")) {
      act(AT_BLOOD, "You calmly slit $N's throat.", ch, NULL,
          victim, TO_CHAR);
      act(AT_BLOOD,
          "$n reaches out with a clawed finger and calmly slits your throat.",
          ch, NULL, victim, TO_VICT);
      act(AT_BLOOD, "$n calmly slits $N's throat.", ch, NULL,
          victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "dog")) {
      act(AT_BLOOD,
          "You order your dogs to rip $N to shreds.", ch,
          NULL, victim, TO_CHAR);
      act(AT_BLOOD, "$n orders $s dogs to rip you apart.", ch,
          NULL, victim, TO_VICT);
      act(AT_BLOOD, "$n orders $s dogs to rip $N to shreds.",
          ch, NULL, victim, TO_NOTVICT);
    } else if (!str_cmp(arg2, "fslay")) {
      act(AT_IMMORT,
          "You point at $N and fall down laughing.", ch, NULL,
          victim, TO_CHAR);
      act(AT_IMMORT,
          "$n points at you and falls down laughing. How embaressing!.",
          ch, NULL, victim, TO_VICT);
      act(AT_IMMORT,
          "$n points at $N and falls down laughing.", ch,
          NULL, victim, TO_NOTVICT);
      return;
    } else if (!str_cmp(arg2, "cookie")) {
      act(AT_BLOOD,
          "You point a finger at $N and $e turns into a cookie!",
          ch, NULL, victim, TO_CHAR);
      act(AT_BLOOD,
          "$n points $s finger at you and you turn into a cookie!",
          ch, NULL, victim, TO_VICT);
      act(AT_BLOOD,
          "$n points $s finger at $N and $e turns into a cookie!",
          ch, NULL, victim, TO_NOTVICT);
    } else {
      act(AT_IMMORT, "You slay $N in cold blood!", ch, NULL,
          victim, TO_CHAR);
      act(AT_IMMORT, "$n slays you in cold blood!", ch, NULL,
          victim, TO_VICT);
      act(AT_IMMORT, "$n slays $N in cold blood!", ch, NULL,
          victim, TO_NOTVICT);
    }

    return;
  }
}
