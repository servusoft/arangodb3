#Build ArangoDB auf WandBoard
##1. WandBoard-Image mit Ubuntu erstellen 
1.1 Ubuntu-Image <http://download.wandboard.org/wandboard-imx6/ubuntu-16.04/wandboard-all-ubuntu-16.04-sdcard-20160525.zip> von  <http://download.wandboard.org/wandboard-imx6/ubuntu-16.04/> herunterladen <br>
1.2 WB-Image auf MicroSD schreiben (min 8 GB)  <br>
1.3 MicroSD in WB platzieren (Slot auf Prozessor-Platine) <br>
1.4 Tastatur und Maus (USB) anschliessen <br>
1.5 WB booten <br>

##2. Per ssh (putty) auf WB einloggen <br>
2.1 Putty starten und einlogen mit <br>
 putty <b>ubuntu@wandboard</b> (user:<b>ubuntur</b> pass:<b>ubuntu</b>, rechnername <b>wandboard</b>) <br>
2.2 <b>root</b> werden mit <b>su</b> -> pass:<b>root</b><br> 
Für die Komilirung reicht <b>ubuntu</b>-user aus<br>

##3. Disk (Parttion) der SD-Karte erweitern
3.1 Partitionen ansehen mit <b>fdisk -l</b> (angezeigt wird mmcblk2p1, mmcblk2p2) <br>
3.2 Editor starten mit <b>fdisk mmcblk2</b> <br>
3.3 Partition erstellen mit "n" Enter, Enter,... (Default-Werte) <br>
3.4 Partition schreiben mit "w" -> (Meldung wegen neu booten) <br>
3.5 Neu starten mit <b>sudo reboot</b>, per ssh erneut (wie unter 2) einlogen <br>
3.6 Partitionen ansehen mit  <b>fdisk -l</b> (angezeigt wird nun mmcblk2p1, mmcblk2p2, mmcblk2p3) <br>
3.7 Auf der Partitionen Datei-System <b>ext4</b> erstellen mit: <b>mkfs.ext4 /dev/mmcblk2p3</b> <br>
3.8 Partition <b>mmcblk2p3</b> mounten unter z.B. <b>/mnt/wb:</b> <br>
 <b>mkdir /mnt/wb</b> <br>
 <b>mount /dev/mmcblk2p3 /mnt/wb</> <br>
3.9 Die Patition eventuell in <b>/etc/fstab</b> permanent eintragen: <br>
 <b>/dev/mmcblk2p3  /mnt/wb  ext4  defaults  0 1</b>

##4. Entwiklungsumgebung anpassen
Die erforderliche Anwendungen sind bereits installiert, jedoch zur Kontrolle sollten installiert oder geprüfrt werden<br>
// System auf dem neisten Stand bringen<br>
<b>sudo apt-get update -y</b>
<b>sudo apt-get upgrade -y</b>
// Git<br>
<b>sudo apt-get install git</b>
//Compiler<rb>
<b>sudo apt-get install cmake make build-essential openssl python2.7 g++ gcc</b>
//SSL, Devel-Version 
<b>sudo apt-get install libssl-dev</b>

##5. ArangoDB Clonen
<b>mkdir /mnt/wb/adb3</b> <br>
<b>cd /mnt/wb/adb3</b> <br>
<b>git clone --single-branch --depth 1 -b 3.0 git://github.com/arangodb/arangodb.git</b> <br>

##6. RocksDB anpassung
Wegen einem fest eingegebenen Schalter wird Fehler angezeigt: <br>
<b>c++: error: unrecognized command line option '-momit-leaf-frame-pointer'</b> <br>
Um das zu beheben wie folgt vorgehen: <br>
6.1 Datei öffnen: <b>/3rdParty/rocksdb/rocksdb/CMakeLists.txt</b><br>
6.2.Option finden und entfernen: <b>"-momit-leaf-frame-pointer"</b> in der Datei (kommt nur 1 mal vor)<br>
6.3 Datei speichern<br>
Der ARM-Compiler kann diese Option nicht "verdauen". <br>
Eine Lösung wäre eine Erkennunt des Schalters von Comilers einzubauen. Links dazu:<br> 
<https://github.com/facebook/rocksdb/pull/964><br>
<https://github.com/facebook/rocksdb/issues/810><br>

##7. Fehlenden Header für ARM-Platform anpassen
7.1 Datei <b>ArangoGlobalContext.h</b> öffnen mit z.B. nano-Eritor: <br>
<b>nano ./lib/Basics/ArangoGlobalContext.h</b> <br>
7.2 hinter <b>#include "Basics/Common.h" </b> hinzufügen: <br>
 #ifdef \_\_arm\_\_<br>
 #include "Basics/FileUtils.h"<br>
 #endif<br>
7.3 Änderingen speichern  <br>

##8. Angepasste Version clonen 
Der Aufwand kann erspart werden, wenn die bereits angepasste Version geklont wird:<br>
<b>git clone -b 3.0-wandboard --single-branch --depth 1 git://github.com/servusoft/arangodb3.git</b> <br>
<https://github.com/servusoft/arangodb3/tree/3.0-wandboard><br>
Die Änderungen können unter <https://github.com/servusoft/arangodb3/blob/master/adb3.diff> angesehen werden<br>

##9. cmake mit SSL Anpassung
Bei der Installation mit <b>sudo apt-get install libssl-dev</b> kann auf ARM der SSL-Pfad von Standard abweichen.<br>
In gegebenem Fall es ist "/usr/local/ssl". Dadurch sollte es cmake mitgeteilt werden:<br>
<b>cmake -DOPENSSL_ROOT_DIR=/usr/local/ssl ..</b> <br>
Hilfe und weitere Informationen unter:<br>
<https://docs.arangodb.com/3.0/cookbook/Compiling/Debian.html><br>
<http://jsteemann.github.io/blog/2016/06/02/compiling-arangodb-3-dot-0-on-ubuntu/>


##10 Kompilierung 
Mit <b>make -j4</b> (bei Wandboard Quad-Version, dauert etwa 3 Stunden)<br>
Nach dem Ablauf könnte ArangoDB in einem beliegeigen Ort getesten werden dazu werden zwei Ordner benötigt:

##11. Tests 
Zum Testen erforderlich sind zwei Ordner aus der Repository:
Das vorhandene <b>./js</b>, kopieren in z.B. <b>/home/test</b> und 
die das erstellte <b>./build/bin</b>, kopieren ebenso in <b>/home/test</b><br>
ArangoDB benötigt noch ein paar weitere Ordner. <br>
ArangoDB sollte unter benutzer <b>arangodb</b> ausgeführt werden. <br>
Dazu sind einige Anpassungen erforderlich: (Als bash-Schript oder einzeln in Test Ordner (/home/test) ausführen)<br>

Datei cho* (mit Ausführungsrechten)
adduser arangodb<br>
mkdir apps<br>
sudo chown arangodb:arangodb -R apps<br>
mkdir db<br>
sudo chown arangodb:arangodb db<br>
mkdir temp<br>
sudo chown arangodb:arangodb temp<br>
<br>

Weiterhin sollte die Datei <b>arangod.conf</b> erstellt und mit folgenden Inhalt befüllt werden:<br>

[server]<br>
authentication = false<br>
endpoint = tcp://0.0.0.0:8529<br>
[javascript]<br>
startup-directory = ./js<br>
app-path = ./apps<br>
[log]<br>
level = info<br>
[database]<br>
directory = ./db<br>
[temp]<br>
path = ./temp<br>
<br>

Gestartet wird es mit: <br>
<b>sudo -u arangodb ./bin/arangod --configuration ./arangod.conf</b><br>
Es kann ebanfalls Datei (z.B. run* mit Ausführungsrechten) erstellt werden.<br>

Der einfache Test der Version mit <b>./bin/arangod --version</b> liefert folgende Werte:<br>
3.0.1<br>
architecture: 32bit<br>
asan: false<br>
asm-crc32: false<br>
boost-version: 1.61.0b1<br>
build-date: 2016-07-06 20:37:07<br>
compiler: gcc<br>
cplusplus: 201103<br>
endianness: little<br>
fd-client-event-handler: poll<br>
fd-setsize: 1024<br>
icu-version: 54.1<br>
jemalloc: false<br>
libev-version: 4.22<br>
maintainer-mode: false<br>
openssl-version: OpenSSL 1.0.2h  3 May 2016<br>
rocksdb-version: 4.8.0<br>
server-version: 3.0.1<br>
sizeof int: 4<br>
sizeof void*: 4<br>
sse42: false<br>
tcmalloc: false<br>
v8-version: 5.0.71.39<br>
vpack-version: 0.1.30<br>
zlib-version: 1.2.8<br>

#Cross-Compiling unter Ubuntu/Debian

##1. Compiler und  utils holen<br>
<b>sudo apt-get install git</b><br>
<b>sudo apt-get install cmake make build-essential openssl python2.7 g++ gcc</b><br>
<b>sudo apt-get install libssl-dev</b><br>

##2. Cross-Compiler tools holen
<b>apt-get install arm-linux-gnueabihf*

##3. Umgebung für Cross-Comiling anpassen
<b>export MACHINE=armv7 \\</b><br>
<b>export ARCH=arm \\</b><br><br>
<b>export CROSSTOOL=arm-linux-gnueabihf</b><br>
<b>export CXX=$CROSSTOOL-g++ \\</b><br>
<b>export CC=$CROSSTOOL-gcc \\</b><br>
<b>export AR=$CROSSTOOL-ar \\</b><br>
<b>export AS=$CROSSTOOL-as \\</b><br>
<b>export RANLIB=$CROSSTOOL-ranlib</b><br>
<!--<b>export LINK=$CXX</b><br>-->

##4. // check
<b>$CC --version</b><br>
=> arm-linux-gnueabi-gcc (Ubuntu/Linaro 5.3.1-14ubuntu2) 5.3.1 20160413
<b>$CXX --version</b><br>
=> arm-linux-gnueabi-g++ (Ubuntu/Linaro 5.3.1-14ubuntu2) 5.3.1 20160413

##5. // get source (3.0)
git clone - b 3.0 --single-branch --depth 1 git://github.com/arangodb/arangodb.git</b><br>

##6. // SSL für ARM-Platform compilieren<br>
6.1 // Source holen
<b>cd ./3rdParty</b><br>
<b>mkdir openssl</b><br>
<b>cd openssl</b><br>
<b>wget https://www.openssl.org/source/openssl-1.0.2h.tar.gz</b><br>
<b>tar xzf openssl-1.0.2h.tar.gz</b><br>

6.2 // Umgebung für Cross-Comiling  anpassen<br>
s. Punkt <b>3</b><br>

6.3. // SSL für ARM configurieren<br>
<b>cd openssl-1.0.2h</b><br>
<b>./Configure linux-armv4 --openssldir=/opt/gnuarm</b><br>

6.4. // SSL Kompilieren und installieren<br>
<b>make -j4</b><br>
//SSL -> /opt/gnuarm <br>
<b>make install</b><br> 

6.5.// zurück in ArangoDB<br>
cd .. // openssl<br>
cd .. // 3rdParty<br>
cd .. // ArangoDB<br>

##7. //build<br>
<b>mkdir -p build</b><br>
<b>cd build</b><br>

<b>cmake \\</b><br>
<b>-DCROSS_COMPILING=true \\</b><br>
<b>-DOPENSSL_ROOT_DIR=/opt/gnuarm \\</b><br>
<b>-DCMAKE_TARGET_ARCHITECTURES==armv7 \\</b><br>
<b>..</b><br>


##8 Problem mit -m32 und -m64 Schalter
Wärend bei nativen Komilierung auf Wandboard tretten keine Feheler auf, <br>
schlägt die Cross-Komilirung an dem o.g. Compiler-Schlater fehl.<br>
(Für arm-linux-gnueabihf-g++ Compiler/Linker unbekannt). <br>
Das Problem tritt nur mit ICU-Modul bei Complieren der V8 auf unter:<br> 
<b>arangodb/3rdParty/V8/V8-5.0.71.39/third_party/icu</b>
Es liegt wohl daran, dass die Erkennung des Plattforms bei Cross-Comilier (noch) nicht perfekt ist.

