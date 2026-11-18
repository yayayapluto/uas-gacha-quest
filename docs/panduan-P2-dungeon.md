# Panduan P2 — Dungeon

Rencana induk: `rencana-pembagian-tugas-5-orang.md`.
Panduan git dan kompilasi: `panduan-0-git-dan-kompilasi.md`.

Panduan ini hanya berisi urutan kerja dan bukti. Spesifikasi fitur ada di
`docs/Projek UAS Daspro Gacha Quest ....md` bagian 7 dan di repo referensi.
Jangan menyalin implementasi referensi; jadikan perilakunya sebagai pembanding.

## Peran

P2 mengerjakan seluruh Dungeon Explorer. P2 tidak mengedit UI lib, `main.c`,
atau modul lain.

## File milik P2

`src/dungeon/dungeon.c`, `src/dungeon/dungeon.h`, `tests/t_dungeon.c`.

## Alur kerja

### 1. Siapkan branch

- Ambil branch dari `contract-v1`.
- Pastikan build awal sukses sebelum mengubah apa pun.

Expected:

```text
$ git branch --show-current
feat/dungeon

$ make
# tanpa warning
```

### 2. Pelajari kontrak

- Baca `src/dungeon/dungeon.h` sampai paham fungsi yang harus ada.
- Baca test `tests/t_dungeon.c`; test ini adalah target, bukan contoh kode.
- Baca bagian Dungeon di `README.md` untuk perilaku pemain.

Expected:

```text
$ grep -n "^[a-z].*(" src/dungeon/dungeon.h
# daftar fungsi publik dungeon terlihat
```

### 3. Implementasi dunia dan gerak

- Kerjakan hash sel, chunk, cache area, dan varian terrain.
- Tambahkan gerak pemain, tabrakan tembok, hitung langkah, dan kamera.
- Jangan memakai malloc dan jangan memakai rekursi.

Expected:

```text
$ make
# tanpa warning

$ bin/game
# Game Baru -> Hub -> Dungeon
# pemain bisa bergerak WASD/panah; menabrak tembok tidak berpindah
```

### 4. Implementasi isi dunia

- Tambahkan treasure, hint, dan isi chunk sesuai spesifikasi.
- Pastikan hasil dunia sama untuk seed yang sama.
- Pastikan hint memotong gold dan dibatasi sesuai aturan.

Expected:

```text
# di dungeon:
# - treasure menambah gold
# - H menandai jalur dan memotong 30 gold
# - gold kurang -> hint ditolak
# - keluar lalu masuk dengan seed sama -> peta sama
```

### 5. Implementasi BFS dan DFS

- BFS dipakai untuk hint; DFS dipakai untuk validasi area awal.
- Keduanya memakai array, bukan rekursi.

Expected:

```text
$ sh tests/run_tests.sh
# driver S12/S14 dungeon lulus
# BFS/DFS lolos pemeriksaan test
```

### 6. Integrasi battle dan chest

- Saat pemain menyentuh monster, tampilkan panel battle.
- Saat chest dibuka, tampilkan panel gacha.
- Jangan menulis ulang aturan battle/gacha; panggil modul P3/P4.
- Input gerak diabaikan selama panel aktif.

Expected:

```text
# menyentuh monster -> panel Battle muncul
# menang -> panel hilang, gold/EXP bertambah
# kalah -> layar game over
# chest -> panel Gacha, hasil masuk inventory/gold satu kali
```

### 7. Resume dan pengujian

- Pastikan state dungeon bisa disimpan dan dilanjutkan.
- Jalankan driver dan skenario pipa.

Expected:

```text
$ make test
# ringkasan: 0 gagal

$ bin/game < tests/in_s12.txt
Tembok! Tidak bisa lewat.

$ bin/game < tests/in_s13.txt
Gold kurang, hint butuh 30 gold

$ bin/game < tests/in_s16.txt
# memuat seed yang sama setelah save/load
```

## Definition of Done

- `tests/t_dungeon.c` lulus.
- `in_s12`, `in_s13`, `in_s16`, `in_demoA` lulus.
- Tidak ada warning.
- Dungeon bisa dimainkan di terminal dan mode pipa.
- Tidak ada input terbaca selama animasi chest.

## Yang tidak boleh

- Menambah fungsi baru di luar header tanpa persetujuan P1.
- Mengedit `ui.c`, `main.c`, `battle.c`, atau `gacha.c`.
- Memakai `printf` untuk layar.
- Memakai `rand()`; gunakan RNG platform.
- Menyimpan peta penuh atau memakai malloc.
