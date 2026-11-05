// Derived from VR Reins, Copyright (C) 2026 SollyStones.
// Non-VR adaptation. SPDX-License-Identifier: GPL-3.0-or-later
		std::string Lower(std::string_view a_s)
		{
			std::string s(a_s);
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		bool IsHorse(RE::Actor* a_actor)
		{
			if (!a_actor || a_actor->IsPlayerRef()) {
				return false;
			}
			auto* race = a_actor->GetRace();
			if (!race) {
				return false;
			}
			if (race->HasKeywordString("ActorTypeHorse")) {
				return true;
			}
			const char* edid = race->GetFormEditorID();
			return Lower(edid ? edid : "").find("horse") != std::string::npos;
		}

		NiPoint3 Forward(float a_angleZ) { return { std::sin(a_angleZ), std::cos(a_angleZ), 0.0f }; }
		NiPoint3 Right(float a_angleZ) { return { std::cos(a_angleZ), -std::sin(a_angleZ), 0.0f }; }

		RE::NiAVObject* FindNode(RE::NiAVObject* a_obj, const std::function<bool(const std::string&)>& a_pred, int a_depth = 0)
		{
			if (!a_obj || a_depth > 40) {
				return nullptr;
			}
			if (a_pred(Lower(a_obj->name.c_str()))) {
				return a_obj;
			}
			if (auto* node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					if (auto* hit = FindNode(child.get(), a_pred, a_depth + 1)) {
						return hit;
					}
				}
			}
			return nullptr;
		}

		void DumpNodes(RE::NiAVObject* a_obj, int a_depth, int& a_count)
		{
			if (!a_obj || a_depth > 40 || a_count > 400) {
				return;
			}
			++a_count;
			logger::info("  {}{}", std::string(a_depth * 2, ' '), a_obj->name.c_str());
			if (auto* node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					DumpNodes(child.get(), a_depth + 1, a_count);
				}
			}
		}

		struct Anatomy
		{
			NiPoint3 mouth;    // centre of the bit
			NiPoint3 withers;  // base of the neck, in front of the saddle
			NiPoint3 rest;     // where the loop lies when nobody holds it: on top of the base of the neck
			NiPoint3 fwd;      // horse facing, horizontal
			NiPoint3 right;    // horse's right, horizontal
			NiPoint3 LeftRing() const { return mouth - right * kBitHalfWidth; }
			NiPoint3 RightRing() const { return mouth + right * kBitHalfWidth; }
		};

		Anatomy GetAnatomy(RE::Actor* a_horse)
		{
			const auto&    cfg = Config::Get();
			const float    a = a_horse->GetAngleZ();
			const NiPoint3 pos = a_horse->GetPosition();
			Anatomy        out;
			out.fwd = Forward(a);
			out.right = Right(a);
			// Fallback guesses for a standard horse, used if no head bone is found.
			out.mouth = pos + out.fwd * 95.0f + NiPoint3{ 0, 0, 125.0f };
			out.withers = pos + out.fwd * 35.0f + NiPoint3{ 0, 0, 125.0f };
			out.rest = out.withers + NiPoint3{ 0.0f, 0.0f, 20.0f };

			auto* root = a_horse->Get3D();
			if (!root) {
				return out;
			}

			// Bone lookup is cached per horse and redone only when its 3D is rebuilt.
			struct Bones
			{
				RE::NiAVObject*               root = nullptr;
				RE::NiPointer<RE::NiAVObject> head, neck, neckBase, lip, jaw;
			};
			static std::unordered_map<RE::FormID, Bones> cache;
			auto& bones = cache[a_horse->GetFormID()];
			if (bones.root != root) {
				bones = Bones{ root };
				static std::unordered_map<RE::FormID, bool> dumped;
				auto* race = a_horse->GetRace();
				if (cfg.dumpHorseNodes && race && !dumped[race->GetFormID()]) {
					dumped[race->GetFormID()] = true;
					logger::info("Horse skeleton for race '{}' (set sHeadNode in HorseReins.ini if the reins sit in the wrong place):",
						race->GetFormEditorID() ? race->GetFormEditorID() : "?");
					int count = 0;
					DumpNodes(root, 0, count);
				}
				auto byName = [&](const char* a_name) { return root->GetObjectByName(RE::BSFixedString(a_name)); };
				RE::NiAVObject* found = cfg.headNode.empty() ? nullptr : byName(cfg.headNode.c_str());
				if (!found) {
					found = byName("HorseScull");  // vanilla horse skeleton (sic)
				}
				if (!found) {
					found = FindNode(root, [](const std::string& n) {
						const bool skull = n.find("skull") != std::string::npos || n.find("scull") != std::string::npos ||
						                   n.find("head") != std::string::npos;
						return skull && n.find("track") == std::string::npos && n.find("target") == std::string::npos &&
						       n.find("look") == std::string::npos;
					});
				}
				if (found) {
					// Neck: the head bone's parent, then up the chain while names still say "neck".
					RE::NiAVObject* neckBase = found->parent;
					while (neckBase && neckBase->parent && Lower(neckBase->parent->name.c_str()).find("neck") != std::string::npos) {
						neckBase = neckBase->parent;
					}
					bones.head.reset(found);
					bones.neck.reset(found->parent);
					bones.neckBase.reset(neckBase);
					bones.lip.reset(byName("HorseFrontLip"));
					bones.jaw.reset(byName("HorseJaw"));
					if (cfg.debugLog) {
						logger::info("Horse {:08X}: head bone '{}', neck base '{}', mouth from {}", a_horse->GetFormID(), found->name.c_str(),
							neckBase ? neckBase->name.c_str() : "(none)", bones.lip && bones.jaw ? "lip + jaw bones" : "estimate");
					}
				} else if (cfg.debugLog) {
					logger::info("Horse {:08X}: no head bone found, using estimated rein position", a_horse->GetFormID());
				}
			}
			auto* head = bones.head.get();
			if (!head) {
				return out;
			}
			auto*          neck = bones.neck.get();
			auto*          neckBase = bones.neckBase.get();
			const NiPoint3 h = head->world.translate;
			const NiPoint3 up{ 0.0f, 0.0f, 1.0f };
			if (bones.lip && bones.jaw) {
				// The bit sits in the corners of the mouth: a third of the way from the front lip back to the jaw hinge.
				const NiPoint3 lip = bones.lip->world.translate;
				const NiPoint3 jaw = bones.jaw->world.translate;
				out.mouth = lip + (jaw - lip) * 0.35f;
				NiPoint3 headFwd = lip - h;
				headFwd.z = 0.0f;
				if (headFwd.Unitize() > 0.001f) {
					out.right = headFwd.Cross(up);  // reins follow the head when the horse turns it
					out.right.Unitize();
				}
			} else {
				NiPoint3 dir = neck ? h - neck->world.translate : out.fwd;
				dir.z *= 0.3f;  // the skull points mostly forward even when the neck is raised
				if (dir.Unitize() < 0.001f) {
					dir = out.fwd;
				}
				out.mouth = h + dir * cfg.mouthForward - NiPoint3{ 0, 0, cfg.mouthDown };
			}
			if (neckBase && neckBase != head) {
				out.withers = neckBase->world.translate;
			}
			out.rest = out.withers + NiPoint3{ 0.0f, 0.0f, cfg.restUp } + out.fwd * cfg.restForward;
			return out;
		}

		RE::Actor* LookupHorse(RE::FormID a_id)
		{
			auto* actor = a_id ? RE::TESForm::LookupByID<RE::Actor>(a_id) : nullptr;
			return actor && actor->Is3DLoaded() && !actor->IsDead() ? actor : nullptr;
		}

