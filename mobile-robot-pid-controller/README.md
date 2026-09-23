# Mobil Robot PID Kontrolcü

Quadrature encoder interrupt okuma, PID hız kontrolü ve JSON seri komut ayrıştırma içeren diferansiyel sürüş motor kontrol örneğidir. Lineer/açısal hız komutlarını veya doğrudan RPM hedeflerini alır; RPM ve pozisyon geri bildirimi üretir.

## Öne Çıkanlar

- `attachInterrupt` ile encoder ISR yönetimi
- 20 Hz kontrol döngüsü
- Anti-windup integral sınırlama
- Diferansiyel sürüş kinematiği
- JSON seri haberleşme arayüzü
