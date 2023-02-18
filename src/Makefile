CC ?= gcc
PREFIX ?= /usr/local/bin
DATA_PREFIX ?= /opt/dbns/db

MATH_LINK = -lm

DATA_FOLDERS = ../area ../clans ../classes ../councils ../deity ../doc ../gods \
	../houses ../planets ../races ../space ../system

C_FLAGS = -O2 -g3 -Wall -std=c11 ${DBUGFLG} ${TIME} ${REG}
L_FLAGS = ${MATH_LINK} -lz

O_FILES = act_comm.o act_info.o act_move.o act_obj.o act_wiz.o \
          board.o boards.o bounty.o build.o clans.o colorize.o comm.o \
          comments.o const.o db.o deity.o editor.o fight.o finger.o grub.o \
          handler.o hashstr.o hiscores.o house.o ibuild.o \
          interp.o magic.o makeobjs.o mapout.o marry.o \
          misc.o mpxset.o mud_comm.o mud_prog.o new_fun.o pfiles.o planes.o \
          planet.o player.o rare.o requests.o reset.o save.o \
          shops.o skills.o skills_android.o skills_dbs.o skills_genie.o \
          space.o special.o stat_obj.o tables.o update.o dns.o \
          sha256.o

C_FILES = act_comm.c act_info.c act_move.c act_obj.c act_wiz.c \
          board.c boards.c bounty.c build.c clans.c colorize.c comm.c \
          comments.c const.c db.c deity.c editor.c fight.c finger.c grub.c \
          handler.c hashstr.c hiscores.c house.c ibuild.c \
          interp.c magic.c makeobjs.c mapout.c marry.c \
          misc.c mpxset.c mud_comm.c mud_prog.c new_fun.c pfiles.c planes.c \
          planet.c player.c rare.c requests.c reset.c save.c \
          shops.c skills.c skills_android.c skills_dbs.c\
          space.c special.c stat_obj.c tables.c update.c dns.c skills_genie.c \
          sha256.c

H_FILES = mud.h bet.h board.h comm.h finger.h house.h pfiles.h rare.h utf8.h

all:
	@make dbns

dbns: ${O_FILES}
	${CC} ${L_FLAGS} -o dbnsd ${O_FILES}
	@chmod 755 dbnsd

.c.o: mud.h comm.h
	${CC} -c ${C_FLAGS} $<

clean:
	@\rm -f *.o dbnsd *~
