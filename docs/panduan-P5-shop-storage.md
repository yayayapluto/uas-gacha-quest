# Panduan P5 — Shop, Storage, Leaderboard

Rencana induk: `rencana-pembagian-tugas-5-orang.md`.
Panduan git dan kompilasi: `panduan-0-git-dan-kompilasi.md`.

Panduan ini berisi urutan kerja dan bukti. Format save, harga, dan batas data
ada di PRD bagian 6 dan 9; jangan mengarang format baru.

## Peran

P5 mengerjakan shop, penyimpanan progres, dan leaderboard. Semua tampilan
memakai UI lib dari P1.

## File milik P5

`src/shop/shop.c`, `src/shop/shop.h`, `src/storage/storage.c`,
`src/storage/storage.h`, `tests/t_storage.c`, `tests/t_leaderboard.c`.

## Alur kerja

### 1. Siapkan branch

Expected:

```text
$ git branch --show-current
feat/shop-storage

$ make
# tanpa warning
```

### 2. Implementasi shop

- Beli potion dan jual item sesuai PRD.
- Tolak pembelian jika gold atau stok tidak cukup.
- Tolak penjualan item terpasang.
- Konfirmasi aksi yang tidak bisa dibatalkan.

Expected:

```text
# Hub -> Shop
# beli potion -> gold berkurang, potion bertambah, stok berkurang
# gold kurang -> pesan gagal, gold tidak berubah
# jual item terpasang -> ditolak
```

### 3. Implementasi katalog dan save/load

- Muat katalog item dan monster dari `data/`.
- Implementasi 5 slot save sesuai format PRD.
- Tangani file hilang, format rusak, ID tidak dikenal, dan nilai di luar
  rentang tanpa crash.
- Simpan seed dunia dan item terpasang.

Expected:

```text
$ make test
# t_storage lulus

$ bin/game < tests/in_s10.txt
Selamat datang kembali, UjiSv!

$ bin/game < tests/in_s11.txt
Gagal memuat
```

### 4. Implementasi leaderboard

- Catat skor saat kembali ke menu utama.
- Urutkan menurun, simpan 10 teratas, tampilkan 5 teratas.

Expected:

```text
$ bin/game < tests/in_s15.txt
# skor pemain muncul di leaderboard

# leaderboard kosong
Belum ada data leaderboard.
```

### 5. Uji manual

Expected:

- Simpan ke slot kosong, timpa slot terisi dengan konfirmasi, dan buat save
  baru.
- Muat save -> nama, level, gold, item, equip, dan seed kembali.
- Save rusak -> pesan gagal, game tetap berjalan.

## Definition of Done

- `tests/t_storage.c` dan `tests/t_leaderboard.c` lulus.
- Skenario S10, S11, S15 lulus.
- `make` tanpa warning.
- Tidak ada crash saat file data hilang atau rusak.

## Yang tidak boleh

- Mengubah urutan field save.
- Menerima ID item yang tidak ada di katalog.
- Menjual item terpasang.
- Memakai `printf` untuk layar.
- Mengubah header tanpa PR ke P1.
