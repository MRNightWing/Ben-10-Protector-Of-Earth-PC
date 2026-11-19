# Ben 10: Protector of Earth (PSP) - Fonksiyon Haritası

Bu harita, `EBOOT.ELF` ikili dosyası üzerinden MIPS assembly, sistem çağrıları (syscall) ve motor içi string çapraz referansları (Xref) analiz edilerek doğrulanmıştır.

---

## 1. Ana Motor ve Oyun Döngüsü (Core & Main Loop)
* `0x0008e298` - **`Engine_MainLoop`**: Ana motor ve oyun döngüsü. Her karede tüm alt sistemleri (girdi, seviye mantığı, vblank, render) tetikler.
* `0x0008e4d0` - **`Engine_WaitVblank`**: `sceDisplayWaitVblankStart` çağrısı yaparak dikey senkronizasyonu sağlar ve FPS'i sabitler.
* `0x0006f824` - **`Engine_LevelManager`**: Bölüm yapılandırma ve durum yöneticisi. Seviye geçişlerini, yüklemeleri ve harita güncellemelerini idare eder.
* `0x000070ac` - **`Engine_LoadStartLevel`**: Oyun başlangıç/ilk bölüm yükleyicisi (`OFFER_MULTIPLAYER`, `AUTO_CONNECT`, `ROLLING_LOG` parametrelerini okur).
* `0x00006e94` - **`Engine_GetConfigParam`**: Motor konfigürasyon ve hafıza parametre sorgusu.

---

## 2. Giriş ve Kontrolcü Sistemi (Input / Controller)
* `0x0007141c` - **`Input_PollController`**: Doğrudan PSP donanımından (`sceCtrlReadBufferPositive`) analog çubuk ve tuş basışlarını okur.
* `0x00071af0` - **`Engine_UpdateInput`**: Okunan kontrolcü verilerini motorun oyun mantığına ve karakter aksiyonlarına aktarır.
* `0x000e6758` - **`sceCtrlSetSamplingMode`**: PSP kontrolcüsü analog örnekleme modu ayarı.
* `0x000e6770` - **`sceCtrlSetSamplingCycle`**: Kontrolcü tarama döngü sıklığı.

---

## 3. Arşiv ve Kaynak Akışı (WAD & Asset Streaming - CStreamManager)
* `0x000811ec` - **`Engine_StreamStratWad`**: WAD arşivi ve strateji/script (`.strat`) yükleme yöneticisi (`StreamStratWad %d:"%s"`).
* `0x0007fb14` - **`CStreamManager_StartWad`**: Bir WAD arşivini hedef katmana bağlayıp yükleme sürecini başlatır.
* `0x0007fd20` - **`CStreamManager_StopWad`**: Belirtilen WAD arşivini durdurur ve temizler.
* `0x0007fdc4` - **`CStreamManager_SuspendWad`**: Bellek tasarrufu için WAD arşivini askıya alır.
* `0x0007ff64` - **`CStreamManager_UnsuspendWad`**: Askıdaki WAD arşivini tekrar aktif hale getirir.
* `0x00080d2c` - **`CStreamManager_StreamWad`**: Arka planda asenkron veri akışını yürütür (`StreamWad %d:"%s"`).
* `0x00081648` - **`CStreamManager_RemoveWad`**: WAD arşivini bellekten tamamen kaldırır.
* `0x00081f5c` - **`CStreamManager_LoadMapAndStrats`**: Bölüm haritası ve seviye scriptlerini yükler.
* `0x00080458` - **`CStreamManager_StartLoad`**: Asenkron dosya yükleme başlatıcı ve hata yakalayıcı.

---

## 4. Düşük Seviyeli Dosya Sistemi (PSPFile - UMD / IO)
* `0x00070e08` - **`PSPFile_Open`**: UMD veya dosya sisteminden dosya tanıtıcısı (`cFilePtr`) açar.
* `0x00070efc` - **`PSPFile_Close`**: Açık dosyayı kapatır ve I/O hatalarını denetler.
* `0x00070f5c` - **`PSPFile_Read`**: `PSPFile::Read()` senkron/asenkron veri blok okuyucusu.
* `0x000705ac` - **`PSPFile_AsyncClose`**: Arka plan asenkron dosya kapatma rutini.
* `0x00070654` - **`PSPFile_AsyncRead`**: Asenkron UMD blok okuma işlemi.
* `0x00070a90` - **`Engine_UMDActivate`**: PSP UMD sürücüsünü aktif duruma getirir.
* `0x00070af8` - **`Engine_UMDCheckState`**: UMD disk hata ve durum kontrolü (`Disc Unusual State`).

---

## 5. Grafik ve Render Tamponları (Graphics & Framebuffer)
* `0x000c78e8` - **`Engine_InitFramebuffer`**: Ekran tamponu ilklendirme wrapper'ı.
* `0x000c76c4` - **`Engine_SetupRenderBuffers`**: Ana render bellek yapılandırması (VRAM / EDRAM yerleşimi).
* `0x000e6738` - **`sceDisplaySetFrameBuf`**: PSP ekran kontrolcüsünde aktif framebuffer adresini günceller (Buffer Swap/Flip).
* `0x000e6740` - **`sceDisplayWaitVblankStart`**: VBlank dikey tarama bekleme çağrısı.

---

## 6. Bellek ve Heap Yönetimi (Memory Management)
* `0x000573c8` - **`Engine_MemAlloc`**: Dinamik bellek tahsisatı (`Allocating %ik, Available: %ik`).
* `0x000578e4` - **`Engine_MemPrintStats`**: Bellek kullanım istatistikleri (`Total Heap %d, High Water Mark %d`).
* `0x000924a0` - **`Engine_VolatileMemInit`**: PSP Volatile (uçucu) bellek alanını başlatır.
* `0x00092774` - **`Engine_VolatileMemShutdown`**: Volatile bellek alanını serbest bırakır.
* `0x0007f5ec` - **`Engine_VolatileMemUnlock`**: Volatile bellek kilidini çözer.

---

## 7. Ses, Müzik ve Sinematik (Audio & Video System)
* `0x0007f01c` - **`Engine_Log`**: Motor içi formatlı ana loglama ve uyarı mekanizması.
* `0x00089dc4` - **`Sound_Play`**: Ses efekti tetikleme ve ses kanalı çalma.
* `0x000af670` - **`Sound_LoadControlFile`**: Ses kontrol ve banka dosyası yükleyicisi.
* `0x000900c4` - **`Video_MpegInit`**: PSP `sceMpeg` kütüphanesini ve video akışını ilklendirir.
* `0x00092fb0` - **`Video_DecodeAtrac`**: Sinematik video içi ATRAC3 ses akış çözücüsü.
* `0x000930a8` - **`Video_DecodeAvc`**: Sinematik video içi AVC/H.264 video karesi çözücüsü.

---

## 8. Animasyon ve İskelet Sistemi (Animation & Skeleton)
* `0x000203d4` - **`Anim_SetSpeed`**: Karakter veya nesne animasyonunun oynatma hızını belirler.
* `0x00020e44` - **`Bone_FindByName`**: Model hiyerarşisi içinden isimle kemik (`Bone - "%s"`) arar.
* `0x00065174` - **`Anim_PlayBlend`**: İki animasyon arasındaki harmanlamayı (blend) ve oynatmayı yönetir.

---

## 9. Hata Ayıklama ve Performans (Debug & Profiling)
* `0x00077c60` - **`Debug_RenderFPS`**: Ekrana anlık kare hızını ve parçacık sayısını çizer (`%2.2f fps %-5d Particles`).
* `0x000775a4` - **`Debug_TakeScreenshotTGA`**: VRAM tamponundan `.tga` formatında ekran görüntüsü yakalar.
* `0x0007774c` - **`Debug_CheckStackSize`**: Kalan yığın bellek miktarını (`nRemainingStackSize=%d`) denetler.
