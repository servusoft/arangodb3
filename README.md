#Build ArangoDB auf WandBoard
##1 WandBoard-Image mit Ubuntu erstellen 
1.1 Ubuntu-Image <http://download.wandboard.org/wandboard-imx6/ubuntu-16.04/wandboard-all-ubuntu-16.04-sdcard-20160525.zip> von  <http://download.wandboard.org/wandboard-imx6/ubuntu-16.04/> herunterladen <br>
1.2 WB-Image auf MicroSD schreiben (min 8 GB)  <br>
1.3 MicroSD in WB platzieren (Slot auf Prozessor-Platine) <br>
1.4 Tastatur und Maus (USB) anschliessen <br>
1.5 WB booten <br>

##2 Per ssh (putty) auf WB einloggen <br>
2.1 Putty starten und einlogen mit <br>
 putty <b>ubuntu@wandboard</b> (user:<b>ubuntur</b> pass:<b>ubuntu</b>, rechnername <b>wandboard</b>) <br>
2.2 <b>root</b> werden mit <b>su</b> -> pass:<b>root</b><br> 
Für die Komilirung reicht <b>ubuntu</b>-user aus<br>

##3 Disk (Parttion) der SD-Karte erweitern
3.1 Partitionen ansehen mit <b>fdisk -l</b> (angezeigt wird mmcblk2p1, mmcblk2p2) <br>
3.2 Editor starten mit <b>fdisk mmcblk2</b> <br>
3.3 Partition erstellen mit "n" Enter, Enter,... (Default-Werte) <br>
3.4 Partition schreiben mit "w" -> (Meldung wegen neu booten) <br>
3.5 Neu starten mit <b>sudo reboot</b>, pe putty erneut wie unter (2) einlogen <br>
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

##6 RocksDB anpassung
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

##7.Fehlenden Header für ARM-Platform anpassen
7.1 Datei <b>ArangoGlobalContext.h</b> öffnen mit z.B. nano-Eritor: <br>
<b>nano ./lib/Basics/ArangoGlobalContext.h</b> <br>
7.2 hinter <b>#include "Basics/Common.h" </b> hinzufügen: <br>
<code>
 #ifdef __arm__<br>
 #include "Basics/FileUtils.h"<br>
 #endif<br>
</code>
6.3 Änderingen speichrn  <br>

##8 Angepasste Version clonen 
Der Aufwand kann erspart werden, wenn die bereits angepasste Version mit geklont wird:<br>
<b>git clone -b 3.0-wandboard --single-branch --depth 1 git://github.com/servusoft/arangodb3.git</b> <br>
<https://github.com/servusoft/arangodb3/tree/3.0-wandboard>

##8 cmake mit SSL Anpassung
Bei der Installation mit <b>sudo apt-get install libssl-dev</b> kann auf ARM der SSL-Pfad von Standard abweichen.
In gegeben Fall es ist "/usr/local/ssl". Dadurch sollte es cmake mitgeteilt werden:<br>
<b>cmake -DOPENSSL_ROOT_DIR=/usr/local/ssl ..</b> <br>
Hilfe und weitere Informationen unter:<br>
<https://docs.arangodb.com/3.0/cookbook/Compiling/Debian.html><br>
<http://jsteemann.github.io/blog/2016/06/02/compiling-arangodb-3-dot-0-on-ubuntu/>


##10 Kompilierung 
Mit <b>make -j4</b> (bei Wandboard Quad-Version, dauert etwa 3 Stunden)<br>
Nach dem Ablauf könnte ArangoDB in einem beliegeigen Ort getesten werden dazu werden zwei Ordner benötigt:

##11 Test 
Zum Testen erforderlich sind zwei Ordner aus der Repository:
Das vorhandene <b>./js</b>, kopieren in z.B. <b>/home/test</b> und 
die das erstellte <b>./build/bin</b>, kopieren such in <b>/home/test</b><br>
ArangoDB benötigt noch eiun paar Ordner, ebenso sollte ArangoDB unter <b>arangodb</b> benutzer ausgeführt werden. <br>
Dazu sind einige Anpassungen erforderlich: (Als bash-Schript oder einzeln in Test Ordner (/home/test) ausführen)
<code>
adduser arangodb<br>
mkdir apps<br>
sudo chown arangodb:arangodb -R apps<br>
mkdir db<br>
sudo chown arangodb:arangodb db<br>
mkdir temp<br>
sudo chown arangodb:arangodb temp<br>
</code><br>

Weiterhin sollte eine die Datei <b>arangod.conf</b> erstellt werden und mit folgenden Inhalt befüllt werden:<br>
<code>
[server]<br>
authentication = false<br>
endpoint = tcp://0.0.0.0:8300<br>
[javascript]<br>
startup-directory = ./js<br>
app-path = ./apps<br>
[log]<br>
level = info<br>
[database]<br>
directory = ./db<br>
[temp]<br>
path = ./temp<br>
#Optional<br>
</code><br>

Gestartet wird es mit (kann ebanfalls als Schript mit Ausführungsrechten aerstellt werden)<br>
<b>sudo -u arangodb ./bin/arangod --configuration ./arangod.conf</b>

Der einfache Test der Version mit <b>./bin/arangod --version</b> liefert folgende Werte:<br>
<code>
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


##Cross-Compiling <br>
TODO
