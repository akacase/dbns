/****************************************************************************
 * [S]imulated [M]edieval [A]dventure multi[U]ser [G]ame      |   \\._.//   *
 * -----------------------------------------------------------|   (0...0)  *
 * SMAUG 1.4 (C)1994, 1995, 1996, 1998  by Derek Snider      |   ).:.(   *
 * -----------------------------------------------------------|    {o o}    *
 * SMAUG code team: Thoric, Altrag, Blodkai, Narn, Haus,      |   / ' ' \   *
 * Scryn, Rennard, Swordbearer, Gorog, Grishnakh, Nivek,      |~'~.VxvxV.~'~*
 * Tricops and Fireblade                                      |             *
 * ------------------------------------------------------------------------ *
 * Merc 2.1 Diku Mud improvments copyright (C)1992, 1993 by Michael        *
 * Chastain, Michael Quan, and Mitchell Tse.                                *
 * Original Diku Mud copyright (C)1990, 1991 by Sebastian Hammer,          *
 * Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.     *
 * ------------------------------------------------------------------------ *
 *			   Rare Item Code Module                            *
 *                       Shadroth - Lands of Shadow                         *
 * ------------------------------------------------------------------------ *
 *			  capsule/corp (C)2016, 2017		    	    *
 *                            case@capsulecorp.org			    *
 ****************************************************************************/

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "mud.h"
#include "comm.h"

#if defined(KEY)
#undef KEY
#endif

#define KEY(literal, field, value) \
  if (!strcmp(word, literal)) {    \
    field = value;                 \
    fMatch = true;                 \
    break;                         \
  }

void scan_equipment(void) {
  DIR *dp;
  struct dirent *dir_entry;
  char dir_name[100];
  int alpha_loop;

  log_string("Updating rare/unique item counts.....");
  log_string("Checking player files....");

  for (alpha_loop = 0; alpha_loop <= 25; alpha_loop++) {
    snprintf(dir_name, sizeof(dir_name), "%s%c", PLAYER_DIR, 'a' + alpha_loop);
    dp = opendir(dir_name);
    if (dp != NULL) {
      dir_entry = readdir(dp);
      while (dir_entry) {
        if (dir_entry->d_name[0] != '.') {
          printf("%s\n", dir_entry->d_name);
          if (scan_players(dir_name, dir_entry->d_name))
            log_string("Found rare/unique items....");
        }
        dir_entry = readdir(dp);
      }
      closedir(dp);
    } else {
      mkdir(dir_name, 0744);
      continue;
    }
  }

  log_string("Checking corpses....");

  snprintf(dir_name, sizeof(dir_name), "%s", CORPSE_DIR);
  dp = opendir(dir_name);
  if (dp != NULL) {
    dir_entry = readdir(dp);
    while (dir_entry) {
      if (dir_entry->d_name[0] != '.') {
        if (scan_corpses(dir_name, dir_entry->d_name))
          log_string("Found rare/unique items.");
      }
      dir_entry = readdir(dp);
    }
    closedir(dp);

    return;
  } else {
    mkdir(dir_name, 0744);
  }
}

bool scan_players(char *dirname, char *filename) {
  FILE *player_file;
  char file_name[MAX_STRING_LENGTH];
  bool found = false;
  bool fMatch;

  snprintf(file_name, sizeof(file_name), "%s/%s", dirname, filename);

  if ((player_file = fopen(file_name, "r")) == NULL) {
    perror(file_name);
    return found;
  }

  for (;;) {
    int counter = 1;
    char letter;
    char *word;
    char *short_descr;
    OBJ_INDEX_DATA *pObjIndex;

    letter = fread_letter(player_file);

    if ((letter != '#') && (!feof(player_file)))
      continue;

    word = feof(player_file) ? "End" : fread_word(player_file);

    if (!str_cmp(word, "End"))
      break;

    if (!str_cmp(word, "OBJECT")) {
      bool eoo = false;
      while (eoo == false) {
        word = feof(player_file) ? "End" : fread_word(player_file);

        switch (UPPER(word[0])) {
          case '*':
            fMatch = true;
            fread_to_eol(player_file);
            break;
          case 'C':
            KEY("Count", counter, fread_number(player_file));
            break;
          case 'E':
            if (!str_cmp(word, "End"))
              eoo = true;
            break;
          case 'S':
            KEY("ShortDescr", short_descr, fread_string(player_file));
            break;
          case 'V':
            if (!strcmp(word, "Vnum")) {
              int vnum;

              vnum = fread_number(player_file);
              if ((pObjIndex = get_obj_index(vnum)) == NULL) {
                bug("Bad vnum while reading in rare.c");
                break;
              } else {
                if (IS_OBJ_STAT(pObjIndex, ITEM_RARE) || IS_OBJ_STAT(pObjIndex, ITEM_UNIQUE)) {
                  pObjIndex->count += counter;
                  found = true;
                }
              }
            }
            break;
        }
      }
    }
  }

  fclose(player_file);
  player_file = NULL;
  return found;
}

bool scan_corpses(char *dirname, char *filename) {
  FILE *corpse_file;
  char file_name[MAX_STRING_LENGTH];
  bool found = false;

  snprintf(file_name, sizeof(file_name), "%s/%s", dirname, filename);

  if ((corpse_file = fopen(file_name, "r")) == NULL) {
    perror(file_name);
    return found;
  }

  for (;;) {
    int vnum, counter = 1;
    char letter;
    char *word;
    OBJ_INDEX_DATA *pObjIndex;

    letter = fread_letter(corpse_file);

    if ((letter != '#') && (!feof(corpse_file)))
      continue;

    word = feof(corpse_file) ? "End" : fread_word(corpse_file);

    if (!str_cmp(word, "End"))
      break;
    if (!str_cmp(word, "OBJECT")) {
      for (;;) {
        word = feof(corpse_file) ? "End" : fread_word(corpse_file);

        if (!str_cmp(word, "End"))
          break;
        if (!str_cmp(word, "Count"))
          counter = fread_number(corpse_file);
        if (!str_cmp(word, "Vnum")) {
          vnum = fread_number(corpse_file);
          if ((get_obj_index(vnum)) == NULL) {
            bug("Bad obj vnum in limits: %d", vnum);
          } else {
            pObjIndex = get_obj_index(vnum);
            if (IS_OBJ_STAT(pObjIndex, ITEM_RARE) || IS_OBJ_STAT(pObjIndex, ITEM_UNIQUE)) {
              pObjIndex->count += counter;
              found = true;
            }
          }
        }
      }
    }
  }

  fclose(corpse_file);
  corpse_file = NULL;
  return found;
}
