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

// The rein: one loop of leather from the left side of the bit to the right side, simulated with Verlet
// integration and fixed-length links (it never stretches). Both ends are fixed to the bit; up to two
// points can be pinned to hands or a tie point. The rest swings under gravity and drapes on the ground.

namespace RopeSim
{
	constexpr int kPoints = 25;  // 24 links; point 0 = left bit ring, point 24 = right bit ring
	constexpr int kMid = kPoints / 2;

	struct Pin
	{
		int          index = -1;  // -1 = not pinned
		RE::NiPoint3 pos;
	};


    // One continuous convex shell, with segment contacts instead of fixed rope indices.
    struct NeckSurface {
        RE::NiPoint3 center,forward,right,up,radius;
        float strength=1.f;
        RE::NiPoint3 Local(const RE::NiPoint3& point) const {
            auto d=point-center;return {d.Dot(forward)/radius.x,d.Dot(right)/radius.y,d.Dot(up)/radius.z};
        }
        RE::NiPoint3 Vector(const RE::NiPoint3& local) const {
            return forward*(local.x*radius.x)+right*(local.y*radius.y)+up*(local.z*radius.z);
        }
        RE::NiPoint3 OutsideAnchor(RE::NiPoint3 point) const {
            auto q=Local(point);
            if(q.Dot(q)<1.f){q.z=std::sqrt((std::max)(0.f,1.f-q.x*q.x-q.y*q.y))+0.04f;return center+Vector(q);}
            return point;
        }
        template<class IsPinned>
        void Resolve(std::array<RE::NiPoint3,kPoints>& now,
                     std::array<RE::NiPoint3,kPoints>& normals,
                     std::array<bool,kPoints>& contacts,IsPinned pinned) const {
            for(int i=0;i<kPoints-1;++i) {
                const auto a=Local(now[i]),delta=Local(now[i+1])-a;
                const float length2=delta.Dot(delta);
                const float t=length2>0.00001f?std::clamp(-a.Dot(delta)/length2,0.f,1.f):0.f;
                const auto q=a+delta*t;
                const float squared=q.Dot(q);
                if(squared>=1.f)continue;
                // Use the actual surface normal. Do not teleport contact to another side
                // or force its height: those tangential corrections fought the length solver.
                auto normal=forward*(q.x/radius.x)+right*(q.y/radius.y)+up*(q.z/radius.z);
                float gradient=normal.Unitize();
                if(gradient<0.0001f){normal=right*(i<kMid?-1.f:1.f);gradient=1.f/radius.y;}
                // Gentle, local bias only at the crest, so each half tends to choose its own flank.
                // Away from this narrow central region the natural sliding direction is untouched.
                if(q.z>0.f && std::abs(q.y)<0.35f) {
                    const float preference=(1.f-std::abs(q.y)/0.35f)*0.18f;
                    normal=normal+right*((i<kMid?-1.f:1.f)*preference);
                    normal.Unitize();
                }
                const float depth=std::clamp((1.f-squared)/(2.f*gradient),0.f,2.f);
                const float l=pinned(i)?0.f:1.f,r=pinned(i+1)?0.f:1.f;
                const float denominator=l*(1-t)*(1-t)+r*t*t;
                if(denominator<0.05f)continue;
                const auto correction=normal*(depth*strength*0.35f/denominator);
                auto da=correction*(l*(1-t)),db=correction*(r*t);
                const float maximum=(std::max)(da.Length(),db.Length());
                if(maximum>0.5f){da=da*(0.5f/maximum);db=db*(0.5f/maximum);}
                now[i]=now[i]+da;now[i+1]=now[i+1]+db;
                if(l){normals[i]=normal;contacts[i]=true;}
                if(r){normals[i+1]=normal;contacts[i+1]=true;}
            }
        }
    };

	struct Rope
	{
		std::array<RE::NiPoint3, kPoints> p{};
		std::array<RE::NiPoint3, kPoints> prev{};
		bool                              init = false;

		// Lays the loop as a U hanging between the two rings (used on first sight and after teleports).
		void Reset(const RE::NiPoint3& a_left, const RE::NiPoint3& a_right, float a_link, float a_ground)
		{
			const float span = a_left.GetDistance(a_right);
			const float half = a_link * (kPoints - 1) * 0.5f;
			const float drop = (std::max)(half - span * 0.5f, 0.0f);
			for (int i = 0; i < kPoints; ++i) {
				const float t = static_cast<float>(i) / (kPoints - 1);
				RE::NiPoint3 q = a_left + (a_right - a_left) * t;
				q.z -= drop * std::sin(t * 3.14159265f);
				q.z = (std::max)(q.z, a_ground);
				p[i] = q;
			}
			prev = p;
			init = true;
		}

		// How far point a_i can be from each ring, along the leather.
		static float ReachLeft(int a_i, float a_link) { return a_link * a_i; }
		static float ReachRight(int a_i, float a_link) { return a_link * (kPoints - 1 - a_i); }

		// Pulls a_pos back inside the region point a_i can actually reach, given the rein's fixed length.
		static RE::NiPoint3 ClampToReach(RE::NiPoint3 a_pos, int a_i, const RE::NiPoint3& a_left, const RE::NiPoint3& a_right, float a_link)
		{
			auto limit = [](RE::NiPoint3& a_q, const RE::NiPoint3& a_c, float a_r) {
				RE::NiPoint3 d = a_q - a_c;
				const float  len = d.Length();
				if (len > a_r && len > 1e-4f) {
					a_q = a_c + d * (a_r / len);
				}
			};
			for (int k = 0; k < 4; ++k) {
				limit(a_pos, a_left, ReachLeft(a_i, a_link));
				limit(a_pos, a_right, ReachRight(a_i, a_link));
			}
			return a_pos;
		}

		// Index of the point closest to a_pos (excluding the two rings), and how far it is.
		std::pair<int, float> Closest(const RE::NiPoint3& a_pos) const
		{
			int   best = kMid;
			float bestD = (std::numeric_limits<float>::max)();
			for (int i = 1; i < kPoints - 1; ++i) {
				const float d = p[i].GetDistance(a_pos);
				if (d < bestD) {
					bestD = d;
					best = i;
				}
			}
			return { best, bestD };
		}

		// Advances the simulation. a_link is the length of one link (rein length per side / 12).
		// Pins are clamped to what the fixed-length rein can reach; the clamped positions are written back.
		void Step(const RE::NiPoint3& a_left, const RE::NiPoint3& a_right, Pin& a_pinA, Pin& a_pinB, float a_link, float a_ground, float a_dt,const NeckSurface* shell=nullptr)
		{
			if (!init || p[0].GetDistance(a_left) > 300.0f) {
				Reset(a_left, a_right, a_link, a_ground);
			}
			// The rope can't reach farther than its length: keep the pins where the leather can actually be.
			for (Pin* pin : { &a_pinA, &a_pinB }) {
				if (pin->index <= 0 || pin->index >= kPoints - 1) {
					pin->index = -1;
				}
			}
			if (a_pinA.index > 0 && a_pinB.index > 0) {
				if (a_pinA.index == a_pinB.index) {
					a_pinB.index = -1;
				} else {
					// The leather between two hands is a fixed length too.
					const float  maxGap = a_link * std::abs(a_pinA.index - a_pinB.index);
					RE::NiPoint3 d = a_pinB.pos - a_pinA.pos;
					const float  len = d.Length();
					if (len > maxGap && len > 1e-4f) {
						a_pinB.pos = a_pinA.pos + d * (maxGap / len);
					}
					a_pinB.pos = ClampToReach(a_pinB.pos, a_pinB.index, a_left, a_right, a_link);
				}
			}

            for (const Pin* pin : { &a_pinA, &a_pinB }) {
                if(pin->index>0) a_link=(std::max)(a_link,(std::max)(
                    pin->pos.GetDistance(a_left)/pin->index,
                    pin->pos.GetDistance(a_right)/(kPoints-1-pin->index)));
            }

			a_dt = std::clamp(a_dt, 0.0f, 1.0f / 30.0f);
			constexpr float kGravity = 686.0f;  // 9.8 m/s^2 in game units
			constexpr float kDamping = 0.985f;
			constexpr int   kSubsteps = 2;
			constexpr int   kIterations = 40;
			const float     h = a_dt / kSubsteps;

			auto pinned = [&](int i) {
                if(i==0 || i==kPoints-1 || i==a_pinA.index || i==a_pinB.index)return true;
                return false;
            };
			auto applyPins = [&]() {
				p[0] = a_left;
				p[kPoints - 1] = a_right;
				if (a_pinA.index > 0) {
					p[a_pinA.index] = a_pinA.pos;
				}
				if (a_pinB.index > 0) {
					p[a_pinB.index] = a_pinB.pos;
				}
			};

            for (int s = 0; s < kSubsteps; ++s) {
                std::array<RE::NiPoint3,kPoints> velocities{},normals{};
                std::array<bool,kPoints> contacts{};
				for (int i = 0; i < kPoints; ++i) {
					const RE::NiPoint3 v = (p[i] - prev[i]) * kDamping;
					prev[i] = p[i];
					if (!pinned(i)) {
                        velocities[i]=v+RE::NiPoint3{0,0,-kGravity*h*h};
                        p[i]=p[i]+velocities[i];
					}
				}
				applyPins();
				for (int it = 0; it < kIterations; ++it) {
					// Alternate sweep direction so tension spreads evenly from both rings.
					for (int k = 0; k < kPoints - 1; ++k) {
						const int    i = (it & 1) ? (kPoints - 2 - k) : k;
						RE::NiPoint3 d = p[i + 1] - p[i];
						const float  len = d.Length();
						if (len < 1e-4f) {
							continue;
						}
						const bool fa = pinned(i), fb = pinned(i + 1);
						if (fa && fb) {
							continue;
						}
						const float diff = (len - a_link) / len;
						const float wa = fa ? 0.0f : (fb ? 1.0f : 0.5f);
						const float wb = fb ? 0.0f : (fa ? 1.0f : 0.5f);
						p[i] = p[i] + d * (diff * wa);
						p[i + 1] = p[i + 1] - d * (diff * wb);
					}
					for (int i = 1; i < kPoints - 1; ++i) {
						if (!pinned(i) && p[i].z < a_ground) {
							p[i].z = a_ground;
							prev[i].x = p[i].x - (p[i].x - prev[i].x) * 0.5f;  // ground friction
							prev[i].y = p[i].y - (p[i].y - prev[i].y) * 0.5f;
						}
					}
                    if(shell)shell->Resolve(p,normals,contacts,pinned);
                    applyPins();
                }
                // Contact corrections are positional, not extra kinetic energy.
                // Remove inward velocity once per substep, retaining gentle tangential sliding.
                for(int i=1;i<kPoints-1;++i)if(contacts[i] && !pinned(i)) {
                    auto velocity=velocities[i];
                    const float inward=velocity.Dot(normals[i]);
                    if(inward<0)velocity=velocity-normals[i]*inward;
                    prev[i]=p[i]-velocity*0.96f;
                }
            }
			// Pinned points don't carry velocity into the next frame.
			for (int i = 0; i < kPoints; ++i) {
				if (pinned(i)) {
					prev[i] = p[i];
				}
			}
		}
	};
}
