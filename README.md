# monitoring-limbah-medis

Sistem monitoring limbah medis berbasis ESP32, Firebase, dan sensor jarak (VL53L0X, HC-SR04). Proyek ini digunakan untuk memantau status tangki limbah medis secara real-time dan mengirimkan data ke Firebase Realtime Database.

## Fitur
- Monitoring status tangki menggunakan sensor jarak VL53L0X dan HC-SR04.
- Pendeteksian level cairan limbah pada dua tangki.
- Notifikasi buzzer jika tangki kosong.
- Update data status perangkat dan sensor ke Firebase secara berkala.

## Wiring dan Pinout

| Nama        | Pin ESP32 | Keterangan                |
|-------------|-----------|---------------------------|
| Pompa 1     | 14        | Input opto-coupler        |
| Pompa 2     | 33        | Input opto-coupler        |
| Pompa 3     | 26        | Input opto-coupler        |
| Pompa 4     | 32        | Input opto-coupler        |
| Motor       | 27        | Input opto-coupler        |
| System      | 23        | Input opto-coupler        |
| Buzzer      | 13        | Output buzzer             |
| HC-SR04 #1  | 18 (Trig) | Sensor utama              |
| HC-SR04 #1  | 19 (Echo) | Sensor utama              |
| HC-SR04 #2  | 17 (Trig) | Sensor tangki tambahan    |
| HC-SR04 #2  | 5 (Echo)  | Sensor tangki tambahan    |
| VL53L0X     | 21 (SDA)  | Sensor I2C                |
| VL53L0X     | 22 (SCL)  | Sensor I2C                |

## Cara Kerja
1. ESP32 membaca status sensor dan perangkat (pompa, motor, sistem).
2. Jika kedua sensor mendeteksi tangki kosong, buzzer akan aktif dengan irama tertentu.
3. Data status dikirim ke Firebase setiap 3 detik.
4. Data dapat diakses melalui Firebase untuk aplikasi pemantauan.

## Catatan
- Pastikan sudah menginstal library yang diperlukan: WiFi, Wire, Adafruit_VL53L0X, NewPing, Firebase_ESP_Client.
- Update `ssid`, `password`, `DATABASE_URL`, dan `DATABASE_SECRET` sesuai konfigurasi masing-masing.
- **Gunakan board ESP32 dan library firebase dengan versi terbaru pada Arduino IDE untuk kompatibilitas maksimal.**
