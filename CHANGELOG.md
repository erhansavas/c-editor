# Değişiklikler

## 0.5.4 — 3 Kasım 2026

- Türkçe harfli dosya adları (`ödev.cpp`) birleşik biçimde kaydediliyor; Windows’a ve başka bilgisayarlara gönderilince adları bozuk görünmüyor.

## 0.5.3 — 3 Kasım 2026

Windows’ta yazılan kod için düzeltmeler ve eksik fonksiyonlar.

**Program çalışırken**

- Program çökerse o ana kadar yazdırdıkları kaybolmuyor: `printf("Sonuç: ");` satırı ardından gelen çökmede de görünür.
- Tam sayıyı sıfıra bölmek (`a / b`, `a % b`) programı durdurur ve nedenini söyler. Önceden 0 sonucuyla sessizce devam ediyordu.
- Değer verilmemiş yerel değişkenler ve diziler 0 gibi doğru görünen bir değer yerine anlamsız bir sayı okur.
- `gets` çalışırken macOS’un İngilizce uyarısı artık ekrana çıkmıyor.

**Klavye**

- `getch`: ok tuşları 224 ve ardından tarama kodu (yukarı 72, aşağı 80, sol 75, sağ 77), Enter 13, Backspace 8, Ctrl+C 3 verir.

**Eklenen fonksiyonlar**

- `<conio.h>`: `kbhit`, `ungetch`, `putch`, `cputs`, `cprintf`, `cgets`, `cscanf`.
- `<string.h>`: `strrev`, `strupr`, `strlwr`, `stricmp`, `strcmpi`, `strnicmp`, `strset`, `strnset`, `memicmp`.
- `<stdlib.h>`: `itoa`, `ltoa`, `ultoa`, `lltoa`, `ulltoa`. `<stdio.h>`: `fputchar`, `fgetchar`.
- `<windows.h>`: `Beep`, `HANDLE`, `GetStdHandle`, `SetConsoleTextAttribute`, `SetConsoleCursorPosition`, `COORD`, `WORD`.
- `<malloc.h>`, `<io.h>`, `<process.h>`, `<direct.h>` (modsuz `mkdir`), `<dos.h>`.

**Derleme**

- Kendi `strrev`, `mergesort`, `heapsort`, `getline`, `random`, `read`, `write`, `close` fonksiyonunuz ya da `index` adlı değişkeniniz artık hata vermiyor.
- `std::system`, `std::setlocale` ve `std::fflush` artık makroya takılmıyor.
- Unutulan `<iostream>` için `cout`’ta ve `printf`, `rand`, `strlen` gibi adlarda başlık dosyasını söyleyen açıklama.

## 0.5.2 — 3 Kasım 2026

Hata düzeltmeleri; yeni özellik yok.

**Dosyalar**

- Başka bir programın değiştirdiği dosyanın üzerine artık sessizce yazılmıyor: Yeniden Yükle, Vazgeç ya da Üzerine Yaz
  sorulur.
- Değişmemiş dosya kaydetmede ve ⌘R’de yeniden yazılmıyor.
- Salt okunur dosyanın üzerine yazılmıyor; sembolik bağ, bağ olarak kalıyor.
- Geçersiz UTF-8 içeren BOM’lu dosyalar sessizce bozulmuyor. Windows-1254’ün tanımsız yedi baytı da dahil her bayt
  aynen korunuyor.
- Karışık satır sonlu dosya ancak onay verilirse tek biçime getiriliyor.

**Derleme ve hatalar**

- `if (x);`, `for (...);`, `while (...);` gibi boş gövde uyarıları hiç çıkmıyordu; artık çıkıyor.
- `ODEV.CPP` gibi büyük harfli uzantılar ve `.c++`, `.cp` C++ olarak derleniyor.
- Yazım hatalı fonksiyon adında (`prinf`) imleç o satıra gidiyor.
- Derleme sürerken yazı yazılırsa imleç yerinden oynamıyor, yanlış satır işaretlenmiyor.
- Yeni açıklamalar: bulunamayan başlık dosyası, fonksiyon dışında kalan kod, `return`’de eksik değer, atanamayan
  ifade, `const`, yersiz `break`/`continue`/`case`, kıvrık tırnaklar. Kıvrık tırnakta yanlış “silinmeli” önerisi kalktı.
- Bellek hatası açıklamasına olası neden eklendi; `assert` açıklamasındaki yanlış ifade düzeltildi.

**Konsol**

- Biten program nadiren konsolda hep çalışıyor görünüyordu; artık her bitiş yakalanıyor.
- Türkçe harf üzerinde Backspace programa yarım harf bırakmıyor.
- Uzun yapıştırmalar kesilmiyor; CRLF’li metin çift satır olmuyor.
- Menüde karşılığı olmayan ⌘ kısayolları programa harf olarak gitmiyor, biten konsolu kapatmıyor.
- Silme (`ESC[K`, `ESC[J`) kipleri doğru çalışıyor; imleç konumu görünen 25 satıra göre; pencere başlığı ve karakter
  kümesi dizileri ekrana yazılmıyor. Bitmeyen kaçış dizisi belleği doldurmuyor.
- Kapatılan sekmenin ya da uygulamanın geçici derleme klasörü siliniyor.

**Düzenleme**

- `/* */` yorumlarındaki parantezler ve komutlar girintiyi bozmuyor; `if (a) /* yorum */` girintileniyor.
- Yinele imleci doğru yere koyuyor.
- Geri Al ve Yinele yapılacak bir şey yokken menüde soluk.
- Kapatılan sekmeyi açma, zaten açık dosyaları atlıyor.
- Görünmeyen denetim karakterleri `�` olarak gösteriliyor; imleç hizası kaymıyor.

## 0.5.1 — 24 Ekim 2026

- `}` işaretinin önünde Enter’a basınca `}` kendi `{` satırıyla hizalı olarak yeni satıra geçer; `{}` arasında
  Enter’a basınca imleç aradaki girintili satıra gelir. Önceden `}` yeni satırın girintisinde kalıyordu.
- Girintide Backspace bir kademe (4 boşluk) birden siler.
- Backspace ya da Delete ile silinen boş satır boşluklarını da götürür; alttaki satır, örneğin `}`, kendi
  hizasında kalır.
- README sadeleşti: üç ekran görüntüsü, hepsi koyu temada; sürüm rozeti GitHub’daki son sürümü gösterir.

## 0.5 — 24 Ekim 2026

- Koşulunda tırnak içinde parantez olan `if`/`while` satırları da (örneğin `if (c == ')')`) Enter’dan sonra
  girintilenir.
- Boş satıra yazılan `}`, tırnak içindeki ya da yorumdaki süslü parantezlere (örneğin `printf("{")`) değil,
  kendi `{` satırına hizalanır.

## 0.4 — 24 Ekim 2026

- Sağ üstteki ▶ düğmesi çerçevesiz, düz yeşil.
- Çökme mesajı iki satır: ne olduğu ve nedeni; 80 sütunluk konsolda artık bölünmez.
- README baştan yazıldı; yeni ekran görüntüleri: not programı, yazarken girinti, hatalar ve çökme.

## 0.3 — 24 Ekim 2026

- Süslü parantezsiz `if`/`else`/`for`/`while`/`do` sonrası otomatik girinti, tek komutun ardından geri dönüş;
  kendi satırına yazılan `{` üstündeki `if`/`else` ile hizalanır.
- Girinti düzeylerini gösteren soluk dikey çizgiler.
- Metinlerdeki kaçış dizileri (`\n`, `\t`, `\"`, `\\`…) ayrı renkte; anahtar kelimeler metinlerden daha
  kolay ayrılan bir renkte.
- Açıklamalar düzeltmeyi uygulamaz, ne yazılması gerektiğini söyler (“;” yazılmalı.); tıklamak o yere götürür.

## 0.2 — 23 Ekim 2026

- Program çıktısı ayrı pencere yerine kodun altındaki panelde; derleme mesajları paneli içeriğine göre boyutlanır.
- ⌘1…⌘8 sekme seçer, ⌘9 son sekmeye gider; ⌘⇧T kapatılan sekmeyi yeniden açar.
- Durum çubuğunda yalnızca sütun gösterilir.
- `{` sonrası otomatik girinti; boş satıra yazılan `}` kendi `{` satırıyla hizalanır.
- Anahtar kelimeler ve türler kalın; ters eğik çizgiyle biten `//` yorumunun alt satırı da yorum renginde gösterilir.
- `\a` tam güçte bir uyarı sesi çalar.
- Sık derleyici hataları için yeni Türkçe açıklamalar.

## 0.1 — 22 Ekim 2026

İlk sürüm.
