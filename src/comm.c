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
bool 	check_reconnect(DESCRIPTOR_DATA * d, char *name, bool fConn);
bool 	check_playing(DESCRIPTOR_DATA * d, char *name);
bool 	reconnect(DESCRIPTOR_DATA * d, char *name);
int 	main(int argc, char **argv);
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

/* locals */
bool 	debug = false;
bool 	log_debug;
unsigned int port;

__dead void
usage(void)
{
	fprintf(stderr, "usage: dbnsd [-Dp]\n");
	exit(1);
}

void
logmsg(int pri, const char *message,...)
{
	va_list ap;

	va_start(ap, message);

	if (log_debug) {
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
	} else
		vsyslog(pri, message, ap);
	va_end(ap);
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
	const char *locale = NULL;
	const char *errstr = NULL;
	int 	ch;

	while ((ch = getopt(argc, argv, "Dp:")) != -1) {
		switch (ch) {
		case 'D':
			debug = true;
			break;
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

	locale = setlocale(LC_ALL, "en_US.UTF-8");
	DONT_UPPER = false;
	num_descriptors = 0;
	first_descriptor = NULL;
	last_descriptor = NULL;
	sysdata.NO_NAME_RESOLVING = true;

	if (port == 0) {
		logmsg(LOG_ERR, "port must be set via -p flag");
		exit(1);
	} else if (port <= 1024) {
		logmsg(LOG_ERR, "please select a port greater than 1024");
		exit(1);
	}
	if (!debug) {
		openlog("dbns", LOG_PID | LOG_CONS, LOG_DAEMON);
		if (daemon(0, 1)) {
			logmsg(LOG_WARNING, "failed to become daemon: %s",
			    strerror(errno));
		}
	}
	/* init time. */
	gettimeofday(&now_time, NULL);
	current_time = (time_t) now_time.tv_sec;
	boot_time = time(0);
	strcpy(str_boot_time, ctime(&current_time));

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
		logmsg(LOG_ERR, "main: gethostname");
		perror("main: gethostname");
		strcpy(hostn, "unresolved");
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
	char 	hostname[64];
	struct sockaddr_in sa;
	struct linger ld;
	int 	x = 1;
	int 	fd;

	gethostname(hostname, sizeof(hostname));

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("init_socket: socket");
		exit(1);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
	    (void *) &x, sizeof(x)) < 0) {
		perror("init_socket: SO_REUSEADDR");
		closesocket(fd);
		exit(1);
	}
	ld.l_onoff = 1;
	ld.l_linger = 1000;

	if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
	    (void *) &ld, sizeof(ld)) < 0) {
		perror("init_socket: SO_DONTLINGER");
		closesocket(fd);
		exit(1);
	}
	memset(&sa, '\0', sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *) & sa, sizeof(sa)) == -1) {
		perror("init_socket: bind");
		closesocket(fd);
		exit(1);
	}
	if (listen(fd, 50) < 0) {
		perror("init_socket: listen");
		closesocket(fd);
		exit(1);
	}
	return (fd);
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
			write_to_descriptor(ch->desc->descriptor, "BRB -- going down for a minute or two.\n\r", 0);
			write_to_descriptor(ch->desc->descriptor, "You have been saved to disk.\n\r", 0);
		}
	}
	logmsg(LOG_ERR, "caught sigquit, hanging up");
	exit(0);
}

static void
sig_hup()
{
	CHAR_DATA *ch;
	char 	buf[MAX_STRING_LENGTH];

	for (ch = first_char; ch; ch = ch->next) {
		if (!IS_NPC(ch))
			save_char_obj(ch);
		if (ch->desc) {
			write_to_descriptor(ch->desc->descriptor, buf, 0);
			write_to_descriptor(ch->desc->descriptor, "BRB -- going down for a minute or two.\n\r", 0);
			write_to_descriptor(ch->desc->descriptor, "You have been saved to disk.\n\r", 0);
		}
	}
	logmsg(LOG_ERR, "caught sighup, hanging up");
	exit(0);
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
			write_to_descriptor(ch->desc->descriptor, "BRB -- going down for a minute or two.\n\r", 0);
			write_to_descriptor(ch->desc->descriptor, "You have been saved to disk.\n\r", 0);
		}
	}
	logmsg(LOG_ERR, "caught sigint, hanging up");
	exit(0);
}

static void
seg_vio()
{
	CHAR_DATA *ch;
	char 	buf[MAX_STRING_LENGTH];

	for (ch = first_char; ch; ch = ch->next) {
		if (!IS_NPC(ch))
			save_char_obj(ch);
		if (ch->desc) {
			write_to_descriptor(ch->desc->descriptor, buf, 0);
			write_to_descriptor(ch->desc->descriptor, "BRB -- going down for a minute or two.\n\r", 0);
			write_to_descriptor(ch->desc->descriptor, "You have been saved to disk.\n\r", 0);
		}
	}
	logmsg(LOG_ERR, "caught segvio, dumping");
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
	strcpy(buf, "Alas, the hideous malevalent entity known only as 'Lag' rises once more!\n\r");
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
	FD_ZERO(&in_set);
	FD_ZERO(&out_set);
	FD_ZERO(&exc_set);
	FD_SET(ctrl, &in_set);

	maxdesc = ctrl;
	newdesc = 0;
	for (d = first_descriptor; d; d = d->next) {
		maxdesc = UMAX(maxdesc, d->descriptor);
		FD_SET(d->descriptor, &in_set);
		FD_SET(d->descriptor, &out_set);
		FD_SET(d->descriptor, &exc_set);
		if (d->ifd != -1 && d->ipid != -1) {
			maxdesc = UMAX(maxdesc, d->ifd);
			FD_SET(d->ifd, &in_set);
		}
		if (d == last_descriptor)
			break;
	}
	if (select(maxdesc + 1, &in_set, &out_set, &exc_set, &null_time) < 0) {
		perror("accept_new: select: poll");
		exit(1);
	}
	if (FD_ISSET(ctrl, &exc_set)) {
		bug("Exception raise on controlling descriptor %d", ctrl);
		FD_CLR(ctrl, &in_set);
		FD_CLR(ctrl, &out_set);
	} else {
		if (FD_ISSET(ctrl, &in_set)) {
			newdesc = ctrl;
			new_descriptor(newdesc);
		}
	}
}

void
game_loop()
{
	struct timeval last_time;
	char 	cmdline[MAX_INPUT_LENGTH];
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

	/* main loop */
	while (!mud_down) {
		accept_new(control);
		/*
		 * Kick out descriptors with raised exceptions
		 * or have been idle, then check for input.
		 */
		for (d = first_descriptor; d; d = d_next) {
			if (d == d->next) {
				bug("descriptor_loop: loop found & fixed");
				d->next = NULL;
			}
			d_next = d->next;
			if (FD_ISSET(d->descriptor, &exc_set)) {
				FD_CLR(d->descriptor, &in_set);
				FD_CLR(d->descriptor, &out_set);
				/* exceptional state, fquit */
				if (d->character) {
					log_string("preparing to fquit comm.c:652\n");
					fquit(d->character);
					continue;
				}
				d->outtop = 0;
				log_string("preparing to close socket at comm.c:657\n");
				close_socket(d, true, false);
				continue;
			} else {
				d->fcommand = false;
				if (FD_ISSET(d->descriptor, &in_set)) {
					if (!read_from_descriptor(d)) {
						FD_CLR(d->descriptor, &out_set);
						if (d->character
						    && (d->connected == CON_PLAYING
						    || d->connected == CON_EDITING)) {
							log_string("preparing to fquit comm.c:668\n");
							fquit(d->character);
							continue;
						}
						/* new creation EOF catch */
						if (d->character) {
							log_string("preparing to close socket at comm.c:647\n");
							close_socket(d, true, true);
							continue;
							/*
							 * odd state, no
							 * character but
							 * descriptor
							 * remains
							 */
						} else {
							log_string("preparing to close socket at comm.c:652\n");
							close_socket(d, true, false);
							continue;
						}
						log_string("made it to the end without clearing EOF");
					}
				}
				/* check for input from the dns */
				if ((d->connected == CON_PLAYING
				    || d->character != NULL)
				    && d->ifd != -1
				    && FD_ISSET(d->ifd, &in_set))
					process_dns(d);
				if (d->character && d->character->wait > 0) {
					--d->character->wait;
					continue;
				}
				read_from_buffer(d);
				if (d->incomm[0] != '\0') {
					d->fcommand = true;
					strcpy(cmdline, d->incomm);
					d->incomm[0] = '\0';

					if (d->character)
						set_cur_char(d->character);
					switch (d->connected) {
					case CON_PLAYING:
						interpret(d->character, cmdline);
						break;
					case CON_EDITING:
						edit_buffer(d->character, cmdline);
						break;
					default:
						nanny(d, cmdline);
						break;
					}
				}
			}
			if (d == last_descriptor)
				break;
		}
		/* autonomous game motion */
		update_handler();

		/* check REQUESTS pipe */
		check_requests();

		/* output */
		for (d = first_descriptor; d; d = d_next) {
			d_next = d->next;
			if ((d->fcommand || d->outtop > 0)
			    && FD_ISSET(d->descriptor, &out_set)) {
				/*
				 * ignore ret value, runtime will clear out
				 * EPIPE or EOF
				 */
				if (!flush_buffer(d, true)) {
					d->outtop = 0;
				}
			}
			if (d == last_descriptor)
				break;
		}

		/*
		 * Synchronize to a clock.
		 * sleep(last_time + 1/PULSE_PER_SECOND - now).
		 */
		struct timeval now_time;
		long 	sec_delta;
		long 	usec_delta;

		gettimeofday(&now_time, NULL);
		usec_delta = ((int) last_time.tv_usec) - ((int) now_time.tv_usec)
		    + 1000000 / PULSE_PER_SECOND;
		sec_delta = ((int) last_time.tv_sec) - ((int) now_time.tv_sec);
		while (usec_delta < 0) {
			usec_delta += 1000000;
			sec_delta -= 1;
		}

		while (usec_delta >= 1000000) {
			usec_delta -= 1000000;
			sec_delta += 1;
		}

		if (sec_delta > 0 || (sec_delta == 0 && usec_delta > 0)) {
			struct timeval stall_time;

			stall_time.tv_usec = usec_delta;
			stall_time.tv_sec = sec_delta;
			if (select(0, NULL, NULL, NULL, &stall_time) < 0 && errno != EINTR) {
				perror("game_loop: select: stall");
				exit(1);
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
	strcpy(log_buf, inet_ntoa(sock.sin_addr));

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
close_socket(DESCRIPTOR_DATA * dclose, bool force, bool clear)
{
	CHAR_DATA *ch;
	DESCRIPTOR_DATA *d;
	bool 	do_not_unlink = false;

	if (!dclose) {
		return;
	}
	if (dclose->ipid != -1) {
		int 	status;

		kill(dclose->ipid, SIGKILL);
		waitpid(dclose->ipid, &status, 0);
	}
	if (dclose->ifd != -1)
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
	/* check for switched people who go link-dead */
	if (dclose->original) {
		if ((ch = dclose->character) != NULL)
			do_return(ch, "");
		else {
			bug("Close_socket: dclose->original without character %s",
			    (dclose->original->name ? dclose->original->name : "unknown"));
			dclose->character = dclose->original;
			dclose->original = NULL;
		}
	}
	ch = dclose->character;
	/* sanity check :( */
	if (!dclose->prev && dclose != first_descriptor) {
		DESCRIPTOR_DATA *dp, *dn;

		bug("Close_socket: %s desc:%p != first_desc:%p and desc->prev = NULL!",
		    ch ? ch->name : d->host, dclose, first_descriptor);
		dp = NULL;
		for (d = first_descriptor; d; d = dn) {
			dn = d->next;
			if (d == dclose) {
				bug("Close_socket: %s desc:%p found, prev should be:%p, fixing.",
				    ch ? ch->name : d->host, dclose, dp);
				dclose->prev = dp;
				break;
			}
			dp = d;
		}
		if (!dclose->prev) {
			bug("Close_socket: %s desc:%p could not be found!.",
			    ch ? ch->name : dclose->host, dclose);
			do_not_unlink = true;
		}
	}
	if (!dclose->next && dclose != last_descriptor) {
		DESCRIPTOR_DATA *dp, *dn;

		bug("Close_socket: %s desc:%p != last_desc:%p and desc->next = NULL!",
		    ch ? ch->name : d->host, dclose, last_descriptor);
		dn = NULL;
		for (d = last_descriptor; d; d = dp) {
			dp = d->prev;
			if (d == dclose) {
				bug("Close_socket: %s desc:%p found, next should be:%p, fixing.",
				    ch ? ch->name : d->host, dclose, dn);
				dclose->next = dn;
				break;
			}
			dn = d;
		}
		if (!dclose->next) {
			bug("Close_socket: %s desc:%p could not be found!.",
			    ch ? ch->name : dclose->host, dclose);
			do_not_unlink = true;
		}
	}
	if (clear) {
		/* sanity check */
		if (ch) {
			dclose->character->desc = NULL;
			free_char(dclose->character);
		} else {
			log_string("bug: calling func thought char descriptor was free");
		}
	} else {
		/*
		 * don't clear, extract_char will take care of it for us,
		 * NULL out the descriptor
		 */
		if (dclose->character) {
			if (dclose->character->desc) {
				dclose->character->desc = NULL;
			}
		}
	}
	if (!do_not_unlink) {
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
	i_start = strlen(d->inbuf);
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
		if (d->incomm[0] != '!' && strcmp(d->incomm, d->inlast)) {
			d->repeat = 0;
		} else {
			if (++d->repeat >= 20) {
				write_to_descriptor(d->descriptor,
				    "\n\rYou cannot enter the same command more than 20 consecutive times!\n\r", 0);
				strcpy(d->incomm, "quit");
			}
		}
	}
	/* do '!' substitution. */
	if (d->incomm[0] == '!')
		strcpy(d->incomm, d->inlast);
	else
		strcpy(d->inlast, d->incomm);

	/* shift the input buffer. */
	while (d->inbuf[i] == '\n' || d->inbuf[i] == '\r')
		i++;
	for (j = 0; (d->inbuf[j] = d->inbuf[i + j]) != '\0'; j++);
	return;
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
		length = strlen(txt);

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
			close_socket(d, true, true);
			return;
		}
		d->outsize *= 2;
		RECREATE(d->outbuf, char, d->outsize);
	}

	/* copy */
	strncpy(d->outbuf + d->outtop, txt, length);
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
		length = strlen(txt);

	for (i_start = 0; i_start < length; i_start += n_write) {
		n_block = UMIN(length - i_start, 4096);
		n_write = send(desc, txt + i_start, n_block, 0);
		i_err = errno;
		/* broken pipe (linkdead) */
		if (i_err == EPIPE) {
			return (false);

		} else if (n_write < 0) {
			logmsg(LOG_ERR, "write_to_descriptor");
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
nanny(DESCRIPTOR_DATA * d, char *argument)
{
	char 	buf[MAX_STRING_LENGTH];
	char 	buf2[MAX_STRING_LENGTH];
	char 	buf3[MAX_STRING_LENGTH];
	char 	buf4[MAX_STRING_LENGTH];
	char 	arg[MAX_STRING_LENGTH];
	CHAR_DATA *ch;
	char   *pwdnew;
	char   *p;
	int 	b = 0;
	int 	i_class;
	bool    p_exists = false;
	bool 	blocked = false;
	NOTE_DATA *catchup_notes;
	int 	i = 0;

	if (d->connected != CON_NOTE_TEXT) {
		while (isspace(*argument))
			argument++;
	}
	ch = d->character;

	switch (d->connected) {
	default:
		bug("Nanny: bad d->connected %d.", d->connected);
		log_string("preparing to close socket at comm.c:1340\n");
		close_socket(d, true, true);
		return;
	case CON_GET_NAME:
		if (argument[0] == '\0') {
			close_socket(d, false, false);
			return;
		}
		*argument = capitalize_string(argument);

		if (!check_parse_name(argument, (d->newstate != 0))) {
			send_to_desc_color("&wIllegal name, try another.\n\rName: &D", d);
			return;
		}
		if (!str_cmp(argument, "New") && !blocked) {
			if (d->newstate == 0) {
				/* new player */
				sprintf(buf, "\n\r&wPlease choose a name for your character: &D");
				send_to_desc_color(buf, d);
				d->newstate++;
				d->connected = CON_GET_NAME;
				return;
			} else {
				send_to_desc_color("&wIllegal name, try another.\n\rName: &D", d);
				return;
			}
		}
		if (!str_cmp(argument, "help")) {
			HELP_DATA *p_help;

			for (p_help = first_help; p_help; p_help = p_help->next) {
				if (!str_cmp(p_help->keyword, "dbznames"))
					break;
			}
			if (!p_help) {
				send_to_desc_color("No help on that word.\n\rName: ", d);
				return;
			}
			send_to_desc_color("\n\r", d);
			send_to_desc_color(p_help->text, d);
			send_to_desc_color("\n\r\n\r&wName: ", d);
			return;
		}
		p_exists  = char_exists(argument);
		if (p_exists && d->newstate == 0) {
			d->user = STRALLOC(argument);
			send_to_desc_color("&wPassword: &D", d);
			write_to_buffer(d, echo_off_str, 0);
			d->connected = CON_GET_OLD_PASSWORD;
			return;
		} else if (p_exists && d->newstate != 0) {
			send_to_desc_color("&wThat name is already taken. Please choose another: &D", d);
			d->connected = CON_GET_NAME;
			return;
		} else if (d->newstate == 0) {
			/* no such player */
			send_to_desc_color("\n\r&wNo such player exists.\n\rPlease check your spelling, or type new to start a new player.\n\r\n\rName: &D", d);
			d->connected = CON_GET_NAME;
			return;
		} else {
			d->user = STRALLOC(argument);
			sprintf(buf, "&wDid I get that right, %s (&WY&w/&WN&w)? &D", argument);
			send_to_desc_color(buf, d);
			d->connected = CON_CONFIRM_NEW_NAME;
			return;
		}
		break;
	case CON_GET_OLD_PASSWORD:
		/* player exists, load in character */
		load_from_fp(d, false);
		ch = d->character;
		if (!check_pfile(d)) {
			return;
		}
		write_to_buffer(d, "\n\r", 2);
		if (str_cmp(sha256_crypt(argument), ch->pcdata->pwd)) {
			write_to_buffer(d, "Wrong password, disconnecting.\n\r", 0);
			close_socket(d, false, true);
			return;
		}
		write_to_buffer(d, echo_on_str, 0);
		if (check_playing(d, ch->pcdata->filename)) {
			if (!(reconnect(d, ch->pcdata->filename))) {
				write_to_buffer(d, "error, disconnecting.\n\r", 0);
				close_socket(d, false, true);
				return;
			}
			break;
		}
		strncpy(buf, ch->pcdata->filename, MAX_STRING_LENGTH);
		if (ch->position > POS_SITTING && ch->position < POS_STANDING)
			ch->position = POS_STANDING;
		sprintf(log_buf, "%s@%s(%s) has connected.", ch->pcdata->filename, d->host, d->user);

		log_string_plus(log_buf, LOG_COMM, sysdata.log_level);
		if (ch->level == 2) {
			xSET_BIT(ch->deaf, CHANNEL_FOS);
			ch->level = 1;
		}
		sprintf(buf3, "%s has logged on", ch->name);
		if (!IS_IMMORTAL(ch))
			do_info(ch, buf3);
		else
			do_ainfo(ch, buf3);

		if (!IS_IMMORTAL(ch) && IS_AFFECTED(ch, AFF_DEAD)) {
			sprintf(buf4, "%s has a halo", ch->name);
			log_string_plus(buf4, LOG_HIGH, LEVEL_LESSER);
		}
		/* player data update checks */
		pager_printf(ch, "Checking for player data updates...\n\r");

		if (ch->pcdata->upgradeL > CURRENT_UPGRADE_LEVEL)
			ch->pcdata->upgradeL = CURRENT_UPGRADE_LEVEL - 1;

		if (upgrade_player(ch))
			pager_printf(ch, "Updated player data successfully.\n\r");
		else
			pager_printf(ch, "No updates to make.\n\r");
		adjust_hiscore("pkill", ch, ch->pcdata->pkills);
		adjust_hiscore("sparwins", ch, ch->pcdata->spar_wins);
		adjust_hiscore("sparloss", ch, ch->pcdata->spar_loss);
		adjust_hiscore("mkills", ch, ch->pcdata->mkills);
		adjust_hiscore("deaths", ch, (ch->pcdata->pdeaths + ch->pcdata->mdeaths));
		update_plHiscore(ch);
		adjust_hiscore("played", ch, ((get_age(ch) - 4) * 2));
		adjust_hiscore("zeni", ch, ch->gold);
		adjust_hiscore("bounty", ch, ch->pcdata->bkills);
		update_member(ch);
		show_title(d);
		break;
	case CON_CONFIRM_NEW_NAME:
		switch (*argument) {
		case 'y':
		case 'Y':
			load_from_fp(d, true);
			ch = d->character;
			sprintf(buf, "\n\r&wMake sure to use a password that won't be easily guessed by someone else."
			    "\n\rPick a good password for %s: %s&D",
			    ch->name, echo_off_str);
			send_to_desc_color(buf, d);
			xSET_BIT(ch->act, PLR_ANSI);
			d->connected = CON_GET_NEW_PASSWORD;
			break;
		case 'n':
		case 'N':
			send_to_desc_color("&wOk, what IS it, then? &D", d);
			d->connected = CON_GET_NAME;
			break;
		default:
			send_to_desc_color("&wPlease type &WY&wes or &WN&wo. &D", d);
			break;
		}
		break;
	case CON_GET_NEW_PASSWORD:
		send_to_desc_color("\n\r", d);
		if (strlen(argument) < 5) {
			send_to_desc_color("&wPassword must be at least five characters long.\n\rPassword: &D", d);
			return;
		}
		pwdnew = sha256_crypt(argument);
		for (p = pwdnew; *p != '\0'; p++) {
			if (*p == '~') {
				send_to_desc_color("&wNew password not acceptable, try again.\n\rPassword: &D", d);
				return;
			}
		}
		DISPOSE(ch->pcdata->pwd);
		ch->pcdata->pwd = str_dup(pwdnew);
		send_to_desc_color("\n\r&wPlease retype the password to confirm: &D", d);
		d->connected = CON_CONFIRM_NEW_PASSWORD;
		break;
	case CON_CONFIRM_NEW_PASSWORD:
		send_to_desc_color("\n\r", d);
		if (str_cmp(sha256_crypt(argument), ch->pcdata->pwd)) {
			write_to_buffer(d, "Passwords don't match.\n\rRetype password: ", 0);
			d->connected = CON_GET_NEW_PASSWORD;
			return;
		}
		write_to_buffer(d, echo_on_str, 0);
		send_to_desc_color("\n\r&wWhat do you want your last name to be? [press enter for none] &D\n\r", d);
		d->connected = CON_GET_LAST_NAME;
		break;
	case CON_GET_LAST_NAME:
		if (argument[0] == '\0') {
			write_to_buffer(d, echo_on_str, 0);
			send_to_desc_color("\n\rDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w)\n\rType &WHELP&w for more information.", d);
			d->connected = CON_GET_HC;
			return;
		}
		argument[0] = UPPER(argument[0]);
		*argument = capitalize_string(argument);
		if (!check_parse_name(argument, true)) {
			send_to_desc_color("&wIllegal name, try another.\n\rLast name: &D", d);
			return;
		}
		sprintf(buf, "&wDid I get that right, %s (&WY&w/&WN&w)? &D", argument);
		send_to_desc_color(buf, d);
		DISPOSE(ch->pcdata->last_name);
		ch->pcdata->last_name = str_dup("");
		buf[0] = ' ';
		strcpy(buf + 1, argument);
		ch->pcdata->last_name = strdup(buf);
		d->connected = CON_CONFIRM_LAST_NAME;
		return;
		break;
	case CON_CONFIRM_LAST_NAME:
		switch (*argument) {
		case 'y':
		case 'Y':
			write_to_buffer(d, echo_on_str, 0);
			send_to_desc_color("\n\rDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w)\n\rType &WHELP&w for more information.", d);
			d->connected = CON_GET_HC;
			break;

		case 'n':
		case 'N':
			send_to_desc_color("&wOk, what IS it, then? &D", d);
			d->connected = CON_GET_LAST_NAME;
			break;

		default:
			send_to_desc_color("&wPlease type &WY&wes or &WN&wo. &D", d);
			break;
		}
		break;
	case CON_GET_HC:
		if (!str_cmp(argument, "help")) {
			HELP_DATA *p_help;

			for (p_help = first_help; p_help; p_help = p_help->next) {
				if (!str_cmp(p_help->keyword, "HC HARDCORE UNKNOWN"))
					break;
			}
			if (!p_help) {
				send_to_desc_color("No help on that word.\n\rDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w): ", d);
				return;
			}
			send_to_desc_color("\n\r", d);
			send_to_desc_color(p_help->text, d);
			send_to_desc_color("&wDo you wish to be a &RHARDCORE&w character? (&WY&w/&WN&w): ", d);
			return;
		}
		switch (*argument) {
		case 'y':
		case 'Y':
			sprintf(buf, "\n\r&wOkay, you are now &RHARDCORE&w!&D");
			send_to_desc_color(buf, d);
			xSET_BIT(ch->act, PLR_HC);
			write_to_buffer(d, echo_on_str, 0);
			break;

		case 'n':
		case 'N':
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
		switch (argument[0]) {
		case 'm':
		case 'M':
			ch->sex = SEX_MALE;
			break;
		case 'f':
		case 'F':
			ch->sex = SEX_FEMALE;
			break;
		case 'n':
		case 'N':
			ch->sex = SEX_NEUTRAL;
			break;
		default:
			send_to_desc_color("&wThat's not a sex.\n\rWhat IS your sex? &D", d);
			return;
		}

		send_to_desc_color("\n\r&wThe following Races are Available to You:&D\n\r", d);
		send_to_desc_color("&c==============================================================================&D", d);
		buf[0] = '\0';

		/* Take this out SHADDAI */
		i = 0;
		send_to_desc_color("\n\r", d);
		for (i_class = 0; i_class < 31; i_class++) {
			if (i_class == 4) {
				continue;
			}
			if (i_class > 8 && i_class < 28)
				continue;
			char 	letters[14] = "abcdefghijklmn";

			if (class_table[i_class]->who_name &&
			    class_table[i_class]->who_name[0] != '\0') {
				sprintf(buf, "&w   (&W%2d&w)  &c%-12s&w  ('&R%c&w' for help)&D\n\r",
				    i, class_table[i_class]->who_name, letters[i]);
				send_to_desc_color(buf, d);
				i++;
			}
		}
		send_to_desc_color("&c==============================================================================&D", d);
		sprintf(buf, "\n\r&wChoose the number of your race: &D");
		send_to_desc_color(buf, d);
		d->connected = CON_GET_NEW_CLASS;
		break;
	case CON_GET_NEW_CLASS:
		/* 1 - saiyan, 2 - human, 3 - halfie, 4 - namek, 5 - icer, 
		 * 6 - bio, 7 - kaio, 8 - demon, 9 - android-h, 
		 * 10 - android-e, 11 - android-fm
		 */
		argument = one_argument(argument, arg);
		if (is_number(arg)) {
			i = atoi(arg);
			int 	c = 0;

			if (i == 0)
				c = 0;
			if (i == 1)
				c = 1;
			if (i == 2)
				c = 2;
			if (i == 3)
				c = 3;
			if (i == 4)
				c = 5;
			if (i == 5)
				c = 6;
			if (i == 6)
				c = 7;
			if (i == 7)
				c = 8;
			if (i == 8)
				c = 28;
			if (i == 9)
				c = 29;
			if (i == 10)
				c = 30;
			for (i_class = 0; i_class < 31; i_class++) {
				if (i_class > 8 && i_class < 28)
					continue;
				if (class_table[i_class]->who_name &&
				    class_table[i_class]->who_name[0] != '\0') {
					if (c == i_class) {
						ch->class = i_class;
						ch->race = i_class;
						break;
					}
				}
			}
		} else {
			char 	letters[14] = "abcdefghijklmn";
			for (i = 0; i < 14; i++) {
				if (arg[0] == letters[i]) {
					int 	c = i;

					if (i == 0)
						c = 0;
					//saian
					if (i == 1)
						c = 1;
					//human
					if (i == 2)
						c = 2;
					//halfbreed
					if (i == 3)
						c = 3;
					//namek
					if (i == 4)
						c = 5;
					//icer
					if (i == 5)
						c = 6;
					//bio
					if (i == 6)
						c = 7;
					//kaio
					if (i == 7)
						c = 8;
					//demon
					if (i == 8)
						c = 28;
					//android - h
					if (i == 9)
						c = 29;
					//android - e
					if (i == 10)
						c = 30;
					//android - fm
					if (!str_cmp(class_table[c]->who_name, "android-h"))
						sprintf(buf, "androidh");
					else if (!str_cmp(class_table[c]->who_name, "android-e"))
						sprintf(buf, "androide");
					else if (!str_cmp(class_table[c]->who_name, "android-fm"))
						sprintf(buf, "androidfm");
					else
						sprintf(buf, "%s", class_table[c]->who_name);
					do_help(ch, buf);
					return;
				}
			}
			i = 0;
			send_to_desc_color("\n\r&c==============================================================================&D", d);
			for (i_class = 0; i_class < 31; i_class++) {
				if (i_class == 4) {
					continue;
				}
				if (i_class > 8 && i_class < 28)
					continue;
				char 	letters[14] = "abcdefghijklmn";

				if (class_table[i_class]->who_name &&
				    class_table[i_class]->who_name[0] != '\0') {
					sprintf(buf, "\n\r&w   (&W%2d&w)  &c%-12s&w  ('&R%c&w' for help)&D",
					    i,
					    class_table[i_class]->who_name, letters[i]);
					send_to_desc_color(buf, d);
					i++;
				}
			}
			send_to_desc_color("\n\r&c==============================================================================&D", d);
			sprintf(buf, "\n\r&wChoose the number of your race: &D");
			send_to_desc_color(buf, d);
			return;
		}

		if (i_class != 28 && i_class != 29 && i_class != 30)
			if (i_class > 8
			    || !class_table[i_class]->who_name
			    || class_table[i_class]->who_name[0] == '\0'
			    || !str_cmp(class_table[i_class]->who_name, "unused")) {
				send_to_desc_color("&wThat's not a race.\n\rWhat IS your race? &D", d);
				return;
			}
		if (ch->race == 3 || ch->race == 5 || ch->race == 6) {
			ch->pcdata->haircolor = 24;
			ch->pcdata->orignalhaircolor = 24;
		} else {
			send_to_desc_color("\n\r&wPlease choose a hair color from the following list:&D\n\r", d);
			buf[0] = '\0';

			for (i_class = 0; i_class < (MAX_HAIR - 2); i_class++) {
				sprintf(buf2, "&w[&W%2d&w] &g%-18.18s  ", i_class, hair_color[i_class]);
				b++;
				strcat(buf, buf2);
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
		if (ch->race != 3 && ch->race != 5 && ch->race != 6) {
			if (!str_cmp(arg, "help")) {
				do_help(ch, argument);
				send_to_desc_color("&wPlease choose a hair color: &D", d);
				return;
			}
			for (i_class = 0; i_class < (MAX_HAIR - 2); i_class++) {
				if (toupper(arg[0]) == toupper(hair_color[i_class][0])
				    && !str_prefix(arg, hair_color[i_class])) {
					ch->pcdata->haircolor = i_class;
					ch->pcdata->orignalhaircolor = i_class;
					break;
				}
				if (is_number(arg) && atoi(arg) == i_class) {
					ch->pcdata->haircolor = i_class;
					ch->pcdata->orignalhaircolor = i_class;
					break;
				}
			}
			if (i_class == (MAX_HAIR - 2) || !hair_color[i_class] || hair_color[i_class][0] == '\0') {
				send_to_desc_color("&wThat's not a hair color.\n\rWhat IS it going to be? &D", d);
				return;
			}
		}
		if (ch->race == 3 || ch->race == 6) {
			send_to_desc_color("\n\r&wPlease choose a main body color from the following list:&D\n\r", d);
			buf[0] = '\0';
			buf2[0] = '\0';
			b = 0;
			for (i_class = (MAX_COMPLEXION - 17); i_class < (MAX_COMPLEXION - 14); i_class++) {
				sprintf(buf2, "&w[&W%2d&W] &g%-15s&D", i_class, complexion[i_class]);
				b++;
				strcat(buf, buf2);
				if ((b % 4) == 0)
					strcat(buf, "\n\r");
			}
			strcat(buf, "\n\r: ");
			strcat(buf, "\r\r\r\r");
			send_to_desc_color(buf, d);
			d->connected = CON_GET_NEW_COMPLEXION;
			break;
		} else if (ch->race == 5) {
			send_to_desc_color("\n\r&wPlease choose a main body color from the following list:&D\n\r", d);
			buf[0] = '\0';
			buf2[0] = '\0';
			b = 0;
			for (i_class = (MAX_COMPLEXION - 14); i_class < (MAX_COMPLEXION); i_class++) {
				sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, complexion[i_class]);
				b++;
				strcat(buf, buf2);
				if ((b % 4) == 0)
					strcat(buf, "\n\r");
			}
			strcat(buf, "\n\r: ");
			strcat(buf, "\r\r\r\r");
			send_to_desc_color(buf, d);
			d->connected = CON_GET_NEW_COMPLEXION;
			break;
		} else {
			send_to_desc_color("\n\r&wPlease choose a complexion from the following list:&D\n\r", d);
			buf[0] = '\0';
			buf2[0] = '\0';
			b = 0;
			for (i_class = 0; i_class < (MAX_COMPLEXION - 17); i_class++) {
				sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, complexion[i_class]);
				b++;
				strcat(buf, buf2);
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
		if (ch->race == 5) {
			if (!str_cmp(arg, "help")) {
				do_help(ch, argument);
				send_to_desc_color("&wPlease choose a main body color: &D", d);
				return;
			}
			for (i_class = (MAX_COMPLEXION - 14); i_class < (MAX_COMPLEXION); i_class++) {
				if (toupper(arg[0]) == toupper(complexion[i_class][0])
				    && !str_prefix(arg, complexion[i_class])) {
					ch->pcdata->complexion = i_class;
					break;
				}
				if (is_number(arg) && atoi(arg) == i_class) {
					ch->pcdata->complexion = i_class;
					break;
				}
			}
			if (i_class == (MAX_COMPLEXION) || !complexion[i_class] || complexion[i_class][0] == '\0') {
				send_to_desc_color("&wThat's not a choice.\n\rWhat IS it going to be? &D", d);
				return;
			}
		} else if (ch->race == 3 || ch->race == 6) {
			if (!str_cmp(arg, "help")) {
				do_help(ch, argument);
				send_to_desc_color("&wPlease choose a main body color: &D", d);
				return;
			}
			for (i_class = (MAX_COMPLEXION - 17); i_class < (MAX_COMPLEXION - 13); i_class++) {
				if (toupper(arg[0]) == toupper(complexion[i_class][0])
				    && !str_prefix(arg, complexion[i_class])) {
					ch->pcdata->complexion = i_class;
					break;
				}
				if (is_number(arg) && atoi(arg) == i_class) {
					ch->pcdata->complexion = i_class;
					break;
				}
			}
			if (i_class == (MAX_COMPLEXION - 14) || !complexion[i_class] || complexion[i_class][0] == '\0') {
				send_to_desc_color("&wThat's not a choice.\n\rWhat IS it going to be? &D", d);
				return;
			}
		} else {
			if (!str_cmp(arg, "help")) {
				do_help(ch, argument);
				send_to_desc_color("&wPlease choose complexion: &D", d);
				return;
			}
			for (i_class = 0; i_class < (MAX_COMPLEXION - 17); i_class++) {
				if (toupper(arg[0]) == toupper(complexion[i_class][0])
				    && !str_prefix(arg, complexion[i_class])) {
					ch->pcdata->complexion = i_class;
					break;
				}
				if (is_number(arg) && atoi(arg) == i_class) {
					ch->pcdata->complexion = i_class;
					break;
				}
			}
			if (i_class == (MAX_COMPLEXION - 17) || !complexion[i_class] || complexion[i_class][0] == '\0') {
				send_to_desc_color("&wThat's not a complexion.\n\rWhat IS it going to be? &D", d);
				return;
			}
		}
		if (ch->race == 5) {
			send_to_desc_color("\n\r&wPlease choose a secondary body color from the following list:&D\n\r", d);
			buf[0] = '\0';
			buf2[0] = '\0';
			b = 0;
			for (i_class = 0; i_class < (MAX_SECONDARYCOLOR - 1); i_class++) {
				sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, secondary_color[i_class]);
				b++;
				strcat(buf, buf2);
				if ((b % 4) == 0)
					strcat(buf, "\n\r");
			}
			strcat(buf, "\n\r: ");
			strcat(buf, "\r\r\r\r");
			send_to_desc_color(buf, d);
			d->connected = CON_GET_NEW_SECONDARYCOLOR;
			break;

		case CON_GET_NEW_SECONDARYCOLOR:
			/*
			 * Black, Brown, Red, Blonde, Strawberry Blonde,
			 * Argent, Golden Blonde, Platinum Blonde, Light
			 * Brown
			 */
			argument = one_argument(argument, arg);
			if (!str_cmp(arg, "help")) {
				do_help(ch, argument);
				send_to_desc_color("&wPlease choose a secondary body color: &D", d);
				return;
			}
			for (i_class = 0; i_class < (MAX_SECONDARYCOLOR - 1); i_class++) {
				if (toupper(arg[0]) == toupper(secondary_color[i_class][0])
				    && !str_prefix(arg, secondary_color[i_class])) {
					ch->pcdata->secondarycolor = i_class;
					break;
				}
				if (is_number(arg) && atoi(arg) == i_class) {
					ch->pcdata->secondarycolor = i_class;
					break;
				}
			}
			if (i_class == (MAX_SECONDARYCOLOR - 1) || !secondary_color[i_class] || secondary_color[i_class][0] == '\0') {
				send_to_desc_color("&wThat's not a choice.\n\rWhat IS it going to be? &D", d);
				return;
			}
		}
		send_to_desc_color("\n\r&wPlease choose a eye color from the following list:&D\n\r", d);
		buf[0] = '\0';
		buf2[0] = '\0';
		b = 0;

		for (i_class = 0; i_class < (MAX_EYE - 3); i_class++) {
			sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, eye_color[i_class]);
			b++;
			strcat(buf, buf2);
			if ((b % 4) == 0)
				strcat(buf, "\n\r");
		}
		strcat(buf, "\n\r: ");
		strcat(buf, "\r\r\r\r");
		send_to_desc_color(buf, d);
		d->connected = CON_GET_NEW_EYE;

		break;
	case CON_GET_NEW_EYE:
		/*
		 * Black, Brown, Red, Blonde, Strawberry Blonde, Argent,
		 * Golden Blonde, Platinum Blonde, Light Brown
		 */
		argument = one_argument(argument, arg);
		if (!str_cmp(arg, "help")) {
			do_help(ch, argument);
			send_to_desc_color("&wPlease choose a hair color: &D", d);
			return;
		}
		for (i_class = 0; i_class < (MAX_EYE - 3); i_class++) {
			if (toupper(arg[0]) == toupper(eye_color[i_class][0])
			    && !str_prefix(arg, eye_color[i_class])) {
				ch->pcdata->eyes = i_class;
				ch->pcdata->orignaleyes = i_class;
				break;
			}
			if (is_number(arg) && atoi(arg) == i_class) {
				ch->pcdata->eyes = i_class;
				ch->pcdata->orignaleyes = i_class;
				break;
			}
		}
		if (i_class == (MAX_EYE - 3) || !eye_color[i_class] || eye_color[i_class][0] == '\0') {
			send_to_desc_color("&wThat's not a eye color.\n\rWhat IS it going to be? &D", d);
			return;
		}
		send_to_desc_color("\n\r&wPlease choose a build type from the following list:&D\n\r", d);
		buf[0] = '\0';
		buf2[0] = '\0';
		b = 0;

		for (i_class = 0; i_class < (MAX_BUILD); i_class++) {
			sprintf(buf2, "&w[&W%2d&w] &g%-15s&D", i_class, build_type[i_class]);
			b++;
			strcat(buf, buf2);
			if ((b % 4) == 0)
				strcat(buf, "\n\r");
		}
		strcat(buf, "\n\r: ");
		strcat(buf, "\r\r\r\r");
		send_to_desc_color(buf, d);
		d->connected = CON_GET_NEW_BUILD;

		break;
	case CON_GET_NEW_BUILD:
		argument = one_argument(argument, arg);
		if (!str_cmp(arg, "help")) {
			do_help(ch, argument);
			send_to_desc_color("&wPlease choose a build type: &D", d);
			return;
		}
		for (i_class = 0; i_class < (MAX_BUILD); i_class++) {
			if (toupper(arg[0]) == toupper(build_type[i_class][0])
			    && !str_prefix(arg, build_type[i_class])) {
				ch->pcdata->build = i_class;
				break;
			}
			if (is_number(arg) && atoi(arg) == i_class) {
				ch->pcdata->build = i_class;
				break;
			}
		}
		if (i_class == (MAX_BUILD) || !build_type[i_class] || build_type[i_class][0] == '\0') {
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
		adjust_hiscore("pkill", ch, ch->pcdata->pkills);	/* cronel hiscore */
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
		/* todo: remove this */
		REMOVE_BIT(ch->pcdata->flags, PCFLAG_WATCH);
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
			ch->exp = 100;
			ch->pl = 100;
			ch->heart_pl = 100;
			ch->max_hit += race_table[ch->race]->hit;
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
		handle_con_note_to(d, argument);
		break;
	case CON_NOTE_SUBJECT:
		handle_con_note_subject(d, argument);
		break;			
	case CON_NOTE_EXPIRE:
		handle_con_note_expire(d, argument);
		break;
	case CON_NOTE_TEXT:
		handle_con_note_text(d, argument);
		break;
	case CON_NOTE_FINISH:
		handle_con_note_finish(d, argument);
		break;
	}

	return;
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
	if (strlen(name) < 3)
		return (false);
	if (strlen(name) > 12)
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

bool
check_playing(DESCRIPTOR_DATA * d, char *name)
{
	DESCRIPTOR_DATA *d_old;

	for (d_old = first_descriptor; d_old; d_old = d_old->next) {
		if (d_old != d
		    && (d_old->character || d_old->original)
		    && !str_cmp(name, d_old->original
		    ? d_old->original->pcdata->filename :
		    d_old->character->pcdata->filename)) {
			return (true);
		}
	}

	return (false);
}

bool
reconnect(DESCRIPTOR_DATA * d, char *name)
{
	CHAR_DATA *ch;
	DESCRIPTOR_DATA *d_old;
	int 	conn_state;

	for (d_old = first_descriptor; d_old; d_old = d_old->next) {
		if (d_old != d
		    && (d_old->character || d_old->original)
		    && !str_cmp(name, d_old->original
		    ? d_old->original->pcdata->filename :
		    d_old->character->pcdata->filename)) {
			conn_state = d_old->connected;
			ch = d_old->original ? d_old->original : d_old->character;
			write_to_buffer(d, "Already playing... Kicking off old connection.\n\r", 0);
			write_to_buffer(d_old, "Kicking off old connection... bye!\n\r", 0);
			log_string("preparing to close socket at comm.c:2824\n");
			close_socket(d_old, false, false);
			d->character = ch;
			ch->desc = d;
			ch->timer = 0;
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
			d->connected = conn_state;
			return (true);
		}
	}

	return (false);
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
		sysdata.outBytesOther += strlen(txt);
		break;

	case LOGBOUTCHANNEL:
		sysdata.outBytesChannel += strlen(txt);
		break;

	case LOGBOUTCOMBAT:
		sysdata.outBytesCombat += strlen(txt);
		break;

	case LOGBOUTMOVEMENT:
		sysdata.outBytesMovement += strlen(txt);
		break;

	case LOGBOUTINFORMATION:
		sysdata.outBytesInformation += strlen(txt);
		break;

	case LOGBOUTPROMPT:
		sysdata.outBytesPrompt += strlen(txt);
		break;

	case LOGBOUTINFOCHANNEL:
		sysdata.outBytesInfoChannel += strlen(txt);
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
			strcpy(buf, "\033[0;37m");
		else
			sprintf(buf, "\033[0;%d;%s%dm", (AType & 8) == 8,
			    (AType > 15 ? "5;" : ""), (AType & 7) + 30);
		write_to_buffer(ch->desc, buf, strlen(buf));
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
			strcpy(buf, "\033[0;37m");
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
	strcpy(point, "\n\r");
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
				strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(%x/&B%P&w)>");
				else
					strcat(buf, "TL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(%x/&B%P&w)>");
				else
					strcat(buf, "PL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			}
			strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w) Armor(&c%z&w)> ");
			break;
		case 1:
			if (is_android(ch)) {
				strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(%x/&Y%P&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(%x/&B%P&w)>");
				else
					strcat(buf, "TL(%x/&W%P&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
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
				strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(&B%p&w)>");
				else
					strcat(buf, "TL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "PL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "PL(&B%p&w)>");
				else
					strcat(buf, "PL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			}
			strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w) Armor(&c%z&w)> ");
			break;
		case 3:
			if (is_android(ch)) {
				strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
				if (ch->exp < ch->pl)
					strcat(buf, "TL(&Y%p&w)>");
				else if (ch->exp > ch->pl)
					strcat(buf, "TL(&B%p&w)>");
				else
					strcat(buf, "TL(&W%p&w)>");

				if (IS_NPC(ch) || IS_IMMORTAL(ch))
					strcat(buf, " %i%R");
			} else {
				strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
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
			strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "TL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "TL(%x/&B%P&w)>");
			else
				strcat(buf, "TL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
		} else {
			strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "PL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "PL(%x/&B%P&w)>");
			else
				strcat(buf, "PL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
		}
		strcat(buf, "\n\r<Enemy(&R%y&w) Focus(&C%f&w) Armor(&c%z&w)> ");
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
				strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
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
				strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
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
				strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
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
				strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
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
			strcpy(buf, "&D<Damage(&c%h&w) En(&c%m&w) ");
			if (ch->exp < ch->pl)
				strcat(buf, "TL(%x/&Y%P&w)>");
			else if (ch->exp > ch->pl)
				strcat(buf, "TL(%x/&B%P&w)>");
			else
				strcat(buf, "TL(%x/&W%P&w)>");

			if (IS_NPC(ch) || IS_IMMORTAL(ch))
				strcat(buf, " %i%R");
		} else {
			strcpy(buf, "&D<Life(&c%h&w) Ki(&c%m&w) ");
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
		strcpy(pbuf, "\033[0;37m");
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
					strcpy(pbuf, "good");
				else if (IS_EVIL(ch))
					strcpy(pbuf, "evil");
				else
					strcpy(pbuf, "neutral");
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
					strcpy(pbuf, "N/A");
				else if (!victim->fighting || (victim = victim->fighting->who) == NULL)
					strcpy(pbuf, "N/A");
				else {
					if (victim->max_hit > 0)
						percent = (100 * victim->hit) / victim->max_hit;
					else
						percent = -1;
					if (percent >= 100)
						strcpy(pbuf, "perfect health");
					else if (percent >= 90)
						strcpy(pbuf, "slightly scratched");
					else if (percent >= 80)
						strcpy(pbuf, "few bruises");
					else if (percent >= 70)
						strcpy(pbuf, "some cuts");
					else if (percent >= 60)
						strcpy(pbuf, "several wounds");
					else if (percent >= 50)
						strcpy(pbuf, "nasty wounds");
					else if (percent >= 40)
						strcpy(pbuf, "bleeding freely");
					else if (percent >= 30)
						strcpy(pbuf, "covered in blood");
					else if (percent >= 20)
						strcpy(pbuf, "leaking guts");
					else if (percent >= 10)
						strcpy(pbuf, "almost dead");
					else
						strcpy(pbuf, "DYING");
				}
				break;
			case 'c':
				if (!IS_IMMORTAL(ch))
					break;
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					strcpy(pbuf, "N/A");
				else {
					if (victim->max_hit > 0)
						percent = (100 * victim->hit) / victim->max_hit;
					else
						percent = -1;
					if (percent >= 100)
						strcpy(pbuf, "perfect health");
					else if (percent >= 90)
						strcpy(pbuf, "slightly scratched");
					else if (percent >= 80)
						strcpy(pbuf, "few bruises");
					else if (percent >= 70)
						strcpy(pbuf, "some cuts");
					else if (percent >= 60)
						strcpy(pbuf, "several wounds");
					else if (percent >= 50)
						strcpy(pbuf, "nasty wounds");
					else if (percent >= 40)
						strcpy(pbuf, "bleeding freely");
					else if (percent >= 30)
						strcpy(pbuf, "covered in blood");
					else if (percent >= 20)
						strcpy(pbuf, "leaking guts");
					else if (percent >= 10)
						strcpy(pbuf, "almost dead");
					else
						strcpy(pbuf, "DYING");
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
					strcpy(pbuf, "N/A");
				else if (!victim->fighting || (victim = victim->fighting->who) == NULL)
					strcpy(pbuf, "N/A");
				else {
					if (ch == victim)
						strcpy(pbuf, "You");
					else if (IS_NPC(victim))
						strcpy(pbuf, victim->short_descr);
					else
						strcpy(pbuf, victim->name);
					pbuf[0] = UPPER(pbuf[0]);
				}
				break;
			case 'n':
				if (!IS_IMMORTAL(ch))
					break;
				if (!ch->fighting || (victim = ch->fighting->who) == NULL)
					strcpy(pbuf, "N/A");
				else {
					if (ch == victim)
						strcpy(pbuf, "You");
					else if (IS_NPC(victim))
						strcpy(pbuf, victim->short_descr);
					else
						strcpy(pbuf, victim->name);
					pbuf[0] = UPPER(pbuf[0]);
				}
				break;
			case 'T':
				if (time_info.hour < 5)
					strcpy(pbuf, "night");
				else if (time_info.hour < 6)
					strcpy(pbuf, "dawn");
				else if (time_info.hour < 19)
					strcpy(pbuf, "day");
				else if (time_info.hour < 21)
					strcpy(pbuf, "dusk");
				else
					strcpy(pbuf, "night");
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
					strcpy(pbuf, "N/A");
				else {
					stat = victim->hit;
				}
				break;

			case 'o':	/* display name of object on auction */
				if (auction->item)
					strcpy(pbuf, auction->item->name);
				break;
			case 'S':
				if (ch->style == STYLE_BERSERK)
					strcpy(pbuf, "B");
				else if (ch->style == STYLE_AGGRESSIVE)
					strcpy(pbuf, "A");
				else if (ch->style == STYLE_DEFENSIVE)
					strcpy(pbuf, "D");
				else if (ch->style == STYLE_EVASIVE)
					strcpy(pbuf, "E");
				else
					strcpy(pbuf, "S");
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
			pbuf += strlen(pbuf);
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
				strcpy(code, ANSI_ITALIC);
				break;
			case 'v':	/* Reverse colors */
			case 'V':
				strcpy(code, ANSI_REVERSE);
				break;
			case 'u':	/* Underline */
			case 'U':
				strcpy(code, ANSI_UNDERLINE);
				break;
			case 'd':	/* Player's client default color */
				strcpy(code, ANSI_RESET);
				break;
			case 'D':	/* Reset to custom color for whatever
					 * is being displayed */
				strcpy(code, ANSI_RESET);	/* Yes, this reset here
								 * is quite necessary to
								 * cancel out other
								 * things */
				strcat(code, ANSI_GREY);
				//strcat(code, color_str(ch->desc->pagecolor, ch));
				break;
			case 'x':	/* Black */
				strcpy(code, ANSI_BLACK);
				break;
			case 'O':	/* Orange/Brown */
				strcpy(code, ANSI_ORANGE);
				break;
			case 'c':	/* Cyan */
				strcpy(code, ANSI_CYAN);
				break;
			case 'z':	/* Dark Grey */
				strcpy(code, ANSI_DGREY);
				break;
			case 'g':	/* Dark Green */
				strcpy(code, ANSI_DGREEN);
				break;
			case 'G':	/* Light Green */
				strcpy(code, ANSI_GREEN);
				break;
			case 'P':	/* Pink/Light Purple */
				strcpy(code, ANSI_PINK);
				break;
			case 'r':	/* Dark Red */
				strcpy(code, ANSI_DRED);
				break;
			case 'b':	/* Dark Blue */
				strcpy(code, ANSI_DBLUE);
				break;
			case 'w':	/* Grey */
				strcpy(code, ANSI_GREY);
				break;
			case 'Y':	/* Yellow */
				strcpy(code, ANSI_YELLOW);
				break;
			case 'C':	/* Light Blue */
				strcpy(code, ANSI_LBLUE);
				break;
			case 'p':	/* Purple */
				strcpy(code, ANSI_PURPLE);
				break;
			case 'R':	/* Red */
				strcpy(code, ANSI_RED);
				break;
			case 'B':	/* Blue */
				strcpy(code, ANSI_BLUE);
				break;
			case 'W':	/* White */
				strcpy(code, ANSI_WHITE);
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
				strcpy(code, BLINK_BLACK);
				break;
			case 'O':	/* Orange/Brown */
				strcpy(code, BLINK_ORANGE);
				break;
			case 'c':	/* Cyan */
				strcpy(code, BLINK_CYAN);
				break;
			case 'z':	/* Dark Grey */
				strcpy(code, BLINK_DGREY);
				break;
			case 'g':	/* Dark Green */
				strcpy(code, BLINK_DGREEN);
				break;
			case 'G':	/* Light Green */
				strcpy(code, BLINK_GREEN);
				break;
			case 'P':	/* Pink/Light Purple */
				strcpy(code, BLINK_PINK);
				break;
			case 'r':	/* Dark Red */
				strcpy(code, BLINK_DRED);
				break;
			case 'b':	/* Dark Blue */
				strcpy(code, BLINK_DBLUE);
				break;
			case 'w':	/* Grey */
				strcpy(code, BLINK_GREY);
				break;
			case 'Y':	/* Yellow */
				strcpy(code, BLINK_YELLOW);
				break;
			case 'C':	/* Light Blue */
				strcpy(code, BLINK_LBLUE);
				break;
			case 'p':	/* Purple */
				strcpy(code, BLINK_PURPLE);
				break;
			case 'R':	/* Red */
				strcpy(code, BLINK_RED);
				break;
			case 'B':	/* Blue */
				strcpy(code, BLINK_BLUE);
				break;
			case 'W':	/* White */
				strcpy(code, BLINK_WHITE);
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
				strcpy(code, BACK_BLACK);
				break;
			case 'r':	/* Dark Red */
				strcpy(code, BACK_DRED);
				break;
			case 'g':	/* Dark Green */
				strcpy(code, BACK_DGREEN);
				break;
			case 'O':	/* Orange/Brown */
				strcpy(code, BACK_ORANGE);
				break;
			case 'b':	/* Dark Blue */
				strcpy(code, BACK_DBLUE);
				break;
			case 'p':	/* Purple */
				strcpy(code, BACK_PURPLE);
				break;
			case 'c':	/* Cyan */
				strcpy(code, BACK_CYAN);
				break;
			case 'w':	/* Grey */
				strcpy(code, BACK_GREY);
				break;
			}
		}
		ln = strlen(code);
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

	for (ii = 0, jj = strlen(aa); ii < jj; ii++) {
		for (kk = 0; kk < 3; kk++) {
			if (aa[ii] == list[kk]) {
				if (aa[ii + 1] != list[kk])
					strcpy(&aa[ii], &aa[ii + 2]);
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
	strcpy(point, "\n\r");
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
		close_socket(d, false, false);
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
