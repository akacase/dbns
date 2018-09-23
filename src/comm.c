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
 *			 Low-level communication module                                 *
 ****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <getopt.h>
#include <syslog.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <locale.h>
#include "mud.h"
#include "utf8.h"
#include "sha256.h"

/* socket and TCP/IP stuff */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <netdb.h>

#define closesocket close
#define ENOSR 63
#define BERR 255
#define	INVALID	1
#define	TOOSMALL 2
#define	TOOLARGE 3

const char echo_off_str[] = {IAC, WILL, TELOPT_ECHO, '\0'};
const char echo_on_str[] = {IAC, WONT, TELOPT_ECHO, '\0'};
const char go_ahead_str[] = {IAC, GA, '\0'};

/* external Functions */
void 	shutdown_mud(char *reason);

/* global variables */
IMMORTAL_HOST *immortal_host_start;	/* Start of Immortal legal domains */
IMMORTAL_HOST *immortal_host_end;	/* End of Immortal legal domains */
DESCRIPTOR_DATA *first_descriptor;	/* First descriptor		 */
DESCRIPTOR_DATA *last_descriptor;	/* Last descriptor		 */
DESCRIPTOR_DATA *d_next;		/* Next descriptor in loop	 */
int 	num_descriptors;
bool 	mud_down;			/* Shutdown			 */
bool 	service_shut_down;		/* Shutdown by operator closing down
					 * service */
bool 	wizlock;
int 	locklev;
time_t 	boot_time;
HOUR_MIN_SEC set_boot_time_struct;
HOUR_MIN_SEC *set_boot_time;
struct tm *new_boot_time;
struct tm new_boot_struct;
char 	str_boot_time[MAX_INPUT_LENGTH];
char 	lastplayercmd[MAX_INPUT_LENGTH * 2];
time_t 	current_time;			/* Time of this pulse		 */
int 	control;			/* Controlling descriptor	 */
int 	newdesc;			/* New descriptor		 */
fd_set 	in_set;				/* Set of desc's for reading	 */
fd_set 	out_set;			/* Set of desc's for writing	 */
fd_set 	exc_set;			/* Set of desc's with errors	 */
int 	maxdesc;
char   *alarm_section = "(unknown)";

/* protos */
void 	game_loop();
int 	init_socket(int port);
void 	new_descriptor(int new_desc);
bool 	read_from_descriptor(DESCRIPTOR_DATA * d);
bool 	write_to_descriptor(int desc, char *txt, int length);
bool 	check_parse_name(char *name, bool newchar);
bool    check_reconnect(DESCRIPTOR_DATA *d, char *name, bool f_conn);
bool	check_playing (DESCRIPTOR_DATA *d, char *name, bool kick);
bool 	reconnect(DESCRIPTOR_DATA * d, char *name);
int 	main(int argc, char **argv);
void	stop_idling(CHAR_DATA *ch );
void 	nanny(DESCRIPTOR_DATA * d, char *argument);
bool 	flush_buffer(DESCRIPTOR_DATA * d, bool f_prompt);
void 	read_from_buffer(DESCRIPTOR_DATA * d);
void 	free_desc(DESCRIPTOR_DATA * d);
void 	display_prompt(DESCRIPTOR_DATA * d);
int 	make_color_sequence(const char *col, char *buf, DESCRIPTOR_DATA * d);
void 	mail_count(CHAR_DATA * ch);
void 	tax_player(CHAR_DATA * ch);
void 	mccp_interest(CHAR_DATA * ch);
bool 	check_total_ip(DESCRIPTOR_DATA * dnew);
bool	char_exists(char *fp);
bool 	check_pfile(DESCRIPTOR_DATA * d);
void    p_to_fp(char *p, char *fp);
void    load_from_fp(DESCRIPTOR_DATA * d, bool np);
void 	usage(void);
long long strtonum(const char *numstr, long long minval, long long maxval,
	 const char **errstrp);

/* locals */
unsigned int port;

void
usage(void)
{
	fprintf(stderr, "usage: dbnsd [-Dp]\n");
	exit(1);
}

char
capitalize_string(char *text)
{
	int 	i;

	if (text[0] == '\0')
		return (*text);

	text[0] = UPPER(text[0]);

	for (i = 1; text[i] != '\0'; i++) {
		text[i] = LOWER(text[i]);
	}

	return (*text);
}

void
send_to_desc_color(const char *txt, DESCRIPTOR_DATA * d)
{
	char   *colstr;
	const char *prevstr = txt;
	char 	colbuf[20];
	int 	ln;

	if (!d) {
		bug("send_to_desc_color: NULL *d");
		return;
	}
	if (!txt || !d->descriptor)
		return;

	while ((colstr = strpbrk(prevstr, "&^}")) != NULL) {
		if (colstr > prevstr)
			write_to_buffer(d, prevstr, (colstr - prevstr));
		ln = make_color_sequence(colstr, colbuf, d);
		if (ln < 0) {
			prevstr = colstr + 1;
			break;
		} else if (ln > 0)
			write_to_buffer(d, colbuf, ln);
		prevstr = colstr + 2;
	}
	if (*prevstr)
		write_to_buffer(d, prevstr, 0);
	return;
}

int
main(int argc, char **argv)
{
	struct timeval now_time;
	char 	hostn[128];
	const char *errstr = NULL;
	int 	ch;

	while ((ch = getopt(argc, argv, "Dp:")) != -1) {
		switch (ch) {
		case 'p':
			/*
			 * minval and maxval correspond to unpriveledged
			 * ports
			 */
			port = strtonum(optarg, 1024, 65535, &errstr);
			if (errstr)
				usage();
			break;
		default:
			usage();
		}

	}

	setlocale(LC_ALL, "en_US.UTF-8");
	DONT_UPPER = false;
	num_descriptors = 0;
	first_descriptor = NULL;
	last_descriptor = NULL;
	sysdata.NO_NAME_RESOLVING = true;

	if (port == 0) {
		log_string("port must be set via -p flag");
		exit(1);
	} else if (port <= 1024) {
		log_string("please select a port greater than 1024");
		exit(1);
	}

	/* init time. */
	gettimeofday(&now_time, NULL);
	current_time = (time_t) now_time.tv_sec;
	boot_time = time(0);
	utf8cpy(str_boot_time, ctime(&current_time));

	/* init boot time. */
	set_boot_time = &set_boot_time_struct;
	set_boot_time->manual = 0;

	new_boot_time = update_time(localtime(&current_time));
	/*
	 * Copies *new_boot_time to new_boot_struct, and then points
	 * new_boot_time to new_boot_struct again. -- Alty
	 */
	new_boot_struct = *new_boot_time;
	new_boot_time = &new_boot_struct;
	if (new_boot_time->tm_hour > 3)
		new_boot_time->tm_mday += 1;
	new_boot_time->tm_sec = 0;
	new_boot_time->tm_min = 0;
	new_boot_time->tm_hour = 4;

	/* update new_boot_time (due to day increment) */
	new_boot_time = update_time(new_boot_time);
	new_boot_struct = *new_boot_time;
	new_boot_time = &new_boot_struct;
	new_boot_time_t = mktime(new_boot_time);
	init_pfile_scan_time();

	if ((fpLOG = fopen(NULL_FILE, "r")) == NULL) {
		perror(NULL_FILE);
		exit(1);
	}
	/* run the game. */
	log_string("Booting Database");
	boot_db();
	log_string("Initializing socket");
	control = init_socket(port);
	if (gethostname(hostn, sizeof(hostn)) < 0) {
		perror("main: gethostname");
		utf8cpy(hostn, "unresolved");
	}
	sprintf(log_buf, "%s ready at address %s on port %d.",
	    sysdata.mud_name, hostn, port);

	log_string(log_buf);

	game_loop();
	closesocket(control);

	log_string("normal termination of game.");
	return (0);
}


int
init_socket(int port)
{
  char hostname[64];
  struct sockaddr_in sa;
  int x = 1;
  int fd;

  gethostname(hostname, sizeof(hostname));


  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("Init_socket: socket");
      exit(1);
    }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		 (void *) &x, sizeof(x)) < 0)
    {
      perror("Init_socket: SO_REUSEADDR");
      closesocket(fd);
      exit(1);
    }

#if defined(SO_DONTLINGER)
  struct linger ld;

  ld.l_onoff  = 1;
  ld.l_linger = 1000;

  if (setsockopt(fd, SOL_SOCKET, SO_DONTLINGER,
		 (void *) &ld, sizeof(ld)) < 0)
    {
      perror("Init_socket: SO_DONTLINGER");
      closesocket(fd);
      exit(1);
    }
#endif

  memset(&sa, '\0', sizeof(sa));
  sa.sin_family   = AF_INET;
  sa.sin_port	    = htons(port);

  if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) == -1)
    {
      perror("Init_socket: bind");
      closesocket(fd);
      exit(1);
    }

  if (listen(fd, 50) < 0)
    {
      perror("Init_socket: listen");
      closesocket(fd);
      exit(1);
    }

  return fd;
}

static void
sig_quit()
{
	CHAR_DATA *ch;
	char 	buf[MAX_STRING_LENGTH];

	for (ch = first_char; ch; ch = ch->next) {
		if (!IS_NPC(ch))
			save_char_obj(ch);
		if (ch->desc) {
			write_to_descriptor(ch->desc->descriptor, buf, 0);
			write_to_descriptor(ch->desc->descriptor, "BRB -- in a few seconds.\n\r", 0);
			write_to_descriptor(ch->desc->descriptor, "You have been saved to disk.\n\r", 0);
		}
	}
	log_string("caught sigquit, hanging up");
	exit(0);
}

static void
sig_hup()
{
	log_string("reloading configuration... restart process to finalize.");
}

static void
sig_int()
{
	CHAR_DATA *ch;
	char 	buf[MAX_STRING_LENGTH];

	for (ch = first_char; ch; ch = ch->next) {
		if (!IS_NPC(ch))
			save_char_obj(ch);
		if (ch->desc) {
			write_to_descriptor(ch->desc->descriptor, buf, 0);
			write_to_descriptor(ch->desc->descriptor, "BRB -- in a few seconds.\n\r", 0);
			write_to_descriptor(ch->desc->descriptor, "You have been saved to disk.\n\r", 0);
		}
	}
	log_string("caught sigint, hanging up");
	exit(0);
}

static void
seg_vio()
{
	log_string("caught segvio, dumping");
	exit(1);
}

static void
sig_term()
{
	CHAR_DATA *vch;
	char 	buf[MAX_STRING_LENGTH];

	sprintf(log_buf, "&RATTENTION!! Message from game server: shutdown called.\a");
	echo_to_all(AT_RED, log_buf, ECHOTAR_ALL);
	sprintf(log_buf, "Executing shutdown proceedure.");
	echo_to_all(AT_YELLOW, log_buf, ECHOTAR_ALL);
	log_string("Message from server: Executing shutdown proceedure.");
	shutdown_mud(log_buf);
	strcat(buf, "\n\r");

	if (auction->item)
		do_auction(supermob, "stop");

	log_string("Saving players....");
	for (vch = first_char; vch; vch = vch->next) {
		if (!IS_NPC(vch)) {
			save_char_obj(vch);
			sprintf(log_buf, "%s saved.", vch->name);
			log_string(log_buf);
			if (vch->desc) {
				write_to_descriptor(vch->desc->descriptor, buf, 0);
				write_to_descriptor(vch->desc->descriptor, "BRB -- in a few seconds.\n\r", 0);
				write_to_descriptor(vch->desc->descriptor, "You have been saved to disk.\n\r", 0);
			}
		}
	}

	fflush(stderr);			/* make sure strerr is flushed */
	close(control);
	log_string("shutdown complete.");
	exit(0);
}

static void
sig_kill()
{
	CHAR_DATA *vch;
	char 	buf[MAX_STRING_LENGTH];

	log_string("server: caught signal 9.");
	shutdown_mud(log_buf);
	strcat(buf, "\n\r");

	if (auction->item)
		do_auction(supermob, "stop");

	log_string("Saving players....");
	for (vch = first_char; vch; vch = vch->next) {
		if (!IS_NPC(vch)) {
			save_char_obj(vch);
			sprintf(log_buf, "%s saved.", vch->name);
			log_string(log_buf);
			if (vch->desc) {
				write_to_descriptor(vch->desc->descriptor, buf, 0);
				write_to_descriptor(vch->desc->descriptor, "BRB -- going down for a minute or two.\n\r", 0);
				write_to_descriptor(vch->desc->descriptor, "You have been saved to disk.\n\r", 0);
			}
		}
	}

	fflush(stderr);			/* make sure strerr is flushed */
	close(control);
	log_string("shutdown complete.");
	exit(0);
}


/*
 * LAG alarm!							-Thoric
 */
void
caught_alarm()
{
	char 	buf[MAX_STRING_LENGTH];

	sprintf(buf, "ALARM CLOCK! In section %s", alarm_section);
	bug(buf);
	utf8cpy(buf, "Alas, the hideous malevalent entity known only as 'Lag' rises once more!\n\r");
	echo_to_all(AT_IMMORT, buf, ECHOTAR_ALL);
	if (newdesc) {
		FD_CLR(newdesc, &in_set);
		FD_CLR(newdesc, &out_set);
		FD_CLR(newdesc, &exc_set);
		log_string("clearing newdesc");
	}
}

bool
check_bad_desc(int desc)
{
	if (FD_ISSET(desc, &exc_set)) {
		FD_CLR(desc, &in_set);
		FD_CLR(desc, &out_set);
		log_string("Bad FD caught and disposed.");
		return (true);
	}
	return (false);
}

void
accept_new(int ctrl)
{
  static struct timeval null_time;
  DESCRIPTOR_DATA *d;

  /*
   * Poll all active descriptors.
   */
  FD_ZERO(&in_set );
  FD_ZERO(&out_set);
  FD_ZERO(&exc_set);
  FD_SET(ctrl, &in_set);

  maxdesc = ctrl;
  newdesc = 0;
  for (d = first_descriptor; d; d = d->next)
    {
      maxdesc = UMAX(maxdesc, d->descriptor);
      FD_SET(d->descriptor, &in_set );
      FD_SET(d->descriptor, &out_set);
      FD_SET(d->descriptor, &exc_set);
      if(d->ifd != -1 && d->ipid != -1)
	{
	  maxdesc = UMAX(maxdesc, d->ifd);
	  FD_SET(d->ifd, &in_set);
	}
      if (d == last_descriptor)
	break;
    }

  if (select(maxdesc+1, &in_set, &out_set, &exc_set, &null_time) < 0)
    {
      perror("accept_new: select: poll");
      exit(1);
    }


  if (FD_ISSET(ctrl, &exc_set))
    {
      bug("Exception raise on controlling descriptor %d", ctrl);
      FD_CLR(ctrl, &in_set);
      FD_CLR(ctrl, &out_set);
    }
  else
    if (FD_ISSET(ctrl, &in_set))
      {
	newdesc = ctrl;
	new_descriptor(newdesc);
      }
}

void
game_loop()
{
  struct timeval last_time;
  char cmdline[MAX_INPUT_LENGTH];
  DESCRIPTOR_DATA *d;

  signal(SIGSEGV, seg_vio);
  signal(SIGKILL, sig_kill);
  signal(SIGTERM, sig_term);
  signal(SIGINT, sig_int);
  signal(SIGHUP, sig_hup);
  signal(SIGQUIT, sig_quit);
  signal(SIGPIPE, SIG_IGN);

  gettimeofday(&last_time, NULL);
  current_time = (time_t) last_time.tv_sec;

  /* Main loop */
  while (!mud_down)
    {
      accept_new(control );

      /*
       * Kick out descriptors with raised exceptions
       * or have been idle, then check for input.
       */
      for (d = first_descriptor; d; d = d_next)
	{
	  if (d == d->next)
	    {
	      bug("descriptor_loop: loop found & fixed");
	      d->next = NULL;
	    }
	  d_next = d->next;

	  d->idle++;	/* make it so a descriptor can idle out */
	  if (d->idle > 2)
	    d->prev_idle = d->idle;

	  if (FD_ISSET(d->descriptor, &exc_set))
	    {
	      FD_CLR(d->descriptor, &in_set );
	      FD_CLR(d->descriptor, &out_set);
	      if (d->character
		  && (d->connected == CON_PLAYING
		      ||   d->connected == CON_EDITING))
		save_char_obj(d->character);
	      d->outtop	= 0;
	      close_socket(d, true);
	      continue;
	    }
	  else
	    if ((!d->character && d->idle > 480)		  /* 2 mins */
		||   (d->connected != CON_PLAYING && d->idle > 1200) /* 5 mins */
		||     d->idle > 28800)				  /* 2 hrs  */
	      {
		write_to_descriptor(d->descriptor,
				    "Idle timeout... disconnecting.\n\r", 0);
		d->outtop	= 0;
		close_socket(d, true);
		continue;
	      }
	    else
	      {
		d->fcommand	= false;

		if (FD_ISSET(d->descriptor, &in_set))
		  {
		    if (d->character)
		      {
			d->character->timer = 0;
			if (d->character->pcdata && d->character->level < 51)
			  {
			    if (d->idle >= 180)
			      {
				if (d->character->pcdata->i_idle < 0)
				  d->character->pcdata->i_idle = 0;
				if (d->character->pcdata->i_idle > 4)
				  d->character->pcdata->i_idle = 0;

				d->character->pcdata->p_idle[d->character->pcdata->i_idle] = (d->idle / 4);
				d->character->pcdata->i_idle++;
			      }
			  }
		      }
		    if (d->prev_idle < 2)
		      d->psuppress_cmdspam = true;
		    else
		      d->psuppress_cmdspam = false;

		    d->psuppress_channel = 0;
		    d->prev_idle = d->idle;
		    d->idle = 0;

		    if (!read_from_descriptor(d))
		      {
			FD_CLR(d->descriptor, &out_set);
			if (d->character
			    && (d->connected == CON_PLAYING
				||   d->connected == CON_EDITING))
			  save_char_obj(d->character);
			d->outtop	= 0;
			close_socket(d, false);
			continue;
		      }
		  }

		/* check for input from the dns */
		if((d->connected == CON_PLAYING || d->character != NULL) && d->ifd != -1 && FD_ISSET(d->ifd, &in_set))
		  process_dns(d);

		if (d->character && d->character->wait > 0)
		  {
		    --d->character->wait;
		    continue;
		  }

		read_from_buffer(d);
		if (d->incomm[0] != '\0')
		  {
		    d->fcommand	= true;
		    stop_idling(d->character);

		    strcpy(cmdline, d->incomm);
		    d->incomm[0] = '\0';

		    if (d->character)
		      set_cur_char(d->character);

		      switch(d->connected)
			{
			default:
			  nanny(d, cmdline);
			  break;
			case CON_PLAYING:
			  interpret(d->character, cmdline);
			  break;
			case CON_EDITING:
			  edit_buffer(d->character, cmdline);
			  break;
			}
		  }
	      }
	  if (d == last_descriptor)
	    break;
	}
      /*
       * Autonomous game motion.
       */
      update_handler();

      /*
       * Check REQUESTS pipe
       */
      check_requests();

      /*
       * Output.
       */
      for (d = first_descriptor; d; d = d_next)
	{
	  d_next = d->next;

	  if ((d->fcommand || d->outtop > 0)
	      &&   FD_ISSET(d->descriptor, &out_set))
	    {
	      if (!flush_buffer(d, true))
		{
		  if (d->character
		      && (d->connected == CON_PLAYING
			  ||   d->connected == CON_EDITING))
		    save_char_obj(d->character);
		  d->outtop	= 0;
		  close_socket(d, false);
		}
	    }
	  if (d == last_descriptor)
	    break;
	}
      {
	struct timeval now_time;
	long secDelta;
	long usecDelta;

	gettimeofday(&now_time, NULL);
	usecDelta	= ((int) last_time.tv_usec) - ((int) now_time.tv_usec)
	  + 1000000 / PULSE_PER_SECOND;
	secDelta	= ((int) last_time.tv_sec) - ((int) now_time.tv_sec);
	while (usecDelta < 0)
	  {
	    usecDelta += 1000000;
	    secDelta  -= 1;
	  }

	while (usecDelta >= 1000000)
	  {
	    usecDelta -= 1000000;
	    secDelta  += 1;
	  }

	if (secDelta > 0 || (secDelta == 0 && usecDelta > 0))
	  {
	    struct timeval stall_time;

	    stall_time.tv_usec = usecDelta;
	    stall_time.tv_sec  = secDelta;
	    if (select(0, NULL, NULL, NULL, &stall_time) < 0 && errno != EINTR)
	      {
		perror("game_loop: select: stall");
		exit(1);
	      }
	  }
      }

      gettimeofday(&last_time, NULL);
      current_time = (time_t) last_time.tv_sec;
    }
  /* make sure strerr is flushed */
  fflush(stderr);	
}


void
new_descriptor(int new_desc)
{
	char 	buf[MAX_STRING_LENGTH];
	DESCRIPTOR_DATA *dnew;
	struct sockaddr_in sock;
	struct hostent *from;
	int 	desc;
	socklen_t size;
	char 	bugbuf[MAX_STRING_LENGTH];

	size = sizeof(sock);
	if (check_bad_desc(new_desc)) {
		set_alarm(0);
		return;
	}
	set_alarm(20);
	alarm_section = "new_descriptor::accept";
	if ((desc = accept(new_desc, (struct sockaddr *) & sock, &size)) < 0) {
		perror("New_descriptor: accept");
		sprintf(bugbuf, "[*****] BUG: New_descriptor: accept");
		log_string_plus(bugbuf, LOG_COMM, sysdata.log_level);
		set_alarm(0);
		return;
	}
	if (check_bad_desc(new_desc)) {
		set_alarm(0);
		return;
	}
	set_alarm(20);
	alarm_section = "new_descriptor: after accept";
	if (fcntl(desc, F_SETFL, O_NONBLOCK) == -1) {
		perror("new_descriptor: fcntl: O_NONBLOCK");
		set_alarm(0);
		return;
	}
	if (check_bad_desc(new_desc))
		return;

	CREATE(dnew, DESCRIPTOR_DATA, 1);
	dnew->next = NULL;
	dnew->descriptor = desc;
	dnew->connected = CON_GET_NAME;
	dnew->ansi = true;
	dnew->outsize = 2000;
	dnew->idle = 0;
	dnew->lines = 0;
	dnew->scrlen = 24;
	dnew->port = ntohs(sock.sin_port);
	dnew->user = 0;
	dnew->newstate = 0;
	dnew->prevcolor = 0x07;
	/* descriptor pipes */
	dnew->ifd = -1;
	dnew->ipid = -1;

	CREATE(dnew->outbuf, char, dnew->outsize);
	utf8cpy(log_buf, inet_ntoa(sock.sin_addr));

	dnew->host = STRALLOC(log_buf);
	from = gethostbyaddr((char *) &sock.sin_addr,
	    sizeof(sock.sin_addr), AF_INET);
	dnew->host2 = STRALLOC((char *) (from ? from->h_name : buf));
	/*
	 * Init descriptor data.
	 */
	if (!last_descriptor && first_descriptor) {
		DESCRIPTOR_DATA *d;

		bug("New_descriptor: last_desc is NULL, but first_desc is not! ...fixing");
		for (d = first_descriptor; d; d = d->next)
			if (!d->next)
				last_descriptor = d;
	}
	LINK(dnew, first_descriptor, last_descriptor, next, prev);
	/*
	 * Send the greeting.
	 */
	{
		extern char *help_greeting;

		if (help_greeting[0] == '.')
			send_to_desc_color(help_greeting + 1, dnew);
		else
			send_to_desc_color(help_greeting, dnew);
	}
	send_to_desc_color("&wEnter your character's name, or type &Wnew&w: &D", dnew);

	if (++num_descriptors > sysdata.maxplayers)
		sysdata.maxplayers = num_descriptors;
	if (sysdata.maxplayers > sysdata.alltimemax) {
		if (sysdata.time_of_max)
			DISPOSE(sysdata.time_of_max);
		sprintf(buf, "%24.24s", ctime(&current_time));
		sysdata.time_of_max = str_dup(buf);
		sysdata.alltimemax = sysdata.maxplayers;
		sprintf(log_buf, "Broke all-time maximum player record: %d", sysdata.alltimemax);
		log_string_plus(log_buf, LOG_COMM, sysdata.log_level);
		to_channel(log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL);
		save_sysdata(sysdata);
	}
	set_alarm(0);
	return;
}

void
free_desc(DESCRIPTOR_DATA * d)
{
	closesocket(d->descriptor);
	DISPOSE(d->outbuf);
	/* identd */
	STRFREE(d->user);
	if (d->pagebuf)
		DISPOSE(d->pagebuf);
	DISPOSE(d);
	return;
}

void
close_socket(DESCRIPTOR_DATA *dclose, bool force)
{
  CHAR_DATA *ch;
  DESCRIPTOR_DATA *d;
  bool do_not_unlink = false;

  if(dclose->ipid != -1)
    {
      int status;

      kill(dclose->ipid, SIGKILL);
      waitpid(dclose->ipid, &status, 0);
    }

  if(dclose->ifd != -1)
    close(dclose->ifd);

  /* flush outbuf */
  if (!force && dclose->outtop > 0)
    flush_buffer(dclose, false);

  /* say bye to whoever's snooping this descriptor */
  if (dclose->snoop_by)
    write_to_buffer(dclose->snoop_by,
		     "Your victim has left the game.\n\r", 0);

  /* stop snooping everyone else */
  for (d = first_descriptor; d; d = d->next)
    if (d->snoop_by == dclose)
      d->snoop_by = NULL;

  /* Check for switched people who go link-dead. -- Altrag */
  if (dclose->original)
    {
      if ((ch = dclose->character) != NULL)
	do_return(ch, "");
      else
	{
	  bug("Close_socket: dclose->original without character %s",
	       (dclose->original->name ? dclose->original->name : "unknown"));
	  dclose->character = dclose->original;
	  dclose->original = NULL;
	}
    }

  ch = dclose->character;

  /* sanity check :(*/
  if (!dclose->prev && dclose != first_descriptor)
    {
      DESCRIPTOR_DATA *dp, *dn;
      bug("Close_socket: %s desc:%p != first_desc:%p and desc->prev = NULL!",
	   ch ? ch->name : d->host, dclose, first_descriptor);
      dp = NULL;
      for (d = first_descriptor; d; d = dn)
	{
	  dn = d->next;
	  if (d == dclose)
	    {
	      bug("Close_socket: %s desc:%p found, prev should be:%p, fixing.",
		   ch ? ch->name : d->host, dclose, dp);
	      dclose->prev = dp;
	      break;
	    }
	  dp = d;
	}
      if (!dclose->prev)
	{
	  bug("Close_socket: %s desc:%p could not be found!.",
	       ch ? ch->name : dclose->host, dclose);
	  do_not_unlink = true;
	}
    }
  if (!dclose->next && dclose != last_descriptor)
    {
      DESCRIPTOR_DATA *dp, *dn;
      bug("Close_socket: %s desc:%p != last_desc:%p and desc->next = NULL!",
	   ch ? ch->name : d->host, dclose, last_descriptor);
      dn = NULL;
      for (d = last_descriptor; d; d = dp)
	{
	  dp = d->prev;
	  if (d == dclose)
	    {
	      bug("Close_socket: %s desc:%p found, next should be:%p, fixing.",
		   ch ? ch->name : d->host, dclose, dn);
	      dclose->next = dn;
	      break;
	    }
	  dn = d;
	}
      if (!dclose->next)
	{
	  bug("Close_socket: %s desc:%p could not be found!.",
	       ch ? ch->name : dclose->host, dclose);
	  do_not_unlink = true;
	}
    }

  if (dclose->character)
    {
      sprintf(log_buf, "Closing link to %s.", ch->pcdata->filename);
      log_string_plus(log_buf, LOG_COMM, UMAX(sysdata.log_level, ch->level));

      if ((dclose->connected == CON_PLAYING
	    || dclose->connected == CON_EDITING)
	   ||(dclose->connected >= CON_NOTE_TO
	      && dclose->connected <= CON_NOTE_FINISH))
	{
	  char ldbuf[MAX_STRING_LENGTH];
	  act(AT_ACTION, "$n has lost $s link.", ch, NULL, NULL, TO_CANSEE);

	  sprintf(ldbuf, "%s has gone linkdead", ch->name);
	  if(!IS_IMMORTAL(ch))
	    do_info(ch, ldbuf);
	  else
	    do_ainfo(ch, ldbuf);

	  ch->desc = NULL;
	}
      else
	{
	  /* clear descriptor pointer to get rid of bug message in log */
	  dclose->character->desc = NULL;
	  free_char(dclose->character);
	}
    }

  if (!do_not_unlink)
    {
      /* make sure loop doesn't get messed up */
      if (d_next == dclose)
	d_next = d_next->next;
      UNLINK(dclose, first_descriptor, last_descriptor, next, prev);
    }

  if (dclose->descriptor == maxdesc)
    --maxdesc;

  free_desc(dclose);
  --num_descriptors;
}


bool
read_from_descriptor(DESCRIPTOR_DATA * d)
{
	unsigned int i_start;
	int 	i_err;

	/* hold horses if pending command already. */
	if (d->incomm[0] != '\0')
		return (true);

	/* check for overflow. */
	i_start = utf8len(d->inbuf);
	if (i_start >= sizeof(d->inbuf) - 10) {
		sprintf(log_buf, "%s input overflow!", d->host);
		log_string(log_buf);
		write_to_descriptor(d->descriptor,
		    "\n\rYou cannot enter the same command more than 20 consecutive times!\n\r", 0);
		return (false);
	}
	for (;;) {
		int 	n_read;

		n_read = recv(d->descriptor, d->inbuf + i_start,
		    sizeof(d->inbuf) - 10 - i_start, 0);
		i_err = errno;
		if (n_read > 0) {
			i_start += n_read;
			if (d->inbuf[i_start - 1] == '\n' || d->inbuf[i_start - 1] == '\r')
				break;
		} else if (n_read == 0) {
			log_string_plus("EOF encountered on read.", LOG_COMM, sysdata.log_level);
			return (false);
		} else if (i_err == EWOULDBLOCK) {
			break;
		} else {
			perror("read_from_descriptor");
			return (false);
		}
	}

	d->inbuf[i_start] = '\0';
	return (true);
}



/* transfer one line from input buffer to input line. */
void
read_from_buffer(DESCRIPTOR_DATA * d)
{
	int 	i , j, k;

	/* hold horses if pending command already. */
	if (d->incomm[0] != '\0')
		return;

	/* look for at least one new line. */
	for (i = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r' && i < MAX_INBUF_SIZE;
	     i++) {
		if (d->inbuf[i] == '\0')
			return;
	}

	/* canonical input processing. */
	for (i = 0, k = 0; d->inbuf[i] != '\n' && d->inbuf[i] != '\r'; i++) {
		int 	z = 0;

		if (d->connected == CON_EDITING)
			z = 254;
		else
			z = 762;
		if (k >= z) {
			write_to_descriptor(d->descriptor, "Line too long.\n\r", 0);
			d->inbuf[i] = '\n';
			d->inbuf[i + 1] = '\0';
			break;
		}
		if (d->inbuf[i] == '\b' && k > 0)
			--k;
		else if (isascii(d->inbuf[i]) && isprint(d->inbuf[i]))
			d->incomm[k++] = d->inbuf[i];
	}

	/* finish off the line. */
	if (k == 0)
		d->incomm[k++] = ' ';
	d->incomm[k] = '\0';

	/* deal with bozos with #repeat 1000 */
	if (k > 1 || d->incomm[0] == '!') {
		if (d->incomm[0] != '!' && utf8cmp(d->incomm, d->inlast)) {
			d->repeat = 0;
		} else {
			if (++d->repeat >= 20) {
				write_to_descriptor(d->descriptor,
				    "\n\rYou cannot enter the same command more than 20 consecutive times!\n\r", 0);
				utf8cpy(d->incomm, "quit");
			}
		}
	}
	/* do '!' substitution. */
	if (d->incomm[0] == '!')
		utf8cpy(d->incomm, d->inlast);
	else
		utf8cpy(d->inlast, d->incomm);

	/* shift the input buffer. */
	while (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
		i++;
	for (j = 0; (d->inbuf[j] = d->inbuf[i + j]) != '\0'; j++);
}

bool
flush_buffer(DESCRIPTOR_DATA * d, bool f_prompt)
{
	char 	buf[MAX_INPUT_LENGTH];
	char 	promptbuf[MAX_STRING_LENGTH];
	extern bool mud_down;

	/*
	 * if buffer has more than 4K inside, spit out .5K at a time
	 * -Thoric
	 */
	if (!mud_down && d->outtop > 4096) {
		memcpy(buf, d->outbuf, 512);
		d->outtop -= 512;
		memmove(d->outbuf, d->outbuf + 512, d->outtop);
		if (d->snoop_by) {
			char 	snoopbuf[MAX_INPUT_LENGTH];

			buf[512] = '\0';
			if (d->character && d->character->name) {
				if (d->original && d->original->name)
					sprintf(snoopbuf, "%s (%s)", d->character->name, d->original->name);
				else
					sprintf(snoopbuf, "%s", d->character->name);
				write_to_buffer(d->snoop_by, snoopbuf, 0);
			}
			write_to_buffer(d->snoop_by, "% ", 2);
			write_to_buffer(d->snoop_by, buf, 0);
		}
		if (!write_to_descriptor(d->descriptor, buf, 512)) {
			d->outtop = 0;
			return (false);
		}
		return (true);
	}
	/* bust a prompt. */
	if (f_prompt && !mud_down && d->connected == CON_PLAYING) {
		CHAR_DATA *ch;

		ch = d->original ? d->original : d->character;

		if (xIS_SET(ch->act, PLR_BLANK))
			write_to_buffer(d, "\n\r", 2);

		if (ch->fighting) {
			d->psuppress_cmdspam = false;
			d->psuppress_channel = 0;
		}
		if (d->psuppress_channel >= 5)
			d->psuppress_channel = 0;

		if (xIS_SET(ch->act, PLR_PROMPT)) {
			sysdata.outBytesFlag = LOGBOUTPROMPT;
			if (!d->psuppress_cmdspam && !d->psuppress_channel) {
				display_prompt(d);
				d->psuppress_cmdspam = false;
			} else if (d->psuppress_channel && ch->pcdata) {
				switch (ch->pcdata->normalPromptConfig) {
				default:
					if (is_android(ch))
						sprintf(promptbuf, "&D<D:%d E:%s T:%s> ", ch->hit, num_punct(ch->mana), abbNumLD(ch->pl));
					else
						sprintf(promptbuf, "&D<L:%d K:%s P:%s> ", ch->hit, num_punct(ch->mana), abbNumLD(ch->pl));
					break;
				case 1:
					if (is_android(ch))
						sprintf(promptbuf, "&D<D:%d E:%s T:%s> ", ch->hit, num_punct(ch->mana), abbNumLD(ch->pl));
					else
						sprintf(promptbuf, "&D<L:%d K:%s P:%s> ", ch->hit, num_punct(ch->mana), abbNumLD(ch->pl));
					break;
				}
				send_to_char(promptbuf, ch);
				if (get_true_rank(ch) >= 11)
					pager_printf(ch, "(KRT:%d) ", sysdata.kaiRestoreTimer);
			} else
				send_to_char("&D> ", ch);
			sysdata.outBytesFlag = LOGBOUTNORM;
		}
		if (!IS_NPC(ch))
			if (xIS_SET(ch->act, PLR_TELNET_GA))
				write_to_buffer(d, go_ahead_str, 0);
	}
	/* short-circuit if nothing to write. */
	if (d->outtop == 0)
		return (true);

	/* snoop-o-rama. */
	if (d->snoop_by) {
		/*
		 * without check, 'force mortal quit' while snooped caused
		 * crash, -h
		 */
		if (d->character && d->character->name) {
			/* show original snooped names. -- Altrag */
			if (d->original && d->original->name)
				sprintf(buf, "%s (%s)", d->character->name, d->original->name);
			else
				sprintf(buf, "%s", d->character->name);
			write_to_buffer(d->snoop_by, buf, 0);
		}
		write_to_buffer(d->snoop_by, "% ", 2);
		write_to_buffer(d->snoop_by, d->outbuf, d->outtop);
	}
	/* output */
	if (!write_to_descriptor(d->descriptor, d->outbuf, d->outtop)) {
		d->outtop = 0;
		return (false);
	} else {
		d->outtop = 0;
		return (true);
	}
}



/* append onto an output buffer. */
void
write_to_buffer(DESCRIPTOR_DATA * d, const char *txt, int length)
{
	if (!d) {
		bug("write_to_buffer: NULL descriptor");
		return;
	}
	/* normally a bug... but can happen if loadup is used. */
	if (!d->outbuf)
		return;

	/* find length in case caller didn't. */
	if (length <= 0)
		length = utf8len(txt);

	/* initial \n\r if needed. */
	if (d->outtop == 0 && !d->fcommand) {
		d->outbuf[0] = '\n';
		d->outbuf[1] = '\r';
		d->outtop = 2;
	}
	/* expand the buffer as needed. */
	while (d->outtop + length >= (int) d->outsize) {
		if (d->outsize > 32000) {
			/* empty buffer */
			d->outtop = 0;
			bug("Buffer overflow. Closing (%s).", d->character ? d->character->name : "???");
			log_string("preparing to close socket at comm.c:1248\n");
			close_socket(d, true);
			return;
		}
		d->outsize *= 2;
		RECREATE(d->outbuf, char, d->outsize);
	}

	/* copy */
	utf8ncpy(d->outbuf + d->outtop, txt, length);
	d->outtop += length;
	d->outbuf[d->outtop] = '\0';
	return;
}

bool
write_to_descriptor(int desc, char *txt, int length)
{
	int 	i_start;
	int 	n_write;
	int 	n_block;
	int 	i_err;

	if (length <= 0)
		length = utf8len(txt);

	for (i_start = 0; i_start < length; i_start += n_write) {
		n_block = UMIN(length - i_start, 4096);
		n_write = send(desc, txt + i_start, n_block, 0);
		i_err = errno;
		/* broken pipe (linkdead) */
		if (i_err == EPIPE) {
			return (false);

		} else if (n_write < 0) {
			perror("write_to_descriptor");
			return (false);
		}
	}

	return (true);
}

void
show_title(DESCRIPTOR_DATA * d)
{
	CHAR_DATA *ch;

	ch = d->character;

	if (!IS_SET(ch->pcdata->flags, PCFLAG_NOINTRO)) {
		if (xIS_SET(ch->act, PLR_RIP))
			send_rip_title(ch);
		else if (xIS_SET(ch->act, PLR_ANSI))
			send_ansi_title(ch);
		else
			send_ascii_title(ch);
	} else {
		write_to_buffer(d, "Press enter...\n\r", 0);
	}
	d->connected = CON_PRESS_ENTER;
}

void
nanny(DESCRIPTOR_DATA *d, char *argument)
{
  char buf[MAX_STRING_LENGTH];
  char buf2[MAX_STRING_LENGTH];
  char buf3[MAX_STRING_LENGTH];
  char buf4[MAX_STRING_LENGTH];
  char arg[MAX_STRING_LENGTH];
  CHAR_DATA *ch;
  char *pwd_new;
  char *p;
  int b = 0;
  int i_class;
  bool f_old, chk;
  NOTE_DATA *catchup_notes;
  int i = 0;

  if(d->connected != CON_NOTE_TEXT)
    {
      while (isspace(*argument))
	argument++;
    }

  ch = d->character;

  switch (d->connected)
    {

    default:
      bug("nanny: bad d->connected %d.", d->connected);
      close_socket(d, true);
      return;

    case CON_GET_NAME:
      if (argument[0] == '\0')
	{
	  close_socket(d, false);
	  return;
	}

      *argument = capitalize_string(argument);

      /* Old players can keep their characters. -- Alty */
      if (!check_parse_name(argument, (d->newstate != 0)))
	{
	  send_to_desc_color("&wIllegal name, try another.\n\rName: &D", d);
	  return;
	}

      if (!str_cmp(argument, "New"))
	{
	  if (d->newstate == 0)
	    {
	      /* New player */
	      /* Don't allow new players if DENY_NEW_PLAYERS is true */
	      if (sysdata.DENY_NEW_PLAYERS == true)
		{
		  sprintf(buf, "The mud is currently preparing for a reboot.\n\r");
		  send_to_desc_color(buf, d);
		  sprintf(buf, "New players are not accepted during this time.\n\r");
		  send_to_desc_color(buf, d);
		  sprintf(buf, "Please try again in a few minutes.\n\r");
		  send_to_desc_color(buf, d);
		  close_socket(d, false);
		}
	      sprintf(buf, "\n\r&gChoosing a name is one of the most important parts of this game...\n\r"
		      "Make sure to pick a name appropriate to the character you are going\n\r"
		      "to role play, and be sure that it fits into the DragonBall Z world.\n\r"
		      "Please type '&WHELP&g' to read what restirictions we have for naming your\n\r"
		      "character.\n\r\n\r&wPlease choose a name for your character: &D");
	      send_to_desc_color(buf, d);
	      d->newstate++;
	      d->connected = CON_GET_NAME;
	      return;
	    }
	  else
	    {
	      send_to_desc_color("&wIllegal name, try another.\n\rName: &D", d);
	      return;
	    }
	}

      if (!str_cmp(argument, "help"))
	{
	  HELP_DATA *pHelp;

	  for (pHelp = first_help; pHelp; pHelp = pHelp->next)
	    {
	      if (!str_cmp(pHelp->keyword, "dbznames"))
		break;
	    }
	  if (!pHelp)
	    {
	      send_to_desc_color("No help on that word.\n\rName: ", d);
	      return;
	    }
	  send_to_desc_color("\n\r", d);
	  send_to_desc_color(pHelp->text, d);
	  send_to_desc_color("\n\r\n\r&wName: ", d);
	  return;
	}

      if (check_playing(d, argument, false) == BERR)
	{
	  write_to_buffer(d, "Name: ", 0);
	  return;
	}

      f_old = load_char_obj(d, argument, true);
      if (!d->character)
	{
	  sprintf(log_buf, "Bad player file %s@%s.", argument, d->host);
	  log_string(log_buf);
	  send_to_desc_color("Your playerfile is corrupt...Please notify GokuDBS@hotmail.com.\n\r", d);
	  close_socket(d, false);
	  return;
	}
      ch   = d->character;
      if (xIS_SET(ch->act, PLR_DENY))
	{
	  sprintf(log_buf, "Denying access to %s@%s.", argument, d->host);
	  log_string_plus(log_buf, LOG_COMM, sysdata.log_level);
	  if (d->newstate != 0)
	    {
	      send_to_desc_color("That name is already taken.  Please choose another: ", d);
	      d->connected = CON_GET_NAME;
	      d->character->desc = NULL;
	      free_char(d->character); /* Big Memory Leak before --Shaddai */
	      d->character = NULL;
	      return;
	    }
	  send_to_desc_color("You are denied access.\n\r", d);
	  close_socket(d, false);
	  return;
	}
      if (check_total_ip(d))
	{
	  send_to_desc_color (
			      "Your maximum amount of players you can have online has been reached.\n\r", d);
	  close_socket(d, false);
	  return;
	}

      chk = check_reconnect(d, argument, false);
      if (chk == BERR)
	return;

      if (chk)
	{
	  f_old = true;
	}
      else
	{
	  if (wizlock && !IS_IMMORTAL(ch))
	    {
	      send_to_desc_color("The game is wizlocked.  Only immortals can connect now.\n\r", d);
	      send_to_desc_color("Please try back later.\n\r", d);
	      close_socket(d, false);
	      return;
	    }
	}

      if (f_old)
	{
	  if (d->newstate != 0)
	    {
	      send_to_desc_color("&wThat name is already taken.  Please choose another: &D", d);
	      d->connected = CON_GET_NAME;
	      d->character->desc = NULL;
	      free_char(d->character); /* Big Memory Leak before --Shaddai */
	      d->character = NULL;
	      return;
	    }
	  /* Old player */
	  send_to_desc_color("&wPassword: &D", d);
	  write_to_buffer(d, echo_off_str, 0);
	  d->connected = CON_GET_OLD_PASSWORD;
	  return;
	}
      else
	{
	  if (d->newstate == 0)
	    {
	      /* No such player */
	      send_to_desc_color("\n\r&wNo such player exists.\n\rPlease check your spelling, or type new to start a new player.\n\r\n\rName: &D", d);
	      d->connected = CON_GET_NAME;
	      d->character->desc = NULL;
	      free_char(d->character); /* Big Memory Leak before --Shaddai */
	      d->character = NULL;
	      return;
	    }

	  sprintf(buf, "&wDid I get that right, %s (&WY&w/&WN&w)? &D", argument);
	  send_to_desc_color(buf, d);
	  d->connected = CON_CONFIRM_NEW_NAME;
	  return;
	}
      break;

    case CON_GET_OLD_PASSWORD:
      send_to_desc_color("\n\r", d);

      if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
	{
	  send_to_desc_color("&wWrong password.&D\n\r", d);
	  sprintf(log_buf, "WRONG PASSWORD: %s@%s.", ch->name, d->host);
	  log_string(log_buf);
	  /* clear descriptor pointer to get rid of bug message in log */
	  d->character->desc = NULL;
	  close_socket(d, false);
	  return;
	}

      write_to_buffer(d, echo_on_str, 0);

      if (check_playing(d, ch->pcdata->filename, true))
	return;

      chk = check_reconnect(d, ch->pcdata->filename, true);
      if (chk == BERR)
	{
	  if (d->character && d->character->desc)
	    d->character->desc = NULL;
	  close_socket(d, false);
	  return;
	}
      if (chk == true)
	return;

      sprintf(buf, ch->pcdata->filename);
      d->character->desc = NULL;
      free_char(d->character);
      d->character = NULL;
      f_old = load_char_obj(d, buf, false);
      ch = d->character;
      if (ch->position ==  POS_FIGHTING
	  || ch->position ==  POS_EVASIVE
	  || ch->position ==  POS_DEFENSIVE
	  || ch->position ==  POS_AGGRESSIVE
	  || ch->position ==  POS_BERSERK)
	ch->position = POS_STANDING;

      sprintf(log_buf, "%s@%s(%s) has connected.", ch->pcdata->filename,
	      d->host, d->user);
      if (ch->level == 2)
	{
	  xSET_BIT(ch->deaf, CHANNEL_FOS);
	  ch->level = 1;
	}
      sprintf(buf3, "%s has logged on", ch->name);
      if (!IS_IMMORTAL(ch))
	do_info(ch, buf3);
      else
	do_ainfo(ch, buf3);

      if (!IS_IMMORTAL(ch) && IS_AFFECTED(ch, AFF_DEAD))
	{
	  sprintf(buf4, "%s has a halo", ch->name);
	  log_string_plus(buf4, LOG_HIGH, LEVEL_LESSER);
	}
      /* player data update checks */

      pager_printf(ch, "Checking for player data updates...\n\r");
      if (upgrade_player(ch))
	pager_printf(ch, "Updated player data successfully.\n\r");
      else
	pager_printf(ch, "No updates to make.\n\r");

      adjust_hiscore("pkill", ch, ch->pcdata->pkills); /* cronel hiscore */
      adjust_hiscore("sparwins", ch, ch->pcdata->spar_wins);
      adjust_hiscore("sparloss", ch, ch->pcdata->spar_loss);
      adjust_hiscore("mkills", ch, ch->pcdata->mkills);
      adjust_hiscore("deaths", ch, (ch->pcdata->pdeaths + ch->pcdata->mdeaths));
      update_plHiscore(ch);
      adjust_hiscore("played", ch, ((get_age(ch) - 4) * 2));
      adjust_hiscore("zeni", ch, ch->gold);
      adjust_hiscore("bounty", ch, ch->pcdata->bkills);
      update_member(ch);

      if (ch->level < LEVEL_DEMI)
	{
	  log_string_plus(log_buf, LOG_COMM, sysdata.log_level);
	}
      else
	log_string_plus(log_buf, LOG_COMM, ch->level);
      show_title(d);
      break;

    case CON_CONFIRM_NEW_NAME:
      switch (*argument)
	{
	case 'y': case 'Y':
	  sprintf(buf, "\n\r&wMake sure to use a password that won't be easily guessed by someone else."
		  "\n\rPick a good password for %s: %s&D",
		  ch->name, echo_off_str);
	  send_to_desc_color(buf, d);
	  xSET_BIT(ch->act, PLR_ANSI);
	  d->connected = CON_GET_NEW_PASSWORD;
	  break;

	case 'n': case 'N':
	  send_to_desc_color("&wOk, what IS it, then? &D", d);
	  /* clear descriptor pointer to get rid of bug message in log */
	  d->character->desc = NULL;
	  free_char(d->character);
	  d->character = NULL;
	  d->connected = CON_GET_NAME;
	  break;

	default:
	  send_to_desc_color("&wPlease type &WY&wes or &WN&wo. &D", d);
	  break;
	}
      break;

    case CON_GET_NEW_PASSWORD:
      send_to_desc_color("\n\r", d);

      if (strlen(argument) < 5)
	{
	  send_to_desc_color("&wPassword must be at least five characters long.\n\rPassword: &D", d);
	  return;
	}

      pwd_new = sha256_crypt(argument);
      for (p = pwd_new; *p != '\0'; p++)
	{
	  if (*p == '~')
	    {
	      send_to_desc_color(
				 "&wNew password not acceptable, try again.\n\rPassword: &D",
				 d);
	      return;
	    }
	}

      DISPOSE(ch->pcdata->pwd);
      ch->pcdata->pwd = str_dup(pwd_new);
      send_to_desc_color("\n\r&wPlease retype the password to confirm: &D", d);
      d->connected = CON_CONFIRM_NEW_PASSWORD;
      break;

    case CON_CONFIRM_NEW_PASSWORD:
      send_to_desc_color("\n\r", d);

      if (strcmp(sha256_crypt(argument), ch->pcdata->pwd))
	{
	  send_to_desc_color("&wPasswords don't match.\n\rRetype password: &D",
			     d);
	  d->connected = CON_GET_NEW_PASSWORD;
	  return;
	}

      write_to_buffer(d, echo_on_str, 0);
      send_to_desc_color("\n\r&wWhat do you want your last name to be? [press enter for none] &D\n\r", d);
      d->connected = CON_GET_LAST_NAME;
      break;

    case CON_GET_LAST_NAME:
      if (argument[0] == '\0')
	{
	  write_to_buffer(d, echo_on_str, 0);
	  send_to_desc_color("\n\rDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w)\n\rType &WHELP&w for more information.", d);
	  d->connected = CON_GET_HC;
	  return;
	}

      *argument = capitalize_string(argument);
      /* Old players can keep their characters. -- Alty */
      if (!check_parse_name(argument, true))
	{
	  send_to_desc_color("&wIllegal name, try another.\n\rLast name: &D", d);
	  return;
	}

      sprintf(buf, "&wDid I get that right, %s (&WY&w/&WN&w)? &D", argument);
      send_to_desc_color(buf, d);
      DISPOSE(ch->pcdata->last_name);
      ch->pcdata->last_name = str_dup("");
      buf[0] = ' ';
      strcpy(buf+1, argument);
      ch->pcdata->last_name = strdup(buf);
      d->connected = CON_CONFIRM_LAST_NAME;
      return;
      break;

    case CON_CONFIRM_LAST_NAME:
      switch (*argument)
	{
	case 'y': case 'Y':
	  write_to_buffer(d, echo_on_str, 0);
	  send_to_desc_color("\n\rDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w)\n\rType &WHELP&w for more information.", d);
	  d->connected = CON_GET_HC;
	  break;

	case 'n': case 'N':
	  send_to_desc_color("&wOk, what IS it, then? &D", d);
	  d->connected = CON_GET_LAST_NAME;
	  break;

	default:
	  send_to_desc_color("&wPlease type &WY&wes or &WN&wo. &D", d);
	  break;
	}
      break;


    case CON_GET_HC:
      if (!str_cmp(argument, "help"))
	{
	  HELP_DATA *pHelp;

	  for (pHelp = first_help; pHelp; pHelp = pHelp->next)
	    {
	      if (!str_cmp(pHelp->keyword, "HC HARDCORE UNKNOWN"))
		break;
	    }
	  if (!pHelp)
	    {
	      send_to_desc_color("No help on that word.\n\rDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w): ", d);
	      return;
	    }
	  send_to_desc_color("\n\r", d);
	  send_to_desc_color(pHelp->text, d);
	  send_to_desc_color("&wDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w): ", d);
	  return;
	}
      switch (*argument)
	{
	case 'y': case 'Y':
	  sprintf(buf, "\n\r&wOkay, you are now &RHARDCORE&w!&D");
	  send_to_desc_color(buf, d);
	  xSET_BIT(ch->act, PLR_HC);
	  write_to_buffer(d, echo_on_str, 0);
	  break;

	case 'n': case 'N':
	  send_to_desc_color("\n\r&wOkay, you are a normal character.&D", d);
	  write_to_buffer(d, echo_on_str, 0);
	  break;

	default:
	  send_to_desc_color("&wPlease type Yes or No. &D", d);
	  return;
	}
      send_to_desc_color("\n\r&wWhat is your sex (&CM&w/&PF&w/&WN&w)? &D", d);
      d->connected = CON_GET_NEW_SEX;
      break;

    case CON_GET_NEW_SEX:
      switch (argument[0])
	{
	case 'm': case 'M': ch->sex = SEX_MALE;    break;
	case 'f': case 'F': ch->sex = SEX_FEMALE;  break;
	case 'n': case 'N': ch->sex = SEX_NEUTRAL; break;
	default:
	  send_to_desc_color("&wThat's not a sex.\n\rWhat IS your sex? &D", d);
	  return;
	}

      send_to_desc_color("\n\r&wThe following Races are Available to You:&D\n\r", d);
      send_to_desc_color("&c==============================================================================&D",d);
      buf[0] = '\0';

      /*
       * Take this out SHADDAI
       */
      i=0;
      for (i_class = 0; i_class < 31; i_class++)
	{
	  if(i_class == 4) {
	    continue;
	  }
	  if(i_class > 8 && i_class < 28)
	    continue;
	  char letters[14] = "abcdefghijklmn";
	  if (class_table[i_class]->who_name &&
	      class_table[i_class]->who_name[0] != '\0')
	    {
	      sprintf(buf, "\n\r&w   (&W%d&w)  &c%-12s&w  ('&R%c&w' for help)&D", i,
		      class_table[i_class]->who_name, letters[i]);
	      send_to_desc_color(buf, d);
	      i++;
	    }
	}
      send_to_desc_color("&c==============================================================================&D",d);
      sprintf(buf, "\n\r&wChoose the number of your race: &D");
      send_to_desc_color(buf, d);
      d->connected = CON_GET_NEW_CLASS;
      break;

    case CON_GET_NEW_CLASS:
      argument = one_argument(argument, arg);
      if (is_number(arg))
	{
	  i = atoi(arg);

	  // map the argument to the actual race IDs
	  int 	c = 0;
	  if (i == 0)
	    c = 0; // saiyan
	  if (i == 1)
	    c = 1; // human
	  if (i == 2)
	    c = 2; // halfbreed
	  if (i == 3)
	    c = 3; // namek
	  if (i == 4)
	    c = 5; // icer
	  if (i == 5)
	    c = 6; // bio-android
	  if (i == 6)
	    c = 7; // kaio
	  if (i == 7)
	    c = 8; // demon
	  if (i == 8)
	    c = 28; // android-h
	  if (i == 9)
	    c = 29; // android-e
	  if (i == 10)
	    c = 30; // android-fm
	  for (i_class = 0; i_class < 31; i_class++)
	    {
	      if(i_class > 8 && i_class < 28)
		{
		  continue;
		}
	      if (class_table[i_class]->who_name &&
		  class_table[i_class]->who_name[0] != '\0')
		{
		  if (c == i_class)
		    {
		      ch->class =  i_class;
		      ch->race  =  i_class;
		      break;
		    }
		}
	    }
	}
      else
	{
	  char letters[14] = "abcdefghijklmn";
	  for (i = 0; i < 14; i++)
	    {
	      if (arg[0] == letters[i])
		{
		  // map the argument to the actual race IDs
		  int 	c = i;
		  if (i == 0)
		    c = 0; // saiyan
		  if (i == 1)
		    c = 1; // human
		  if (i == 2)
		    c = 2; // halfbreed
		  if (i == 3)
		    c = 3; // namek
		  if (i == 4)
		    c = 5; // icer
		  if (i == 5)
		    c = 6; // bio-android
		  if (i == 6)
		    c = 7; // kaio
		  if (i == 7)
		    c = 8; // demon
		  if (i == 8)
		    c = 28; // android-h
		  if (i == 9)
		    c = 29; // android-e
		  if (i == 10)
		    c = 30; // android-fm
		  if (!str_cmp(class_table[c]->who_name, "android-h"))
		    {
		      sprintf(buf, "androidh");
		    }
		  else if (!str_cmp(class_table[c]->who_name, "android-e"))
		    {
		      sprintf(buf, "androide");
		    }
		  else if (!str_cmp(class_table[c]->who_name, "android-fm"))
		    {
		      sprintf(buf, "androidfm");
		    }
		  else
		    {
		      sprintf(buf, "%s", class_table[c]->who_name);
		    }
		  do_help(ch, buf);
		  return;
		}
	    }
	  i=0;
	  send_to_desc_color("\n\r&c==============================================================================&D",d);
	  for (i_class = 0; i_class < 31; i_class++)
	    {
	      if(i_class == 4)
		{
		  continue;
		}
	      if(i_class > 8 && i_class < 28)
		continue;
	      char letters[14] = "abcdefghijklmn";
	      if (class_table[i_class]->who_name &&
		  class_table[i_class]->who_name[0] != '\0')
		{
		  sprintf(buf, "\n\r&w   (&W%d&w)  &c%-12s&w  ('&R%c&w' for help)&D", i,
			  class_table[i_class]->who_name, letters[i]);
		  send_to_desc_color(buf, d);
		  i++;
		}
	    }
	  send_to_desc_color("\n\r&c==============================================================================&D",d);
	  sprintf(buf, "\n\r&wChoose the number of your race: &D");
	  send_to_desc_color(buf, d);
	  return;
	}
      
      if (i_class != 28 && i_class != 29 && i_class != 30)
	{
	  if (i_class > 8
	      || !class_table[i_class]->who_name
	      || class_table[i_class]->who_name[0] == '\0'
	      || !str_cmp(class_table[i_class]->who_name, "unused"))
	    {
	      send_to_desc_color("&wThat's not a race.\n\rWhat IS your race? &D", d);
	      return;
	    }
	}

      if (!str_cmp(class_table[i_class]->who_name, "bio-android")
	  || !str_cmp(class_table[i_class]->who_name, "demon")
	  || !str_cmp(class_table[i_class]->who_name, "android-e")
	  || !str_cmp(class_table[i_class]->who_name, "android-h")
	  || !str_cmp(class_table[i_class]->who_name, "android-fm"))
        {
	  send_to_desc_color("&wThat race is currently disabled.\n\rPlease select a different race. &D", d);
	  return;
	}

      if (ch->race == 3 || ch->race == 5 || ch->race == 6)
	{
	  ch->pcdata->haircolor = 24;
	  ch->pcdata->orignalhaircolor = 24;
	}
      else
	{
	  send_to_desc_color("\n\r&wPlease choose a hair color from the following list:&D\n\r", d);
	  buf[0] = '\0';

	  for (i_class = 0; i_class < (MAX_HAIR - 2); i_class++)
	    {
	      sprintf(buf2, "&w[&W%2d&w] &g%-18.18s  ", i_class, hair_color[i_class]);
	      b++;
	      strcat(buf,buf2);
	      if ((b % 3) == 0)
		strcat(buf, "&D\n\r");
	    }
	  strcat(buf, "\n\r: ");
	  strcat(buf, "\r\r");
	  send_to_desc_color(buf, d);
	  d->connected = CON_GET_NEW_HAIR;
	  break;
	}
    case CON_GET_NEW_HAIR:
      argument = one_argument(argument, arg);
      if (ch->race != 3 && ch->race != 5 && ch->race != 6)
	{
	  if (!str_cmp(arg, "help"))
	    {
	      do_help(ch, argument);
	      send_to_desc_color("&wPlease choose a hair color: &D", d);
	      return;
	    }
	  for (i_class = 0; i_class < (MAX_HAIR - 2); i_class++)
	    {
	      if (toupper(arg[0]) == toupper(hair_color[i_class][0])
		  &&   !str_prefix(arg, hair_color[i_class]))
		{
		  ch->pcdata->haircolor = i_class;
		  ch->pcdata->orignalhaircolor = i_class;
		  break;
		}
	      if (is_number(arg) && atoi(arg) == i_class)
		{
		  ch->pcdata->haircolor = i_class;
		  ch->pcdata->orignalhaircolor = i_class;
		  break;
		}
	    }
	  if (i_class == (MAX_HAIR - 2) || !hair_color[i_class] || hair_color[i_class][0] == '\0')
	    {
	      send_to_desc_color("&wThat's not a hair color.\n\rWhat IS it going to be? &D", d);
	      return;
	    }
	}
      if (ch->race == 3 || ch->race == 6)
	{
	  send_to_desc_color("\n\r&wPlease choose a main body color from the following list:&D\n\r", d);
	  buf[0] = '\0';
	  buf2[0] = '\0';
	  b = 0;
	  for (i_class = (MAX_COMPLEXION - 17); i_class < (MAX_COMPLEXION - 14); i_class++)
	    {
	      sprintf(buf2, "&w[&W%2d&W] &g%-15s&D", i_class, complexion[i_class]);
	      b++;
	      strcat(buf,buf2);
	      if ((b % 4) == 0)
		strcat(buf, "\n\r");
	    }
	  strcat(buf, "\n\r: ");
	  strcat(buf, "\r\r\r\r");
	  send_to_desc_color(buf, d);
	  d->connected = CON_GET_NEW_COMPLEXION;
	  break;
	}

      else if (ch->race == 5)
	{
	  send_to_desc_color("\n\r&wPlease choose a main body color from the following list:&D\n\r", d);
	  buf[0] = '\0';
	  buf2[0] = '\0';
	  b = 0;
	  for (i_class = (MAX_COMPLEXION - 14); i_class < (MAX_COMPLEXION); i_class++)
	    {
	      sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, complexion[i_class]);
	      b++;
	      strcat(buf,buf2);
	      if ((b % 4) == 0)
		strcat(buf, "\n\r");
	    }
	  strcat(buf, "\n\r: ");
	  strcat(buf, "\r\r\r\r");
	  send_to_desc_color(buf, d);
	  d->connected = CON_GET_NEW_COMPLEXION;
	  break;
	}
      else
	{
	  send_to_desc_color("\n\r&wPlease choose a complexion from the following list:&D\n\r", d);
	  buf[0] = '\0';
	  buf2[0] = '\0';
	  b = 0;
	  for (i_class = 0; i_class < (MAX_COMPLEXION - 17); i_class++)
	    {
	      sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, complexion[i_class]);
	      b++;
	      strcat(buf,buf2);
	      if ((b % 4) == 0)
		strcat(buf, "\n\r");
	    }
	  strcat(buf, "\n\r: ");
	  strcat(buf, "\r\r\r\r");
	  send_to_desc_color(buf, d);
	  d->connected = CON_GET_NEW_COMPLEXION;
	  break;
	}
    case CON_GET_NEW_COMPLEXION:

      argument = one_argument(argument, arg);
      if (ch->race == 5)
	{
	  if (!str_cmp(arg, "help"))
	    {
	      do_help(ch, argument);
	      send_to_desc_color("&wPlease choose a main body color: &D", d);
	      return;
	    }
	  for (i_class = (MAX_COMPLEXION - 14); i_class < (MAX_COMPLEXION); i_class++)
	    {
	      if (toupper(arg[0]) == toupper(complexion[i_class][0])
		  &&   !str_prefix(arg, complexion[i_class]))
		{
		  ch->pcdata->complexion = i_class;
		  break;
		}
	      if (is_number(arg) && atoi(arg) == i_class)
		{
		  ch->pcdata->complexion = i_class;
		  break;
		}
	    }
	  if (i_class == (MAX_COMPLEXION) || !complexion[i_class] || complexion[i_class][0] == '\0')
	    {
	      send_to_desc_color("&wThat's not a choice.\n\rWhat IS it going to be? &D", d);
	      return;
	    }
	}
      else if (ch->race == 3 || ch->race == 6)
	{
	  if (!str_cmp(arg, "help"))
	    {
	      do_help(ch, argument);
	      send_to_desc_color("&wPlease choose a main body color: &D", d);
	      return;
	    }
	  for (i_class = (MAX_COMPLEXION - 17); i_class < (MAX_COMPLEXION - 13); i_class++)
	    {
	      if (toupper(arg[0]) == toupper(complexion[i_class][0])
		  &&   !str_prefix(arg, complexion[i_class]))
		{
		  ch->pcdata->complexion = i_class;
		  break;
		}
	      if (is_number(arg) && atoi(arg) == i_class)
		{
		  ch->pcdata->complexion = i_class;
		  break;
		}
	    }
	  if (i_class == (MAX_COMPLEXION - 14) || !complexion[i_class] || complexion[i_class][0] == '\0')
	    {
	      send_to_desc_color("&wThat's not a choice.\n\rWhat IS it going to be? &D", d);
	      return;
	    }
	}
      else
	{
	  if (!str_cmp(arg, "help"))
	    {
	      do_help(ch, argument);
	      send_to_desc_color("&wPlease choose complexion: &D", d);
	      return;
	    }
	  for (i_class = 0; i_class < (MAX_COMPLEXION - 17); i_class++)
	    {
	      if (toupper(arg[0]) == toupper(complexion[i_class][0])
		  &&   !str_prefix(arg, complexion[i_class]))
		{
		  ch->pcdata->complexion = i_class;
		  break;
		}
	      if (is_number(arg) && atoi(arg) == i_class)
		{
		  ch->pcdata->complexion = i_class;
		  break;
		}
	    }
	  if (i_class == (MAX_COMPLEXION - 17) || !complexion[i_class] || complexion[i_class][0] == '\0')
	    {
	      send_to_desc_color("&wThat's not a complexion.\n\rWhat IS it going to be? &D", d);
	      return;
	    }
	}
      if (ch->race == 5)
	{
	  send_to_desc_color("\n\r&wPlease choose a secondary body color from the following list:&D\n\r", d);
	  buf[0] = '\0';
	  buf2[0] = '\0';
	  b = 0;
	  for (i_class = 0; i_class < (MAX_SECONDARYCOLOR - 1); i_class++)
	    {
	      sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, secondary_color[i_class]);
	      b++;
	      strcat(buf,buf2);
	      if ((b % 4) == 0)
		strcat(buf, "\n\r");
	    }
	  strcat(buf, "\n\r: ");
	  strcat(buf, "\r\r\r\r");
	  send_to_desc_color(buf, d);
	  d->connected = CON_GET_NEW_SECONDARYCOLOR;
	  break;

	case CON_GET_NEW_SECONDARYCOLOR:
	  /* Black, Brown, Red, Blonde, Strawberry Blonde, Argent, Golden Blonde, Platinum Blonde, Light Brown*/
	  argument = one_argument(argument, arg);
	  if (!str_cmp(arg, "help"))
	    {
	      do_help(ch, argument);
	      send_to_desc_color("&wPlease choose a secondary body color: &D", d);
	      return;
	    }
	  for (i_class = 0; i_class < (MAX_SECONDARYCOLOR - 1); i_class++)
	    {
	      if (toupper(arg[0]) == toupper(secondary_color[i_class][0])
		  &&   !str_prefix(arg, secondary_color[i_class]))
		{
		  ch->pcdata->secondarycolor = i_class;
		  break;
		}
	      if (is_number(arg) && atoi(arg) == i_class)
		{
		  ch->pcdata->secondarycolor = i_class;
		  break;
		}
	    }
	  if (i_class == (MAX_SECONDARYCOLOR - 1) || !secondary_color[i_class] || secondary_color[i_class][0] == '\0')
	    {
	      send_to_desc_color("&wThat's not a choice.\n\rWhat IS it going to be? &D", d);
	      return;
	    }
	}
      send_to_desc_color("\n\r&wPlease choose a eye color from the following list:&D\n\r", d);
      buf[0] = '\0';
      buf2[0] = '\0';
      b = 0;

      for (i_class = 0; i_class < (MAX_EYE - 3); i_class++)
	{
	  sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, eye_color[i_class]);
	  b++;
	  strcat(buf,buf2);
	  if ((b % 4) == 0)
	    strcat(buf, "\n\r");
	}
      strcat(buf, "\n\r: ");
      strcat(buf, "\r\r\r\r");
      send_to_desc_color(buf, d);
      d->connected = CON_GET_NEW_EYE;

      break;
    case CON_GET_NEW_EYE:
      /* Black, Brown, Red, Blonde, Strawberry Blonde, Argent, Golden Blonde, Platinum Blonde, Light Brown*/
      argument = one_argument(argument, arg);
      if (!str_cmp(arg, "help"))
	{
	  do_help(ch, argument);
	  send_to_desc_color("&wPlease choose a hair color: &D", d);
	  return;
	}
      for (i_class = 0; i_class < (MAX_EYE - 3); i_class++)
	{
	  if (toupper(arg[0]) == toupper(eye_color[i_class][0])
	      &&   !str_prefix(arg, eye_color[i_class]))
	    {
	      ch->pcdata->eyes = i_class;
	      ch->pcdata->orignaleyes = i_class;
	      break;
	    }
	  if (is_number(arg) && atoi(arg) == i_class)
	    {
	      ch->pcdata->eyes = i_class;
	      ch->pcdata->orignaleyes = i_class;
	      break;
	    }
	}
      if (i_class == (MAX_EYE - 3) || !eye_color[i_class] || eye_color[i_class][0] == '\0')
	{
	  send_to_desc_color("&wThat's not a eye color.\n\rWhat IS it going to be? &D", d);
	  return;
	}

      send_to_desc_color("\n\r&wPlease choose a build type from the following list:&D\n\r", d);
      buf[0] = '\0';
      buf2[0] = '\0';
      b = 0;

      for (i_class = 0; i_class < (MAX_BUILD); i_class++)
	{
	  sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, build_type[i_class]);
	  b++;
	  strcat(buf,buf2);
	  if ((b % 4) == 0)
	    strcat(buf, "\n\r");
	}
      strcat(buf, "\n\r: ");
      strcat(buf, "\r\r\r\r");
      send_to_desc_color(buf, d);
      d->connected = CON_GET_NEW_BUILD;

      break;
    case CON_GET_NEW_BUILD:
      /* Black, Brown, Red, Blonde, Strawberry Blonde, Argent, Golden Blonde, Platinum Blonde, Light Brown*/
      argument = one_argument(argument, arg);
      if (!str_cmp(arg, "help"))
	{
	  do_help(ch, argument);
	  send_to_desc_color("&wPlease choose a build type: &D", d);
	  return;
	}
      for (i_class = 0; i_class < (MAX_BUILD); i_class++)
	{
	  if (toupper(arg[0]) == toupper(build_type[i_class][0])
	      &&   !str_prefix(arg, build_type[i_class]))
	    {
	      ch->pcdata->build = i_class;
	      break;
	    }
	  if (is_number(arg) && atoi(arg) == i_class)
	    {
	      ch->pcdata->build = i_class;
	      break;
	    }
	}
      if (i_class == (MAX_BUILD) || !build_type[i_class] || build_type[i_class][0] == '\0')
	{
	  send_to_desc_color("&wThat's not a build type.\n\rWhat IS it going to be? &D", d);
	  return;
	}

      sprintf(log_buf, "%s@%s new %s %s.", ch->name, d->host,
	      race_table[ch->race]->race_name,
	      class_table[ch->class]->who_name);
      log_string_plus(log_buf, LOG_COMM, sysdata.log_level);
      to_channel(log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL);
      send_to_desc_color("&wPress [&RENTER&w] &D", d);
      show_title(d);
      ch->level = 0;
      ch->position = POS_STANDING;
      d->connected = CON_PRESS_ENTER;
      set_pager_color(AT_PLAIN, ch);
      adjust_hiscore("pkill", ch, ch->pcdata->pkills); /* cronel hiscore */
      adjust_hiscore("sparwins", ch, ch->pcdata->spar_wins);
      adjust_hiscore("sparloss", ch, ch->pcdata->spar_loss);
      adjust_hiscore("mkills", ch, ch->pcdata->mkills);
      adjust_hiscore("deaths", ch, (ch->pcdata->pdeaths + ch->pcdata->mdeaths));
      update_plHiscore(ch);
      adjust_hiscore("played", ch, ((get_age(ch) - 4) * 2));
      adjust_hiscore("zeni", ch, ch->gold);
      return;
      break;

    case CON_PRESS_ENTER:
      if (ch->position == POS_MOUNTED)
	ch->position = POS_STANDING;
      set_pager_color(AT_PLAIN, ch);
      if (xIS_SET(ch->act, PLR_RIP))
	send_rip_screen(ch);
      if (xIS_SET(ch->act, PLR_ANSI))
	send_to_pager("\033[2J", ch);
      else
	send_to_pager("\014", ch);
      if (IS_IMMORTAL(ch))
	do_help(ch, "imotd");
      if (ch->level == 50)
	do_help(ch, "amotd");
      if (ch->level < 50 && ch->level > 0)
	do_help(ch, "motd");
      if (ch->level == 0)
	do_help(ch, "nmotd");
      send_to_pager("\n\rPress [ENTER] ", ch);
      d->connected = CON_READ_MOTD;
      break;

    case CON_READ_MOTD:
      {
	char 	motdbuf[MAX_STRING_LENGTH];
	sprintf(motdbuf, "\n\rWelcome to %s...\n\r", sysdata.mud_name);
	send_to_desc_color(motdbuf, d);
      }
      add_char(ch);
      d->connected = CON_PLAYING;
      set_char_color(AT_DGREEN, ch);
      if (!xIS_SET(ch->act, PLR_ANSI) && d->ansi == true)
	d->ansi = false;
      else if (xIS_SET(ch->act, PLR_ANSI) && d->ansi == false)
	d->ansi = true;
      if (ch->level == 0) {
	int 	i_lang;

	ch->pcdata->upgradeL = CURRENT_UPGRADE_LEVEL;

	ch->pcdata->clan_name = STRALLOC("");
	ch->pcdata->clan = NULL;
	switch (class_table[ch->class]->attr_prime) {
	case APPLY_STR:
	  ch->perm_str = 10;
	  break;
	case APPLY_INT:
	  ch->perm_int = 10;
	  break;
	case APPLY_DEX:
	  ch->perm_dex = 10;
	  break;
	case APPLY_CON:
	  ch->perm_con = 10;
	  break;
	case APPLY_LCK:
	  ch->perm_lck = 10;
	  break;
	}

	ch->perm_str += race_table[ch->race]->str_plus;
	ch->perm_int += race_table[ch->race]->int_plus;
	ch->perm_dex += race_table[ch->race]->dex_plus;
	ch->perm_con += race_table[ch->race]->con_plus;
	ch->affected_by = race_table[ch->race]->affected;
	ch->perm_lck = number_range(0, 30);

	ch->pcdata->permTstr = ch->perm_str;
	ch->pcdata->permTspd = ch->perm_dex;
	ch->pcdata->permTint = ch->perm_int;
	ch->pcdata->permTcon = ch->perm_con;

	ch->armor += race_table[ch->race]->ac_plus;
	ch->alignment += race_table[ch->race]->alignment;
	ch->attacks = race_table[ch->race]->attacks;
	ch->defenses = race_table[ch->race]->defenses;
	ch->saving_poison_death = race_table[ch->race]->saving_poison_death;
	ch->saving_wand = race_table[ch->race]->saving_wand;
	ch->saving_para_petri = race_table[ch->race]->saving_para_petri;
	ch->saving_breath = race_table[ch->race]->saving_breath;
	ch->saving_spell_staff = race_table[ch->race]->saving_spell_staff;

	ch->height = number_range(race_table[ch->race]->height * .9, race_table[ch->race]->height * 1.1);
	ch->weight = number_range(race_table[ch->race]->weight * .9, race_table[ch->race]->weight * 1.1);

	if ((i_lang = skill_lookup("common")) < 0)
	  bug("Nanny: cannot find common language.");
	else
	  ch->pcdata->learned[i_lang] = 100;

	for (i_lang = 0; lang_array[i_lang] != LANG_UNKNOWN; i_lang++)
	  if (lang_array[i_lang] == race_table[ch->race]->language)
	    break;
	if (lang_array[i_lang] == LANG_UNKNOWN);
	else {
	  if ((i_lang = skill_lookup(lang_names[i_lang])) < 0)
	    bug("Nanny: cannot find racial language.");
	  else
	    ch->pcdata->learned[i_lang] = 100;
	}

	name_stamp_stats(ch);

	ch->level = 1;
	ch->exp = 5;
	ch->pl = 5;
	ch->gravAcc = 1;
	ch->gravSetting = 1;
	ch->exintensity = 0;
	ch->gravExp = 1000;
	ch->masteryicer = 0;
	ch->masteryssj = 0;
	ch->masterymystic = 0;
	ch->masterynamek = 0;
	ch->masterypowerup = 0;
	ch->workoutstrain = 0;
	ch->energy_ballpower = 0;
	ch->energy_balleffic = 0;
	ch->masteryenergy_ball = 0;
	ch->crusherballpower = 0;
	ch->crusherballeffic = 0;
	ch->masterycrusherball = 0;
	ch->meteorpower = 0;
	ch->meteoreffic = 0;
	ch->masterymeteor = 0;
	ch->gigantic_meteorpower = 0;
	ch->gigantic_meteoreffic = 0;
	ch->masterygigantic_meteor = 0;
	ch->ecliptic_meteorpower = 0;
	ch->ecliptic_meteoreffic = 0;
	ch->masteryecliptic_meteor = 0;
	ch->death_ballpower = 0;
	ch->death_balleffic = 0;
	ch->masterydeath_ball = 0;
	ch->energybeampower = 0;
	ch->energybeameffic = 0;
	ch->masteryenergybeam = 0;
	ch->eye_beampower = 0;
	ch->eye_beameffic = 0;
	ch->masteryeye_beam = 0;
	ch->masenkopower = 0;
	ch->masenkoeffic = 0;
	ch->masterymasenko = 0;
	ch->makosenpower = 0;
	ch->makoseneffic = 0;
	ch->masterymakosen = 0;
	ch->sbcpower = 0;
	ch->sbceffic = 0;
	ch->masterysbc = 0;
	ch->concentrated_beampower = 0;
	ch->concentrated_beameffic = 0;
	ch->masteryconcentrated_beam = 0;
	ch->kamehamehapower = 0;
	ch->kamehamehaeffic = 0;
	ch->masterykamehameha = 0;
	ch->gallic_gunpower = 0;
	ch->gallic_guneffic = 0;
	ch->masterygallic_gun = 0;
	ch->finger_beampower = 0;
	ch->finger_beameffic = 0;
	ch->masteryfinger_beam = 0;
	ch->destructo_discpower = 0;
	ch->destructo_disceffic = 0;
	ch->masterydestructo_disc = 0;
	ch->destructive_wavepower = 0;
	ch->destructive_waveeffic = 0;
	ch->masterydestructive_wave = 0;
	ch->forcewavepower = 0;
	ch->forcewaveeffic = 0;
	ch->masteryforcewave = 0;
	ch->shockwavepower = 0;
	ch->shockwaveeffic = 0;
	ch->masteryshockwave = 0;
	ch->energy_discpower = 0;
	ch->energy_disceffic = 0;
	ch->masteryenergy_disc = 0;
	ch->punchpower = 0;
	ch->puncheffic = 0;
	ch->masterypunch = 0;
	ch->haymakerpower = 0;
	ch->haymakereffic = 0;
	ch->masteryhaymaker = 0;
	ch->bashpower = 0;
	ch->basheffic = 0;
	ch->masterybash = 0;
	ch->collidepower = 0;
	ch->collideeffic = 0;
	ch->masterycollide = 0;
	ch->lariatpower = 0;
	ch->lariateffic = 0;
	ch->masterylariat = 0;
	ch->newbiepl = 0;
	
	ch->sptotal = 0;
	ch->spgain = 0;
	
	ch->sspgain = 0;
	ch->kspgain = 0;
	ch->bspgain = 0;
	
	ch->spallocated = 0;
	ch->school = 0;
	ch->strikemastery = 0;
	ch->strikerank = 0;
	ch->energymastery = 0;
	ch->energyrank = 0;
	ch->bodymastery = 0;
	ch->gravityrankup = 0;
	ch->bodyrank = 0;
	ch->heart_pl = 5;
	ch->max_hit = 1000;
	ch->max_mana += race_table[ch->race]->mana;
	ch->max_move = 100;
	ch->hit = UMAX(1, ch->max_hit);
	ch->mana = UMAX(1, ch->max_mana);
	ch->move = UMAX(1, ch->max_move);
	ch->train = 5;
	ch->max_train = 1;
	ch->pcdata->xTrain = 0;
	ch->pcdata->total_xTrain = 0;
	ch->practice = 0;
	ch->max_prac = 0;
	ch->max_energy = 1;
	ch->pcdata->admintalk = 0;
	ch->pcdata->age = 18;
	ch->pcdata->sparcount = 0;
	ch->skilleye_beam = 0;
	ch->skilldestructive_wave = 0;
	ch->skillbash = 0;
	ch->skillhaymaker = 0;
	ch->skillenergy_style = 1;
	ch->skillbruiser_style = 1;
	ch->skillhybrid_style = 1;
	ch->skillaggressive_style = 1;
	ch->skillberserk_style = 1;
	ch->skillblock = 1;
	ch->skillcollide = 0;
	ch->skilldcd = 1;
	ch->skilldefensive_style = 1;
	ch->skilldodge = 1;
	ch->skillevasive_style = 1;
	ch->skilllariat = 0;
	ch->skillpunch = 1;
	ch->skillmeditate = 1;
	ch->skillstandard_style = 1;
	ch->skillssj1 = 0;
	ch->skillssj2 = 0;
	ch->skillssj3 = 0;
	ch->skillssgod = 0;
	ch->skillssblue = 0;
	ch->skillultra_instinct = 0;
	ch->skillbbk = 0;
	ch->skillburning_attack = 0;
	ch->skillconcentrated_beam = 0;
	ch->skillblast_zone = 0;
	ch->skillcrusherball = 0;
	ch->skilldeath_ball = 0;
	ch->skilldestructo_disc = 0;
	ch->skillecliptic_meteor = 0;
	ch->skillenergy_ball = 1;
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
	ch->skillsense = 1;
	ch->skillshockwave = 0;
	ch->skillsbc = 0;
	ch->skillspirit_ball = 0;
	ch->skillspirit_bomb = 0;
	ch->skillsuppress = 0;
	ch->skillvigor = 0;
	ch->vigoreffec = 0;
	if (is_saiyan(ch) || is_hb(ch) || is_icer(ch) || is_bio(ch))
	  ch->pcdata->tail = true;
	else
	  ch->pcdata->tail = false;
	if (is_android(ch))
	  ch->pcdata->natural_ac_max = 500;
	if (is_bio(ch))
	  ch->pcdata->absorb_pl_mod = 6;
	if (is_saiyan(ch) || is_hb(ch))
	  ch->pcdata->learned[gsn_monkey_gun] = 95;

	sprintf(buf, "the %s",
		title_table[ch->class][ch->level]
		[ch->sex == SEX_FEMALE ? 1 : 0]);
	set_title(ch, buf);
	ch->pcdata->creation_date = current_time;

	xSET_BIT(ch->act, PLR_AUTOGOLD);
	xSET_BIT(ch->act, PLR_AUTOEXIT);
	xSET_BIT(ch->act, PLR_AUTO_COMPASS);
	xSET_BIT(ch->act, PLR_SPAR);
	SET_BIT(ch->pcdata->flags, PCFLAG_DEADLY);
	xSET_BIT(ch->deaf, CHANNEL_FOS);

	for (i = 1; i < MAX_BOARD; i++) {
	  for (catchup_notes = ch->pcdata->board->note_first; catchup_notes && catchup_notes->next; catchup_notes = catchup_notes->next);

	  if (!catchup_notes);
	  else {
	    ch->pcdata->last_note[i] = catchup_notes->date_stamp;
	  }
	}

	char_to_room(ch, get_room_index(ROOM_VNUM_SCHOOL));
	ch->pcdata->prompt = STRALLOC("");
      } else if (!IS_IMMORTAL(ch) && ch->pcdata->release_date > 0 &&
		 ch->pcdata->release_date > current_time) {
	if (ch->in_room->vnum == 6
	    || ch->in_room->vnum == 8
	    || ch->in_room->vnum == 1206)
	  char_to_room(ch, ch->in_room);
	else
	  char_to_room(ch, get_room_index(8));
      } else if (!IS_IMMORTAL(ch) && ch->in_room
		 && ch->in_room->vnum == 2060) {
	act(AT_GREEN, "A Strange Force rips you from the Hyperbolic Time Chamber.", ch, NULL, NULL, TO_CHAR);
	char_to_room(ch, get_room_index(2059));
      } else if (ch->in_room && (IS_IMMORTAL(ch)
				 || !xIS_SET(ch->in_room->room_flags, ROOM_PROTOTYPE))) {
	char_to_room(ch, ch->in_room);
      } else if (IS_IMMORTAL(ch)) {
	char_to_room(ch, get_room_index(ROOM_VNUM_CHAT));
      } else {
	char_to_room(ch, get_room_index(ROOM_VNUM_TEMPLE));
      }

      act(AT_ACTION, "$n has entered the game.", ch, NULL, NULL, TO_CANSEE);
      if (ch->pcdata->pet) {
	act(AT_ACTION, "$n returns to $s master from the Void.",
	    ch->pcdata->pet, NULL, ch, TO_NOTVICT);
	act(AT_ACTION, "$N returns with you to the realms.",
	    ch, NULL, ch->pcdata->pet, TO_CHAR);
      }
      ch->tmystic = 0;
      ch->mysticlearn = 0;
      ch->teaching = NULL;
      if (is_kaio(ch) && ch->alignment < 0)
	ch->alignment = 0;
      if (is_demon(ch) && ch->alignment > 0)
	ch->alignment = 0;

      remove_member(ch);
      if (ch->pcdata->clan)
	update_member(ch);

      /* For the logon pl tracker */
      ch->logon_start = ch->exp;
      do_global_boards(ch, "");

      ch->dodge = false;
      ch->block = false;
      ch->ki_dodge = false;
      ch->ki_cancel = false;
      ch->ki_deflect = false;

      do_look(ch, "auto");
      tax_player(ch);
      mccp_interest(ch);
      mail_count(ch);
      check_loginmsg(ch);

      if (!ch->was_in_room && ch->in_room == get_room_index(ROOM_VNUM_TEMPLE))
	ch->was_in_room = get_room_index(ROOM_VNUM_TEMPLE);
      else if (ch->was_in_room == get_room_index(ROOM_VNUM_TEMPLE))
	ch->was_in_room = get_room_index(ROOM_VNUM_TEMPLE);
      else if (!ch->was_in_room)
	ch->was_in_room = ch->in_room;
      break;
    case CON_NOTE_TO:
      handle_con_note_to (d, argument);
      break;
    case CON_NOTE_SUBJECT:
      handle_con_note_subject (d, argument);
      break; /* subject */
    case CON_NOTE_EXPIRE:
      handle_con_note_expire (d, argument);
      break;
    case CON_NOTE_TEXT:
      handle_con_note_text (d, argument);
      break;
    case CON_NOTE_FINISH:
      handle_con_note_finish (d, argument);
      break;
    }
}

bool
is_reserved_name(char *name)
{
	RESERVE_DATA *res;

	for (res = first_reserved; res; res = res->next)
		if ((*res->name == '*' && !str_infix(res->name + 1, name)) ||
		    !str_cmp(res->name, name))
			return (true);
	return (false);
}


/*
 * Parse a name for acceptability.
 */
bool
check_parse_name(char *name, bool newchar)
{
	/*
	 * Names checking should really only be done on new characters, otherwise
	 * we could end up with people who can't access their characters.  Would
	 * have also provided for that new area havoc mentioned below, while still
	 * disallowing current area mobnames.  I personally think that if we can
	 * have more than one mob with the same keyword, then may as well have
	 * players too though, so I don't mind that removal.  -- Alty
	 */

	if (is_reserved_name(name) && newchar)
		return (false);
	if (utf8len(name) < 3)
		return (false);
	if (utf8len(name) > 12)
		return (false);

	/*
	 * Alphanumerics only.
	 * Lock out IllIll twits.
	 */
	{
		char   *pc;
		bool 	fIll;

		fIll = true;
		for (pc = name; *pc != '\0'; pc++) {
			if (!isalpha(*pc))
				return (false);
			if (LOWER(*pc) != 'i' && LOWER(*pc) != 'l')
				fIll = false;
		}

		if (fIll)
			return (false);
	}

	return (true);
}

/*
 * Look for link-dead player to reconnect.
 */
bool
check_reconnect(DESCRIPTOR_DATA *d, char *name, bool f_conn)
{
  CHAR_DATA *ch;

  for (ch = first_char; ch; ch = ch->next)
    {
      if (!IS_NPC(ch)
	  && (!f_conn || !ch->desc)
	  &&    ch->pcdata->filename
	  &&   !str_cmp(name, ch->pcdata->filename))
	{
	  if (f_conn && ch->switched)
	    {
	      write_to_buffer(d, "Already playing.\n\rName: ", 0);
	      d->connected = CON_GET_NAME;
	      if (d->character)
		{
		  /* clear descriptor pointer to get rid of bug message in log */
		  d->character->desc = NULL;
		  free_char(d->character);
		  d->character = NULL;
		}
	      return BERR;
	    }
	  if (f_conn == false)
	    {
	      DISPOSE(d->character->pcdata->pwd);
	      d->character->pcdata->pwd = str_dup(ch->pcdata->pwd);
	    }
	  else
	    {
	      /* clear descriptor pointer to get rid of bug message in log */
	      d->character->desc = NULL;
	      free_char(d->character);
	      d->character = ch;
	      ch->desc	 = d;
	      ch->timer	 = 0;
	      send_to_char("Reconnecting.\n\r", ch);
	      do_look(ch, "auto");
	      act(AT_ACTION, "$n has reconnected.", ch, NULL, NULL, TO_CANSEE);
	      sprintf(log_buf, "%s@%s(%s) reconnected.",
		      ch->pcdata->filename, d->host, d->user);
	      log_string_plus(log_buf, LOG_COMM, UMAX(sysdata.log_level, ch->level));
	      d->connected = CON_PLAYING;
	      if (ch->pcdata->in_progress)
		send_to_char ("You have a note in progress. Type \"note write\" to continue it.\n\r",ch);

	    }
	  return true;
	}
    }

  return false;
}

bool
check_playing(DESCRIPTOR_DATA *d, char *name, bool kick)
{
  CHAR_DATA *ch;

  DESCRIPTOR_DATA *dold;
  int	cstate;

  for (dold = first_descriptor; dold; dold = dold->next)
    {
      if (dold != d
	   && ( dold->character || dold->original)
	   &&   !str_cmp(name, dold->original
			  ? dold->original->pcdata->filename :
			  dold->character->pcdata->filename))
	{
	  cstate = dold->connected;
	  ch = dold->original ? dold->original : dold->character;
	  if (!ch->name
	       || (cstate != CON_PLAYING && cstate != CON_EDITING))
	    {
	      write_to_buffer(d, "Already connected - try again.\n\r", 0);
	      sprintf(log_buf, "%s already connected.",
		       ch->pcdata->filename);
	      log_string_plus(log_buf, LOG_COMM, sysdata.log_level);
	      return BERR;
	    }
	  if (!kick)
	    return true;
	  write_to_buffer(d, "Already playing... Kicking off old connection.\n\r", 0);
	  write_to_buffer(dold, "Kicking off old connection... bye!\n\r", 0);
	  close_socket(dold, false);
	  /* clear descriptor pointer to get rid of bug message in log */
	  d->character->desc = NULL;
	  free_char(d->character);
	  d->character = ch;
	  ch->desc	 = d;
	  ch->timer	 = 0;
	  if (ch->switched)
	    do_return(ch->switched, "");
	  ch->switched = NULL;
	  send_to_char("Reconnecting.\n\r", ch);
	  do_look(ch, "auto");
	  act(AT_ACTION, "$n has reconnected, kicking off old link.",
	       ch, NULL, NULL, TO_CANSEE);
	  sprintf(log_buf, "%s@%s reconnected, kicking off old link.",
		   ch->pcdata->filename, d->host);
	  log_string_plus(log_buf, LOG_COMM, UMAX(sysdata.log_level, ch->level));
	  d->connected = cstate;
	  return true;
	}
    }

  return false;
}

void
stop_idling(CHAR_DATA *ch)
{
  ROOM_INDEX_DATA *was_in_room;

  if (!ch
       ||   !ch->desc
       ||   ch->desc->connected != CON_PLAYING
       ||   !IS_IDLE(ch))
    return;

  ch->timer = 0;
  was_in_room = ch->was_in_room;
  char_from_room(ch);
  char_to_room(ch, was_in_room);
  ch->was_in_room = ch->in_room;
  REMOVE_BIT(ch->pcdata->flags, PCFLAG_IDLE);
  act(AT_ACTION, "$n has returned from the void.", ch, NULL, NULL, TO_ROOM);
}

void
send_to_char(const char *txt, CHAR_DATA * ch)
{
	if (!ch) {
		bug("Send_to_char: NULL *ch");
		return;
	}
	send_to_char_color(txt, ch);
	return;
}

void
send_to_char_color(const char *txt, CHAR_DATA * ch)
{
	DESCRIPTOR_DATA *d;
	char   *colstr;
	const char *prevstr = txt;
	char 	colbuf[20];
	int 	ln;

	if (!ch) {
		bug("Send_to_char_color: NULL *ch");
		return;
	}
	if (!txt || !ch->desc)
		return;
	d = ch->desc;

	switch (sysdata.outBytesFlag) {
	default:
		sysdata.outBytesOther += utf8len(txt);
		break;
	case LOGBOUTCHANNEL:
		sysdata.outBytesChannel += utf8len(txt);
		break;
	case LOGBOUTCOMBAT:
		sysdata.outBytesCombat += utf8len(txt);
		break;
	case LOGBOUTMOVEMENT:
		sysdata.outBytesMovement += utf8len(txt);
		break;
	case LOGBOUTINFORMATION:
		sysdata.outBytesInformation += utf8len(txt);
		break;
	case LOGBOUTPROMPT:
		sysdata.outBytesPrompt += utf8len(txt);
		break;

	case LOGBOUTINFOCHANNEL:
		sysdata.outBytesInfoChannel += utf8len(txt);
		break;
	}
	while ((colstr = strpbrk(prevstr, "&^}")) != NULL) {
		if (colstr > prevstr)
			write_to_buffer(d, prevstr, (colstr - prevstr));
		ln = make_color_sequence(colstr, colbuf, d);
		if (ln < 0) {
			prevstr = colstr + 1;
			break;
		} else if (ln > 0)
			write_to_buffer(d, colbuf, ln);
		prevstr = colstr + 2;
	}
	if (*prevstr)
		write_to_buffer(d, prevstr, 0);
}

void
send_to_pager(const char *txt, CHAR_DATA * ch)
{
	if (!ch) {
		bug("Send_to_pager: NULL *ch");
		return;
	}
	send_to_pager_color(txt, ch);
	return;
}

void
send_to_pager_color(const char *txt, CHAR_DATA * ch)
{
	DESCRIPTOR_DATA *d;

	if (!ch) {
		bug("Send_to_pager_color: NULL *ch");
		return;
	}
	if (!txt || !ch->desc)
		return;
	d = ch->desc;
	ch = d->original ? d->original : d->character;
	send_to_char_color(txt, d->character);
}

sh_int
figure_color(sh_int AType, CHAR_DATA * ch)
{
	int 	at = AType;

	if (at >= AT_COLORBASE && at < AT_TOPCOLOR) {
		at -= AT_COLORBASE;
		if (IS_NPC(ch) || ch->pcdata->colorize[at] == -1)
			at = at_color_table[at].def_color;
		else
			at = ch->pcdata->colorize[at];
	}
	if (at < 0 || at > AT_WHITE + AT_BLINK) {
		at = AT_GREY;
	}
	return (at);
}

void
set_char_color(sh_int AType, CHAR_DATA * ch)
{
	char 	buf[16];
	CHAR_DATA *och;

	if (!ch || !ch->desc)
		return;

	och = (ch->desc->original ? ch->desc->original : ch);
	if (!IS_NPC(och) && xIS_SET(och->act, PLR_ANSI)) {
		AType = figure_color(AType, och);
		if (AType == 7)
			utf8cpy(buf, "\033[0;37m");
		else
			sprintf(buf, "\033[0;%d;%s%dm", (AType & 8) == 8,
			    (AType > 15 ? "5;" : ""), (AType & 7) + 30);
		write_to_buffer(ch->desc, buf, utf8len(buf));
	}
}

void
set_pager_color(sh_int AType, CHAR_DATA * ch)
{
	char 	buf[16];
	CHAR_DATA *och;

	if (!ch || !ch->desc)
		return;

	och = (ch->desc->original ? ch->desc->original : ch);
	if (!IS_NPC(och) && xIS_SET(och->act, PLR_ANSI)) {
		AType = figure_color(AType, och);
		if (AType == 7)
			utf8cpy(buf, "\033[0;37m");
		else
			sprintf(buf, "\033[0;%d;%s%dm", (AType & 8) == 8,
			    (AType > 15 ? "5;" : ""), (AType & 7) + 30);
		send_to_pager(buf, ch);
		ch->desc->pagecolor = AType;
	}
}

void
ch_printf(CHAR_DATA * ch, char *fmt,...)
{
	char 	buf[MAX_STRING_LENGTH * 2];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	send_to_char(buf, ch);
}

void
pager_printf(CHAR_DATA * ch, char *fmt,...)
{
	char 	buf[MAX_STRING_LENGTH * 2];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	send_to_pager(buf, ch);
}


/*
 * Function to strip off the "a" or "an" or "the" or "some" from an object's
 * short description for the purpose of using it in a sentence sent to
 * the owner of the object.  (Ie: an object with the short description
 * "a long dark blade" would return ("long dark blade" for use in a sentence)
 * like "Your long dark blade".  The object name isn't always appropriate
 * since it contains keywords that may not look proper.		-Thoric
 */
char   *
myobj(OBJ_DATA * obj)
{
	if (!str_prefix("a ", obj->short_descr))
		return (obj->short_descr + 2);
	if (!str_prefix("an ", obj->short_descr))
		return (obj->short_descr + 3);
	if (!str_prefix("the ", obj->short_descr))
		return (obj->short_descr + 4);
	if (!str_prefix("some ", obj->short_descr))
		return (obj->short_descr + 5);
	return (obj->short_descr);
}


char   *
obj_short(OBJ_DATA * obj)
{
	static char buf[MAX_STRING_LENGTH];

	if (obj->count > 1) {
		sprintf(buf, "%s (%d)", obj->short_descr, obj->count);
		return (buf);
	}
	return (obj->short_descr);
}

/* the primary output interface for formatted output. */
void
ch_printf_color(CHAR_DATA * ch, char *fmt,...)
{
	char 	buf[MAX_STRING_LENGTH * 2];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	send_to_char_color(buf, ch);
}

void
pager_printf_color(CHAR_DATA * ch, char *fmt,...)
{
	char 	buf[MAX_STRING_LENGTH * 2];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	send_to_pager_color(buf, ch);
}

#define MORPHNAME(ch)   ((ch->morph&&ch->morph->morph)? \
    ch->morph->morph->short_desc:			\
    IS_NPC(ch) ? ch->short_descr : ch->name)
#define NAME(ch)        (IS_NPC(ch) ? ch->short_descr : ch->name)

char   *
act_string(const char *format, CHAR_DATA * to, CHAR_DATA * ch,
    const void *arg1, const void *arg2, int flags)
{
	static char *const he_she[] = {"it", "he", "she"};
	static char *const him_her[] = {"it", "him", "her"};
	static char *const his_her[] = {"its", "his", "her"};
	static char *const che_she[] = {"It", "He", "She"};
	static char *const chim_her[] = {"It", "Him", "Her"};
	static char *const chis_her[] = {"Its", "His", "Her"};
	static char buf[MAX_STRING_LENGTH];
	char 	fname[MAX_INPUT_LENGTH];
	char 	temp[MAX_STRING_LENGTH];
	char   *point = buf;
	const char *str = format;
	const char *i;
	CHAR_DATA *vch = (CHAR_DATA *) arg2;
	OBJ_DATA *obj1 = (OBJ_DATA *) arg1;
	OBJ_DATA *obj2 = (OBJ_DATA *) arg2;
	bool 	capitalize = false;

	if (str[0] == '$')
		DONT_UPPER = false;
	else
		DONT_UPPER = true;

	while (*str != '\0') {
		capitalize = false;

		if (*str != '$') {
			*point++ = *str++;
			continue;
		}
		++str;
		if (!arg2 && *str >= 'A' && *str <= 'Z') {
			bug("Act: missing arg2 for code %c:", *str);
			bug(format);
			i = " <@@@> ";
		} else {
			if (*str == '*') {
				capitalize = true;
				str++;
			}
			switch (*str) {
			default:
				bug("Act: bad code %c.", *str);
				i = " <@@@> ";
				break;
			case 't':
				i = (char *) arg1;
				break;
			case 'T':
				i = (char *) arg2;
				break;
			case 'n':
				if (ch->morph == NULL)
					i = (to ? PERS(ch, to) : NAME(ch));
				else if (!IS_SET(flags, STRING_IMM))
					i = (to ? MORPHPERS(ch, to) : MORPHNAME(ch));
				else {
					sprintf(temp, "(MORPH) %s", (to ? PERS(ch, to) : NAME(ch)));
					i = temp;
				}
				break;
			case 'N':
				if (vch->morph == NULL)
					i = (to ? PERS(vch, to) : NAME(vch));
				else if (!IS_SET(flags, STRING_IMM))
					i = (to ? MORPHPERS(vch, to) : MORPHNAME(vch));
				else {
					sprintf(temp, "(MORPH) %s", (to ? PERS(vch, to) : NAME(vch)));
					i = temp;
				}
				break;

			case 'e':
				if (ch->sex > 2 || ch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", ch->name,
					    ch->sex);
					i = "it";
				} else
					i = capitalize ? che_she[URANGE(0, ch->sex, 2)] : he_she[URANGE(0, ch->sex, 2)];
					    break;
			case 'E':
				if (vch->sex > 2 || vch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", vch->name,
					    vch->sex);
					i = "it";
				} else
					i = capitalize ? che_she[URANGE(0, vch->sex, 2)] : he_she[URANGE(0, vch->sex, 2)];
					    break;
			case 'm':
				if (ch->sex > 2 || ch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", ch->name,
					    ch->sex);
					i = "it";
				} else
					i = capitalize ? chim_her[URANGE(0, ch->sex, 2)] : him_her[URANGE(0, ch->sex, 2)];
					    break;
			case 'M':
				if (vch->sex > 2 || vch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", vch->name,
					    vch->sex);
					i = "it";
				} else
					i = capitalize ? chim_her[URANGE(0, vch->sex, 2)] : him_her[URANGE(0, vch->sex, 2)];
					    break;
			case 's':
				if (ch->sex > 2 || ch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", ch->name,
					    ch->sex);
					i = "its";
				} else
					i = capitalize ? chis_her[URANGE(0, ch->sex, 2)] : his_her[URANGE(0, ch->sex, 2)];
					    break;
			case 'S':
				if (vch->sex > 2 || vch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", vch->name,
					    vch->sex);
					i = "its";
				} else
					i = capitalize ? chis_her[URANGE(0, vch->sex, 2)] : his_her[URANGE(0, vch->sex, 2)];
					    break;
			case 'q':
				i = (to == ch) ? "" : "s";
				break;
			case 'Q':
				i = (to == ch) ? "your" :
				    his_her[URANGE(0, ch->sex, 2)];
				break;
			case 'p':
				i = (!to || can_see_obj(to, obj1)
				    ? obj_short(obj1) : "something");
				break;
			case 'P':
				i = (!to || can_see_obj(to, obj2)
				    ? obj_short(obj2) : "something");
				break;
			case 'd':
				if (!arg2 || ((char *) arg2)[0] == '\0')
					i = "door";
				else {
					one_argument((char *) arg2, fname);
					i = fname;
				}
				break;
			}
		}
		++str;
		while ((*point = *i) != '\0')
			++point, ++i;
	}
	utf8cpy(point, "\n\r");
	if (!DONT_UPPER)
		buf[0] = UPPER(buf[0]);

	DONT_UPPER = false;

	return (buf);
}

#undef NAME

void
act(sh_int AType, const char *format, CHAR_DATA * ch, const void *arg1, const void *arg2, int type)
{
	char   *txt;
	CHAR_DATA *to;
	CHAR_DATA *vch = (CHAR_DATA *) arg2;

	/*
	 * Discard null and zero-length messages.
	 */
	if (!format || format[0] == '\0')
		return;

	if (!ch) {
		bug("Act: null ch. (%s)", format);
		return;
	}
	if (!ch->in_room)
		to = NULL;
	else if (type == TO_CHAR)
		to = ch;
	else
		to = ch->in_room->first_person;

	/*
	 * ACT_SECRETIVE handling
	 */
	if (IS_NPC(ch) && xIS_SET(ch->act, ACT_SECRETIVE) && type != TO_CHAR)
		return;

	if (type == TO_VICT) {
		if (!vch) {
			bug("Act: null vch with TO_VICT.");
			bug("%s (%s)", ch->name, format);
			return;
		}
		if (!vch->in_room) {
			bug("Act: vch in NULL room!");
			bug("%s -> %s (%s)", ch->name, vch->name, format);
			return;
		}
		to = vch;
	}
	if (MOBtrigger && type != TO_CHAR && type != TO_VICT && to) {
		OBJ_DATA *to_obj;

		txt = act_string(format, NULL, ch, arg1, arg2, STRING_IMM);
		if (HAS_PROG(to->in_room, ACT_PROG))
			rprog_act_trigger(txt, to->in_room, ch, (OBJ_DATA *) arg1, (void *) arg2);
		for (to_obj = to->in_room->first_content; to_obj;
		     to_obj = to_obj->next_content)
			if (HAS_PROG(to_obj->pIndexData, ACT_PROG))
				oprog_act_trigger(txt, to_obj, ch, (OBJ_DATA *) arg1, (void *) arg2);
	}
	/*
	 * Anyone feel like telling me the point of looping through the
	 * whole room when we're only sending to one char anyways..? -- Alty
	 */
	for (; to; to = (type == TO_CHAR || type == TO_VICT)
		 ? NULL : to->next_in_room) {
		if ((!to->desc
		    && (IS_NPC(to) && !HAS_PROG(to->pIndexData, ACT_PROG)))
		    || !IS_AWAKE(to))
			continue;
		if (type == TO_CHAR && to != ch)
			continue;
		if (type == TO_VICT && (to != vch || to == ch))
			continue;
		if (type == TO_ROOM && to == ch)
			continue;
		if (type == TO_NOTVICT && (to == ch || to == vch))
			continue;
		if (type == TO_CANSEE && (to == ch ||
		    (!IS_IMMORTAL(to) && !IS_NPC(ch) && (xIS_SET(ch->act, PLR_WIZINVIS)
		    && (get_trust(to) < (ch->pcdata ? ch->pcdata->wizinvis : 0))))))
			continue;

		if (IS_IMMORTAL(to))
			txt = act_string(format, to, ch, arg1, arg2, STRING_IMM);
		else
			txt = act_string(format, to, ch, arg1, arg2, STRING_NONE);

		if (to->desc) {
			set_char_color(AType, to);
			send_to_char(txt, to);
		}
		if (MOBtrigger) {
			/*
			 * Note: use original string, not string with ANSI.
			 * -- Alty
			 */
			mprog_act_trigger(txt, to, ch, (OBJ_DATA *) arg1, (void *) arg2);
		}
	}
	MOBtrigger = true;
	return;
}

char   *
default_fprompt(CHAR_DATA * ch)
{
	static char buf[60];

	if (!IS_NPC(ch)) {
		switch (ch->pcdata->battlePromptConfig) {
		default:
			if (is_android(ch)) {
				utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(%x/&B%P&w)>");
				else
					strcat(buf, "TL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(%x/&B%P&w)>");
				else
					strcat(buf, "PL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			}
			strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w)> ");
			break;
		case 1:
			if (is_android(ch)) {
				utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(%x/&B%P&w)>");
				else
					strcat(buf, "TL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(%x/&B%P&w)>");
				else
					strcat(buf, "PL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			}
			strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w)> ");
			break;
		case 2:
			if (is_android(ch)) {
				utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(&B%p&w)>");
				else
					strcat(buf, "TL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(&B%p&w)>");
				else
					strcat(buf, "PL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			}
			strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w)> ");
			break;
		case 3:
			if (is_android(ch)) {
				utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(&B%p&w)>");
				else
					strcat(buf, "TL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(&B%p&w)>");
				else
					strcat(buf, "PL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			}
			strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w)> ");
			break;
		}
	} else {
		if (is_android(ch)) {
			utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "TL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "TL(%x/&B%P&w)>");
			else
				strcat(buf, "TL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
		} else {
			utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "PL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "PL(%x/&B%P&w)>");
			else
				strcat(buf, "PL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
		}
		strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w)> ");
	}

	return (buf);
}



char   *
default_prompt(CHAR_DATA * ch)
{
	static char buf[60];

	if (!IS_NPC(ch)) {
		switch (ch->pcdata->normalPromptConfig) {
		default:
			if (is_android(ch)) {
				utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(%x/&B%P&w)>");
				else
					strcat(buf, "TL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
				else
					strcat(buf, " ");
			} else {
				utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(%x/&B%P&w)>");
				else
					strcat(buf, "PL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
				else
					strcat(buf, " ");
			}
			break;
		case 1:
			if (is_android(ch)) {
				utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(&B%p&w)>");
				else
					strcat(buf, "TL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
				else
					strcat(buf, " ");
			} else {
				utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(&B%p&w)>");
				else
					strcat(buf, "PL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
				else
					strcat(buf, " ");
			}
			break;
		}
	} else {
		if (is_android(ch)) {
			utf8cpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "TL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "TL(%x/&B%P&w)>");
			else
				strcat(buf, "TL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
		} else {
			utf8cpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "PL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "PL(%x/&B%P&w)>");
			else
				strcat(buf, "PL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
			else
				strcat(buf, " ");
		}
	}

	return (buf);
}

int
getcolor(char clr)
{
	static const char colors[16] = "xrgObpcwzRGYBPCW";
	int 	r;

	for (r = 0; r < 16; r++)
		if (clr == colors[r])
			return r;
	return (-1);
}

void
display_prompt(DESCRIPTOR_DATA * d)
{
	CHAR_DATA *ch = d->character;
	CHAR_DATA *och = (d->original ? d->original : d->character);
	CHAR_DATA *victim;
	bool 	ansi = (!IS_NPC(och) && xIS_SET(och->act, PLR_ANSI));
	const char *prompt;
	const char *helpstart = "<Type HELP START>";
	char 	buf[MAX_STRING_LENGTH];
	char   *pbuf = buf;
	int 	stat, percent;
	long double stat_ld = -1;
	long double stat_ldLong = -1;

	if (!ch) {
		bug("display_prompt: NULL ch");
		return;
	}
	if (!IS_NPC(ch) && !IS_SET(ch->pcdata->flags, PCFLAG_HELPSTART))
		prompt = helpstart;
	else if (!IS_NPC(ch) && ch->substate != SUB_NONE && ch->pcdata->subprompt
	    && ch->pcdata->subprompt[0] != '\0')
		prompt = ch->pcdata->subprompt;
	else if (IS_NPC(ch) || (!ch->fighting && (!ch->pcdata->prompt
	    || !*ch->pcdata->prompt)))
		prompt = default_prompt(ch);
	else if (ch->fighting) {
		if (!ch->pcdata->fprompt || !*ch->pcdata->fprompt)
			prompt = default_fprompt(ch);
		else
			prompt = ch->pcdata->fprompt;
	} else
		prompt = ch->pcdata->prompt;
	if (ansi) {
		utf8cpy(pbuf, "\033[0;37m");
		d->prevcolor = 0x07;
		pbuf += 7;
	}
	for (; *prompt; prompt++) {
		if (*prompt != '%') {
			*(pbuf++) = *prompt;
			continue;
		}
		++prompt;
		if (!*prompt)
			break;
		if (*prompt == *(prompt - 1)) {
			*(pbuf++) = *prompt;
			continue;
		}
		switch (*(prompt - 1)) {
		default:
			bug("Display_prompt: bad command char '%c'.", *(prompt - 1));
			break;
		case '%':
			*pbuf = '\0';
			stat = 0x80000000;
			switch (*prompt) {
			case '%':
				*pbuf++ = '%';
				*pbuf = '\0';
				break;
			case 'a':
				if (ch->level >= 10)
					stat = ch->alignment;
				else if (IS_GOOD(ch))
					utf8cpy(pbuf, "good");
				else if (IS_EVIL(ch))
					utf8cpy(pbuf, "evil");
				else
					utf8cpy(pbuf, "neutral");
				break;
			case 'A':
				sprintf(pbuf, "%s%s%s", IS_AFFECTED(ch, AFF_INVISIBLE) ? "I" : "",
				    IS_AFFECTED(ch, AFF_HIDE) ? "H" : "",
				    IS_AFFECTED(ch, AFF_SNEAK) ? "S" : "");
				break;
			case 'C':	/* Tank */
				if (!IS_IMMORTAL(ch))
					break;
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else if (!victim->fighting || (victim = victim->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else {
					if (victim->max_hit > 0)
						percent = (100 * victim->hit) / victim->max_hit;
					else
						percent = -1;
					if (percent >= 100)
						utf8cpy(pbuf, "perfect health");
					else if (percent >= 90)
						utf8cpy(pbuf, "slightly scratched");
					else if (percent >= 80)
						utf8cpy(pbuf, "few bruises");
					else if (percent >= 70)
						utf8cpy(pbuf, "some cuts");
					else if (percent >= 60)
						utf8cpy(pbuf, "several wounds");
					else if (percent >= 50)
						utf8cpy(pbuf, "nasty wounds");
					else if (percent >= 40)
						utf8cpy(pbuf, "bleeding freely");
					else if (percent >= 30)
						utf8cpy(pbuf, "covered in blood");
					else if (percent >= 20)
						utf8cpy(pbuf, "leaking guts");
					else if (percent >= 10)
						utf8cpy(pbuf, "almost dead");
					else
						utf8cpy(pbuf, "DYING");
				}
				break;
			case 'c':
				if (!IS_IMMORTAL(ch))
					break;
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else {
					if (victim->max_hit > 0)
						percent = (100 * victim->hit) / victim->max_hit;
					else
						percent = -1;
					if (percent >= 100)
						utf8cpy(pbuf, "perfect health");
					else if (percent >= 90)
						utf8cpy(pbuf, "slightly scratched");
					else if (percent >= 80)
						utf8cpy(pbuf, "few bruises");
					else if (percent >= 70)
						utf8cpy(pbuf, "some cuts");
					else if (percent >= 60)
						utf8cpy(pbuf, "several wounds");
					else if (percent >= 50)
						utf8cpy(pbuf, "nasty wounds");
					else if (percent >= 40)
						utf8cpy(pbuf, "bleeding freely");
					else if (percent >= 30)
						utf8cpy(pbuf, "covered in blood");
					else if (percent >= 20)
						utf8cpy(pbuf, "leaking guts");
					else if (percent >= 10)
						utf8cpy(pbuf, "almost dead");
					else
						utf8cpy(pbuf, "DYING");
				}
				break;
			case 'h':
				stat = ch->hit;
				break;
			case 'H':
				stat = ch->max_hit;
				break;
			case 'm':
				stat = ch->mana;
				break;
			case 'M':
				stat = ch->max_mana;
				break;
			case 'P':
				stat_ld = ch->pl;
				break;
			case 'p':
				stat_ldLong = ch->pl;
				break;
			case 'N':	/* Tank */
				if (!IS_IMMORTAL(ch))
					break;
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else if (!victim->fighting || (victim = victim->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else {
					if (ch == victim)
						utf8cpy(pbuf, "You");
					else if (IS_NPC(victim))
						utf8cpy(pbuf, victim->short_descr);
					else
						utf8cpy(pbuf, victim->name);
					pbuf[0] = UPPER(pbuf[0]);
				}
				break;
			case 'n':
				if (!IS_IMMORTAL(ch))
					break;
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else {
					if (ch == victim)
						utf8cpy(pbuf, "You");
					else if (IS_NPC(victim))
						utf8cpy(pbuf, victim->short_descr);
					else
						utf8cpy(pbuf, victim->name);
					pbuf[0] = UPPER(pbuf[0]);
				}
				break;
			case 'T':
				if (time_info.hour < 5)
					utf8cpy(pbuf, "night");
				else if (time_info.hour < 6)
					utf8cpy(pbuf, "dawn");
				else if (time_info.hour < 19)
					utf8cpy(pbuf, "day");
				else if (time_info.hour < 21)
					utf8cpy(pbuf, "dusk");
				else
					utf8cpy(pbuf, "night");
				break;
			case 'b':
				stat = 0;
				break;
			case 'B':
				stat = 0;
				break;
			case 'u':
				stat = num_descriptors;
				break;
			case 'U':
				stat = sysdata.maxplayers;
				break;
			case 'v':
				stat = ch->move;
				break;
			case 'V':
				stat = ch->max_move;
				break;
			case 'g':
				stat = ch->gold;
				break;
			case 'r':
				if (IS_IMMORTAL(och))
					stat = ch->in_room->vnum;
				break;
			case 'F':
				if (IS_IMMORTAL(och))
					sprintf(pbuf, "%s", ext_flag_string(&ch->in_room->room_flags, r_flags));
				break;
			case 'R':
				if (xIS_SET(och->act, PLR_ROOMVNUM))
					sprintf(pbuf, "<#%d> ", ch->in_room->vnum);
				break;
			case 'x':
				stat_ld = ch->exp;
				break;
			case 'X':
				stat = exp_level(ch, ch->level + 1) - ch->exp;
			case 'y':
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					utf8cpy(pbuf, "N/A");
				else {
					stat = victim->hit;
				}
				break;

			case 'o':	/* display name of object on auction */
				if (auction->item)
					utf8cpy(pbuf, auction->item->name);
				break;
			case 'S':
				if (ch->style == STYLE_BERSERK)
					utf8cpy(pbuf, "B");
				else if (ch->style == STYLE_AGGRESSIVE)
					utf8cpy(pbuf, "A");
				else if (ch->style == STYLE_DEFENSIVE)
					utf8cpy(pbuf, "D");
				else if (ch->style == STYLE_EVASIVE)
					utf8cpy(pbuf, "E");
				else
					utf8cpy(pbuf, "S");
				break;
			case 'i':
				if ((!IS_NPC(ch) && xIS_SET(ch->act, PLR_WIZINVIS)) ||
				    (IS_NPC(ch) && xIS_SET(ch->act, ACT_MOBINVIS)))
					sprintf(pbuf, "(Invis %d) ", (IS_NPC(ch) ? ch->mobinvis : ch->pcdata->wizinvis));
				else if (IS_AFFECTED(ch, AFF_INVISIBLE))
					sprintf(pbuf, "(Invis) ");
				break;
			case 'I':
				stat = (IS_NPC(ch) ? (xIS_SET(ch->act, ACT_MOBINVIS) ? ch->mobinvis : 0)
				    : (xIS_SET(ch->act, PLR_WIZINVIS) ? ch->pcdata->wizinvis : 0));
				break;
			case 'z':
				stat = get_armor(ch);
				break;
			case 'Z':
				stat = get_maxarmor(ch);
				break;
			case 'f':
				stat = ch->focus;
				break;
			}
			if (stat != 0x80000000) {
				if (stat >= 1000)
					sprintf(pbuf, "%s", num_punct(stat));
				else
					sprintf(pbuf, "%d", stat);
			}
			if (stat_ld != -1) {
				sprintf(pbuf, "%s", abbNumLD(stat_ld));
				stat_ld = -1;
			} else if (stat_ldLong != -1) {
				sprintf(pbuf, "%s", num_punct_ld(stat_ldLong));
				stat_ldLong = -1;
			}
			pbuf += utf8len(pbuf);
			break;
		}
	}
	*pbuf = '\0';
	send_to_char(buf, ch);
	return;
}

int
make_color_sequence(const char *col, char *code, DESCRIPTOR_DATA * d)
{
	const char *ctype = col;
	int 	ln;

	col++;

	if (!*col)
		ln = -1;
	else if (*ctype != '&' && *ctype != '^' && *ctype != '}') {
		bug("colorcode: command '%c' not '&', '^' or '}'", *ctype);
		ln = -1;
	} else if (*col == *ctype) {
		code[0] = *col;
		code[1] = '\0';
		ln = 1;
	} else if (!d->ansi)
		ln = 0;
	else {
		/* Foreground text - non-blinking */
		if (*ctype == '&') {
			switch (*col) {
			default:
				code[0] = *ctype;
				code[1] = *col;
				code[2] = '\0';
				return 2;
			case 'i':	/* Italic text */
			case 'I':
				utf8cpy(code, ANSI_ITALIC);
				break;
			case 'v':	/* Reverse colors */
			case 'V':
				utf8cpy(code, ANSI_REVERSE);
				break;
			case 'u':	/* Underline */
			case 'U':
				utf8cpy(code, ANSI_UNDERLINE);
				break;
			case 'd':	/* Player's client default color */
				utf8cpy(code, ANSI_RESET);
				break;
			case 'D':	/* Reset to custom color for whatever
					 * is being displayed */
				utf8cpy(code, ANSI_RESET);	/* Yes, this reset here
								 * is quite necessary to
								 * cancel out other
								 * things */
				strcat(code, ANSI_GREY);
				//strcat(code, color_str(ch->desc->pagecolor, ch));
				break;
			case 'x':	/* Black */
				utf8cpy(code, ANSI_BLACK);
				break;
			case 'O':	/* Orange/Brown */
				utf8cpy(code, ANSI_ORANGE);
				break;
			case 'c':	/* Cyan */
				utf8cpy(code, ANSI_CYAN);
				break;
			case 'z':	/* Dark Grey */
				utf8cpy(code, ANSI_DGREY);
				break;
			case 'g':	/* Dark Green */
				utf8cpy(code, ANSI_DGREEN);
				break;
			case 'G':	/* Light Green */
				utf8cpy(code, ANSI_GREEN);
				break;
			case 'P':	/* Pink/Light Purple */
				utf8cpy(code, ANSI_PINK);
				break;
			case 'r':	/* Dark Red */
				utf8cpy(code, ANSI_DRED);
				break;
			case 'b':	/* Dark Blue */
				utf8cpy(code, ANSI_DBLUE);
				break;
			case 'w':	/* Grey */
				utf8cpy(code, ANSI_GREY);
				break;
			case 'Y':	/* Yellow */
				utf8cpy(code, ANSI_YELLOW);
				break;
			case 'C':	/* Light Blue */
				utf8cpy(code, ANSI_LBLUE);
				break;
			case 'p':	/* Purple */
				utf8cpy(code, ANSI_PURPLE);
				break;
			case 'R':	/* Red */
				utf8cpy(code, ANSI_RED);
				break;
			case 'B':	/* Blue */
				utf8cpy(code, ANSI_BLUE);
				break;
			case 'W':	/* White */
				utf8cpy(code, ANSI_WHITE);
				break;
			}
		}
		/* Foreground text - blinking */
		if (*ctype == '}') {
			switch (*col) {
			default:
				code[0] = *ctype;
				code[1] = *col;
				code[2] = '\0';
				return 2;
			case 'x':	/* Black */
				utf8cpy(code, BLINK_BLACK);
				break;
			case 'O':	/* Orange/Brown */
				utf8cpy(code, BLINK_ORANGE);
				break;
			case 'c':	/* Cyan */
				utf8cpy(code, BLINK_CYAN);
				break;
			case 'z':	/* Dark Grey */
				utf8cpy(code, BLINK_DGREY);
				break;
			case 'g':	/* Dark Green */
				utf8cpy(code, BLINK_DGREEN);
				break;
			case 'G':	/* Light Green */
				utf8cpy(code, BLINK_GREEN);
				break;
			case 'P':	/* Pink/Light Purple */
				utf8cpy(code, BLINK_PINK);
				break;
			case 'r':	/* Dark Red */
				utf8cpy(code, BLINK_DRED);
				break;
			case 'b':	/* Dark Blue */
				utf8cpy(code, BLINK_DBLUE);
				break;
			case 'w':	/* Grey */
				utf8cpy(code, BLINK_GREY);
				break;
			case 'Y':	/* Yellow */
				utf8cpy(code, BLINK_YELLOW);
				break;
			case 'C':	/* Light Blue */
				utf8cpy(code, BLINK_LBLUE);
				break;
			case 'p':	/* Purple */
				utf8cpy(code, BLINK_PURPLE);
				break;
			case 'R':	/* Red */
				utf8cpy(code, BLINK_RED);
				break;
			case 'B':	/* Blue */
				utf8cpy(code, BLINK_BLUE);
				break;
			case 'W':	/* White */
				utf8cpy(code, BLINK_WHITE);
				break;
			}
		}
		/* Background color */

		if (*ctype == '^') {
			switch (*col) {
			default:
				code[0] = *ctype;
				code[1] = *col;
				code[2] = '\0';
				return 2;
			case 'x':	/* Black */
				utf8cpy(code, BACK_BLACK);
				break;
			case 'r':	/* Dark Red */
				utf8cpy(code, BACK_DRED);
				break;
			case 'g':	/* Dark Green */
				utf8cpy(code, BACK_DGREEN);
				break;
			case 'O':	/* Orange/Brown */
				utf8cpy(code, BACK_ORANGE);
				break;
			case 'b':	/* Dark Blue */
				utf8cpy(code, BACK_DBLUE);
				break;
			case 'p':	/* Purple */
				utf8cpy(code, BACK_PURPLE);
				break;
			case 'c':	/* Cyan */
				utf8cpy(code, BACK_CYAN);
				break;
			case 'w':	/* Grey */
				utf8cpy(code, BACK_GREY);
				break;
			}
		}
		ln = utf8len(code);
	}
	if (ln <= 0)
		*code = '\0';
	return (ln);
}


bool
check_total_ip(DESCRIPTOR_DATA * dnew)
{
	DESCRIPTOR_DATA *d;
	CHAR_DATA *vch;
	int 	ip_total = 0;
	bool 	ch = true;
	bool 	is_nolimit = false;
	bool 	linkdead = false;

	if (!sysdata.check_plimit)
		return (false);

	if (dnew->character->level >= sysdata.level_noplimit)
		return (false);

	for (d = first_descriptor; d; d = d->next) {

		if (d == dnew) {
			continue;
		}
		vch = d->character;

		if (!vch)
			ch = false;

		if (ch) {
			if (!vch->desc)
				linkdead = true;
			if (!linkdead && dnew->character) {
				if (!str_cmp(dnew->character->name, vch->name))
					return (false);
			}
		}
		if (ch && !str_cmp(d->host, dnew->host)
		    && vch->level >= sysdata.level_noplimit)
			is_nolimit = true;

		if (!linkdead && !str_cmp(d->host, dnew->host))
			ip_total++;
	}
	if (is_nolimit)
		return (false);
	if (ip_total >= sysdata.plimit)
		return (true);
	return (false);
}

char   *
color_str(sh_int AType, CHAR_DATA * ch)
{
	if (!ch) {
		bug("color_str: NULL ch!", 0);
		return ((""));
	}
	if (IS_NPC(ch) || !xIS_SET(ch->act, PLR_ANSI))
		return ((""));

	switch (AType) {
	case 0:
		return ((ANSI_BLACK));
		break;
	case 1:
		return ((ANSI_DRED));
		break;
	case 2:
		return ((ANSI_DGREEN));
		break;
	case 3:
		return ((ANSI_ORANGE));
		break;
	case 4:
		return ((ANSI_DBLUE));
		break;
	case 5:
		return ((ANSI_PURPLE));
		break;
	case 6:
		return ((ANSI_CYAN));
		break;
	case 7:
		return ((ANSI_GREY));
		break;
	case 8:
		return ((ANSI_DGREY));
		break;
	case 9:
		return ((ANSI_RED));
		break;
	case 10:
		return ((ANSI_GREEN));
		break;
	case 11:
		return ((ANSI_YELLOW));
		break;
	case 12:
		return ((ANSI_BLUE));
		break;
	case 13:
		return ((ANSI_PINK));
		break;
	case 14:
		return ((ANSI_LBLUE));
		break;
	case 15:
		return ((ANSI_WHITE));
		break;

	default:
		return ((ANSI_RESET));
		break;
	}
}

/*
 *  Remove Colour Codes - By Komarosu
 *
 *  Colour code removal function, removes color codes but wont remove
 *  double instances of chars ( &&, }}. ^^ etc.).
 *  Not sure about memleaks, but i doubt any.
 *  Modified from a C++ Class for string modification.
 *
 */

char   *
remove_color(char *str)
{
	/* List of chars to remove */
	char   *list = "&}^";
	int 	ii, jj, kk;

	char   *aa = (char *) strdup(str);

	for (ii = 0, jj = utf8len(aa); ii < jj; ii++) {
		for (kk = 0; kk < 3; kk++) {
			if (aa[ii] == list[kk]) {
				if (aa[ii + 1] != list[kk])
					utf8cpy(&aa[ii], &aa[ii + 2]);
				ii += 2;
				break;
			}
		}
	}
	return (aa);
}

#define NAME(ch)        (IS_NPC(ch) ? ch->short_descr : ch->name)

char   *
act2_string(const char *format, CHAR_DATA * to, CHAR_DATA * ch,
    const void *arg1, const void *arg2, int flags, const void *arg3)
{
	static char *const he_she[] = {"it", "he", "she"};
	static char *const him_her[] = {"it", "him", "her"};
	static char *const his_her[] = {"its", "his", "her"};
	static char buf[MAX_STRING_LENGTH];
	char 	fname[MAX_INPUT_LENGTH];
	char 	temp[MAX_STRING_LENGTH];
	char   *point = buf;
	const char *str = format;
	const char *i;
	CHAR_DATA *vch = (CHAR_DATA *) arg2;
	OBJ_DATA *obj1 = (OBJ_DATA *) arg1;
	OBJ_DATA *obj2 = (OBJ_DATA *) arg2;

	if (str[0] == '$')
		DONT_UPPER = false;

	while (*str != '\0') {
		if (*str != '$') {
			*point++ = *str++;
			continue;
		}
		++str;
		if (!arg2 && *str >= 'A' && *str <= 'Z') {
			bug("Act: missing arg2 for code %c:", *str);
			bug(format);
			i = " <@@@> ";
		} else {
			switch (*str) {
			default:
				bug("Act: bad code %c.", *str);
				i = " <@@@> ";
				break;
			case 't':
				i = (char *) arg1;
				break;
			case 'T':
				i = (char *) arg3;
				break;
			case 'n':
				if (ch->morph == NULL)
					i = (to ? PERS(ch, to) : NAME(ch));
				else if (!IS_SET(flags, STRING_IMM))
					i = (to ? MORPHPERS(ch, to) : MORPHNAME(ch));
				else {
					sprintf(temp, "(MORPH) %s", (to ? PERS(ch, to) : NAME(ch)));
					i = temp;
				}
				break;
			case 'N':
				if (vch->morph == NULL)
					i = (to ? PERS(vch, to) : NAME(vch));
				else if (!IS_SET(flags, STRING_IMM))
					i = (to ? MORPHPERS(vch, to) : MORPHNAME(vch));
				else {
					sprintf(temp, "(MORPH) %s", (to ? PERS(vch, to) : NAME(vch)));
					i = temp;
				}
				break;

			case 'e':
				if (ch->sex > 2 || ch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", ch->name,
					    ch->sex);
					i = "it";
				} else
					i = he_she[URANGE(0, ch->sex, 2)];
				break;
			case 'E':
				if (vch->sex > 2 || vch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", vch->name,
					    vch->sex);
					i = "it";
				} else
					i = he_she[URANGE(0, vch->sex, 2)];
				break;
			case 'm':
				if (ch->sex > 2 || ch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", ch->name,
					    ch->sex);
					i = "it";
				} else
					i = him_her[URANGE(0, ch->sex, 2)];
				break;
			case 'M':
				if (vch->sex > 2 || vch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", vch->name,
					    vch->sex);
					i = "it";
				} else
					i = him_her[URANGE(0, vch->sex, 2)];
				break;
			case 's':
				if (ch->sex > 2 || ch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", ch->name,
					    ch->sex);
					i = "its";
				} else
					i = his_her[URANGE(0, ch->sex, 2)];
				break;
			case 'S':
				if (vch->sex > 2 || vch->sex < 0) {
					bug("act_string: player %s has sex set at %d!", vch->name,
					    vch->sex);
					i = "its";
				} else
					i = his_her[URANGE(0, vch->sex, 2)];
				break;
			case 'q':
				i = (to == ch) ? "" : "s";
				break;
			case 'Q':
				i = (to == ch) ? "your" :
				    his_her[URANGE(0, ch->sex, 2)];
				break;
			case 'p':
				i = (!to || can_see_obj(to, obj1)
				    ? obj_short(obj1) : "something");
				break;
			case 'P':
				i = (!to || can_see_obj(to, obj2)
				    ? obj_short(obj2) : "something");
				break;
			case 'd':
				if (!arg2 || ((char *) arg2)[0] == '\0')
					i = "door";
				else {
					one_argument((char *) arg2, fname);
					i = fname;
				}
				break;
			}
		}
		++str;
		while ((*point = *i) != '\0')
			++point, ++i;
	}
	utf8cpy(point, "\n\r");
	if (!DONT_UPPER)
		buf[0] = UPPER(buf[0]);
	return (buf);
}

#undef NAME

void
act2(sh_int AType, const char *format, CHAR_DATA * ch, const void *arg1, const void *arg2, int type, const void *arg3)
{
	char   *txt;
	CHAR_DATA *to;
	CHAR_DATA *vch = (CHAR_DATA *) arg2;

	/*
	 * Discard null and zero-length messages.
	 */
	if (!format || format[0] == '\0')
		return;

	if (!ch) {
		bug("Act: null ch. (%s)", format);
		return;
	}
	if (!ch->in_room)
		to = NULL;
	else if (type == TO_CHAR)
		to = ch;
	else
		to = ch->in_room->first_person;

	/*
	 * ACT_SECRETIVE handling
	 */
	if (IS_NPC(ch) && xIS_SET(ch->act, ACT_SECRETIVE) && type != TO_CHAR)
		return;

	if (type == TO_VICT) {
		if (!vch) {
			bug("Act: null vch with TO_VICT.");
			bug("%s (%s)", ch->name, format);
			return;
		}
		if (!vch->in_room) {
			bug("Act: vch in NULL room!");
			bug("%s -> %s (%s)", ch->name, vch->name, format);
			return;
		}
		to = vch;
	}
	if (MOBtrigger && type != TO_CHAR && type != TO_VICT && to) {
		OBJ_DATA *to_obj;

		txt = act2_string(format, NULL, ch, arg1, arg2, STRING_IMM, arg3);
		if (HAS_PROG(to->in_room, ACT_PROG))
			rprog_act_trigger(txt, to->in_room, ch, (OBJ_DATA *) arg1, (void *) arg2);
		for (to_obj = to->in_room->first_content; to_obj;
		     to_obj = to_obj->next_content)
			if (HAS_PROG(to_obj->pIndexData, ACT_PROG))
				oprog_act_trigger(txt, to_obj, ch, (OBJ_DATA *) arg1, (void *) arg2);
	}
	/*
	 * Anyone feel like telling me the point of looping through the
	 * whole room when we're only sending to one char anyways..? -- Alty
	 */
	for (; to; to = (type == TO_CHAR || type == TO_VICT)
		 ? NULL : to->next_in_room) {
		if ((!to->desc
		    && (IS_NPC(to) && !HAS_PROG(to->pIndexData, ACT_PROG)))
		    || !IS_AWAKE(to))
			continue;

		if (type == TO_CHAR && to != ch)
			continue;
		if (type == TO_VICT && (to != vch || to == ch))
			continue;
		if (type == TO_ROOM && to == ch)
			continue;
		if (type == TO_NOTVICT && (to == ch || to == vch))
			continue;
		if (type == TO_CANSEE && (to == ch ||
		    (!IS_IMMORTAL(to) && !IS_NPC(ch) && (xIS_SET(ch->act, PLR_WIZINVIS)
		    && (get_trust(to) < (ch->pcdata ? ch->pcdata->wizinvis : 0))))))
			continue;

		if (IS_IMMORTAL(to))
			txt = act2_string(format, to, ch, arg1, arg2, STRING_IMM, arg3);
		else
			txt = act2_string(format, to, ch, arg1, arg2, STRING_NONE, arg3);

		if (to->desc) {
			if (AType == AT_COLORIZE) {
				if (txt[0] == '&')
					send_to_char_color(txt, to);
				else {
					set_char_color(AT_MAGIC, to);
					send_to_char_color(txt, to);
				}
			} else {
				set_char_color(AType, to);
				send_to_char_color(txt, to);
			}
		}
		if (MOBtrigger) {
			mprog_act_trigger(txt, to, ch, (OBJ_DATA *) arg1, (void *) arg2);
		}
	}
	MOBtrigger = true;
	return;
}

bool
char_exists(char *fp) {
	char pl[MAX_STRING_LENGTH];
	p_to_fp(fp, pl);

	if(access(pl, F_OK) != -1 ) {
		return (true);
	} else {
		return (false);
	}
}

bool
check_pfile(DESCRIPTOR_DATA * d) {
	if (!d->character) {
		sprintf(log_buf, "Bad player file %s", d->user);
		log_string(log_buf);
		send_to_desc_color("Your playerfile is corrupt... please notify case@capsulecorp.org\n\r", d);
		close_socket(d, true);
		return (false);
	} else {
		return(true);
	}
}

void
p_to_fp(char *p, char *fp) {
	sprintf(fp, "%s%c/%s", PLAYER_DIR, tolower(p[0]), capitalize(p));
}

void
load_from_fp(DESCRIPTOR_DATA * d, bool np) {
	if (np)
	    load_char_obj(d, d->user, true);
	else
	    load_char_obj(d, d->user, false);
}


long long
strtonum(const char *numstr, long long minval, long long maxval,
    const char **errstrp)
{
	long long ll = 0;
	int error = 0;
	char *ep;
	struct errval {
		const char *errstr;
		int err;
	} ev[4] = {
		{ NULL,		0 },
		{ "invalid",	EINVAL },
		{ "too small",	ERANGE },
		{ "too large",	ERANGE },
	};

	ev[0].err = errno;
	errno = 0;
	if (minval > maxval) {
		error = INVALID;
	} else {
		ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval)
			error = TOOSMALL;
		else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval)
			error = TOOLARGE;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].err;
	if (error)
		ll = 0;

	return (ll);
}
