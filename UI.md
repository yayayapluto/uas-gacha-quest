# UI.md — Semua Layar Game Gacha Quest

Dokumen ini mencatat setiap layar/UI yang ada di game. Semua teks pesan (bahasa
Indonesia) dipertahankan persis; yang berubah hanya tata letak dan warna. Output
di blok kode adalah hasil run sungguhan dengan kode ANSI di-strip.

## Arsitektur lapisan UI

Modul di `src/ui/`, terpisah dari logika game:

| File | Tanggung jawab |
|---|---|
| `term.c/h` | Lapisan platform: raw mode, ukuran terminal, baca tombol, alternate screen, kursor, restore saat keluar. |
| `caps.c/h` | Deteksi kemampuan: mode warna, Unicode, ukuran, interaktif/TTY. |
| `style.c/h` | Token warna semantik → escape ANSI sesuai mode. |
| `render.c/h` | Frame buffer grid; semua gambar ditulis ke buffer lalu dikirim **satu write per frame**. |
| `widgets.c/h` | Panel, tabel/menu ter-highlight, bar HP/EXP, footer hint, kotak pesan/log. |
| `ui.c/h` | API publik yang dipakai layar game. |

Modul `src/platform/` tidak menggambar apa pun, tetapi memengaruhi tampilan
lewat tiga hal: pemulihan terminal saat sinyal/crash, tawaran memulihkan sesi
sebelum UI aktif, dan pesan kegagalan autosave di panel pesan.

Setiap layar di-redraw penuh: layar dibersihkan di grid, frame baru digambar,
lalu menunggu input. Tidak ada lagi output yang menumpuk ke bawah.

## Geometri dan layout

Gaya rata kiri dan padat; tidak ada pemusatan horizontal.

| Aturan | Nilai |
|---|---|
| Lebar panel | tetap 50 kolom (`UI_COLS`), rata kiri; sisa terminal dibiarkan kosong di kanan |
| Tinggi minimum | 20 baris (`UI_ROWS`); bila lebih kecil tampil "Perbesar terminal (min 50x20)" |
| Baris judul | `GACHA QUEST · <nama layar>` rata kiri (ASCII: `GACHA QUEST > <nama layar>`), lalu satu baris garis |
| Panel status pemain | nama, level, bar HP, gold, potion — di hub/battle/gacha/inventory/shop/status |
| Panel pesan sistem | 1-5 baris terakhir menggantikan printf di tengah layar |
| Footer | dicetak langsung di bawah konten, dipisah satu baris kosong: "↑↓ pilih  Enter konfirmasi  Esc kembali" |
| Jarak antar blok | maksimal 1 baris kosong; padding dalam panel 1 spasi horizontal, 0 vertikal |
| Highlight menu | hanya selebar teks item |
| Bar HP/EXP | `█` terisi, `░` kosong (ASCII: `#` dan `-`) |

## Deteksi kemampuan (deteksi, bukan tebak)

### Warna

| Kondisi | Mode |
|---|---|
| Bukan TTY, atau `NO_COLOR` ada, atau `TERM=dumb` | mono (tanpa ANSI) |
| `COLORTERM` mengandung `truecolor`/`24bit` | truecolor `\x1b[38;2;R;G;Bm` |
| `TERM` mengandung `256color` | 256 warna `\x1b[38;5;Nm` (kubus 6x6x6 + grayscale) |
| Selain itu | 16 warna ANSI (RGB → warna terdekat) |
| Windows 10+ | `ENABLE_VIRTUAL_TERMINAL_PROCESSING`; gagal → mono |

### Karakter (glyph)

| Kondisi | Hasil |
|---|---|
| locale (`LC_ALL`/`LC_CTYPE`/`LANG`) mengandung `UTF-8`/`utf8` dan `TERM` ≠ `linux`/`dumb` | Unicode: `┌ ┐ └ ┘ ─ │ ├ ┤ █ ░` |
| Selain itu | ASCII: `+ - | #` |
| Windows + konsol VT | `SetConsoleOutputCP(CP_UTF8)`, default Unicode |

Unicode hanya memakai garis tipis + blok; tanpa emoji, sudut rounded, atau
karakter lebar-ganda (CJK) agar perataan kolom tidak rusak. Semua karakter
diambil dari tabel `Glyphs` (varian unicode & ascii), tidak ada border
hardcode di layar.

### Override lewat environment

| Env | Efek |
|---|---|
| `GACHA_COLOR=mono\|16\|256\|truecolor` | paksa mode warna |
| `GACHA_ASCII=1` | paksa ASCII |
| `GACHA_UNICODE=1` | paksa Unicode |
| `GACHA_TTY=1` | hook uji: anggap stdout terminal (tanpa TTY nyata) |

### Input

- POSIX raw mode (`tcgetattr`/`tcsetattr`, ICANON & ECHO mati, VMIN=1).
  Windows: `SetConsoleMode` + `_getch` (prefix 0xE0).
- Tombol: panah atas/bawah (`ESC [ A/B`, juga `ESC O A/B`), `W`/`S`, `K`/`J`,
  Enter, angka 0-9 sebagai shortcut item menu, `Esc`/`Q` kembali.
- ESC tunggal dibedakan dari urutan panah dengan timeout singkat (poll ~40 ms).
- Menu: item terpilih di-highlight (warna latar + penanda `▸ `/`> `), wrap-around.
- Layar yang butuh teks (nama, seed, cari item, jumlah potion) sementara balik
  ke mode cooked (baris) lalu raw lagi; validasi lama dipertahankan.
- Restore terminal dijamin: `atexit()` + handler `SIGINT`/`SIGTERM`/`SIGHUP`/
  `SIGTSTP`/`SIGCONT` (POSIX). Setelah keluar/crash: kursor tampil, echo hidup,
  keluar alternate screen, warna reset.

### Mode non-interaktif (fallback)

Bila stdin atau stdout bukan TTY: tanpa raw mode, tanpa alternate screen, tanpa
clear screen, tanpa warna, ASCII, dan input baris seperti versi lama (termasuk
pesan EOF). Perilaku fallback identik dengan versi sebelumnya sehingga test lama
tetap lulus.

## Palet semantik

Token, bukan warna per layar (`style.h`). Nilai RGB awal:

| Token | Warna |
|---|---|
| accent | #5FB3FF |
| border | #4A5568 |
| text | #E2E8F0 |
| dim | #718096 |
| hp_good / success | #48BB78 |
| hp_mid | #ECC94B |
| hp_low / danger | #F56565 |
| gold | #F6C453 |
| rarity common | #A0AEC0 |
| rarity rare | #4299E1 |
| rarity epic | #ECC94B |
| rarity legendary | #D53F8C |
| selected_bg | #2D3748 (teks accent) |

Kontras tiap token terhadap latar gelap (WCAG rasio, ambang teks normal 4.5:1):

| Latar | dim | accent | text |
|---|---|---|---|
| hitam | 5.2:1 | 9.4:1 | 17.0:1 |
| #1E1E1E | 4.2:1 | 7.5:1 | 13.5:1 |
| panel #2D3748 | 3.0:1 | 5.4:1 | 9.7:1 |

Karena `dim` gagal ambang 4.5:1 di latar panel, petunjuk jeda
("Tekan Enter untuk lanjut (panah baru aktif setelah ini).") memakai `accent`,
bukan `dim`. Ini satu-satunya petunjuk bahwa tombol panah belum berfungsi
sebelum Enter ditekan, jadi tidak boleh tenggelam.

Bar HP: truecolor mewarnai tiap sel dengan gradien halus; 256/16 memakai satu
warna per ambang (hijau >60%, kuning 31-60%, merah ≤30%); mono terbaca dari
karakter `█`/`░` saja. Info penting selalu disertai teks/simbol (mis. tag
`[Epic]`), bukan warna saja, agar tetap terbaca di mode mono.

## Tangkapan layar (ASCII, ANSI-stripped)

Contoh frame showcase widget (panel, bar HP tiga ambang, menu ter-highlight),
rata kiri selebar 50 kolom:

```
GACHA QUEST > Preview
--------------------------------------------------
+Panel-------------------------------------------+
| text        title       accent      border     |
| dim         gold        danger      success    |
| common      rare        epic        legendary  |
|  #######-----    #####-------    #-----------  |
|  ########------------    ##############------  |
+------------------------------------------------+
  1. Pilihan satu
> 2. Pilihan dua
  3. Pilihan tiga

  1. Pilihan satu    2. Pilihan dua    3. Pilihan tiga

> Contoh pesan sistem.

Up/Down pilih  Enter konfirmasi  Esc kembali
```

## Kode ANSI (legacy)

Teks pesan per layar di bawah ini tetap sama persis; tata letaknya kini memakai
panel/bar berwarna di atas. Nilai ANSI lama (`ANSI_CYAN`, `ANSI_RED`, dst.)
masih tersedia di `ui.h` untuk kompatibilitas tetapi layar baru memakai token
`Style`.

---


## 1. Menu Utama

```
           GACHA QUEST
 1. Game Baru
 2. Load Game
 3. Leaderboard
 0. Keluar
Pilih:
```

Pesan sistem (jika ada) dicetak tepat di atas `Pilih:`, contoh:

```
Skor farras (0) dicatat ke leaderboard. Kembali ke menu utama.
Pilih:
```

## 2. Game Baru — input nama & seed

Interaktif: nama dan seed ditanyakan sebagai **dua langkah terpisah**, masing-masing
dengan frame input sendiri (header `GACHA QUEST · Game Baru`, panel label, prompt
`> ` berwarna accent, footer `Enter konfirmasi  Esc batal`). Prompt selalu di baris 4
dari atas, tidak tergantung ukuran terminal. Error validasi tampil di baris tepat di
bawah prompt (warna danger) lalu frame digambar ulang.

```
GACHA QUEST · Game Baru
+Input-------------------------------------------+
| Masukkan nama pemain (maks 29 karakter):       |
| >                                              |
+------------------------------------------------+

Enter konfirmasi  Esc batal
```

lalu langkah seed:

```
GACHA QUEST · Game Baru
+Input-------------------------------------------+
| Seed dunia (Enter saja untuk acak):            |
| >                                              |
+------------------------------------------------+

Enter konfirmasi  Esc batal
```

Seed dikosongkan -> `Seed acak dipilih: <angka>`. Seed diisi angka -> dipakai apa adanya.
Seed tidak valid -> `Seed harus angka bulat 0 sampai 4294967295.`

Mode non-interaktif (stdin pipe) tetap satu baris prompt polos tanpa escape code,
seperti versi lama:

```
Pilih: Masukkan nama pemain (maks 29 karakter): Seed dunia (Enter saja untuk acak): Seed acak dipilih: 1794728246
```

Pesan sambutan:

```
Selamat datang, farras! Kamu dapat 200 gold.
```

## 3. Load Game — muat progres

Game menampilkan lima slot beserta nama pemain tiap slot:

```
GACHA QUEST > Load Game
Slot 1  farras
Slot 2  (kosong)
Slot 3  budi
Slot 4  (kosong)
Slot 5  (kosong)
Kembali
Pilih baris (0 batal):
```

Berhasil:

```
Selamat datang kembali, farras!
```

Gagal:

```
Gagal memuat: save slot %d tidak ada.
Gagal memuat: format save slot %d rusak.
Gagal memuat: jumlah item tidak valid.
Gagal memuat: daftar item tidak lengkap.
Gagal memuat: item ID %d tidak ada di katalog.
Gagal memuat: nilai di luar rentang.
```

## 4. Leaderboard

```
=== Leaderboard (Top 5) ===
1. P12             level  1 skor 120
2. P11             level  1 skor 110
3. P10             level  1 skor 100
4. UjiWn           level  2 skor 92
5. P9              level  1 skor 90
```

Kosong:

```
=== Leaderboard (Top 5) ===
Belum ada data leaderboard.
```

## 5. Hub — menu utama game

```
+HUB---------------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP [##########] 100/100                        |
| Gold   : 200   Potion: 2                       |
+------------------------------------------------+
 1. Battle
 2. Gacha
 3. Inventory
 4. Shop
 5. Dungeon
 6. Status
 7. Simpan
 0. Kembali ke menu utama
Pilih:
```

Baris HP memakai bar 10 sel: `#` terisi, `-` kosong. Warna bar ikut persentase HP
(hijau > 60%, kuning 31-60%, merah <= 30%).

## 6. Battle

```
+BATTLE------------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP [##########] 100/100                        |
| Gold   : 200   Potion: 2                       |
+------------------------------------------------+
+Slime-------------------------------------------+
| HP musuh: 30                                   |
| ATK 6 | DEF 0                                  |
+------------------------------------------------+
+Log---------------------------------------------+
| Potion dipakai, HP sekarang 100/100.           |
| Goblin menyerang farras: 7 damage              |
| farras menyerang Goblin: 9 damage              |
| Goblin menyerang farras: 5 damage              |
+------------------------------------------------+
 1. Serang
 2. Bertahan
 3. Potion
 4. Kabur
Aksi:
```

Panel **Log** menampung 4 baris terakhir (log yang lebih lama dibuang). Teks
yang lebih panjang dari dalam panel dipotong di kolom `UI_COLS - 4`, jadi
tidak pernah menimpa garis tepi kanan.

### 6.1 Hasil aksi

```
farras menyerang Slime: 11 damage
Slime menyerang farras: 6 damage
```

```
Kamu bertahan.
Slime menyerang: 3 damage
Karena bertahan, damage masuk hanya 1.
```

```
Potion dipakai, HP sekarang 100/100.
Slime menyerang farras: 4 damage
```

```
Kabur berhasil! Battle selesai tanpa hadiah.
```

```
Kabur gagal!
Slime menyerang farras: 5 damage
```

Potion habis:

```
Potion habis! (giliran tidak terpakai)
```

### 6.2 Menang

```
Slime kalah! Kamu menang!
Hadiah: +10 EXP, +15 gold, +10 skor.
```

### 6.3 Kalah

```
Kamu dikalahkan oleh Slime...
```

Lalu layar game over langsung muncul tanpa perlu menekan Enter:

```
+GAME OVER---------------------------------------+
| Kamu kalah di pertempuran.                     |
| Pemain : farras (level 1, skor 0)              |
| Gold   : 107   Potion: 0   Item: 0             |
| Reset mengembalikan level, gold, dan item ke awal run.
+------------------------------------------------+
  1. Mulai ulang (reset run)
  2. Kembali ke menu utama
```

Pilihan 1 mereset level, HP, gold, potion, dan inventory ke nilai awal run
(nama dan seed tetap), lalu kembali ke hub. Pilihan 2 meninggalkan run dan
kembali ke menu utama. HP tidak pernah dipulihkan diam-diam.

Naik level (pesan di hub):

```
Naik level! Sekarang level 2 (maxHp 120, ATK 13, DEF 5).
```

## 7. Gacha

```
+GACHA-------------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP [##########] 100/100                        |
| Gold   : 200   Potion: 2                       |
+------------------------------------------------+
+PULL--------------------------------------------+
| Pity    : 0/10                                 |
| Total pull: 0                                  |
+------------------------------------------------+
+Hasil-------------------------------------------+
|   [Rare] Silver Aegis (power 12) -> baru!      |
|   [Common] Wooden Sword -> DUPLIKAT, +20 gold  |
+------------------------------------------------+
 1. Pull 1x (100 gold)
 2. Pull 10x (900 gold)
 0. Kembali
Pilih:
```

Hasil pull (1x) — tampil di panel **Hasil**:

```
  [Rare] Storm Hammer (power 22) -> baru!
```

Hasil pull 10x:

```
Hasil pull 10x:
  [Common] Scale Cuirass (power 8) -> baru!
  [Epic] Flame Blade (power 30) -> baru!
```

Duplikat (dijual jadi gold):

```
  [Common] Wooden Sword -> DUPLIKAT, jadi +20 gold
```

Gold kurang:

```
Gold kurang! Pull 1x butuh 100 gold.
Gold kurang! Pull 10x butuh 900 gold.
```

## 8. Inventory

```
+INVENTORY---------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP [##########] 100/100                        |
| Gold   : 0   Potion: 2                         |
+------------------------------------------------+
+Inventory (2/36)--------------------------------+
| Idx  Nama              Tipe Rarity    Power   |
| 1*   Storm Hammer      S    Rare      22      |
| 0#   Scale Cuirass     A    Common    8       |
|                                               |
|                                               |
|                                               |
+-----------------------------------------------+
 1. Urutkan
 2. Cari item
 3. Equip item
 0. Kembali
Pilih:
```

Tabel inventory berada di dalam panel. Penanda terpasang menempel pada kolom
`Idx`: `*` untuk senjata dan `#` untuk armor, supaya kolom tetap muat 46
karakter di dalam panel 50 kolom. Nama lebih dari 17 karakter dipotong (bukan
mendorong kolom lain).

Urutkan:

```
Urutkan berdasarkan (1 rarity, 2 power, 3 nama):
```

```
Inventory terurut.
```

Cari item:

```
Cari nama item:
```

```
Ditemukan: idx 0 Storm Hammer (tipe S, power 22)
```

```
Tidak ada item yang cocok.
```

Equip:

```
Nomor indeks item yangpasang (999 batal):
```

```
Iron Dagger dipasang.
```

```
Inventory kosong.
Indeks tidak ada di inventory.
```

## 9. Shop

```
+SHOP--------------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP [##########] 100/100                        |
| Gold   : 200   Potion: 2                       |
+------------------------------------------------+
+Produk------------------------------------------+
| Nama                    Harga       Stok       |
|> Potion               40 gold         18      |
|                                                |
|   Jual item             0 gold     banyak      |
|                                                |
| 0  Kembali                                     |
+------------------------------------------------+
+Detail------------------------------------------+
| Potion                                         |
| Memulihkan HP saat battle                      |
| Punya 2 / maks 20                              |
+------------------------------------------------+
+Info--------------------------------------------+
| Beli 2 potion. Gold: 120, Potion: 4.           |
|                                                |
+------------------------------------------------+
Pilih:
```

Shop tampil sebagai daftar produk (nama, harga, stok) dengan panel `Detail`
untuk baris yang disorot, bukan menu bernomor. Panel `Info` selalu tampil walau
pesan kosong, jadi posisi produk tidak bergeser antar aksi.

Beli potion:

```
Jumlah potion yang dibeli (1-5):
```

```
Beli 2 potion. Gold: 120, Potion: 4.
```

```
Stok potion sudah penuh (maks 20).
Gold tidak cukup untuk membeli potion.
```

Jual item:

```
Idx  Nama                 Rarity     Power Harga jual
0    Wooden Sword         Common     3     20
1    Iron Dagger          Common     5     20
Nomor indeks item yang dijual (999 batal):
```

```
Terjual: Wooden Sword +20 gold.
```

```
Inventory kosong, tidak ada yang bisa dijual.
Item sedang terpasang, tidak bisa dijual.
Indeks tidak ada di inventory.
```

## 10. Status

```
GACHA QUEST > Status
--------------------------------------------------
+Status------------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP  #################################  100/100 |
| Gold   : 200   Potion: 2                       |
+------------------------------------------------+
+STATISTIK & PERLENGKAPAN------------------------+
| Level      : 1 (EXP 0/50)                      |
|  ------------------------------------------    |
| ATK        : 10   DEF: 3                       |
| Potion     : 2   Pity: 0/10                    |
| Total pull : 0   Item: 0/36                    |
| Seed dunia : 1794728246                        |
| Senjata: -                                     |
| Armor  : -                                     |
+------------------------------------------------+
Tekan tombol apa saja untuk kembali
```

Satu panel untuk statistik dan perlengkapan, total 20 baris, supaya tidak
terpotong di terminal 24 baris. Baris senjata & armor terpasang:

```
Senjata: Storm Hammer (power 22)
Armor  : Scale Cuirass (power 8)
```

## 11. Dungeon

Intro:

```
Dunia dungeon dibangun dari seed 12345. Masukkan seed ini lagi kapan saja untuk peta yang persis sama.
Jalan ke segala arah. Kumpulkan treasure ($), lalu tekan Q kalau sudah puas.
Tekan Enter untuk masuk:
```

Peta (`P` = pemain, `$` = treasure, `G` = gold, `C` = chest, `M` = monster,
`*` = jalur hint BFS. Tembok digambar sebagai blok berwarna sesuai biome, dan
hanya sel dalam radius 6 dari pemain yang terlihat. Ukuran peta mengikuti
terminal: lebar = lebar terminal - 2, tinggi = tinggi terminal - 10, dibatasi
maksimum 25x15 dan minimum 11x7.)

```
.#####.#####.#####.#####.
.#####.#####.#####.#####.
.#####.#####.#####.#####.
.........................
.#####.#####.#####.#####.
.#####.#####.#####.#####.
.#####.#####.#####.#####.
............P............
.#####.#####.#####.#####.
.#####.#####.#####.#####.
.#####.#####.#####.#####.
.........................
.#####.#####.#####.#####.
.#####.#####.#####.#####.
.#####.#####.#####.#####.
+Status------------------------------------------+
| Posisi dunia (1,1)   langkah 0   gold 200      |
| HP 100/100                                     |
| ####################        seed 12345         |
+------------------------------------------------+
Area awal tersambung dari titik awal sampai isi terjauh (DFS).

Isi chunk (roll per 16x16, dari seed):

| Marker | Peluang | Efek |
| --- | --- | --- |
| `M` | 45% | Battle melawan monster acak katalog |
| `C` | 35% | Buka chest: 55% equipment, 25% potion, sisanya gold 40-120 |
| `G` | 55% | Gold 10-40 langsung masuk |

Equipment dari chest memakai bobot rarity yang sama dengan gacha
(Common 60 / Rare 25 / Epic 12 / Legendary 3 setelah pity tidak berlaku),
lalu memilih item katalog dengan rarity tersebut. Chest menampilkan animasi
slot horizontal di panel `Gacha`: tiap sel strip memakai warna rarity-nya
sendiri selama bergeser, lalu strip berhenti di hasil. Monster yang diinjak
langsung membuka layar Battle tanpa kembali ke frame dungeon lebih dulu.
Gerak (W/A/S/D, H hint 30 gold, Q keluar):
```

Pesan runtime:

```
Posisi (2,1), langkah 1.
Tembok! Tidak bisa lewat.
Treasure! +30 gold. Sudah 1 treasure di run ini.
Treasure terdekat 12 langkah dari sini. Sisa hint 2.
Di sekitar sini tidak ada treasure.
Hint sudah habis (maks 3 kali).
Gold kurang, hint butuh 30 gold.
Masukkan satu huruf: W/A/S/D, H, atau Q.
Gerakan tidak dikenal.
```

Keluar dungeon:

```
Keluar dungeon. Langkah total: 1.
```

## 12. Simpan & Keluar

Simpan (hub menu 7):

```
Progres tersimpan di slot %d.
```

Keluar dari hub:

```
Simpan sebelum keluar? (y/n):
```

Kembali ke menu utama (skor ikut dicatat):

```
Skor farras (0) dicatat ke leaderboard. Kembali ke menu utama.
```

Keluar dari game:

```
Terima kasih sudah bermain Gacha Quest!
```

## 12b. Startup, sesi, dan autosave

Pesan ini dicetak dengan `printf` biasa, **sebelum** `ui_init()`, sehingga
muncul di terminal yang masih mode cooked:

```
Game sudah berjalan di terminal lain.
```
Instance kedua langsung keluar dengan status 1 (tidak ada layar yang digambar).

```
Pulihkan sesi sebelumnya? (y/n)
```
Muncul hanya bila sesi sebelumnya berakhir tidak wajar (`session.lock` masih
`RUNNING`) dan `data/autosave.dat` (atau `.bak`) lolos validasi. Jawaban `y`
langsung membuka layar tempat sesi berhenti:

| State tersimpan | Layar pertama setelah `y` |
| --- | --- |
| sedang battle | Battle dengan monster dan HP tersisa yang sama |
| sedang di dungeon | Dungeon pada posisi, langkah, hint, dan peta yang sama |
| di hub/menu | Hub dengan progres terakhir |

Jawaban selain `y` (termasuk Enter/EOF) tetap ke Menu Utama seperti biasa.

Kegagalan tulis autosave tampil sekali di panel pesan layar berikutnya, tanpa
menghentikan permainan:

```
Autosave gagal: <alasan dari sistem berkas>.
```

Tidak ada indikator autosave sukses di layar; keberhasilan hanya terlihat dari
isi `data/autosave.dat`. Simpan manual (hub menu 7) tetap pesan lama
"Progres tersimpan di slot %d.".

## 13. Validasi input (semua layar)

```
Input tidak valid, masukkan angka 0 sampai 7.
Angka harus 1 sampai 4.
Input berakhir (EOF), memakai nilai 0.
Input tidak boleh kosong.
Input berakhir (EOF).
```

---

## Peta layar -> file sumber

| Layar | File |
|---|---|
| Menu utama, Game Baru, Load Game | `main.c` |
| Box/header/HP bar | `src/ui/ui.c` |
| Hub | `main.c` (`hubMenu`) |
| Battle | `src/battle/battle.c` |
| Gacha | `src/gacha/gacha.c` |
| Inventory | `src/inventory/inventory.c` |
| Shop | `src/shop/shop.c` |
| Status | `src/player/player.c` (`showStatus`) |
| Dungeon | `src/dungeon/dungeon.c` |
| Simpan/muat, Leaderboard | `src/storage/storage.c` |
| Tawaran pulihkan sesi, pesan kunci sesi | `main.c` (`main`) |
| Pesan "Autosave gagal" | `main.c` (`runHub`, dari `autosave_poll_error`) |
| Restore battle / dungeon | `battle.c` (`resumeBattle`), `dungeon.c` (`resumeDungeon`) |

---

## Keputusan desain (ambiguitas → pilihan aman)

1. **Fallback identik.** Saat non-TTY, semua layar memakai jalur lama (printf /
   input baris, ASCII, tanpa clear/alternate screen). Ini menjaga test lama dan
   pemakaian pipa tetap lulus.
2. **Windows Unicode default.** Di konsol Windows dengan VT aktif, mode
   interaktif + berwarna memaksa Unicode (`SetConsoleOutputCP(CP_UTF8)`) karena
   konsol Windows mendukung UTF-8 walau `LANG` tidak diset. `GACHA_ASCII=1`
   tetap bisa memaksa ASCII. Aturan `LANG` (C→ASCII, UTF-8→Unicode) berlaku penuh
   di POSIX dan di Windows sebelum VT aktif.
3. **Lebar panel tetap 50 kolom, rata kiri.** Lebih aman untuk perataan tabel
   (inventory/shop/leaderboard) daripada layout penuh selebar terminal; sisa
   kolom di kanan dibiarkan kosong.
4. **q/ESC pada dungeon langsung keluar** (bukan prompt), sesuai perilaku lama
   "Q keluar"; tidak menambah langkah konfirmasi.
5. **Pesan EOF non-interaktif** dipertahankan persis ("Input berakhir (EOF)...").
6. **Bar HP mono** mengandalkan `█`/`░`; ambang warna tidak dipakai agar tetap
   konsisten terbaca tanpa warna.
7. **Tanpa indikator autosave di layar.** Menulis status autosave ke panel
   setiap aksi akan berkedip dan menimpa pesan aksi; kegagalan saja yang
   dilaporkan karena itu satu-satunya hal yang butuh tindakan pemain.
8. **Pemulihan sesi memakai `printf` biasa.** Ditanyakan sebelum UI aktif agar
   pemain bisa menjawab walaupun terminal belum siap untuk raw mode; sesudah
   itu layar game mengambil alih seperti biasa.
