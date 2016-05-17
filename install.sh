cd $HOME
mkdir -p .Backup
cd .Backup
mkfifo fifo -m 0666
touch err_log.txt
chmod 0666 err_log.txt
