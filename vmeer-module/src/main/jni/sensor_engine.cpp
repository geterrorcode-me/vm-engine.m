#include "include/vmeer_context.h"
#include <random>

namespace vmeer {
namespace sensor {

void ApplyJitter(float* values) {
    // Get master seed to seed the PRNG
    const auto& seed = RuntimeContext::Get().GetMasterSeed();
    
    // Logic: Apply Gaussian Noise
    // std::mt19937 gen(seed_to_int(seed));
    // values[0] += dist(gen);
}

} // namespace sensor
} // namespace vmeer
