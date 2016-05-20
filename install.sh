mkdir -p $HOME/.Backup/data
mkdir -p $HOME/.Backup/metadata
if [ ! -p $HOME/.Backup/fifo ]; then
	mkfifo $HOME/.Backup/fifo -m 0666
fi
sudo mv -t /usr/bin sobucli sobusrv 
