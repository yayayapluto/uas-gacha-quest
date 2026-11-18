# Panduan P4 — Gacha dan Inventory

Rencana induk: `rencana-pembagian-tugas-5-orang.md`.
Panduan git dan kompilasi: `panduan-0-git-dan-kompilasi.md`.

Panduan ini berisi urutan kerja dan bukti. Bobot rarity, harga, dan pity ada
di PRD bagian 6 dan `src/core/config.h`; jangan menebak.

## Peran

P4 mengerjakan gacha dan inventory. Semua tampilan memakai UI lib dari P1.

## File milik P4

`src/gacha/gacha.c`, `src/gacha/gacha.h`, `src/inventory/inventory.c`,
`src/inventory/inventory.h`, `tests/t_gacha.c`, `tests/t_inventory.c`.

## Alur kerja

### 1. Siapkan branch

Expected:

```text
$ git branch --show-current
feat/gacha-inventory

$ make
# tanpa warning
```

### 2. Implementasi gacha

- Isi fungsi sesuai header dan PRD.
- Tangani 1x, 10x, pity, duplikat, dan inventory penuh.
- Tampilkan hasil lewat UI lib.

Expected:

```text
# Hub -> Gacha
# 1x dan 10x bisa dipilih
# gold kurang -> pull ditolak, gold tidak berubah
# item duplikat -> gold bertambah
# inventory penuh -> tidak menagih pull yang gagal
```

### 3. Implementasi inventory

- Tampilkan daftar item, urutkan, cari, dan equip.
- Daftar panjang harus bisa di-scroll.
- Tandai item terpasang; item terpasang tidak bisa dijual.

Expected:

```text
# Hub -> Inventory
# urut rarity/power/nama berubah sesuai pilihan
# cari nama sebagian, tidak peka huruf besar/kecil
# equip senjata/armor mengganti slot yang sama
```

### 4. Uji

Expected:

```text
$ make test
# ringkasan: 0 gagal

$ bin/game < tests/in_s05.txt
Gold kurang! Butuh 900 gold

$ bin/game < tests/in_s08.txt
# sort dan search sesuai harapan

$ bin/game < tests/in_s09.txt
Terjual:
```

### 5. Cek integrasi dungeon

- Pastikan fungsi gacha yang dipakai dungeon tetap berjalan.
- Jangan mengubah signature tanpa memberi tahu P2.

Expected:

```text
# chest di dungeon membuka panel Gacha
# hasil loot masuk inventory/gold satu kali
```

## Definition of Done

- `tests/t_gacha.c` dan `tests/t_inventory.c` lulus.
- Skenario S05-S09 lulus.
- `make` tanpa warning.
- Gacha dan inventory berjalan di terminal dan mode pipa.

## Yang tidak boleh

- Memakai `rand()`.
- Menyimpan item duplikat.
- Menagih pull yang gagal karena inventory penuh.
- Mengubah teks rarity atau nama item.
- Memakai `printf` untuk layar.
