DATA ?= /var/dbns/system

install:
	@doas \cp ${DATA}/hiscores.dat /tmp/hiscores.dat
	@doas \cp *.dat ${DATA}
	@doas \cp /tmp/hiscores.dat ${DATA}/hiscores.dat
	@doas chgrp -R _dbnsd ${DATA}

.include <bsd.prog.mk>
