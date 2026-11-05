// Derived from VR Reins, Copyright (C) 2026 SollyStones.
// Non-VR adaptation. SPDX-License-Identifier: GPL-3.0-or-later
		bool TieLayer(RE::COL_LAYER a_l)
		{
			using L = RE::COL_LAYER;
			switch (a_l) {
			case L::kStatic:
			case L::kAnimStatic:
			case L::kTrees:
			case L::kProps:
			case L::kTransparent:
				return true;
			case L::kTerrain:
			case L::kGround:
				return Config::Get().tieToTerrain;
			default:
				return false;
			}
		}

		// Looks for something solid and fixed right next to the hand.
		std::optional<NiPoint3> TieProbe(RE::PlayerCharacter* a_player, const NiPoint3& a_hand)
		{
			auto* cell = a_player->GetParentCell();
			auto* world = cell ? cell->GetbhkWorld() : nullptr;
			if (!world) {
				return std::nullopt;
			}
			RE::CFilter f{};
			a_player->GetCollisionFilterInfo(f);
			const std::uint32_t   filter = (f.filter & 0xFFFF0000) | static_cast<std::uint32_t>(RE::COL_LAYER::kCharController);
			const float           s = RE::bhkWorld::GetWorldScale();
			const float           reach = Config::Get().tieReach;
			static const NiPoint3 dirs[] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 },
				{ 0.7f, 0.7f, 0 }, { -0.7f, 0.7f, 0 }, { 0.7f, -0.7f, 0 }, { -0.7f, -0.7f, 0 } };
			std::optional<NiPoint3> best;
			float                   bestFrac = reach+1.f;
			for (const auto& d : dirs) {
				const NiPoint3 from = a_hand-d*reach;
                const NiPoint3 to = a_hand+d*reach;
				RE::bhkPickData pick;
				pick.rayInput.from = RE::hkVector4(from.x * s, from.y * s, from.z * s, 0.0f);
				pick.rayInput.to = RE::hkVector4(to.x * s, to.y * s, to.z * s, 0.0f);
				pick.rayInput.filterInfo.filter = filter;
				world->PickObject(pick);
				if (!pick.rayOutput.HasHit() || !pick.rayOutput.rootCollidable) {
					continue;
				}
				if (!TieLayer(pick.rayOutput.rootCollidable->GetCollisionLayer())) {
					continue;
				}
				auto* ref = RE::TESHavokUtilities::FindCollidableRef(*pick.rayOutput.rootCollidable);
				if (ref && ref->Is(RE::FormType::ActorCharacter)) {
					continue;
				}
				const auto contact=from+(to-from)*pick.rayOutput.hitFraction;
                const float distance=contact.GetDistance(a_hand);
                if (distance<=reach && distance < bestFrac) {
					bestFrac = distance;
					best = contact;
				}
			}
			return best;
		}

		void Untie(RE::Actor* a_horse)
		{
			if (!a_horse)return;
            auto tie=g_tied.find(a_horse->GetFormID());
            if(tie!=g_tied.end()) {
                if(auto* marker=RE::TESForm::LookupByID<RE::TESObjectREFR>(tie->second.marker)){marker->Disable();marker->SetDelete(true);}
                attached.erase(a_horse->GetFormID());g_tied.erase(tie);
				ReleaseMovement(a_horse);
				if (Config::Get().debugLog) {
					logger::info("Untied {:08X}", a_horse->GetFormID());
				}
			}
		}

		void Tie(RE::Actor* a_horse, int a_index, const NiPoint3& a_point)
		{
            // Original VR Reins sequence: lock before cancelling the follow order.
            Papyrus::SetDontMove(a_horse,true);
            Papyrus::SetRestrained(a_horse,true);
            Papyrus::ClearKeepOffsetFromActor(a_horse);
            Papyrus::CallActor(a_horse,"EvaluatePackage");
			g_tied[a_horse->GetFormID()] = { a_index, a_point };
			if (Config::Get().debugLog) {
				logger::info("Tied {:08X} (rein point {}) at ({:.0f}, {:.0f}, {:.0f})", a_horse->GetFormID(), a_index, a_point.x, a_point.y,
					a_point.z);
			}
		}

        void StartLeading(RE::PlayerCharacter* player, RE::Actor* horse)
        {
            if(g_tied.contains(horse->GetFormID()))return;
            g_led=horse->GetFormID();leadTurnVelocity=0;
            const auto& cfg=Config::Get();
            Papyrus::KeepOffsetFromActor(horse,player,0.f,0.f,0.f,cfg.leadCatchUpRadius,cfg.leadFollowDistance);
        }
