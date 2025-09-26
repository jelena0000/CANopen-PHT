# 🖥️ Master čvor

Master čvor ne zahtijeva poseban kod – koristi se postojeći Linux paket **can-utils** za nadgledanje i slanje CAN poruka.  
On služi samo za praćenje podataka koje šalje slave čvor i eventualno slanje komandnih poruka nazad.

---

## 📦 Instalacija can-utils

Na Linuxu (npr. Ubuntu ili Raspberry Pi OS), instalacija se radi pomoću:
```bash
git clone --depth=1 https://github.com/linux-can/can-utils.git

Sljedeći korak je kroskompajliranje biblioteke. Kroskompajliranje ćemo obaviti na sličan način kao što smo to radili u slučaju libmodbus biblioteke, jer se i u ovom projektu koriste alati za automatizovano kompajliranje projekata. Prvo je potrebno napraviti folder (npr. folder usr u radnom direktorijumu) u kojem će se nalaziti prekompajliranja (binarna) verzija biblioteke sa kojom će se kasnije dinamički linkovati izvršni fajl aplikacije.
```bash
mkdir usr

Nakon toga, prelazimo u folder u kojem se nalazi repozitorijum can-utils projekta i pokrećemo niz komandi za konfiguraciju build sistema i kompajliranje projekta.
```bash
cd can-utils
./autogen.sh
./configure --prefix=/path/to/usr --host=arm-linux-gnueabihf
make
sudo make install

Kao rezultat, u okviru usr foldera dobijamo binarne fajlove alata koji su sastavni dio can-utils projekta.

Kopirati candump iz foldera usr na ciljnu platformu.
Komanda za kroskompajliranje za Raspberry Pi platformu:
```bash
cd CANopenLinux
make CC="arm-linux-gnueabihf-gcc -std=gnu11"  
(Prije toga je potrebno uraditi make clean)

Sljedeći korak podrazumijeva aktiviranje CAN interefejsa. Ovo se postiže istim komandama kao kada se radi sa klasičnim mrežnim interfejsima.
```bash
sudo ip link set up can0 type can bitrate 125000  # enable interface
ip link show dev can0			          # print info
sudo ip link set can0 down                        # disable interface

K
