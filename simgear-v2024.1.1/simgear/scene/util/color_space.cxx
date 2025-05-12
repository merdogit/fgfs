// color_space.cxx - Color space conversion utilities
// Copyright (C) 2023 Fernando García Liñán
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "color_space.hxx"

namespace simgear {

SGVec3f
eotf_sRGB(const SGVec3f &in)
{
    SGVec3f out;
    for (int i = 0; i < 3; ++i) {
        float c = in[i];
        if (c <= 0.0031308f) {
            out[i] = 12.92f * c;
        } else {
            out[i] = 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        }
    }
    return out;
}

SGVec3f
eotf_inverse_sRGB(const SGVec3f &in)
{
    SGVec3f out;
    for (int i = 0; i < 3; ++i) {
        float c = in[i];
        if (c <= 0.04045f) {
            out[i] = c / 12.92f;
        } else {
            out[i] = powf((c + 0.055f) / 1.055f, 2.4f);
        }
    }
    return out;
}

} // namespace simgear
