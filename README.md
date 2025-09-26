# 📡 PHT Sensor – CANopen Projekat

## 📌 Opis projekta
Ovaj projekat demonstrira kako se podaci sa **PHT senzora** (pritisak, vlažnost, temperatura) mogu prenositi preko **I²C magistrale** na **Raspberry Pi** (slave čvor), a zatim putem **CANopen mreže** do **master čvora**.  
Master čvor zatim prima vrijednosti i prikazuje ih na terminalu pomoću `candump` alata.

---

## 🛠 Korišćene tehnologije
- **PHT senzor** (temperature, humidity, pressure)- https://www.mikroe.com/pht-click?srsltid=AfmBOorAwQCGiEvDxiJhZ9U22w2EniEKsk12RDy7NyGN2Wl6Xgmj-3L3  
- **Raspberry Pi** – slave čvor, I²C komunikacija  
- **CANopenLinux** – [CANopenNode/CANopenLinux](https://canopennode.github.io/CANopenLinux/index.html)  
- **CAN-utils (candump)** – alat za prikaz CAN poruka [CAN-utils](https://github.com/guticdejan/ikm-prj.git)
- **CANopen konfigurator (EDS Editor)** – [CANopenEditor](https://github.com/CANopenNode/CANopenEditor)
-  **Dodatni linkovi** - https://github.com/guticdejan/ikm-prj.git

---

## 🔗 Arhitektura sistema
1. **PHT senzor** šalje podatke preko I²C.  
2. **Raspberry Pi (slave čvor)** prikuplja podatke i šalje ih putem CANopen mreže.  
3. **Master čvor** koristi `candump` za prijem i prikazivanje vrijednosti.  

```
[PHT sensor] --I²C--> [RPi Slave] --CANopen--> [Master] --> [Terminal Output]
```

![PHT Sensor](https://cdn1-shop.mikroe.com/img/product/pht-click/pht-click-thickbox_default-12x.jpg)
![Raspberry Pi](https://upload.wikimedia.org/wikipedia/commons/f/f1/Raspberry_Pi_4_Model_B_-_Side.jpg)

---

## 🔌 Hardversko povezivanje

Za uspješno funkcionisanje sistema potrebno je pravilno povezati **PHT senzor** i **CAN ekspanzionu pločicu** na Raspberry Pi.

### 1. PHT senzor (I²C komunikacija)
PHT senzor se povezuje na RPi preko I²C linija:

- **SDA** → Pin 3 (GPIO2)  
- **SCL** → Pin 5 (GPIO3)  
- **VCC** → 3.3V ili 5V (u našem slučaju 3.3V)  
- **GND** → GND (Pin 6 ili drugi dostupni GND)  

### 2. CAN ekspanziona pločica
Ekspanziona pločica se postavlja na GPIO header Raspberry Pi-ja kao HAT i obezbjeđuje CAN interfejs.  
Na pločici se nalaze šrafni konektori za povezivanje CAN linija:

- **CANH** → High linija CAN mreže (CANH -> CANH) 
- **CANL** → Low linija CAN mreže  (CANL -> CANL)

📌 Napomena: Na oba kraja CAN mreže mora postojati **120Ω terminacioni otpornik**. Na pločici postoji jumper označen kao `TERM` koji se može postaviti za aktiviranje terminacije.

Primjer CAN ekspanzione pločice:

![can_hat](https://github.com/user-attachments/assets/24296192-6db7-4866-bf98-e482010d19b5)

---

## ⚙️ Instalacija i podešavanje

### 1. Raspberry Pi (Slave čvor)
- Instalacija CANopenLinux:
  ```bash
  git clone https://github.com/CANopenNode/CANopenLinux.git
  cd CANopenLinux
  make
  sudo make install
  ```
- Omogućiti I²C na RPi:
  ```bash
  sudo raspi-config
  ```
  → Interfacing Options → I2C → Enable.

- Proveriti da li senzor radi:
  ```bash
  i2cdetect -y 1
  ```

- Pokretanje slave čvora:
  ```bash
  ./canopend can0 -i 2
  ```

Primjer izlaza:
```
CANopenNode running...
Prosjecna temperatura (3 sec) = 26.04 C
Prosjecna temperatura (3 sec) = 26.15 C
Prosjecna temperatura (3 sec) = 22.83 C
...
```

### 2. Master čvor
- Instalirati **can-utils** paket:
  ```bash
  sudo apt-get install can-utils
  ```

- Pokretanje master čvora (slušanje CAN poruka):
  ```bash
  candump -td -a can0
  ```

Primjer izlaza:
```
(000.023657) can0 182 [4] 1A 00 00 00
(000.447498) can0 701 [1] 05
(000.550439) can0 182 [4] 1B 00 00 00
...
```

---


## Konfiguracija mreže - podešavanje konfiguratora 
--------------------------------------------------------------

Prema specifikacijama projekta potrebno je kroz CANopen mrežu prenositi podatke o temperaturi, pritisku i vlažnosti vazduha sa PHT senzora. Svaka od vrijednosti treba da bude predstavljena određenom varijablom koju će slave čvor slati, a master čvor primati i obrađivati. Varijable se u CANopen mreži dodaju izmjenama u tzv. _Object Dictionary_ (_Rječnik podataka_). 

Da bi se rječnik podataka (skraćeno **OD**) mijenjao, potrebno je pokrenuti konfigurator jer se OD ne bi smio mijenjati ručno (prema uputstvu sa _GitHub_ repozitorijuma _CANopen_-a koji je dat u opisu laboratorijske vježbe).
Da bi se izvršile potrebne promjene, potrebno je pokrenuti [CANopen konfigurator](https://github.com/CANopenNode/CANopenEditor). 

Prvi korak jeste klonirati repozitorijum i analizirati _README_ fajl, kao i strukturu repozitorijuma.
```bash
git clone https://github.com/CANopenNode/CANopenEditor
cd CANopenEditor
```
* U zavisnosti od korištenog sistema sadržaj foldera može se izlistati pomoću

Windows:
```win
DIR
``` 
Linux:
```bash
ls -la
```
U sadržaju foldera može se primijetiti datoteka _EDSEditor.sln_. Kako bismo pokrenuli ovaj fajl i samim tim konfigurator, koristićemo **Visual Studio 2022** pokrenut na _Windows 11_ OS-u. Pomenuta datotetka predstavlja fajl koji sadrži sve projekte i fajlove koji čine konfigurator. 

> [!VAŽNO]
> Visual Studio 2022 mora imati instalirane sve zavisnosti potrebne za bildanje i pokretanje potrebnih fajlova i projekata koji čine konfigurator. <br>
To su:
1. **Visual Studio 2022**
   - Preuzeti sa [Microsoft stranice](https://visualstudio.microsoft.com/).
   - Tokom instalacije uključiti workload:
     - `.NET desktop development`
2. **.NET Framework**
   - Potreban je **.NET Framework 4.7.2** ili noviji.
   - Moguće je provjeriti u projektu koji je tačno potreban: *Project Properties → Target Framework*.
   - Ako nije instaliran, preuzeti i instalirati sa _Microsoft_ stranice ili kroz _Visual Studio Installer_.
3. **NuGet paketi**
   - Visual Studio automatski povlači potrebne pakete.
   - Ako nešto nedostaje:
     - Solution Explorer → desni klik na Solution → **Restore NuGet Packages**.
4. **Windows Forms (WinForms)**
   - Dio je `.NET desktop development` workloada.
   - Koristi se za _GUI_ editora.
  
Nakon uspješnog pokretanja _.sln_ fajla, potrebno je jedan od editora postaviti kao _startup project_ na način da se klikne desnim klikom na fajl _EDSEditorGUI_ koji se nalazi u folder strukturi, a potom čekira opcija _Set as Startup Project_. 

Nakon toga, moguće je pokrenuti GUI koji služi za lakše kretanje kroz konfigurator. <br>
![izgled-konfiguratora](https://github.com/jelena0000/CANopen-PHT/blob/main/images/izgled_konfiguratora.png) <br>
_Izgled početnog ekrana konfiguratora_

Da bi se uređivao OD mreže, potrebno je učitati _DS301_profile.xpd_ fajl, a koji se nalazi u folderu CANopenLinux → CANopenNode → example. To je moguće uraditi klikom na karticu _File_, potom izborom opcije _Open_ i navigiranjem do željenog fajla. Sada će konfigurator imati izgled kao na slici ispod.

![izgled-konfigurator](https://github.com/jelena0000/CANopen-PHT/blob/main/images/izgled_konf_2.png) <br>
_Izgled konfiguratora sa učitanim .xpd fajlom_

Sada je moguće dodavati objekte, pregledati postojeće i konfigurisati ostale parametre u mreži. <br> U našem slučaju dodavaćemo objekat koji će predstavljati podatak o temperaturi vazduha koju mjeri _PHT_ senzor.

#### Postupak dodavanja novog objekta u OD

Klikom na _Index_ ili _Name_ na kartici _Manufacturer Specific Parameters_ moguće je dodati objekat na željenom indeksu (paziti da li je indeks u dozvoljenom opsegu) i željene strukture (_VAR_, _RECORD_ ili _ARRAY_), te dati ime objektu. U našem slučaju promjenljiva će se zvati _temperature_ i biće obična promjenljiva - VAR.<br>
Kada se objekat doda, moguće je podesiti dodatne parametre za taj objekat, kao što je prikazano na slici ispod.
![temperature-var](https://github.com/jelena0000/CANopen-PHT/blob/main/images/temperature_var.png)<br>
_Dodavanje i konfigurisanje novog objekta_ <br>
Potrebno je parametre definisati u skladu sa upotrebom objekta, pa je u našem slučaju za objekat kojim se predstavlja temperatura
* Data type - _unsigned int 32_ jer senzor čita 4B podatak o temperaturi
* Access SDO - pristup SDO mapiranju sa dozvolom za čitanje i upis
* Access PDO - pristup PDO mapiranju sa dozvolom za čitanje i upis (moguće je za neki čvor da to bude samo upis ili samo čitanje, zavisi od namjene tj. čvora gdje se dodaje objekat)
* Default value - inicijalna vrijednost varijable <br>

Na _slave_ čvoru je potrebno mapirati ovaj objekat u _TPDO_ (za prenos), a na _master_ čvoru u _RPDO_ (za čitanje). To se može uraditi na karticama _TX PDO Mapping_ i _RX PDO Mapping_, redom. <br>
Mapiranje se može izvršiti na za to predviđenim indeksima, tako što se željeni objekat prevuče na poziciju na odabranom indeksu. Osim toga, moguće je podesiti parametre mapiranja (_COB_, tip prenosa, tajmer i slično). 

![TPDO](https://github.com/jelena0000/CANopen-PHT/blob/main/images/TPDO.png)
_TPDO mapiranje objekta na slave čvoru_ <br>

U konkretnom slučaju _COB_ dodjeljujemo broj 182 (0x180 + Node ID). Tip prenosa je _event-driven_ i šaljemo podatke svakih 1000 ms, tako da u odgovarajuća polja upisujemo vrijednosti 0xFF (255) i 1000, kao što je pokazano na slici iznad.

Na _master_ čvoru potrebno je u skladu sa podešavanjima na _slave_ čvoru dodati objekat koji će ovaj čvor čitati i obrađivati. Objekat dodajemo na isti način, pazeći da se tip podataka i ostali parametri poklapaju. Jedina razlika se pravi u mapiranju objekta jer je sada potrebno da se taj podatak čita - _RPDO_ mapiranje. 

![RPDO](https://github.com/jelena0000/CANopen-PHT/blob/main/images/RPDO.png)
_RPDO mapiranje objekta na master čvoru_ <br>

Objekat mora imati isti _COB_ kako bi postojala veza između mapiranih objekata i kako bi razmjena podataka mogla biti uspješna. 

Sve promjene je potrebno sačuvati klikom na _Save_ i _Save Changes_, a potom generisati nove fajlove (_OD.h_, _OD.c_, _.eds_ i sl). To je moguće uraditi klikom na karticu _File_, a potom odabirom opcija _Export CanOpenNode_ i klikom na _Save Object_. <br>
Potrebno je voditi računa o ekstenzijama, a koje moraju biti iste onima koje pripadaju originalnim fajlovima (_.eds_ i _.xpd_).

---
## ✏️ Izmena koda u CANopenLinux projektu

Da bi slave čvor mogao da šalje podatke sa PHT senzora, potrebno je izmijeniti fajl:  

**`CANopenLinux/CO_main_basic.c`**  

U ovaj fajl se dodaje:  
- Inicijalizacija senzora (I²C komunikacija)  
- Logika za čitanje podataka (temperatura, vlažnost, pritisak)  
- Upis vrijednosti u **Object Dictionary (OD)**, koje se dalje šalju preko CANopen mreže  

👉 **`CO_main_basic.c` je centralni fajl u kojem se nalazi glavna logika slave čvora** i on je zadužen za pokretanje `canopend` procesa.  

Tipična mjesta izmjene:  
- Funkcija za inicijalizaciju → dodati inicijalizaciju PHT senzora  
- Glavna petlja → dodati periodično čitanje senzora i upis u odgovarajuće OD varijable  

---
## 🛠 Rješavanje problema (troubleshooting):

Ako sistem ne radi očekivano, provjeri sljedeće:  

### 1. I²C problemi
- `i2cdetect -y 1` ne vidi senzor → provjeriti:  
  - Da li je I²C omogućen (`sudo raspi-config`).  
  - Ispravnost **SDA/SCL** veza.  
  - Da li je senzor napajan na 3.3V   

### 2. CAN mreža ne šalje/primа poruke
- Provjeriti da li je interfejs podignut:  
  ```bash
  sudo ip link set can0 up type can bitrate 125000
  ifconfig can0

### 3. Slave ne šalje PDO
- Provjeriti da li je objekat pravilno dodat u Object Dictionary (OD) i mapiran u TPDO.
- Provjeriti Node ID (isti COB-ID mora biti na masteru i slave-u).

---

## 🖥 Demonstracija rada
Na slici ispod vidi se rad slave i master strane:  
- **Lijevo:** slave čvor (RPi) šalje prosječne vrijednosti temperature na 3 sekunde.  
- **Desno:** master čvor prima CANopen poruke pomoću `candump`.

![demo](https://github.com/jelena0000/CANopen-PHT/blob/main/images/demo.png)

---

## 🚀 Kako pokrenuti cijeli sistem

Prvo je potrebno preuzeti repozitorij i pripremiti Raspberry Pi za rad *slave* i *master* čvora.
### 1️⃣ Preuzimanje repozitorija
Na svom računaru:
```bash
git clone https://github.com/jelena0000/CANopen-PHT.git
cd CANopen-PHT
```

---

### 2️⃣ Kros-kompajliranje slave aplikacije
Na lokalnom računaru potrebno je kros-kompajlirati aplikaciju za Raspberry Pi:

```bash
cd code/slave/CANopenLinux
make CC="arm-linux-gnueabihf-gcc -std=gnu11"
```

➡️ Ovo će generisati izvršni fajl **canopend** spreman za Raspberry Pi.

---

### 3️⃣ Priprema master čvora
Na Raspberry Pi (ili drugom Linux računaru koji će biti **master**):

```bash
sudo apt-get update
sudo apt-get install can-utils
```

---

### 4️⃣ Prebacivanje fajlova na Raspberry Pi
Prebaci kros-kompajlirani slave program i potrebne biblioteke na Raspberry Pi (npr. pomoću `scp`):

```bash
scp ./canopend pi@<RPi-IP>:/home/pi/CANopen-PHT/code/slave/
```

---

### 5️⃣ Pokretanje sistema

🔹 Na **Raspberry Pi (slave)**:
```bash
cd /home/pi/CANopen-PHT/code/slave
./canopend can0 -i 2
```

🔹 Na **master strani** (drugi RPi ili Linux računar):  
Pokrenuti CAN interfejs i koristiti `can-utils` za testiranje i prijem podataka.

Primjer:
```bash
./candump -td -a can0
```
--- 

```
Slave → šalje podatke
Master → prikazuje podatke
```
📌 Napomena: očitavanje vrijednosti urađeno je samo za temperaturu, potrebno je dodati za pritisak i vlažnost
