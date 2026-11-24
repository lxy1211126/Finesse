#include "super_feature/finesse_sf_generator.h"
#include "super_feature/ntransform_sf_generator.h"

#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {

void print(const char* label, const finesse::SuperFeatures& result) {
    std::cout << label << '\n';
    for (std::size_t i = 0; i < result.values.size(); ++i) {
        std::cout << "sf" << i << " features=";
        for (const auto feature : result.groups[i]) {
            std::cout << " 0x" << std::hex << std::setw(8)
                      << std::setfill('0') << feature;
        }
        std::cout << " hash=0x" << std::setw(8) << result.values[i]
                  << std::dec << '\n';
    }
}

}  // namespace

int main() {
    const finesse::FeatureVector features = {
        0xff9ab74eU, 0xfff82845U, 0xfff1da26U,
        0xff7f156cU, 0xff12c814U, 0xffe3735cU,
        0xff32bd8eU, 0xfff9b8d2U, 0xffe87e52U,
        0xfff93729U, 0xffe2fcafU, 0xff16ecf3U,
    };
    print("finesse",
          finesse::FinesseSfGenerator{}.generate(features));
    print("ntransform",
          finesse::NTransformSfGenerator{}.generate(features));
}
