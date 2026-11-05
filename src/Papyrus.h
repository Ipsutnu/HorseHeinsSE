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

// Calls native Papyrus Actor functions on a horse from C++ by dispatching them through the script VM.
// This works on any actor, with no script attached and no plugin (.esp) needed.

namespace Papyrus
{
	template <class... Args>
	inline bool CallActor(RE::Actor* a_actor, const char* a_fn, Args&&... a_args)
	{
		if (!a_actor) {
			return false;
		}
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return false;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return false;
		}
		const auto handle = policy->GetHandleForObject(RE::Actor::FORMTYPE, a_actor);
		if (handle == policy->EmptyHandle()) {
			return false;
		}
		auto* args = RE::MakeFunctionArguments(std::forward<Args>(a_args)...);
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
		const bool ok = vm->DispatchMethodCall(handle, RE::BSFixedString("Actor"), RE::BSFixedString(a_fn), args, result);
		if (!ok) {
			logger::warn("Papyrus: Actor.{} dispatch failed", a_fn);
		}
		return ok;
	}

	// Horse keeps a fixed offset from the target (offsets are in the target's local space).
	inline void KeepOffsetFromActor(RE::Actor* a_horse, RE::Actor* a_target, float a_x, float a_y, float a_z, float a_catchUp, float a_follow)
	{
		CallActor(a_horse, "KeepOffsetFromActor", std::move(a_target), std::move(a_x), std::move(a_y), std::move(a_z),
			0.0f, 0.0f, 0.0f, std::move(a_catchUp), std::move(a_follow));
	}

	inline void ClearKeepOffsetFromActor(RE::Actor* a_horse) { CallActor(a_horse, "ClearKeepOffsetFromActor"); }

	// Restrained actors cannot walk away but still idle (graze, shake their head, ...).
	inline void SetRestrained(RE::Actor* a_horse, bool a_restrained) { CallActor(a_horse, "SetRestrained", std::move(a_restrained)); }

	// The actor's AI won't move it from where it stands (it can still turn its head, graze and idle).
	inline void SetDontMove(RE::Actor* a_horse, bool a_dontMove) { CallActor(a_horse, "SetDontMove", std::move(a_dontMove)); }

}
