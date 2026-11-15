# UI.md — Semua Layar Game Gacha Quest

Dokumen ini mencatat setiap layar/UI yang ada di game. Semua output diambil dari
hasil run game sungguhan (ANSI di-strip, jadi warna tidak terlihat di blok kode).

## Aturan tampilan

| Aturan | Nilai | Sumber |
|---|---|---|
| Lebar box | 50 karakter | `UI_WIDTH` di `src/ui/ui.h:6` |
| Bentuk box | `+` di corners, `-` di garis atas/bawah, `\|` di sisi | `src/ui/ui.c` (`draw_box_top`, `draw_box_bottom`, `draw_box_line`) |
| Padding baris | `\| ` + isi + spasi + ` \|` | `draw_box_line` |
| Panjang baris dihitung tanpa kode ANSI | `visible_len()` | `src/ui/ui.c:64` |
| Judul box | Tempel di garis atas, sisanya `-` | `draw_box_top` |

### Warna (ANSI)

| Kode | Warna | Dipakai untuk |
|---|---|---|
| `ANSI_CYAN` | cyan | Judul box, pesan sistem, border |
| `ANSI_GREEN` | hijau | HP > 60%, pesan menang |
| `ANSI_YELLOW` | kuning | HP 31-60%, angka gold, rarity Epic |
| `ANSI_RED` | merah | HP <= 30%, pesan kalah |
| `ANSI_MAGENTA` | magenta | rarity Legendary |
| `ANSI_DIM` | redup | teks petunjuk, intro dungeon |

---

## 1. Menu Utama

```
           GACHA QUEST
 1. Game Baru
 2. Lanjutkan
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

```
Pilih: Masukkan nama pemain: Seed dunia (Enter saja untuk acak): Seed acak dipilih: 1794728246
```

Seed dikosongkan -> `Seed acak dipilih: <angka>`. Seed diisi angka -> dipakai apa adanya.
Seed tidak valid -> `Seed harus angka bulat 0 sampai 4294967295.`

Pesan sambutan:

```
Selamat datang, farras! Kamu dapat 200 gold.
```

## 3. Lanjutkan — muat progres

Berhasil:

```
Selamat datang kembali, farras!
```

Gagal:

```
Gagal memuat: data/save.txt tidak ada.
Gagal memuat: format data/save.txt rusak.
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

 1. Serang
 2. Bertahan
 3. Potion
 4. Kabur
Aksi:
```

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
Gold tinggal 107, HP dipulihkan penuh. Kembali ke hub.
```

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
 1. Pull 1x (100 gold)
 2. Pull 10x (900 gold)
 0. Kembali
Pilih:
```

Hasil pull (1x):

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
Inventory (2/36)
Idx  Nama                 Tipe Rarity     Power Terpasang
0    Storm Hammer         S    Rare       22
1    Scale Cuirass        A    Common     8
 1. Urutkan
 2. Cari item
 3. Equip item
 0. Kembali
Pilih:
```

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
 1. Beli potion (40 gold)
 2. Jual item
 0. Kembali
Pilih:
```

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
+STATUS------------------------------------------+
| Pemain : farras                                |
| Level  : 1 (Skor 0)                            |
| HP [##########] 100/100                        |
| Gold   : 200   Potion: 2                       |
+------------------------------------------------+
+DETAIL------------------------------------------+
| Level      : 1 (EXP 0/50)                      |
| ATK        : 10                                |
| DEF        : 3                                 |
| Potion     : 2                                 |
| Pity       : 0/10                              |
| Total pull : 0                                 |
| Seed dunia : 1794728246                        |
| Item       : 0/36                              |
+------------------------------------------------+
```

Baris senjata & armor terpasang (di bawah box):

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

Peta (25x15, `P` = pemain, `$` = treasure, `*` = jalur hint BFS, `#` = tembok):

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
Posisi dunia (1,1) | langkah 0 | HP 100/100 | gold 200 | seed 12345
Area awal tersambung dari titik awal sampai ujung koridor (DFS).
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
Progres tersimpan di data/save.txt.
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
| Menu utama, Game Baru, Lanjutkan | `main.c` |
| Box/header/HP bar | `src/ui/ui.c` |
| Hub | `main.c` (`hubMenu`) |
| Battle | `src/battle/battle.c` |
| Gacha | `src/gacha/gacha.c` |
| Inventory | `src/inventory/inventory.c` |
| Shop | `src/shop/shop.c` |
| Status | `src/player/player.c` (`showStatus`) |
| Dungeon | `src/dungeon/dungeon.c` |
| Simpan/muat, Leaderboard | `src/storage/storage.c` |
