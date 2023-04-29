
# PVR ADB-Telnet server
cmdFile="/data/data/viritualisres.phonevr/files/pvr-adb-telnet" # Will made and written by Android; Deleted by Server
lockFile="/sdcard/pvr-adb-telnet.lock" # will be made, written and deleted by Server
shutdownFile="/data/data/viritualisres.phonevr/files/pvr-adb-telnet.sd" # will be made by Android; Deleted by Server
shutdownFile2="pvr-adb-telnet.sd" # made by host (the one who runs this server)

lenShutdownFile=0

GREEN='\033[0;32m'
NC='\033[0m'

while [ $lenShutdownFile -eq 0 ] && [ ! -e $shutdownFile2 ]
do
    cmd=$(adb shell "run-as viritualisres.phonevr cat $cmdFile")
    len=${#cmd}

    if [ $len -gt 0 ]; # Either file does not exist or empty
    then

        echo -e "Got CMD $GREEN$cmd$NC ($len)"
        token=$(cat ~/.emulator_console_auth_token)
        adb shell "echo meow >> $lockFile"

        {
          echo "auth $token";
          sleep 3;
          echo "$cmd";
          sleep 3;
          echo "exit";
          sleep 3;
        } | telnet localhost 5554

        sleep 0.1

        adb shell "run-as viritualisres.phonevr rm -f $cmdFile"
        adb shell "rm -f $lockFile"

    else
        sleep 1 # or less like 0.2
        echo "Waiting for command from Android..."
    fi

    sdData=$(adb shell "run-as viritualisres.phonevr cat $shutdownFile")
    lenShutdownFile=${#sdData}
done

echo "Shutting down Server..."
adb shell "run-as viritualisres.phonevr rm -f $shutdownFile"
rm -f $shutdownFile2
echo "Server down."