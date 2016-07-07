#Build ArangoDB auf WandBoard
##1 WB-Image mit Ubuntu erstellen 
1.1 WB-Image herunterladen <Link> <br>
1.2 WB-Image auf MicroSD schreiben (min 8 GB)  <br>
1.3 MicroSD in WB platzieren (in der Slot auf Prozessor-Platine) <br>
1.4 Tastatur und Maus (USB) anschliessen <br>
1.5 WB booten <br>

##2 per ssh (putty) auf WB einloggen <br>
2.1 putty starten und einlogen mit <br>
 putty ubuntu@wandboard (pass:ubuntu, rechnername "wandboard") <br>
2.2 root werden mit <br>
 su -> pass:root<br>

##3 Disk erweitern
3.1 Partitionen ansehen mit <br>
 fdisk -l (-> mmcblk2p1, mmcblk2p2) <br>
3.2 Editor starten mit <br>
 fdisk mmcblk2  <br>
3.3 partition erstellen "n" Enter, Enter,... (Default-Werte) <br>
3.4 partition schreiben "w" -> Meldung wegen neu booten <br>
3.5 reboot, putty erneut starten und wie unter 2 einlogen <br>
3.6 Partitionen ansehen mit  <br>
fdisk -l (-> mmcblk2p1, mmcblk2p2, mmcblk2p3) <br>
3.7 FileSystem erstellen: <br>
 mkfs.ext4 /dev/mmcblk2p3 <br>
3.8 FileSystem mounten unter /mnt/wb: <br>
 mkdir /mnt/wb <br>
 mount /dev/mmcblk2p3 /mnt/wb <br>

##4. ArangoDB Clonen <br>
  mkdir /mnt/wb/adb3 <br>
  cd /mnt/wb/adb3 <br>
  git clone --single-branch --depth 1 -b 3.0 git://github.com/arangodb/arangodb.git <br>

##5 RocksDB anpassung -> auch für Cross-Compiling! <br>
  Wegen fest eingegebenen Schalter wird Fehler angezeigt: <br>
c++: error: unrecognized command line option '-momit-leaf-frame-pointer' <br>
Um das zu beheben wie folgt: <br>
5. öffnen /3rdParty/rocksdb/rocksdb/CMakeLists.txt <br>
5.1. finden und entfernen Option "-momit-leaf-frame-pointer" in der Datei (kommt nur 1 mal vor),  <br>
5.2 CMakeLists.txt speichern  <br>

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
