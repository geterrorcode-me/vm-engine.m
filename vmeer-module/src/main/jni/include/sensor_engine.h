#ifndef VMEER_SENSOR_ENGINE_H
#define VMEER_SENSOR_ENGINE_H

#include <string>

namespace vmeer {
namespace sensor {

/**
 * Menginisialisasi hook untuk sistem sensor Android.
 * Memintercept ASensorEventQueue dan poll() di level native.
 */
void InitHooks();

/**
 * Menyuntikkan jitter/noise ke dalam data sensor mentah.
 * @param type Jenis sensor (misal: "accel", "gyro")
 * @param values Array float data sensor (X, Y, Z)
 * @param count Jumlah elemen dalam array
 */
void ApplySensorJitter(const std::string& type, float* values, int count);

/**
 * Menghentikan semua hook sensor saat VM dimatikan.
 */
void StopHooks();

} // namespace sensor
} // namespace vmeer

#endif // VMEER_SENSOR_ENGINE_H
