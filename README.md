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

##3 Disk erweitern
3.1 Partitionen ansehen mit <br>
 fdisk -l (-> mmcblk2p1, mmcblk2p2) <br>
3.2 Editor starten mit <br>
 fdisk mmcblk2  <br>
3.3 partition erstellen "n" Enter, Enter,... (Default-Werte) <br>
3.4 partition schreiben "w" -> Meldung wegen neu booten <br>
3.5 reboot, putty erneut starten und wie unter 2 einlogen <br>
3.6 Partitionen ansehen mit  <br>
<b>fdisk -l</b> (-> mmcblk2p1, mmcblk2p2, mmcblk2p3) <br>
3.7 File-System erstellen mit: <b>mkfs.ext4 /dev/mmcblk2p3</b> <br>
3.8 FileSystem mounten unter /mnt/wb: <br>
 <b>mkdir /mnt/wb</b> <br>
 <b>mount /dev/mmcblk2p3 /mnt/wb</> <br>
3.9 Permanent mounten: <br>
 Bei die Patition Bedarf in <b>/etc/fstab</b> permanent eintragen: <br>
 <b>/dev/mmcblk2p3  /mnt/wb  ext4  defaults  0 1</b>

##4. ArangoDB Clonen <br>
  <b>mkdir /mnt/wb/adb3</b> <br>
  <b>cd /mnt/wb/adb3</b> <br>
  <b>git clone --single-branch --depth 1 -b 3.0 git://github.com/arangodb/arangodb.git</b> <br>

##5 RocksDB anpassung -> auch für Cross-Compiling! <br>
  Wegen fest eingegebenen Schalter wird Fehler angezeigt: <br>
c++: error: unrecognized command line option '-momit-leaf-frame-pointer' <br>
Um das zu beheben wie folgt anpasse: <br>
5. öffnen <b>/3rdParty/rocksdb/rocksdb/CMakeLists.txt</b> <br>
5.1. Option finden und entfernen: <b>"-momit-leaf-frame-pointer"</b> in der Datei (kommt nur 1 mal vor),  <br>
5.2 CMakeLists.txt speichern  <br>
Der ARM-Compiler kann diese Option nicht "verdauen". <br>
Eine Lösung wäre eine Erkennunt des Schalters von Comilers einzubauen. Links:<br> 
<https://github.com/facebook/rocksdb/pull/964><br>
<https://github.com/facebook/rocksdb/issues/810><br>

##6. #include für __arm__ anpassen

6.1 ArangoGlobalContext.h öffnen mit z.B.  <br>
 nano lib/Basics/ArangoGlobalContext.h <br>
6.2 hinter <br>
  #include "Basics/Common.h" <br>
hinzufügen: <br>

 #ifdef __arm__ <br>
 #include "Basics/FileUtils.h" <br>
 #endif <br>
6.3 ArangoGlobalContext.h speichrn  <br>

##7 cmake mit SSL Anpassung:
cmake -DOPENSSL_ROOT_DIR=/usr/local/ssl -DCMAKE_BUILD_TYPE=Release .. <br>

##8 make -j4 <br>
