DEPRICATED: TO BUILD FOR ARM PLEASE USE <br>
https://github.com/arangodb-helper/build-docker-containers/blob/master/readme_ARM.md


# Inhalt
1. Einrichtung eines Cross-Compilers
2. Vorbereitung der Umgebungsvariablen
3. Kompilirtung der SSL
4. Kompilirtung der ArangoDB
5. Vorbereitung zum Testen auf einem ARM-Board

## 1. Einrichtung eines Cross-Compilers
Es gibt zwei Möglichkeiten einen Cross-Compiler zu verwenden
- als eine Installation in Linux-System (Debian oder Ubuntu)
- ohne Installation, nur das entpackte in einem Vertzeichnis (binäre) Version<br>

Zur Verwendung als <b>installation</b> sind folgende Befehle (als root in Terminal) erforderlich:<br>
<b>apt-get install g++-arm-linux-gnueabihf</b> - für ARMv7 (32-bit) Archritekturen<br>
<b>apt-get install g++-aarch64-linux-gnu</b> - für ARMv8 (64-bit) Archritekturen<br><br>
<b>Ohne Installation</b> sollten folgende Pakete heruntergeladen werden:<br>
Für ARMv7 wird <b>arm-linux-gnueabihf</b> Compiler verwendet, der unter dem Link heruntergeladen werden kann:<br>
https://releases.linaro.org/components/toolchain/binaries/latest-5/arm-linux-gnueabihf/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf.tar.xz<br>
Für ARMv8 ist <b>aarch64-linux-gnu</b> erforderlich: <br>
https://releases.linaro.org/components/toolchain/binaries/latest-5/aarch64-linux-gnu/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu.tar.xz<br>
Das Herunterladen kann mit <b>wget (Dateiname)</b> erfolgen. Die heruntergeladen Dateien sollten in ein Verzeichnis abgelegt werden, wo die entpackt und dann verwenden werden können. 
Für die Einrichtung des Compiler sind absolute Pfade erforderlich, so nehmen wir an, dass die Dateien unter<br> 
<b>/mnt/sda4/_LINARAO</b> gespeichert werden. Die gepackte Dateien können mit <b>tar xpvf (Dateiname)</b> entpackt werden.

##2. Vorbereitung der Umgebungsvariablen
Für die Kompilirung sind mehreren Utiliten erforderlich, die für eine bestimte Archritekrut abgestimmt sind. 
Die namen werden so gesetzt, damit die Einrichtung des Umgebungsvariablen vereinfacht wird. 
So wird eine Basis-Variable definiert, die für weitere Definitionen verwendet wird.<br>
Einrichtung der Basis-Variable für folgende Architerktugen erfolgt so:<br>
Für die installierte ARMv7 Version es ist:<br>
<b>export TOOL_PREFIX=arm-linux-gnueabihf</b><br>
Für die entpackte ARMv7 Variante:<br>
<b>export TOOL_PREFIX=/mnt/sda4/_LINARO/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf</b><br>
Entsprechend installierte ARMv8 Version:<br>
<b>export TOOL_PREFIX=aarch64-linux-gnu</b><br>
Und die entpackte ARMv8 Variante:<br>
<b>export TOOL_PREFIX=/mnt/sda4/_LINARO/gcc-linaro-5.3-2016.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu</b><br>
Weitere Definitionen sind:<br>
<b>export CXX=$TOOL_PREFIX-g++</b><br>
<b>export AR=$TOOL_PREFIX-ar</b><br>
<b>export RANLIB=$TOOL_PREFIX-ranlib</b><br>
<b>export CC=$TOOL_PREFIX-gcc</b><br>
<b>export LD=$TOOL_PREFIX-g++</b><br>
<b>export LINK=$TOOL_PREFIX-g++</b><br>
<b>export STRIP=$TOOL_PREFIX-strip</b><br>
##3. Kompilirtung der SSL
ArangoDB verwendet OpenSSL, was aber nicht im Quellcode enhalten ist. 
Dafür müsste SSL Packen separat heruntergeladen und installiert werden.
Es ist erfordelich die Installation einmal pro Architektur zu machen (mit installirten oder enpackten Version des Compilers).
Für die Kompilierung sollten die Umgebungsvariablen gesetzt werden. Pro Architerktur sollte ein separate Verzeichnis verwerden werden.<br>
Die Schritte für ARMv7 sehen dann so aus (Unterstellt in /mnt/sda4/_SSL gespeichert):<br>
<b>mkdir /mnt//sda4/_SSL/openssl-armv7</b><br>
<b>cd /mnt//sda4/_SSL/openssl-armv7</b><br>
<b>wget https://www.openssl.org/source/openssl-1.0.2h.tar.gz</b><br>
<b>tar xzf openssl-1.0.2h.tar.gz</b><br>
Für ARMv8 wird es in <b>/mnt//sda4/_SSL/openssl-armv8</b> abgelegt.<br>
Es kann auch die aktuelle GIT-Version geklont werden. Das Klonen und die Kompilirung für jeweilige Versionen:<br>
ARMv7:<br>
<b>git clone --single-branch --depth 1 -b OpenSSL_1_0_2-stable  git://github.com/openssl/openssl ./openssl/arm-hf</b><br>
<b>cd ./openssl/arm-hf</b><br>
<b>./Configure linux-armv4 --openssldir=/opt/gnuarm-hf</b><br>
<b>make -j4</b><br>
<b>make install</b><br>
ARMv8:<br>
<b>git clone --single-branch --depth 1 -b OpenSSL_1_0_2-stable  git://github.com/openssl/openssl ./openssl/arm-64</b><br>
<b>cd ./openssl/arm-64</b><br>
<b>./Configure linux-aarch64 --openssldir=/opt/gnuarm-64</b><br>
<b>make -j4</b><br>
<b>make install</b><br>
Die enpackte Versionen könne genau so kompiliert werden<br>
##4. Kompilirtung der ArangoDB
Zunächst sollte ArangoDB mit GIT geklont werden: 
<b>git clone git://github.com/arangodb/arangodb.git</b> - der aktuellen Devel Branch<br>
<b>git clone -b 3.0 git://github.com/arangodb/arangodb.git</b> - der aktuellen 3.0 Release Branch<br>
Die Kompilierung der ArangoDB unterscheidet sich nicht wesentlich, bis auf die Anpassung der CMake -Varioblan, die pro Architektur unterschiendlich sind.<br>
Die Schritte sind:<br>
<b>mkdir -p A32</b> - für ARMv7 Architerktur</b><br>
<b>cd A32</b><br>
<b>cmake .. -DCROSS_COMPILING=true -DOPENSSL_ROOT_DIR=/opt/gnuarm-hf</b><br>
<b>mkdir -p A64</b> - für ARMv8 Architerktur<br>
<b>cd A64</b><br>
<b>cmake .. -DCROSS_COMPILING=true -DOPENSSL_ROOT_DIR=/opt/gnuarm-64</b><br>
Anschliessend wird <b>make -j4</b> ausgeführt. (4=Anzahl der verfügbaren Kernen bei für die Kompilierung)<br>
Als Ergebnis der Kompilirung stehen die Dateien in dem ./Axx/bin/ Verzeichnis parat.
##5. Vorbereitung zum Testen auf einem ARM-Board
Damit das ArangoDB auf der jeweiligen Architektur getestet werden kann, sollte das Ergebnis in ein Packet gepackt werden.<br> 
Hier ist eine Besonderheit erforderlich: CPack verwendet die lokale strip-Variante, die mit Binären Dateien für ARM nicht zu recht kommt. 
Deswegen sollten die Dateien nach der Kompilirung mit <b>$STRIP ./bin/*</b> (strip-Variante der Architektur) behandelt werden. 
Anschliessend kann mit <b>cpack -G STGZ</b> eine ausführbare Datei erstellt werden, die auf einem ARM-Board entpackt und getestet werden kann.








