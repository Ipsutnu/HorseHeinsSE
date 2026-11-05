// VR Reins - physical horse reins for Skyrim VR
// Copyright (C) 2026 SollyStones
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details. You should have received a copy of the GNU General Public License along with this program
// (LICENSE.txt). If not, see <https://www.gnu.org/licenses/>.
#pragma once

// Draws reins as a chain of short strap-segment meshes laid through a list of points.
// Each horse gets one container node (attached to the horse's 3D) holding two reins (left and right).

namespace Rope
{
	constexpr int   kSegments = 256;     // lazily allocated fixed-size pieces; extra capacity for taut reins
	constexpr float kSegmentLength = 7.5f;  // must match tools/make_assets.cpp

	bool Init();  // loads the segment mesh; returns false if it is missing

	// Draws rein a_index (0 = left, 1 = right) of a_horse through a_count world-space points.
	void DrawPoints(RE::Actor* a_horse, int a_index, const RE::NiPoint3* a_pts, int a_count);

	// Hides every rein that was not drawn since the last call (call once per frame after drawing).
	void EndFrame();
	void Clear();
}
