deploy:
	ninja -t clean
	ninja
	sudo systemctl stop dbns
	cp dbnsd /home/dbns/dbns/src
	sudo systemctl start dbns
