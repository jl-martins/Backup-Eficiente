cd $HOME
mkdir -p .Backup
cd .Backup
if [ ! -f "fifo" ]; then
	mkfifo fifo -m 0666
fi
sudo mv sobucli sobusrv /usr/bin
