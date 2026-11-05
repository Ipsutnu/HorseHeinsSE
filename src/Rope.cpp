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
#include "Rope.h"

namespace Rope
{
	namespace
	{
		RE::NiPointer<RE::NiNode> g_model;

		struct RopeSet
		{
			RE::ObjectRefHandle                                            horse;
			RE::NiPointer<RE::NiNode>                                      container;
			std::array<RE::NiPointer<RE::NiNode>, 2>                       rope;
			std::array<std::array<RE::NiPointer<RE::NiNode>, kSegments>, 2> seg;
			std::array<bool, 2>                                            drawnThisFrame{};
			std::array<bool, 2>                                            visible{};
		};
		std::unordered_map<RE::FormID, RopeSet> g_sets;

		RE::NiPointer<RE::NiNode> CloneModel()
		{
			if (!g_model) {
				return nullptr;
			}
			auto* obj = g_model->Clone();
			auto* node = obj ? obj->AsNode() : nullptr;
			return RE::NiPointer<RE::NiNode>(node);
		}

		RopeSet* Ensure(RE::Actor* a_horse)
		{
			auto* root3D = a_horse ? a_horse->Get3D() : nullptr;
			auto* root = root3D ? root3D->AsNode() : nullptr;
			if (!root || !g_model) {
				return nullptr;
			}
			auto& s = g_sets[a_horse->GetFormID()];
			s.horse = a_horse->GetHandle();
			if (!s.container) {
				s.container.reset(RE::NiNode::Create(2));
				s.container->name = "VRReins";
				for (int r = 0; r < 2; ++r) {
					s.rope[r].reset(RE::NiNode::Create(kSegments));
					s.rope[r]->SetAppCulled(true);
					s.container->AttachChild(s.rope[r].get(), true);
				}
			}
			// The horse's 3D is rebuilt when it reloads (cell change, etc.) - re-attach if needed.
			if (s.container->parent != root) {
				if (s.container->parent) {
					s.container->parent->DetachChild(s.container.get());
				}
				root->AttachChild(s.container.get(), true);
			}
			return &s;
		}

		// Rotation whose local +Y axis points along a_dir.
		RE::NiMatrix3 AlongY(RE::NiPoint3 a_dir)
		{
			a_dir.Unitize();
			RE::NiPoint3 up{ 0.0f, 0.0f, 1.0f };
			if (std::fabs(a_dir.Dot(up)) > 0.98f) {
				up = { 1.0f, 0.0f, 0.0f };
			}
			RE::NiPoint3 x = a_dir.Cross(up);
			x.Unitize();
			const RE::NiPoint3 z = x.Cross(a_dir);
			RE::NiMatrix3 m;
			for (int i = 0; i < 3; ++i) {
				m.entry[i][0] = (&x.x)[i];
				m.entry[i][1] = (&a_dir.x)[i];
				m.entry[i][2] = (&z.x)[i];
			}
			return m;
		}
	}

	bool Init()
	{
		RE::BSModelDB::DBTraits::ArgsType args{};
		for (const char* path : { "VRReins\\reinsegment.nif", "meshes\\VRReins\\reinsegment.nif" }) {
			RE::NiPointer<RE::NiNode> model;
			const auto err = RE::BSModelDB::Demand(path, model, args);
			if (err == RE::BSResource::ErrorCode::kNone && model) {
				g_model = model;
				logger::info("Rope: loaded {}", path);
				return true;
			}
		}
		logger::error("Rope: could not load meshes\\VRReins\\reinsegment.nif - reins will be invisible");
		return false;
	}

	void DrawPoints(RE::Actor* a_horse, int a_index, const RE::NiPoint3* a_pts, int a_count)
	{
		auto* s = Ensure(a_horse);
		if (!s || a_index < 0 || a_index > 1 || a_count < 2) {
			return;
		}
		auto& rope = s->rope[a_index];
		const RE::NiTransform toLocal = s->container->world.Invert();
        int used=0;
        for(int link=0;link<a_count-1 && used<kSegments;++link) {
            const auto delta=a_pts[link+1]-a_pts[link];
            const float length=delta.Length();
            if(length<0.001f)continue;
            const auto direction=delta*(1.f/length);
            const auto rotation=AlongY(direction);
            const int pieces=static_cast<int>(std::ceil(length/kSegmentLength));
            for(int piece=0;piece<pieces && used<kSegments;++piece,++used) {
                auto& node=s->seg[a_index][used];
                if(!node) {
                    node=CloneModel();
                    if(node)rope->AttachChild(node.get(),true);
                }
                if(!node)continue;
                RE::NiTransform world;
                // Overlap the final piece rather than scaling its width or leaving a gap.
                const float offset=(std::min)(piece*kSegmentLength,(std::max)(0.f,length-kSegmentLength));
                world.translate=a_pts[link]+direction*offset;
                world.rotate=rotation;
                world.scale=1.f;
                node->local=toLocal*world;
                node->SetAppCulled(false);
            }
        }
        for(int i=used;i<kSegments;++i)if(s->seg[a_index][i])s->seg[a_index][i]->SetAppCulled(true);
		rope->SetAppCulled(false);
		RE::NiUpdateData ctx{};
		rope->Update(ctx);
		s->drawnThisFrame[a_index] = true;
		s->visible[a_index] = true;
	}

	void EndFrame()
	{
		for (auto it = g_sets.begin(); it != g_sets.end();) {
			auto& s = it->second;
			for (int r = 0; r < 2; ++r) {
				if (s.visible[r] && !s.drawnThisFrame[r]) {
					s.rope[r]->SetAppCulled(true);
					s.visible[r] = false;
				}
				s.drawnThisFrame[r] = false;
			}
			// Drop containers for horses that no longer exist.
			if (!s.horse.get()) {
				if (s.container && s.container->parent) {
					s.container->parent->DetachChild(s.container.get());
				}
				it = g_sets.erase(it);
			} else {
				++it;
			}
		}
	}

	void Clear()
	{
		for (auto& [id, s] : g_sets) {
			if (s.container && s.container->parent) {
				s.container->parent->DetachChild(s.container.get());
			}
		}
		g_sets.clear();
	}
}
