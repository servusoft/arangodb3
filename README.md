#Build ArangoDB auf Wandboard
Wandboard kann über <http://wandboard.org/index.php/buy> bestellt werden.<br>
Der direkte Link ist z.B.:<br>
Mouser : <http://www.mouser.de/ProductDetail/Wandboard/WBQUAD/><br>
Texim <https://www.texim-europe.com/wandboard_order.aspx><br>
##1. WandBoard-Image mit Ubuntu erstellen 
1.1 Ubuntu-Image <http://download.wandboard.org/wandboard-imx6/ubuntu-16.04/wandboard-all-ubuntu-16.04-sdcard-20160525.zip> von  <http://download.wandboard.org/> herunterladen <br>
1.2 WB-Image auf MicroSD schreiben (min 8 GB)  <br>
1.3 MicroSD in WB platzieren (Slot auf Prozessor-Platine) <br>
1.4 Monitor (HDMI), Tastatur und Maus (USB) anschliessen <br>
1.5 Wandboard booten <br>
Linx liefert Werkzeuge, die Erstellen einer SD-Karte ermöglicht.<br> 
Unter Windows könnte folgende Anwendungen verwendet werden:<br>
<b>SDFormatter.exe</b> - SD Karte formatieren: <https://www.sdcard.org/downloads/><br>
<b>Win32DiskImager.exe</b> - SD Karte beschreiben: <http://sourceforge.net/projects/win32diskimager/><br>


##2. Arbeit an dem Board<vr>
Es direkt an dem Board per LXTerminal gearbeitet werden, jedoch von  Vorteil ist die Arbeit mit einem SSH Client (z.B.putty)<br>
2.1 Putty starten und einlogen mit <br>
 [putty] <b>ubuntu@wandboard</b> (User:<b>ubuntur</b> Password:<b>ubuntu</b>, Rechnername <b>wandboard</b>) <br>
2.2 <b>root</b> werden mit <b>su</b> -> Password:<b>root</b><br> 
Für die Kompilirung reicht <b>ubuntu</b>-user aus<br>

##3. Disk (Parttion) der SD-Karte erweitern
Die erstellte SD Karte wird nicht 100% verwendet. Damit der freie Platz auch verwendet werden kann, sind einie Anpassungen weforderlich.<br>
3.1 Partitionen ansehen mit <b>fdisk -l</b> (angezeigt wird mmcblk2p1, mmcblk2p2) <br>
3.2 Editor starten mit <b>fdisk mmcblk2</b> <br>
3.3 Partition erstellen mit "n" Enter, Enter,... (Default-Werte) <br>
3.4 Partition schreiben mit "w" -> (Meldung wegen neu booten) <br>
3.5 Neu starten mit <b>sudo reboot</b>, per ssh erneut (wie unter 2) einlogen <br>
3.6 Partitionen ansehen mit  <b>fdisk -l</b> (angezeigt wird nun mmcblk2p1, mmcblk2p2, mmcblk2p3) <br>
3.7 Auf der Partitionen Datei-System <b>ext4</b> erstellen mit: <b>mkfs.ext4 /dev/mmcblk2p3</b> <br>
3.8 Partition <b>mmcblk2p3</b> mounten unter z.B. <b>/mnt/wb:</b> <br>
 <b>mkdir /mnt/wb</b> <br>
 <b>mount /dev/mmcblk2p3 /mnt/wb</b> <br>
3.9 Die Patition eventuell in <b>/etc/fstab</b> permanent eintragen: <br>
 <b>/dev/mmcblk2p3  /mnt/wb  ext4  defaults  0 1</b>

##4. Entwiklungsumgebung anpassen
Die erforderliche Anwendungen sind bereits installiert, jedoch zur Kontrolle sollten installiert oder geprüfrt werden<br>
// System auf dem neusten Stand bringen<br>
<b>sudo apt-get update -y</b><br>
<b>sudo apt-get upgrade -y</b><br>
// Git<br>
<b>sudo apt-get install git</b><br>
//Compiler<br>
<b>sudo apt-get install cmake make build-essential openssl python2.7 g++ gcc</b><br>
//SSL, Devel-Version <br>
<b>sudo apt-get install libssl-dev</b><br>

##5. ArangoDB Clonen
<b>mkdir /mnt/wb/adb3</b> <br>
<b>cd /mnt/wb/adb3</b> <br>
<b>git clone --single-branch --depth 1 git://github.com/arangodb/arangodb.git</b> <br>

##6. cmake mit SSL Anpassung
Bei der Installation mit <b>sudo apt-get install libssl-dev</b> kann auf ARM der SSL-Pfad von Standard abweichen.<br>
In gegebenem Fall es ist "/usr/local/ssl". Dadurch sollte es cmake mitgeteilt werden:<br>
<b>cmake -DOPENSSL_ROOT_DIR=/usr/local/ssl ..</b> <br>
Hilfe und weitere Informationen unter:<br>
<https://docs.arangodb.com/3.0/cookbook/Compiling/Debian.html><br>
<http://jsteemann.github.io/blog/2016/06/02/compiling-arangodb-3-dot-0-on-ubuntu/><br>
Mit <b>cmake -L</b> können alle zusätzliche Parameter angezeigt werden, die mit cmake verwendet werden können<br>


##7 Kompilierung 
Mit <b>make -j4</b> (bei Wandboard Quad-Version, dauert etwa 3 Stunden)<br>
Die Komilierung kann ein wenig optimiert weren. Wenn Der Compiler legt die Dateien in einem temporären Ordner. Standartmäßig es ist <b>/tmp/</b>. Wenn der Temp-Ordner ein RAM-Dsik ist, wird die SD-Karte nicht beschrieben, sondern werden Datein in RAM-Speicher abgelegt. Erreicht wird es mit dem Eintrag in <b>/etc/fstab </b>:<br>
<b>tmpfs /var/tmp tmpfs  defaults 0 0</b><br>
Danach es ist erforderlich die TMPDIR Variafle anzupassen:<br>
<b>export TMPDIR=/var/tmp</b><br>
Sollte ein Problem wegen Speichrmangael (z.B. bei Testen von ArangoDB) auftreten, so kann der Temp-Ordner auf Standart-Wert (/tmp/) angepasst werden;<br>
<b>export TMPDIR=/tmp</b><br><br>
Damit "/var/tmp/" tatsächlich ein RAM-Disk wird, muss <b>[sudo] mount -a </b> ausgeführt werden. 
Auch nach dem Neustart werden Änderungen wirksam.<br>
Geprüft kann es mit <b>df -lH</b> wo <b>/var/tmp</b> als <b>tmpfs</b> erkennbar ist:<br>
tmpfs           1.1G     0  1.1G   0% /var/tmp<br>
Standardmäßig werden bis zu 50% des RAM-Speichers als RAM-Disk verwendet.<br> 
Angepasst werden kann es in <b>/etc/fstab</b> mit:<br>
<b>tmpfs			/var/tmp tmpfs	nosuid,size=33%	0	0	</b><br>
wo #33% des RAMs	verwendet wird.<br>
In dem Fall die Last auf der Kühler wird höher, da auch RAM-Chips gekühlt werden müssen, was mit <b>make -j4</b>  zu einer Überhitzung führen kann. In dem Fall wird das System gestoppt und ein Reset ist erforderlich. Um die Last zu reduziern könnte ein-zwei Kerne  weniger verwendet werden was mit <b>make -j3</b> oder <b>make -j2</b> erreicht wird.<br>
Bei Bedarf kann der RAM-Disk mit <b>umount /var/tmp</b> deaktiviert werden, was nach <b>df -lH</b> erkennbar ist, wo <b>/var/tmp</b> nicht mehr als <b>tmpfs</b> erscheint.<br><br>
Mit <b>make help</b> könne alle Ziele (Targets) aufgelistet werden. Ein paar sinnvolle davon sind:<br> 
<b>make package</b> - erstellt ein Package mit Binary-Daten<br>
<b>make package_source</b> - erstellt ein Package mit Quellcode, was z.B. bei Git hochgeladen werden kann.<br>


##8. Tests 

8.1 Der einfache Test der Version mit <b>./bin/arangod --version</b><br>
8.2 Test mit <b>unittests</b><br>
Nach der erfolgreichen Kompilierung mit <b>make -j4</b> kann ArangoDB mit Hilfe eines integrierten Test-Fameworks getestet werden.
Dieses kann mit <b>./scripts/unittest all</b> aufgerufen werden.<br>
Aufruf <b>./scripts/unittest</b> ohne Parameter listet alle verfügbate Tests auf.<br>
Die Tests können auch einzeln aufgerufen werden. Beispiele:<br>
<b>./scripts/unittest arangobench</b><br>  
<b>./scripts/unittest stress_crud</b><br>
<b>./scripts/unittest http_server</b><br>
Das Ergebnis kann in eine Datei umgeleitet werden:<br>
Dieses kann mit <b>./scripts/unittest all</b> aufgerufen werden.<br>
<b>./scripts/unittest ssl_server 1>&2>unittest.out</b><br>  

Für http-Tests sind noch weitere Komponenten erfordserlich, die wie folgt installiert werden können:<br>
<b>apt-get install ruby ruby-rspec ruby-httparty -y</b> - installiert Ruby<br>
<b>apt-get install bundler -y</b> - Ruby Dependency Management<br>
<b>gem install persistent_httparty</b> - installiert <b>persistent_httparty</b> für Ruby<br>

8.3 Zum Starten des ArangoDB Servers in einem beliebigen Ort (Ordener) sind einige Ordner aus der Source-Repository erforderlich:<br>
- das vorhandene <b>./js</b>, kopieren in z.B. <b>/home/test/js</b> und<br> 
- das erstellte Ordner <b>./build/bin</b>, kopieren in <b>/home/test/bin</b><br>
- auch der Ordner mit Config-Dateien sollte nicht fehlen:<b>./etc</b>, kopieren ebenso in <b>/home/test/etc</b><br>
- ArangoDB benötigt noch ein paar weitere Ordner. <br>
Dazu sind einige Anpassungen erforderlich: (Als bash-Schript oder einzeln in Test Ordner (/home/test) ausführen)<br><br>

Datei cho* (mit Ausführungsrechten)<br>
adduser arangodb<br>
mkdir apps<br>
sudo chown arangodb:arangodb -R apps<br>
mkdir db<br>
sudo chown arangodb:arangodb db<br>
mkdir temp<br>
sudo chown arangodb:arangodb temp<br>
<br><br>

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
path = ./temp<br><br>

Gestartet wird es mit: <br>
<b>sudo -u arangodb ./bin/arangod --configuration ./arangod.conf</b> oder <br>
<b>sudo -u arangodb ./bin/arangod -c ./etc/arangod.conf</b> (Standard Einstallungen) <br>
Es kann ebanfalls Datei (z.B. run* mit Ausführungsrechten) erstellt werden.<br>

ArangoDB sollte unter dem Benutzer <b>arangodb</b> ausgeführt werden. <br>
Der ausgelagerte <b>apps</b> ist von Vorteil, so dass <b>arangodb</b>Benutzer schreiben kann.<br>
Der Orner <b>./js</b> kann schreibgechützt bleiben, was für eine bessere Sicherheit des Systems sorgen kann.<br>

#Cross-Compiling unter Ubuntu/Debian
Weitere Infos unter: https://github.com/servusoft/arangodb3/blob/master/README_CROSS_COMPILING.md
