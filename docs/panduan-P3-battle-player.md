# Panduan P3 — Battle dan Player

Rencana induk: `rencana-pembagian-tugas-5-orang.md`.
Panduan git dan kompilasi: `panduan-0-git-dan-kompilasi.md`.

Panduan ini berisi urutan kerja dan bukti. Aturan angka dan rumus ada di PRD
bagian 6 dan `src/core/config.h`; jangan mengarang aturan baru.

## Peran

P3 mengerjakan aturan pertarungan dan statistik pemain. Semua tampilan memakai
UI lib dari P1.

## File milik P3

`src/player/player.c`, `src/player/player.h`, `src/battle/battle.c`,
`src/battle/battle.h`, `tests/t_player.c`, `tests/t_battle.c`.

## Alur kerja

### 1. Siapkan branch

Expected:

```text
$ git branch --show-current
feat/player-battle

$ make
# tanpa warning
```

### 2. Implementasi player

- Isi fungsi di `player.h` sesuai PRD.
- Pastikan nilai awal, naik level, batas level, dan statistik total benar.
- Tampilkan status lewat UI lib, bukan `printf`.

Expected:

```text
# Game Baru -> Hub -> Status
# nama, level, HP, ATK, DEF, gold, potion tampil
# EXP cukup -> level naik dan HP penuh
```

### 3. Implementasi damage dan battle

- Implementasi damage, crit, dan pilihan aksi sesuai PRD.
- Gunakan RNG platform, bukan `rand()`.
- Menang: EXP, gold, skor bertambah. Kalah: layar game over.

Expected:

```text
$ make
$ sh tests/run_tests.sh
# t_battle lulus

$ bin/game < tests/in_s03.txt
# memuat hasil kemenangan (EXP/gold/skor)

$ bin/game < tests/in_s04.txt
# memuat pesan kekalahan dan pilihan game over
```

### 4. Tangani resume dan hasil run

- Battle harus bisa dilanjutkan dari state tersimpan.
- Sediakan penanda hasil untuk hub dan dungeon: selesai, kalah, atau kembali
  ke menu.
- Layar game over harus memberi pilihan reset run atau kembali ke menu.

Expected:

```text
$ bin/game < tests/in_s04.txt
# muncul pilihan "Mulai ulang (reset run)" dan kembali ke menu

# setelah kalah, hub/dungeon tidak melanjutkan run dengan HP 0
```

### 5. Uji manual

Expected:

- Battle dari hub dan dari dungeon memberi aturan sama.
- Potion mengurangi stok dan memulihkan HP.
- Kabur berhasil mengakhiri battle; gagal berarti monster menyerang.
- Tidak ada warning saat build.

## Definition of Done

```text
$ make test
# ringkasan: 0 gagal

$ bin/game < tests/in_s03.txt
$ bin/game < tests/in_s04.txt
$ bin/game < tests/in_s18.txt
# ketiganya sesuai harapan
```

## Yang tidak boleh

- Memakai `rand()`.
- Memulihkan HP diam-diam saat kalah.
- Menulis aturan battle di `dungeon.c`.
- Memakai `printf` untuk layar.
- Mengubah header tanpa PR ke P1.
