# Gacha Quest 5 Orang — Starter Kit Panduan

## Context

Buat paket kerja berisi panduan, bukan implementasi. Tujuannya agar lima orang
bisa mengerjakan project dari nol sampai setara repo referensi
`D:/Kuliah/dasar_pemrograman/projek uas`. Starter kit tidak berisi `.c`, tidak
berisi solusi, dan tidak menjalankan build. Yang dibagikan hanya rencana,
panduan per peran, aturan git, dan cara kompilasi.

Lokasi starter kit: `D:/Kuliah/dasar_pemrograman/Projek-UAS-DasPro-GachaQuest/`.
Repo referensi hanya dibaca; jangan diubah.

## Isi Starter Kit

```text
Projek-UAS-DasPro-GachaQuest/
├── README.md
├── .gitignore
└── docs/
    ├── rencana-pembagian-tugas-5-orang.md
    ├── panduan-5-orang.md
    ├── panduan-0-git-dan-kompilasi.md
    ├── panduan-P1-scaffold-ui.md
    ├── panduan-P2-dungeon.md
    ├── panduan-P3-battle-player.md
    ├── panduan-P4-gacha-inventory.md
    └── panduan-P5-shop-storage.md
```

Tidak ada file `.c`, `.h`, `Makefile`, data, atau test di starter kit. Semua itu
disalin peserta dari repo referensi mengikuti panduan P1. Starter kit hanya
mengatur siapa mengerjakan apa, urutan kerja, dan bukti yang harus muncul.

## Pembagian Peran

| Orang | Peran | Modul yang dikerjakan |
| --- | --- | --- |
| P1 | Scaffold, UI lib, platform, integrator | Makefile, `src/ui`, `src/platform`, `src/util`, `main.c` |
| P2 | Dungeon | `src/dungeon` |
| P3 | Battle dan player | `src/player`, `src/battle` |
| P4 | Gacha dan inventory | `src/gacha`, `src/inventory` |
| P5 | Shop, storage, leaderboard | `src/shop`, `src/storage` |

P3, P4, dan P5 hanya memakai UI lib dari P1. Mereka tidak menyentuh terminal,
ANSI, atau `printf` layar.

## Fase Penyusunan Starter Kit

### Fase 0 — Siapkan folder

1. Buat `D:/Kuliah/dasar_pemrograman/Projek-UAS-DasPro-GachaQuest`.
2. Inisialisasi git dan branch `main`.
3. Buat folder `docs/`.

Expected:

```sh
cd D:/Kuliah/dasar_pemrograman/Projek-UAS-DasPro-GachaQuest
git status
git branch --show-current
```

```text
On branch main
nothing to commit
main
```

### Fase 1 — Salin panduan

1. Salin `rencana-pembagian-tugas-5-orang.md` dari repo lama.
2. Salin tujuh panduan: `panduan-5-orang.md`,
   `panduan-0-git-dan-kompilasi.md`, dan `panduan-P1` sampai `panduan-P5`.
3. Jangan menyalin file implementasi, data, atau test.

Expected:

```sh
find docs -maxdepth 1 -type f | sort
```

```text
docs/panduan-0-git-dan-kompilasi.md
docs/panduan-5-orang.md
docs/panduan-P1-scaffold-ui.md
docs/panduan-P2-dungeon.md
docs/panduan-P3-battle-player.md
docs/panduan-P4-gacha-inventory.md
docs/panduan-P5-shop-storage.md
docs/rencana-pembagian-tugas-5-orang.md
```

### Fase 2 — Tulis README

`README.md` harus memuat:

- Tujuan starter kit.
- Daftar isi dan urutan baca.
- Prasyarat: Git, gcc, make, MSYS2.
- Path repo referensi.
- Aturan bahwa starter kit tidak berisi jawaban.
- Definisi hasil akhir: project baru yang setara repo referensi.
- Cara memulai: baca `docs/panduan-5-orang.md`.

Expected:

```sh
grep -n "panduan-5-orang" README.md
```

```text
... docs/panduan-5-orang.md
```

### Fase 3 — Rapikan panduan

Setiap panduan per peran wajib punya bagian:

- Peran.
- File milik.
- Langkah kerja.
- Expected output.
- Definition of Done.
- Larangan.

Panduan tidak boleh memuat implementasi fungsi, rumus, atau kode solusi.
Spesifikasi fitur tetap menunjuk ke PRD, header referensi, `README.md`, `UI.md`,
dan test referensi.

Expected:

```sh
for f in docs/panduan-P*.md; do
  grep -q "Expected" "$f" || echo "TANPA EXPECTED: $f"
  grep -q "Definition of Done" "$f" || echo "TANPA DOD: $f"
done
```

```text
# tidak ada output
```

### Fase 4 — Validasi tautan

1. Pastikan `panduan-5-orang.md` menautkan semua panduan.
2. Pastikan rencana induk menautkan indeks dan semua panduan.
3. Pastikan path file yang disebut benar-benar ada.

Expected:

```sh
grep -o 'panduan-[A-Za-z0-9-]*\.md' docs/rencana-pembagian-tugas-5-orang.md | sort -u
```

```text
panduan-0-git-dan-kompilasi.md
panduan-5-orang.md
panduan-P1-scaffold-ui.md
panduan-P2-dungeon.md
panduan-P3-battle-player.md
panduan-P4-gacha-inventory.md
panduan-P5-shop-storage.md
```

### Fase 5 — Commit

```sh
git add README.md .gitignore docs
git commit -m "docs: starter kit pembagian tugas 5 orang"
```

Expected:

```text
[main ...] docs: starter kit pembagian tugas 5 orang
```

## Alur Kerja yang Dipandu

Peserta mengikuti urutan ini setelah starter kit dibagikan:

1. P1 menyiapkan repo kerja dan menyalin kontrak dari referensi.
2. P1 membangun scaffold, UI lib, dan platform sampai tag `ui-ready`.
3. P2-P5 bekerja paralel sesuai panduan masing-masing.
4. P1 menggabungkan modul ke `dev`.
5. Semua menjalankan `make test`; P1 menjalankan paritas output.
6. Demo manual 9 layar dan latihan presentasi.

Starter kit tidak mengerjakan langkah di atas. Starter kit hanya memastikan
setiap orang tahu langkah, batas file, dan bukti yang harus muncul.

## Verification

Dari folder starter kit:

```sh
find . -maxdepth 2 -type f | sort
test ! -e Makefile
test -z "$(find . -name '*.c' -o -name '*.h')"
grep -R "Expected" docs | wc -l
```

Expected:

- Hanya `README.md`, `.gitignore`, dan delapan file `docs/*.md`.
- Tidak ada `Makefile`, `.c`, atau `.h`.
- `grep "Expected"` menghasilkan angka lebih dari nol.
- `panduan-5-orang.md` bisa dibuka dan semua tautannya valid.

Uji pemahaman manual:

1. Minta P1 membaca `panduan-P1-scaffold-ui.md` dan menyebut perintah pertama
   serta expected output-nya.
2. Minta P2-P5 membaca panduan masing-masing dan menyebut file yang boleh
   diedit.
3. Minta salah satu pemula menjalankan `make` dari repo kerja setelah P1
   scaffold; mereka harus tahu output yang diharapkan tanpa bertanya.

## Assumptions & Contingencies

- Repo referensi tersedia di path yang disebut. Jika tidak, ganti path di
  README dan panduan sebelum dibagikan.
- Starter kit tidak menyertakan header, data, atau test. P1 menyalinnya dari
  referensi pada Fase 0 pekerjaan, sesuai panduannya.
- Jika peserta ingin kontrak ikut dibundel, tambahkan folder `contract/`
  berisi header/data/test dari referensi. Itu perubahan scope dan harus
  disetujui sebelum dikerjakan.
- Jika isi panduan bertentangan dengan rencana induk, rencana induk menang
  untuk pembagian peran dan urutan fase; panduan menang untuk langkah harian.
- Jika ada panduan yang memuat kode solusi, hapus kodenya dan ganti dengan
  langkah plus expected output. Panduan bukan jawaban.
