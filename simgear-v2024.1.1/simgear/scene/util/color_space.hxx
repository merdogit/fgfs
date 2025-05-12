// color_space.hxx - Color space conversion utilities
// Copyright (C) 2023 Fernando García Liñán
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <simgear/math/SGMath.hxx>

namespace simgear {

/// Transform a linear sRGB color to sRGB (gamma correction)
SGVec3f eotf_sRGB(const SGVec3f &in);
/// Transform an sRGB color to linear sRGB
SGVec3f eotf_inverse_sRGB(const SGVec3f &in);

} // namespace simgear
