# Değişiklikler

## 0.6.1 — 14 Kasım 2026

- Windows-1254 kaydedilmiş, Türkçe harf içeren bir satırda derleme mesajına ya da altındaki açıklamaya tıklayınca
  imleç artık hatanın tam yerine gider. Önceden satırdaki her Türkçe harf için bir karakter geride kalıyordu.

## 0.6.0 — 13 Kasım 2026

C Editor artık macOS 13 Ventura yüklenebilen her Mac’te çalışıyor: Apple çipli ve Intel işlemcili, 2018’den bu yana
çıkan bütün modeller ve 2017 modellerinin çoğu. Önceki sürümler yalnızca Apple çipli ve macOS 27 yüklü Mac’lerde açılıyordu.

**Her Mac için tek uygulama**

- Tek DMG iki işlemci türünü de içerir; indirirken seçim yapmak gerekmez.
- Programlarınıza eklenen kitaplık artık hazır gelmiyor: İlk ⌘R’de Mac’inizin kendi derleyicisiyle bir kez derlenir
  ve saklanır. Böylece eski bir macOS’taki eski derleyiciyle de uyuşur. İlk derleme bu yüzden bir kereliğine
  yaklaşık 0,1 saniye uzun sürer.
- Programlar, uygulamanın çalıştığı işlemci için derlenir.

**Okuldan gelen kod**

- Windows-1254 kaydedilmiş dosyada Türkçe harf, Windows’taki gibi tek karakterdir: `char harf = 'ş';` derlenir,
  `strlen("Şifre")` 5 verir, `char ad[6] = "Şeyma";` sığar. Yazdırılan ve klavyeden okunan harfler doğru görünür;
  `cout` ve `cin` için de böyledir. UTF-8 kaydedilmiş dosyalar eskisi gibi çalışır.
- Böyle bir dosyada `setlocale(LC_ALL, "Turkish")` sonrasında `toupper`, `isalpha` ve `strupr` Türkçe harfleri tanır.
- `system("cls & pause")` gibi `&` ve `&&` ile birleştirilmiş komutlar sırayla çalışır; `echo`, `> nul` ve
  `> nul 2>&1` Windows’taki gibidir. Önceden yalnızca ilk komut çalışıyordu.
- `rand()` Windows’taki sayıları verir ve `RAND_MAX` 32767’dir; `rand() * 100 / RAND_MAX` gibi hesaplar taşmaz.
- Dizinin sınırı aşılırsa program çıktısının altında satırıyla söylenir: `[C Editor] 6. satır: dizinin sınırı aşıldı.`
- `malloc` ve `new` ile alınan, değer verilmemiş bellek de 0 gibi doğru görünen bir değer yerine anlamsız bir sayı okur.
- Tam sayıyı sıfıra bölen program Intel ve Apple çipli Mac’lerde aynı biçimde durur ve nedeni aynı sözlerle yazılır.

**Açıklamalar**

- Windows-1254 dosyada bir adın içine yazılan Türkçe harf (`int yaş;`) ve kıvrık tırnak için Türkçe açıklama; hata
  satırı günlükte harfleriyle birlikte gösterilir.
- `i = i++ + i;` gibi aynı ifadede hem değişen hem okunan değişken için açıklama.
- Eski derleyicilerin aynı hata için kullandığı farklı sözler de tanınır, böylece açıklamalar onlarda da çıkar.
