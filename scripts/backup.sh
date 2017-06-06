#!/bin/sh
BACKUP_DIR=/var/dbns/backup
if [ ! -d $BACKUP_DIR ]; then
    doas mkdir -p $BACKUP_DIR
fi		
	
cd /var/dbns/player && doas tar czf $BACKUP_DIR/$(date +%Y%m%d-%H%M%S).tar.gz .



