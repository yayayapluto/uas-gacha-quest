# DECISIONS.md

Keputusan untuk hal yang tidak dispesifikasi PRD, diambil sederhana dan konsisten:

1. **EOF pada prompt.** `readInt` yang kehabisan input (EOF/Ctrl+D) memakai
   nilai `min` dan program bergerak ke jalur keluar yang aman (menu bernilai 0
   keluar; battle berhenti lewat cek `feof(stdin)`); tidak ada loop abadi.
2. **Nama kosong.** Ditolak dan diminta ulang; nama lebih panjang dari 29
   karakter dipotong (skenario 1: "ditolak atau dipotong").
3. **Beli potion.** Satu transaksi bisa membeli beberapa potion sekaligus;
   jumlah diminta dan divalidasi terhadap sisa stok (maks 20) dan gold.
4. **Potion saat HP penuh.** Tetap boleh dipakai (heal di-cap di maxHp); PRD
   tidak melarangnya. Potion 0 ditolak tanpa memakai giliran.
5. **Pity pada paket 10x.** Pity diproses per tarikan secara berurutan; tarikan
   yang dijamin pity menghasilkan Epic (bukan Legendary) kecuali roll acak
   memberi Legendary.
6. **EXP di level 10.** EXP direset ke 0 dan tidak lagi dihitung (maks level 10).
7. **Dungeon prosedural, bukan peta dari file.** PRD bagian 7 meminta peta
   `dungeon1-3.txt`, tetapi permintaan terbaru adalah dungeon dengan seed
   seperti Minecraft. Dunia sekarang dihitung per sel dari
   `cellHash(seed, x, y)`: koridor pada `y = 1 (mod 4)`, poros pada
   `x = 1 (mod 6)`, treasure kira-kira 1 dari 89 sel lantai. File peta
   dihapus, `muatPeta` dan konstanta `MAX_BARIS`/`MAX_KOLOM` ikut dibuang.
8. **Seed dunia.** Seed diminta sekali saat game baru, disimpan di
   `data/save.txt`, dan dipakai ulang setelah Lanjutkan. Prompt yang dijawab
   Enter memakai seed dari jam sistem lalu menampilkannya. Input selain angka
   0 sampai 4294967295 ditolak dan diminta ulang. Seed 0 sah dipakai.
9. **Keluar dungeon.** Q keluar tanpa bonus exit karena dungeon tidak berujung
   dan tidak punya pintu keluar. Treasure yang sudah diambil tetap dihitung
   (tidak dikembalikan).
10. **Hint BFS.** BFS mencari treasure terdekat di dalam cache, bukan exit.
    Jalur tanda `*` hanya tampil pada tampilan saat itu dan hilang setelah
    langkah berikutnya; peta asli tidak berubah. Biaya 30 gold, maks 3x.
11. **Cache area dan kamera.** Dungeon menghitung area 5x5 chunk (80x80 sel)
    di sekitar pemain dan menggesernya saat pemain pindah chunk. Kamera
    menampilkan jendela 25x15 dengan pemain di tengah. Ukuran tetap ini menjaga
    pemakaian memori statis tanpa malloc.
12. **Batas treasure per run.** Maksimal 64 posisi treasure disimpan per
    kunjungan (`MAX_TAKEN_TREASURES`). Lewat dari itu treasure tetap memberi
    gold, hanya tidak lagi dicatat sehingga bisa muncul kembali setelah cache
    dibangun ulang.
13. **Simpan ID terpasang.** data/save.txt menyimpan `weaponIndex`/`armorIndex`
    sebagai ID item (bukan indeks), lalu dimetakan kembali ke indeks inventory
    saat muat, sesuai bagian 9 PRD.
14. **Graceful shutdown.** Keluar normal lewat satu pintu (`shutdownGame`) di
    jalur menu. Sinyal (Ctrl+C, `kill`, terminal ditutup) ditangani
    `src/platform/sig.c`: handler hanya menyetel flag, loop UI berhenti di
    titik aman, autosave terakhir ditulis, lalu `session.lock` ditandai
    `CLEAN`. Lihat keputusan 19-24 untuk mekanismenya.
15. **`recordScore` saat penuh.** Jika leaderboard sudah 10 entri, entri terbaru
    menggantikan skor terendah; file selalu diurut menurun dan dipotong 10.
16. **Jual item terpasang.** Ditolak dengan pesan; sinkronisasi
    `weaponIndex`/`armorIndex` setelah sort/jual memakai ID item (fungsi
    `syncEquipped`).
17. **Format save bertambah satu field.** Field `seed` disisipkan sebelum
    `idSenjata`, sehingga baris pertama berisi 15 field. Save lama 14 field
    ditolak dengan pesan "format data/save.txt rusak" dan pemain perlu memulai
    game baru sekali. Membaca dua format sekaligus dianggap tidak sepadan
    karena save hanya berisi progres sesi, bukan data yang dibagikan.
18. **make debug di Windows.** Karena runtime sanitizer gcc MSYS2 (libasan/
    libubsan) tidak tersedia, target debug memakai clang64 (clang.exe) yang
    punya libclang_rt.asan/ubsan; di Linux tetap gcc.
19. **Modul platform.** Kode OS-spesifik dipisah ke `src/platform/` (rng,
    atomicfile, lock, sig, crash, autosave, platform) dan dipanggil dari
    `main()` sebelum `ui_init`. Logika game tidak berubah; hanya `rand()`
    diganti `plat_rand()` karena state RNG harus ikut tersimpan.
20. **PRNG terserialisasi.** xorshift64* dengan keluaran 31 bit meniru rentang
    `rand() % N`. `plat_rng_state`/`plat_rng_set` memakai `unsigned long long`
    karena `long` hanya 4 byte di Windows; state 64 bit tidak muat di `long`.
21. **Autosave terpisah dari save.txt.** Snapshot biner `data/autosave.dat`
    (magic `GQAS`, versi, panjang, CRC32, payload struct) ditulis thread
    latar; `save.txt` tetap format teks manual. Hanya snapshot terbaru yang
    ditulis (pending di memori), debounce 1500 ms (env `GACHA_AUTOSAVE_MS`,
    minimal 200), kejadian penting langsung flush. CRC identik dilewati.
22. **Rotasi .bak.** Sebelum rename, file lama dijadikan `autosave.bak`;
    muat memvalidasi magic/versi/panjang/CRC dan jatuh ke `.bak` bila utama
    rusak.
23. **Kunci sesi.** `data/session.lock` berisi `RUNNING pid=<pid> start=<epoch>`
    dan `CLEAN` saat keluar rapi; proses kedua ditolak. Windows mengunci satu
    byte di offset 0x7FFFFFF0, bukan byte 0, supaya pembaca isi file tidak
    terkena lock violation.
24. **Shutdown dan crash.** POSIX memakai self-pipe + `poll` di `term_getkey`,
    handler hanya menyetel flag, sinyal kedua diabaikan, watchdog `_exit(130)`
    setelah 3 detik. Windows memakai `SetConsoleCtrlHandler` + `_kbhit` polling
    agar loop UI tetap reaktif. Crash handler (sigaltstack) memulihkan terminal
    lalu menambah satu baris di `data/crash.log` sebelum meneruskan sinyal;
    handler tidak pernah menulis state game.
25. **RNG ikut disimpan, bukan diseed ulang.** Karena `rand()` tidak bisa
    dibaca state-nya, seluruh panggilan diganti `plat_rand()` dan state ikut
    masuk snapshot. Sesi yang dipulihkan melanjutkan urutan acak yang sama
    seperti sebelum crash, bukan urutan baru.
26. **Autosave hanya menulis snapshot terbaru.** Setiap `autosave_publish`
    menyalin snapshot ke memori; thread latar membuang versi lama bila ada
    snapshot lebih baru selama debounce. Tidak ada antrean berkas.
27. **Titik publish.** Setelah tiap aksi dan sebelum redraw (`important=0`,
    kena debounce), serta langsung untuk kejadian yang tidak boleh hilang:
    akhir battle, hasil gacha, jual/beli, equip, treasure, dan keluar dungeon.
28. **`make start` hanya menjalankan biner.** Deteksi tipe terminal tetap
    tanggung jawab `caps_detect`, bukan Makefile; duplikasi logika di Makefile
    akan berbeda hasil dengan programnya sendiri.
29. **Log aksi disimpan sebagai segmen bergaya, bukan penanda dalam teks.**
    `LogLine` menyimpan sampai empat bagian (`actor`, `action`, `target`,
    `value`) dengan `Style` masing-masing, lalu `ui_log_draw` memanggil
    `render_span` per bagian. Penanda `~` di dalam string akan rusak begitu
    nama pemain atau item memuat karakter itu.
30. **Lima save slot (`data/save_1..5.txt`).** Nomor slot tetap supaya daftar
    di picker tidak berubah panjang; `data/save.txt` lama hanya dibaca sebagai
    slot 1 selama `save_1.txt` belum ada, lalu otomatis tidak dipakai.
    Autosave tetap terpisah (snapshot biner), karena tujuannya memulihkan sesi
    yang crash, bukan memilih progres.
31. **Picker baris punya fallback pipa.** `ui_pick_row` kini menggambar sekali
    lalu membaca nomor baris saat stdin bukan terminal, sehingga jalur
    non-interaktif (uji pipa) bisa memilih slot seperti jalur interaktif.
