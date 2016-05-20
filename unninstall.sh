rm -f  $HOME/.Backup/fifo $HOME/.Backup/log.txt

sudo rm -f  /usr/bin/sobucli /usr/bin/sobusrv 
echo "Deseja apagar os dados de backups antigos?"
select sn in "Sim" "Nao"; do
    case $sn in
        Sim ) rm -rf $HOME/.Backup; break;;
        Nao ) exit;;
    esac
done

