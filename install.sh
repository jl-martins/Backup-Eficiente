mkdir -p $HOME/.Backup/data
mkdir -p $HOME/.Backup/metadata
if [ ! -f "$HOME fifo" ]; then
	mkfifo $HOME/.Backup/fifo -m 0666
fi
sudo mv -t /usr/bin sobucli sobusrv 
