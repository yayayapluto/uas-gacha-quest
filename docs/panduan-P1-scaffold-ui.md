# Panduan P1 — Scaffold, UI Lib, Platform, Integrator

Rencana induk: `rencana-pembagian-tugas-5-orang.md`.
Panduan git dan kompilasi: `panduan-0-git-dan-kompilasi.md`.

Panduan ini berisi urutan kerja dan bukti yang harus terlihat. Bukan contoh
kode dan bukan solusi. Kontrak fungsi ada di header referensi; perilaku yang
diharapkan ada di `UI.md`, `README.md`, dan test.

## Peran

P1 membangun fondasi sebelum tiga pemula mulai. Target P1 bukan UI sempurna,
tetapi UI yang bisa dipakai dan tidak berubah sesudah diberi tag.

## File milik P1

`Makefile`, `.gitignore`, `main.c`, `src/core/config.h`, `src/util/*`,
`src/ui/*`, `src/platform/*`, `tools/ui_preview.c`, `tests/run_tests.sh`,
`tests/parity.sh`, `README.md`, `UI.md`, `DECISIONS.md`.

## Alur kerja

### 1. Siapkan repo dan branch

- Buat folder `D:/Kuliah/dasar_pemrograman/Projek-UAS-DasPro-GachaQuest`.
- Inisialisasi git, buat branch `feat/scaffold`.
- Salin header, data katalog, dan folder `tests/` dari repo referensi.
- Jangan mengubah isi file yang disalin.

Expected:

```text
$ git branch --show-current
feat/scaffold

$ ls src/core/config.h src/ui/ui.h data/item.txt tests/run_tests.sh
src/core/config.h  src/ui/ui.h  data/item.txt  tests/run_tests.sh
```

### 2. Buat scaffold yang bisa dikompilasi

- Tulis `Makefile` dengan target `all`, `start`, `test`, `debug`, `preview`,
  `clean`.
- Buat stub semua modul agar linker tidak gagal.
- Belum perlu fitur berjalan.

Expected:

```text
$ make
gcc ... -o bin/game ...
# selesai tanpa warning

$ ls bin/game
bin/game
```

### 3. Bekukan kontrak

- Tulis `CONTRACTS.md` berisi pemilik file, aturan branch, dan larangan edit
  lintas file.
- Commit scaffold, lalu buat tag `contract-v1`.
- Umumkan tag ke P2-P5.

Expected:

```text
$ git tag
contract-v1
```

### 4. Publikasikan UI minimum

- Implementasi fungsi UI yang dipakai layar: frame, panel, menu, footer,
  pesan, status pemain, bar HP, log, input baris, dan pemilih baris.
- Buat `tools/ui_preview.c` sebagai bukti UI bisa dipakai tanpa game.
- Tulis `UI-LIB.md` berisi contoh pemakaian dan larangan akses terminal
  langsung.
- Setelah selesai, beri tag `ui-ready` dan beri tahu P2-P5.

Expected:

```text
$ make preview
# jendela/terminal menampilkan panel, menu ter-highlight, bar HP, log

$ git tag
contract-v1
ui-ready
```

### 5. Lengkapi lapisan terminal

- Tangani input panah/WASD/Enter/Esc.
- Tangani resize, mode warna, Unicode/ASCII, dan mode pipa.
- Pastikan terminal pulih setelah keluar normal dan Ctrl+C.

Expected:

```text
$ bin/game
# menu bisa digerakkan panah; Esc kembali; tidak ada teks menumpuk

# perkecil terminal di bawah 50x20
Perbesar terminal (min 50x20)

# Ctrl+C
# prompt kembali normal, kursor terlihat
```

### 6. Selesaikan util dan platform

- Implementasi util dan seluruh modul platform sesuai header referensi.
- Pastikan tidak ada `rand()` di luar modul RNG.
- Pastikan file runtime ditulis di `data/`.

Expected:

```text
$ make
# tanpa warning

$ bin/game
# berjalan; data/session.lock berisi RUNNING

# keluar normal
$ cat data/session.lock
CLEAN
```

### 7. Integrasi main dan hub

- Tulis `main.c`: menu utama, hub, Game Baru, Load Game, Leaderboard, Simpan,
  dan pemulihan sesi.
- Sambungkan modul lewat fungsi publiknya, bukan menyalin logika modul.
- Jangan mengubah file modul milik orang lain tanpa PR pemiliknya.

Expected:

```text
$ make
$ bin/game
# Menu Utama -> Game Baru -> nama + seed -> Hub
# Hub -> Battle/Gacha/Inventory/Shop/Dungeon/Status/Simpan/Kembali
# Load Game menampilkan 5 slot
```

### 8. Paritas

- Tulis `tests/parity.sh` untuk membandingkan output biner baru dengan
  referensi memakai input pipa.
- Perbaiki setiap selisih.

Expected:

```text
$ sh tests/parity.sh "D:/Kuliah/dasar_pemrograman/projek uas"
# tidak ada diff
# exit code 0
```

## Definition of Done

```text
$ make clean && make
# tanpa warning

$ make preview
# preview tampil

$ make test
# ringkasan: 0 gagal

$ sh tests/parity.sh ...
# exit 0
```

Manual:

- Ctrl+C memulihkan terminal.
- Instance kedua ditolak dengan pesan `Game sudah berjalan di terminal lain.`
- Resize tidak merusak layout.
- Pipeline input tetap berjalan tanpa warna dan tanpa clear screen.

## Aturan review

- Setiap PR P1 harus lulus `make` dan driver terkait.
- Perubahan header bersama harus diumumkan ke P2-P5 sebelum merge.
- P1 tidak menimpa file modul untuk mempercepat integrasi. Minta pemilik
  memperbaiki, atau buka PR kecil dengan persetujuannya.
