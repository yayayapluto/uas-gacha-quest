# Panduan 0 — Git dan Kompilasi

Berlaku untuk semua anggota. Rencana induk:
`rencana-pembagian-tugas-5-orang.md`.

## Prasyarat Windows

- Git for Windows.
- MSYS2 dengan `gcc`, `make`, `sh`, `rm`, `grep`, `cp`.
- Tambahkan `C:\msys64\usr\bin` ke `PATH`.
- Untuk `make debug`, gunakan clang64:
  `C:\msys64\clang64\bin\clang.exe`.
- Jalankan build dari root repo, bukan dari subfolder `src/`.

Cek sekali:

```sh
git --version
gcc --version
make --version
sh --version
```

Expected: keempat perintah mencetak versi, bukan `command not found`.

## Setup repo baru

1. Buat folder kerja.
2. Inisialisasi git.
3. Buat branch utama.
4. Buat branch kerja.

```sh
mkdir Projek-UAS-DasPro-GachaQuest
cd Projek-UAS-DasPro-GachaQuest
git init
git branch -M main
git checkout -b feat/scaffold
```

Expected:

```text
Initialized empty Git repository in .../Projek-UAS-DasPro-GachaQuest/.git/
```

Cek branch:

```sh
git branch --show-current
```

Expected:

```text
feat/scaffold
```

## Identitas commit

Sekali per komputer:

```sh
git config --global user.name "Nama Kamu"
git config --global user.email "email@contoh.com"
```

Cek:

```sh
git config --global --list
```

Expected: `user.name` dan `user.email` muncul.

## Alur harian

Kerjakan dari branch sendiri, bukan `main`.

```sh
git checkout main
git pull
git checkout feat/nama-modul
git rebase main
```

Expected: tidak ada konflik. Jika ada konflik, selesaikan dulu sebelum
mengubah kode.

Sebelum commit:

```sh
git status
git diff
```

Expected: hanya file milikmu yang berubah.

Commit:

```sh
git add path/file-yang-berubah
git commit -m "feat(dungeon): tambah gerak dan kamera"
```

Expected:

```text
[feat/dungeon 1a2b3c4] feat(dungeon): tambah gerak dan kamera
 3 files changed, ...
```

Push:

```sh
git push -u origin feat/nama-modul
```

Expected: branch baru muncul di remote.

## Aturan branch

| Branch | Pemilik | Isi |
| --- | --- | --- |
| `main` | P1 | hasil stabil, hanya menerima merge |
| `feat/scaffold` | P1 | repo, Makefile, kontrak |
| `feat/ui` | P1 | UI lib dan preview |
| `feat/platform` | P1 | util dan platform |
| `feat/main-integration` | P1 | main dan hub |
| `feat/dungeon` | P2 | dungeon |
| `feat/player-battle` | P3 | player dan battle |
| `feat/gacha-inventory` | P4 | gacha dan inventory |
| `feat/shop-storage` | P5 | shop dan storage |

Branch dibuat dari tag `contract-v1`, kecuali branch integrasi P1.

```sh
git checkout -b feat/dungeon contract-v1
```

Expected:

```text
Switched to a new branch 'feat/dungeon'
```

## Urutan merge

1. `feat/scaffold` → `main`, tag `contract-v1`.
2. `feat/ui` → `main`, tag `ui-ready`.
3. `feat/player-battle` → `dev`.
4. `feat/gacha-inventory` → `dev`.
5. `feat/shop-storage` → `dev`.
6. `feat/dungeon` → `dev`.
7. `feat/main-integration` → `dev`.
8. `dev` diuji penuh, lalu merge ke `main`.

## Konflik

Jika `git rebase` berhenti:

```sh
git status
```

Expected: daftar file konflik.

Aturan:

- File milikmu: perbaiki sendiri.
- File milik orang lain: jangan menimpa. Hubungi pemiliknya.
- Header bersama: P1 yang memutuskan.
- Setelah selesai:

```sh
git add file-yang-sudah-diperbaiki
git rebase --continue
```

Jika salah dan ingin membatalkan rebase:

```sh
git rebase --abort
```

Expected: kembali ke kondisi sebelum rebase.

## Review

Sebelum minta review:

```sh
make
make test
```

Expected: build tanpa warning, test `0 gagal`.

Yang dicek reviewer:

- Hanya file milik pemohon yang berubah.
- Tidak ada `printf` layar atau akses terminal langsung di modul pemula.
- Tidak ada `rand()` di luar modul RNG.
- Test tidak dilemahkan.
- Teks pemain tidak berubah.

## Tag

Tag hanya dibuat P1 setelah gate lulus.

```sh
git tag contract-v1
git tag ui-ready
git tag
```

Expected:

```text
contract-v1
ui-ready
```

## Kompilasi

Semua perintah dari root repo.

### Build

```sh
make
```

Expected:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic ... -o bin/game ...
# tidak ada warning
```

Jika ada warning, jangan lanjut. Perbaiki dulu.

### Jalankan

```sh
make start
```

Expected: game langsung berjalan dari `bin/game`.

Atau:

```sh
bin/game
```

### Test

```sh
make test
```

Expected:

```text
=== Ringkasan: ... lulus, 0 gagal ===
```

Jika ada `GAGAL`, baca nama driver dan file input yang disebut.

### Debug sanitizer

```sh
make debug
```

Expected: build dan seluruh test berjalan dengan AddressSanitizer/UBSan,
tanpa laporan memory error.

Di Windows, target ini memakai clang64. Jika gagal, cek
`C:\msys64\clang64\bin\clang.exe` ada.

### Preview UI

```sh
make preview
```

Expected: `bin/ui_preview` tampil. Tekan Esc atau ikuti footer untuk keluar.

### Bersihkan

```sh
make clean
```

Expected: folder `bin/` hilang. Build berikutnya dari nol.

## Troubleshooting

| Gejala | Cek |
| --- | --- |
| `make: command not found` | `C:\msys64\usr\bin` belum masuk `PATH` |
| `sh: command not found` | Paket MSYS2 belum lengkap; buka MSYS2 shell |
| `gcc: command not found` | `gcc` belum terpasang di MSYS2 |
| Warning `-Wpedantic` | Jangan abaikan; perbaiki kode |
| `Permission denied` saat `make clean` | Game masih berjalan; tutup dulu |
| Input aneh di mintty | Jalankan dari Windows Terminal, PowerShell, atau CMD |
| Data tidak ditemukan | Jalankan dari root repo, bukan dari `bin/` |

## Aturan keselamatan git

Jangan jalankan tanpa tahu akibatnya:

```text
git reset --hard
git clean -fd
git push --force
```

Jika ragu, tanya P1. Simpan pekerjaan dengan commit kecil sebelum mencoba
perintah berisiko.
