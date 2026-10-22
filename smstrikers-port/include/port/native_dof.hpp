#pragma once

namespace port::native_dof
{
// GLX submission-thread state: mirrors the projection registers read by DOF.
// Reset only at GL initialization; sender resets retain the last projection.
struct Projection
{
    float scale;
    float offset;
    bool perspective;
};
void reset_projection() noexcept;
Projection projection() noexcept;
}
