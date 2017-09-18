#!/bin/sh
AREA_DIR=/opt/dbns/db/area
BACKUP_DIR=/home/case/db/area

\cp $AREA_DIR/*.are $BACKUP_DIR
echo $AREA_DIR
echo $BACKUP_DIR
cd $BACKUP_DIR
git add .
git commit -m ".are copyover at $(date)"



