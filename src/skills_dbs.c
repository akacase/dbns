#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "mud.h"

extern bool removeGenieTrans args((CHAR_DATA * ch));

int summon_state;
AREA_DATA *summon_area;
ROOM_INDEX_DATA *summon_room;

/* this is for spirit bomb, if we ever need it some where else
   it should probably be moved to new_fun.c  -Goku */
int get_npc_rank(CHAR_DATA *ch) {
  if (!IS_NPC(ch))
    return get_true_rank(ch);

  if (ch->exp < 250)
    return 1;
  else if (ch->exp < 100000)
    return 2;
  else if (ch->exp < 1000000)
    return 3;
  else if (ch->exp < 10000000)
    return 4;
  else if (ch->exp < 100000000)
    return 5;
  else if (ch->exp < 1000000000)
    return 6;
  else if (ch->exp < 10000000000ULL)
    return 7;
  else if (ch->exp < 50000000000ULL)
    return 8;
  else if (ch->exp < 100000000000ULL)
    return 9;
  else if (ch->exp < 300000000000ULL)
    return 10;
  else if (ch->exp < 600000000000ULL)
    return 11;
  else if (ch->exp < 1000000000000ULL)
    return 12;
  else if (ch->exp >= 1000000000000ULL)
    return 13;

  return 0;
}

/* For puting people back together */
void reuniteSplitForms(CHAR_DATA *ch) {
  int count = 0;
  CHAR_DATA *och;
  CHAR_DATA *och_next;
  CHAR_DATA *purgeQue[5];

  for (och = first_char; och; och = och_next) {
    och_next = och->next;

    if (!IS_NPC(och))
      continue;

    if (xIS_SET(och->affected_by, AFF_SPLIT_FORM) && och->master == ch) {
      if (och->in_room != ch->in_room) {
        send_to_char("You must be in the same room as your double.",
                     ch);
        return;
      }
      extract_char(och, true, false);
      xREMOVE_BIT((ch)->affected_by, AFF_SPLIT_FORM);
      act(AT_SKILL,
          "You and your double merge back into one being.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_SKILL,
          "$n and $s double merge back into one being.", ch,
          NULL, NULL, TO_NOTVICT);
      return;
    }
    if (xIS_SET(och->affected_by, AFF_BIOJR) && och->master == ch) {
      act(AT_BLUE, "$n explodes in a cloud of blue smoke.",
          och, NULL, NULL, TO_NOTVICT);
      extract_char(och, true, false);
    }
    if ((xIS_SET(och->affected_by, AFF_TRI_FORM) || xIS_SET(och->affected_by, AFF_MULTI_FORM)) && och->master == ch) {
      purgeQue[count] = och;
      count++;
    }
  }

  if (xIS_SET((ch)->affected_by, AFF_BIOJR)) {
    xREMOVE_BIT((ch)->affected_by, AFF_BIOJR);
    return;
  }
  if ((xIS_SET(ch->affected_by, AFF_TRI_FORM) && count == 2)) {
    if (purgeQue[0]->in_room != ch->in_room || purgeQue[1]->in_room != ch->in_room) {
      send_to_char("You must be in the same room as all your doubles.",
                   ch);
      return;
    } else {
      extract_char(purgeQue[0], true, false);
      extract_char(purgeQue[1], true, false);
      xREMOVE_BIT((ch)->affected_by, AFF_TRI_FORM);

      if (is_demon(ch)) {
        act(AT_DGREY,
            "You banish your minions back to the Shadow Earth realm.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_DGREY,
            "$n banishes $s minions back to the Shadow Earth realm.",
            ch, NULL, NULL, TO_NOTVICT);
      } else {
        act(AT_YELLOW,
            "You and your doubles merge back into one being.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_YELLOW,
            "$n and $s doubles merge back into one being.",
            ch, NULL, NULL, TO_NOTVICT);
      }
      return;
    }
  }
  if (xIS_SET(ch->affected_by, AFF_MULTI_FORM) && count == 3) {
    if (purgeQue[0]->in_room != ch->in_room || purgeQue[1]->in_room != ch->in_room || purgeQue[2]->in_room != ch->in_room) {
      send_to_char("You must be in the same room as all your doubles.",
                   ch);
      return;
    } else {
      extract_char(purgeQue[0], true, false);
      extract_char(purgeQue[1], true, false);
      extract_char(purgeQue[2], true, false);
      xREMOVE_BIT((ch)->affected_by, AFF_MULTI_FORM);
      act(AT_WHITE,
          "You and your doubles merge back into one being.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE,
          "$n and $s doubles merge back into one being.", ch,
          NULL, NULL, TO_NOTVICT);
      return;
    }
  }
  send_to_char("Something wierd happened?", ch);
  /*
   * bug("reuniteSplitForms: couldn't clean splits normaly, purging
   * all of %s's forms", ch->name);
   */

  for (och = first_char; och; och = och_next) {
    och_next = och->next;

    if (!IS_NPC(och))
      continue;

    if ((xIS_SET(och->affected_by, AFF_SPLIT_FORM) || xIS_SET(och->affected_by, AFF_TRI_FORM) || xIS_SET(och->affected_by, AFF_MULTI_FORM) || xIS_SET(och->affected_by, AFF_BIOJR)) && och->master == ch) {
      extract_char(och, true, false);
    }
  }
  xREMOVE_BIT((ch)->affected_by, AFF_BIOJR);
  xREMOVE_BIT((ch)->affected_by, AFF_MULTI_FORM);
  xREMOVE_BIT((ch)->affected_by, AFF_TRI_FORM);
  xREMOVE_BIT((ch)->affected_by, AFF_SPLIT_FORM);

  return;
}

/* Copy player stats to a mob for split, tri, and multi form */
void statsToMob(CHAR_DATA *ch, CHAR_DATA *victim, sh_int form_gsn,
                bool botched, sh_int amount, sh_int num) {
  char buf[MAX_STRING_LENGTH];
  float botchMod = 0;

  if (is_demon(ch)) { /* For the minions command */
    sprintf(buf, "minion%s%d", ch->name, num);
    STRFREE(victim->name);
    victim->name = STRALLOC(buf);
    STRFREE(victim->short_descr);
    victim->short_descr = STRALLOC("Demon Minion");
  } else if (form_gsn == gsn_clone) { /* For the bio clone skill */
    sprintf(buf, "clone%s%d", ch->name, num);
    STRFREE(victim->name);
    victim->name = STRALLOC(buf);
    STRFREE(victim->short_descr);
    sprintf(buf, "%s Jr", ch->name);
    victim->short_descr = STRALLOC(buf);
  } else { /* Split, Tri, and Multi form */

    sprintf(buf, "split%s%d", ch->name, num);
    STRFREE(victim->name);
    victim->name = STRALLOC(buf);
    STRFREE(victim->short_descr);
    victim->short_descr = STRALLOC(ch->name);
  }

  if (xIS_SET(ch->affected_by, AFF_SNAMEK))
    xSET_BIT(victim->affected_by, AFF_SNAMEK);
  if (xIS_SET(ch->affected_by, AFF_HYPER))
    xSET_BIT(victim->affected_by, AFF_HYPER);
  if (xIS_SET(ch->affected_by, AFF_GROWTH))
    xSET_BIT(victim->affected_by, AFF_GROWTH);
  if (xIS_SET(ch->affected_by, AFF_GIANT))
    xSET_BIT(victim->affected_by, AFF_GIANT);
  if (xIS_SET(ch->affected_by, AFF_KAIOKEN))
    xSET_BIT(victim->affected_by, AFF_KAIOKEN);
  if (xIS_SET(ch->affected_by, AFF_SSJ))
    xSET_BIT(victim->affected_by, AFF_SSJ);
  if (xIS_SET(ch->affected_by, AFF_SSJ2))
    xSET_BIT(victim->affected_by, AFF_SSJ2);
  if (xIS_SET(ch->affected_by, AFF_SSJ3))
    xSET_BIT(victim->affected_by, AFF_SSJ3);
  if (xIS_SET(ch->affected_by, AFF_SSJ4))
    xSET_BIT(victim->affected_by, AFF_SSJ4);
  if (xIS_SET(ch->affected_by, AFF_SGOD))
    xSET_BIT(victim->affected_by, AFF_SGOD);
  if (xIS_SET(ch->affected_by, AFF_DEAD))
    xSET_BIT(victim->affected_by, AFF_DEAD);
  if (xIS_SET(ch->affected_by, AFF_USSJ))
    xSET_BIT(victim->affected_by, AFF_USSJ);
  if (xIS_SET(ch->affected_by, AFF_USSJ2))
    xSET_BIT(victim->affected_by, AFF_USSJ2);
  if (xIS_SET(ch->affected_by, AFF_FLYING))
    xSET_BIT(victim->affected_by, AFF_FLYING);
  if (xIS_SET(ch->affected_by, AFF_SEMIPERFECT))
    xSET_BIT(victim->affected_by, AFF_SEMIPERFECT);
  if (xIS_SET(ch->affected_by, AFF_PERFECT))
    xSET_BIT(victim->affected_by, AFF_PERFECT);
  if (xIS_SET(ch->affected_by, AFF_ULTRAPERFECT))
    xSET_BIT(victim->affected_by, AFF_ULTRAPERFECT);
  if (xIS_SET(ch->affected_by, AFF_EXTREME))
    xSET_BIT(victim->affected_by, AFF_EXTREME);
  if (xIS_SET(ch->affected_by, AFF_MYSTIC))
    xSET_BIT(victim->affected_by, AFF_MYSTIC);
  if (xIS_SET(ch->affected_by, AFF_EVILBOOST))
    xSET_BIT(victim->affected_by, AFF_EVILBOOST);
  if (xIS_SET(ch->affected_by, AFF_EVILSURGE))
    xSET_BIT(victim->affected_by, AFF_EVILSURGE);
  if (xIS_SET(ch->affected_by, AFF_EVILOVERLOAD))
    xSET_BIT(victim->affected_by, AFF_EVILOVERLOAD);

  if (!botched) {
    sprintf(buf, "%s is standing here.\n\r", victim->short_descr);
    STRFREE(victim->long_descr);
    victim->long_descr = STRALLOC(buf);

    victim->perm_str = get_curr_str(ch);
    victim->perm_dex = get_curr_dex(ch);
    victim->perm_int = get_curr_int(ch);
    victim->perm_con = get_curr_con(ch);
    victim->perm_lck = get_curr_lck(ch);
    victim->sex = ch->sex;
    /* victim->exp = ch->pl; */
    victim->alignment = ch->alignment;

    if (form_gsn == gsn_split_form) {
      victim->exp = (ch->exp * 0.75);
      victim->pl = (ch->pl * 0.75);
      victim->hit = 20;
      victim->max_hit = 20;
      victim->mana = ch->mana / 2;
      victim->max_mana = ch->max_mana / 2;
    } else if (form_gsn == gsn_tri_form) {
      victim->exp = (ch->exp / 2);
      victim->pl = (ch->pl / 2);
      victim->hit = 20;
      victim->max_hit = 20;
      victim->mana = ch->mana / 2;
      victim->max_mana = ch->max_mana / 2;
    } else if (form_gsn == gsn_multi_form) {
      victim->exp = (ch->exp / 4);
      victim->pl = (ch->pl / 4);
      victim->hit = 20;
      victim->max_hit = 20;
      victim->mana = ch->mana / 2;
      victim->max_mana = ch->max_mana / 2;
    } else if (form_gsn == gsn_clone) {
      if (amount < 5)
        amount = 5;
      if (amount > 10)
        amount = 10;
      victim->exp = (ch->exp / amount);
      victim->pl = (ch->pl / amount);
      victim->hit = 20;
      victim->max_hit = 20;
      victim->mana = ch->mana / 2;
      victim->max_mana = ch->max_mana / 2;
    }
  } else {
    sprintf(buf,
            "%sSomething doesn't look right with this creature.\n\r",
            victim->description);
    STRFREE(victim->description);
    victim->description = STRALLOC(buf);
    sprintf(buf, "%s is standing here.\n\r", victim->short_descr);
    STRFREE(victim->long_descr);
    victim->long_descr = STRALLOC(buf);

    botchMod =
        (float)number_range(ch->pcdata->learned[form_gsn] / 2,
                            ch->pcdata->learned[form_gsn]) /
        100;

    victim->perm_str = UMAX(1, botchMod * get_curr_str(ch));
    victim->perm_dex = UMAX(1, botchMod * get_curr_dex(ch));
    victim->perm_int = UMAX(1, botchMod * get_curr_int(ch));
    victim->perm_con = UMAX(1, botchMod * get_curr_con(ch));
    victim->perm_lck = UMAX(1, botchMod * get_curr_lck(ch));
    victim->sex = ch->sex;
    /* victim->exp = UMAX(1, botchMod * ch->pl); */
    victim->alignment = ch->alignment;

    if (form_gsn == gsn_split_form) {
      victim->exp = UMAX(1, botchMod * (ch->exp * 0.75));
      victim->pl = UMAX(1, botchMod * (ch->pl * 0.75));
      victim->hit = URANGE(1, botchMod * 20, 20);
      victim->max_hit = URANGE(1, botchMod * 20, 20);
      victim->mana = UMAX(1, botchMod * (ch->mana / 2));
      victim->max_mana =
          UMAX(1, botchMod * (ch->max_mana / 2));
    } else if (form_gsn == gsn_tri_form) {
      victim->exp = UMAX(1, botchMod * (ch->exp / 2));
      victim->pl = UMAX(1, botchMod * (ch->pl / 2));
      victim->hit = URANGE(1, botchMod * 20, 20);
      victim->max_hit = URANGE(1, botchMod * 20, 20);
      victim->mana = UMAX(1, botchMod * (ch->mana / 2));
      victim->max_mana =
          UMAX(1, botchMod * (ch->max_mana / 2));
    } else if (form_gsn == gsn_multi_form) {
      victim->exp = UMAX(1, botchMod * (ch->exp / 4));
      victim->pl = UMAX(1, botchMod * (ch->pl / 4));
      victim->hit = URANGE(1, botchMod * 20, 20);
      victim->max_hit = URANGE(1, botchMod * 20, 20);
      victim->mana = UMAX(1, botchMod * (ch->mana / 2));
      victim->max_mana =
          UMAX(1, botchMod * (ch->max_mana / 2));
    } else if (form_gsn == gsn_clone) {
      victim->exp = UMAX(1, botchMod * (ch->exp / amount));
      victim->pl = UMAX(1, botchMod * (ch->pl / amount));
      victim->hit = URANGE(1, botchMod * 20, 20);
      victim->max_hit = URANGE(1, botchMod * 20, 20);
      victim->mana = UMAX(1, botchMod * (ch->mana / 2));
      victim->max_mana =
          UMAX(1, botchMod * (ch->max_mana / 2));
    }
  }

  return;
}

/* Remove the stats placed on players from racial transformations */
void transStatRemove(CHAR_DATA *ch) {
  AFFECT_DATA *tAff;
  AFFECT_DATA *tAffNext;

  if (!ch->first_affect)
    return;

  for (tAff = ch->first_affect; tAff; tAff = tAffNext) {
    tAffNext = tAff->next;
    if (tAff->affLocator == LOC_TRANS_STAT_APPLY)
      affect_remove(ch, tAff);
  }

  return;
}

/* Apply stats to characters who use racial transformations */
void transStatApply(CHAR_DATA *ch, int strMod, int spdMod, int intMod,
                    int conMod) {
  AFFECT_DATA affStr;
  AFFECT_DATA affSpd;
  AFFECT_DATA affInt;
  AFFECT_DATA affCon;

  transStatRemove(ch);

  if (strMod) {
    affStr.type = -1;
    affStr.duration = -1;
    affStr.location = APPLY_STR;
    affStr.modifier = strMod;
    xCLEAR_BITS(affStr.bitvector);
    affStr.affLocator = LOC_TRANS_STAT_APPLY;
    affect_to_char(ch, &affStr);
  }
  if (spdMod) {
    affSpd.type = -1;
    affSpd.duration = -1;
    affSpd.location = APPLY_DEX;
    affSpd.modifier = spdMod;
    xCLEAR_BITS(affSpd.bitvector);
    affSpd.affLocator = LOC_TRANS_STAT_APPLY;
    affect_to_char(ch, &affSpd);
  }
  if (intMod) {
    affInt.type = -1;
    affInt.duration = -1;
    affInt.location = APPLY_INT;
    affInt.modifier = intMod;
    xCLEAR_BITS(affInt.bitvector);
    affInt.affLocator = LOC_TRANS_STAT_APPLY;
    affect_to_char(ch, &affInt);
  }
  if (conMod) {
    affCon.type = -1;
    affCon.duration = -1;
    affCon.location = APPLY_CON;
    affCon.modifier = conMod;
    xCLEAR_BITS(affCon.bitvector);
    affCon.affLocator = LOC_TRANS_STAT_APPLY;
    affect_to_char(ch, &affCon);
  }
  return;
}

void rage(CHAR_DATA *ch, CHAR_DATA *victim) {
  return;
  if (!ch->desc)
    return;

  if (!victim)
    return;
  if (IS_NPC(ch))
    return;
  if (!ch->in_room)
    return;
  if (!victim->in_room)
    return;

  if (ch->skillssj1 >= 1)
    return;

  if (ch->delay <= 0) {
    if (!is_saiyan(ch) && !is_hb(ch))
      return;

    if (ch->rage < 500)
      return;

    int saiyanTotal = 0;
    saiyanTotal = ((ch->perm_str * 2) + (ch->perm_dex * 2) + (ch->perm_int) + (ch->perm_con * 2));

    if (saiyanTotal < 4000)
      return;

    if (ch->skillssj1 >= 1)
      return;

    switch (number_range(1, 5)) {
      default:
      case 1:
      case 2:
      case 3:
      case 4:
        break;
      case 5:
        if (number_percent() > ((ch->rage - 400) / 50)) {
          switch (number_range(1, 10)) {
            default:
              break;
            case 1:
              act(AT_BLUE,
                  "Your hair flashes blonde, but quickly returns to normal.",
                  ch, NULL, victim, TO_CHAR);
              act(AT_BLUE,
                  "$n's hair flashes blonde, but quickly returns to normal.",
                  ch, NULL, victim, TO_VICT);
              act(AT_BLUE,
                  "$n's hair flashes blonde, but quickly returns to normal.",
                  ch, NULL, victim, TO_NOTVICT);
              break;
            case 2:
              act(AT_BLUE,
                  "You scream out, in pure rage, your hatred for $N.",
                  ch, NULL, victim, TO_CHAR);
              act(AT_BLUE,
                  "$n screams out, in pure rage, $s hatred for you.",
                  ch, NULL, victim, TO_VICT);
              act(AT_BLUE,
                  "$n screams out, in pure rage, $s hatred for $N.",
                  ch, NULL, victim, TO_NOTVICT);
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
            case 6:
              break;
            case 7:
              break;
            case 8:
              break;
            case 9:
              break;
            case 10:
              break;
          }
          return;
        }
        ch->delay_vict = victim;
        ch->delay = 6;
        break;
    }
  }
  switch (ch->delay) {
    case 6:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "You scream out, in pure rage, your hatred for $N.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for you.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for $N.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 5:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your hair flashes blonde, but quickly returns to normal.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair flashes blonde, but quickly returns to normal.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's hair flashes blonde, but quickly returns to normal.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 4:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "Your hair flashes blonde, but quickly returns to normal.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n's hair flashes blonde, but quickly returns to normal.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n's hair flashes blonde, but quickly returns to normal.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 3:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your hair flashes blonde, but slowly fades back to normal.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair flashes blonde, but slowly fades back to normal.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's hair flashes blonde, but slowly fades back to normal.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 2:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "Your voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n's voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n's voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_NOTVICT);
      do_yell(ch, "rrrrrRRRRRRRRAAAAAAAAAAAAHHHHHHHHHH!!!");
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 1:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your hair flashes blonde and your eyes turn blue as a fiery aura erupts around you.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "You look up at $N, with rage filled eyes, ready to kill...",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair flashes blonde and $s eyes turn blue as a fiery aura erupts around $m.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n looks up at you, with rage filled eyes, ready to kill...",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's hair flashes blonde and $s eyes turn blue as a fiery aura erupts around $m.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "$n looks up at $N, with rage filled eyes, ready to kill...",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);

      ch->skillssj1 = 1;
      if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
        xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
      if (xIS_SET((ch)->affected_by, AFF_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_OOZARU);
      xSET_BIT((ch)->affected_by, AFF_SSJ);
      ch->pl = ch->exp * 50;
      transStatApply(ch, 10, 5, 3, 5);
      ch->rage = 0;
      ch->delay = 0;
      add_timer(ch, TIMER_ASUPRESSED, 6, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 6, NULL, 1);
      break;
  }
  return;
}

void rage2(CHAR_DATA *ch, CHAR_DATA *victim) {
  return;
  if (!ch->desc)
    return;
  if (!victim)
    return;
  if (IS_NPC(ch))
    return;
  if (!ch->in_room)
    return;
  if (!victim->in_room)
    return;

  if ((ch->skillssj2 >= 1) && (ch->skillssj1 = 0))
    return;

  if (!xIS_SET(ch->affected_by, AFF_SSJ))
    return;

  if (ch->delay <= 0) {
    if (!is_saiyan(ch) && !is_hb(ch))
      return;

    if (ch->rage < 800)
      return;

    if (ch->train < 2610000)
      return;

    if (ch->skillssj2 >= 1)
      return;

    switch (number_range(1, 5)) {
      default:
      case 1:
      case 2:
      case 3:
      case 4:
        break;
      case 5:
        if (number_percent() > ((ch->rage - 800) / 50)) {
          switch (number_range(1, 10)) {
            default:
              break;
            case 1:
              act(AT_BLUE,
                  "Electricity begins arcing around your body, but it quickly disappears.",
                  ch, NULL, victim, TO_CHAR);
              act(AT_BLUE,
                  "Electricity begins arcing around $n's body, but it quickly disappears.",
                  ch, NULL, victim, TO_VICT);
              act(AT_BLUE,
                  "Electricity begins arcing around $n's body, but it quickly disappears.",
                  ch, NULL, victim, TO_NOTVICT);
              break;
            case 2:
              act(AT_BLUE,
                  "You scream out, in pure rage, your hatred for $N.",
                  ch, NULL, victim, TO_CHAR);
              act(AT_BLUE,
                  "$n screams out, in pure rage, $s hatred for you.",
                  ch, NULL, victim, TO_VICT);
              act(AT_BLUE,
                  "$n screams out, in pure rage, $s hatred for $N.",
                  ch, NULL, victim, TO_NOTVICT);
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
            case 6:
              break;
            case 7:
              break;
            case 8:
              break;
            case 9:
              break;
            case 10:
              break;
          }
          return;
        }
        ch->delay_vict = victim;
        ch->delay = 6;
        break;
    }
  }
  switch (ch->delay) {
    case 6:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "You scream out, in pure rage, your hatred for $N.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for you.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for $N.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 5:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "Electricity begins arcing around your body.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "Electricity begins arcing around $n's body.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "Electricity begins arcing around $n's body.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 4:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "Electricity violently arcs around your body.", ch,
          NULL, victim, TO_CHAR);
      act(AT_RED, "Electricity violently arcs around $n's body.", ch,
          NULL, victim, TO_VICT);
      act(AT_RED, "Electricity violently arcs around $n's body.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 3:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your hair flies back and begins to stand straight up.", ch,
          NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair flies back and begins to stand straight up.", ch,
          NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's hair flies back and begins to stand straight up.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 2:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "Your voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n's voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n's voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_NOTVICT);
      do_yell(ch, "rrrrrRRRRRRRRAAAAAAAAAAAAHHHHHHHHHH!!!");
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 1:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your hair flies back all the way and electricity arcs all around you as your aura flares brightly.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair flies back all the way and electricity arcs all around $m as $s aura flares brightly.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's hair flies back all the way and electricity arcs all around $m as $s aura flares brightly",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);

      ch->skillssj2 = 1;
      if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
        xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
      if (xIS_SET((ch)->affected_by, AFF_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_OOZARU);
      if (!xIS_SET(ch->affected_by, AFF_SSJ))
        xSET_BIT((ch)->affected_by, AFF_SSJ);
      xSET_BIT((ch)->affected_by, AFF_SSJ2);
      ch->pl = ch->exp * 225;
      transStatApply(ch, 16, 8, 4, 8);
      ch->delay = 0;
      ch->rage = 0;
      add_timer(ch, TIMER_ASUPRESSED, 6, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 6, NULL, 1);
      break;
  }
  return;
}

void rage3(CHAR_DATA *ch, CHAR_DATA *victim) {
  return;
  if (!ch->desc)
    return;
  if (!victim)
    return;
  if (IS_NPC(ch))
    return;
  if (!ch->in_room)
    return;
  if (!victim->in_room)
    return;

  if ((ch->skillssj3 >= 1) && (ch->skillssj1 = 0) && (ch->skillssj2 = 0))
    return;

  if (!xIS_SET(ch->affected_by, AFF_SSJ) && !xIS_SET(ch->affected_by, AFF_SSJ2))
    return;

  if (ch->delay <= 0) {
    if (!is_saiyan(ch) && !is_hb(ch))
      return;

    if (ch->rage < 1000)
      return;

    if (ch->train < 3510000)
      return;

    if (ch->skillssj3 >= 1)
      return;

    switch (number_range(1, 5)) {
      default:
      case 1:
      case 2:
      case 3:
      case 4:
        break;
      case 5:
        if (number_percent() > ((ch->rage - 1000) / 50)) {
          return;
        }
        ch->delay_vict = victim;
        ch->delay = 6;
        break;
    }
  }
  switch (ch->delay) {
    case 6:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "You scream out, in pure rage, your hatred for $N.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for you.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for $N.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 5:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "Electricity begins arcing around your body.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "Electricity begins arcing around $n's body.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "Electricity begins arcing around $n's body.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 4:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "Electricity violently arcs around your body.", ch,
          NULL, victim, TO_CHAR);
      act(AT_RED, "Electricity violently arcs around $n's body.", ch,
          NULL, victim, TO_VICT);
      act(AT_RED, "Electricity violently arcs around $n's body.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 3:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your hair begins to flow all the way down your back.", ch,
          NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair begins to flow all the way down $s back.", ch,
          NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's hair begins to flow all the way down $s back.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 2:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "Your voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n's voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n's voice bellows out through the area, with a deep angry howl.",
          ch, NULL, victim, TO_NOTVICT);
      do_yell(ch, "rrrrrRRRRRRRRAAAAAAAAAAAAHHHHHHHHHH!!!");
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 1:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW,
          "Your eyebrows disappear and your aura flares even brighter.",
          ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW,
          "$n's eyebrows disappear and $s aura flares even brighter.",
          ch, NULL, victim, TO_VICT);
      act(AT_YELLOW,
          "$n's eyebrows disappear and $s aura flares even brighter.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);

      ch->skillssj3 = 1;
      if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
        xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
      if (xIS_SET((ch)->affected_by, AFF_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_OOZARU);
      if (!xIS_SET(ch->affected_by, AFF_SSJ))
        xSET_BIT((ch)->affected_by, AFF_SSJ);
      if (!xIS_SET(ch->affected_by, AFF_SSJ2))
        xSET_BIT((ch)->affected_by, AFF_SSJ2);
      xSET_BIT((ch)->affected_by, AFF_SSJ3);
      ch->pl = ch->exp * 325;
      transStatApply(ch, 24, 12, 6, 12);
      ch->delay = 0;
      ch->rage = 0;
      add_timer(ch, TIMER_ASUPRESSED, 6, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 6, NULL, 1);
      break;
  }
  return;
}

void rage4(CHAR_DATA *ch, CHAR_DATA *victim) {
  return;
  if (!ch->desc)
    return;
  if (!victim)
    return;
  if (IS_NPC(ch))
    return;
  if (!ch->in_room)
    return;
  if (!victim->in_room)
    return;

  if ((ch->skillssgod >= 1) && (ch->skillssj1 = 0) && (ch->skillssj2 = 0) && (ch->skillssj3 = 0))
    return;

  if (ch->delay <= 0) {
    if (!is_saiyan(ch) && !is_hb(ch))
      return;

    if (ch->rage < 1500)
      return;

    if (ch->train < 4680000)
      return;

    if (ch->skillssgod >= 1)
      return;

    switch (number_range(1, 5)) {
      default:
      case 1:
      case 2:
      case 3:
      case 4:
        break;
      case 5:
        if (number_percent() > ((ch->rage - 1500) / 50)) {
          return;
        }
        ch->delay_vict = victim;
        ch->delay = 6;
        break;
    }
  }
  switch (ch->delay) {
    case 6:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "You scream out, in pure rage, your hatred for $N.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for you.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for $N.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 5:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "Electricity violently arcs around your body.", ch,
          NULL, victim, TO_CHAR);
      act(AT_RED, "Electricity violently arcs around $n's body.", ch,
          NULL, victim, TO_VICT);
      act(AT_RED, "Electricity violently arcs around $n's body.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 4:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "Your eyes flash red momentarily.", ch,
          NULL, victim, TO_CHAR);
      act(AT_RED, "$n's eyes flash red.", ch,
          NULL, victim, TO_VICT);
      act(AT_RED, "$n's eyes flash red.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 3:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "Your hair flashes red.", ch, NULL, victim,
          TO_CHAR);
      act(AT_YELLOW, "$n's hair flashes red.", ch, NULL, victim,
          TO_VICT);
      act(AT_YELLOW, "$n's hair flashes red.", ch, NULL, victim,
          TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 2:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "Your voice bellows out through the area with a deep angry howl.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n's voice bellows out through the area with a deep angry howl.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n's voice bellows out through the area with a deep angry howl.",
          ch, NULL, victim, TO_NOTVICT);
      do_yell(ch, "rrrrrRRRRRRRRAAAAAAAAAAAAHHHHHHHHHH!!!");
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 1:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "You howl with anger as your hair turns red. A giant golden",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "aura envelopes you, intertwined with a menacing crimson hue.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "You've tasted what it truly means to become a God.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "$n howls with anger as $s hair turns red. A giant golden",
          ch, NULL, NULL, TO_NOTVICT);
      act(AT_RED,
          "aura envelopes $m, intertwined with a menacing crimson hue.", ch,
          NULL, NULL, TO_NOTVICT);
      act(AT_RED,
          "$n has tasted what it truly means to become a God...",
          ch, NULL, NULL, TO_NOTVICT);
      act(AT_RED,
          "You look up at $N with rage filled eyes, ready to kill...",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n looks up at you with rage filled eyes, ready to kill...",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n looks up at $N with rage filled eyes, ready to kill...",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);

      ch->skillssgod = 1;
      if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
        xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
      if (xIS_SET((ch)->affected_by, AFF_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_OOZARU);
      if (xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_GOLDEN_OOZARU);
      if (!xIS_SET(ch->affected_by, AFF_SSJ))
        xSET_BIT((ch)->affected_by, AFF_SSJ);
      if (!xIS_SET(ch->affected_by, AFF_SSJ2))
        xSET_BIT((ch)->affected_by, AFF_SSJ2);
      if (!xIS_SET(ch->affected_by, AFF_SSJ3))
        xSET_BIT((ch)->affected_by, AFF_SSJ3);
      xSET_BIT((ch)->affected_by, AFF_SSJ4);
      ch->pl = ch->exp * 425;
      transStatApply(ch, 30, 15, 8, 15);
      ch->delay = 0;
      ch->rage = 0;
      add_timer(ch, TIMER_ASUPRESSED, 8, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 8, NULL, 1);
      break;
  }
  return;
}

void rage5(CHAR_DATA *ch, CHAR_DATA *victim) {
  return;
  if (!ch->desc)
    return;
  if (!victim)
    return;
  if (IS_NPC(ch))
    return;
  if (!ch->in_room)
    return;
  if (!victim->in_room)
    return;

  if ((ch->skillssblue >= 1) && (ch->skillssj1 = 0) && (ch->skillssj2 = 0) && (ch->skillssj3 = 0) && (ch->skillssgod = 0))
    return;

  if (ch->delay <= 0) {
    if (!is_saiyan(ch) && !is_hb(ch))
      return;

    if (ch->rage < 1500)
      return;

    if (ch->train < 5670000)
      return;

    if (ch->skillssblue >= 1)
      return;

    switch (number_range(1, 5)) {
      default:
      case 1:
      case 2:
      case 3:
      case 4:
        break;
      case 5:
        if (number_percent() > ((ch->rage - 1500) / 50)) {
          return;
        }
        ch->delay_vict = victim;
        ch->delay = 6;
        break;
    }
  }
  switch (ch->delay) {
    case 6:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "You scream out, in pure rage, your hatred for $N.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for you.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED, "$n screams out, in pure rage, $s hatred for $N.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 5:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_LBLUE, "Countless white flecks begin to gather around you.", ch,
          NULL, victim, TO_CHAR);
      act(AT_LBLUE, "Countless white flecks begin to gather around $n's body.", ch,
          NULL, victim, TO_VICT);
      act(AT_LBLUE, "Countless white flecks begin to gather around $n's body.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 4:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED, "Your golden aura grows.", ch,
          NULL, victim, TO_CHAR);
      act(AT_RED, "$n's golden aura grows", ch,
          NULL, victim, TO_VICT);
      act(AT_RED, "$n's golden aura grows.", ch,
          NULL, victim, TO_NOTVICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 3:
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_LBLUE, "Your hair flashes blue.", ch, NULL, victim,
          TO_CHAR);
      act(AT_LBLUE, "$n's hair flashes blue.", ch, NULL, victim,
          TO_VICT);
      act(AT_LBLUE, "$n's hair flashes blue.", ch, NULL, victim,
          TO_NOTVICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_RED, "=-", ch, NULL, victim, TO_VICT);
      act(AT_RED, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 2:
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_RED,
          "Your voice bellows out through the area with a deep angry howl.",
          ch, NULL, victim, TO_CHAR);
      act(AT_RED,
          "$n's voice bellows out through the area with a deep angry howl.",
          ch, NULL, victim, TO_VICT);
      act(AT_RED,
          "$n's voice bellows out through the area with a deep angry howl.",
          ch, NULL, victim, TO_NOTVICT);
      do_yell(ch, "rrrrrRRRRRRRRAAAAAAAAAAAAHHHHHHHHHH!!!");
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_VICT);
      act(AT_YELLOW, "=-", ch, NULL, victim, TO_NOTVICT);
      ch->delay -= 1;
      add_timer(ch, TIMER_DELAY, 4, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 4, NULL, 1);
      break;
    case 1:
      act(AT_LBLUE, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_LBLUE, "=-", ch, NULL, victim, TO_VICT);
      act(AT_LBLUE, "=-", ch, NULL, victim, TO_NOTVICT);
      act(AT_LBLUE,
          "A blue aura of God Ki envelopes you.", ch, NULL, NULL, TO_CHAR);
      act(AT_LBLUE,
          "Your hair and eyes turn blue", ch, NULL, NULL, TO_CHAR);
      act(AT_LBLUE,
          "The ground shakes around you, the skies move.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_LBLUE,
          "$n's hair turns blue and the ground around you shakes.",
          ch, NULL, NULL, TO_NOTVICT);
      act(AT_LBLUE,
          "$n's power is moving the sky around you.", ch,
          NULL, NULL, TO_NOTVICT);
      act(AT_LBLUE,
          "$n's power is no longer sensed.",
          ch, NULL, NULL, TO_NOTVICT);
      act(AT_LBLUE,
          "You look up at $N and all doubts disappear. You've ascended beyond even a God.",
          ch, NULL, victim, TO_CHAR);
      act(AT_LBLUE,
          "$n looks up at you, but you can't sense their power.",
          ch, NULL, victim, TO_VICT);
      act(AT_LBLUE,
          "$n looks up at $N, ready for battle.",
          ch, NULL, victim, TO_NOTVICT);
      act(AT_LBLUE, "=-", ch, NULL, victim, TO_CHAR);
      act(AT_LBLUE, "=-", ch, NULL, victim, TO_VICT);
      act(AT_LBLUE, "=-", ch, NULL, victim, TO_NOTVICT);

      ch->skillssblue = 1;
      if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
        xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
      if (xIS_SET((ch)->affected_by, AFF_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_OOZARU);
      if (xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU))
        xREMOVE_BIT((ch)->affected_by, AFF_GOLDEN_OOZARU);
      if (!xIS_SET(ch->affected_by, AFF_SSJ))
        xSET_BIT((ch)->affected_by, AFF_SSJ);
      if (!xIS_SET(ch->affected_by, AFF_SSJ2))
        xSET_BIT((ch)->affected_by, AFF_SSJ2);
      if (!xIS_SET(ch->affected_by, AFF_SSJ3))
        xSET_BIT((ch)->affected_by, AFF_SSJ3);
      if (!xIS_SET(ch->affected_by, AFF_SSJ4))
        xSET_BIT((ch)->affected_by, AFF_SSJ4);
      xSET_BIT((ch)->affected_by, AFF_SGOD);
      ch->pl = ch->exp * 600;
      transStatApply(ch, 50, 25, 15, 20);
      ch->delay = 0;
      ch->rage = 0;
      add_timer(ch, TIMER_ASUPRESSED, 8, NULL, 1);
      add_timer(victim, TIMER_ASUPRESSED, 8, NULL, 1);
      break;
  }
  return;
}

/* TODO: fix the modifier, this is heavily favored for humans to win by stacking the stat CON  - case */
void heart_calc(CHAR_DATA *ch, char *argument) {
  double pl_mult = 1;
  double pl_mult2 = 1;
  int con = 0;
  double chk = 0;

  if (!is_human(ch) || IS_NPC(ch))
    return;

  if (ch->exp > ch->pl)
    return;

  if (!IS_SET(ch->pcdata->combatFlags, CMB_NO_HEART))
    return;

  if (IS_SET(ch->pcdata->combatFlags, CMB_NO_HEART))
    return;

  if (!xIS_SET(ch->affected_by, AFF_HEART))
    ch->heart_pl = ch->pl;

  con = (get_curr_con(ch)) - 10;

  if (con < 1) {
    if (xIS_SET(ch->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    ch->pl = ch->heart_pl;
    return;
  }
  if (ch->hit >= 90 || ch->hit <= 0) {
    if (xIS_SET(ch->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    ch->pl = ch->heart_pl;
    return;
  }
  pl_mult = con / 10;

  chk = (double)(18 - (ch->hit / 5)) / 18 * pl_mult;

  if (chk <= 1) {
    if (xIS_SET(ch->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    ch->pl = ch->heart_pl;
    return;
  }
  if (!xIS_SET(ch->affected_by, AFF_HEART)) {
    xSET_BIT(ch->affected_by, AFF_HEART);
  }
  pl_mult2 = (double)ch->heart_pl / ch->exp;
  chk = (double)pl_mult2 + chk;

  ch->pl = ch->exp * chk;

  act(AT_RED, "", ch, NULL, NULL, TO_CHAR);
  act(AT_RED, "", ch, NULL, NULL, TO_NOTVICT);

  return;
}

void do_powerdown(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *och;
  CHAR_DATA *och_next;

  if (IS_AFFECTED(ch, AFF_BIOJR) && IS_NPC(ch))
    return;

  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
    xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
  if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
    xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
  if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
    xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
  if (ch->pl <= ch->exp) {
    send_to_char("You are already powered down.\n\r", ch);
    ch->powerup = 0;
    if (xIS_SET((ch)->affected_by, AFF_SSJ)) {
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
      if (!IS_NPC(ch)) {
        ch->pcdata->haircolor =
            ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
      xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
    if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
      xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
    if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
      xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
    if (xIS_SET((ch)->affected_by, AFF_USSJ))
      xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
    if (xIS_SET((ch)->affected_by, AFF_USSJ2))
      xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
    if (xIS_SET((ch)->affected_by, AFF_SSJ2))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
    if (xIS_SET((ch)->affected_by, AFF_SSJ3))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
    if (xIS_SET((ch)->affected_by, AFF_SSJ4))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
    if (xIS_SET((ch)->affected_by, AFF_SGOD))
      xREMOVE_BIT((ch)->affected_by, AFF_SGOD);
    if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
      xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
    if (xIS_SET((ch)->affected_by, AFF_SNAMEK))
      xREMOVE_BIT((ch)->affected_by, AFF_SNAMEK);
    if (xIS_SET((ch)->affected_by, AFF_ICER2))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    if (xIS_SET((ch)->affected_by, AFF_ICER3))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    if (xIS_SET((ch)->affected_by, AFF_ICER4))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    if (xIS_SET((ch)->affected_by, AFF_ICER5))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    if (xIS_SET((ch)->affected_by, AFF_GOLDENFORM))
      xREMOVE_BIT((ch)->affected_by, AFF_GOLDENFORM);
    if (xIS_SET((ch)->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    if (xIS_SET((ch)->affected_by, AFF_HYPER))
      xREMOVE_BIT(ch->affected_by, AFF_HYPER);
    if (xIS_SET((ch)->affected_by, AFF_EXTREME))
      xREMOVE_BIT((ch)->affected_by, AFF_EXTREME);
    if (xIS_SET((ch)->affected_by, AFF_SEMIPERFECT))
      xREMOVE_BIT(ch->affected_by, AFF_SEMIPERFECT);
    if (xIS_SET((ch)->affected_by, AFF_PERFECT))
      xREMOVE_BIT(ch->affected_by, AFF_PERFECT);
    if (xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT))
      xREMOVE_BIT(ch->affected_by, AFF_ULTRAPERFECT);
    if (xIS_SET((ch)->affected_by, AFF_GROWTH))
      xREMOVE_BIT(ch->affected_by, AFF_GROWTH);
    if (xIS_SET((ch)->affected_by, AFF_GIANT))
      xREMOVE_BIT(ch->affected_by, AFF_GIANT);
    if (xIS_SET((ch)->affected_by, AFF_EVIL_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_EVIL_TRANS);
    if (xIS_SET((ch)->affected_by, AFF_SUPER_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_SUPER_TRANS);
    if (xIS_SET((ch)->affected_by, AFF_KID_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_KID_TRANS);
	if (xIS_SET((ch)->affected_by, AFF_THIN_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_THIN_TRANS);
    if (xIS_SET((ch)->affected_by, AFF_MYSTIC))
      xREMOVE_BIT(ch->affected_by, AFF_MYSTIC);
    if (xIS_SET((ch)->affected_by, AFF_SUPERANDROID))
      xREMOVE_BIT(ch->affected_by, AFF_SUPERANDROID);
    if (xIS_SET((ch)->affected_by, AFF_EVILBOOST))
      xREMOVE_BIT(ch->affected_by, AFF_EVILBOOST);
    if (xIS_SET((ch)->affected_by, AFF_EVILSURGE))
      xREMOVE_BIT(ch->affected_by, AFF_EVILSURGE);
    if (xIS_SET((ch)->affected_by, AFF_EVILOVERLOAD))
      xREMOVE_BIT(ch->affected_by, AFF_EVILOVERLOAD);
    transStatRemove(ch);
    heart_calc(ch, "");
    if (is_splitformed(ch)) {
      for (och = first_char; och; och = och_next) {
        och_next = och->next;

        if (!IS_NPC(och))
          continue;

        if (is_split(och) && och->master == ch)
          do_powerdown(och, "");
      }
    }
    return;
  }
  if (ch->exp != ch->pl && xIS_SET(ch->affected_by, AFF_OOZARU)) {
    send_to_char("You can't while you are an Oozaru.\n\r", ch);
    return;
  }
  if (ch->exp != ch->pl && xIS_SET(ch->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't while you are a Golden Oozaru.\n\r",
                 ch);
    return;
  }
  if (ch->exp != ch->pl && xIS_SET(ch->affected_by, AFF_MAKEOSTAR)) {
    send_to_char("You can't while you are being affected by the Makeo Star.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SSJ) || xIS_SET((ch)->affected_by, AFF_USSJ) || xIS_SET((ch)->affected_by, AFF_USSJ2) || xIS_SET((ch)->affected_by, AFF_SSJ2) || xIS_SET((ch)->affected_by, AFF_SSJ3) || xIS_SET((ch)->affected_by, AFF_SSJ4) || xIS_SET((ch)->affected_by, AFF_SGOD) || xIS_SET((ch)->affected_by, AFF_KAIOKEN) || xIS_SET((ch)->affected_by, AFF_HYPER) || xIS_SET((ch)->affected_by, AFF_EXTREME) || xIS_SET((ch)->affected_by, AFF_SNAMEK) || xIS_SET((ch)->affected_by, AFF_ICER2) || xIS_SET((ch)->affected_by, AFF_ICER3) || xIS_SET((ch)->affected_by, AFF_ICER4) || xIS_SET((ch)->affected_by, AFF_ICER5) || xIS_SET((ch)->affected_by, AFF_GOLDENFORM) || xIS_SET((ch)->affected_by, AFF_SEMIPERFECT) || xIS_SET((ch)->affected_by, AFF_PERFECT) || xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT) || xIS_SET((ch)->affected_by, AFF_GROWTH) || xIS_SET((ch)->affected_by, AFF_GIANT) || xIS_SET((ch)->affected_by, AFF_EVIL_TRANS) || xIS_SET((ch)->affected_by, AFF_SUPER_TRANS) || xIS_SET((ch)->affected_by, AFF_KID_TRANS) ||xIS_SET((ch)->affected_by, AFF_THIN_TRANS) || xIS_SET((ch)->affected_by, AFF_MYSTIC) || xIS_SET((ch)->affected_by, AFF_SUPERANDROID) || xIS_SET((ch)->affected_by, AFF_EVILBOOST) || xIS_SET((ch)->affected_by, AFF_EVILSURGE) || xIS_SET((ch)->affected_by, AFF_EVILOVERLOAD)) {
    if (xIS_SET((ch)->affected_by, AFF_SSJ)) {
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
      if (!IS_NPC(ch)) {
        ch->pcdata->haircolor =
            ch->pcdata->orignalhaircolor;
        ch->pcdata->eyes = ch->pcdata->orignaleyes;
      }
    }
    if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
      xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
    if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))
      xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
    if (xIS_SET((ch)->affected_by, AFF_SAFEMAX))
      xREMOVE_BIT((ch)->affected_by, AFF_SAFEMAX);
    if (xIS_SET((ch)->affected_by, AFF_USSJ))
      xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
    if (xIS_SET((ch)->affected_by, AFF_USSJ2))
      xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
    if (xIS_SET((ch)->affected_by, AFF_SSJ2))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
    if (xIS_SET((ch)->affected_by, AFF_SSJ3))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
    if (xIS_SET((ch)->affected_by, AFF_SSJ4))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
    if (xIS_SET((ch)->affected_by, AFF_SGOD))
      xREMOVE_BIT((ch)->affected_by, AFF_SGOD);
    if (xIS_SET((ch)->affected_by, AFF_KAIOKEN))
      xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
    if (xIS_SET((ch)->affected_by, AFF_SNAMEK))
      xREMOVE_BIT((ch)->affected_by, AFF_SNAMEK);
    if (xIS_SET((ch)->affected_by, AFF_ICER2))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    if (xIS_SET((ch)->affected_by, AFF_ICER3))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    if (xIS_SET((ch)->affected_by, AFF_ICER4))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    if (xIS_SET((ch)->affected_by, AFF_ICER5))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    if (xIS_SET((ch)->affected_by, AFF_GOLDENFORM))
      xREMOVE_BIT((ch)->affected_by, AFF_GOLDENFORM);
    if (xIS_SET((ch)->affected_by, AFF_HYPER))
      xREMOVE_BIT(ch->affected_by, AFF_HYPER);
    if (xIS_SET((ch)->affected_by, AFF_EXTREME))
      xREMOVE_BIT((ch)->affected_by, AFF_EXTREME);
    if (xIS_SET((ch)->affected_by, AFF_SEMIPERFECT))
      xREMOVE_BIT(ch->affected_by, AFF_SEMIPERFECT);
    if (xIS_SET((ch)->affected_by, AFF_PERFECT))
      xREMOVE_BIT(ch->affected_by, AFF_PERFECT);
    if (xIS_SET((ch)->affected_by, AFF_ULTRAPERFECT))
      xREMOVE_BIT(ch->affected_by, AFF_ULTRAPERFECT);
    if (xIS_SET((ch)->affected_by, AFF_GROWTH))
      xREMOVE_BIT(ch->affected_by, AFF_GROWTH);
    if (xIS_SET((ch)->affected_by, AFF_GIANT))
      xREMOVE_BIT(ch->affected_by, AFF_GIANT);
    if (xIS_SET((ch)->affected_by, AFF_EVIL_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_EVIL_TRANS);
    if (xIS_SET((ch)->affected_by, AFF_SUPER_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_SUPER_TRANS);
    if (xIS_SET((ch)->affected_by, AFF_KID_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_KID_TRANS);
	if (xIS_SET((ch)->affected_by, AFF_THIN_TRANS))
      xREMOVE_BIT(ch->affected_by, AFF_THIN_TRANS);
    if (xIS_SET((ch)->affected_by, AFF_MYSTIC))
      xREMOVE_BIT(ch->affected_by, AFF_MYSTIC);
    if (xIS_SET((ch)->affected_by, AFF_SUPERANDROID))
      xREMOVE_BIT(ch->affected_by, AFF_SUPERANDROID);
    if (xIS_SET((ch)->affected_by, AFF_EVILBOOST))
      xREMOVE_BIT(ch->affected_by, AFF_EVILBOOST);
    if (xIS_SET((ch)->affected_by, AFF_EVILSURGE))
      xREMOVE_BIT(ch->affected_by, AFF_EVILSURGE);
    if (xIS_SET((ch)->affected_by, AFF_EVILOVERLOAD))
      xREMOVE_BIT(ch->affected_by, AFF_EVILOVERLOAD);
  }
  ch->powerup = 0;
  send_to_pager_color("&BYou power down and return to normal.\n\r", ch);
  if (xIS_SET((ch)->affected_by, AFF_HEART))
    xREMOVE_BIT(ch->affected_by, AFF_HEART);
  transStatRemove(ch);
  ch->pl = ch->exp;
  ch->releasepl = ch->exp;
  ch->pcdata->suppress = ch->exp;
  heart_calc(ch, "");

  if (is_splitformed(ch)) {
    for (och = first_char; och; och = och_next) {
      och_next = och->next;

      if (!IS_NPC(och))
        continue;

      if (is_split(och) && och->master == ch)
        do_powerdown(och, "");
    }
  }
  return;
}

void do_powerup(CHAR_DATA *ch, char *argument) {
  char arg[MAX_INPUT_LENGTH];
  double safemaximum = 0;
  int form_mastery = 0;
  double plmod = 0;
  int auraColor = AT_WHITE;
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

  one_argument(argument, arg);
  form_mastery = (ch->train / 90000);
  safemaximum = form_mastery;
  plmod = (ch->pl / ch->exp);

  if (IS_NPC(ch))
    return;
  if (ch->pcdata->auraColorPowerUp > 0)
    auraColor = ch->pcdata->auraColorPowerUp;
  if (ch->position < POS_STANDING) {
    send_to_char("You must be standing to do that.\n\r", ch);
    return;
  }
  if (is_saiyan(ch) || is_hb(ch)) {
    onestr = ch->perm_str * 0.20;
    twostr = ch->perm_str * 0.30;
    threestr = ch->perm_str * 0.50;
    fourstr = ch->perm_str * 0.50;
    fivestr = ch->perm_str * 0.60;
    sixstr = ch->perm_str;
    sevenstr = ch->perm_str * 1.25;
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
    fivecon = ch->perm_con * 0.50;
    sixcon = ch->perm_con;
    sevencon = ch->perm_con * 1.25;
  } else if (is_icer(ch)) {
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
  } else if (is_namek(ch)) {
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
  } else if (is_human(ch) || is_kaio(ch)) {
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
  }
  if (arg[0] == '\0') {
    if (ch->pl >= ch->exp) {
      send_to_char("Powerup how? Commands are: 'begin', 'push', 'stop', 'train', and 'release'.\n\r", ch);
      send_to_char("'powerup push' is dangerous and can only be used after first powering up to maximum.\n\r", ch);
      return;
    }
    if (ch->pl < ch->exp) {
      ch->pl = ch->exp;
      act(AT_WHITE, "You unsuppress your power.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n unsuppresses $s power.", ch, NULL, NULL, TO_NOTVICT);
      return;
    }
  }
  if (!str_cmp(arg, "train") && !xIS_SET((ch)->affected_by, AFF_POWERUPTRAIN) && !xIS_SET((ch)->affected_by, AFF_POWERCHANNEL) && !xIS_SET((ch)->affected_by, AFF_OVERCHANNEL)) {
    xSET_BIT((ch)->affected_by, AFF_POWERUPTRAIN);
    act(AT_WHITE, "You enter a state of intense concentration and begin carefully regulating your power.", ch, NULL, NULL, TO_CHAR);
    act(AT_WHITE, "$n enters a state of intense concentration.", ch, NULL, NULL, TO_NOTVICT);
    return;
  }
  if (!str_cmp(arg, "ssj1")) {
    if (!is_saiyan(ch) && !is_hb(ch)) {
      send_to_char("You don't even HAVE Saiyan blood, you clown.\n\r", ch);
      return;
    } else if (ch->masteryssj < 2610000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SSJ)) {
      send_to_char("You're already a Super Saiyan.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_SSJ);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_YELLOW, "Your eyes turn blue, your hair flashes blonde and a fiery golden aura erupts around you!", ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW, "$n's hair suddenly flashes blonde, transcending beyond $s normal limits in a fiery display of golden ki!", ch, NULL, NULL, TO_NOTVICT);
      ch->powerup = 0;
      ch->pl = ch->exp * 50;
      transStatApply(ch, onestr, onespd, oneint, onecon);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 0;
        ch->pcdata->haircolor = 3;
      }
      return;
    }
  } else if (!str_cmp(arg, "ssj2")) {
    if (!is_saiyan(ch) && !is_hb(ch)) {
      send_to_char("You don't even HAVE Saiyan blood, you clown.\n\r", ch);
      return;
    } else if (ch->masteryssj < 3510000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (!xIS_SET((ch)->affected_by, AFF_SSJ)) {
      send_to_char("You must be transformed into a Super Saiyan to attempt this.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SSJ2)) {
      send_to_char("You're already too powerful to see any benefit from doing that.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_USSJ);
      xSET_BIT((ch)->affected_by, AFF_USSJ2);
      xSET_BIT((ch)->affected_by, AFF_SSJ2);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_YELLOW, "In an intense explosion of rage your power grows, sending arcing bolts of energy from your body.", ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW, "Your hair stands further on end as you ascend to the true next level.", ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW, "$n is enveloped in a storm golden ki. Wreathed in crackling, pure energy, $e truly ascends to the next level.", ch, NULL, NULL, TO_NOTVICT);
      act(AT_YELLOW, "$n stares straight ahead with absolute confidence.", ch, NULL, NULL, TO_NOTVICT);
      ch->powerup = 29;
      ch->pl = ch->exp * 225;
      transStatApply(ch, fourstr, fourspd, fourint, fourcon);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 0;
        ch->pcdata->haircolor = 3;
      }
      return;
    }
  } else if (!str_cmp(arg, "ssj3")) {
    if (!is_saiyan(ch) && !is_hb(ch)) {
      send_to_char("You don't even HAVE Saiyan blood, you clown.\n\r", ch);
      return;
    } else if (ch->masteryssj < 4680000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (!xIS_SET((ch)->affected_by, AFF_SSJ2)) {
      send_to_char("You must be transformed into a Super Saiyan 2 to attempt this.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SSJ3)) {
      send_to_char("You're already too powerful to see any benefit from doing that.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_SSJ3);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_YELLOW, "An earth-shattering burst of energy expands your aura. Your eyebrows disappear and your hair lengthens, flowing down your back.", ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW, "Only the stench of ozone accompanies the countless bolts of energy wreathing your body.", ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW, "The world feel as though it could pull apart as $n's aura expands! $s eyebrows disappear slowly and $s hair lengthens, flowing down $s back.", ch, NULL, NULL, TO_NOTVICT);
      act(AT_YELLOW, "When the bright light fades, $n stands within a wreath of countless bolts of energy, unleashing the primal rage of the Saiyan race.", ch, NULL, NULL, TO_NOTVICT);
      ch->powerup = 39;
      ch->pl = ch->exp * 350;
      transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 0;
        ch->pcdata->haircolor = 3;
      }
      return;
    }
  } else if (!str_cmp(arg, "ssgod")) {
    if (!is_saiyan(ch) && !is_hb(ch)) {
      send_to_char("You don't even HAVE Saiyan blood, you clown.\n\r", ch);
      return;
    } else if (ch->masteryssj < 5670000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (!xIS_SET((ch)->affected_by, AFF_SSJ3)) {
      send_to_char("You must be transformed into a Super Saiyan 3 to attempt this.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SSJ4)) {
      send_to_char("You're already too powerful to see any benefit from doing that.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_SSJ4);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_RED, "Your aura fades and your hair and eyes return to normal. However, in the next instant something inside you changes.", ch, NULL, NULL, TO_CHAR);
      act(AT_RED, "Godly Ki radiates from deep within, and with a mighty shout that pierces the heavens, a brilliant red and gold aura encompasses you.", ch, NULL, NULL, TO_CHAR);
      act(AT_RED, "Your hair and eyes flash red, tinted subtly with violet as you ascend beyond your mortal restrictions.", ch, NULL, NULL, TO_CHAR);
      act(AT_RED, "$n's hair and eyes return to normal. However, in the next instant something feels very different.", ch, NULL, NULL, TO_NOTVICT);
      act(AT_RED, "$n is encompassed in a massive aura of crimson and gold, $s hair and eyes shifting red with a subtle violet tint.", ch, NULL, NULL, TO_NOTVICT);
      ch->powerup = 48;
      ch->pl = ch->exp * 500;
      transStatApply(ch, sixstr, sixspd, sixint, sixcon);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 0;
        ch->pcdata->haircolor = 3;
      }
      return;
    }
  } else if (!str_cmp(arg, "4th")) {
    if (is_saiyan(ch) || is_hb(ch)) {
      send_to_char("A filthy monkey like you? I don't think so.\n\r", ch);
      return;
    }
    if (!is_icer(ch)) {
      send_to_char("You clearly lack proper breeding.\n\r", ch);
      return;
    } else if (ch->masteryicer < 1800000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_ICER4)) {
      send_to_char("You're already in your fourth form.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_ICER4);
      if (xIS_SET((ch)->affected_by, AFF_ICER2))
        xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
      if (xIS_SET((ch)->affected_by, AFF_ICER3))
        xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_PURPLE, "In an explosion of ki your body fades away, emerging from the dust in a new form!", ch, NULL, NULL, TO_CHAR);
      act(AT_PURPLE, "Your chitinous body is replaced with smooth skin and patches as reflective as glass.", ch, NULL, NULL, TO_CHAR);
      act(AT_PURPLE, "$n emerges from an explosion of ki, $s body transformed into a sleek, smooth form.", ch, NULL, NULL, TO_NOTVICT);
      ch->pl = ch->exp * 50;
      ch->powerup = 11;
      transStatApply(ch, threestr, threespd, threeint, threecon);
      return;
    }
  } else if (!str_cmp(arg, "5th")) {
    if (is_saiyan(ch) || is_hb(ch)) {
      send_to_char("A filthy monkey like you? I don't think so.\n\r", ch);
      return;
    }
    if (!is_icer(ch)) {
      send_to_char("You clearly lack proper breeding.\n\r", ch);
      return;
    } else if (ch->masteryicer < 4140000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (!xIS_SET((ch)->affected_by, AFF_ICER4)) {
      send_to_char("You must be in your fourth form to do that.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_ICER5)) {
      send_to_char("You're already in your fifth form.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_ICER5);
      if (xIS_SET((ch)->affected_by, AFF_ICER2))
        xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
      if (xIS_SET((ch)->affected_by, AFF_ICER3))
        xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
      if (xIS_SET((ch)->affected_by, AFF_ICER4))
        xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_PURPLE, "Your muscles expand massively in size, swelling with incredible energy!", ch, NULL, NULL, TO_CHAR);
      act(AT_PURPLE, "$n's muscles expand massively in size, swelling with incredible energy!", ch, NULL, NULL, TO_NOTVICT);
      ch->pl = ch->exp * 150;
      transStatApply(ch, fourstr, fourspd, fourint, fourcon);
      ch->powerup = 45;
      return;
    }
  } else if (!str_cmp(arg, "mystic")) {
    if (!is_kaio(ch) && !is_human(ch)) {
      send_to_char("You have other means of unlocking your potential.\n\r", ch);
      return;
    } else if (ch->masterymystic < 2520000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_MYSTIC)) {
      send_to_char("You've already unlocked your potential.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_MYSTIC);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      if (ch->masterymystic >= 4950000) {
        act(auraColor, "In a violent flash of energy, the deepest reaches of your potential explodes to the surface.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "In a violent flash of energy, the deepest reaches of $n's potential explodes to the surface.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, sixstr, sixspd, sixint, sixcon);
        ch->pl = ch->exp * 346.708;
        ch->powerup = 47;
        return;
      } else if (ch->masterymystic >= 4230000) {
        act(auraColor, "Hundreds of bolts of pure, white energy crackle across the surface of your body as you access your true potential.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "Hundreds of bolts of pure, white energy crackle across the surface of $n's body as they access their true potential.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
        ch->pl = ch->exp * 246.399;
        ch->powerup = 40;
        return;
      } else if (ch->masterymystic >= 3600000) {
        act(auraColor, "A massive amount of energy floods through your body as you access your latent potential.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "A massive amount of energy floods through $n's body as they access their latent potential.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, fourstr, fourspd, fourint, fourcon);
        ch->pl = ch->exp * 151.267;
        ch->powerup = 30;
        return;
      } else if (ch->masterymystic >= 2520000) {
        act(auraColor, "A massive amount of energy floods through your body as you access your latent potential.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "A massive amount of energy floods through $n's body as they access their latent potential.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, onestr, onespd, oneint, onecon);
        ch->pl = ch->exp * 51.710;
        ch->powerup = 8;
        return;
      }
    }
  } else if (!str_cmp(arg, "supernamek")) {
    if (!is_namek(ch)) {
      send_to_char("Maybe in another life you were a Namekian, but definitely not in this one.\n\r", ch);
      return;
    } else if (ch->masterynamek < 2520000) {
      send_to_char("You lack enough control over your power to transform instantly.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    } else if (xIS_SET((ch)->affected_by, AFF_SNAMEK)) {
      send_to_char("You're already channeling the knowledge of the Ancients.\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_SNAMEK);
      if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL))
        xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      if (ch->masterynamek >= 5310000) {
        act(auraColor, "The heavens shake and the earth trembles at your feet as you unleash your ancestral might.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "The heavens shake and the earth trembles at $n's feet as they unleash their ancestral might.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, fivestr, fivespd, fiveint, fivecon);
        ch->pl = ch->exp * 346.708;
        ch->powerup = 47;
        return;
      }
      if (ch->masterynamek >= 4500000) {
        act(auraColor, "A blinding white aura suffuses your body, sending crackling energy scattering in all directions.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "A blinding white aura suffuses $n's body, sending crackling energy scattering in all directions.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, fourstr, fourspd, fourint, fourcon);
        ch->pl = ch->exp * 246.399;
        ch->powerup = 40;
        return;
      }
      if (ch->masterynamek >= 3600000) {
        act(auraColor, "Giant beams of light erupt from the surface of your body as you unleash ancient secrets.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "Giant beams of light erupt from the surface of $n's body as they unleash ancient secrets.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, threestr, threespd, threeint, threecon);
        ch->pl = ch->exp * 151.267;
        ch->powerup = 30;
        return;
      }
      if (ch->masterynamek >= 2520000) {
        act(auraColor, "A brilliant white light emerges from within, flooding the room with a sense of spiritual calm.", ch, NULL, NULL, TO_CHAR);
        act(auraColor, "A brilliant white light emerges from within $n, flooding the room with a sense of spiritual calm.", ch, NULL, NULL, TO_NOTVICT);
        transStatApply(ch, onestr, onespd, oneint, onecon);
        ch->pl = ch->exp * 50;
        ch->powerup = 0;
        return;
      }
    }
  } else if (!str_cmp(arg, "release")) {
    if ((ch->pl - (ch->pl * 0.03)) > ch->exp) {
      ch->pl -= (ch->pl * 0.03);
      act(AT_WHITE, "You take a deep breath, releasing some of your pent-up energy.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n takes a deep breath, releasing some pent-up energy.", ch, NULL, NULL, TO_NOTVICT);
      return;
    } else if ((ch->pl - (ch->pl * 0.03)) <= ch->exp) {
      send_to_char("It might be a better idea to 'powerdown'.\n\r", ch);
      return;
    }
  } else if (!str_cmp(arg, "begin")) {
    if (xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You'd have to push yourself to go beyond this level.\n\r", ch);
      return;
    }
    if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL || xIS_SET((ch)->affected_by, AFF_OVERCHANNEL))) {
      send_to_char("You're already powering up!\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_WHITE, "You ball your hands into fists and begin raising your power.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n balls $s hands into fists and begins raising $s power.", ch, NULL, NULL, TO_NOTVICT);
      return;
    }
  } else if (!str_cmp(arg, "push")) {
    if (!xIS_SET((ch)->affected_by, AFF_SAFEMAX)) {
      send_to_char("You have no reason to tear your body apart like that!\n\r", ch);
      return;
    }
    if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL)) {
      send_to_char("You're already pushing beyond your limits!\n\r", ch);
      return;
    } else {
      xSET_BIT((ch)->affected_by, AFF_OVERCHANNEL);
      act(AT_WHITE, "You push beyond your body's normal limits, risking life and limb to increase your power.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n pushes beyond $s body's normal limits.", ch, NULL, NULL, TO_NOTVICT);
      return;
    }
  } else if (!str_cmp(arg, "stop")) {
    if (xIS_SET((ch)->affected_by, AFF_POWERCHANNEL)) {
      xREMOVE_BIT((ch)->affected_by, AFF_POWERCHANNEL);
      act(AT_WHITE, "You cut your display short.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n cuts $s display short.", ch, NULL, NULL, TO_NOTVICT);
      return;
    }
    if (xIS_SET((ch)->affected_by, AFF_OVERCHANNEL)) {
      xREMOVE_BIT((ch)->affected_by, AFF_OVERCHANNEL);
      act(AT_WHITE, "You stop pushing your limits and collapse to your knees.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n stops pushing $s limits and collapses to $s knees.", ch, NULL, NULL, TO_NOTVICT);
      return;
    }
    if (xIS_SET((ch)->affected_by, AFF_POWERUPTRAIN)) {
      xREMOVE_BIT((ch)->affected_by, AFF_POWERUPTRAIN);
      act(AT_WHITE, "You put a stop to your training.", ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE, "$n puts a stop to $s training.", ch, NULL, NULL, TO_NOTVICT);
      return;
    } else {
      send_to_char("Stop what? You're not doing anything.\n\r", ch);
      return;
    }
  } else {
    send_to_char("Powerup how? Commands are: 'begin', 'push', and 'stop'.\n\r", ch);
    send_to_char("'powerup push' is dangerous and can only be used after first powering up to maximum.\n\r", ch);
    return;
  }
}

void show_char_to_char_scan(CHAR_DATA *list, CHAR_DATA *ch,
                            OBJ_DATA *scouter) {
  CHAR_DATA *rch;
  long double scaned_pl = (long double)0;
  long double scouter_pl = (long double)0;

  for (rch = list; rch; rch = rch->next_in_room) {
    if (rch == ch)
      continue;

    if (IS_NPC(rch))
      scaned_pl = rch->exp;
    else
      scaned_pl = rch->pl;

    if (!is_android(ch) && !is_superandroid(ch)) {
      scouter_pl =
          (long double)(scouter->value[2] * (long double)100);
      if ((long double)scaned_pl > (long double)scouter_pl) {
        scaned_pl = -1;
      }
      if (scaned_pl == -1) {
        act(AT_GREY,
            "Your scouter explodes from a very strong power.",
            ch, NULL, rch, TO_CHAR);
        act(AT_GREY,
            "$n's scouter explodes from someones power.",
            ch, NULL, rch, TO_ROOM);
        obj_from_char(scouter);
        extract_obj(scouter);
        break;
      }
    }
    if (is_android(ch) || is_superandroid(ch)) {
      if (ch->pl < ch->exp) {
        if (scaned_pl >
            (long double)(ch->exp * (long double)8)) {
          return;
        }
      } else {
        if (scaned_pl >
            (long double)(ch->pl * (long double)8)) {
          return;
        }
      }
    }
    pager_printf_color(ch, "You detect a power level of %s.\n\r",
                       num_punct_ld(scaned_pl));
    return;
  }
}

void do_scan(CHAR_DATA *ch, char *argument) {
  ROOM_INDEX_DATA *was_in_room;
  EXIT_DATA *pexit;
  sh_int dir = -1;
  sh_int dist;
  sh_int max_dist = 0;
  OBJ_DATA *scouter = NULL;
  CHAR_DATA *victim = NULL;
  long double scaned_pl;
  long double scouter_pl;
  bool HBTCset = false;

  set_char_color(AT_ACTION, ch);

  if (IS_AFFECTED(ch, AFF_BLIND)) {
    send_to_char("Not very effective when you're blind...\n\r", ch);
    return;
  }
  if (!is_android(ch) && !is_superandroid(ch) &&
      !wearing_sentient_chip(ch) && (scouter = has_scouter(ch)) == NULL) {
    send_to_char("You need a scouter to do that.\n\r", ch);
    return;
  }
  if (argument[0] == '\0') {
    send_to_char("You must scan in a direction or focus on a person.\n\r",
                 ch);
    return;
  }
  if ((dir = get_door(argument)) == -1 && (victim = get_char_room(ch, argument)) == NULL) {
    send_to_char("Scan what?\n\r", ch);
    return;
  }
  if (IS_IMMORTAL(ch) && get_trust(ch) < 59) {
    send_to_char("You shouldn't be scanning mortals.\n\r", ch);
    return;
  }
  if (victim) {
    act(AT_GREY, "You scan $N with your scouter.", ch, NULL, victim,
        TO_CHAR);
    act(AT_GREY, "$n scans you with $s scouter.", ch, NULL, victim,
        TO_VICT);
    act(AT_GREY, "$n scans $N with $s scouter.", ch, NULL, victim,
        TO_NOTVICT);
    if (IS_NPC(victim))
      scaned_pl = victim->exp;
    else
      scaned_pl = victim->pl;

    if (!is_android(ch) && !is_superandroid(ch) &&
        !wearing_sentient_chip(ch)) {
      scouter_pl = ((long double)(scouter->value[2])) * 100;
      if ((long double)scaned_pl > (long double)scouter_pl) {
        scaned_pl = -1;
      }
    }
    if (scaned_pl == -1) {
      act(AT_GREY, "Your scouter explodes from $N's power.",
          ch, NULL, victim, TO_CHAR);
      act(AT_GREY, "$n's scouter explodes from your power.",
          ch, NULL, victim, TO_VICT);
      act(AT_GREY, "$n's scouter explodes from $N's power.",
          ch, NULL, victim, TO_NOTVICT);
      obj_from_char(scouter);
      extract_obj(scouter);
      return;
    }
    if (is_android(ch) || is_superandroid(ch) ||
        wearing_sentient_chip(ch)) {
      int a = 0;

      if (is_android(ch) || wearing_sentient_chip(ch))
        a = 8;
      if (is_superandroid(ch))
        a = 24;
      if (ch->pl < ch->exp) {
        if (scaned_pl > ch->exp * a) {
          pager_printf_color(ch,
                             "You don't want your head to explode do you?");
          return;
        }
      } else {
        if (scaned_pl > ch->pl * a) {
          pager_printf_color(ch,
                             "You don't want your head to explode do you?");
          return;
        }
      }
    }
    if (IS_NPC(victim))
      pager_printf_color(ch, "%s's power level is %s.",
                         victim->short_descr,
                         num_punct_ld(scaned_pl));
    else
      pager_printf_color(ch, "%s's power level is %s.",
                         victim->name,
                         num_punct_ld(scaned_pl));
    return;
  }
  if (!IS_NPC(ch) && ch->pcdata->nextHBTCDate != 0) {
    HBTCset = true;
  }
  was_in_room = ch->in_room;
  act(AT_GREY, "Scanning $t...", ch, dir_name[dir], NULL, TO_CHAR);
  act(AT_GREY, "$n scans $t.", ch, dir_name[dir], NULL, TO_ROOM);

  if ((pexit = get_exit(ch->in_room, dir)) == NULL) {
    act(AT_GREY, "You can't see $t.", ch, dir_name[dir], NULL,
        TO_CHAR);
    return;
  }
  if (scouter)
    max_dist = scouter->value[2] / 10000;
  else
    max_dist = ch->exp / 100000;

  if (max_dist > 15)
    max_dist = 15;

  for (dist = 1; dist <= max_dist;) {
    char_from_room(ch);
    char_to_room(ch, pexit->to_room);
    set_char_color(AT_RMNAME, ch);
    send_to_char(ch->in_room->name, ch);
    send_to_char("\n\r", ch);
    show_char_to_char_scan(ch->in_room->first_person, ch, scouter);
    if (!has_scouter(ch))
      break;

    switch (ch->in_room->sector_type) {
      default:
        dist++;
        break;
      case SECT_AIR:
        if (number_percent() < 80)
          dist++;
        break;
      case SECT_INSIDE:
      case SECT_FIELD:
      case SECT_UNDERGROUND:
        dist++;
        break;
      case SECT_FOREST:
      case SECT_CITY:
      case SECT_DESERT:
      case SECT_HILLS:
        dist += 2;
        break;
      case SECT_WATER_SWIM:
      case SECT_WATER_NOSWIM:
        dist += 3;
        break;
      case SECT_MOUNTAIN:
      case SECT_UNDERWATER:
      case SECT_OCEANFLOOR:
        dist += 4;
        break;
    }

    if (dist >= max_dist) {
      act(AT_GREY,
          "There is to much interference to scan farther $t.",
          ch, dir_name[dir], NULL, TO_CHAR);
      break;
    }
    if ((pexit = get_exit(ch->in_room, dir)) == NULL) {
      act(AT_GREY,
          "Your scans $t are being blocked by an unknown force...",
          ch, dir_name[dir], NULL, TO_CHAR);
      break;
    }
  }

  char_from_room(ch);
  char_to_room(ch, was_in_room);

  if (HBTCset == false) {
    ch->pcdata->nextHBTCDate = 0;
  }
  return;
}

void do_radar(CHAR_DATA *ch, char *argument) {
  OBJ_DATA *radar;
  CHAR_DATA *vch;
  AREA_DATA *area;
  OBJ_DATA *obj;
  sh_int count = 0;

  if ((radar = has_dragonradar(ch)) == NULL) {
    send_to_char("You must be holding a dragonball radar to do that.\n\r",
                 ch);
    return;
  }
  if (argument[0] == '\0') {
    ch_printf(ch, "\n\rSyntax: radar area\n\r");
    ch_printf(ch, "    or: radar (victim)\n\r");
    ch_printf(ch,
              "\n\rUsing \"radar area\" will sweep the entire area and tell you if it detects any dragonballs, and how many.\n\r");
    ch_printf(ch,
              "Using \"radar (victim)\" will scan a person and see if they are holding any dragonballs.\n\r");
    return;
  }
  if (!str_cmp(argument, "area")) {
    act(AT_GREY, " ", ch, NULL, NULL, TO_CHAR);
    act(AT_GREY,
        "You tap the button on the top of the dragonball radar a couple times.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_GREY, " ", ch, NULL, NULL, TO_NOTVICT);
    act(AT_GREY,
        "$n taps the button on the top of $s dragonball radar a couple times.",
        ch, NULL, NULL, TO_NOTVICT);
    area = ch->in_room->area;
    for (obj = first_object; obj; obj = obj->next) {
      if (obj->item_type != ITEM_DRAGONBALL)
        continue;
      if (!obj->in_room && !obj->carried_by)
        continue;
      if (obj->in_room) {
        if (obj->in_room->area != area)
          continue;
        count++;
        continue;
      }
      if (obj->carried_by) {
        if (!obj->carried_by->in_room)
          continue;
        if (obj->carried_by->in_room->area != area)
          continue;
        count++;
        continue;
      }
    }
    ch_printf(ch,
              "&gThere are &Y%d &gdragonball(s) in this region.\n\r",
              count);
    return;
  }
  if ((vch = get_char_room(ch, argument)) == NULL) {
    ch_printf(ch, "There is nobody here by that name.\n\r");
    return;
  } else {
    act(AT_GREY, " ", ch, NULL, NULL, TO_CHAR);
    act(AT_GREY,
        "You tap the button on the top of the dragonball radar a couple times.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_GREY, " ", ch, NULL, NULL, TO_NOTVICT);
    act(AT_GREY,
        "$n taps the button on the top of $s dragonball radar a couple times.",
        ch, NULL, NULL, TO_NOTVICT);
    if ((obj = carrying_dball(vch)) == NULL) {
      ch_printf(ch, "%s is not carrying any dragonballs.\n\r",
                vch->name);
      return;
    }
    OBJ_DATA *obj_next;

    for (obj = vch->first_carrying; obj != NULL; obj = obj_next) {
      obj_next = obj->next_content;
      if (obj->item_type == ITEM_DRAGONBALL) {
        count++;
      }
    }
    ch_printf(ch, "&g%s is carrying &Y%d &gdragonball(s).\n\r",
              vch->name, count);
    return;
  }
  do_radar(ch, "");
}

/*
 * Check for parry.
 */
bool check_parry(CHAR_DATA *ch, CHAR_DATA *victim) {
  int chances = 0;

  return false;

  if (!IS_AWAKE(victim))
    return false;

  if (IS_NPC(victim) && !xIS_SET(victim->defenses, DFND_PARRY))
    return false;

  if (IS_NPC(victim) && IS_NPC(ch))
    chances = victim->exp / ch->exp;
  if (IS_NPC(victim) && !IS_NPC(ch))
    chances = victim->exp / ch->pl;
  if (!IS_NPC(victim) && IS_NPC(ch))
    chances = victim->pl / ch->exp;
  if (!IS_NPC(victim) && !IS_NPC(ch))
    chances = victim->pl / ch->pl;

  chances += get_curr_int(victim) - 5;

  if (chances != 0 && victim->morph)
    chances += victim->morph->parry;

  if (IS_NPC(victim) && IS_NPC(ch) && !chance(victim, chances + (victim->exp / ch->exp))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (IS_NPC(victim) && !IS_NPC(ch) && !chance(victim, chances + (victim->exp / ch->pl))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (!IS_NPC(victim) && IS_NPC(ch) && !chance(victim, chances + (victim->pl / ch->exp))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (!IS_NPC(victim) && !IS_NPC(ch) && !chance(victim, chances + (victim->pl / ch->pl))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (!IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG))
    /* SB */
    act(AT_BLUE, "You anticipate $n's attack and block it.",
        ch, NULL, victim, TO_VICT);

  if (!IS_NPC(ch) && !IS_SET(ch->pcdata->flags, PCFLAG_GAG)) /* SB */
    act(AT_BLUE, "$N anticipates your attack and blocks it.", ch,
        NULL, victim, TO_CHAR);

  learn_from_success(victim, gsn_parry);
  return (true);
}

/*
 * Check for dodge.
 */
bool check_dodge(CHAR_DATA *ch, CHAR_DATA *victim) {
  int chances = 0;

  return false;

  if (!IS_AWAKE(victim))
    return false;

  if (IS_NPC(victim) && !xIS_SET(victim->defenses, DFND_DODGE))
    return false;

  if (IS_NPC(victim) && IS_NPC(ch))
    chances = victim->exp / ch->exp;
  if (IS_NPC(victim) && !IS_NPC(ch))
    chances = victim->exp / ch->pl;
  if (!IS_NPC(victim) && !IS_NPC(ch))
    chances = victim->pl / ch->exp;
  if (!IS_NPC(victim) && !IS_NPC(ch))
    chances = victim->pl / ch->pl;

  chances += get_curr_dex(victim) - 5;

  if (chances != 0 && victim->morph != NULL)
    chances += victim->morph->dodge;

  /* Consider luck as a factor */
  if (IS_NPC(victim) && IS_NPC(ch) && !chance(victim, chances + (victim->exp / ch->exp))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (IS_NPC(victim) && !IS_NPC(ch) && !chance(victim, chances + (victim->exp / ch->pl))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (!IS_NPC(victim) && IS_NPC(ch) && !chance(victim, chances + (victim->pl / ch->exp))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (!IS_NPC(victim) && !IS_NPC(ch) && !chance(victim, chances + (victim->pl / ch->pl))) {
    learn_from_failure(victim, gsn_dodge);
    return false;
  }
  if (!IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG))
    act(AT_SKILL, "You dodge $n's attack.", ch, NULL, victim,
        TO_VICT);

  if (!IS_NPC(ch) && !IS_SET(ch->pcdata->flags, PCFLAG_GAG))
    act(AT_SKILL, "$N dodges your attack.", ch, NULL, victim,
        TO_CHAR);

  learn_from_success(victim, gsn_dodge);
  return true;
}

bool check_tumble(CHAR_DATA *ch, CHAR_DATA *victim) {
  return (false);
}

void do_tail(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;

  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->level < skill_table[gsn_tail]->skill_level[ch->class]) {
    send_to_char("That isn't quite one of your natural skills.\n\r",
                 ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_tail]->beats);
  if (can_use_skill(ch, number_percent(), gsn_tail)) {
    learn_from_success(ch, gsn_tail);
    global_retcode =
        damage(ch, victim, number_range(1, ch->level), gsn_tail);
  } else {
    learn_from_failure(ch, gsn_tail);
    global_retcode = damage(ch, victim, 0, gsn_tail);
  }
  return;
}

void do_kaiocreate(CHAR_DATA *ch, char *argument) {
  OBJ_DATA *o;
  OBJ_INDEX_DATA *pObjIndex;

  if (IS_NPC(ch))
    return;

  if (argument[0] == '\0') {
    ch_printf(ch, "\n\rSyntax: Kaio create <item>\n\r");
    ch_printf(ch, "\n\r");
    ch_printf(ch, "Item can only be one of the following:\n\r");
    ch_printf(ch, "\n\r");
    ch_printf(ch, "shirt pants belt vest sleeves boots\n\r");
    return;
  }
  if (!(!str_cmp(argument, "shirt")) && !(!str_cmp(argument, "pants")) &&
      !(!str_cmp(argument, "belt")) && !(!str_cmp(argument, "vest")) &&
      !(!str_cmp(argument, "sleeves")) && !(!str_cmp(argument, "boots"))) {
    do_kaiocreate(ch, "");
    return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_kaiocreate]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_kaiocreate]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->carry_number >= 19) {
    ch_printf(ch, "You haven't got any room.\n\r");
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_kaiocreate]->beats);
  if (can_use_skill(ch, number_percent(), gsn_kaiocreate)) {
    int a = 0;

    if (!str_cmp(argument, "shirt"))
      a = 1210;
    else if (!str_cmp(argument, "pants"))
      a = 1211;
    else if (!str_cmp(argument, "vest"))
      a = 1212;
    else if (!str_cmp(argument, "belt"))
      a = 1213;
    else if (!str_cmp(argument, "sleeves"))
      a = 1214;
    else if (!str_cmp(argument, "boots"))
      a = 1215;
    if (a == 0) {
      bug("Serious problem in function kaio create", 0);
      return;
    }
    act(AT_SKILL,
        "You focus your powers as a kaio and craft an item from nothingness.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n focuses $s powers as a kaio and crafts an item from nothingness.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_success(ch, gsn_kaiocreate);
    pObjIndex = get_obj_index(a);
    o = create_object_new(pObjIndex, 1, ORIGIN_OINVOKE, ch->name);
    o = obj_to_char(o, ch);
    save_char_obj(ch);
  } else {
    act(AT_SKILL,
        "You fail to use your kaio powers to create an item.", ch,
        NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n fails to use $s kaio powers to create an item.", ch,
        NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_kaiocreate);
  }
  ch->mana -= skill_table[gsn_kaiocreate]->min_mana;
  return;
}

void do_rescue(CHAR_DATA *ch, char *argument) {
  char arg[MAX_INPUT_LENGTH];
  CHAR_DATA *victim;
  CHAR_DATA *fch;
  int percent;

  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (IS_AFFECTED(ch, AFF_BERSERK)) {
    send_to_char("You aren't thinking clearly...\n\r", ch);
    return;
  }
  one_argument(argument, arg);
  if (arg[0] == '\0') {
    send_to_char("Rescue whom?\n\r", ch);
    return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("They aren't here.\n\r", ch);
    return;
  }
  if (victim == ch) {
    send_to_char("How about fleeing instead?\n\r", ch);
    return;
  }
  if (ch->mount) {
    send_to_char("You can't do that while mounted.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && IS_NPC(victim)) {
    send_to_char("They don't need your help!\n\r", ch);
    return;
  }
  if (!ch->fighting) {
    send_to_char("Too late...\n\r", ch);
    return;
  }
  if ((fch = who_fighting(victim)) == NULL) {
    send_to_char("They are not fighting right now.\n\r", ch);
    return;
  }
  if (who_fighting(victim) == ch) {
    send_to_char("Just running away would be better...\n\r", ch);
    return;
  }
  if (IS_AFFECTED(victim, AFF_BERSERK)) {
    send_to_char("Stepping in front of a berserker would not be an intelligent decision.\n\r",
                 ch);
    return;
  }
  percent = number_percent() - (get_curr_lck(ch) - 14) - (get_curr_lck(victim) - 16);

  WAIT_STATE(ch, skill_table[gsn_rescue]->beats);
  if (!can_use_skill(ch, percent, gsn_rescue)) {
    send_to_char("You fail the rescue.\n\r", ch);
    act(AT_SKILL, "$n tries to rescue you!", ch, NULL, victim,
        TO_VICT);
    act(AT_SKILL, "$n tries to rescue $N!", ch, NULL, victim,
        TO_NOTVICT);
    learn_from_failure(ch, gsn_rescue);
    return;
  }
  act(AT_SKILL, "You rescue $N!", ch, NULL, victim, TO_CHAR);
  act(AT_SKILL, "$n rescues you!", ch, NULL, victim, TO_VICT);
  act(AT_SKILL, "$n moves in front of $N!", ch, NULL, victim, TO_NOTVICT);

  learn_from_success(ch, gsn_rescue);
  adjust_favor(ch, 8, 1);
  stop_fighting(fch, false);
  stop_fighting(victim, false);
  if (ch->fighting)
    stop_fighting(ch, false);

  set_fighting(ch, fch);
  set_fighting(fch, ch);
  return;
}

void do_kick(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_kick))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_kick]->skill_level[ch->class]) {
    send_to_char("You better leave the martial arts to fighters.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_kick]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_kick]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_kick]->focus;

  WAIT_STATE(ch, skill_table[gsn_kick]->beats);
  if (can_use_skill(ch, number_percent(), gsn_kick)) {
    learn_from_success(ch, gsn_kick);
    ch->melee = true;
    global_retcode =
        damage(ch, victim,
               (get_attmod(ch, victim) * number_range(2, 4)),
               TYPE_HIT);
    ch->melee = false;
  } else {
    learn_from_failure(ch, gsn_kick);
    ch->melee = true;
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
    ch->melee = false;
  }

  if (!is_android_h(ch))
    ch->mana -= skill_table[gsn_kick]->min_mana;
}

void do_punch(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float physmult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillpunch < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	physmult = (float)get_curr_str(ch) / 950 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterypunch / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 1;
  adjcost = basecost;
  lowdam = 20;
  highdam = 40;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 6);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You carefully study $N's movements, waiting for the perfect opportunity to strike.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"$N presents an opening, and you hammer $M directly in the stomach! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n carefully studies your movements.", ch,NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n exploits an opening in your defense and hammers you in the stomach! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n carefully studies $N's movements.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n exploits an opening in $N's defense and hammers $M in the stomach! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->punchpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You carefully study $N's movements, waiting for the perfect opportunity to strike.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"$N presents an opening, and you hammer $M directly in the stomach with two consecutive punches! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n carefully studies your movements.", ch,NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n exploits an opening in your defense and hammers you in the stomach with two consecutive punches! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n carefully studies $N's movements.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n exploits an opening in $N's defense and hammers $M in the stomach with two consecutive punches! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->punchpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You carefully study $N's movements, waiting for the perfect opportunity to strike.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"$N presents an opening, and you hammer $M directly in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n carefully studies your movements.", ch,NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n exploits an opening in your defense and hammers you in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n carefully studies $N's movements.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n exploits an opening in $N's defense and hammers $M in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->punchpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You carefully study $N's movements, waiting for the perfect opportunity to strike.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"$N presents an opening, and you hammer $M directly in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n carefully studies your movements.", ch,NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n exploits an opening in your defense and hammers you in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n carefully studies $N's movements.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n exploits an opening in $N's defense and hammers $M in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->punchpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You carefully study $N's movements, waiting for the perfect opportunity to strike.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"$N presents an opening, and you hammer $M directly in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n carefully studies your movements.", ch,NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n exploits an opening in your defense and hammers you in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n carefully studies $N's movements.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n exploits an opening in $N's defense and hammers $M in the stomach with a flurry of blows! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your punch.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s punch.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s punch.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "str", 5);
	ch->train += 1;
	ch->masterypunch += 1;
	ch->strikemastery += 1;
	if (ch->strikemastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_kaioken(CHAR_DATA *ch, char *argument) {
  int arg = 1;
  long double max = 0.0;
  int strMod = 0, spdMod = 0, intMod = 0, conMod = 0;
  char buf[MAX_STRING_LENGTH];
  int kicontrol = 0;

  arg = atoi(argument);

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_kaioken))
      return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SSJ) || xIS_SET((ch)->affected_by, AFF_SSJ2) || xIS_SET((ch)->affected_by, AFF_SSJ3) || xIS_SET((ch)->affected_by, AFF_SSJ4)) {
    send_to_char("You can't use kaioken if you are a Super Saiyan.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't use kaioken while you are an Oozaru.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_EXTREME)) {
    send_to_char("You can't use kaioken while using the 'Extreme Technique'.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_HYPER)) {
    send_to_char("You can't use kaioken while you are using the Hyper technique.\n\r",
                 ch);
    return;
  }
  transStatRemove(ch);
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You power down and return to normal.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_KAIOKEN);
    if (xIS_SET((ch)->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    ch->pl = ch->exp;
    heart_calc(ch, "");
    return;
  }
  if (arg <= 1) {
    act(AT_SKILL,
        "Isn't it kind of pointless to do Kaioken times 1?", ch,
        NULL, NULL, TO_CHAR);
    return;
  }

  kicontrol = get_curr_int(ch);
  max = (kicontrol / 50) + 4;

  if (arg > max) {
    act(AT_SKILL, "Your body can't sustain that level of Kaioken.",
        ch, NULL, NULL, TO_CHAR);
    return;
  }
  if (arg > 20 && !is_kaio(ch)) {
    act(AT_SKILL, "More than 20x Kaioken would rip your body to pieces.", ch, NULL,
        NULL, TO_CHAR);
    return;
  } else if (arg > 30 && is_kaio(ch)) {
    act(AT_SKILL, "More than 30x Kaioken would rip your body to pieces.", ch, NULL,
        NULL, TO_CHAR);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_kaioken]->beats);

  if (can_use_skill(ch, number_percent(), gsn_kaioken)) {
    sprintf(buf,
            "Bright red flames erupt around you as you yell out KAIOKEN TIMES %d!!!",
            arg);
    act(AT_FIRE, buf, ch, NULL, NULL, TO_CHAR);
    if (IS_NPC(ch))
      sprintf(buf,
              "Bright red flames erupt around %s as $e yells out KAIOKEN TIMES %d!!!",
              ch->short_descr, arg);
    else
      sprintf(buf,
              "Bright red flames erupt around %s as $e yells out KAIOKEN TIMES %d!!!",
              ch->name, arg);
    act(AT_FIRE, buf, ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_KAIOKEN);
    if (xIS_SET((ch)->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    ch->pl = ch->exp * arg;
    heart_calc(ch, "");
    learn_from_success(ch, gsn_kaioken);
    switch (arg) {
      case 2:
        strMod = 4;
        spdMod = 6;
        intMod = 0;
        conMod = -1;
        break;
      case 3:
        strMod = 6;
        spdMod = 8;
        intMod = 0;
        conMod = -2;
        break;
      case 4:
        strMod = 8;
        spdMod = 11;
        intMod = 1;
        conMod = -3;
        break;
      case 5:
        strMod = 10;
        spdMod = 13;
        intMod = 1;
        conMod = -3;
        break;
      case 6:
        strMod = 12;
        spdMod = 16;
        intMod = 2;
        conMod = -4;
        break;
      case 7:
        strMod = 14;
        spdMod = 18;
        intMod = 2;
        conMod = -4;
        break;
      case 8:
        strMod = 16;
        spdMod = 20;
        intMod = 3;
        conMod = -5;
        break;
      case 9:
        strMod = 18;
        spdMod = 22;
        intMod = 3;
        conMod = -5;
        break;
      case 10:
        strMod = 20;
        spdMod = 24;
        intMod = 4;
        conMod = -6;
        break;
      case 11:
        strMod = 22;
        spdMod = 26;
        intMod = 5;
        conMod = -7;
        break;
      case 12:
        strMod = 24;
        spdMod = 28;
        intMod = 6;
        conMod = -8;
        break;
      case 13:
        strMod = 26;
        spdMod = 30;
        intMod = 7;
        conMod = -9;
        break;
      case 14:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 15:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 16:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 17:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 18:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 19:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 20:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 21:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 22:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 23:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 24:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 25:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 26:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 27:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 28:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 29:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
      case 30:
        strMod = 28;
        spdMod = 32;
        intMod = 8;
        conMod = -10;
        break;
    }
    transStatApply(ch, strMod, spdMod, intMod, conMod);
  } else {
    sprintf(buf,
            "You yell out KAIOKEN TIMES %d!!!  Red flames flicker around your body, but quickly fade away.",
            arg);
    act(AT_FIRE, buf, ch, NULL, NULL, TO_CHAR);
    sprintf(buf,
            "%s yells out KAIOKEN TIMES %d!!!  Red flames flicker around $s body, but quickly fade away.",
            ch->name, arg);
    act(AT_FIRE, buf, ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_kaioken);
  }

  return;
}

void do_ssj(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ssj))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You can't transform in to a super saiyan while using kaioken.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SSJ) || xIS_SET((ch)->affected_by, AFF_SSJ2) || xIS_SET((ch)->affected_by, AFF_SSJ3) || xIS_SET((ch)->affected_by, AFF_SSJ4)) {
    send_to_char("You power down and return to normal.\n\r", ch);
    if (xIS_SET((ch)->affected_by, AFF_USSJ))
      xREMOVE_BIT((ch)->affected_by, AFF_USSJ);
    if (xIS_SET((ch)->affected_by, AFF_USSJ2))
      xREMOVE_BIT((ch)->affected_by, AFF_USSJ2);
    if (xIS_SET((ch)->affected_by, AFF_SSJ))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ);
    if (xIS_SET((ch)->affected_by, AFF_SSJ2))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);
    if (xIS_SET((ch)->affected_by, AFF_SSJ3))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
    if (xIS_SET((ch)->affected_by, AFF_SSJ4))
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
    ch->pl = ch->exp;
    transStatRemove(ch);
    if (!IS_NPC(ch)) {
      ch->pcdata->haircolor = ch->pcdata->orignalhaircolor;
      ch->pcdata->eyes = ch->pcdata->orignaleyes;
    }
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) || xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't transform while you are an Oozaru.\n\r",
                 ch);
    return;
  }
  if (ch->mana < skill_table[gsn_ssj]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_ssj]->beats);
  if (can_use_skill(ch, number_percent(), gsn_ssj)) {
    act(AT_YELLOW,
        "Your hair flashes blonde and your eyes turn blue as a fiery aura erupts around you.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_YELLOW,
        "$n's hair flashes blonde and $s eyes turn blue as a fiery aura erupts around $m.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_SSJ);
    ch->pl = ch->exp * 50;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 8);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_ssj);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 0;
      ch->pcdata->haircolor = 3;
    }
  } else {
    switch (number_range(1, 3)) {
      default:
        act(AT_BLUE,
            "Your hair flashes blonde, but quickly returns to normal.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair flashes blonde, but quickly returns to normal.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 1:
        act(AT_BLUE,
            "Your hair flashes blonde, but quickly returns to normal.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair flashes blonde, but quickly returns to normal.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 2:
        act(AT_BLUE, "You, a super Saiyan?  I don't think so.",
            ch, NULL, NULL, TO_CHAR);
        break;
      case 3:
        act(AT_BLUE,
            "You almost pop a vein trying to transform in to a super Saiyan.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n almost pops a vein trying to transform in to a super Saiyan",
            ch, NULL, NULL, TO_NOTVICT);
        break;
    }
    learn_from_failure(ch, gsn_ssj);
  }

  ch->mana -= skill_table[gsn_ssj]->min_mana;
  return;
}

void do_ssj2(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ssj2))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You can't transform in to a super saiyan while using kaioken.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_USSJ) || xIS_SET((ch)->affected_by, AFF_USSJ2)) {
    send_to_char("You have to power out of USSJ to do this.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SSJ2)) {
    send_to_char("You power down to super Saiyan 1.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_SSJ2);

    if (xIS_SET((ch)->affected_by, AFF_SSJ3)) {
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);
    }
    if (xIS_SET((ch)->affected_by, AFF_SSJ4)) {
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
    }
    ch->pl = ch->exp * 50;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 8);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 0;
      ch->pcdata->haircolor = 3;
    }
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't transform while you are an Oozaru.\n\r",
                 ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_SSJ)) {
    if (ch->mana < skill_table[gsn_ssj2]->min_mana * 1.5) {
      send_to_char("You don't have enough energy.\n\r", ch);
      return;
    }
    WAIT_STATE(ch, skill_table[gsn_ssj2]->beats);
    if (can_use_skill(ch, number_percent(), gsn_ssj2)) {
      act(AT_YELLOW,
          "Your hair flashes blonde, standing up in a mass of spikes. "
          "Your eyes turn blue as a fiery aura bursts into existence around you, "
          "quickly becoming charged with sheets of electricity.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW,
          "$n's hair flashes blonde, standing up in a mass of spikes. "
          "$n's eyes turn blue as a fiery aura bursts into existence around $m, "
          "quickly becoming charged with sheets of electricity.",
          ch, NULL, NULL, TO_NOTVICT);
      xSET_BIT((ch)->affected_by, AFF_SSJ);
      xSET_BIT((ch)->affected_by, AFF_SSJ2);
      ch->pl = ch->exp * 225;
      int kistat = 0;
      kistat = (get_curr_int(ch) / 7);
      transStatApply(ch, kistat, kistat, kistat, kistat);
      learn_from_success(ch, gsn_ssj2);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 0;
        ch->pcdata->haircolor = 3;
      }
    } else {
      switch (number_range(1, 2)) {
        default:
          act(AT_BLUE,
              "Electricity begins arcing around your body, but it quickly disappears.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "Electricity begins arcing around $n's body, but it quickly disappears.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 1:
          act(AT_BLUE,
              "Your hair flashes blonde and starts to spike up, but quickly returns to normal.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair flashes blonde and starts to spike up, but quickly returns to normal.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 2:
          act(AT_BLUE,
              "You almost pop a vein trying to transform in to a super saiyan 2.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n almost pops a vein trying to transform in to a super saiyan 2.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
      }
      learn_from_failure(ch, gsn_ssj2);
    }

    ch->mana -= skill_table[gsn_ssj2]->min_mana * 1.5;
    return;
  }
  if (ch->mana < skill_table[gsn_ssj2]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_ssj2]->beats);
  if (can_use_skill(ch, number_percent(), gsn_ssj2)) {
    act(AT_YELLOW,
        "Your hair straightens and stands more on end, your aura flaring as "
        "electricity arcs around you.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_YELLOW,
        "$n's hair straightens and stands more on end, $s aura flaring as "
        "electricity arcs around $m.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_SSJ2);
    ch->pl = ch->exp * 225;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 7);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_ssj2);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 0;
      ch->pcdata->haircolor = 3;
    }
  } else {
    switch (number_range(1, 2)) {
      default:
        act(AT_BLUE,
            "Electricity begins arcing around your body, but it quickly disappears.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "Electricity begins arcing around $n's body, but it quickly disappears.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 1:
        act(AT_BLUE,
            "Electricity begins arcing around your body, but it quickly disappears.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "Electricity begins arcing around $n's body, but it quickly disappears.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 2:
        act(AT_BLUE,
            "You almost pop a vein trying to transform in to a super Saiyan 2.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n almost pops a vein trying to transform in to a super Saiyan 2.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
    }
    learn_from_failure(ch, gsn_ssj2);
  }

  ch->mana -= skill_table[gsn_ssj2]->min_mana;
  return;
}

void do_ssj3(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ssj3))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You can't transform in to a super saiyan while using kaioken.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_USSJ) || xIS_SET((ch)->affected_by, AFF_USSJ2)) {
    send_to_char("You have to power out of USSJ to do this.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SSJ3)) {
    send_to_char("You power down to super Saiyan 2.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_SSJ3);

    if (xIS_SET((ch)->affected_by, AFF_SSJ4)) {
      xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
    }
    ch->pl = ch->exp * 225;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 7);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 0;
      ch->pcdata->haircolor = 3;
    }
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't transform while you are an Oozaru.\n\r",
                 ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_SSJ2)) {
    if (ch->mana < skill_table[gsn_ssj3]->min_mana * 2) {
      send_to_char("You don't have enough energy.\n\r", ch);
      return;
    }
    WAIT_STATE(ch, skill_table[gsn_ssj3]->beats);
    if (can_use_skill(ch, number_percent(), gsn_ssj3)) {
      act(AT_YELLOW,
          "A brilliant golden aura explodes around you as your hair suddenly flashes a similar color. "
          "It flows rapidly down your back as your eyebrows disappear.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_YELLOW,
          "A brilliant golden aura explodes around $n as $s hair suddenly flashes a similar color. "
          "It flows rapidly down $s back as $s eyebrows disappear.",
          ch, NULL, NULL, TO_NOTVICT);
      xSET_BIT((ch)->affected_by, AFF_SSJ);
      xSET_BIT((ch)->affected_by, AFF_SSJ2);
      xSET_BIT((ch)->affected_by, AFF_SSJ3);
      ch->pl = ch->exp * 325;
      int kistat = 0;
      kistat = (get_curr_int(ch) / 6);
      transStatApply(ch, kistat, kistat, kistat, kistat);
      learn_from_success(ch, gsn_ssj3);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 0;
        ch->pcdata->haircolor = 3;
      }
    } else {
      switch (number_range(1, 3)) {
        default:
          act(AT_BLUE,
              "Your hair begins to crawl down your back, but it quickly returns.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair begins to crawl down $s back, but it quickly returns.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 1:
          act(AT_BLUE,
              "Your hair begins to crawl down your back, but it quickly returns.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair begins to crawl down $s back, but it quickly returns.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 2:
          act(AT_BLUE,
              "You almost pop a vein trying to transform in to a super saiyan 3.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n almost pops a vein trying to transform in to a super saiyan 3.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 3:
          act(AT_BLUE,
              "Your hair starts to lengthen and change color, but quickly returns to normal.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair starts to lengthen and change color, but quickly returns to normal.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
      }
      learn_from_failure(ch, gsn_ssj3);
    }

    ch->mana -= skill_table[gsn_ssj3]->min_mana * 2;
    return;
  }
  if (ch->mana < skill_table[gsn_ssj3]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_ssj3]->beats);
  if (can_use_skill(ch, number_percent(), gsn_ssj3)) {
    act(AT_YELLOW,
        "Your hair lengthens, flowing down your back.  Your eyebrows disappear as your "
        "aura flashes a bright gold.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_YELLOW,
        "$n's hair lengthens, flowing down $s back.  $n's eyebrows disappear as $s "
        "aura flashes a bright gold.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_SSJ3);
    ch->pl = ch->exp * 325;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 6);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_ssj3);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 0;
      ch->pcdata->haircolor = 3;
    }
  } else {
    switch (number_range(1, 3)) {
      default:
        act(AT_BLUE,
            "Your hair begins to crawl down your back, but it quickly returns.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair begins to crawl down $s back, but it quickly returns.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 1:
        act(AT_BLUE,
            "Your hair begins to crawl down your back, but it quickly returns.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair begins to crawl down $s back, but it quickly returns.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 2:
        act(AT_BLUE,
            "You almost pop a vein trying to transform in to a super Saiyan 3.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n almost pops a vein trying to transform in to a super Saiyan 3.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 3:
        act(AT_BLUE,
            "Your aura begins to flare up, but it quickly shrinks down again.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's aura begins to flare up, but it quickly shrinks down again.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
    }
    learn_from_failure(ch, gsn_ssj3);
  }

  ch->mana -= skill_table[gsn_ssj3]->min_mana;
  return;
}

void do_ssj4(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_sgod))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (!IS_NPC(ch)) {
    if (ch->pcdata->tail < 1) {
      ch_printf(ch,
                "You can't become a Super Saiyan 4 without a tail!\n\r");
      return;
    }
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You can't transform in to a super saiyan while using kaioken.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_USSJ) || xIS_SET((ch)->affected_by, AFF_USSJ2)) {
    send_to_char("You have to power out of USSJ to do this.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SSJ4)) {
    send_to_char("You power down to super Saiyan 3.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_SSJ4);
    ch->pl = ch->exp * 325;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 7);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 0;
      ch->pcdata->haircolor = 3;
    }
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't transform while you are an Oozaru.\n\r",
                 ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_SSJ3)) {
    if (ch->mana < skill_table[gsn_ssj4]->min_mana * 3) {
      send_to_char("You don't have enough energy.\n\r", ch);
      return;
    }
    WAIT_STATE(ch, skill_table[gsn_ssj4]->beats);
    if (can_use_skill(ch, number_percent(), gsn_ssj4)) {
      act(AT_RED,
          "You howl with anger as a menacing, fiery, red aura explodes around you. "
          "Thick red fur sprouts all over your body as your hair turns a darker variant of normal.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "$n howls with anger as a menacing, fiery, red aura explodes around $m. "
          "Thick red fur sprouts all over $s body as $s hair turns a darker variant of normal.",
          ch, NULL, NULL, TO_NOTVICT);
      xSET_BIT((ch)->affected_by, AFF_SSJ);
      xSET_BIT((ch)->affected_by, AFF_SSJ2);
      xSET_BIT((ch)->affected_by, AFF_SSJ3);
      xSET_BIT((ch)->affected_by, AFF_SSJ4);
      ch->pl = ch->exp * 425;
      int kistat = 0;
      kistat = (get_curr_int(ch) / 6);
      transStatApply(ch, kistat, kistat, kistat, kistat);
      learn_from_success(ch, gsn_ssj4);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 4;
        ch->pcdata->haircolor = 9;
      }
    } else {
      switch (number_range(1, 4)) {
        default:
          act(AT_BLUE,
              "Your hair flashes black, but quickly returns to blonde.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair flashes black, but quickly returns to blonde.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 1:
          act(AT_BLUE,
              "Your hair flashes black, but quickly returns to blonde.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair flashes black, but quickly returns to blonde.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 2:
          act(AT_BLUE,
              "You almost pop a vein trying to transform in to a super Saiyan 4.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n almost pops a vein trying to transform in to a super Saiyan 4.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 3:
          act(AT_BLUE,
              "Red fur begins to sprout all over your body, but quickly disappears.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "Red fur begins to sprout all over $n's body, but quickly disappears.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 4:
          act(AT_BLUE,
              "Your aura flashes a fiery red, but quickly goes back to gold.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's aura flashes a fiery red, but quickly goes back to gold.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
      }
      learn_from_failure(ch, gsn_ssj4);
    }

    ch->mana -= skill_table[gsn_ssj4]->min_mana * 3;
    return;
  }
  if (ch->mana < skill_table[gsn_ssj4]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_ssj4]->beats);
  if (can_use_skill(ch, number_percent(), gsn_ssj4)) {
    act(AT_RED,
        "You howl with anger, your golden aura exploding and reforming as a fiery red.  "
        "Your hair returns to its original shade and form as thick red fur begins "
        "to sprout all over your body.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_RED,
        "$n howls with anger, $s golden aura exploding and reforming as a fiery red.  "
        "$n's hair returns to its original shade and form as thick red fur begins "
        "to sprout all over $s body.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_SSJ4);
    ch->pl = ch->exp * 425;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 6);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_ssj4);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 4;
      ch->pcdata->haircolor = 9;
    }
  } else {
    switch (number_range(1, 4)) {
      default:
        act(AT_BLUE,
            "Your hair flashes black, but quickly returns to blonde.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair flashes black, but quickly returns to blonde.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 1:
        act(AT_BLUE,
            "Your hair flashes black, but quickly returns to blonde.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair flashes black, but quickly returns to blonde.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 2:
        act(AT_BLUE,
            "You almost pop a vein trying to transform in to a super Saiyan 4.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n almost pops a vein trying to transform in to a super Saiyan 4.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 3:
        act(AT_BLUE,
            "Red fur begins to sprout all over your body, but quickly disappears.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "Red fur begins to sprout all over $n's body, but quickly disappears.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 4:
        act(AT_BLUE,
            "Your aura flashes a fiery red, but quickly goes back to gold.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's aura flashes a fiery red, but quickly goes back to gold.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
    }
    learn_from_failure(ch, gsn_ssj4);
  }

  ch->mana -= skill_table[gsn_ssj4]->min_mana;
  return;
}

void do_sgod(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ssj4))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You can't transform in to a Saiyan God while using Kaioken.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_USSJ) || xIS_SET((ch)->affected_by, AFF_USSJ2)) {
    send_to_char("You have to power out of USSJ to do this.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SGOD)) {
    send_to_char("You power down to super Saiyan 4.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_SGOD);
    ch->pl = ch->exp * 425;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 6);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 4;
      ch->pcdata->haircolor = 9;
    }
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    send_to_char("You can't transform while you are an Oozaru.\n\r",
                 ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_SSJ4)) {
    if (ch->mana < skill_table[gsn_sgod]->min_mana * 4) {
      send_to_char("You don't have enough energy.\n\r", ch);
      return;
    }
    WAIT_STATE(ch, skill_table[gsn_sgod]->beats);
    if (can_use_skill(ch, number_percent(), gsn_sgod)) {
      act(AT_PURPLE,
          "You shimmer in a gold light. "
          "Your hair flashes purple.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_PURPLE,
          "$n shimmers in a gold light. "
          "$ hair flashes purple.",
          ch, NULL, NULL, TO_NOTVICT);
      xSET_BIT((ch)->affected_by, AFF_SSJ);
      xSET_BIT((ch)->affected_by, AFF_SSJ2);
      xSET_BIT((ch)->affected_by, AFF_SSJ3);
      xSET_BIT((ch)->affected_by, AFF_SSJ4);
      xSET_BIT((ch)->affected_by, AFF_SGOD);
      ch->pl = ch->exp * 500;
      int kistat = 0;
      kistat = (get_curr_int(ch) / 5);
      transStatApply(ch, kistat, kistat, kistat, kistat);
      learn_from_success(ch, gsn_sgod);
      if (!IS_NPC(ch)) {
        ch->pcdata->eyes = 4;
        ch->pcdata->haircolor = 8;
      }
    } else {
      switch (number_range(1, 4)) {
        default:
          act(AT_BLUE,
              "Your hair flashes purple, but quickly returns to black.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair flashes purple, but quickly returns to black.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 1:
          act(AT_BLUE,
              "Your hair flashes purple, but quickly returns to black.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's hair flashes purple, but quickly returns to black.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 2:
          act(AT_BLUE,
              "You almost transform in to a super Saiyan God, your golden aura glows brightly",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n almost transforms in to a super Saiyan God, $ns golden aura glows brightly.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 3:
          act(AT_BLUE,
              "Your eyes flash purple, your aura grows.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's eyes flash purple, his aura grows.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
        case 4:
          act(AT_BLUE,
              "Your golden aura begins to pulse.",
              ch, NULL, NULL, TO_CHAR);
          act(AT_BLUE,
              "$n's golden aura begins to pulse.",
              ch, NULL, NULL, TO_NOTVICT);
          break;
      }
      learn_from_failure(ch, gsn_sgod);
    }

    ch->mana -= skill_table[gsn_sgod]->min_mana * 4;
    return;
  }
  if (ch->mana < skill_table[gsn_sgod]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_sgod]->beats);
  if (can_use_skill(ch, number_percent(), gsn_sgod)) {
    act(AT_WHITE,
        "Your aura envelopes you, your senses highten and you lose touch with humanity as you ascend.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_WHITE,
        "$n's aura envelopes everything, you're temporarily blinded and can no longer sense his power.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_SGOD);
    ch->pl = ch->exp * 500;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 5);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_sgod);
    if (!IS_NPC(ch)) {
      ch->pcdata->eyes = 4;
      ch->pcdata->haircolor = 8;
    }
  } else {
    switch (number_range(1, 4)) {
      default:
        act(AT_BLUE,
            "Your hair flashes purple, but quickly returns to black.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair flashes purple, but quickly returns to black.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 1:
        act(AT_BLUE,
            "Your hair flashes purple, but quickly returns to black.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's hair flashes purple, but quickly returns to black.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 2:
        act(AT_BLUE,
            "You almost transform in to a super Saiyan God, your golden aura glows brightly",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n almost transforms in to a super Saiyan God, $ns golden aura glows brightly.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 3:
        act(AT_BLUE,
            "Your eyes flash purple, your aura grows.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's eyes flash purple, his aura grows.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
      case 4:
        act(AT_BLUE,
            "Your golden aura begins to pulse.",
            ch, NULL, NULL, TO_CHAR);
        act(AT_BLUE,
            "$n's golden aura begins to pulse.",
            ch, NULL, NULL, TO_NOTVICT);
        break;
    }
    learn_from_failure(ch, gsn_sgod);
  }

  ch->mana -= skill_table[gsn_sgod]->min_mana;
  return;
}

void do_super_namek(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_snamek))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_int(ch) < 1000) {
    send_to_char("You feel that you lack enough control over your ki to use this technique.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  int kicontrol = 0;
  double pl_mult = 0;
  int arg;
  bool affGrowth = false;
  int sizeStr = 0, sizeSpd = 0, sizeCon = 0, sizeInt = 0;

  kicontrol = get_curr_int(ch);
  arg = atoi(argument);

  if (xIS_SET((ch)->affected_by, AFF_GROWTH)) {
    affGrowth = true;
    sizeStr = 10;
    sizeCon = 5;
    sizeInt = 5;
    sizeSpd = -10;
  }
  if (xIS_SET((ch)->affected_by, AFF_GIANT)) {
    affGrowth = true;
    sizeStr = 25;
    sizeCon = 15;
    sizeInt = 10;
    sizeSpd = -25;
  }
  if (xIS_SET((ch)->affected_by, AFF_SNAMEK) && arg == 0) {
    send_to_char("You power down and return to normal.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_SNAMEK);
    transStatRemove(ch);
    ch->pl = ch->exp;
    if (affGrowth) {
      transStatApply(ch, sizeStr, sizeSpd, sizeInt, sizeCon);
    }
    return;
  } else if (!xIS_SET((ch)->affected_by, AFF_SNAMEK) && arg == 0) {
    ch_printf(ch, "You're not in super namek, though!\n\r");
    return;
  }
  pl_mult = (double)kicontrol * 0.02;
  if (pl_mult > 500)
    pl_mult = 500;

  if (arg > pl_mult)
    pl_mult = (int)pl_mult;
  else if (arg < 1)
    pl_mult = 1;
  else
    pl_mult = arg;

  if (ch->mana < skill_table[gsn_snamek]->min_mana * pl_mult) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_snamek]->beats);
  if (can_use_skill(ch, number_percent(), gsn_snamek)) {
    act(AT_SKILL,
        "You draw upon the ancient knowledge of the Namekians to transform into a Super Namek.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n draws upon the ancient knowledge of the Namekians to transform into a Super Namek.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_SNAMEK);

    ch->pl = ch->exp * pl_mult;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 5);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_snamek);
  } else {
    act(AT_SKILL,
        "You can not quite comprehend the ancient knowledge necessary to become a Super Namek.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n can not quite comprehend the ancient knowledge necessary to become a Super Namek.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_snamek);
  }

  ch->mana -= skill_table[gsn_snamek]->min_mana * pl_mult;
  return;
}

void do_icer_transform_2(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_icer2))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_ICER2) || xIS_SET((ch)->affected_by, AFF_ICER3) || xIS_SET((ch)->affected_by, AFF_ICER4) || xIS_SET((ch)->affected_by, AFF_ICER5)) {
    send_to_char("You power down and transform into your first form.\n\r",
                 ch);
    if (xIS_SET((ch)->affected_by, AFF_ICER2))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    if (xIS_SET((ch)->affected_by, AFF_ICER3))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    if (xIS_SET((ch)->affected_by, AFF_ICER4))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    if (xIS_SET((ch)->affected_by, AFF_ICER5))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    ch->pl = ch->exp;
    transStatRemove(ch);
    return;
  }
  if (ch->mana < skill_table[gsn_icer2]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_icer2]->beats);
  if (can_use_skill(ch, number_percent(), gsn_icer2)) {
    act(AT_PURPLE,
        "A dread chill emanates from you as your form grows huge in size, your muscles bulge and your horns"
        " become much more prominent, completing your transformation to your second form.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "A dread chill emanates from $n as $s form grows huge in size, $s muscles bulge and $s horns"
        " become much more prominent, completing $s transformation to $s second form.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_ICER2);
    ch->pl = ch->exp * 4;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 9);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_icer2);
  } else {
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as you are unable to transform.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as $n is unable to transform.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_icer2);
  }
  if (xIS_SET((ch)->affected_by, AFF_ICER3))
    xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
  if (xIS_SET((ch)->affected_by, AFF_ICER4))
    xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
  if (xIS_SET((ch)->affected_by, AFF_ICER5))
    xREMOVE_BIT((ch)->affected_by, AFF_ICER5);

  ch->mana -= skill_table[gsn_icer2]->min_mana;
  return;
}

void do_icer_transform_3(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_icer3))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_con(ch) < 200) {
    send_to_char("Your body lacks the necessary toughness to withstand your transformation.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_ICER3) || xIS_SET((ch)->affected_by, AFF_ICER4) || xIS_SET((ch)->affected_by, AFF_ICER5)) {
    send_to_char("You power down and transform into your second form.\n\r",
                 ch);
    if (xIS_SET((ch)->affected_by, AFF_ICER3))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    if (xIS_SET((ch)->affected_by, AFF_ICER4))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    if (xIS_SET((ch)->affected_by, AFF_ICER5))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    if (!xIS_SET((ch)->affected_by, AFF_ICER2))
      xSET_BIT((ch)->affected_by, AFF_ICER2);
    ch->pl = ch->exp * 4;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 9);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    return;
  }
  if (ch->mana < skill_table[gsn_icer3]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_ICER2)) {
    send_to_char("You must be in second form first.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_icer3]->beats);
  if (can_use_skill(ch, number_percent(), gsn_icer3)) {
    act(AT_PURPLE,
        "A cold breeze swirls out from your form as you shrink slightly, your head elongates and your"
        " mouth and nose form into a sort of beak.  Your horns shrink, leaving you looking like quite"
        " a monster in your third form.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "A cold breeze swirls out from $n's form as $e shrinks slightly, $s head elongates and $s"
        " mouth and nose form into a sort of beak.  $n's horns shrink, leaving $m looking like quite"
        " a monster in $s third form.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_ICER3);
    xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    ch->pl = ch->exp * 12;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 8);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_icer3);
  } else {
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as you are unable to transform.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as $n is unable to transform.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_icer3);
  }

  if (xIS_SET((ch)->affected_by, AFF_ICER4))
    xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
  if (xIS_SET((ch)->affected_by, AFF_ICER5))
    xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
  ch->mana -= skill_table[gsn_icer3]->min_mana;
  return;
}

void do_icer_transform_4(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_icer4))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_con(ch) < 1250) {
    send_to_char("Your body lacks the necessary toughness to withstand your transformation.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_ICER4) || xIS_SET((ch)->affected_by, AFF_ICER5)) {
    send_to_char("You power down and transform into your third form.\n\r",
                 ch);
    if (xIS_SET((ch)->affected_by, AFF_ICER2))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    if (xIS_SET((ch)->affected_by, AFF_ICER4))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    if (xIS_SET((ch)->affected_by, AFF_ICER5))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    if (!xIS_SET((ch)->affected_by, AFF_ICER3))
      xSET_BIT((ch)->affected_by, AFF_ICER3);
    ch->pl = ch->exp * 12;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 8);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    return;
  }
  if (ch->mana < skill_table[gsn_icer4]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_ICER3)) {
    send_to_char("You must be in third form first.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_icer4]->beats);
  if (can_use_skill(ch, number_percent(), gsn_icer4)) {
    act(AT_PURPLE,
        "With a sudden gust of freezing air your body reverts to a more normal shape and size.  "
        "Your horns disappear completely now, your skin and features turning as smooth as glass, "
        "leaving you looking rather sleek in your fourth form.",
        ch,
        NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "With a sudden gust of freezing air $n's body reverts to a more normal shape and size.  "
        "$n's horns disappear completely now, $s skin and features turning as smooth as glass, "
        "leaving $m looking rather sleek in $s fourth form.",
        ch,
        NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_ICER4);
    xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    ch->pl = ch->exp * 50;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 7);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_icer4);
  } else {
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as you are unable to transform.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as $n is unable to transform.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_icer4);
  }

  if (xIS_SET((ch)->affected_by, AFF_ICER5))
    xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
  ch->mana -= skill_table[gsn_icer4]->min_mana;
  return;
}

void do_icer_transform_5(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_icer5))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_con(ch) < 2000) {
    send_to_char("Your body lacks the necessary toughness to withstand your transformation.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_ICER5)) {
    send_to_char("You power down and transform into your fourth form.\n\r",
                 ch);
    if (xIS_SET((ch)->affected_by, AFF_ICER2))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    if (xIS_SET((ch)->affected_by, AFF_ICER3))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    if (xIS_SET((ch)->affected_by, AFF_ICER5))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    if (!xIS_SET((ch)->affected_by, AFF_ICER4))
      xSET_BIT((ch)->affected_by, AFF_ICER4);
    ch->pl = ch->exp * 50;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 7);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    return;
  }
  if (ch->mana < skill_table[gsn_icer5]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_ICER4)) {
    send_to_char("You must be in fourth form first.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_icer5]->beats);
  if (can_use_skill(ch, number_percent(), gsn_icer5)) {
    act(AT_PURPLE,
        "An arctic tempest roars as your body grows huge, your muscles bulging.  Armored plates "
        "slide over weak points, spikes bristling over your body for extra protection.  An armored "
        "mask slides over your face, ending your transformation to your fifth form.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "An arctic tempest roars as $n's body grows huge, $s muscles bulging.  Armored plates "
        "slide over weak points, spikes bristling over $s body for extra protection.  An armored "
        "mask slides over $n's face, ending $s transformation to $s fifth form.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_ICER5);
    xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    ch->pl = ch->exp * 150;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 6);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_icer5);
  } else {
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as you are unable to transform.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as $n is unable to transform.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_icer5);
  }

  ch->mana -= skill_table[gsn_icer5]->min_mana;
  return;
}

void do_icer_transform_golden_form(CHAR_DATA *ch, char *argument) {
  if (IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  double pl_mult = 0;
  int kicontrol = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_goldenform))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_str(ch) < 1500) {
    send_to_char("Your body is tough and durable, but that alone is not enough to achieve true power.\n\r", ch);
    return;
  }
  if (get_curr_int(ch) < 1500) {
    send_to_char("Your body is tough and durable, but that alone is not enough to achieve true power.\n\r", ch);
    return;
  }
  if (get_curr_dex(ch) < 1500) {
    send_to_char("Your body is tough and durable, but that alone is not enough to achieve true power.\n\r", ch);
    return;
  }
  if (get_curr_con(ch) < 2000) {
    send_to_char("Your body is tough and durable, but that alone is not enough to achieve true power.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_GOLDENFORM)) {
    send_to_char("You power down and transform into your fifth form.\n\r",
                 ch);
    if (xIS_SET((ch)->affected_by, AFF_ICER2))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER2);
    if (xIS_SET((ch)->affected_by, AFF_ICER3))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER3);
    if (xIS_SET((ch)->affected_by, AFF_ICER4))
      xREMOVE_BIT((ch)->affected_by, AFF_ICER4);
    if (xIS_SET((ch)->affected_by, AFF_GOLDENFORM))
      xREMOVE_BIT((ch)->affected_by, AFF_GOLDENFORM);
    if (!xIS_SET((ch)->affected_by, AFF_ICER5))
      xSET_BIT((ch)->affected_by, AFF_ICER5);
    ch->pl = ch->exp * 150;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 6);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    return;
  }
  if (ch->mana < skill_table[gsn_goldenform]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_ICER5)) {
    send_to_char("You must be in fifth form first.\n\r", ch);
    return;
  }

  kicontrol = get_curr_int(ch);

  WAIT_STATE(ch, skill_table[gsn_goldenform]->beats);
  if (can_use_skill(ch, number_percent(), gsn_goldenform)) {
    act(AT_PURPLE,
        "An arctic blast surrounds you, the ground freezes and your golden aura shimmers.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "An arctic blast hits you, the ground freezes underneath you, you shiver in fear.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_GOLDENFORM);
    xREMOVE_BIT((ch)->affected_by, AFF_ICER5);
    pl_mult = (double)kicontrol / 100 + 380;
    ch->pl = ch->exp * pl_mult;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 5);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_goldenform);
  } else {
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as you are unable to transform.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_PURPLE,
        "The temperature dips a few degrees, but quickly returns to normal, as $n is unable to transform.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_goldenform);
  }

  ch->mana -= skill_table[gsn_goldenform]->min_mana;
  return;
}

void do_fly(CHAR_DATA *ch, char *argument) {
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_fly))
      return;
  }
  if (xIS_SET((ch)->affected_by, AFF_FLYING)) {
    send_to_char("You float down and land on the ground.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_FLYING);
    return;
  }
  if (ch->mana < skill_table[gsn_fly]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_fly]->beats);
  if (can_use_skill(ch, number_percent(), gsn_fly)) {
    act(AT_SKILL, "You rise up into the currents of air....", ch,
        NULL, NULL, TO_CHAR);
    act(AT_SKILL, "$n rises up into the currents of air....", ch,
        NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_FLYING);
    learn_from_success(ch, gsn_fly);
  } else {
    act(AT_SKILL,
        "You try to fly but you fall back down to the earth.", ch,
        NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n tries to fly but falls back down to the earth.", ch,
        NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_fly);
  }

  if (!is_android_h(ch))
    ch->mana -= skill_table[gsn_fly]->min_mana;

  return;
}

void do_respec(CHAR_DATA *ch, char *argument) {
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);

  if (IS_NPC(ch)) {
    send_to_char("Stop that!\n\r", ch);
    return;
  }

  if (arg[0] == '\0') {
    send_to_char("This command will reset ALL currently allocated SP at no cost.\n\r", ch);
    send_to_char("If you're certain you want to do this, type 'respec now'.\n\r", ch);
    return;
  }
  if (!str_cmp(arg, "now")) {
    ch->spallocated = 0;
    ch->energy_ballpower = 0;
    ch->energy_balleffic = 0;
    ch->crusherballpower = 0;
    ch->crusherballeffic = 0;
    ch->meteorpower = 0;
    ch->meteoreffic = 0;
    ch->gigantic_meteorpower = 0;
    ch->gigantic_meteoreffic = 0;
    ch->ecliptic_meteorpower = 0;
    ch->ecliptic_meteoreffic = 0;
    ch->death_ballpower = 0;
    ch->death_balleffic = 0;
    ch->energybeampower = 0;
    ch->energybeameffic = 0;
    ch->eye_beampower = 0;
    ch->eye_beameffic = 0;
    ch->masenkopower = 0;
    ch->masenkoeffic = 0;
    ch->makosenpower = 0;
    ch->makoseneffic = 0;
    ch->sbcpower = 0;
    ch->sbceffic = 0;
    ch->concentrated_beampower = 0;
    ch->concentrated_beameffic = 0;
    ch->kamehamehapower = 0;
    ch->kamehamehaeffic = 0;
    ch->gallic_gunpower = 0;
    ch->gallic_guneffic = 0;
    ch->finger_beampower = 0;
    ch->finger_beameffic = 0;
    ch->destructo_discpower = 0;
    ch->destructo_disceffic = 0;
    ch->destructive_wavepower = 0;
    ch->destructive_waveeffic = 0;
    ch->forcewavepower = 0;
    ch->forcewaveeffic = 0;
    ch->shockwavepower = 0;
    ch->shockwaveeffic = 0;
    ch->energy_discpower = 0;
    ch->energy_disceffic = 0;
    ch->punchpower = 0;
    ch->puncheffic = 0;
    ch->haymakerpower = 0;
    ch->haymakereffic = 0;
    ch->bashpower = 0;
    ch->basheffic = 0;
    ch->collidepower = 0;
    ch->collideeffic = 0;
    ch->lariatpower = 0;
    ch->lariateffic = 0;
    ch->vigoreffec = 0;

    ch->skilleye_beam = 0;
    ch->skilldestructive_wave = 0;
    ch->skillbash = 0;
    ch->skillhaymaker = 0;
    ch->skillcollide = 0;
    ch->skilllariat = 0;
    ch->skillultra_instinct = 0;
    ch->skillbbk = 0;
    ch->skillburning_attack = 0;
    ch->skillconcentrated_beam = 0;
    ch->skillblast_zone = 0;
    ch->skillcrusherball = 0;
    ch->skilldeath_ball = 0;
    ch->skilldestructo_disc = 0;
    ch->skillecliptic_meteor = 0;
    ch->skillenergy_beam = 0;
    ch->skillenergy_disc = 0;
    ch->skillfinal_flash = 0;
    ch->skillfinger_beam = 0;
    ch->skillforcewave = 0;
    ch->skillgallic_gun = 0;
    ch->skillfinishing_buster = 0;
    ch->skillgigantic_meteor = 0;
    ch->skillheaven_splitter = 0;
    ch->skillhells_flash = 0;
    ch->skillhellzone_grenade = 0;
    ch->skillinstant_trans = 0;
    ch->skillkaio_create = 0;
    ch->skillkaioken = 0;
    ch->skillkamehameha = 0;
    ch->skillki_absorb = 0;
    ch->skillki_heal = 0;
    ch->skillmakosen = 0;
    ch->skillmasenko = 0;
    ch->skillmeteor = 0;
    ch->skillmultidisc = 0;
    ch->skillmulti_eye_beam = 0;
    ch->skillmonkey_cannon = 0;
    ch->skillpsionic_blast = 0;
    ch->skillmulti_finger_beam = 0;
    ch->skillshockwave = 0;
    ch->skillsbc = 0;
    ch->skillspirit_ball = 0;
    ch->skillspirit_bomb = 0;
    send_to_char("Done.\n\r", ch);
    return;
  }
}

void do_research(CHAR_DATA *ch, char *argument) {
  char arg1[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  int spremaining = 0;
  int spcostpow = 0;
  int spcosteff = 0;

  argument = one_argument(argument, arg1);
  argument = one_argument(argument, arg2);

  if (IS_NPC(ch)) {
    send_to_char("Stop that!\n\r", ch);
    return;
  }

  spremaining = (ch->sptotal - ch->spallocated);
  if (arg1[0] == '\0' && arg2[0] == '\0') {
    pager_printf_color(ch, "Research what? You currently have %d Skill Points\n\r", spremaining);
    send_to_char("syntax: research <skill> <field>\n\r", ch);
    send_to_char("\n\r", ch);
    send_to_char("Field being one of:\n\r", ch);
    send_to_char("   power   efficiency\n\r", ch);
    return;
  }
  if (arg2[0] == '\0') {
    pager_printf_color(ch, "Research what? You currently have %d Skill Points\n\r", spremaining);
    send_to_char("syntax: research <skill> <field>\n\r", ch);
    send_to_char("\n\r", ch);
    send_to_char("Field being one of:\n\r", ch);
    send_to_char("   power   efficiency\n\r", ch);
    return;
  }
  if (!str_cmp(arg1, "vigor")) {
    if (ch->skillenergy_ball < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->vigoreffec * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("Your Vigor overflows!\n\r", ch);
        ch->vigoreffec += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      send_to_char("You may only increase the 'power' of Vigor.\n\r", ch);
      return;
    }
  }
  if (!str_cmp(arg1, "energyball")) {
    if (ch->skillenergy_ball < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->energy_ballpower * 5);
      if (spcostpow < 5)
        spcostpow = 5;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Energy Ball has increased!\n\r", ch);
        ch->energy_ballpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->energy_balleffic * 5);
      if (spcosteff < 5)
        spcosteff = 5;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Energy Ball has increased!\n\r", ch);
        ch->energy_balleffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "crusherball")) {
    if (ch->skillcrusherball < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->crusherballpower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Crusher Ball has increased!\n\r", ch);
        ch->crusherballpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->crusherballeffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Crusher Ball has increased!\n\r", ch);
        ch->crusherballeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "meteor")) {
    if (ch->skillmeteor < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->meteorpower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Meteor has increased!\n\r", ch);
        ch->meteorpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->meteoreffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Meteor has increased!\n\r", ch);
        ch->meteoreffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "giganticmeteor")) {
    if (ch->skillgigantic_meteor < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->gigantic_meteorpower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Gigantic Meteor has increased!\n\r", ch);
        ch->gigantic_meteorpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->gigantic_meteoreffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Gigantic Meteor has increased!\n\r", ch);
        ch->gigantic_meteoreffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "eclipticmeteor")) {
    if (ch->skillecliptic_meteor < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->ecliptic_meteorpower * 250);
      if (spcostpow < 250)
        spcostpow = 250;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Ecliptic Meteor has increased!\n\r", ch);
        ch->ecliptic_meteorpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->ecliptic_meteoreffic * 250);
      if (spcosteff < 250)
        spcosteff = 250;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Ecliptic Meteor has increased!\n\r", ch);
        ch->ecliptic_meteoreffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "deathball")) {
    if (ch->skilldeath_ball < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->death_ballpower * 250);
      if (spcostpow < 250)
        spcostpow = 250;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Death Ball has increased!\n\r", ch);
        ch->death_ballpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->death_balleffic * 250);
      if (spcosteff < 250)
        spcosteff = 250;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Death Ball has increased!\n\r", ch);
        ch->death_balleffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "energybeam")) {
    if (ch->skillenergy_beam < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->energybeampower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Energy Beam has increased!\n\r", ch);
        ch->energybeampower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->energybeameffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Energy Beam has increased!\n\r", ch);
        ch->energybeameffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "concentratedbeam")) {
    if (ch->skillconcentrated_beam < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->concentrated_beampower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Concentrated Beam has increased!\n\r", ch);
        ch->concentrated_beampower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->concentrated_beameffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Concentrated Beam has increased!\n\r", ch);
        ch->concentrated_beameffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "punch")) {
    if (ch->skillpunch < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->punchpower * 5);
      if (spcostpow < 5)
        spcostpow = 5;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Punch has increased!\n\r", ch);
        ch->punchpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->puncheffic * 5);
      if (spcosteff < 5)
        spcosteff = 5;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Punch has increased!\n\r", ch);
        ch->puncheffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "haymaker")) {
    if (ch->skillhaymaker < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->haymakerpower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Haymaker has increased!\n\r", ch);
        ch->haymakerpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->haymakereffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Haymaker has increased!\n\r", ch);
        ch->haymakereffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "bash")) {
    if (ch->skillbash < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->bashpower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Bash has increased!\n\r", ch);
        ch->bashpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->basheffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Bash has increased!\n\r", ch);
        ch->basheffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "collide")) {
    if (ch->skillcollide < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->collidepower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Collide has increased!\n\r", ch);
        ch->collidepower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->collideeffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Collide has increased!\n\r", ch);
        ch->collideeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "lariat")) {
    if (ch->skilllariat < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->lariatpower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Lariat has increased!\n\r", ch);
        ch->lariatpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->lariateffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Lariat has increased!\n\r", ch);
        ch->lariateffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "eyebeam")) {
    if (ch->skilleye_beam < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->eye_beampower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Eyebeam has increased!\n\r", ch);
        ch->eye_beampower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->eye_beameffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Eyebeam has increased!\n\r", ch);
        ch->eye_beameffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "energydisc")) {
    if (ch->skillenergy_disc < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->energy_discpower * 100);
      if (spcostpow < 100)
        spcostpow = 100;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Energy Disc has increased!\n\r", ch);
        ch->energy_discpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->energy_disceffic * 100);
      if (spcosteff < 100)
        spcosteff = 100;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Energy Disc has increased!\n\r", ch);
        ch->energy_disceffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "destructivewave")) {
    if (ch->skilldestructive_wave < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->destructive_wavepower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Destructive Wave has increased!\n\r", ch);
        ch->destructive_wavepower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->destructive_waveeffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Destructive Wave has increased!\n\r", ch);
        ch->destructive_waveeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "forcewave")) {
    if (ch->skillforcewave < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->forcewavepower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Forcewave has increased!\n\r", ch);
        ch->forcewavepower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->forcewaveeffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Forcewave has increased!\n\r", ch);
        ch->forcewaveeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "destructodisc")) {
    if (ch->skilldestructo_disc < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->destructo_discpower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Destructo Disc has increased!\n\r", ch);
        ch->destructo_discpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->destructo_disceffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Destructo Disc has increased!\n\r", ch);
        ch->destructo_disceffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "masenko")) {
    if (ch->skillmasenko < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->masenkopower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Masenko has increased!\n\r", ch);
        ch->masenkopower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->masenkoeffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Masenko has increased!\n\r", ch);
        ch->masenkoeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "makosen")) {
    if (ch->skillmakosen < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->makosenpower * 150);
      if (spcostpow < 150)
        spcostpow = 150;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Makosen has increased!\n\r", ch);
        ch->makosenpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->makoseneffic * 150);
      if (spcosteff < 150)
        spcosteff = 150;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Makosen has increased!\n\r", ch);
        ch->makoseneffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "sbc")) {
    if (ch->skillsbc < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->sbcpower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Special Beam Cannon has increased!\n\r", ch);
        ch->sbcpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->sbceffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Special Beam Cannon has increased!\n\r", ch);
        ch->sbceffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "fingerbeam")) {
    if (ch->skillfinger_beam < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->finger_beampower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Finger Beam has increased!\n\r", ch);
        ch->finger_beampower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->finger_beameffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Finger Beam has increased!\n\r", ch);
        ch->finger_beameffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "kamehameha")) {
    if (ch->skillkamehameha < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->kamehamehapower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Kamehameha has increased!\n\r", ch);
        ch->kamehamehapower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->kamehamehaeffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Kamehameha has increased!\n\r", ch);
        ch->kamehamehaeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "shockwave")) {
    if (ch->skillshockwave < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->shockwavepower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Shockwave has increased!\n\r", ch);
        ch->shockwavepower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->shockwaveeffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Shockwave has increased!\n\r", ch);
        ch->shockwaveeffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  if (!str_cmp(arg1, "gallicgun")) {
    if (ch->skillgallic_gun < 1) {
      send_to_char("You can't research a skill you don't even know!\n\r", ch);
      return;
    }
    if (!str_cmp(arg2, "power")) {
      spcostpow = (ch->gallic_gunpower * 200);
      if (spcostpow < 200)
        spcostpow = 200;
      if (spremaining < spcostpow) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The power of your Gallic Gun has increased!\n\r", ch);
        ch->gallic_gunpower += 1;
        ch->spallocated += spcostpow;
        return;
      }
    }
    if (!str_cmp(arg2, "efficiency")) {
      spcosteff = (ch->gallic_guneffic * 200);
      if (spcosteff < 200)
        spcosteff = 200;
      if (spremaining < spcosteff) {
        send_to_char("You don't have enough SP to do that.\n\r", ch);
        return;
      } else {
        send_to_char("The Ki efficiency of your Gallic Gun has increased!\n\r", ch);
        ch->gallic_guneffic += 1;
        ch->spallocated += spcosteff;
        return;
      }
    }
  }
  return;
}

void do_energy_ball(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];
  int AT_AURACOLOR = ch->pcdata->auraColorPowerUp;

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillenergy_ball < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryenergy_ball / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 1;
  adjcost = basecost;
  lowdam = 20;
  highdam = 40;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_AURACOLOR,"You blast $N with a single energy ball. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n blasts you with a single energy ball. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n blasts $N with a single energy ball. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->energy_ballpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_AURACOLOR,"You blast $N with two quick shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n blasts you with two quick shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n blasts $N with two quick shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy balls fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy balls fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy balls fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->energy_ballpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_AURACOLOR,"You blast $N with three rapid shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n blasts you with three rapid shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n blasts $N with three rapid shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy balls fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy balls fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy balls fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->energy_ballpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_AURACOLOR,"You blast $N with four rapid shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n blasts you with four rapid shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n blasts $N with four rapid shots of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy balls fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy balls fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy balls fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->energy_ballpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_AURACOLOR,"You blast $N with a barrage of consecutive shots! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n blasts you with a barrage of consecutive shots! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n blasts $N with a barrage of consecutive shots! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your energy attack.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with an energy attack.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with an energy attack.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 5);
	ch->train += 1;
	ch->masteryenergy_ball += 1;
	ch->energymastery += 1;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_energy_disc(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillenergy_disc < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryenergy_disc / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 15;
  adjcost = basecost;
  lowdam = 120;
  highdam = 140;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You hurl a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy disc fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->energy_discpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You hurl a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy disc fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->energy_discpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You hurl a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy disc fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->energy_discpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You hurl a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy disc fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->energy_discpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You hurl a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls a tiny spinning blade of pure energy straight at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy disc fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy disc fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your energy disc.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with an energy disc.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with an energy disc.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 10);
	ch->train += 10;
	ch->masteryenergy_disc += 1;
	ch->energymastery += 2;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_forcewave(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillforcewave < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryforcewave / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 50;
  adjcost = basecost;
  lowdam = 200;
  highdam = 250;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You force your palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding you with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Forcewave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Forcewave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Forcewave Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->forcewavepower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You force your palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding you with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Forcewave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Forcewave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Forcewave Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->forcewavepower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You force your palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding you with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Forcewave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Forcewave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Forcewave Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->forcewavepower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You force your palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding you with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Forcewave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Forcewave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Forcewave Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->forcewavepower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You force your palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding you with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n forces $s palm forward, bombarding $N with unseen psionic power. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Forcewave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Forcewave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Forcewave Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your forcewave.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with an invisible wave of force.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with an invisible wave of force.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 13);
	ch->train += 13;
	ch->masteryforcewave += 1;
	ch->energymastery += 4;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_concentrated_beam(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillconcentrated_beam < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryconcentrated_beam / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 40;
  adjcost = basecost;
  lowdam = 180;
  highdam = 240;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You thrust one palm forward, firing a dangerous beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a dangerous beam of concentrated energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a dangerous beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your concentrated beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->concentrated_beampower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You thrust one palm forward, firing a large, dangerous beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a large, dangerous beam of concentrated energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a large, dangerous beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your concentrated beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->concentrated_beampower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You thrust one palm forward, firing a huge beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a huge beam of concentrated energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a huge beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your concentrated beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->concentrated_beampower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You thrust one palm forward, firing a massive beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a massive beam of concentrated energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a massive beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your concentrated beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->concentrated_beampower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You thrust one palm forward, firing a colossal beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a colossal beam of concentrated energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts one palm forward, firing a colossal beam of concentrated energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your concentrated beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's concentrated beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your concentrated beam.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with a concentrated beam.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a concentrated beam.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 14);
	ch->train += 14;
	ch->masteryconcentrated_beam += 1;
	ch->energymastery += 3;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_energybeam(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillenergy_beam < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryenergybeam / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 10;
  adjcost = basecost;
  lowdam = 60;
  highdam = 90;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You concentrate momentarily, then suddenly blast $N with a weak beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts you with a weak beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts $N with a weak beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->energybeampower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You concentrate momentarily, then suddenly blast $N with a beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts you with a beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts $N with a beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->energybeampower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You concentrate momentarily, then suddenly blast $N with an impressive beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts you with an impressive beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts $N with an impressive beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->energybeampower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You concentrate momentarily, then suddenly blast $N with a huge beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts you with a huge beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts $N with a huge beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->energybeampower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You concentrate momentarily, then suddenly blast $N with a focused beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts you with a focused beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n concentrates momentarily, then suddenly blasts $N with a focused beam of energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your energy beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's energy beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your energy beam.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with an energy beam.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with an energy beam.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 10);
	ch->train += 10;
	ch->masteryenergybeam += 1;
	ch->energymastery += 2;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_lariat(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float physmult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skilllariat < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	physmult = (float)get_curr_str(ch) / 950 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterylariat / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 80;
  adjcost = basecost;
  lowdam = 450;
  highdam = 500;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"Your arm hooks around $N's neck, dragging them with you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n's arm hooks around your neck, dragging you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's arm hooks around $N's neck, dragging them directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->lariatpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"Your arm hooks around $N's neck, dragging them with you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n's arm hooks around your neck, dragging you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's arm hooks around $N's neck, dragging them directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->lariatpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"Your arm hooks around $N's neck, dragging them with you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n's arm hooks around your neck, dragging you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's arm hooks around $N's neck, dragging them directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->lariatpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"Your arm hooks around $N's neck, dragging them with you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n's arm hooks around your neck, dragging you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's arm hooks around $N's neck, dragging them directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->lariatpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"Your arm hooks around $N's neck, dragging them with you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n's arm hooks around your neck, dragging you directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's arm hooks around $N's neck, dragging them directly through the terrain! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with a barreling collision.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with a barreling collision.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a barreling collision.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "str", 16);
	ch->train += 16;
	ch->masterylariat += 1;
	ch->strikemastery += 5;
	if (ch->strikemastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_ecliptic_meteor(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillecliptic_meteor < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryecliptic_meteor / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 500;
  adjcost = basecost;
  lowdam = 1000;
  highdam = 1250;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You lift your hand high in the sky, a massive aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"An immense ball of energy forms above you, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->ecliptic_meteorpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You lift your hand high in the sky, a massive aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"An immense ball of energy forms above you, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->ecliptic_meteorpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You lift your hand high in the sky, a massive aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"An immense ball of energy forms above you, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->ecliptic_meteorpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You lift your hand high in the sky, a massive aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"An immense ball of energy forms above you, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->ecliptic_meteorpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You lift your hand high in the sky, a massive aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"An immense ball of energy forms above you, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, a massive aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"An immense ball of energy forms above $n, eclipsing the sky before consuming $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Ecliptic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with an Ecliptic Meteor ... somehow.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Ecliptic Meteor ... somehow.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Ecliptic Meteor ... somehow.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 20);
	ch->train += 20;
	ch->masteryecliptic_meteor += 1;
	ch->energymastery += 6;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_gigantic_meteor(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillgigantic_meteor < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterygigantic_meteor / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 250;
  adjcost = basecost;
  lowdam = 700;
  highdam = 750;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You lift your hand high in the sky, an aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"A giant ball of energy forms above you, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping you in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->gigantic_meteorpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You lift your hand high in the sky, an aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"A giant ball of energy forms above you, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping you in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->gigantic_meteorpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You lift your hand high in the sky, an aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"A giant ball of energy forms above you, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping you in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->gigantic_meteorpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You lift your hand high in the sky, an aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"A giant ball of energy forms above you, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping you in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->gigantic_meteorpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You lift your hand high in the sky, an aura of crackling energy encompassing your body.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"A giant ball of energy forms above you, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping you in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n lifts $s hand high in the sky, an aura of crackling energy encompassing $s body.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"A giant ball of energy forms above $n, trapping $N in a massive explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gigantic Meteor fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with a Gigantic Meteor ... somehow.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Gigantic Meteor ... somehow.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Gigantic Meteor ... somehow.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 12);
	ch->train += 12;
	ch->masterygigantic_meteor += 1;
	ch->energymastery += 5;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_meteor(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillmeteor < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterymeteor / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 100;
  adjcost = basecost;
  lowdam = 300;
  highdam = 350;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your palm into $N, detonating the blast and sending $M flying! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes $s palm into you, detonating the blast and sending you flying! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes $s palm into $N, detonating the blast and sending $N flying! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your mass of ki fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->meteorpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your palm into $N, detonating the blast and sending $M flying! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes $s palm into you, detonating the blast and sending you flying! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes $s palm into $N, detonating the blast and sending $N flying! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your mass of ki fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->meteorpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your palm into $N, detonating the blast and sending $M flying! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes $s palm into you, detonating the blast and sending you flying! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes $s palm into $N, detonating the blast and sending $N flying! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your mass of ki fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->meteorpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your palm into $N, detonating the blast and sending $M flying! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes $s palm into you, detonating the blast and sending you flying! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes $s palm into $N, detonating the blast and sending $N flying! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your mass of ki fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->meteorpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your palm into $N, detonating the blast and sending $M flying! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes $s palm into you, detonating the blast and sending you flying! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes $s palm into $N, detonating the blast and sending $N flying! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your mass of ki fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's mass of ki fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Meteor attack.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Meteor attack.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Meteor attack.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 8);
	ch->train += 8;
	ch->masterymeteor += 1;
	ch->energymastery += 4;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_crusherball(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillcrusherball < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterycrusherball / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 15;
  adjcost = basecost;
  lowdam = 120;
  highdam = 140;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You hurl an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->crusherballpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You hurl an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->crusherballpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You hurl an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->crusherballpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You hurl an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->crusherballpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You pull back an arm and focus, drawing ki into your open palm.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You hurl an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls back an arm and focuses, drawing ki into $s open palm.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n hurls an oversized ball of seething energy directly into $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Crusher Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Crusher Ball.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with a Crusher Ball.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a Crusher Ball.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 5);
	ch->train += 5;
	ch->masterycrusherball += 1;
	ch->energymastery += 2;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_haymaker(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float physmult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillhaymaker < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	physmult = (float)get_curr_str(ch) / 950 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryhaymaker / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 10;
  adjcost = basecost;
  lowdam = 60;
  highdam = 90;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 6);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You rush down $N with an arm drawn back, throwing all of your weight forward.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your fist straight into $N with all your might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n rushes you down with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes his fist straight into you with all $s might, crushing you into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n rushes down $N with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes his fist straight into $N with all $s might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->haymakerpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You rush down $N with an arm drawn back, throwing all of your weight forward.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your fist straight into $N with all your might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n rushes you down with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes his fist straight into you with all $s might, crushing you into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n rushes down $N with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes his fist straight into $N with all $s might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->haymakerpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You rush down $N with an arm drawn back, throwing all of your weight forward.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your fist straight into $N with all your might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n rushes you down with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes his fist straight into you with all $s might, crushing you into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n rushes down $N with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes his fist straight into $N with all $s might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->haymakerpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You rush down $N with an arm drawn back, throwing all of your weight forward.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your fist straight into $N with all your might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n rushes you down with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes his fist straight into you with all $s might, crushing you into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n rushes down $N with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes his fist straight into $N with all $s might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->haymakerpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You rush down $N with an arm drawn back, throwing all of your weight forward.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You crash your fist straight into $N with all your might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n rushes you down with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n crashes his fist straight into you with all $s might, crushing you into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n rushes down $N with an arm drawn back, throwing all of $s weight forward.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n crashes his fist straight into $N with all $s might, crushing $M into the dirt! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You can't seem to get any power out of your punch!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n can't seem to get any power out of $s punch!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your haymaker.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s haymaker.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s haymaker.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "str", 12);
	ch->train += 12;
	ch->masteryhaymaker += 1;
	ch->strikemastery += 2;
	if (ch->strikemastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_collide(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float physmult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillcollide < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	physmult = (float)get_curr_str(ch) / 950 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterycollide / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 25;
  adjcost = basecost;
  lowdam = 180;
  highdam = 220;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You collide your body directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$N's body collides directly with you, sending you careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's body collides directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->collidepower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You collide your body directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$N's body collides directly with you, sending you careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's body collides directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->collidepower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You collide your body directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$N's body collides directly with you, sending you careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's body collides directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->collidepower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You collide your body directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$N's body collides directly with you, sending you careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's body collides directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->collidepower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * physmult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_str(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"Through a billowing cloud of dust and debris, you tear forward at $N.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You collide your body directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n tears forward at you, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$N's body collides directly with you, sending you careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n tears forward at $N, leaving a billowing cloud of dust and debris in $s wake.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's body collides directly with $N, sending them careening from the impact! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"You collide with $N, but can't seem to generate any power!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n collides with you but can't seem to generate any power!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n collides with $N but can't seem to generate any power!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with a barreling collision.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with a barreling collision.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a barreling collision.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "str", 14);
	ch->train += 14;
	ch->masterycollide += 1;
	ch->strikemastery += 4;
	if (ch->strikemastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_kamehameha(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillkamehameha < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterykamehameha / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 100;
  adjcost = basecost;
  lowdam = 500;
  highdam = 600;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_LBLUE,"You put your arms back and cup your hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"You push your hands forward, throwing a blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a blue beam at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Kamehameha fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->kamehamehapower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_LBLUE,"You put your arms back and cup your hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"You push your hands forward, throwing a large blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a large blue beam at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a large blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Kamehameha fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->kamehamehapower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_LBLUE,"You put your arms back and cup your hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"You push your hands forward, throwing a huge blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a huge blue beam at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a huge blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Kamehameha fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->kamehamehapower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_LBLUE,"You put your arms back and cup your hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"You push your hands forward, throwing a massive blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a massive blue beam at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"$n pushes $s hands forward, throwing a massive blue beam at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Kamehameha fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Kamehameha fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->kamehamehapower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_LBLUE,"You put your arms back and cup your hands. 'KAAAA-MEEEE...'", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"Your aura explodes into a raging inferno around you, coalescing between your palms.", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"'HAAAA-MEEEE--'", ch, NULL, victim, TO_CHAR);
	  act(AT_LBLUE,"'HAAAAAAAAAAAA!!!' You push your hands forward, unleashing a colossal beam of energy at $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KAAAA-MEEEE...'", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"$n's aura explodes into a raging inferno, coalescing between $s palms.", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"'HAAAA-MEEEE--'", ch, NULL, victim, TO_VICT);
	  act(AT_LBLUE,"'HAAAAAAAAAAAA!!!' $n pushes $s hands forward, unleashing a colossal beam of energy at you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  
	  act(AT_LBLUE,"$n puts $s arms back and cups $s hands. 'KAAAA-MEEEE...'", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"$n's aura explodes into a raging inferno, coalescing between $s palms.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"'HAAAA-MEEEE--'", ch, NULL, victim, TO_NOTVICT);
	  act(AT_LBLUE,"'HAAAAAAAAAAAA!!!' $n pushes $s hands forward, unleashing a colossal beam of energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Super Kamehameha fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Super Kamehameha fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Super Kamehameha fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Kamehameha.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Kamehameha.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a Kamehameha.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 16);
	ch->train += 16;
	ch->masterykamehameha += 1;
	ch->energymastery += 5;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

/* Old Kame for reference
void do_kamehameha(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  float smastery = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillkamehameha < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
    kilimit = ch->train / 10000;
    kimult = (float)get_curr_int(ch) / 1000 + 1;
    kicmult = (float)kilimit / 100 + 1;
    smastery = (float)ch->masterykamehameha / 10000;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
    adjcost = 100 * (ch->kamehamehapower - ch->kamehamehaeffic);
    if (adjcost < 100)
      adjcost = 100;
  } else {
    adjcost = 1;
  }
  if (ch->mana < adjcost) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
    if (!IS_NPC(ch)) {
      argdam = ((number_range(50, 60) + (ch->kamehamehapower * 5)) * (kicmult + smastery));
      dam = get_attmod(ch, victim) * (argdam * kimult);
      stat_train(ch, "int", 16);
      ch->train += 16;
      ch->masterykamehameha += 1;
      ch->energymastery += 5;
      if (ch->energymastery >= (ch->kspgain * 100)) {
        pager_printf_color(ch,
                           "&CYou gained 5 Skill Points!\n\r");
        ch->sptotal += 5;
        ch->kspgain += 1;
      }
    }
    if (IS_NPC(ch)) {
      argdam = number_range(50, 60);
      dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
    }
    if (dam < 1)
      dam = 1;
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);
    act(AT_LBLUE,
        "You put your arms back and cup your hands. 'KA-ME-HA-ME-HA!!!!'	",
        ch, NULL, victim, TO_CHAR);
    act(AT_LBLUE,
        "You push your hands forward, throwing a blue beam at $N. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);
    act(AT_LBLUE,
        "$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	",
        ch, NULL, victim, TO_VICT);
    act(AT_LBLUE,
        "$n pushes $s hands forward, throwing a blue beam at you. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);
    act(AT_LBLUE,
        "$n puts $s arms back and cups $s hands. 'KA-ME-HA-ME-HA!!!!'	",
        ch, NULL, victim, TO_NOTVICT);
    act(AT_LBLUE,
        "$n pushes $s hands forward, throwing a blue beam at $N. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);

    global_retcode = damage(ch, victim, dam, TYPE_HIT);
  } else {
    act(AT_LBLUE, "You missed $N with your kamehameha.", ch, NULL,
        victim, TO_CHAR);
    act(AT_LBLUE, "$n misses you with $s kamehameha.", ch, NULL,
        victim, TO_VICT);
    act(AT_LBLUE, "$n missed $N with a kamehameha.", ch, NULL,
        victim, TO_NOTVICT);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  ch->mana -= adjcost;
  return;
}*/

void do_masenko(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  float smastery = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillmasenko < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	// Player pfile mastery values for damage scaling -- change carefully
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterymasenko / 10000;
	if (smastery > 10)
	  smastery = 10; // cap bonus for # times skill used
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  adjcost = 60; //base ki cost
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') { //no argument, base skill
	  if (!IS_NPC(ch)) {
		argdam = (number_range(300, 350) * (kicmult + smastery)); //average base damage x - y
		dam = get_attmod(ch, victim) * (argdam * kimult); //scale player off pfile masteries
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(300, 350);  //average base damage x - y
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40)); //scale mob off raw int value
	  }
	  if (dam < 1)
		dam = 1;
	  // ACT descriptors  $n first actor(user) short desc, $N second actor(victim) short desc.
	  // $s $S his/her/its // $m $M him/her/it // $t num_punct final damage value
	  // act(AT_COLOR,) choose from ANSI standards
	  // AT_COLOR strings accept 'help pcolor' styles
	  act(AT_YELLOW,"You hold your arms high above your head, charging your ki.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You throw your hands at $N and send a yellow beam at $M, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n throws $s hands at you and fires a yellow beam, hitting you in the chest. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s hands at $N and sends a yellow beam shooting forward, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Masenko fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT); // return generic damage, failure
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT); // return generic damage, success
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->masenkopower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(600, 700) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(600, 700);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 240; // increase cost
	  act(AT_YELLOW,"You hold your arms high above your head, charging your ki.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You throw your hands at $N and send a large yellow beam at $M, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n throws $s hands at you and fires a large yellow beam, hitting you in the chest. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s hands at $N and sends a large yellow beam shooting forward, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Masenko fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->masenkopower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(900, 1050) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(900, 1050);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 960; // increase cost
	  act(AT_YELLOW,"You hold your arms high above your head, charging your ki.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You throw your hands at $N and send a huge yellow beam at $M, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n throws $s hands at you and fires a huge yellow beam, hitting you in the chest. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s hands at $N and sends a huge yellow beam shooting forward, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Masenko fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->masenkopower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(1200, 1400) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(1200, 1400);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 3840; // increase cost
	  act(AT_YELLOW,"You hold your arms high above your head, charging your ki.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You throw your hands at $N and send a massive yellow beam at $M, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n throws $s hands at you and fires a massive yellow beam, hitting you in the chest. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, charging $s ki.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s hands at $N and sends a massive yellow beam shooting forward, hitting $M in the chest. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Masenko fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->masenkopower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(1500, 1750) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(1500, 1750);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 15360; // increase cost
	  act(AT_YELLOW,"You hold your arms high above your head, focusing an immense amount of energy.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You throw your hands at $N and send a devastating beam forward at $M, engulfing $M in the ensuing explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, focusing an immense amount of energy.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n throws $s hands at you and sends forth a devastating beam, engulfing you in the ensuing explosion! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds $s arms high above $s head, focusing an immense amount of energy.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s hands at $N and sends forth a devastating beam, engulfing $M in the ensuing explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Masenko fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Masenko fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Masenko.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Masenko.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a Masenko.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 14);
	ch->train += 14;
	ch->masterymasenko += 1;
	ch->energymastery += 4;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_sbc(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  float smastery = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillsbc < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	// Player pfile mastery values for damage scaling -- change carefully
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterysbc / 10000;
	if (smastery > 10)
	  smastery = 10; // cap bonus for # times skill used
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  adjcost = 110; //base ki cost
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') { //no argument, base skill
	  if (!IS_NPC(ch)) {
		argdam = (number_range(550, 650) * (kicmult + smastery)); //average base damage x - y
		dam = get_attmod(ch, victim) * (argdam * kimult); //scale player off pfile masteries
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(550, 650);  //average base damage x - y
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40)); //scale mob off raw int value
	  }
	  if (dam < 1)
		dam = 1;
	  // ACT descriptors  $n first actor(user) short desc, $N second actor(victim) short desc.
	  // $s $S his/her/its // $m $M him/her/it // $t num_punct final damage value
	  // act(AT_COLOR,) choose from ANSI standards
	  // AT_COLOR strings accept 'help pcolor' styles
	  act(AT_YELLOW,"You put two fingers to your forehead, crackling energy gathering at their tips.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You thrust your fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts $s fingers toward you, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Special Beam Cannon fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT); // return generic damage, failure
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT); // return generic damage, success
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->sbcpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(1100, 1300) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(1100, 1300);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 240; // increase cost
	  act(AT_YELLOW,"You put two fingers to your forehead, crackling energy gathering at their tips.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You thrust your fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts $s fingers toward you, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Special Beam Cannon fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->sbcpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(1650, 1950) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(1650, 1950);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 960; // increase cost
	  act(AT_YELLOW,"You put two fingers to your forehead, crackling energy gathering at their tips.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You thrust your fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts $s fingers toward you, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Special Beam Cannon fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->sbcpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(2200, 2600) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(2200, 2600);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 3840; // increase cost
	  act(AT_YELLOW,"You put two fingers to your forehead, crackling energy gathering at their tips.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You thrust your fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts $s fingers toward you, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Special Beam Cannon fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->sbcpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(2750, 3250) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(2750, 3250);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = 15360; // increase cost
	  act(AT_YELLOW,"You put two fingers to your forehead, crackling energy gathering at their tips.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"It pulses and lances menacingly through the air, building to a fever pitch.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"You thrust your fingers toward $N, sending out a deadly corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n thrusts $s fingers toward you, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n puts two fingers to $s forehead as crackling energy gathers at their tips.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n throws $s fingers toward $N, sending out a corkscrew beam! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Special Beam Cannon fizzles out!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Special Beam Cannon fizzles out!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Special Beam Cannon.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Special Beam Cannon.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a Special Beam Cannon.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 18);
	ch->train += 18;
	ch->masterysbc += 1;
	ch->energymastery += 5;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_dd(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skilldestructo_disc < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterydestructo_disc / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 55;
  adjcost = basecost;
  lowdam = 280;
  highdam = 280;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You hold a hand aloft, generating a giant, deadly spinning disc of energy.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"With precision and a strong arm, you hurl your Destructo Disc directly at $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds a hand aloft, generating a giant, deadly spinning disc of energy.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Disc straight at you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds a hand aloft, generating a giant, deadly spinning disc of energy.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Disc straight at $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructo Disc fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructo Disc fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructo Disc fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->destructo_discpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You hold a hand aloft, generating two giant, deadly spinning discs of energy.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"With precision and a strong arm, you hurl your Destructo Discs directly at $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds a hand aloft, generating two giant, deadly spinning discs of energy.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Discs straight at you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds a hand aloft, generating two giant, deadly spinning discs of energy.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Disc straight at $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->destructo_discpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You hold a hand aloft, generating three giant, deadly spinning discs of energy.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"With precision and a strong arm, you hurl your Destructo Discs directly at $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds a hand aloft, generating three giant, deadly spinning discs of energy.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Discs straight at you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds a hand aloft, generating three giant, deadly spinning discs of energy.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Discs straight at $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->destructo_discpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You hold a hand aloft, generating four giant, deadly spinning discs of energy.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"With precision and a strong arm, you hurl your Destructo Discs directly at $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds a hand aloft, generating four giant, deadly spinning disc of energy.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Discs straight at you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds a hand aloft, generating four giant, deadly spinning disc of energy.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"With precision and a strong arm, $n hurls their Destructo Discs straight at $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->destructo_discpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You hold a hand aloft, generating a colossal, deadly spinning disc of energy.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"Your single destructo disc splits apart into countless smaller discs, chasing down $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n holds a hand aloft, generating a colossal, deadly spinning disc of energy.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"$n's single destructo disc splits apart into countless smaller discs, chasing you down! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n holds a hand aloft, generating a colossal, deadly spinning disc of energy.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"$n's single destructo disc splits apart into countless smaller discs, chasing $N down! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructo Discs fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Destructo Disc.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Destructo Disc.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with a Destructo Disc attack.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 13);
	ch->train += 13;
	ch->masterydestructo_disc += 1;
	ch->energymastery += 4;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_ff(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ff))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_ff]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_ff]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_ff]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_ff]->focus;

  WAIT_STATE(ch, skill_table[gsn_ff]->beats);
  if (can_use_skill(ch, number_percent(), gsn_ff)) {
    dam = get_attmod(ch, victim) * (number_range(56, 62) + (get_curr_int(ch) / 20));
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);
    act(AT_YELLOW,
        "Your body begins to surge with power, arms outstretched. Bringing your hands together, a clang like echo is heard all around the area. ",
        ch, NULL, victim, TO_CHAR);
    act(AT_YELLOW,
        "A swirling vortex of sparkling energy gathers then disappears into your hands, exploding forth like a raging storm towards $N. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);
    act(AT_YELLOW,
        "$n begins to surge with power, arms outstretched. Bringing $s hands together, a clang like echo is heard all around the area. ",
        ch, NULL, victim, TO_VICT);
    act(AT_YELLOW,
        "A swirling vortex of sparkling energy gathers then disappears into $s hands, exploding forth like a raging storm towards you. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);
    act(AT_YELLOW,
        "$n begins to surge with power, arms outstretched. Bringing $s hands together, a clang like echo is heard all around the area. ",
        ch, NULL, victim, TO_NOTVICT);
    act(AT_YELLOW,
        "A swirling vortex of sparkling energy gathers then disappears into $s hands, exploding forth like a raging storm towards $N. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);
    learn_from_success(ch, gsn_ff);
    global_retcode = damage(ch, victim, dam, TYPE_HIT);
    if (!IS_NPC(ch)) {
      stat_train(ch, "int", 12);
      ch->train += 3;
    }
  } else {
    act(AT_YELLOW, "You missed $N with your final flash.", ch, NULL,
        victim, TO_CHAR);
    act(AT_YELLOW, "$n misses you with $s final flash.", ch, NULL,
        victim, TO_VICT);
    act(AT_YELLOW, "$n missed $N with a final flash.", ch, NULL,
        victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_ff);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  ch->mana -= skill_table[gsn_ff]->min_mana;
  return;
}

void do_suppress(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *och;
  CHAR_DATA *och_next;
  long double arg = 0;
  int hitcheck = 0;

  if (IS_NPC(ch)) {
    send_to_char("huh?\n\r", ch);
    return;
  }

  if (argument[0] != '\0') {
    arg = is_number(argument) ? atof(argument) : -1;
    if (atof(argument) < -1 && arg == -1)
      arg = atof(argument);

    if (arg < 1) {
      send_to_char("You can't suppress -that- much!\n\r",
                   ch);
      return;
    }
    if (arg > ch->exp && arg >= ch->releasepl) {
      send_to_char("Nice try!  You can't pull power from nowhere!\n\r",
                   ch);
      return;
    }
	if (arg > ch->exp && arg < ch->releasepl) {
	  ch->pcdata->suppress = arg;
      send_to_char("Okay.  Suppress level set!\n\r",
                   ch);
      return;
    }
    if (!IS_NPC(ch))
      ch->pcdata->suppress = arg;
    send_to_char("Okay.  Suppress level set!\n\r", ch);
    return;
  }
  if (ch->position == POS_EVASIVE || ch->position == POS_DEFENSIVE || ch->position == POS_AGGRESSIVE || ch->position == POS_BERSERK || ch->position == POS_FIGHTING) {
    send_to_char("You can't suppress while in battle.\n\r", ch);
    return;
  }
  WAIT_STATE(ch, 8);
  hitcheck = number_range(1, 100);
  if (hitcheck <= 100) {
    if (!IS_NPC(ch)) {
      send_to_pager_color("&BYou power down and suppress your power.\n\r",
                          ch);
      act(AT_BLUE,
          "$n takes a slow, deep breath and exhales calmly.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    if (!IS_NPC(ch)) {
      if (ch->pcdata->suppress < 1)
        ch->pcdata->suppress = ch->exp;

      ch->pl = ch->pcdata->suppress;
    } else {
      if (!ch->master)
        return;
      if (!ch->master->pcdata)
        return;

      if (ch->master->pcdata->suppress < 1 ||
          ch->master->pcdata->suppress > ch->exp)
        ch->master->pcdata->suppress = ch->exp;

      ch->pl = ch->master->pcdata->suppress;
    }

    heart_calc(ch, "");
    if (!IS_NPC(ch)) {
      if (is_splitformed(ch)) {
        for (och = first_char; och; och = och_next) {
          och_next = och->next;

          if (!IS_NPC(och))
            continue;

          if (ch->pl < ch->exp)
            continue;

          if (is_split(och) && och->master == ch)
            do_suppress(och, "");
        }
      }
    }
    if (IS_NPC(ch)) {
      send_to_pager_color("&BYou power down and suppress your power.\n\r",
                          ch);
      act(AT_BLUE,
          "$n takes a slow, deep breath and exhales calmly.",
          ch, NULL, NULL, TO_NOTVICT);
    }
  } else {
    act(AT_SKILL, "You try but can't seem to suppress your power.",
        ch, NULL, NULL, TO_CHAR);
  }
  return;
}

void do_meditate(CHAR_DATA *ch, char *argument) {
  float left = 0;
  float right = 0;
  long double xp_gain = 0;
  int statComb = 0;
  int increase = 0;
  int hitcheck = 0;
  double weighttrainmult = 0;
  double weightstatmult = 0;
  int weightstat = 0;
  int weighttrain = 0;
  double totalrgrav = 0;
  double addedrweight = 0;
  double playerrweight = 0;

  if (IS_NPC(ch))
    return;

  addedrweight = (double)weightedtraining(ch) / 100000;
  playerrweight = (double)1 + addedrweight;
  totalrgrav = (double)ch->gravSetting * playerrweight;
  weighttrainmult = (double)((double)totalrgrav / 50) + 1;
  if (is_kaio(ch) || is_namek(ch)) {
    weighttrain = 30 * weighttrainmult;
    weightstatmult = (double)((double)totalrgrav / 200) + 1;
    weightstat = 60 * weightstatmult;
  } else {
    weighttrain = 26 * weighttrainmult;
    weightstatmult = (double)((double)totalrgrav / 200) + 1;
    weightstat = 50 * weightstatmult;
  }
  // check for current gravity training effects
  if (IS_AFFECTED(ch, AFF_PUSHUPS) || IS_AFFECTED(ch, AFF_SHADOWBOXING) || IS_AFFECTED(ch, AFF_ENDURING) || IS_AFFECTED(ch, AFF_MEDITATION) || IS_AFFECTED(ch, AFF_EXPUSHUPS) || IS_AFFECTED(ch, AFF_EXSHADOWBOXING) || IS_AFFECTED(ch, AFF_EXENDURING) || IS_AFFECTED(ch, AFF_POWERUPTRAIN) || IS_AFFECTED(ch, AFF_INTEXPUSHUPS) || IS_AFFECTED(ch, AFF_INTEXSHADOWBOXING) || IS_AFFECTED(ch, AFF_INTEXENDURING)) {
    send_to_char("You can't seem to focus while you're this busy!\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->in_room->room_flags, ROOM_GRAV)) {
    send_to_char("You're not currently able to meditate in a gravity chamber due to a potential exploit. Sorry!\n\r", ch);
    send_to_char("This will be fixed soon. -Zerhyn\n\r", ch);
    return;
  }
  // check if user is powering up
  if (IS_AFFECTED(ch, AFF_POWERCHANNEL)) {
    send_to_char("You cannot meditate while powering up.\n\r", ch);
    return;
  }

  right = (float)95 / 100;

  if (ch->fighting) {
    send_to_char("&wYou can't concentrate enough for that.\n\r",
                 ch);
    return;
  }
  if (left >= right) {
    send_to_char("&wYou are already at peace with everything.\n\r",
                 ch);
    return;
  }
  switch (ch->substate) {
    default:
      if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
        send_to_char("&wYou can't concentrate enough for that.\n\r",
                     ch);
        return;
      }
      if (ch->mount) {
        send_to_char("&wYou can't do that while mounted.\n\r",
                     ch);
        return;
      }
      add_timer(ch, TIMER_DO_FUN, number_range(1, 3), do_meditate, 1);
      send_to_char("&wYou begin to meditate...\n\r", ch);
      act(AT_PLAIN, "$n begins to meditate...", ch, NULL, NULL,
          TO_ROOM);
      return;

    case 1:
      if (ch->fighting) {
        send_to_char("&wYou stop meditating...\n\r", ch);
        act(AT_PLAIN, "$n stops meditating...", ch, NULL, NULL,
            TO_ROOM);
        return;
      }
      if (!ch->desc)
        return;
      hitcheck = number_range(1, 100);
      if (hitcheck <= 100) {
        send_to_char("&wYou meditate peacefully, collecting energy from the cosmos\n\r",
                     ch);
        if (!IS_NPC(ch)) {
          stat_train(ch, "int", weightstat);
          statComb = ((get_curr_str(ch) + get_curr_dex(ch) + get_curr_int(ch) + get_curr_con(ch)) - 39);
          increase = number_range(1, 3);
          xp_gain = (long double)increase / 75000 * statComb;
          gain_exp(ch, xp_gain);
          ch->mana += (float)right / 19 * ch->max_mana;
          ch->mana += 10;
          ch->train += weighttrain;
          if (is_namek(ch) || is_kaio(ch))
            ch->energymastery += 8;
          if (ch->energymastery >= (ch->kspgain * 100)) {
            pager_printf_color(ch,
                               "&CYou gained 5 Skill Points!\n\r");
            ch->sptotal += 5;
            ch->kspgain += 1;
          }
          if (ch->mana > ch->max_mana) {
            ch->mana = ch->max_mana;
            send_to_char("&wYour excess energy dissipates back into the cosmos\n\r",
                         ch);
          }
        }
      } else {
        send_to_char("&wYou spend several minutes in deep concentration, but fail to collect much energy.\n\r",
                     ch);
        ch->mana += 5;
        if (ch->mana > ch->max_mana)
          ch->mana = ch->max_mana;
        send_to_char("&wYour excess energy dissipates back into the cosmos\n\r",
                     ch);
      }

      if (left >= right) {
        send_to_char("&wYou are now at peace with everything.\n\r", ch);
        return;
      }
      add_timer(ch, TIMER_DO_FUN, number_range(1, 3), do_meditate, 2);
      return;
    case 2:
      if (ch->fighting) {
        send_to_char("&wYou stop meditating...\n\r", ch);
        act(AT_PLAIN, "$n stops meditating...", ch, NULL, NULL,
            TO_ROOM);
        return;
      }
      if (!ch->desc)
        return;
      hitcheck = number_range(1, 100);
      if (hitcheck <= 100) {
        send_to_char("&wYou meditate peacefully, collecting energy from the cosmos\n\r",
                     ch);
        if (!IS_NPC(ch)) {
          stat_train(ch, "int", weightstat);
          statComb = ((get_curr_str(ch) + get_curr_dex(ch) + get_curr_int(ch) + get_curr_con(ch)) - 39);
          increase = number_range(1, 3);
          xp_gain = (long double)increase / 75000 * statComb;
		  if (!is_bio(ch)) {
            gain_exp(ch, xp_gain);
		  }
          ch->mana += (float)right / 19 * ch->max_mana;
          ch->mana += 10;
          ch->train += weighttrain;
          if (is_namek(ch) || is_kaio(ch))
            ch->energymastery += 8;
          if (ch->energymastery >= (ch->kspgain * 100)) {
            pager_printf_color(ch,
                               "&CYou gained 5 Skill Points!\n\r");
            ch->sptotal += 5;
            ch->kspgain += 1;
          }
          if (ch->mana > ch->max_mana) {
            ch->mana = ch->max_mana;
            send_to_char("&wYour excess energy dissipates back into the cosmos\n\r",
                         ch);
          }
        }
      } else {
        send_to_char("&wYou spend several minutes in deep concentration, but fail to collect much energy.\n\r",
                     ch);
        ch->mana += 5;
        if (ch->mana > ch->max_mana)
          ch->mana = ch->max_mana;
        send_to_char("&wYour excess energy dissipates back into the cosmos\n\r",
                     ch);
      }
      if (left >= right) {
        send_to_char("&wYou are now at peace with everything.\n\r", ch);
        return;
      }
      add_timer(ch, TIMER_DO_FUN, number_range(1, 3), do_meditate, 1);
      return;
    case SUB_TIMER_DO_ABORT:
      ch->substate = SUB_NONE;
      send_to_char("&wYou stop meditating...\n\r", ch);
      act(AT_PLAIN, "$n stops meditating...", ch, NULL, NULL,
          TO_ROOM);
      return;
  }
  return;
}
void do_scatter_shot(CHAR_DATA *ch, char *arg) {
  char buf[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  CHAR_DATA *victim;
  int shot = 0;
  int shots = 0;
  int dam = 0;
  int energy = 0;
  int damPerShot = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_scatter_shot))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_scatter_shot]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (arg[0] == '\0' || atoi(arg) <= 0) {
    send_to_char("Syntax: scatter <# of Shots>\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  shot = atoi(arg);
  if (shot > 50)
    shot = 50;
  energy = shot * skill_table[gsn_scatter_shot]->min_mana;
  shots = shot;
  strcpy(buf, num_punct(shot));

  if (ch->mana < energy) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus <
      (skill_table[gsn_scatter_shot]->focus * (1 + (shot / 10)))) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -=
        (skill_table[gsn_scatter_shot]->focus * (1 + (shot / 10)));

  sh_int z = get_aura(ch);

  WAIT_STATE(ch,
             (skill_table[gsn_scatter_shot]->beats * (1 + (shot / 10))));
  if (can_use_skill(ch, number_percent(), gsn_scatter_shot)) {
    while (shots > 0) {
      switch (number_range(1, 4)) {
        default:
        case 1:
          break;
        case 2:
        case 3:
          dam += number_range(0, 1);
          break;
        case 4:
          dam += number_range(0, 2);
          break;
      }
      shots--;
    }
    damPerShot = number_range(1, 3);
    strcpy(buf2,
           num_punct(get_attmod(ch, victim) * dam * damPerShot));

    act(z, "You power up and yell, 'SCATTER SHOT!'", ch, NULL,
        victim, TO_CHAR);
    act2(z,
         "You launch a barrage of $t energy balls towards $N, "
         "all exploding on contact. &W[$T]",
         ch, buf, victim,
         TO_CHAR, buf2);
    act(z, "$n powers up and yells, 'SCATTER SHOT!'", ch, NULL,
        victim, TO_VICT);
    act2(z,
         "$e launches a barrage of $t energy balls towards you, "
         "all exploding on contact. &W[$T]",
         ch, buf, victim,
         TO_VICT, buf2);
    act(z, "$n powers up and yells, 'SCATTER SHOT!'", ch, NULL,
        victim, TO_NOTVICT);
    act2(z,
         "$e launches a barrage of $t energy balls towards $N, "
         "all exploding on contact. &W[$T]",
         ch, buf, victim,
         TO_NOTVICT, buf2);

    learn_from_success(ch, gsn_scatter_shot);
    if (!IS_NPC(ch)) {
      stat_train(ch, "int", 6);
      ch->train += 2;
    }
    global_retcode =
        damage(ch, victim,
               (get_attmod(ch, victim) * dam * damPerShot),
               TYPE_HIT);
  } else {
    act(z,
        "You launch a barrage of $t energy balls towards $N but $E is to fast to hit.",
        ch, buf, victim, TO_CHAR);
    act(z,
        "$n launches a barrage of $t energy balls towards $N but you are to fast to hit.",
        ch, buf, victim, TO_VICT);
    act(z,
        "$n launches a barrage of $t energy balls towards $N but $E is to fast to hit.",
        ch, buf, victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_scatter_shot);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!is_android_h(ch))
    ch->mana -= energy;
  return;
}

void do_sense(CHAR_DATA *ch, char *argument) {
  char arg[MAX_INPUT_LENGTH];
  CHAR_DATA *victim;
  char *msg;
  long double diff;
  int hitcheck = 0;

  one_argument(argument, arg);

  if (arg[0] == '\0') {
    send_to_char("Sense whose power?\n\r", ch);
    return;
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("They must be too far away for you to sense.\n\r",
                 ch);
    return;
  }
  if (victim == ch) {
    pager_printf_color(ch,
                       "You would get slaughtered in a battle against %s.\n\r",
                       himher(ch, false));
    return;
  }
  if (is_android(victim) || is_superandroid(victim) ||
      wearing_sentient_chip(victim)) {
    pager_printf_color(ch, "You can't sense any thing from %s.\n\r",
                       himher(ch, false));
    return;
  }
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 100) {
    if (!IS_NPC(victim))
      diff = (long double)victim->pl / ch->pl * 10;
    else
      diff = (long double)victim->exp / ch->pl * 10;

    if (diff < 1)
      msg =
          "Compared to yourself, you sense no power from $N.";
    else if (diff <= 1)
      msg =
          "The flies buzzing around $N would prove more challenging.";
    else if (diff <= 2)
      msg = "You sense a child's power coming from $N.";
    else if (diff <= 3)
      msg = "You sense a feeble power coming from $N.";
    else if (diff <= 4)
      msg = "You sense a laughable power coming from $N.";
    else if (diff <= 5)
      msg = "You sense a very small power coming from $N.";
    else if (diff <= 6)
      msg =
          "You sense an insignificant power coming from $N.";
    else if (diff <= 7)
      msg = "You sense a small power coming from $N.";
    else if (diff <= 8)
      msg = "You sense a minor power coming from $N.";
    else if (diff <= 9)
      msg = "You sense a lesser power coming from $N.";
    else if (diff <= 10)
      msg = "$E seems to be about as strong as you.";
    else if (diff <= 20)
      msg = "You sense a greater power coming from $N.";
    else if (diff <= 30)
      msg = "You sense a significant power coming from $N.";
    else if (diff <= 40)
      msg = "You sense a substantial power coming from $N.";
    else if (diff <= 50)
      msg = "You sense an imposing power coming from $N.";
    else if (diff <= 60)
      msg = "You sense a disturbing power coming from $N.";
    else if (diff <= 70)
      msg = "You sense a frightening power coming from $N.";
    else if (diff <= 80)
      msg = "You sense a horrifying power coming from $N.";
    else if (diff <= 90)
      msg = "$N's power is too monstrous for you to even comprehend.";

    act(AT_CONSIDER, msg, ch, NULL, victim, TO_CHAR);
  } else {
    pager_printf_color(ch,
                       "You try, but you sense nothing from %s.\n\r",
                       himher(victim, false));
  }

  return;
}

/*void do_ddd(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ddd))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_ddd]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_ddd]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_ddd]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_ddd]->focus;

  WAIT_STATE(ch, skill_table[gsn_ddd]->beats);
  if (can_use_skill(ch, number_percent(), gsn_ddd)) {
    dam = get_attmod(ch, victim) * (number_range(24, 30) + (get_curr_int(ch) / 35));
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);
    act(AT_RED,
        "You charge two spinning energy disks above your head and hurl them towards $N. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);
    act(AT_RED,
        "$n charges two spinning energy disks above $s head and hurls them towards you. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);
    act(AT_RED,
        "$n charges two spinning energy disks above $s head and hurl them towards $N. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);

    learn_from_success(ch, gsn_ddd);
    global_retcode = damage(ch, victim, dam, TYPE_HIT);
    if (!IS_NPC(ch)) {
      stat_train(ch, "int", 8);
      ch->train += 2;
    }
  } else {
    act(AT_RED, "You missed $N with your dual destructo disk.", ch,
        NULL, victim, TO_CHAR);
    act(AT_RED, "$n misses you with $s dual destructo disk.", ch,
        NULL, victim, TO_VICT);
    act(AT_RED, "$n missed $N with a dual destructo disk.", ch,
        NULL, victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_ddd);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  ch->mana -= skill_table[gsn_ddd]->min_mana;
  return;
}*/

void do_death_ball(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skilldeath_ball < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterydeath_ball / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 500;
  adjcost = basecost;
  lowdam = 1000;
  highdam = 1250;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_ORANGE,"You raise your arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_CHAR);
	  act(AT_ORANGE,"You point effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_VICT);
	  act(AT_ORANGE,"$n points effortlessly at you, sending you to your impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_VICT);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_NOTVICT);
	  act(AT_ORANGE,"$n points effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Death Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->death_ballpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_ORANGE,"You raise your arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_CHAR);
	  act(AT_ORANGE,"You point effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_VICT);
	  act(AT_ORANGE,"$n points effortlessly at you, sending you to your impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_VICT);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_NOTVICT);
	  act(AT_ORANGE,"$n points effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Death Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->death_ballpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_ORANGE,"You raise your arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_CHAR);
	  act(AT_ORANGE,"You point effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_VICT);
	  act(AT_ORANGE,"$n points effortlessly at you, sending you to your impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_VICT);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_NOTVICT);
	  act(AT_ORANGE,"$n points effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Death Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->death_ballpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_ORANGE,"You raise your arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_CHAR);
	  act(AT_ORANGE,"You point effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_VICT);
	  act(AT_ORANGE,"$n points effortlessly at you, sending you to your impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_VICT);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, hovering lifelessly above.",
	  ch, NULL, victim, TO_NOTVICT);
	  act(AT_ORANGE,"$n points effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Death Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->death_ballpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_ORANGE,"You raise your arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, blotting out all else.",
	  ch, NULL, victim, TO_CHAR);
	  act(AT_ORANGE,"You point effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, blotting out all else.",
	  ch, NULL, victim, TO_VICT);
	  act(AT_ORANGE,"$n points effortlessly at you, sending you to your impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_VICT);
	  act(AT_ORANGE,"$n raises $m arm to the heavens, palm open. A swirling vortex of hellish light gathers into a ball, blotting out all else.",
	  ch, NULL, victim, TO_NOTVICT);
	  act(AT_ORANGE,"$n points effortlessly at $N, sending them to their impending demise. &W[$t]",
	  ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Death Ball fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Death Ball fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Death Ball.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Death Ball.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Death Ball.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 20);
	ch->train += 20;
	ch->masterydeath_ball += 1;
	ch->energymastery += 6;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_eye_beam(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];
  int AT_AURACOLOR = ch->pcdata->auraColorPowerUp;

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skilleye_beam < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryeye_beam / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 12;
  adjcost = basecost;
  lowdam = 120;
  highdam = 140;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_AURACOLOR,"Your eyes glow brightly, firing two beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing two beams directly at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing two beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Eye Beams fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->eye_beampower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_AURACOLOR,"Your eyes glow brightly, firing a barrage of beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing a barrage of beams directly at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing a barrage of beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Eye Beams fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->eye_beampower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_AURACOLOR,"Your eyes glow brightly, firing an unending barrage of beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing an unending barrage of beams directly at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing an unending barrage of beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Eye Beams fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->eye_beampower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_AURACOLOR,"Your eyes glow brightly, firing an unending barrage of beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing an unending barrage of beams directly at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, firing an unending barrage of beams directly at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Eye Beams fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->eye_beampower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_AURACOLOR,"Your eyes glow brightly, lighting up $N with blast after blast from your explosive gaze! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, lighting you up with blast after blast under &s explosive gaze! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n's eyes glow brightly, lighting up $N with blast after blast from $s explosive gaze! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Eye Beams fizzle into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Eye Beams fizzle into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Eye Beams.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Eye Beams.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Eye Beams.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 10);
	ch->train += 10;
	ch->masteryeye_beam += 1;
	ch->energymastery += 3;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_finger_beam(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];
  int AT_AURACOLOR = ch->pcdata->auraColorPowerUp;

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillfinger_beam < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryfinger_beam / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 75;
  adjcost = basecost;
  lowdam = 600;
  highdam = 650;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_AURACOLOR,"You point your hand toward $N, firing a deadly beam of energy from the tip of your finger. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n points $s hand toward you, firing a deadly beam of energy from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n points $s hand toward $N, firing a deadly beam of enery from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Finger Beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->finger_beampower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_AURACOLOR,"You point your hand toward $N, firing a deadly beam of energy from the tip of your finger. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n points $s hand toward you, firing a deadly beam of energy from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n points $s hand toward $N, firing a deadly beam of enery from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Finger Beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->finger_beampower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_AURACOLOR,"You point your hand toward $N, firing a deadly beam of energy from the tip of your finger. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n points $s hand toward you, firing a deadly beam of energy from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n points $s hand toward $N, firing a deadly beam of enery from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Finger Beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->finger_beampower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_AURACOLOR,"You point your hand toward $N, firing a deadly beam of energy from the tip of your finger. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n points $s hand toward you, firing a deadly beam of energy from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n points $s hand toward $N, firing a deadly beam of enery from the tip of $s finger. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Finger Beam fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Finger Beam fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->finger_beampower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_AURACOLOR,"You point your hand effortlessly toward $N, erupting the landscape into a supermassive explosion! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n points $s hand effortlessly toward you, engulfing you in a supermassive eruption! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n points $s hand effortlessly toward $N, erupting the landscape into a supermassive explosion! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your explosion fizzles into nothing before causing any real damage!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's explosion fizzles into nothing before causing any real damage!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's explosion fizzles into nothing before causing any real damage!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Finger Beam.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Finger Beam.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Finger Beam.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 16);
	ch->train += 16;
	ch->masteryfinger_beam += 1;
	ch->energymastery += 5;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

/*void do_tribeam(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_tribeam))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_tribeam]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_tribeam]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_tribeam]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_tribeam]->focus;

  WAIT_STATE(ch, skill_table[gsn_tribeam]->beats);
  if (can_use_skill(ch, number_percent(), gsn_tribeam)) {
    
                   dam = get_attmod(ch, victim) * number_range( 34, 40 );
                 
    dam = get_attmod(ch, victim) * (number_range(52, 56) + (get_curr_int(ch) / 20));
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);
    	act( AT_SKILL, "You form your fingers into a triangle, spotting $N in your sights before pummeling $M with a powerfull beam of inner energy. &W[$t]", ch, num_punct(dam), victim, TO_CHAR );
     	act( AT_SKILL, "$n forms $s fingers into a triangle, spotting you in $s sights before pummeling you with a powerfull beam of inner energy. &W[$t]", ch, num_punct(dam), victim, TO_VICT );
     	act( AT_SKILL, "$n forms $s fingers into a triangle, spotting $N in $s sights before pummeling $M with a powerfull beam of inner energy. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT );

    act(AT_YELLOW,
        "You form your fingers and thumbs into a triangle, "
        "lining up $N in your sights.",
        ch, NULL, victim, TO_CHAR);
    act(AT_YELLOW,
        "Powerful inner energy gathers in the space between your hands, "
        "blasting out to pummel $N. &W[$t]",
        ch, num_punct(dam),
        victim, TO_CHAR);
    act(AT_YELLOW,
        "$n forms $s fingers and thumbs into a triangle, "
        "lining you up in $s sights.",
        ch, NULL, victim, TO_VICT);
    act(AT_YELLOW,
        "Powerful inner energy gathers in the space between $s hands, "
        "blasting out to pummel you. &W[$t]",
        ch, num_punct(dam),
        victim, TO_VICT);
    act(AT_YELLOW,
        "$n forms $s fingers and thumbs into a triangle, "
        "lining up $N in $s sights.",
        ch, NULL, victim, TO_NOTVICT);
    act(AT_YELLOW,
        "Powerful inner energy gathers in the space between $s hands, "
        "blasting out to pummel $N. &W[$t]",
        ch, num_punct(dam),
        victim, TO_NOTVICT);

    learn_from_success(ch, gsn_tribeam);
    global_retcode = damage(ch, victim, dam, TYPE_HIT);
    if (!IS_NPC(ch)) {
      stat_train(ch, "int", 12);
      ch->train += 3;
    }
  } else {
    act(AT_YELLOW, "You missed $N with your tri-beam attack.", ch,
        NULL, victim, TO_CHAR);
    act(AT_YELLOW, "$n misses you with $s tri-beam attack.", ch,
        NULL, victim, TO_VICT);
    act(AT_YELLOW, "$n missed $N with a tri-beam attack.", ch, NULL,
        victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_tribeam);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  ch->mana -= skill_table[gsn_tribeam]->min_mana;
  return;
}*/

/*void do_solar_flare(CHAR_DATA *ch, char *argument) {
  AFFECT_DATA af;
  CHAR_DATA *vch, *vch_next;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_solar_flare))
      return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_solar_flare]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (xIS_SET(ch->in_room->room_flags, ROOM_ARENA) || xIS_SET(ch->in_room->room_flags, ROOM_SAFE)) {
    send_to_char("There's no need to use that here.\n\r", ch);
    return;
  }
  if (who_fighting(ch) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_solar_flare]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_solar_flare]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_solar_flare]->focus;

  WAIT_STATE(ch, skill_table[gsn_solar_flare]->beats);

  if (can_use_skill(ch, number_percent(), gsn_solar_flare)) {
    act(AT_WHITE,
        "You put your hands to your face and yell out 'SOLAR FLARE!' emitting a blinding flash of light.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_WHITE,
        "$n puts $s hands to $s face and yells out 'SOLAR FLARE!' blinding everyone in the room!",
        ch, NULL, NULL, TO_NOTVICT);

    learn_from_success(ch, gsn_solar_flare);
    if (!IS_NPC(ch))
      ch->train += 2;
    for (vch = ch->in_room->first_person; vch; vch = vch_next) {
      vch_next = vch->next_in_room;
      if (!IS_AWAKE(vch))
        continue;

      if (number_range(1, 100) < (get_curr_int(vch) / 10)) {
        send_to_char(
            "You manage to look away at the last moment and shield "
            "your eyes.\n\r",
            vch);
        continue;
      }
      if (ch != vch) {
        af.type = gsn_solar_flare;
        af.duration = 10;
        af.location = APPLY_HITROLL;
        af.modifier = -6;
        af.bitvector = meb(AFF_BLIND);
        affect_to_char(vch, &af);
      }
    }

  } else {
    act(AT_WHITE,
        "You put your hands to your face but forget the words.  Nothing happens.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_WHITE,
        "$n raises $s hands to $s head, gets a stupid look on $s face, then stands there.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_solar_flare);
  }
  ch->mana -= skill_table[gsn_solar_flare]->min_mana;
  return;
}*/

/*void do_hyper(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  if (IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
  }
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_hyper))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_str(ch) < 300) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (get_curr_int(ch) < 300) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (get_curr_dex(ch) < 100) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (get_curr_con(ch) < 800) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_HYPER)) {
    send_to_char("You power down and return to normal.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_HYPER);
    ch->pl = ch->exp;
    transStatRemove(ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_EXTREME)) {
    send_to_char("You can't use the hyper technique while using extreme.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You can't use the hyper technique while using kaioken.\n\r",
                 ch);
    return;
  }
  if (ch->mana < skill_table[gsn_hyper]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  sh_int z = get_aura(ch);

  WAIT_STATE(ch, skill_table[gsn_hyper]->beats);
  if (can_use_skill(ch, number_percent(), gsn_hyper)) {
    act(z,
        "You clench your fists and yell out as all your muscles bulge and your aura explodes outward.",
        ch, NULL, NULL, TO_CHAR);
    act(z,
        "$n clenches $s fists and yells out as all of $s muscles bulge and $s aura explodes outward.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_HYPER);
    if (xIS_SET((ch)->affected_by, AFF_HEART))
      xREMOVE_BIT(ch->affected_by, AFF_HEART);
    ch->pl = ch->exp * 50;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 8);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    heart_calc(ch, "");
    learn_from_success(ch, gsn_hyper);
  } else {
    act(z,
        "You clench your fists and yell out, but nothing happens.",
        ch, NULL, NULL, TO_CHAR);
    act(z,
        "$n clenchs $s fists and yells out, but nothing happens.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_hyper);
  }

  ch->mana -= skill_table[gsn_hyper]->min_mana;
  return;
}*/

/*void do_instant_trans(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  CHAR_DATA *fch;
  CHAR_DATA *fch_next;
  ROOM_INDEX_DATA *prev_room;
  int hitcheck = 0;

  if (!IS_NPC(ch) && (ch->skillinstant_trans < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (ch->mana < 5000) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (IS_NPC(ch))
    return;

  if ((is_android(ch) || is_superandroid(ch)) &&
      !wearing_sentient_chip(ch)) {
    ch_printf(ch, "Huh?\n\r");
    return;
  }
  if (ch->fighting) {
    send_to_char("You can't find enough time to concentrate.\n\r",
                 ch);
    return;
  }
  if (xIS_SET(ch->in_room->room_flags, ROOM_ASTRALSHIELD)) {
    ch_printf(ch,
              "&RThis room is blocking your ability to de-materialize.\n\r");
    return;
  }
  WAIT_STATE(ch, 8);

  if ((victim = get_char_ki_world(ch, argument)) == NULL || !can_astral(ch, victim) || !in_hard_range(ch, victim->in_room->area) || !in_soft_range(ch, victim->in_room->area) || xIS_SET(victim->in_room->room_flags, ROOM_PROTOTYPE) || (is_android(victim) && !wearing_sentient_chip(victim)) || (is_superandroid(victim) && !wearing_sentient_chip(victim)) || IS_IMMORTAL(victim)) {
    send_to_char("You can't seem to sense their energy anywhere.\n\r", ch);
    return;
  }
  if (is_split(victim)) {
    send_to_char("You can't seem to sense their energy anywhere.\n\r", ch);
    return;
  }
  if (!IS_NPC(victim) && !victim->desc) {
    send_to_char("You cannot IT to someone who has lost link.\n\r",
                 ch);
    return;
  }
  if (!victim->in_room) {
    ch_printf(ch, "You can't IT to them at the moment.\n\r");
    return;
  }
  if (ch->in_room == victim->in_room) {
    ch_printf(ch, "You're already there!\n\r");
    return;
  }
  if (victim->master == ch) {
    ch_printf(ch,
              "You can't IT to someone that is following you!\n\r");
    return;
  }
  hitcheck = number_range(1, 100);
  if (hitcheck <= 95) {
    act(AT_MAGIC, "You disappear in a flash of light!", ch, NULL,
        NULL, TO_CHAR);
    act(AT_MAGIC, "$n disappears in a flash of light!", ch, NULL,
        NULL, TO_ROOM);
    prev_room = ch->in_room;
    char_from_room(ch);
    if (ch->mount) {
      char_from_room(ch->mount);
      char_to_room(ch->mount, victim->in_room);
    }
    char_to_room(ch, victim->in_room);
    for (fch = prev_room->first_person; fch; fch = fch_next) {
      fch_next = fch->next_in_room;
      if ((fch != ch) && fch->master && fch->master == ch) {
        char_from_room(fch);
        act(AT_ACTION,
            "You disappear in a flash of light!",
            fch, NULL, ch, TO_CHAR);
        act(AT_ACTION,
            "$n disappears in a flash of light!",
            fch, NULL, ch, TO_ROOM);
        char_to_room(fch, victim->in_room);
        act(AT_MAGIC, "You appear in a flash of light!",
            fch, NULL, ch, TO_CHAR);
        act(AT_MAGIC, "$n appears in a flash of light!",
            fch, NULL, ch, TO_ROOM);
        do_look(fch, "auto");
      }
    }
    act(AT_MAGIC, "You appear in a flash of light!", ch, NULL, NULL,
        TO_CHAR);
    act(AT_MAGIC, "$n appears in a flash of light!", ch, NULL, NULL,
        TO_ROOM);
    do_look(ch, "auto");
  } else {
    send_to_char("&BYou disappear momentarily, but reappear in the same spot, failing to control your energy.\n\r",
                 ch);
  }
  if (!is_android_h(ch)) {
    if (is_kaio(ch))
      ch->mana -=
          (5000 / 5);
    else
      ch->mana -= 5000;
  }
  return;
}*/

ch_ret
spell_sensu_bean(int sn, int level, CHAR_DATA *ch, void *vo) {
  ch->hit = ch->max_hit;
  ch->mana += ch->max_mana * 0.25;
  ch->mana = URANGE(0, ch->mana, ch->max_mana);
  act(AT_MAGIC, "A warm feeling fills your body.", ch, NULL, NULL,
      TO_CHAR);
  return rNONE;
}

ch_ret
spell_sleep(int sn, int level, CHAR_DATA *ch, void *vo) {
  AFFECT_DATA af;

  af.type = gsn_sleep;
  af.duration = number_range(1, 20);
  af.location = APPLY_NONE;
  af.bitvector = meb(AFF_SLEEP);
  af.modifier = -1;

  affect_to_char(ch, &af);
  act(AT_MAGIC, "An overwhelming urge to sleep washes over you.", ch,
      NULL, NULL, TO_CHAR);
  return rNONE;
}

ch_ret
spell_poison(int sn, int level, CHAR_DATA *ch, void *vo) {
  act(AT_MAGIC, "Your stomach aches and you feel ill.", ch, NULL, NULL,
      TO_CHAR);
  return rNONE;
}

ch_ret
spell_paralyze(int sn, int level, CHAR_DATA *ch, void *vo) {
  act(AT_MAGIC, "Your limbs begin to act sluggishly.", ch, NULL, NULL,
      TO_CHAR);
  return rNONE;
}

/*void do_ki_heal(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  char arg[MAX_STRING_LENGTH];
  int healTo = 100;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_ki_heal))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_ki_heal]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  argument = one_argument(argument, arg);

  if (arg[0] == '\0') {
    send_to_char(
        "Syntax: heal <target> [<amount>]\n\r"
        "    or: heal <target> full\n\r",
        ch);
    return;
  }
  if ((victim = get_char_room(ch, arg)) == NULL) {
    send_to_char("I see no one here with that name.\n\r", ch);
    return;
  }
  if (is_android(victim) || is_superandroid(victim)) {
    send_to_char("You can't heal a being without Ki.\n\r", ch);
    return;
  }
  if (victim->hit >= victim->max_hit) {
    send_to_char("There's no need to heal them.\n\r", ch);
    return;
  }
  if (argument[0] != '\0') {
    if (strcmp(argument, "full")) {
      healTo = victim->max_hit - victim->hit;
    }
    if (atoi(argument) > 0 && atoi(argument) < 100) {
      healTo = atoi(argument);
    }
  }
  if (healTo > (victim->max_hit - victim->hit))
    healTo = victim->max_hit - victim->hit;

  if (!IS_NPC(ch) && (ch->pcdata->learned[gsn_ki_heal] < 95) && (healTo > ch->pcdata->learned[gsn_ki_heal])) {
    send_to_char("You aren't skilled enough to heal that amount.\n\r", ch);
    return;
  }
  if (ch->mana < (healTo * skill_table[gsn_ki_heal]->min_mana)) {
    send_to_char("You don't have enough energy to heal anyone.\n\r",
                 ch);
    return;
  }
  if (who_fighting(victim) != NULL) {
    send_to_char("You'll have to wait until they aren't fighting.\n\r", ch);
    return;
  }
  if (who_fighting(ch) != NULL) {
    send_to_char("You're a bit busy to do that right now.\n\r", ch);
    return;
  }
  if (victim == ch) {
    send_to_char("You can't heal yourself.\n\r", ch);
    return;
  }
  if (can_use_skill(ch, number_percent(), gsn_ki_heal)) {
    act(AT_YELLOW,
        "You lay your palms upon $N's wounds, a soft glow forming as you use your energy to heal $M.",
        ch, NULL, victim, TO_CHAR);
    act(AT_YELLOW,
        "$n lays $s palms upon your wounds, a soft glow forming as $e uses $s energy to heal you.",
        ch, NULL, victim, TO_VICT);
    act(AT_YELLOW,
        "$n lays $s palms upon $N's wounds, a soft glow forming as $e uses $s energy to heal $N.",
        ch, NULL, victim, TO_NOTVICT);

    victim->hit += healTo;
    if (victim->hit > victim->max_hit) {
      victim->hit = victim->max_hit;
    }
    learn_from_success(ch, gsn_ki_heal);
    ch->train += 6;
    ch->mana -= (healTo * skill_table[gsn_ki_heal]->min_mana);
  } else {
    act(AT_YELLOW,
        "You lay your palms upon $N's wounds, but nothing happens.",
        ch, NULL, victim, TO_CHAR);
    act(AT_YELLOW,
        "$n lays $s plams upon your wounds, but nothing happens.",
        ch, NULL, victim, TO_VICT);
    act(AT_YELLOW,
        "$n lays $s palms upon $N's wounds, but nothing happens.",
        ch, NULL, victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_ki_heal);
    ch->mana -= (healTo * skill_table[gsn_ki_heal]->min_mana) / 2;
  }
}*/

/*void do_growth(CHAR_DATA *ch, char *argument) {
  int strMod = 10;
  int conMod = 5;
  int intMod = 5;
  int spdMod = -10;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_growth))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_growth]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_GROWTH) &&
      !xIS_SET((ch)->affected_by, AFF_GIANT)) {
    act(AT_SKILL, "You shrink and return to your original size.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL, "$n shrinks and returns to $s original size.", ch,
        NULL, NULL, TO_NOTVICT);
    transStatRemove(ch);
    xREMOVE_BIT((ch)->affected_by, AFF_GROWTH);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_GROWTH) &&
      xIS_SET((ch)->affected_by, AFF_GIANT)) {
    send_to_char("You must revert back from giant size before doing this.\n\r",
                 ch);
    return;
  }
  if (ch->mana < skill_table[gsn_growth]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (can_use_skill(ch, number_percent(), gsn_growth)) {
    act(AT_SKILL,
        "Your body surges with power, suddenly expanding and growing to at least twice your normal size.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n's body surges with power, suddenly expanding and growing to at least twice $s normal size.",
        ch, NULL, NULL, TO_NOTVICT);

    learn_from_success(ch, gsn_growth);
    xSET_BIT((ch)->affected_by, AFF_GROWTH);
    transStatApply(ch, strMod, spdMod, intMod, conMod);
  } else {
    act(AT_SKILL,
        "You expand slightly before shrinking back to your normal size and stature.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n expands slightly before shrinking back to $s normal size and stature.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_growth);
  }
  ch->mana -= skill_table[gsn_growth]->min_mana;
}*/

/*void do_giant_size(CHAR_DATA *ch, char *argument) {
  int strMod = 25;
  int conMod = 15;
  int intMod = 10;
  int spdMod = -25;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_giant))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_giant]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_GROWTH) &&
      xIS_SET((ch)->affected_by, AFF_GIANT)) {
    act(AT_SKILL, "You shrink and return to your original size.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL, "$n shrinks and returns to $s original size.", ch,
        NULL, NULL, TO_NOTVICT);
    transStatRemove(ch);
    xREMOVE_BIT((ch)->affected_by, AFF_GIANT);
    xREMOVE_BIT((ch)->affected_by, AFF_GROWTH);
    return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_GROWTH)) {
    send_to_char("You must first grow before you can use this.\n\r",
                 ch);
    return;
  }
  if (ch->mana < skill_table[gsn_giant]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (can_use_skill(ch, number_percent(), gsn_giant)) {
    transStatRemove(ch);
    act(AT_SKILL,
        "You expand even further than before, transforming yourself into a Namekian giant.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n expands even further than before, transforming $mself into a Namekian giant.",
        ch, NULL, NULL, TO_NOTVICT);

    learn_from_success(ch, gsn_giant);
    ch->mana -= skill_table[gsn_giant]->min_mana;
    xSET_BIT((ch)->affected_by, AFF_GIANT);
    transStatApply(ch, strMod, spdMod, intMod, conMod);
  } else {
    act(AT_SKILL,
        "Your body fills with energy but you can't seem to grow any larger.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n fills $s body with energy but can't seem to grow any larger.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_giant);
    ch->mana -= skill_table[gsn_giant]->min_mana;
  }
}*/

void do_split_form(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  MOB_INDEX_DATA *pMobIndex;

  if (IS_NPC(ch)) {
    send_to_char("NPC's can't do that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
    send_to_char("This ability is currently disabled.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_split_form]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (IS_IMMORTAL(ch) && get_trust(ch) < 59) {
    send_to_char("You can't split-form.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU)) {
    ch_printf(ch, "You can't use this while in Oozaru!\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    ch_printf(ch, "You can't use this while in Golden Oozaru!\n\r");
    return;
  }
  if (IS_NPC(ch) && ch->pIndexData->vnum == 610) {
    send_to_char("You aren't allowed to do that.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_BIOJR)) {
    ch_printf(ch,
              "You can't use this skill while you have clones.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SPLIT_FORM)) {
    reuniteSplitForms(ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_TRI_FORM) ||
      xIS_SET((ch)->affected_by, AFF_MULTI_FORM)) {
    reuniteSplitForms(ch);
    return;
  }
  if (ch->mana < skill_table[gsn_split_form]->min_mana) {
    pager_printf_color(ch,
                       "You must have at least %s energy to split.\n\r",
                       num_punct(skill_table
                                     [gsn_split_form]
                                         ->min_mana));
    return;
  }
  if ((pMobIndex = get_mob_index(610)) == NULL) {
    bug("do_split_form: mob does not exist");
    send_to_char("Hmm... Something went wrong...\n\r", ch);
    return;
  }
  victim = create_mobile(pMobIndex);
  char_to_room(victim, ch->in_room);

  if (can_use_skill(ch, number_percent(), gsn_split_form)) {
    statsToMob(ch, victim, gsn_split_form, false, 0, 1);
    act(AT_SKILL,
        "Powering up, your body begins to separate and with a booming yell you create a double of your self!",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n powers up as $s body begins to separate and with a booming yell two $n's now exist!",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_success(ch, gsn_split_form);
  } else {
    statsToMob(ch, victim, gsn_split_form, true, 0, 1);
    act(AT_SKILL,
        "You begin to create a double of your self, but just before the final separation you lose focus and something goes wrong.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n begins to create a double of $s self, but just before the final separation $e loses focus and something goes wrong.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_split_form);
  }

  ch->mana = ch->mana / 2;
  add_follower(victim, ch);
  victim->master = ch;
  victim->leader = ch;
  ch->leader = ch;
  xSET_BIT(ch->affected_by, AFF_SPLIT_FORM);
  xSET_BIT(victim->affected_by, AFF_SPLIT_FORM);

  return;
}

void do_tri_form(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  CHAR_DATA *victim2;
  MOB_INDEX_DATA *pMobIndex;

  if (IS_NPC(ch)) {
    send_to_char("NPC's can't do that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
    send_to_char("This ability is currently disabled.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_tri_form]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (IS_IMMORTAL(ch) && get_trust(ch) < 59) {
    send_to_char("You can't tri-form.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    ch_printf(ch, "You can't use this while in Oozaru!\n\r");
    return;
  }
  if (IS_NPC(ch) && ch->pIndexData->vnum == 610) {
    send_to_char("You aren't allowed to do that.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_BIOJR)) {
    ch_printf(ch,
              "You can't use this skill while you have clones.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_TRI_FORM)) {
    reuniteSplitForms(ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SPLIT_FORM) ||
      xIS_SET((ch)->affected_by, AFF_MULTI_FORM)) {
    reuniteSplitForms(ch);
    return;
  }
  if (ch->mana < skill_table[gsn_tri_form]->min_mana) {
    pager_printf_color(ch,
                       "You must have at least %s energy to tri-form.\n\r",
                       num_punct(skill_table
                                     [gsn_tri_form]
                                         ->min_mana));
    return;
  }
  if ((pMobIndex = get_mob_index(610)) == NULL) {
    bug("do_tri_form: mob does not exist");
    send_to_char("Hmm... Something went wrong...\n\r", ch);
    return;
  }
  victim = create_mobile(pMobIndex);
  char_to_room(victim, ch->in_room);

  victim2 = create_mobile(pMobIndex);
  char_to_room(victim2, ch->in_room);

  if (can_use_skill(ch, number_percent(), gsn_tri_form)) {
    statsToMob(ch, victim, gsn_tri_form, false, 0, 1);
    statsToMob(ch, victim2, gsn_tri_form, false, 0, 2);
    act(AT_YELLOW,
        "Powering up greatly, your body starts glowing bright gold as your chest and back begin to bulge. You scream out as two copies of yourself burst forth from your body and stand next to you, prepared to fight.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_YELLOW,
        "$n powers up greatly; $s body starts glowing bright gold as $s chest and back begin to bulge. $n screams out as two copies of $mself bursts forth from $s body and stand next to $m, prepared to fight.",
        ch, NULL, NULL, TO_NOTVICT);

    learn_from_success(ch, gsn_tri_form);
  } else {
    statsToMob(ch, victim, gsn_tri_form, true, 0, 1);
    statsToMob(ch, victim2, gsn_tri_form, true, 0, 2);
    act(AT_YELLOW,
        "You begin to create two copies of yourself, but just before the final separation you lose focus and something goes wrong.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_YELLOW,
        "$n begins to create two doubles of $mself, but just before the final separation $e loses focus and something goes wrong.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_tri_form);
  }

  ch->mana = ch->mana / 2;
  add_follower(victim, ch);
  add_follower(victim2, ch);
  victim->master = ch;
  victim->leader = ch;
  victim2->master = ch;
  victim2->leader = ch;
  ch->leader = ch;
  xSET_BIT(ch->affected_by, AFF_TRI_FORM);
  xSET_BIT(victim->affected_by, AFF_TRI_FORM);
  xSET_BIT(victim2->affected_by, AFF_TRI_FORM);

  return;
}

void do_multi_form(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  CHAR_DATA *victim2;
  CHAR_DATA *victim3;
  MOB_INDEX_DATA *pMobIndex;

  if (IS_NPC(ch)) {
    send_to_char("NPC's can't do that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
    send_to_char("This ability is currently disabled.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_multi_form]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (IS_IMMORTAL(ch) && get_trust(ch) < 59) {
    send_to_char("You can't multi-form.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    ch_printf(ch, "You can't use this while in Oozaru!\n\r");
    return;
  }
  if (IS_NPC(ch) && ch->pIndexData->vnum == 610) {
    send_to_char("You aren't allowed to do that.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_BIOJR)) {
    ch_printf(ch,
              "You can't use this skill while you have clones.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_MULTI_FORM)) {
    reuniteSplitForms(ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SPLIT_FORM) ||
      xIS_SET((ch)->affected_by, AFF_TRI_FORM)) {
    reuniteSplitForms(ch);
    return;
  }
  if (ch->mana < skill_table[gsn_multi_form]->min_mana) {
    pager_printf_color(ch,
                       "You must have at least %s energy to multi-form.\n\r",
                       num_punct(skill_table
                                     [gsn_multi_form]
                                         ->min_mana));
    return;
  }
  if ((pMobIndex = get_mob_index(610)) == NULL) {
    bug("do_multi_form: mob does not exist");
    send_to_char("Hmm... Something went wrong...\n\r", ch);
    return;
  }
  victim = create_mobile(pMobIndex);
  char_to_room(victim, ch->in_room);

  victim2 = create_mobile(pMobIndex);
  char_to_room(victim2, ch->in_room);

  victim3 = create_mobile(pMobIndex);
  char_to_room(victim3, ch->in_room);

  if (can_use_skill(ch, number_percent(), gsn_multi_form)) {
    statsToMob(ch, victim, gsn_multi_form, false, 0, 1);
    statsToMob(ch, victim2, gsn_multi_form, false, 0, 2);
    statsToMob(ch, victim3, gsn_multi_form, false, 0, 3);
    act(AT_WHITE,
        "You cross your arms directly infront of yourself and yell 'MULTI FORM!!'. Your body turns transparent as you suddenly split into two images that move away from each other till they are a couple feet away. Then the two split, becoming four. Each copy of you, including yourself, becomes opaque again. All four of you get prepared to fight.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_WHITE,
        "$n crosses $s arms directly infront of $mself and yells 'MULTI FORM!!'. $*s body turns transparent as $e suddenly splits into two images that move away from each other till they are a couple feet away. Then the two split, becoming four. Each copy of $n, including $mself, becomes opaque again. All four of $n look prepared to fight.",
        ch, NULL, NULL, TO_NOTVICT);

    learn_from_success(ch, gsn_multi_form);
  } else {
    statsToMob(ch, victim, gsn_multi_form, true, 0, 1);
    statsToMob(ch, victim2, gsn_multi_form, true, 0, 2);
    statsToMob(ch, victim3, gsn_multi_form, true, 0, 3);
    act(AT_WHITE,
        "You begin to create three copies of yourself, but just before the final separation you lose focus and something goes wrong.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_WHITE,
        "$n begins to create three copies of $mself, but just before the final separation $e loses focus and something goes wrong.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_multi_form);
  }

  ch->mana = ch->mana / 2;
  add_follower(victim, ch);
  add_follower(victim2, ch);
  add_follower(victim3, ch);
  victim->master = ch;
  victim->leader = ch;
  victim2->master = ch;
  victim2->leader = ch;
  victim3->master = ch;
  victim3->leader = ch;
  ch->leader = ch;
  xSET_BIT(ch->affected_by, AFF_MULTI_FORM);
  xSET_BIT(victim->affected_by, AFF_MULTI_FORM);
  xSET_BIT(victim2->affected_by, AFF_MULTI_FORM);
  xSET_BIT(victim3->affected_by, AFF_MULTI_FORM);

  return;
}

void do_sanctuary(CHAR_DATA *ch, char *argument) {
  if (IS_NPC(ch))
    return;

  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (ch->kairank < 1 && ch->demonrank < 3) {
    ch_printf(ch, "Huh?\n\r");
    return;
  }
  if (ch->mana < 5000) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (!IS_AFFECTED(ch, AFF_SANCTUARY)) {
    if (IS_GOOD(ch)) {
      act(AT_WHITE,
          "You relax and concentrate your supernatural powers. A soft white glow begins to radiate from your body, exploding into an aura of divine protection.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE,
          "$n relaxes and concentrates $s supernatural powers. A soft white glow begins to radiate from $n's body, exploding into an aura of divine protection.",
          ch, NULL, NULL, TO_NOTVICT);
    } else if (IS_NEUTRAL(ch)) {
      act(AT_GREY,
          "You relax and concentrate your incredible powers. A soft glow begins to radiate from your body, exploding into an aura of protection.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_GREY,
          "$n relaxes and concentrates $s incredible powers. A soft glow begins to radiate from $n's body, exploding into an aura of protection.",
          ch, NULL, NULL, TO_NOTVICT);
    } else if (IS_EVIL(ch)) {
      act(AT_DGREY,
          "You relax and concentrate your demonic powers. A soft black glow begins to radiate from your body, exploding into an aura of evil protection.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_DGREY,
          "$n relaxes and concentrates $s demonic powers. A soft black glow begins to radiate from $n's body, exploding into an aura of evil protection.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    xSET_BIT((ch)->affected_by, AFF_SANCTUARY);
  } else {
    if (IS_GOOD(ch)) {
      act(AT_WHITE,
          "The aura of divine protection around you breaks apart and vanishes into thin air.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_WHITE,
          "The aura of divine protection around $n breaks apart and vanishes into thin air.",
          ch, NULL, NULL, TO_NOTVICT);
    } else if (IS_NEUTRAL(ch)) {
      act(AT_GREY,
          "The aura of protection around you breaks apart and vanishes into thin air.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_GREY,
          "The aura of protection around $n breaks apart and vanishes into thin air.",
          ch, NULL, NULL, TO_NOTVICT);
    } else if (IS_EVIL(ch)) {
      act(AT_DGREY,
          "The aura of evil protection around you breaks apart and vanishes into thin air.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_DGREY,
          "The aura of evil protection around $n breaks apart and vanishes into thin air.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    xREMOVE_BIT((ch)->affected_by, AFF_SANCTUARY);
  }
  ch->mana -= 5000;
  save_char_obj(ch);
  return;
}

void do_clone(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  CHAR_DATA *victim2;
  CHAR_DATA *victim3;
  CHAR_DATA *victim4;
  CHAR_DATA *victim5;
  CHAR_DATA *victim6 = NULL;
  CHAR_DATA *victim7 = NULL;
  CHAR_DATA *victim8 = NULL;
  CHAR_DATA *victim9 = NULL;
  CHAR_DATA *victim10 = NULL;
  MOB_INDEX_DATA *pMobIndex;
  char arg[MAX_INPUT_LENGTH];
  int count = 0;

  argument = one_argument(argument, arg);

  if (IS_NPC(ch)) {
    send_to_char("NPC's can't do that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
    send_to_char("This ability is currently disabled.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_clone]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (IS_IMMORTAL(ch) && get_trust(ch) < 59) {
    send_to_char("You can't clone.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_OOZARU) ||
      xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    ch_printf(ch, "You can't use this while in Oozaru!\n\r");
    return;
  }
  if (IS_NPC(ch) && ch->pIndexData->vnum == 610) {
    send_to_char("You aren't allowed to do that.\n\r", ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_BIOJR)) {
    reuniteSplitForms(ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_SPLIT_FORM) ||
      xIS_SET((ch)->affected_by, AFF_TRI_FORM) ||
      xIS_SET((ch)->affected_by, AFF_MULTI_FORM)) {
    ch_printf(ch,
              "You can't create clones while seperated into multiple parts.\n\r");
    return;
  }
  count = atoi(arg);

  if (count < 5 || count > 10) {
    ch_printf(ch,
              "The amount of clones has to be between 5 and 10.\n\r");
    return;
  }
  if (count != 5) {
    ch_printf(ch,
              "Clone is currently restricted to a maximum of 5.\n\r");
    return;
  }
  if (ch->mana < (skill_table[gsn_clone]->min_mana * count)) {
    pager_printf_color(ch,
                       "You must have at least %s energy to create %d clones.\n\r",
                       num_punct((skill_table[gsn_clone]->min_mana *
                                  count)),
                       count);
    return;
  }
  if ((pMobIndex = get_mob_index(610)) == NULL) {
    bug("do_clone: mob does not exist");
    send_to_char("Hmm... Something went wrong...\n\r", ch);
    return;
  }
  victim = create_mobile(pMobIndex);
  char_to_room(victim, ch->in_room);
  victim2 = create_mobile(pMobIndex);
  char_to_room(victim2, ch->in_room);
  victim3 = create_mobile(pMobIndex);
  char_to_room(victim3, ch->in_room);
  victim4 = create_mobile(pMobIndex);
  char_to_room(victim4, ch->in_room);
  victim5 = create_mobile(pMobIndex);
  char_to_room(victim5, ch->in_room);
  if (count > 5) {
    victim6 = create_mobile(pMobIndex);
    char_to_room(victim6, ch->in_room);
  }
  if (count > 6) {
    victim7 = create_mobile(pMobIndex);
    char_to_room(victim7, ch->in_room);
  }
  if (count > 7) {
    victim8 = create_mobile(pMobIndex);
    char_to_room(victim8, ch->in_room);
  }
  if (count > 8) {
    victim9 = create_mobile(pMobIndex);
    char_to_room(victim9, ch->in_room);
  }
  if (count > 9) {
    victim10 = create_mobile(pMobIndex);
    char_to_room(victim10, ch->in_room);
  }
  if (can_use_skill(ch, number_percent(), gsn_clone)) {
    statsToMob(ch, victim, gsn_clone, false, count, 1);
    statsToMob(ch, victim2, gsn_clone, false, count, 2);
    statsToMob(ch, victim3, gsn_clone, false, count, 3);
    statsToMob(ch, victim4, gsn_clone, false, count, 4);
    statsToMob(ch, victim5, gsn_clone, false, count, 5);
    if (count > 5)
      statsToMob(ch, victim6, gsn_clone, false, count, 6);
    if (count > 6)
      statsToMob(ch, victim7, gsn_clone, false, count, 7);
    if (count > 7)
      statsToMob(ch, victim8, gsn_clone, false, count, 8);
    if (count > 8)
      statsToMob(ch, victim9, gsn_clone, false, count, 9);
    if (count > 9)
      statsToMob(ch, victim10, gsn_clone, false, count, 10);
    act(AT_DGREEN,
        "You clench your fists and begin to charge your power into the center of your body. You arch back as you open up your tail in your back. Suddenly, you start spitting out many small clones of yourself through your tail. After a moment, they all get onto their feet and look prepared to fight.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_DGREEN,
        "$n clenches $s fists and begins to charge $s power into the center of $s body. $*e arches back as $e opens up $s tail in $s back. Suddenly, $n starts spitting out many small clones of $mself through $s tail. After a moment, they all get onto their feet and look prepared to fight.",
        ch, NULL, NULL, TO_NOTVICT);

    learn_from_success(ch, gsn_clone);
  } else {
    extract_char(victim, true, false);
    extract_char(victim2, true, false);
    extract_char(victim3, true, false);
    extract_char(victim4, true, false);
    extract_char(victim5, true, false);
    if (count > 5)
      extract_char(victim6, true, false);
    if (count > 6)
      extract_char(victim7, true, false);
    if (count > 7)
      extract_char(victim8, true, false);
    if (count > 8)
      extract_char(victim9, true, false);
    if (count > 9)
      extract_char(victim10, true, false);
    act(AT_DGREEN,
        "You try to gather energy to create clones of yourself, but something goes wrong and you fail.",
        ch, NULL, NULL, TO_CHAR);
    act(AT_DGREEN,
        "$n tries to gather energy to create clones of $mself, but something goes wrong and $e fails.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_clone);
    return;
  }

  ch->mana -= (skill_table[gsn_clone]->min_mana * count);
  ch->leader = ch;
  xSET_BIT(ch->affected_by, AFF_BIOJR);

  add_follower(victim, ch);
  victim->master = ch;
  victim->leader = ch;
  xSET_BIT(victim->affected_by, AFF_BIOJR);

  add_follower(victim2, ch);
  victim2->master = ch;
  victim2->leader = ch;
  xSET_BIT(victim2->affected_by, AFF_BIOJR);

  add_follower(victim3, ch);
  victim3->master = ch;
  victim3->leader = ch;
  xSET_BIT(victim3->affected_by, AFF_BIOJR);

  add_follower(victim4, ch);
  victim4->master = ch;
  victim4->leader = ch;
  xSET_BIT(victim4->affected_by, AFF_BIOJR);

  add_follower(victim5, ch);
  victim5->master = ch;
  victim5->leader = ch;
  xSET_BIT(victim5->affected_by, AFF_BIOJR);

  if (count > 5) {
    add_follower(victim6, ch);
    victim6->master = ch;
    victim6->leader = ch;
    xSET_BIT(victim6->affected_by, AFF_BIOJR);
  }
  if (count > 6) {
    add_follower(victim7, ch);
    victim7->master = ch;
    victim7->leader = ch;
    xSET_BIT(victim7->affected_by, AFF_BIOJR);
  }
  if (count > 7) {
    add_follower(victim8, ch);
    victim8->master = ch;
    victim8->leader = ch;
    xSET_BIT(victim8->affected_by, AFF_BIOJR);
  }
  if (count > 8) {
    add_follower(victim9, ch);
    victim9->master = ch;
    victim9->leader = ch;
    xSET_BIT(victim9->affected_by, AFF_BIOJR);
  }
  if (count > 9) {
    add_follower(victim10, ch);
    victim10->master = ch;
    victim10->leader = ch;
    xSET_BIT(victim10->affected_by, AFF_BIOJR);
  }
  return;
}

void do_destructive_wave(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];
  int AT_AURACOLOR = ch->pcdata->auraColorPowerUp;

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skilldestructive_wave < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterydestructive_wave / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 60;
  adjcost = basecost;
  lowdam = 260;
  highdam = 280;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_AURACOLOR,"You steady an arm with your free hand, bracing as you unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->destructive_wavepower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_AURACOLOR,"You steady an arm with your free hand, bracing as you unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->destructive_wavepower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_AURACOLOR,"You steady an arm with your free hand, bracing as you unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->destructive_wavepower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_AURACOLOR,"You steady an arm with your free hand, bracing as you unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, bracing as they unleash a wave of potent energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->destructive_wavepower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_AURACOLOR,"You steady an arm with your free hand, unleashing a wall of enormous energy at $N! &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, unleashing a wall of enormous energy at you! &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n steadies an arm with $s free hand, unleashing a wall of enormous energy at $N! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Destructive Wave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Destructive Wave.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Destructive Wave.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Destructive Wave.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 10);
	ch->train += 10;
	ch->masterydestructive_wave += 1;
	ch->energymastery += 4;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

/*void do_dodon_ray(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_dodon_ray))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_dodon_ray]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_dodon_ray]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_dodon_ray]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_dodon_ray]->focus;

  sh_int z = get_aura(ch);

  WAIT_STATE(ch, skill_table[gsn_dodon_ray]->beats);
  if (can_use_skill(ch, number_percent(), gsn_dodon_ray)) {
    dam = get_attmod(ch, victim) * (number_range(22, 26) + (get_curr_int(ch) / 40));
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);

    act(z,
        "You point your first finger towards $N, thumb pointing upwards, forming a pretend gun.",
        ch, NULL, victim, TO_CHAR);
    act(z,
        "A very real, very bright beam of energy zaps from your extended finger, your hand recoiling as the ray fires. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);
    act(z,
        "$n points $s first finger towards you, thumb pointing upwards, forming a pretend gun.",
        ch, NULL, victim, TO_VICT);
    act(z,
        "A very real, very bright beam of energy zaps from $n's extended finger, $s hand recoiling as the ray fires. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);
    act(z,
        "$n points $s first finger towards $N, thumb pointing upwards, forming a pretend gun.",
        ch, NULL, victim, TO_NOTVICT);
    act(z,
        "A very real, very bright beam of energy zaps from $n's extended finger, $s hand recoiling as the ray fires. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);

    learn_from_success(ch, gsn_dodon_ray);
    global_retcode = damage(ch, victim, dam, TYPE_HIT);
    if (!IS_NPC(ch)) {
      stat_train(ch, "int", 8);
      ch->train += 2;
    }
  } else {
    act(z, "You missed $N with your dodon ray attack.", ch, NULL,
        victim, TO_CHAR);
    act(z, "$n misses you with $s dodon ray attack.", ch, NULL,
        victim, TO_VICT);
    act(z, "$n missed $N with a dodon ray attack.", ch, NULL,
        victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_dodon_ray);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  ch->mana -= skill_table[gsn_dodon_ray]->min_mana;
  return;
}*/

/*void do_spirit_ball(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_spirit_ball))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_spirit_ball]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_spirit_ball]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->focus < skill_table[gsn_spirit_ball]->focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= skill_table[gsn_spirit_ball]->focus;

  WAIT_STATE(ch, skill_table[gsn_spirit_ball]->beats);
  if (can_use_skill(ch, number_percent(), gsn_spirit_ball)) {
    dam = get_attmod(ch, victim) * (number_range(26, 32) + (get_curr_int(ch) / 35));
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);

    act(AT_YELLOW,
        "You grab your wrist tightly with your other hand, concentrating your ki into a brilliant ball of energy that floats above your palm, your fingers curling upwards.",
        ch, NULL, victim, TO_CHAR);
    act(AT_YELLOW,
        "The ball of light zips through the air as you gesture with two fingers, controlling its path as it zigzags wildly, finally smashing into $N with a blinding explosion. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);
    act(AT_YELLOW,
        "$n grabs $s wrist tightly with $s other hand, concentrating $s ki into a brilliant ball of energy that floats above $s palm, $s fingers curling upwards.",
        ch, NULL, victim, TO_VICT);
    act(AT_YELLOW,
        "The ball of light zips through the air as $n gestures with two fingers, controlling its path as it zigzags wildly, finally smashing into you with a blinding explosion. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);
    act(AT_YELLOW,
        "$n grabs $s wrist tightly with $s other hand, concentrating $s ki into a brilliant ball of energy that floats above $s palm, $s fingers curling upwards.",
        ch, NULL, victim, TO_NOTVICT);
    act(AT_YELLOW,
        "The ball of light zips through the air as $n gestures with two fingers, controlling its path as it zigzags wildly, finally smashing into $N with a blinding explosion. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);

    learn_from_success(ch, gsn_spirit_ball);
    global_retcode = damage(ch, victim, dam, TYPE_HIT);
    if (!IS_NPC(ch)) {
      stat_train(ch, "int", 8);
      ch->train += 2;
    }
  } else {
    act(AT_YELLOW, "You missed $N with your spirit ball attack.",
        ch, NULL, victim, TO_CHAR);
    act(AT_YELLOW, "$n misses you with $s spirit ball attack.", ch,
        NULL, victim, TO_VICT);
    act(AT_YELLOW, "$n missed $N with a spirit ball attack.", ch,
        NULL, victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_spirit_ball);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  ch->mana -= skill_table[gsn_spirit_ball]->min_mana;
  return;
}*/

void do_shockwave(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];
  int AT_AURACOLOR = ch->pcdata->auraColorPowerUp;

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillshockwave < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masteryshockwave / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 70;
  adjcost = basecost;
  lowdam = 450;
  highdam = 650;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_AURACOLOR,"You clench your fists and deeply concentrate on focusing your physical and mental powers.", ch, NULL, victim, TO_CHAR);
	  act(AT_AURACOLOR,"You scream out as you throw one arm forward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_VICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Shockwave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->shockwavepower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_AURACOLOR,"You clench your fists and deeply concentrate on focusing your physical and mental powers.", ch, NULL, victim, TO_CHAR);
	  act(AT_AURACOLOR,"You scream out as you throw one arm forward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_VICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Shockwave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->shockwavepower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_AURACOLOR,"You clench your fists and deeply concentrate on focusing your physical and mental powers.", ch, NULL, victim, TO_CHAR);
	  act(AT_AURACOLOR,"You scream out as you throw one arm forward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_VICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Shockwave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->shockwavepower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_AURACOLOR,"You clench your fists and deeply concentrate on focusing your physical and mental powers.", ch, NULL, victim, TO_CHAR);
	  act(AT_AURACOLOR,"You scream out as you throw one arm forward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_VICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws one arm foward, firing a shockwave of invisible energy at $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Shockwave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->shockwavepower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_AURACOLOR,"You clench your fists and deeply concentrate on focusing your physical and mental powers.", ch, NULL, victim, TO_CHAR);
	  act(AT_AURACOLOR,"You scream out as you throw both arms forward, collapsing the atmosphere around $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_VICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws both arms foward, collapsing the atmosphere around you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_AURACOLOR,"$n clenches $s fists and deeply concentrates on focusing $s physical and mental powers.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_AURACOLOR,"$n screams out as $e throws both arms foward, collapsing the atmosphere around $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Shockwave fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Shockwave fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Shockwave.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Shockwave.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Shockwave.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 17);
	ch->train += 17;
	ch->masteryshockwave += 1;
	ch->energymastery += 5;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_gallic_gun(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillgallic_gun < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterygallic_gun / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 125;
  adjcost = basecost;
  lowdam = 700;
  highdam = 750;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_PURPLE,"You pull your hands to one side of your body, a dark purple sphere of energy flaring up suddenly around you.", ch, NULL, victim, TO_CHAR);
	  act(AT_PURPLE,"You thrust both hands towards $N, a beam of ki blasting from your purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_VICT);
	  act(AT_PURPLE,"$n thrusts both hands towards you, a beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_PURPLE,"$n thrusts both hands towards $N, a beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->gallic_gunpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_PURPLE,"You pull your hands to one side of your body, a dark purple sphere of energy flaring up suddenly around you.", ch, NULL, victim, TO_CHAR);
	  act(AT_PURPLE,"You thrust both hands towards $N, a large beam of ki blasting from your purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_VICT);
	  act(AT_PURPLE,"$n thrusts both hands towards you, a large beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_PURPLE,"$n thrusts both hands towards $N, a large beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->gallic_gunpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_PURPLE,"You pull your hands to one side of your body, a dark purple sphere of energy flaring up suddenly around you.", ch, NULL, victim, TO_CHAR);
	  act(AT_PURPLE,"You thrust both hands towards $N, a huge beam of ki blasting from your purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_VICT);
	  act(AT_PURPLE,"$n thrusts both hands towards you, a huge beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_PURPLE,"$n thrusts both hands towards $N, a huge beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->gallic_gunpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_PURPLE,"You pull your hands to one side of your body, a dark purple sphere of energy flaring up suddenly around you.", ch, NULL, victim, TO_CHAR);
	  act(AT_PURPLE,"You thrust both hands towards $N, a massive beam of ki blasting from your purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_VICT);
	  act(AT_PURPLE,"$n thrusts both hands towards you, a massive beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_PURPLE,"$n thrusts both hands towards $N, a massive beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->gallic_gunpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_PURPLE,"You pull your hands to one side of your body, muscles bulging as your aura flares into a swirling rage.", ch, NULL, victim, TO_CHAR);
	  act(AT_PURPLE,"You thrust both hands towards $N, a colossal beam of ki blasting from your purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_VICT);
	  act(AT_PURPLE,"$n thrusts both hands towards you, a colossal beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE!' &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_PURPLE,"$n pulls $s hands to one side of $s body, a dark purple sphere of energy flaring up suddenly around $m.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_PURPLE,"$n thrusts both hands towards $N, a colossal beam of ki blasting from $s purple aura, screaming, 'Gallic Gun ... FIRE! &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Gallic Gun fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Gallic Gun.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Gallic Gun.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Gallic Gun.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 16);
	ch->train += 16;
	ch->masterygallic_gun += 1;
	ch->energymastery += 5;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

void do_makosen(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;
  int argdam = 0;
  float kimult = 0;
  float kicmult = 0;
  int kilimit = 0;
  int hitcheck = 0;
  int adjcost = 0;
  int basecost = 0;
  float smastery = 0;
  int lowdam = 0;
  int highdam = 0;
  char arg[MAX_INPUT_LENGTH];

  argument = one_argument(argument, arg);
  
  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && (ch->skillmakosen < 1)) {
    send_to_char("You're not able to use that skill.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch)) {
	kilimit = ch->train / 10000;
	kimult = (float)get_curr_int(ch) / 1000 + 1;
	kicmult = (float)kilimit / 100 + 1;
	smastery = (float)ch->masterymakosen / 10000;
	if (smastery > 10)
	  smastery = 10;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  basecost = 60;
  adjcost = basecost;
  lowdam = 400;
  highdam = 420;
  hitcheck = number_range(1, 100);
  WAIT_STATE(ch, 8);
  if (hitcheck <= 95) {
	if (arg[0] == '\0') {
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam, highdam) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam, highdam);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  act(AT_YELLOW,"You pull both hands to one side of your body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"'Makosen!'  You thrust your hands in front of you, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Makosen fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "200")) {
	  if (ch->makosenpower < 1) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 2, highdam * 2) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 2, highdam * 2);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 4;
	  act(AT_YELLOW,"You pull both hands to one side of your body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"'Makosen!'  You thrust your hands in front of you, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Makosen fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "300")) {
	  if (ch->makosenpower < 2) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 3, highdam * 3) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 3, highdam * 3);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 16;
	  act(AT_YELLOW,"You pull both hands to one side of your body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"'Makosen!'  You thrust your hands in front of you, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Makosen fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "400")) {
	  if (ch->makosenpower < 3) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 4, highdam * 4) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 4, highdam * 4);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 64;
	  act(AT_YELLOW,"You pull both hands to one side of your body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"'Makosen!'  You thrust your hands in front of you, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Makosen fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else if (!str_cmp(arg, "500")) {
	  if (ch->makosenpower < 4) {
		send_to_char("You lack the skill.\n\r", ch);
		return;
	  }
	  if (!IS_NPC(ch)) {
		argdam = (number_range(lowdam * 5, highdam * 5) * (kicmult + smastery));
		dam = get_attmod(ch, victim) * (argdam * kimult);
	  }
	  else if (IS_NPC(ch)) {
		argdam = number_range(lowdam * 5, highdam * 5);
		dam = get_attmod(ch, victim) * (argdam + (get_curr_int(ch) / 40));
	  }
	  if (dam < 1)
		dam = 1;
	  adjcost = basecost * 256;
	  act(AT_YELLOW,"You pull both hands to one side of your body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_CHAR);
	  act(AT_YELLOW,"'Makosen!'  You thrust your hands in front of you, firing a thin, spiraling beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_CHAR);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_VICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin, spiraling beam of energy towards you. &W[$t]", ch, num_punct(dam), victim, TO_VICT);
	  act(AT_YELLOW,"$n pulls both hands to one side of $s body, cupping them as a ball of ki begins forming.", ch, NULL, victim, TO_NOTVICT);
	  act(AT_YELLOW,"'Makosen!'  $n thrusts $s hands in front of $m, firing a thin, spiraling beam of energy towards $N. &W[$t]", ch, num_punct(dam), victim, TO_NOTVICT);
	  if ((ch->mana - adjcost) < 0) {
		act(AT_LBLUE,"Your Makosen fizzles into nothing!", ch, NULL, victim, TO_CHAR);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_VICT);
		act(AT_LBLUE,"$n's Makosen fizzles into nothing!", ch, NULL, victim, TO_NOTVICT);
		global_retcode = damage(ch, victim, 0, TYPE_HIT);
		ch->mana = 0;
	  } else {
		global_retcode = damage(ch, victim, dam, TYPE_HIT);
	  }
	}
	else {
		send_to_char("Only increments of 200-500.\n\r", ch);
		return;
	}
  }
  else {
	act(AT_LBLUE, "You missed $N with your Makosen.", ch, NULL, victim, TO_CHAR);
	act(AT_LBLUE, "$n misses you with $s Makosen.", ch, NULL, victim, TO_VICT);
	act(AT_LBLUE, "$n missed $N with $s Makosen.", ch, NULL, victim, TO_NOTVICT);
	global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  if (!IS_NPC(ch) && ch->mana != 0) {
	// train player masteries, no benefit from spamming at no ki
	stat_train(ch, "int", 15);
	ch->train += 15;
	ch->masterymakosen += 1;
	ch->energymastery += 4;
	if (ch->energymastery >= (ch->kspgain * 100)) {
	  pager_printf_color(ch,"&CYou gained 5 Skill Points!\n\r");
	  ch->sptotal += 5;
	  ch->kspgain += 1;
	}
  }
  if ((ch->mana - adjcost) < 0)
	ch->mana = 0;
  else
	ch->mana -= adjcost;
  return;
}

/*void do_demonweapon(CHAR_DATA *ch, char *argument) {
  OBJ_DATA *o;
  OBJ_INDEX_DATA *pObjIndex;

  if (IS_NPC(ch))
    return;

  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (argument[0] == '\0') {
    ch_printf(ch, "\n\rSyntax: 'Demon weapon' <item>\n\r");
    ch_printf(ch, "\n\r");
    ch_printf(ch, "Item can only be one of the following:\n\r");
    ch_printf(ch, "\n\r");
    ch_printf(ch,
              "scimitar shortsword longsword broadsword lance maul\n\r");
    return;
  }
  if (!(!str_cmp(argument, "scimitar")) && !(!str_cmp(argument, "shortsword")) && !(!str_cmp(argument, "longsword")) && !(!str_cmp(argument, "broadsword")) && !(!str_cmp(argument, "lance")) && !(!str_cmp(argument, "maul"))) {
    do_demonweapon(ch, "");
    return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_demonweapon]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (ch->mana < ch->max_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (ch->hit < 100) {
    ch_printf(ch, "You don't have enough health.\n\r", ch);
    return;
  }
  if (ch->carry_number >= 3) {
    ch_printf(ch, "You haven't got any room.\n\r");
    return;
  }
  WAIT_STATE(ch, skill_table[gsn_demonweapon]->beats);
  if (can_use_skill(ch, number_percent(), gsn_demonweapon)) {
    int a = 0;

    if (!str_cmp(argument, "scimitar"))
      a = 1260;
    else if (!str_cmp(argument, "shortsword"))
      a = 1261;
    else if (!str_cmp(argument, "longsword"))
      a = 1262;
    else if (!str_cmp(argument, "broadsword"))
      a = 1263;
    else if (!str_cmp(argument, "lance"))
      a = 1264;
    else if (!str_cmp(argument, "maul"))
      a = 1265;
    if (a == 0) {
      bug("Serious problem in function demon weapon", 0);
      return;
    }
    if (!str_cmp(argument, "scimitar")) {
      act(AT_RED,
          "You begin to push a tremendous amount of energy into your arms; bloating your muscles up to a very large size. The moment your arms feel like they are going to explode, a twin pair of demon scimitars bursts out of them and into your hands; your arms healing back up almost instantly.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "$n begins to push a tremendous amount of energy into $s arms; bloating $s muscles up to a very large size. The moment $s arms look like they are going to explode, a twin pair of demon scimitars bursts out of them and into $s hands; $s arms healing back up almost instantly.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    if (!str_cmp(argument, "shortsword") ||
        !str_cmp(argument, "longsword") ||
        !str_cmp(argument, "broadsword")) {
      act(AT_RED,
          "The pupils of your eyes suddenly glow bright red as you grab a spike sticking out of your leg in one hand and begin to pull on it. Yelling out, your flesh is torn away as you pull a gleaming demon sword out of your leg. Your leg heals back up almost instantly.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "The pupils of $n's eyes suddenly glow bright red as $e grabs a spike sticking out of $s leg in one hand and begins to pull on it. Yelling out, $s flesh is torn away as $n pulls a gleaming demon sword out of $s leg. $n's leg heals back up almost instantly.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    if (!str_cmp(argument, "lance")) {
      act(AT_RED,
          "With a spray of demon blood you rip off your left arm with your right hand. The severed limb begins to bulge as you pump your own energy into it. It starts to stretch out from end to end, when all of a sudden it explodes, turning into a demon lance. Your left arm regenerates almost instantly after.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "With a spray of demon blood $n rips off $s left arm with $s right hand. The severed limb begins to bulge as $n pumps $s own energy into it. It starts to stretch out from end to end, when all of a sudden it explodes, turning into a demon lance. $n's left arm regenerates almost instantly after.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    if (!str_cmp(argument, "maul")) {
      act(AT_RED,
          "With a spray of demon blood you rip off your right arm with your left hand. The severed limb begins to bulge as you pump your own energy into it. It starts to stretch out from end to end, when all of a sudden it explodes, turning into a demon maul. Your right arm regenerates almost instantly after.",
          ch, NULL, NULL, TO_CHAR);
      act(AT_RED,
          "With a spray of demon blood $n rips off $s right arm with $s left hand. The severed limb begins to bulge as $n pumps $s own energy into it. It starts to stretch out from end to end, when all of a sudden it explodes, turning into a demon maul. $n's right arm regenerates almost instantly after.",
          ch, NULL, NULL, TO_NOTVICT);
    }
    learn_from_success(ch, gsn_demonweapon);
    pObjIndex = get_obj_index(a);
    o = create_object_new(pObjIndex, 1, ORIGIN_OINVOKE, ch->name);
    o = obj_to_char(o, ch);
    ch->hit -= 80;
    save_char_obj(ch);
  } else {
    act(AT_SKILL,
        "You fail to use your demon powers to create a weapon.", ch,
        NULL, NULL, TO_CHAR);
    act(AT_SKILL,
        "$n fails to use $s demon powers to create a weapon.", ch,
        NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_demonweapon);
  }
  ch->mana = 0;
  return;
}*/

/*void do_monkey_gun(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  int dam = 0;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_monkey_gun))
      return;
  }
  if (!xIS_SET((ch)->affected_by, AFF_OOZARU) &&
      !xIS_SET((ch)->affected_by, AFF_GOLDEN_OOZARU)) {
    ch_printf(ch,
              "You can only use this while in the oozaru form!\n\r");
    return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_monkey_gun]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }

  int focus = (get_curr_int(ch) / 5);

  if (focus < 2)
    focus = 2;

  if (ch->focus < focus) {
    send_to_char("You need to focus more.\n\r", ch);
    return;
  } else
    ch->focus -= focus;

  sh_int z = get_aura(ch);

  WAIT_STATE(ch, skill_table[gsn_monkey_gun]->beats);
  if (can_use_skill(ch, number_percent(), gsn_monkey_gun)) {
    dam = get_attmod(ch, victim) * number_range(40, 80);
    if (ch->charge > 0)
      dam = chargeDamMult(ch, dam);

    act(z,
        "You suddenly condense a tremendous amount of energy in the pit of your body. Aiming your head at $N, you open your giant mouth and essentially vomit up the energy, firing a massive column of energy at $M. You throw your clenched fists in the air and roar with all your might as $N is enveloped in a collosal explosion. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);
    act(z,
        "$n suddenly condenses a tremendous amount of energy in the pit of $s body. Aiming $s head at you, $n opens $s giant mouth and essentially vomits up the energy, firing a massive column of energy at you. $n throws $s clenched fists in the air and roars with all $s might as you are enveloped in a collosal explosion. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);
    act(z,
        "$n suddenly condenses a tremendous amount of energy in the pit of $s body. Aiming $s head at $N, $n opens $s giant mouth and essentially vomits up the energy, firing a massive column of energy at $N. $n throws $s clenched fists in the air and roars with all $s might as $N is enveloped in a collosal explosion. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);

    dam = ki_absorb(victim, ch, dam, gsn_monkey_gun);
    learn_from_success(ch, gsn_monkey_gun);
    global_retcode = damage(ch, victim, dam, TYPE_HIT);
  } else {
    act(z, "You missed $N with your oozaru mouth cannon.", ch, NULL,
        victim, TO_CHAR);
    act(z, "$n misses you with $s oozaru mouth cannon.", ch, NULL,
        victim, TO_VICT);
    act(z, "$n missed $N with a oozaru mouth cannon.", ch, NULL,
        victim, TO_NOTVICT);
    learn_from_failure(ch, gsn_monkey_gun);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }
  return;
}*/

/*void do_spirit_bomb(CHAR_DATA *ch, char *arg) {
  CHAR_DATA *victim;
  CHAR_DATA *vch;
  CHAR_DATA *gvch_prev;
  int dam = 0;
  int damPercent = 0;
  float numFound = 1.0;
  int damMult = 0;
  char size[MAX_STRING_LENGTH];
  char facialExpression[MAX_STRING_LENGTH];
  char engeryColor[MAX_STRING_LENGTH];
  char failVerb[MAX_STRING_LENGTH];

  size[0] = '\0';
  facialExpression[0] = '\0';
  engeryColor[0] = '\0';
  failVerb[0] = '\0';

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_spirit_bomb))
      return;
  }
  if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM)) {
    send_to_char("You can't concentrate enough for that.\n\r", ch);
    return;
  }
  if (!IS_NPC(ch) && ch->exp < skill_table[gsn_spirit_bomb]->skill_level[ch->class]) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (arg[0] == '\0') {
    send_to_char("Syntax: 'spirit bomb' <damage/MAX>\n\r", ch);
    return;
  }
  if ((victim = who_fighting(ch)) == NULL) {
    send_to_char("You aren't fighting anyone.\n\r", ch);
    return;
  }
  if (ch->mana < skill_table[gsn_spirit_bomb]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  if (!strcmp(arg, "max")) {
    damPercent = 100;
  } else if (is_number(arg)) {
    damPercent = atoi(arg);
    if (damPercent < 75 || damPercent > 99) {
      send_to_char("Percent must be 75 - 99.\n\r", ch);
      return;
    }
  } else {
    send_to_char("Syntax: 'spirit bomb' <percent/MAX>\n\r", ch);
    return;
  }

  if (can_use_skill(ch, number_percent(), gsn_spirit_bomb)) {
    for (vch = last_char; vch; vch = gvch_prev) {
      if (vch == first_char && vch->prev) {
        bug("do_spirit_bomb: first_char->prev != NULL... fixed", 0);
        vch->prev = NULL;
      }
      gvch_prev = vch->prev;

      if (ch->in_room->area != vch->in_room->area)
        continue;

      if (vch == victim)
        continue;

      if (vch->race == race_lookup("android"))
        continue;

      if (IS_GOOD(ch) && !IS_EVIL(vch)) {
        if (IS_NPC(vch))
          numFound += 0.45;
        else
          numFound +=
              (get_true_rank(vch) >
                       10
                   ? 1.0
                   : 0.65);
      } else if (IS_EVIL(ch) && !IS_GOOD(vch)) {
        if (IS_NPC(vch))
          numFound += 0.45;
        else
          numFound +=
              (get_true_rank(vch) >
                       10
                   ? 1.0
                   : 0.65);
      } else {
        if (IS_NPC(vch))
          numFound += 0.45;
        else
          numFound +=
              (get_true_rank(vch) >
                       10
                   ? 1.0
                   : 0.65);
      }
    }
    damMult = (int)numFound;

    if (ch->focus < damPercent) {
      send_to_char("You need to focus more.\n\r", ch);
      return;
    } else
      ch->focus -= damPercent;

    dam = number_range(15, 20);
    dam *= damMult;
    dam *= damPercent;
    dam /= 1000;

    if (dam <= 15)
      strcpy(size, "tiny");
    else if (dam <= 25)
      strcpy(size, "small");
    else if (dam <= 35)
      strcpy(size, "little");
    else if (dam <= 45)
      strcpy(size, "large");
    else if (dam <= 50)
      strcpy(size, "big");
    else if (dam <= 75)
      strcpy(size, "huge");
    else if (dam <= 100)
      strcpy(size, "great");
    else if (dam <= 250)
      strcpy(size, "enormous");
    else if (dam <= 500)
      strcpy(size, "massive");
    else if (dam <= 750)
      strcpy(size, "gigantic");
    else if (dam <= 1000)
      strcpy(size, "immense");
    else if (dam <= 2500)
      strcpy(size, "monstrous");
    else if (dam <= 5000)
      strcpy(size, "mammoth");
    else if (dam <= 7500)
      strcpy(size, "vast");
    else if (dam <= 10000)
      strcpy(size, "gargantuan");
    else
      strcpy(size, "colossal");
    dam = get_attmod(ch, victim) * dam;

    if (IS_GOOD(ch)) {
      strcpy(facialExpression, "smile");
      strcpy(engeryColor, "bluish");
    } else if (IS_EVIL(ch)) {
      strcpy(facialExpression, "smirk");
      strcpy(engeryColor, "reddish");
    } else {
      strcpy(facialExpression, "grimace");
      strcpy(engeryColor, "whitish");
    }

    act(AT_BLUE,
        "You raise your hands above your head, motes of $t energy beginning to form above your palms.",
        ch, engeryColor, victim, TO_CHAR);
    act(AT_BLUE,
        "$n raises $s hands above $s head, motes of $t energy beginning to form above $n's palms.",
        ch, engeryColor, victim, TO_VICT);
    act(AT_BLUE,
        "$n raises $s hands above $s head, motes of $t energy beginning to form above $n's palms.",
        ch, engeryColor, victim, TO_NOTVICT);
    if (IS_GOOD(ch)) {
      do_say(ch,
             "&YThe stars, the trees, all living things...lend me your energy!");
    } else if (IS_EVIL(ch)) {
      do_say(ch,
             "&YThe stars, the storm, crash of death....give me your power!");
    } else {
      do_say(ch,
             "&YThe stars, the planets, the spark of life...I need your energy!");
    }

    act2(AT_BLUE,
         "A $t ball of swirling $T energy forms above your upturned palms, growing slowly larger as you draw in more energy from the cosmos.",
         ch, size, victim, TO_CHAR, engeryColor);
    act(AT_BLUE,
        "You $t as you toss the brilliant sphere of pure ki towards $N.",
        ch, facialExpression, victim, TO_CHAR);
    act(AT_BLUE,
        "$N is slowly engulfed by the spirit bomb, ripped apart by the gathered energy. &W[$t]",
        ch, num_punct(dam), victim, TO_CHAR);

    act2(AT_BLUE,
         "A $t ball of swirling $T energy forms above $n's upturned palms, growing slowly larger as $e draws in more energy from the cosmos.",
         ch, size, victim, TO_VICT, engeryColor);
    act(AT_BLUE,
        "$n $ts as $e tosses the brilliant sphere of pure ki towards you.",
        ch, facialExpression, victim, TO_VICT);
    act(AT_BLUE,
        "You are slowly engulfed by the spirit bomb, ripped apart by the gathered energy. &W[$t]",
        ch, num_punct(dam), victim, TO_VICT);

    act2(AT_BLUE,
         "A $t ball of swirling $T energy forms above $n's upturned palms, growing slowly larger as $e draws in more energy from the cosmos.",
         ch, size, victim, TO_NOTVICT, engeryColor);
    act(AT_BLUE,
        "$n $ts as $e tosses the brilliant sphere of pure ki towards $N.",
        ch, facialExpression, victim, TO_NOTVICT);
    act(AT_BLUE,
        "$N is slowly engulfed by the spirit bomb, ripped apart by the gathered energy. &W[$t]",
        ch, num_punct(dam), victim, TO_NOTVICT);

    learn_from_success(ch, gsn_spirit_bomb);
    global_retcode =
        damage(ch, victim,
               (get_attmod(ch, victim) * dam * number_range(2, 5)),
               TYPE_HIT);
  } else {
    if (IS_GOOD(ch)) {
      strcpy(failVerb, "cry for help");
    } else if (IS_EVIL(ch)) {
      strcpy(failVerb, "plea for assistance");
    } else {
      strcpy(failVerb, "demand for power");
    }

    act(AT_BLUE,
        "Only a few dots of energy gather before quickly dispersing, your $t going unanswered.",
        ch, failVerb, victim, TO_CHAR);
    act(AT_BLUE,
        "Only a few dots of energy gather before quickly dispersing, $n's $t going unanswered.",
        ch, failVerb, victim, TO_VICT);
    act(AT_BLUE,
        "Only a few dots of energy gather before quickly dispersing, $n's $t going unanswered.",
        ch, failVerb, victim, TO_NOTVICT);

    learn_from_failure(ch, gsn_spirit_bomb);
    global_retcode = damage(ch, victim, 0, TYPE_HIT);
  }

  WAIT_STATE(ch, (skill_table[gsn_spirit_bomb]->beats));
  ch->mana -= skill_table[gsn_spirit_bomb]->min_mana;
  return;
}*/

void do_extreme(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    send_to_char("Temporarily disabled. Try 'powerup' instead.", ch);
    return;
  }
  double pl_mult;
  int kicontrol = 0;

  kicontrol = get_curr_int(ch);

  if (IS_NPC(ch) && !is_split(ch))
    return;

  if (IS_NPC(ch) && is_split(ch)) {
    if (!ch->master)
      return;
    if (!can_use_skill(ch->master, number_percent(), gsn_extreme))
      return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->flags, PCFLAG_KNOWSMYSTIC)) {
      ch_printf(ch,
                "You are unable to call upon those powers while you know mystic.\n\r");
      return;
    }
  }
  if (get_curr_str(ch) < 600) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (get_curr_int(ch) < 600) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (get_curr_dex(ch) < 250) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (get_curr_con(ch) < 1600) {
    send_to_char("Someday--with the right training--you feel you may be ready to use this technique.\n\r", ch);
    return;
  }
  if (wearing_chip(ch)) {
    ch_printf(ch, "You can't while you have a chip installed.\n\r");
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_EXTREME)) {
    send_to_char("You power down and return to normal.\n\r", ch);
    xREMOVE_BIT((ch)->affected_by, AFF_EXTREME);
    ch->pl = ch->exp;
    transStatRemove(ch);
    return;
  }
  if (!IS_NPC(ch)) {
    if (IS_SET(ch->pcdata->combatFlags, CMB_NO_HEART)) {
      send_to_char("You must first disable heart to use the 'Extreme Technique.'\n\r",
                   ch);
      return;
    }
  }
  if (xIS_SET((ch)->affected_by, AFF_KAIOKEN)) {
    send_to_char("You cannot use the 'Extreme Technique' in combination with kaioken.\n\r",
                 ch);
    return;
  }
  if (xIS_SET((ch)->affected_by, AFF_HYPER)) {
    send_to_char("You may not be in 'Hyper' to use the 'Extreme Technique'.\n\r",
                 ch);
    return;
  }
  if (ch->mana < skill_table[gsn_extreme]->min_mana) {
    send_to_char("You don't have enough energy.\n\r", ch);
    return;
  }
  sh_int z = get_aura(ch);

  WAIT_STATE(ch, skill_table[gsn_extreme]->beats);
  if (can_use_skill(ch, number_percent(), gsn_extreme)) {
    /*
     * note to self in case this needs to be changed back to
     * white color. it originally says "bright white light" in a
     * section of these messages. Changed it to bright light for
     * now. - Karma
     */
    act(z,
        "You close your eyes and concentrate, focusing all your energy and will to live on one point in your body.  A dot of light forms on your forehead, a bright light spreading out and bathing you in its glow as your hidden power awakens.",
        ch, NULL, NULL, TO_CHAR);
    act(z,
        "$n closes $s eyes and concentrates, focusing all of $s energy and will to live on one point in $s body.  A dot of light forms on $n's forehead, a bright light spreading out and bathing $s in its glow as $s hidden power awakens.",
        ch, NULL, NULL, TO_NOTVICT);
    xSET_BIT((ch)->affected_by, AFF_EXTREME);
    pl_mult = (double)kicontrol / 50 + 125;
    ch->pl = ch->exp * pl_mult;
    int kistat = 0;
    kistat = (get_curr_int(ch) / 5);
    transStatApply(ch, kistat, kistat, kistat, kistat);
    learn_from_success(ch, gsn_extreme);
  } else {
    act(z,
        "You close your eyes and concentrate, but can't seem to properly focus your immense energies.",
        ch, NULL, NULL, TO_CHAR);
    act(z,
        "$n closes $s eyes and concentrates, but can't seem to properly focus $s immense energies.",
        ch, NULL, NULL, TO_NOTVICT);
    learn_from_failure(ch, gsn_extreme);
  }

  ch->mana -= skill_table[gsn_extreme]->min_mana;
  return;
}

void do_kairestore(CHAR_DATA *ch, char *argument) {
  char arg[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];

  set_char_color(AT_SKILL, ch);
  set_pager_color(AT_SKILL, ch);
  argument = one_argument(argument, arg);
  argument = one_argument(argument, arg2);

  CHAR_DATA *vch;
  CHAR_DATA *vch_next;

  if (!ch->pcdata)
    return;

  if (get_true_rank(ch) < 11) {
    send_to_char("You can't do that.\n\r", ch);
    return;
  }
  if (IS_NPC(ch))
    return;

  if (sysdata.kaiRestoreTimer > 0) {
    pager_printf(ch,
                 "You can't restore for another %d minutes.  (Overall timer)\n\r",
                 sysdata.kaiRestoreTimer);
    return;
  }
  if (ch->pcdata->eKTimer > 0) {
    pager_printf(ch,
                 "You can't restore for another %d minutes.  (Personal timer)\n\r",
                 ch->pcdata->eKTimer);
    return;
  }
  if (ch->mana < 50000) {
    send_to_char("You need 50,000 energy to restore.\n\r", ch);
    return;
  }
  ch->mana -= 50000;
  ch->pcdata->eKTimer = 360;
  sysdata.kaiRestoreTimer = 20;
  save_char_obj(ch);
  send_to_char("Beginning kairestore...\n\r", ch);
  for (vch = first_char; vch; vch = vch_next) {
    vch_next = vch->next;
    if (!IS_NPC(vch)) {
      vch->hit = vch->max_hit;
      vch->mana = vch->max_mana;
      vch->move = vch->max_move;
      update_pos(vch);
      heart_calc(vch, "");
      if (ch->pcdata->kaiRestoreMsg && ch->pcdata->kaiRestoreMsg[0] != '\0' && str_cmp("(null)", ch->pcdata->kaiRestoreMsg))
        act(AT_LBLUE, ch->pcdata->kaiRestoreMsg, ch,
            NULL, vch, TO_VICT);
      else {
        act(AT_LBLUE,
            "An avatar of $n appears and performs some arcane symbols while",
            ch, NULL, vch, TO_VICT);
        act(AT_LBLUE,
            "muttering unheard words, and suddenly you feel refreshed as the",
            ch, NULL, vch, TO_VICT);
        act(AT_LBLUE,
            "avatar vanishes into nothingness.", ch,
            NULL, vch, TO_VICT);
      }
    }
  }
  send_to_char("All restored.\n\r", ch);
}

void do_setrestoremessage(CHAR_DATA *ch, char *argument) {
  if (!IS_NPC(ch)) {
    smash_tilde(argument);
    if (argument[0] == '\0') {
      pager_printf(ch, "Your current message is:\n\r%s\n\r",
                   ch->pcdata->kaiRestoreMsg);
      pager_printf(ch,
                   "Use any argument to set your message.\n\r");
      pager_printf(ch,
                   "Use clear to set it to the default.\n\r");
      return;
    }
    DISPOSE(ch->pcdata->kaiRestoreMsg);
    if (!str_cmp(argument, "clear")) {
      ch->pcdata->kaiRestoreMsg = str_dup("(null)");
      send_to_char_color("&YKai restore message reset.\n\r",
                         ch);
      return;
    } else {
      if (has_phrase("$n", argument) || has_phrase(strupper(ch->name),
                                                   strupper(argument)))
        ch->pcdata->kaiRestoreMsg = str_dup(argument);
      else {
        send_to_char("You must have your name in your message.\n\r",
                     ch);
        return;
      }
    }
    send_to_char_color("&YKai restore message set.\n\r", ch);
  }
  return;
}

void do_potarafuse(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *rch;
  CHAR_DATA *vch;
  CHAR_DATA *dch; /* dominant fuse char */
  CHAR_DATA *sch; /* stasis fuse char */
  char arg[MAX_INPUT_LENGTH];
  char arg2[MAX_INPUT_LENGTH];
  bool cright = false;
  bool vright = false;
  bool cleft = false;
  bool vleft = false;
  OBJ_DATA *ro;
  OBJ_DATA *vo;
  OBJ_DATA *po;
  OBJ_DATA *o;
  OBJ_INDEX_DATA *pObjIndex;
  int sn;
  int value;
  bool saiyan1 = false, saiyan2 = false;
  bool namek1 = false, namek2 = false;
  bool human1 = false, human2 = false;
  bool halfb1 = false, halfb2 = false;

  argument = one_argument(argument, arg);
  argument = one_argument(argument, arg2);

  if (ch->level < 63) {
    ch_printf(ch, "Huh?");
    return;
  }
  if (arg[0] == '\0') {
    ch_printf(ch,
              "\n\rSyntax: potarafuse (first person) (second person)\n\r");
    ch_printf(ch,
              "\n\rThe order in which you put the two people's names in does not matter.\n\r");
    ch_printf(ch,
              "Whoever has the earring on the right ear will be the person in control\n\rof the resulting character.\n\r");
    return;
  }
  if ((rch = get_char_room(ch, arg)) == NULL) {
    ch_printf(ch, "There is nobody here named %s.\n\r", arg);
    return;
  }
  if ((vch = get_char_room(ch, arg2)) == NULL) {
    ch_printf(ch, "There is nobody here named %s.\n\r", arg2);
    return;
  }
  if (IS_NPC(rch) || IS_NPC(vch)) {
    ch_printf(ch, "NPC's cannot fuse.\n\r");
    return;
  }
  if (is_fused(rch)) {
    ch_printf(ch, "%s has already fused before.\n\r", rch->name);
    return;
  }
  if (is_fused(vch)) {
    ch_printf(ch, "%s has already fused before.\n\r", vch->name);
    return;
  }
  if (IS_IMMORTAL(rch) || IS_IMMORTAL(vch)) {
    ch_printf(ch, "Admins may not fuse.\n\r");
    return;
  }
  if (is_android(rch) || is_superandroid(rch) || is_icer(rch) ||
      is_bio(rch) || is_kaio(rch) || is_demon(rch)) {
    ch_printf(ch, "%s's race cannot fuse this way.\n\r", rch->name);
    return;
  }
  if (is_android(vch) || is_superandroid(vch) || is_icer(vch) ||
      is_bio(vch) || is_kaio(vch) || is_demon(vch)) {
    ch_printf(ch, "%s's race cannot fuse this way.\n\r", vch->name);
    return;
  }
  if (rch->pcdata->clan) {
    ch_printf(ch, "%s must be outcasted from their clan first.\n\r",
              rch->name);
    return;
  }
  if (vch->pcdata->clan) {
    ch_printf(ch, "%s must be outcasted from their clan first.\n\r",
              vch->name);
    return;
  }
  if (is_transformed(rch)) {
    ch_printf(ch, "%s must power down completely first.\n\r",
              rch->name);
    return;
  }
  if (is_transformed(vch)) {
    ch_printf(ch, "%s must power down completely first.\n\r",
              vch->name);
    return;
  }
  if ((ro = wearing_potara_right(rch)) == NULL) {
    if ((ro = wearing_potara_left(rch)) == NULL) {
      ch_printf(ch, "%s is not wearing a potara earring.\n\r",
                rch->name);
      return;
    } else
      cleft = true;
  } else
    cright = true;
  if ((vo = wearing_potara_right(vch)) == NULL) {
    if ((vo = wearing_potara_left(vch)) == NULL) {
      ch_printf(ch, "%s is not wearing a potara earring.\n\r",
                vch->name);
      return;
    } else
      vleft = true;
  } else
    vright = true;

  if (cright && vright) {
    ch_printf(ch,
              "One of them must be wearing a left ear potara earring.\n\r");
    return;
  }
  if (cleft && vleft) {
    ch_printf(ch,
              "One of them must be wearing a right ear potara earring.\n\r");
    return;
  }
  if (cright && vleft) {
    dch = rch;
    sch = vch;
  } else if (cleft && vright) {
    dch = vch;
    sch = rch;
  } else {
    ch_printf(ch,
              "A serious bug has occured. Contant a coder immediately.\n\r");
    return;
  }

  unequip_char(rch, ro);
  unequip_char(vch, vo);
  extract_obj(ro);
  extract_obj(vo);

  pObjIndex = get_obj_index(78); /* Dual-potara-earring item */
  po = create_object_new(pObjIndex, 1, ORIGIN_OINVOKE, ch->name);
  po = obj_to_char(po, dch);
  equip_char(dch, po, WEAR_EARS);

  dch->fused[dch->fusions] = STRALLOC(sch->name);
  sch->fused[sch->fusions] = STRALLOC(dch->name);
  dch->fusions++;
  sch->fusions++;
  dch->bck_name = STRALLOC(dch->name);
  sch->bck_name = STRALLOC(sch->name);
  dch->bck_race = dch->race;
  sch->bck_race = sch->race;
  dch->bck_pl = dch->exp;
  sch->bck_pl = sch->exp;

  dch->pcdata->quest_curr += sch->pcdata->quest_curr;
  dch->pcdata->quest_accum += sch->pcdata->quest_accum;
  dch->pcdata->pkills += sch->pcdata->pkills;
  dch->pcdata->pdeaths += sch->pcdata->pdeaths;
  dch->pcdata->mkills += sch->pcdata->mkills;
  dch->pcdata->mdeaths += sch->pcdata->mdeaths;
  dch->pcdata->spar_wins += sch->pcdata->spar_wins;
  dch->pcdata->spar_loss += sch->pcdata->spar_loss;

  if (!str_cmp(get_race(dch), "saiyan"))
    saiyan1 = true;
  if (!str_cmp(get_race(sch), "saiyan"))
    saiyan2 = true;
  if (!str_cmp(get_race(dch), "namek"))
    namek1 = true;
  if (!str_cmp(get_race(sch), "namek"))
    namek2 = true;
  if (!str_cmp(get_race(dch), "human"))
    human1 = true;
  if (!str_cmp(get_race(sch), "human"))
    human2 = true;
  if (!str_cmp(get_race(dch), "halfbreed"))
    halfb1 = true;
  if (!str_cmp(get_race(sch), "halfbreed"))
    halfb2 = true;

  if ((saiyan1 && namek2)) {
    dch->race = get_race_num("saiyan-n");
    dch->class = get_class_num("saiyan-n");
  } else if ((saiyan2 && namek1)) {
    dch->race = get_race_num("namek-s");
    dch->class = get_class_num("namek-s");
  } else if ((saiyan1 && human2)) {
    dch->race = get_race_num("saiyan-h");
    dch->class = get_class_num("saiyan-h");
  } else if ((saiyan2 && human1)) {
    dch->race = get_race_num("human-s");
    dch->class = get_class_num("human-s");
  } else if ((saiyan1 && halfb2)) {
    dch->race = get_race_num("saiyan-hb");
    dch->class = get_class_num("saiyan-hb");
  } else if ((saiyan2 && halfb1)) {
    dch->race = get_race_num("halfbreed-s");
    dch->class = get_class_num("halfbreed-s");
  } else if ((saiyan1 && saiyan2)) {
    dch->race = get_race_num("saiyan-s");
    dch->class = get_class_num("saiyan-s");
  } else if ((namek1 && halfb2)) {
    dch->race = get_race_num("namek-hb");
    dch->class = get_class_num("namek-hb");
  } else if ((namek2 && halfb1)) {
    dch->race = get_race_num("halfbreed-n");
    dch->class = get_class_num("halfbreed-n");
  } else if ((human1 && namek2)) {
    dch->race = get_race_num("human-n");
    dch->class = get_class_num("human-n");
  } else if ((human2 && namek1)) {
    dch->race = get_race_num("namek-h");
    dch->class = get_class_num("namek-h");
  } else if ((human1 && halfb2)) {
    dch->race = get_race_num("human-hb");
    dch->class = get_class_num("human-hb");
  } else if ((human2 && halfb1)) {
    dch->race = get_race_num("halfbreed-h");
    dch->class = get_class_num("halfbreed-h");
  } else if ((human1 && human2)) {
    dch->race = get_race_num("human-h");
    dch->class = get_class_num("human-h");
  } else if ((namek1 && namek2)) {
    dch->race = get_race_num("namek-n");
    dch->class = get_class_num("namek-n");
  } else if ((halfb1 && halfb2)) {
    dch->race = get_race_num("halfbreed-hb");
    dch->class = get_class_num("halfbreed-hb");
  }
  dch->exp += sch->exp;
  dch->pl = dch->exp;

  value = 95;
  for (sn = 0; sn < top_sn; sn++) {
    if (skill_table[sn]->min_level[dch->class] == 0 || skill_table[sn]->skill_level[dch->class] == 0)
      continue;

    if (skill_table[sn]->name && (dch->exp >= skill_table[sn]->skill_level[dch->class] || value == 0)) {
      if (value > GET_ADEPT(dch, sn) && !IS_IMMORTAL(dch))
        dch->pcdata->learned[sn] = GET_ADEPT(dch, sn);
      else
        dch->pcdata->learned[sn] = value;
    }
  }

  /* message sent to dch */
  ch_printf(dch,
            "&CThe potara earring on your right ear suddenly begins to glow,\n\r");
  ch_printf(dch,
            "as does the one on %s's left ear. You are then violently hurled\n\r",
            sch->name);
  ch_printf(dch,
            "through the air towards %s, and slam into each other. A blinding\n\r",
            sch->name);
  ch_printf(dch,
            "light envelopes you as you feel your mind and body ripped apart\n\r");
  ch_printf(dch,
            "and mixed together with %s's; becoming one. You are no longer who\n\r",
            sch->name);
  ch_printf(dch,
            "you once were, nor is %s. You thoughts are seperate, but one.\n\r",
            sch->name);
  ch_printf(dch,
            "Your movements are seperate, but one. Even your voice sounds as\n\r");
  ch_printf(dch,
            "if the both of you were speaking at once. You have become a fused\n\r");
  ch_printf(dch, "being.\n\r");
  ch_printf(dch,
            "&z(You were wearing the earring on the right ear, so you are the\n\r");
  ch_printf(dch,
            "one in control of the fused character. You will be required to\n\r");
  ch_printf(dch,
            "choose a new name, based on the combination of yours, and %'s.\n\r",
            sch->name);
  ch_printf(dch,
            "You will also need to edit your bio to reflect the change in\n\r");
  ch_printf(dch,
            "your state of being, unless you didn't have an authed bio to\n\r");
  ch_printf(dch, "begin with. Congratulations.)\n\r");

  /* message sent to sch */
  ch_printf(sch,
            "&CThe potara earring on your left ear suddenly begins to glow,\n\r");
  ch_printf(sch,
            "as does the one on %s's right ear. You are then violently hurled\n\r",
            dch->name);
  ch_printf(sch,
            "through the air towards %s, and slam into each other. A blinding\n\r",
            dch->name);
  ch_printf(sch,
            "light envelopes you as you feel your mind and body ripped apart\n\r");
  ch_printf(sch,
            "and mixed together with %s's; becoming one. You are no longer who\n\r",
            dch->name);
  ch_printf(sch,
            "you once were, nor is %s. You thoughts are seperate, but one.\n\r",
            dch->name);
  ch_printf(sch,
            "Your movements are seperate, but one. Even your voice sounds as\n\r");
  ch_printf(sch,
            "if the both of you were speaking at once. You have become a fused\n\r");
  ch_printf(sch, "being.\n\r");
  ch_printf(sch,
            "&R(You were wearing the earring on the left ear, so you are now\n\r");
  ch_printf(sch,
            "put into fusion stasis (meaning you are frozen), where you will\n\r");
  ch_printf(sch,
            "remain for all time. Consider this character non-existant. You\n\r");
  ch_printf(sch,
            "may not recreate as this character, nor can it be renamed. That\n\r");
  ch_printf(sch, "is the price you pay for fusing.\n\r");

  while ((o = carrying_noquit(sch)) != NULL) {
    obj_from_char(o);
    obj_to_room(o, sch->in_room);
    ch_printf(sch, "&wYou drop %s&w.\n\r", o->short_descr);
  }

  SET_BIT(dch->fusionflags, FUSION_POTARA);
  SET_BIT(sch->fusionflags, FUSION_STASIS);
  char_to(sch, ROOM_VNUM_FUSIONSTASIS);
  save_char_obj(dch);
  save_char_obj(sch);

  /* do_sset(ch,strcat(dch->name," all 95")); */
  ch_printf(ch, "%s and %s have been fused together.\n\r",
            dch->name, sch->name);
}

bool is_fused(CHAR_DATA *ch) {
  if (IS_SET(ch->fusionflags, FUSION_DANCE))
    return true;
  else if (IS_SET(ch->fusionflags, FUSION_POTARA))
    return true;
  else if (IS_SET(ch->fusionflags, FUSION_NAMEK))
    return true;
  else if (IS_SET(ch->fusionflags, FUSION_STASIS))
    return true;
  else if (IS_SET(ch->fusionflags, FUSION_SUPERANDROID))
    return true;
  else
    return false;
}

bool is_dancefused(CHAR_DATA *ch) {
  if (IS_SET(ch->fusionflags, FUSION_DANCE))
    return true;
  else
    return false;
}

bool is_potarafused(CHAR_DATA *ch) {
  if (IS_SET(ch->fusionflags, FUSION_POTARA))
    return true;
  else
    return false;
}

bool is_namekfused(CHAR_DATA *ch) {
  if (IS_SET(ch->fusionflags, FUSION_NAMEK))
    return true;
  else
    return false;
}

bool fusion_stasis(CHAR_DATA *ch) {
  if (IS_SET(ch->fusionflags, FUSION_STASIS))
    return true;
  else
    return false;
}

OBJ_DATA *
carrying_noquit(CHAR_DATA *ch) {
  OBJ_DATA *obj, *obj_next;

  for (obj = ch->first_carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    if (IS_OBJ_STAT(obj, ITEM_NOQUIT)) {
      return obj;
    }
  }
  return NULL;
}

OBJ_DATA *
carrying_dball(CHAR_DATA *ch) {
  OBJ_DATA *obj, *obj_next;

  for (obj = ch->first_carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    if (obj->item_type == ITEM_DRAGONBALL) {
      return obj;
    }
  }
  return NULL;
}

OBJ_DATA *
wearing_potara_right(CHAR_DATA *ch) {
  OBJ_DATA *obj, *obj_next;

  for (obj = ch->first_carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    if (obj->wear_loc != WEAR_NONE) {
      if (obj->pIndexData->vnum == 76)
        return obj;
    }
  }
  return NULL;
}

OBJ_DATA *
wearing_potara_left(CHAR_DATA *ch) {
  OBJ_DATA *obj, *obj_next;

  for (obj = ch->first_carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    if (obj->wear_loc != WEAR_NONE) {
      if (obj->pIndexData->vnum == 77)
        return obj;
    }
  }
  return NULL;
}

OBJ_DATA *
wearing_chip(CHAR_DATA *ch) {
  OBJ_DATA *obj, *obj_next;

  for (obj = ch->first_carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    /*
     * Chips should have been like this a *long* time ago -Goku
     * 09.30.04
     */
    if (CAN_WEAR(obj, ITEM_WEAR_PANEL))
      return obj;
  }
  return NULL;
}

OBJ_DATA *
wearing_sentient_chip(CHAR_DATA *ch) {
  OBJ_DATA *obj, *obj_next;

  for (obj = ch->first_carrying; obj != NULL; obj = obj_next) {
    obj_next = obj->next_content;
    /*
     * Chips should have been like this a *long* time ago -Goku
     * 09.30.04
     */
    if (CAN_WEAR(obj, ITEM_WEAR_PANEL)) {
      if (obj->pIndexData->vnum == 1337)
        return obj;
    }
  }
  return NULL;
}

bool seven_dballs_here(CHAR_DATA *ch, char *planet) {
  OBJ_DATA *obj;
  bool onestar = false;
  bool twostar = false;
  bool threestar = false;
  bool fourstar = false;
  bool fivestar = false;
  bool sixstar = false;
  bool sevenstar = false;

  if (!str_cmp(planet, "earth")) {
    if ((obj = get_obj_vnum_room(ch, 1280)) != NULL)
      onestar = true;
    if ((obj = get_obj_vnum_room(ch, 1281)) != NULL)
      twostar = true;
    if ((obj = get_obj_vnum_room(ch, 1282)) != NULL)
      threestar = true;
    if ((obj = get_obj_vnum_room(ch, 1283)) != NULL)
      fourstar = true;
    if ((obj = get_obj_vnum_room(ch, 1284)) != NULL)
      fivestar = true;
    if ((obj = get_obj_vnum_room(ch, 1285)) != NULL)
      sixstar = true;
    if ((obj = get_obj_vnum_room(ch, 1286)) != NULL)
      sevenstar = true;

    if (onestar && twostar && threestar && fourstar &&
        fivestar && sixstar && sevenstar)
      return true;
    else
      return false;
  }
  return false;
}

void do_summon(CHAR_DATA *ch, char *argument) {
  CHAR_DATA *victim;
  char buf[MAX_STRING_LENGTH];
  OBJ_DATA *obj;

  if (IS_NPC(ch))
    return;

  if (summon_state != SUMMON_NONE) {
    ch_printf(ch, "A dragon is already being summoned!\n\r");
    return;
  }
  if (summon_room != NULL || summon_area != NULL) {
    ch_printf(ch,
              "A dragon is already summoned somewhere else.\n\r");
    return;
  }
  if (argument[0] == '\0') {
    ch_printf(ch, "\n\rSyntax: summon (dragon)\n\r");
    ch_printf(ch, "\n\rDragon has to be one of the following:\n\r");
    ch_printf(ch, "shenron | porunga\n\r");
    return;
  }
  if (xIS_SET(ch->in_room->room_flags, ROOM_SAFE)) {
    ch_printf(ch,
              "You can't summon the dragon in a safe room.\n\r");
    return;
  }
  if (!str_cmp(argument, "shenron")) {
    /* make a check to see that Karma is in the room first */
    if ((victim = get_char_room(ch, "karma")) == NULL) {
      ch_printf(ch,
                "\n\r&RYou cannot summon the dragon without Karma here.\n\r");
      return;
    }
    if (!seven_dballs_here(ch, "earth")) {
      ch_printf(ch,
                "\n\r&RAll seven dragonballs must be here on the ground for you to be able to summon the dragon.\n\r");
      return;
    }
    summon_state = SUMMON_SKY1;
    summon_area = ch->in_room->area;
    summon_room = ch->in_room;

    sprintf(buf,
            "%s has summoned the eternal dragon Shenron at room %d",
            ch->name, ch->in_room->vnum);
    to_channel(buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL);

    if ((obj = get_obj_vnum_room(ch, 1280)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);
    if ((obj = get_obj_vnum_room(ch, 1281)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);
    if ((obj = get_obj_vnum_room(ch, 1282)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);
    if ((obj = get_obj_vnum_room(ch, 1283)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);
    if ((obj = get_obj_vnum_room(ch, 1284)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);
    if ((obj = get_obj_vnum_room(ch, 1285)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);
    if ((obj = get_obj_vnum_room(ch, 1286)) != NULL)
      REMOVE_BIT(obj->wear_flags, ITEM_TAKE);

    xSET_BIT(summon_room->room_flags, ROOM_SAFE);
    return;
  }
  if (!str_cmp(argument, "porunga")) {
    do_summon(ch, "");
    return;
  }
  do_summon(ch, "");
}

void do_grant(CHAR_DATA *ch, char *argument) {
  if (IS_NPC(ch))
    return;

  if (argument[0] == '\0') {
    ch_printf(ch, "\n\rSyntax: grant (option)\n\r");
    ch_printf(ch, "\n\rOption has to be one of the following:\n\r");
    ch_printf(ch, "  finish (for the dragons leaving scene)\n\r");
    return;
  }
  if (!str_cmp(argument, "finish")) {
    summon_state = SUMMON_LEAVE1;
    return;
  }
  return;
}

void do_showrank(CHAR_DATA *ch, char *argument) {
  char arg[MAX_STRING_LENGTH];

  argument = one_argument(argument, arg);

  if (arg[0] == '\0') {
    ch_printf(ch, "\n\rSyntax: showrank (kaio/demon)\n\r");
    return;
  }
  if (!str_cmp(arg, "kaio")) {
    ch_printf(ch, "\n\r");
    ch_printf(ch,
              "&cKaioshin ranks and who currently holds them:\n\r");
    ch_printf(ch,
              "&z============================================\n\r");
    ch_printf(ch, "&RSupreme Kai: &w%s\n\r", kaioshin[5]);
    ch_printf(ch, "&W  Grand Kai: &w%s\n\r", kaioshin[4]);
    ch_printf(ch, "&Y  North Kai: &w%s\n\r", kaioshin[3]);
    ch_printf(ch, "&B   West Kai: &w%s\n\r", kaioshin[2]);
    ch_printf(ch, "&G   East Kai: &w%s\n\r", kaioshin[1]);
    ch_printf(ch, "&g  South Kai: &w%s\n\r", kaioshin[0]);
    ch_printf(ch,
              "&z============================================\n\r");
    return;
  }
  if (!str_cmp(arg, "demon")) {
    ch_printf(ch, "\n\r");
    ch_printf(ch,
              "&cDemon ranks and who currently holds them:\n\r");
    ch_printf(ch,
              "&z============================================\n\r");
    ch_printf(ch, "&R   Demon King: &w%s\n\r", demonking);
    ch_printf(ch, "&rDemon Warlord: &w%s\n\r", demonwarlord[0]);
    ch_printf(ch, "&rDemon Warlord: &w%s\n\r", demonwarlord[1]);
    ch_printf(ch, "&rDemon Warlord: &w%s\n\r", demonwarlord[2]);
    ch_printf(ch, "&OGreater Demon: &w%s\n\r", greaterdemon[0]);
    ch_printf(ch, "&OGreater Demon: &w%s\n\r", greaterdemon[1]);
    ch_printf(ch, "&OGreater Demon: &w%s\n\r", greaterdemon[2]);
    ch_printf(ch, "&OGreater Demon: &w%s\n\r", greaterdemon[3]);
    ch_printf(ch, "&OGreater Demon: &w%s\n\r", greaterdemon[4]);
    ch_printf(ch, "&OGreater Demon: &w%s\n\r", greaterdemon[5]);
    ch_printf(ch,
              "&z============================================\n\r");
  }
}

/*eof*/
