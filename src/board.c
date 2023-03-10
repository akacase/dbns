/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "comm.h"
#include "mud.h"

/*

 Note Board system, (c) 1995-96 Erwin S. Andreasen, erwin@pip.dknet.dk
 =====================================================================

 Basically, the notes are split up into several boards. The boards do not
 exist physically, they can be read anywhere and in any position.

 Each of the note boards has its own file. Each of the boards can have its own
 "rights": who can read/write.

 Each character has an extra field added, namele the timestamp of the last note
 read by him/her on a certain board.

 The note entering system is changed too, making it more interactive. When
 entering a note, a character is put AFK and into a special CON_ state.
 Everything typed goes into the note.

 For the immortals it is possible to purge notes based on age. An Archive
 options is available which moves the notes older than X days into a special
 board. The file of this board should then be moved into some other directory
 during e.g. the startup script and perhaps renamed depending on date.

 Note that write_level MUST be >= read_level or else there will be strange
 output in certain functions.

 Board DEFAULT_BOARD must be at least readable by *everyone*.

*/

NOTE_DATA *note_free = NULL;

#define L_SUP (MAX_LEVEL - 1) /* if not already defined */

GLOBAL_BOARD_DATA boards[MAX_BOARD] =
    {
        {"Announce", "Announcements from Admins", 0, LEVEL_IMMORTAL, "all", DEF_NORMAL, 60, NULL, false, 1},
        {"General", "General discussion", 0, 1, "all", DEF_INCLUDE, 14, NULL, false, 1},
        {"Clan", "Discussion with Clans", 0, 1, "all", DEF_NORMAL, 14, NULL, false, 2},
        {"RP", "[IC] Discussion", 0, 1, "all", DEF_INCLUDE, 14, NULL, false, 3},
        {"Ideas", "Suggestion for improvement", 0, 1, "all", DEF_INCLUDE, 14, NULL, false, 1},
        {"Bugs", "Typos, bugs, errors", 0, 1, "all", DEF_INCLUDE, 14, NULL, false, 1},
        {"Personal", "Personal messages", 0, 1, "all", DEF_EXCLUDE, 21, NULL, false, 1},
        {"Admin", "General admin board", LEVEL_IMMORTAL, LEVEL_IMMORTAL, "all", DEF_INCLUDE, 30, NULL, false, 1},
        {"Building", "Building and areas", LEVEL_IMMORTAL, LEVEL_IMMORTAL, "all", DEF_INCLUDE, 30, NULL, false, 1},
        /* Locking out the Coding board to non-coders */
        {"Coding", "Coding board (64+)", 63, LEVEL_IMMORTAL, "all", DEF_INCLUDE, 30, NULL, false, 1},
        {"Punish", "Punishments dealt", 0, LEVEL_IMMORTAL, "all", DEF_INCLUDE, 30, NULL, false, 1},
        {"TopAdmin", "REAL admin board! (58+)", LEVEL_GOD, LEVEL_GOD, "all", DEF_INCLUDE, 30, NULL, false, 1}};

/* The prompt that the character is given after finishing a note with ~ or END */
const char *szFinishPrompt = "(&WC&w)ontinue, (&WV&w)iew, (&WP&w)ost or (&WF&w)orget it?";

long last_note_stamp = 0; /* To generate unique timestamps on notes */

#define BOARD_NOACCESS -1
#define BOARD_NOTFOUND -1

bool next_board(CHAR_DATA *ch);

/* recycle a note */
void free_global_note(NOTE_DATA *note) {
  if (note->sender)
    STRFREE(note->sender);
  if (note->to_list)
    STRFREE(note->to_list);
  if (note->subject)
    STRFREE(note->subject);
  if (note->date) /* was note->datestamp for some reason */
    STRFREE(note->date);
  if (note->text)
    STRFREE(note->text);

  note->next = note_free;
  note_free = note;
}

/* allocate memory for a new note or recycle */
/*
 * This should be redone, btw. It should work as it is, but smaug really
 * doesn't really bother much with recyling... why start now?
 * Also means redoing function above... Not difficult to redo, but your
 * choice out there.
 */
NOTE_DATA *new_note() {
  NOTE_DATA *note;

  if (note_free) {
    note = note_free;
    note_free = note_free->next;
  } else
    CREATE(note, NOTE_DATA, 1);

  note->next = NULL;
  note->sender = NULL;
  note->expire = 0;
  note->to_list = NULL;
  note->subject = NULL;
  note->date = NULL;
  note->date_stamp = 0;
  note->text = NULL;
  note->text = STRALLOC("");

  return note;
}

/* append this note to the given file */
void append_note(FILE *fp, NOTE_DATA *note) {
  fprintf(fp, "Sender  %s~\n", note->sender);
  fprintf(fp, "Date    %s~\n", note->date);
  fprintf(fp, "Stamp   %ld\n", note->date_stamp);
  fprintf(fp, "Expire  %ld\n", note->expire);
  fprintf(fp, "To      %s~\n", note->to_list);
  fprintf(fp, "Subject %s~\n", note->subject);
  fprintf(fp, "Text\n%s~\n\n", note->text);
}

/* Save a note in a given board */
void finish_note(GLOBAL_BOARD_DATA *board, NOTE_DATA *note) {
  FILE *fp;
  NOTE_DATA *p;
  char filename[200];

  /* The following is done in order to generate unique date_stamps */

  if (last_note_stamp >= current_time)
    note->date_stamp = ++last_note_stamp;
  else {
    note->date_stamp = current_time;
    last_note_stamp = current_time;
  }

  if (board->note_first) /* are there any notes in there now? */
  {
    for (p = board->note_first; p->next; p = p->next)
      ; /* empty */

    p->next = note;
  } else /* nope. empty list. */
    board->note_first = note;

  /* append note to note file */

  sprintf(filename, "%s%s", NOTE_DIR, board->short_name);

  fp = fopen(filename, "a");
  if (!fp) {
    bug("Could not open one of the note files in append mode", 0);
    board->changed = true; /* set it to true hope it will be OK later? */
    return;
  }

  append_note(fp, note);
  fclose(fp);
}

/* Find the number of a board */
int board_number(const GLOBAL_BOARD_DATA *board) {
  int i;

  for (i = 0; i < MAX_BOARD; i++)
    if (board == &boards[i])
      return i;

  return -1;
}

/* Find a board number based on  a string */
int board_lookup(const char *name) {
  int i;

  for (i = 0; i < MAX_BOARD; i++)
    if (!str_cmp(boards[i].short_name, name))
      return i;

  return -1;
}

/* Remove list from the list. Do not free note */
void unlink_note(GLOBAL_BOARD_DATA *board, NOTE_DATA *note) {
  NOTE_DATA *p;

  if (board->note_first == note)
    board->note_first = note->next;
  else {
    for (p = board->note_first; p && p->next != note; p = p->next)
      ;
    if (!p)
      bug("unlink_note: could not find note.", 0);
    else
      p->next = note->next;
  }
}

/* Find the nth note on a board. Return NULL if ch has no access to that note */
NOTE_DATA *find_note(CHAR_DATA *ch, GLOBAL_BOARD_DATA *board, int num) {
  int count = 0;
  NOTE_DATA *p;

  for (p = board->note_first; p; p = p->next)
    if (++count == num)
      break;

  if ((count == num) && is_note_to(ch, p))
    return p;
  else
    return NULL;
}

/* save a single board */
void save_board(GLOBAL_BOARD_DATA *board) {
  FILE *fp;
  char filename[200];
  char buf[200];
  NOTE_DATA *note;

  sprintf(filename, "%s%s", NOTE_DIR, board->short_name);

  fp = fopen(filename, "w");
  if (!fp) {
    sprintf(buf, "Error writing to: %s", filename);
    bug(buf, 0);
  } else {
    for (note = board->note_first; note; note = note->next)
      append_note(fp, note);

    fclose(fp);
  }
}

/* Show one note to a character */
void show_note_to_char(CHAR_DATA *ch, NOTE_DATA *note, int num) {
  char buf[4 * MAX_STRING_LENGTH];

  /* Ugly colors ? */
  sprintf(buf,
          "\n\r[&W%4d&w] &W&Y%s&w: &g%s&w\n\r"
          "&W&YDate&w:  %s\n\r"
          "&W&YTo&w:    %s\n\r"
          "&g===========================================================================&w\n\r"
          "%s\n\r",
          num, note->sender, note->subject,
          note->date,
          note->to_list,
          note->text);

  send_to_char_color(buf, ch);
}

/* Save changed boards */
void save_notes() {
  int i;

  for (i = 0; i < MAX_BOARD; i++)
    if (boards[i].changed) /* only save changed boards */
      save_board(&boards[i]);
}

/* Load a single board */
void load_board(GLOBAL_BOARD_DATA *board) {
  FILE *fp;
  NOTE_DATA *last_note;
  char filename[200];

  sprintf(filename, "%s%s", NOTE_DIR, board->short_name);

  fp = fopen(filename, "r");

  /* Silently return */
  if (!fp)
    return;

  /* Start note fetching. copy of db.c:load_notes() */

  last_note = NULL;

  for (;;) {
    NOTE_DATA *pnote;
    char letter;

    do {
      letter = getc(fp);
      if (feof(fp)) {
        fclose(fp);
        return;
      }
    } while (isspace(letter));
    ungetc(letter, fp);

    CREATE(pnote, NOTE_DATA, sizeof(*pnote));

    if (str_cmp(fread_word(fp), "sender"))
      break;
    pnote->sender = fread_string(fp);

    if (str_cmp(fread_word(fp), "date"))
      break;
    pnote->date = fread_string(fp);

    if (str_cmp(fread_word(fp), "stamp"))
      break;
    pnote->date_stamp = fread_number(fp);

    if (str_cmp(fread_word(fp), "expire"))
      break;
    pnote->expire = fread_number(fp);

    if (str_cmp(fread_word(fp), "to"))
      break;
    pnote->to_list = fread_string(fp);

    if (str_cmp(fread_word(fp), "subject"))
      break;
    pnote->subject = fread_string(fp);

    if (str_cmp(fread_word(fp), "text"))
      break;
    pnote->text = fread_string(fp);

    pnote->next = NULL; /* jic */

    /* Should this note be archived right now ? */
    if (pnote->expire < current_time) {
      free_global_note(pnote);
      board->changed = true;
      continue;
    }

    if (board->note_first == NULL)
      board->note_first = pnote;
    else
      last_note->next = pnote;

    last_note = pnote;
  }

  bug("Load_notes: bad key word.", 0);
  return; /* just return */
}

/* Initialize structures. Load all boards. */
void load_global_boards() {
  int i;

  for (i = 0; i < MAX_BOARD; i++)
    load_board(&boards[i]);
}

/* Returns true if the specified note is address to ch */
bool is_note_to(CHAR_DATA *ch, NOTE_DATA *note) {
  if (!str_cmp(ch->name, note->sender))
    return true;

  if (is_full_name("all", note->to_list))
    return true;

  if (IS_IMMORTAL(ch) && (is_full_name("imm", note->to_list) ||
                          is_full_name("imms", note->to_list) ||
                          is_full_name("immortal", note->to_list) ||
                          is_full_name("god", note->to_list) ||
                          is_full_name("gods", note->to_list) ||
                          is_full_name("admin", note->to_list) ||
                          is_full_name("admins", note->to_list) ||
                          is_full_name("administrators", note->to_list) ||
                          is_full_name("immortals", note->to_list)))
    return true;

  if ((get_trust(ch) == MAX_LEVEL) && (is_full_name("imp", note->to_list) ||
                                       is_full_name("imps", note->to_list) ||
                                       is_full_name("implementor", note->to_list) ||
                                       is_full_name("owner", note->to_list) ||
                                       is_full_name("owners", note->to_list) ||
                                       is_full_name("implementors", note->to_list)))
    return true;

  if (get_trust(ch) >= sysdata.read_all_mail)
    return true;

  if (is_full_name(ch->name, note->to_list))
    return true;

  if (!IS_NPC(ch)) {
    if (ch->pcdata->clan && is_full_name(ch->pcdata->clan->name, note->to_list))
      return true;
  }
  /* Allow a note to e.g. 40 to send to characters level 40 and above */
  if (is_number(note->to_list) && get_trust(ch) >= atoi(note->to_list))
    return true;

  return false;
}

/* Returns true if the specified note is ch->name is in the to list */
bool is_note_to_personal(CHAR_DATA *ch, NOTE_DATA *note) {
  if (is_full_name(ch->name, note->to_list))
    return true;

  return false;
}

/* Returns true if the specified note is to to imms and ch is an imm */
bool is_note_to_immort(CHAR_DATA *ch, NOTE_DATA *note) {
  if (!IS_IMMORTAL(ch))
    return false;

  if (IS_IMMORTAL(ch) && (is_full_name("imm", note->to_list) ||
                          is_full_name("imms", note->to_list) ||
                          is_full_name("immortal", note->to_list) ||
                          is_full_name("god", note->to_list) ||
                          is_full_name("gods", note->to_list) ||
                          is_full_name("admin", note->to_list) ||
                          is_full_name("admins", note->to_list) ||
                          is_full_name("administrators", note->to_list) ||
                          is_full_name("immortals", note->to_list)))
    return true;

  if ((get_trust(ch) == MAX_LEVEL) && (is_full_name("imp", note->to_list) ||
                                       is_full_name("imps", note->to_list) ||
                                       is_full_name("implementor", note->to_list) ||
                                       is_full_name("owner", note->to_list) ||
                                       is_full_name("owners", note->to_list) ||
                                       is_full_name("implementors", note->to_list)))
    return true;

  /* Allow a note to e.g. 40 to send to characters level 40 and above */
  if (is_number(note->to_list) && get_trust(ch) >= atoi(note->to_list))
    return true;

  return false;
}

/* Returns true if the specified note is to ch's clan */
bool is_note_to_clan(CHAR_DATA *ch, NOTE_DATA *note) {
  if (!IS_NPC(ch)) {
    if (ch->pcdata->clan && is_full_name(ch->pcdata->clan->name, note->to_list))
      return true;
  }

  return false;
}

/* Return the number of unread notes 'ch' has in 'board' */
/* Returns BOARD_NOACCESS if ch has no access to board */
int unread_notes(CHAR_DATA *ch, GLOBAL_BOARD_DATA *board) {
  NOTE_DATA *note;
  time_t last_read;
  int count = 0;

  if (board->read_level > get_trust(ch))
    return BOARD_NOACCESS;

  last_read = ch->pcdata->last_note[board_number(board)];

  for (note = board->note_first; note; note = note->next)
    if (is_note_to(ch, note) && ((long)last_read < (long)note->date_stamp))
      count++;

  return count;
}

/* Return the number of unread personal notes 'ch' has in 'board' */
/* Returns BOARD_NOACCESS if ch has no access to board */
int unread_personal_notes(CHAR_DATA *ch, GLOBAL_BOARD_DATA *board) {
  NOTE_DATA *note;
  time_t last_read;
  int count = 0;

  if (board->read_level > get_trust(ch))
    return BOARD_NOACCESS;

  last_read = ch->pcdata->last_note[board_number(board)];

  for (note = board->note_first; note; note = note->next)
    if (is_note_to_personal(ch, note) && ((long)last_read < (long)note->date_stamp))
      count++;

  return count;
}

/* Return the number of unread immortal notes 'ch' has in 'board' */
/* Returns BOARD_NOACCESS if ch has no access to board */
int unread_immortal_notes(CHAR_DATA *ch, GLOBAL_BOARD_DATA *board) {
  NOTE_DATA *note;
  time_t last_read;
  int count = 0;

  if (board->read_level > get_trust(ch))
    return BOARD_NOACCESS;

  last_read = ch->pcdata->last_note[board_number(board)];

  for (note = board->note_first; note; note = note->next)
    if (is_note_to_immort(ch, note) && ((long)last_read < (long)note->date_stamp))
      count++;

  return count;
}

/* Return the number of unread clan notes 'ch' has in 'board' */
/* Returns BOARD_NOACCESS if ch has no access to board */
int unread_clan_notes(CHAR_DATA *ch, GLOBAL_BOARD_DATA *board) {
  NOTE_DATA *note;
  time_t last_read;
  int count = 0;

  if (board->read_level > get_trust(ch))
    return BOARD_NOACCESS;

  last_read = ch->pcdata->last_note[board_number(board)];

  for (note = board->note_first; note; note = note->next)
    if (is_note_to_clan(ch, note) && ((long)last_read < (long)note->date_stamp))
      count++;

  return count;
}

/*
 * COMMANDS
 */

/* Start writing a note */
void do_nwrite(CHAR_DATA *ch, char *argument) {
  char *strtime;
  char buf[200];

  if (IS_NPC(ch)) /* NPC cannot post notes */
    return;

  if ((ch->pcdata->board->write_level > get_trust(ch)) || (!ch->pcdata->clan && ch->pcdata->board->access == BOARD_CLAN_ONLY && !IS_IMMORTAL(ch)) || (!xIS_SET(ch->act, PLR_CAN_CHAT) && !IS_IMMORTAL(ch) && ch->pcdata->board->access == BOARD_AUTHED_BIO)) {
    send_to_char_color("You cannot post notes on this board.\n\r", ch);
    return;
  }

  /* continue previous note, if any text was written*/
  if (ch->pcdata->in_progress && (!ch->pcdata->in_progress->text || ch->pcdata->in_progress->text[0] == '\0')) {
    send_to_char_color(
        "Note in progress cancelled because you did not manage to write any text \n\r"
        "before losing link.\n\r\n\r",
        ch);
    free_global_note(ch->pcdata->in_progress);
    ch->pcdata->in_progress = NULL;
  }

  if (!ch->pcdata->in_progress) {
    ch->pcdata->in_progress = new_note();
    ch->pcdata->in_progress->sender = STRALLOC(ch->name);

    /* convert to ascii. ctime returns a string which last character is \n, so remove that */
    strtime = ctime(&current_time);
    strtime[strlen(strtime) - 1] = '\0';

    ch->pcdata->in_progress->date = STRALLOC(strtime);
  }

  act(AT_GREEN, "$n starts writing a note.&w", ch, NULL, NULL, TO_ROOM);

  /* Begin writing the note ! */
  sprintf(buf,
          "You are now %s a new note on the &W%s&w board.\n\r"
          "If you are using tintin, type #verbose to turn off alias expansion!\n\r\n\r",
          ch->pcdata->in_progress->text ? "continuing" : "posting",
          ch->pcdata->board->short_name);
  send_to_char_color(buf, ch);

  sprintf(buf, "&YFrom&w:    %s\n\r\n\r", ch->name);
  send_to_char_color(buf, ch);

  if (!ch->pcdata->in_progress->text || ch->pcdata->in_progress->text[0] == '\0') /* Are we continuing an old note or not? */
  {
    switch (ch->pcdata->board->force_type) {
      case DEF_NORMAL:
        sprintf(buf, "If you press Return, default recipient \"&W%s&w\" will be chosen.\n\r",
                ch->pcdata->board->names);
        break;
      case DEF_INCLUDE:
        sprintf(buf, "The recipient list MUST include \"&W%s&w\". If not, it will be added automatically.\n\r",
                ch->pcdata->board->names);
        break;

      case DEF_EXCLUDE:
        sprintf(buf, "The recipient of this note must NOT include: \"&W%s&w\".\n\rUse '&WAdmin&w' as the recipient to send this message to an administrator.\n\r",
                ch->pcdata->board->names);

        break;
    }

    send_to_char_color(buf, ch);
    send_to_char_color("\n\r&YTo&w:      ", ch);

    ch->desc->connected = CON_NOTE_TO;
    /* nanny takes over from here */

  } else /* we are continuing, print out all the fields and the note so far*/
  {
    sprintf(buf,
            "&YTo&w:      %s\n\r"
            "&YExpires&w: %s\n\r"
            "&YSubject&w: %s\n\r",
            ch->pcdata->in_progress->to_list,
            ctime(&ch->pcdata->in_progress->expire),
            ch->pcdata->in_progress->subject);
    send_to_char_color(buf, ch);
    send_to_char_color("&GYour note so far:\n\r&w", ch);
    send_to_char_color(ch->pcdata->in_progress->text, ch);

    //        send_to_char_color ("\n\rEnter text. Type &W~&w or &WEND&w on an empty line to end note.\n\r=======================================================\n\r",ch);

    ch->desc->connected = CON_NOTE_TEXT;
    if (ch->substate == SUB_NONE) {
      ch->substate = SUB_GNOTE;
      ch->dest_buf = ch;
      start_editing(ch, ch->pcdata->in_progress->text);
    }
  }
}

/* Read next note in current group. If no more notes, go to next board */
void do_nread(CHAR_DATA *ch, char *argument) {
  NOTE_DATA *p;
  int count = 0, number;
  time_t *last_note = &ch->pcdata->last_note[board_number(ch->pcdata->board)];

  if (!str_cmp(argument, "again")) { /* read last note again */

  } else if (is_number(argument)) {
    number = atoi(argument);

    for (p = ch->pcdata->board->note_first; p; p = p->next)
      if (++count == number)
        break;

    if (!p || !is_note_to(ch, p))
      send_to_char_color("No such note.\n\r", ch);
    else {
      show_note_to_char(ch, p, count);
      *last_note = UMAX(*last_note, p->date_stamp);
    }
  } else /* just next one */
  {
    char buf[200];

    count = 1;

    for (p = ch->pcdata->board->note_first; p; p = p->next, count++)
      if ((p->date_stamp > *last_note) && is_note_to(ch, p)) {
        show_note_to_char(ch, p, count);
        /* Advance if new note is newer than the currently newest for that char */
        *last_note = UMAX(*last_note, p->date_stamp);
        return;
      }

    send_to_char_color("No new notes in this board.\n\r", ch);

    if (next_board(ch))
      sprintf(buf, "Changed to next board, %s.\n\r", ch->pcdata->board->short_name);
    else
      sprintf(buf, "There are no more boards.\n\r");

    send_to_char_color(buf, ch);
  }
}

/* Remove a note */
void do_nremove(CHAR_DATA *ch, char *argument) {
  NOTE_DATA *p;

  if (!is_number(argument)) {
    send_to_char_color("Remove which note?\n\r", ch);
    return;
  }

  p = find_note(ch, ch->pcdata->board, atoi(argument));
  if (!p) {
    send_to_char_color("No such note.\n\r", ch);
    return;
  }

  if (str_cmp(ch->name, p->sender) && (get_trust(ch) < LEVEL_SUB_IMPLEM)) {
    send_to_char_color("You are not authorized to remove this note.\n\r", ch);
    return;
  }

  unlink_note(ch->pcdata->board, p);
  free_global_note(p);
  send_to_char_color("Note removed!\n\r", ch);

  save_board(ch->pcdata->board); /* save the board */
}

/* List all notes or if argument given, list N of the last notes */
/* Added in 'to' and 'from' for arguments, so you can see just */
/* specific subsets of notes -- Melora */
/* Shows REAL note numbers! */
void do_nlist(CHAR_DATA *ch, char *argument) {
  int count = 0, show = 0, num = 0, has_shown = 0;
  time_t last_note;
  NOTE_DATA *p;
  char buf[MAX_STRING_LENGTH];
  char arg[MAX_STRING_LENGTH];
  char arg2[MAX_STRING_LENGTH];
  bool showTo = false, showFrom = false, showNew = false;

  if (is_number(argument)) /* first, count the number of notes */
  {
    show = atoi(argument);

    for (p = ch->pcdata->board->note_first; p; p = p->next)
      if (is_note_to(ch, p))
        count++;
  }

  /* Check to see if the argument is 'to', 'from' or 'new' */
  argument = one_argument(argument, arg);
  if (!str_cmp(arg, "to"))
    showTo = true;
  else if (!str_cmp(arg, "from"))
    showFrom = true;
  else if (!str_cmp(arg, "new"))
    showNew = true;

  if (showTo && argument[0] == '\0') {
    if (IS_IMMORTAL(ch)) {
      send_to_char_color("&WYou must provide a name for 'to'.\n\r", ch);
      return;
    }
  } else if (showFrom && argument[0] == '\0') {
    send_to_char_color("&WYou must provide a name for 'from'.\n\r", ch);
    return;
  } else if ((showTo || showFrom) && argument[0] != '\0') {
    argument = one_argument(argument, arg2);
  }

  send_to_char_color(
      "&WNotes on this board:&w\n\r"
      "&rNum> Author        Subject&w\n\r",
      ch);

  last_note = ch->pcdata->last_note[board_number(ch->pcdata->board)];

  for (p = ch->pcdata->board->note_first; p; p = p->next) {
    num++;
    if (is_note_to(ch, p)) {
      has_shown++; /* note that we want to see X VISIBLE note, not just last X */
      if (showTo) {
        if ((!IS_IMMORTAL(ch) &&
             (is_note_to_personal(ch, p) || is_note_to_clan(ch, p))) ||
            (IS_IMMORTAL(ch) && is_full_name(arg2, p->to_list))) {
          sprintf(buf, "&W%3d&w> &B%c&w &Y%-13s&w &Y%s&w \n\r",
                  num,
                  last_note < p->date_stamp ? '*' : ' ',
                  p->sender, p->subject);
          send_to_char_color(buf, ch);
        }
      } else if (showFrom) {
        if (is_full_name(arg2, p->sender)) {
          sprintf(buf, "&W%3d&w> &B%c&w &Y%-13s&w &Y%s&w \n\r",
                  num,
                  last_note < p->date_stamp ? '*' : ' ',
                  p->sender, p->subject);
          send_to_char_color(buf, ch);
        }
      } else if (showNew) {
        if ((long)last_note < (long)p->date_stamp) {
          sprintf(buf, "&W%3d&w> &B%c&w &Y%-13s&w &Y%s&w \n\r",
                  num,
                  last_note < p->date_stamp ? '*' : ' ',
                  p->sender, p->subject);
          send_to_char_color(buf, ch);
        }
      } else if (!show || ((count - show) < has_shown)) {
        sprintf(buf, "&W%3d&w> &B%c&w &Y%-13s&w &Y%s&w \n\r",
                num,
                last_note < p->date_stamp ? '*' : ' ',
                p->sender, p->subject);
        send_to_char_color(buf, ch);
      }
    }
  }
}

/* catch up with some notes */
void do_ncatchup(CHAR_DATA *ch, char *argument) {
  NOTE_DATA *p;

  /* Find last note */
  for (p = ch->pcdata->board->note_first; p && p->next; p = p->next)
    ;

  if (!p)
    send_to_char_color("Alas, there are no notes in that board.\n\r", ch);
  else {
    ch->pcdata->last_note[board_number(ch->pcdata->board)] = p->date_stamp;
    send_to_char_color("All mesages skipped.\n\r", ch);
  }
}

/* Dispatch function for backwards compatibility */
void do_global_note(CHAR_DATA *ch, char *argument) {
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  if (ch->substate == SUB_GNOTE) {
    handle_con_note_text(ch->desc, NULL);
    return;
  }
  argument = one_argument(argument, arg);

  if ((!arg[0]) || (!str_cmp(arg, "read"))) /* 'note' or 'note read X' */
    do_nread(ch, argument);

  else if (!str_cmp(arg, "list"))
    do_nlist(ch, argument);

  else if (!str_cmp(arg, "write")) {
    if (xIS_SET(ch->act, PLR_NOGBOARD)) {
      send_to_char("You can't do that.\n\r", ch);
      return;
    }
    do_nwrite(ch, argument);
  } else if (!str_cmp(arg, "remove"))
    do_nremove(ch, argument);

  else if (!str_cmp(arg, "purge"))
    send_to_char_color("Obsolete.\n\r", ch);

  else if (!str_cmp(arg, "archive"))
    send_to_char_color("Obsolete.\n\r", ch);

  else if (!str_cmp(arg, "catchup"))
    do_ncatchup(ch, argument);
  else
    do_help(ch, "note");
}

/* Show all accessible boards with their numbers of unread messages OR
 change board. New board name can be given as a number or as a name (e.g.
 board personal or board 4 */
void do_global_boards(CHAR_DATA *ch, char *argument) {
  int i, count, number;
  char buf[200];

  if (IS_NPC(ch))
    return;

  if (!argument[0]) /* show boards */
  {
    int unread, unread_pers, unread_imm, unread_clan;

    count = 1;
    ch_printf(ch, "\n\r");
    if (IS_IMMORTAL(ch)) {
      send_to_char_color(
          "&RNum         Name Unread Personal Clan Immort Description&w\n\r"
          "&r=== ============ ====== ======== ==== ====== ===========&w\n\r",
          ch);
    } else {
      send_to_char_color(
          "&RNum         Name Unread Personal Clan Description&w\n\r"
          "&r=== ============ ====== ======== ==== ===========&w\n\r",
          ch);
    }
    for (i = 0; i < MAX_BOARD; i++) {
      unread = unread_notes(ch, &boards[i]);               /* how many unread notes? */
      unread_pers = unread_personal_notes(ch, &boards[i]); /* personal notes? */
      unread_imm = unread_immortal_notes(ch, &boards[i]);  /* immortal notes? */
      unread_clan = unread_clan_notes(ch, &boards[i]);     /* clan notes? */
      if (unread != BOARD_NOACCESS) {
        if (IS_IMMORTAL(ch)) {
          sprintf(buf, "&W%2d&w> &G%12s&w [%s%4d&w] [%s%6d&w] [%s%2d&w] [%s%4d&w] &Y%s&w\n\r",
                  count, boards[i].short_name,
                  unread ? "&r" : "&g", unread,
                  unread_pers ? "&r" : "&g", unread_pers,
                  unread_clan ? "&r" : "&g", unread_clan,
                  unread_imm ? "&r" : "&g", unread_imm,
                  boards[i].long_name);
        } else {
          sprintf(buf, "&W%2d&w> &G%12s&w [%s%4d&w] [%s%6d&w] [%s%2d&w] &Y%s&w\n\r",
                  count, boards[i].short_name,
                  unread ? "&r" : "&g", unread,
                  unread_pers ? "&r" : "&g", unread_pers,
                  unread_clan ? "&r" : "&g", unread_clan,
                  boards[i].long_name);
        }
        send_to_char_color(buf, ch);
        count++;
      } /* if has access */

    } /* for each board */

    if (ch->pcdata->board == NULL)
      ch->pcdata->board = &boards[DEFAULT_BOARD];
    sprintf(buf, "\n\rYou current board is &W%s&w.\n\r", ch->pcdata->board->short_name);
    send_to_char_color(buf, ch);

    /* Inform of rights */
    if (ch->pcdata->board->read_level > get_trust(ch))
      send_to_char_color("You cannot read nor write notes on this board.\n\r", ch);
    else if ((ch->pcdata->board->write_level > get_trust(ch)) || (!ch->pcdata->clan && ch->pcdata->board->access == BOARD_CLAN_ONLY && !IS_IMMORTAL(ch)) || (!xIS_SET(ch->act, PLR_CAN_CHAT) && !IS_IMMORTAL(ch) && ch->pcdata->board->access == BOARD_AUTHED_BIO))
      send_to_char_color("You can only read notes from this board.\n\r", ch);
    else
      send_to_char_color("You can both read and write on this board.\n\r", ch);

    return;
  } /* if empty argument */

  if (ch->pcdata->in_progress) {
    send_to_char_color("Please finish your interrupted note first.\n\r", ch);
    return;
  }

  /* Change board based on its number */
  if (is_number(argument)) {
    count = 0;
    number = atoi(argument);
    for (i = 0; i < MAX_BOARD; i++)
      if (unread_notes(ch, &boards[i]) != BOARD_NOACCESS)
        if (++count == number)
          break;

    if (count == number) /* found the board.. change to it */
    {
      ch->pcdata->board = &boards[i];
      sprintf(buf, "Current board changed to &W%s&w. ", boards[i].short_name);
      send_to_char_color(buf, ch);
      if ((get_trust(ch) < boards[i].write_level) || (!ch->pcdata->clan && boards[i].access == BOARD_CLAN_ONLY && !IS_IMMORTAL(ch)) || (!xIS_SET(ch->act, PLR_CAN_CHAT) && !IS_IMMORTAL(ch) && boards[i].access == BOARD_AUTHED_BIO))
        sprintf(buf, "You can only read here.\n\r");
      else
        sprintf(buf, "You can both read and write here.\n\r");
      send_to_char_color(buf, ch);
    } else /* so such board */
      send_to_char_color("No such board.\n\r", ch);

    return;
  }

  /* Non-number given, find board with that name */

  for (i = 0; i < MAX_BOARD; i++)
    if (!str_cmp(boards[i].short_name, argument))
      break;

  if (i == MAX_BOARD) {
    send_to_char_color("No such board.\n\r", ch);
    return;
  }

  /* Does ch have access to this board? */
  if (unread_notes(ch, &boards[i]) == BOARD_NOACCESS) {
    send_to_char_color("No such board.\n\r", ch);
    return;
  }

  ch->pcdata->board = &boards[i];
  ch->pcdata->board = &boards[i];
  sprintf(buf, "Current board changed to &W%s&w. ", boards[i].short_name);
  send_to_char_color(buf, ch);
  if ((get_trust(ch) < boards[i].write_level) || (!ch->pcdata->clan && boards[i].access == BOARD_CLAN_ONLY && !IS_IMMORTAL(ch)) || (!xIS_SET(ch->act, PLR_CAN_CHAT) && !IS_IMMORTAL(ch) && boards[i].access == BOARD_AUTHED_BIO))
    sprintf(buf, "You can only read here.\n\r");
  else
    sprintf(buf, "You can both read and write here.\n\r");
  send_to_char_color(buf, ch);
}

/* Send a note to someone on the personal board */
void personal_message(const char *sender, const char *to, const char *subject, const int expire_days, const char *text) {
  make_note("Personal", sender, to, subject, expire_days, text);
}

void make_note(const char *board_name, const char *sender, const char *to, const char *subject, const int expire_days, const char *text) {
  int board_index = board_lookup(board_name);
  GLOBAL_BOARD_DATA *board;
  NOTE_DATA *note;
  char *strtime;

  if (board_index == BOARD_NOTFOUND) {
    bug("make_note: board not found", 0);
    return;
  }

  if (strlen(text) > MAX_NOTE_TEXT) {
    bug("make_note: text too long (%d bytes)", strlen(text));
    return;
  }

  board = &boards[board_index];

  note = new_note(); /* allocate new note */

  note->sender = STRALLOC((char *)sender);
  note->to_list = STRALLOC((char *)to);
  note->subject = STRALLOC((char *)subject);
  note->expire = current_time + expire_days * 60 * 60 * 24;
  note->text = STRALLOC((char *)text);

  /* convert to ascii. ctime returns a string which last character is \n, so remove that */
  strtime = ctime(&current_time);
  strtime[strlen(strtime) - 1] = '\0';

  note->date = STRALLOC(strtime);

  finish_note(board, note);
}

/* tries to change to the next accessible board */
bool next_board(CHAR_DATA *ch) {
  int i = board_number(ch->pcdata->board) + 1;

  while ((i < MAX_BOARD) && (unread_notes(ch, &boards[i]) == BOARD_NOACCESS))
    i++;

  if (i == MAX_BOARD) {
    /* Would it be better to just start over? */
    ch->pcdata->board = &boards[0];
    return true;
  } else {
    ch->pcdata->board = &boards[i];
    return true;
  }
}

void handle_con_note_to(DESCRIPTOR_DATA *d, char *argument) {
  char buf[MAX_INPUT_LENGTH];
  CHAR_DATA *ch = d->character;

  if (!ch->pcdata->in_progress) {
    d->connected = CON_PLAYING;
    bug("nanny: In CON_NOTE_TO, but no note in progress", 0);
    return;
  }

  strcpy(buf, argument);
  smash_tilde(buf); /* change ~ to - as we save this field as a string later */

  switch (ch->pcdata->board->force_type) {
    case DEF_NORMAL: /* default field */
      if (!buf[0])   /* empty string? */
      {
        ch->pcdata->in_progress->to_list = STRALLOC(ch->pcdata->board->names);
        sprintf(buf, "Assumed default recipient: &W%s&w\n\r", ch->pcdata->board->names);
        send_to_char_color(buf, ch);
      } else
        ch->pcdata->in_progress->to_list = STRALLOC(buf);

      break;

    case DEF_INCLUDE: /* forced default */
      if (!is_full_name(ch->pcdata->board->names, buf)) {
        strcat(buf, " ");
        strcat(buf, ch->pcdata->board->names);
        ch->pcdata->in_progress->to_list = STRALLOC(buf);

        sprintf(buf,
                "\n\rYou did not specify %s as recipient, so it was automatically added.\n\r"
                "&YNew To&w :  %s&w\n\r",
                ch->pcdata->board->names, ch->pcdata->in_progress->to_list);
        send_to_char_color(buf, ch);
      } else
        ch->pcdata->in_progress->to_list = STRALLOC(buf);
      break;

    case DEF_EXCLUDE: /* forced exclude */
      if (!buf[0]) {
        send_to_char_color(
            "You must specify a recipient.\n\r"
            "&YTo&w:      ",
            ch);
        return;
      }

      if (is_full_name(ch->pcdata->board->names, buf)) {
        sprintf(buf,
                "You are not allowed to send notes to %s on this board. Try again.\n\r"
                "&YTo&w:      ",
                ch->pcdata->board->names);
        send_to_char_color(buf, ch);
        return; /* return from nanny, not changing to the next state! */
      } else
        ch->pcdata->in_progress->to_list = STRALLOC(buf);
      break;
  }

  send_to_char_color("&Y\n\rSubject&w: ", ch);
  d->connected = CON_NOTE_SUBJECT;
}

void handle_con_note_subject(DESCRIPTOR_DATA *d, char *argument) {
  char buf[MAX_INPUT_LENGTH];
  CHAR_DATA *ch = d->character;

  if (!ch->pcdata->in_progress) {
    d->connected = CON_PLAYING;
    bug("nanny: In CON_NOTE_SUBJECT, but no note in progress", 0);
    return;
  }

  strcpy(buf, argument);
  smash_tilde(buf); /* change ~ to - as we save this field as a string later */

  /* Do not allow empty subjects */

  if (!buf[0]) {
    send_to_char_color("Please find a meaningful subject!\n\r", ch);
    send_to_char_color("&YSubject&w: ", ch);
  } else if (strlen(buf) > 60) {
    send_to_char_color("No, no. This is just the Subject. You're note writing the note yet. Twit.\n\r", ch);
  } else
  /* advance to next stage */
  {
    ch->pcdata->in_progress->subject = STRALLOC(buf);
    if (IS_IMMORTAL(ch)) /* immortals get to choose number of expire days */
    {
      sprintf(buf,
              "\n\rHow many days do you want this note to expire in?\n\r"
              "Press Enter for default value for this board, &W%d&w days.\n\r"
              "&YExpire&w:  ",
              ch->pcdata->board->purge_days);
      send_to_char_color(buf, ch);
      d->connected = CON_NOTE_EXPIRE;
    } else {
      ch->pcdata->in_progress->expire =
          current_time + ch->pcdata->board->purge_days * 24L * 3600L;
      sprintf(buf, "This note will expire %s\r", ctime(&ch->pcdata->in_progress->expire));
      send_to_char_color(buf, ch);
      //            send_to_char_color ( "\n\rEnter text. Type &W~&w or &WEND&w on an empty line to end note.\n\r=======================================================\n\r",ch);
      d->connected = CON_NOTE_TEXT;
      if (ch->substate == SUB_NONE) {
        ch->substate = SUB_GNOTE;
        ch->dest_buf = ch;
        start_editing(ch, ch->pcdata->in_progress->text);
      }
    }
  }
}

void handle_con_note_expire(DESCRIPTOR_DATA *d, char *argument) {
  CHAR_DATA *ch = d->character;
  char buf[MAX_STRING_LENGTH];
  time_t expire;
  int days;

  if (!ch->pcdata->in_progress) {
    d->connected = CON_PLAYING;
    bug("nanny: In CON_NOTE_EXPIRE, but no note in progress", 0);
    return;
  }

  /* Numeric argument. no tilde smashing */
  strcpy(buf, argument);
  if (!buf[0]) /* assume default expire */
    days = ch->pcdata->board->purge_days;
  else if (!is_number(buf)) /* use this expire */
  {
    send_to_char_color("Write the number of days!\n\r", ch);
    send_to_char_color("&YExpire&w:  ", ch);
    return;
  } else {
    days = atoi(buf);
    if (days <= 0) {
      send_to_char_color("This is a positive MUD. Use positive numbers only! :)\n\r", ch);
      send_to_char_color("&YExpire&w:  ", ch);
      return;
    }
  }

  expire = current_time + (days * 24L * 3600L); /* 24 hours, 3600 seconds */

  ch->pcdata->in_progress->expire = expire;

  /* note that ctime returns XXX\n so we only need to add an \r */

  //    send_to_char_color ( "\n\rEnter text. Type &W~&w or &WEND&w on an empty line to end note.\n\r=======================================================\n\r",ch);

  d->connected = CON_NOTE_TEXT;
  if (ch->substate == SUB_NONE) {
    ch->substate = SUB_GNOTE;
    ch->dest_buf = ch;
    start_editing(ch, ch->pcdata->in_progress->text);
  }

  //        start_editing(ch, ch->pcdata->in_progress->text);
}

void handle_con_note_text(DESCRIPTOR_DATA *d, char *argument) {
  CHAR_DATA *ch = d->character;
  char buf[MAX_STRING_LENGTH];
  char letter[4 * MAX_STRING_LENGTH];

  if (!ch->pcdata->in_progress) {
    d->connected = CON_PLAYING;
    bug("nanny: In CON_NOTE_TEXT, but no note in progress", 0);
    return;
  }

  switch (ch->substate) {
    default:
      /*bug( "do_bio: illegal substate", 0 );*/
      return;

    case SUB_RESTRICTED:
      send_to_char("You cannot use this command from within another command.\n\r", ch);
      return;

    case SUB_GNOTE:
      ch->pcdata->in_progress->text = copy_buffer(ch);
      stop_editing(ch);
      send_to_char_color("\n\r\n\r", ch);
      send_to_char_color(szFinishPrompt, ch);
      d->connected = CON_NOTE_FINISH;
      return;
  }
  /* First, check for EndOfNote marker */

  strcpy(buf, argument);
  if ((!str_cmp(buf, "~")) || (!str_cmp(buf, "END"))) {
    send_to_char_color("\n\r\n\r", ch);
    send_to_char_color(szFinishPrompt, ch);
    send_to_char_color("\n\r", ch);
    d->connected = CON_NOTE_FINISH;
    return;
  }

  smash_tilde(buf); /* smash it now */

  /* Check for too long lines. Do not allow lines longer than 80 chars */

  if (strlen(buf) > MAX_LINE_LENGTH) {
    send_to_char_color("Too long line rejected. Do NOT go over 80 characters!\n\r", ch);
    return;
  }

  /* Not end of note. Copy current text into temp buffer, add new line, and copy back */

  /* How would the system react to strcpy( , NULL) ? */
  if (ch->pcdata->in_progress->text) {
    strcpy(letter, ch->pcdata->in_progress->text);
    STRFREE(ch->pcdata->in_progress->text);
    /*        ch->pcdata->in_progress->text = NULL; */ /* be sure we don't free it twice */
  } else
    strcpy(letter, "");

  /* Check for overflow */

  if ((strlen(letter) + strlen(buf)) > MAX_NOTE_TEXT) { /* Note too long, take appropriate steps */
    send_to_char_color("Note too long!\n\r", ch);
    free_global_note(ch->pcdata->in_progress);
    ch->pcdata->in_progress = NULL; /* important */
    d->connected = CON_PLAYING;
    return;
  }

  /* Add new line to the buffer */

  strcat(letter, buf);
  strcat(letter, "\r\n"); /* new line. \r first to make note files better readable */

  /* allocate dynamically */
  ch->pcdata->in_progress->text = STRALLOC(letter);
}

void handle_con_note_finish(DESCRIPTOR_DATA *d, char *argument) {
  CHAR_DATA *ch = d->character;

  if (!ch->pcdata->in_progress) {
    d->connected = CON_PLAYING;
    bug("nanny: In CON_NOTE_FINISH, but no note in progress", 0);
    return;
  }

  switch (tolower(argument[0])) {
    case 'c': /* keep writing */

      send_to_char_color("Continuing note...\n\r", ch);
      d->connected = CON_NOTE_TEXT;
      if (ch->substate == SUB_NONE) {
        ch->substate = SUB_GNOTE;
        ch->dest_buf = ch;
        start_editing(ch, ch->pcdata->in_progress->text);
      }
      break;

    case 'v': /* view note so far */
      if (ch->pcdata->in_progress->text) {
        send_to_char_color("&gText of your note so far:&w\n\r", ch);
        send_to_char_color(ch->pcdata->in_progress->text, ch);
      } else
        send_to_char_color("You haven't written a thing!\n\r\n\r", ch);
      send_to_char_color(szFinishPrompt, ch);
      send_to_char_color("\n\r", ch);
      break;

    case 'p': /* post note */
      finish_note(ch->pcdata->board, ch->pcdata->in_progress);
      send_to_char_color("Note posted.\n\r", ch);
      d->connected = CON_PLAYING;
      /* remove AFK status */
      ch->pcdata->in_progress = NULL;
      act(AT_GREEN, "$n finishes $s note.", ch, NULL, NULL, TO_ROOM);
      break;

    case 'f':
      send_to_char_color("Note cancelled!\n\r", ch);
      free_global_note(ch->pcdata->in_progress);
      ch->pcdata->in_progress = NULL;
      d->connected = CON_PLAYING;
      /* remove afk status */
      break;

    default: /* invalid response */
      send_to_char_color("Huh? Valid answers are:\n\r\n\r", ch);
      send_to_char_color(szFinishPrompt, ch);
      send_to_char_color("\n\r", ch);
  }
}
