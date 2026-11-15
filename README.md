# Gacha Quest

Mini RPG + Gacha berbasis CLI (Bahasa C) sesuai `docs/Projek UAS Daspro Gacha Quest 41774797398a802c9345c49291a6f695.md`.

Catatan lengkap semua layar (tampilan, warna, pesan): `UI.md`.

## Tampilan CLI (Redraw per Layar)

Setiap layar (menu utama, Hub, Battle, Gacha, Inventory, Shop, Dungeon, Status,
Leaderboard) digambar ulang dari awal lewat ANSI escape `\033[2J\033[H` yang
dikelola satu fungsi `clear_screen()` di `src/ui/ui.c`. Saat startup `ui_init()`
mengaktifkan Virtual Terminal Processing (Windows) dan code page UTF-8, jadi
tanpa `system("cls")` dan tanpa teks lama menumpuk.

- **Header panel.** Semua layar berbingkai panel 50 kolom (`draw_header`) yang
  menampilkan nama pemain, level, skor, HP bar (`HP [########--] 80/100`,
  hijau/kuning/merah sesuai persentase), gold (kuning), dan potion.
- **Pesan terakhir.** Hasil aksi ("Gold kurang!...", "Dapat item X") disimpan
  lewat `set_message()` dan dirender di slot tetap setelah redraw, jadi tidak
  tertimpa clear. Warna: judul cyan, error merah, sukses/hijau, rarity
  Legendary magenta / Epic kuning / Rare cyan.
- **Pause.** Setelah hasil yang perlu dibaca (hasil gacha, ringkasan battle,
  keluar dungeon) muncul "Tekan Enter untuk lanjut..." (`ui_wait_enter`); saat
  stdin bukan terminal (input pipa), pause dilewati otomatis.
- **Input tidak valid** (huruf, di luar rentang, kosong, EOF) menyebabkan layar
  yang sama digambar ulang dengan pesan error, tanpa crash.

## Cara Kompilasi

Butuh `gcc` (C11) dan `make` (GNU make). Build memakai `-Wall -Wextra -Wpedantic`.

- Linux / WSL:
  ```
  make            # kompilasi ke bin/game, tanpa warning (-Wall -Wextra)
  make start      # kompilasi lalu langsung main bin/game
  make test       # jalankan seluruh pengujian otomatis
  make debug      # kompilasi + uji dengan AddressSanitizer/UBSan
  make preview    # jalankan perkakas pratinjau widget UI (tools/ui_preview.c)
  make clean
  ```

`make start` hanya menjalankan biner; tipe terminal tetap dideteksi game
sendiri, jadi tidak perlu variabel apa pun. Di Windows pakai cmd, PowerShell,
atau Windows Terminal (bukan mintty MSYS2) karena `_getch()` butuh konsol
asli.
- Windows (MSYS2 / MinGW): tambahkan `C:\msys64\usr\bin` ke PATH agar `sh`,
  `rm`, `grep`, `cp` tersedia untuk Makefile, lalu jalankan perintah yang sama.
  Target `make debug` otomatis memakai clang64 (`C:\msys64\clang64\bin\clang.exe`)
  karena runtime sanitizer milik gcc MSYS2 mungkin belum terpasang.

Buka shell pada folder proyek (root tempat Makefile berada) sebelum
menjalankan make (file data dibaca dengan path relatif).

## Cara Bermain dari Terminal Apa Saja

Kompilasi sekali (`make`), lalu jalankan biner yang sama dari shell mana pun.
Game mendeteksi kemampuan terminal secara otomatis; tidak ada flag yang wajib
disetel.

| Terminal | Perintah menjalankan | Hasil tampilan |
| --- | --- | --- |
| Windows Terminal, PowerShell, CMD | `bin\game.exe` | truecolor + Unicode (VT diaktifkan otomatis) |
| Git Bash / MSYS2 mintty | `./bin/game.exe` | truecolor/256 + Unicode |
| WSL (Ubuntu dkk.) | `./bin/game` | ikuti `TERM`/`COLORTERM` milik WSL |
| Linux: GNOME Terminal, Konsole, xterm, Alacritty, kitty | `./bin/game` | truecolor/256 + Unicode |
| macOS: Terminal.app, iTerm2 | `./bin/game` | 256/truecolor + Unicode |
| SSH, tmux, screen | `./bin/game` | truecolor bila `COLORTERM` diteruskan; sisanya 256/16 |
| Terminal lawas (`TERM=dumb`, cons68xx, output di-redirect) | sama | mono ASCII otomatis |

Aturan yang berlaku di semua terminal:

- **Ukuran minimum 50x20.** Lebih kecil → pesan "Perbesar terminal (min 50x20)",
  layout tidak digambar rusak. Layout rata kiri dan padat: panel tetap 50 kolom,
  sisa terminal dibiarkan kosong di kanan, footer langsung di bawah konten.
- **Kontrol menu:** panah atas/bawah (atau `W`/`S`, `K`/`J`) + Enter; angka
  `0-9` langsung memilih item; `Esc`/`Q` kembali.
- **Dungeon:** `W`/`A`/`S`/`D` atau panah untuk jalan langsung tanpa Enter,
  `H` hint (30 gold), `Q` keluar.
- **Build + main:** `make start` (compile semua lalu jalankan `bin/game`);
  `make` saja hanya membangun.
- **Ctrl+C / kill / terminal ditutup aman:** handler sinyal memulihkan terminal
  (kursor tampil, echo hidup, alternate screen keluar), autosave terakhir
  ditulis, lalu `data/session.lock` ditandai `CLEAN`. Crash (segmentasi,
  abort) juga memulihkan terminal dan menambah satu baris di `data/crash.log`.
- **Autosave:** thread latar menulis `data/autosave.dat` setiap aksi (debounce
  1500 ms, `GACHA_AUTOSAVE_MS` untuk mengubah, minimal 200). Kejadian penting
  (akhir battle, hasil gacha, jual/beli, treasure, equip) langsung ditulis.
  File ditulis atomik (tmp + fsync + rename) dan file lama disimpan sebagai
  `autosave.bak`; jika utama rusak, versi `.bak` yang dipakai.
- **Satu sesi sekaligus:** instance kedua ditolak dengan pesan "Game sudah
  berjalan di terminal lain." Setelah crash, saat dijalankan lagi muncul
  tawaran "Pulihkan sesi sebelumnya? (y/n)" yang melanjutkan battle atau
  dungeon dari posisi terakhir.
- **Resize jendela** saat bermain langsung menggambar ulang layout.

Mode warna dan glyph dideteksi, bukan ditebak:

| Kondisi | Mode |
| --- | --- |
| `NO_COLOR` terisi, `TERM=dumb`, atau output bukan TTY | mono, ASCII |
| `COLORTERM` berisi `truecolor`/`24bit` | truecolor |
| `TERM` berisi `256color` | 256 warna |
| selain itu | 16 warna |
| locale UTF-8 (`LANG`/`LC_ALL`/`LC_CTYPE`) | glyph Unicode `┌ ─ │ █ ░` |
| locale non-UTF-8 (mis. `LANG=C`) | ASCII `+ - \| #` |

Override manual lewat environment, berguna kalau deteksi keliru:

| Shell | Contoh |
| --- | --- |
| bash/zsh/fish (Git Bash, WSL, Linux, macOS) | `GACHA_COLOR=truecolor ./bin/game` |
| PowerShell | `$env:GACHA_COLOR="truecolor"; bin\game.exe` |
| CMD | `set GACHA_COLOR=truecolor && bin\game.exe` |

Nilai yang valid: `GACHA_COLOR=mono|16|256|truecolor`, `GACHA_ASCII=1`
(paksa ASCII), `GACHA_UNICODE=1` (paksa Unicode).

**Mode non-interaktif:** bila stdin/stdout di-pipe atau di-redirect (mis.
`bin/game < tests/in_demoA.txt`), game otomatis memakai mode fallback: tanpa
warna, tanpa clear screen, input baris seperti versi lama — dipakai oleh seluruh
test dan demo alur di bagian bawah README.

## Cara Bermain

```
bin/game          (Linux: ./bin/game)
```

1. **Game Baru** → masukkan nama, lalu **seed dunia** → dapat HP 100, ATK 10,
   DEF 3, 200 gold, 2 potion.
2. **Battle** (hub menu 1) → monster acak sesuai level, 12 jenis dari Slime
   sampai Ancient Wyrm (level minimum 1 sampai 10); serang / bertahan /
   potion / kabur. Menang: EXP + gold + skor. Kalah: gold setengah, HP pulih.
3. **Gacha** (hub menu 2) → 100 gold per tarikan, 900 gold per 10x. Katalog 36
   item, 18 senjata dan 18 armor dengan nama Inggris: 12 Common, 12 Rare,
   8 Epic, 4 Legendary. Rarity: Common/Rare/Epic/Legendary; pity dijamin Epic
   setelah 10 tarikan tanpa Epic+. Item duplikat otomatis jadi gold.
4. **Inventory** (hub menu 3) → lihat, urutkan (rarity/power/nama), cari, equip.
5. **Shop** (hub menu 4) → beli potion (40 gold, maks 20), jual item.
6. **Dungeon** (hub menu 5) → dunia prosedural tak berujung dari seed, gerak
   WASD, `$` = treasure (+30 gold), H = hint BFS ke treasure terdekat
   (30 gold, maks 3x), Q = keluar. Rincian di bagian berikut.
7. **Simpan** (hub menu 7) atau Kembali ke menu utama (dipaksa menawarkan simpan,
   lalu skor dicatat ke leaderboard).
8. Menu utama **Load Game** menampilkan 5 save slot (nama pemain tiap slot) dan
   memuat slot yang dipilih; **Leaderboard** menampilkan top 5.

## Dungeon Prosedural dan Seed

Peta dungeon tidak lagi dibaca dari file. Setiap sel dunia dihitung dari seed
pemain, jadi dunia tidak terbatas ke segala arah dan tidak butuh memori untuk
menyimpan peta.

- **Isi sel.** Koridor terbuka pada `y = 1 (mod 4)` dan poros vertikal pada
  `x = 1 (mod 6)`. Sel lantai lain kosong, dan sekitar satu dari 89 sel lantai
  berisi treasure. Semua keputusan itu berasal dari `cellHash(seed, x, y)`,
  sebuah hash integer murni tanpa `rand()`.
- **Seed sama, peta sama.** Seed yang diminta saat game baru ikut tersimpan di
  save slot, jadi **Load Game** mengembalikan dunia yang sama persis.
  Seed juga tampil di menu Status dan di baris info dungeon.
- **Seed kosong.** Kalau prompt seed dijawab Enter, program memakai seed dari
  jam sistem dan menampilkannya supaya bisa dicatat dan dipakai ulang.
- **Cache 5x5 chunk.** Dungeon hanya menghitung area 80x80 sel di sekitar
  pemain, lalu menggeser cache ketika pemain pindah chunk. Kamera menampilkan
  jendela 25x15 dengan pemain selalu di tengah.
- **DFS.** Saat masuk, DFS memeriksa apakah ujung koridor area awal bisa
  dicapai dari titik start. Hasilnya muncul sebagai pesan status.
- **BFS.** Tombol H menandai jalur terpendek ke treasure terdekat di dalam
  cache. Tanda `*` hanya ada di tampilan dan hilang setelah langkah berikutnya.
- **Keluar.** Q keluar tanpa bonus exit. Treasure yang sudah diambil tetap
  dihitung ke gold, dan dunia yang sama tidak lagi punya treasure di posisi itu
  selama run berlangsung.

## Daftar Modul

| File | Isi |
| --- | --- |
| main.c | Menu utama, loop hub, dan alur pemulihan sesi |
| src/core/config.h | Konstanta dan struct Item, Monster, Player, Point, DungeonState |
| src/util/util.c/.h | readInt, readString, readLine, clearScreen, printLine, shutdownGame |
| src/ui/ui.c/.h | API tampilan: frame layar, panel, menu, input frame, pesan terakhir |
| src/ui/term.c/.h | Lapisan terminal: raw mode, ukuran, baca tombol, alternate screen, restore |
| src/ui/caps.c/.h | Deteksi kemampuan terminal (warna, Unicode, ukuran, TTY) |
| src/ui/style.c/.h | Token warna semantik → escape ANSI sesuai mode warna |
| src/ui/render.c/.h | Frame buffer grid; satu write per frame |
| src/ui/widgets.c/.h | Panel, tabel/menu ter-highlight, bar HP/EXP, footer, kotak log |
| src/player/player.c/.h | initPlayer, addExp, totalAtk/totalDef, showStatus |
| src/battle/battle.c/.h | startBattle, resumeBattle, battleLoop, calculateDamage |
| src/gacha/gacha.c/.h | gachaMenu, rollRarity, findItemInInventory, pullOnce |
| src/inventory/inventory.c/.h | inventoryMenu, sortInventory, searchItemByName, equipItem |
| src/shop/shop.c/.h | shopMenu (beli potion, jual item) |
| src/storage/storage.c/.h | loadCatalog, loadMonsters, saveGame, loadGame, recordScore, showLeaderboard |
| src/dungeon/dungeon.c/.h | cellHash, dungeonCell, promptWorldSeed, dfsReachable, bfsShortestPath, runDungeon, resumeDungeon |
| src/platform/rng.c/.h | PRNG xorshift64* yang state-nya bisa disimpan (`plat_srand`, `plat_rand`, `plat_rng_state`, `plat_rng_set`) |
| src/platform/atomicfile.c/.h | Tulis atomik (tmp + fsync + rename) dan pembersihan tmp yatim |
| src/platform/lock.c/.h | Kunci sesi `data/session.lock` (RUNNING/CLEAN) |
| src/platform/sig.c/.h | Sinyal shutdown: self-pipe + poll (POSIX), SetConsoleCtrlHandler (Windows) |
| src/platform/crash.c/.h | Handler crash di sigaltstack: pulihkan terminal, catat `data/crash.log` |
| src/platform/autosave.c/.h | Thread autosave (debounce, CRC, rotasi .bak, backoff) |
| src/platform/platform.c/.h | platform_init/shutdown, status sesi sebelumnya, toggle permintaan shutdown |
| data/ | item.txt, monster.txt (runtime: save_1..5.txt, leaderboard.txt, autosave.dat/.bak, session.lock, crash.log) |
| tests/ | Driver uji C + skenario input pipa + run_tests.sh |

Nama fungsi dan komentar di kode memakai bahasa Inggris. Teks yang dilihat
pemain tetap bahasa Indonesia.

## Berkas runtime di data/

Dibuat saat bermain, tidak perlu di-commit (ada di `.gitignore`):

| Berkas | Isi |
| --- | --- |
| `save_1.txt` .. `save_5.txt` | Progres manual per slot (format di bawah). `save.txt` lama dibaca sebagai slot 1 selama `save_1.txt` belum ada |
| `leaderboard.txt` | 10 skor teratas |
| `autosave.dat` | Snapshot biner: magic `GQAS`, versi, panjang, CRC32, lalu struct `AutoSnapshot` (pemain, layar aktif, state battle/dungeon, state RNG) |
| `autosave.bak` | Snapshot sebelumnya, dipakai bila `autosave.dat` rusak/terpotong |
| `session.lock` | `RUNNING pid=<pid> start=<epoch>` selama berjalan, `CLEAN` setelah keluar rapi |
| `crash.log` | Satu baris per crash (`crash sig=<n> pid=<pid>`) |

## Format data/save_N.txt

Baris pertama berisi 15 field dipisah `|`:

```
nama|hp|maxHp|level|exp|gold|atk|def|potion|pity|totalPull|skor|seed|idSenjata|idArmor
```

Baris kedua adalah jumlah item, lalu satu ID item per baris. Field `seed`
bertipe unsigned dan menyimpan seed dunia dungeon. Simpan di hub (menu 7)
menampilkan kelima slot; pilih slot kosong, timpa slot terisi, atau pakai
"Buat save baru" yang menunjuk slot kosong pertama. Save lama tanpa field seed
ditolak dengan pesan "format save slot N rusak", jadi mulai game baru sekali
setelah pembaruan ini.

## Pembagian Tugas (anggota A-E, sesuai PRD bagian 13)

| Anggota | Peran | Modul |
| --- | --- | --- |
| A | Arsitek dungeon | src/core/config.h, src/dungeon/dungeon.c/.h |
| B | Integrator | main.c, src/battle/battle.c/.h, src/storage/storage.c/.h |
| C | Gacha | src/gacha/gacha.c/.h |
| D | Inventory | src/inventory/inventory.c/.h |
| E | Shop dan status | src/shop/shop.c/.h, src/player/player.c/.h, src/util/util.c/.h |
| bersama | Lapisan UI dan platform | src/ui/*, src/platform/* (dibagi sesuai kebutuhan revisi) |

## Pengujian

`make test` menjalankan `tests/run_tests.sh` dengan 20 pemeriksaan:
- Driver C (logika murni): readInt/readString, level & EXP, damage,
  rarity/pity/duplikat, sort & search inventory, katalog/monster/save-load
  round-trip termasuk seed, leaderboard, serta determinisme sel dungeon,
  BFS, dan DFS.
- Platform (`tests/t_platform.c`): round-trip state RNG, snapshot autosave
  (payload utuh), penolakan snapshot rusak/terpotong, fallback `.bak`, tulis
  atomik, serta isi `session.lock` (`RUNNING pid=` lalu `CLEAN`).
- Skenario menu: input pipa dari file ke program utama, hasil dicek grep.
  S16 memastikan seed 777 dipakai lagi setelah simpan dan Load Game.
- `make debug` mengulang seluruh pengujian dengan AddressSanitizer + UBSan.

## Demo alur (new game -> battle -> gacha -> equip -> dungeon -> save -> load -> leaderboard)

Tiga run input pipa yang sudah disiapkan di `tests/`:

```
bin/game < tests/in_demoA.txt   # game baru Demo seed 20240601: gacha, equip, dungeon, simpan
bin/game < tests/in_demoB.txt   # lanjutkan, status, skor dicatat, leaderboard
bin/game < tests/in_demoC.txt   # lanjutkan, battle beberapa kali (EOF menghentikan dengan rapi)
```

Jalankan berurutan karena demoB dan demoC memakai save dari demoA.
