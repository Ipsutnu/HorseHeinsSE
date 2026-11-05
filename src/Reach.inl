// Experimental third-person reach. Rotations only; no bone translations or scaling.
namespace ReachPose {
struct Edit {RE::NiPointer<RE::NiAVObject> node;RE::NiMatrix3 base,applied;};
std::array<Edit,2> edits;
RE::NiPointer<RE::NiAVObject> root;
float blend=0;
NiPoint3 localGoal{};
bool Near(const RE::NiMatrix3& a,const RE::NiMatrix3& b) {
 for(int i=0;i<3;++i)for(int j=0;j<3;++j)
  if(std::abs(a.entry[i][j]-b.entry[i][j])>0.00001f)return false;
 return true;
}
void Refresh(RE::NiAVObject* node,int depth=0) {
 if(!node || depth>16)return;
 RE::NiUpdateData data{};node->UpdateWorldData(&data);
 if(auto* branch=node->AsNode())for(auto& child:branch->children)if(child)Refresh(child.get(),depth+1);
}
void Restore() {
 auto* player=RE::PlayerCharacter::GetSingleton();
 if(player && player->Get3D(false)==root.get()) {
  for(auto& edit:edits)if(edit.node && Near(edit.node->local.rotate,edit.applied))
   edit.node->local.rotate=edit.base;
  if(edits[0].node)Refresh(edits[0].node.get());
 }
 edits={};root.reset();
}
void Reset() {Restore();blend=0;localGoal={};}
void Apply(RE::PlayerCharacter* player,float dt,bool active,const NiPoint3& goal) {
 RE::NiPointer<RE::Actor> mount;
 if(player->GetMount(mount) && mount){blend=0;return;}
 auto* camera=RE::PlayerCamera::GetSingleton();
 if(!camera || camera->IsInFirstPerson() || !player->Get3D(false) || player->IsDead()) {blend=0;return;}
 const float desired=active?1.f:0.f;
 blend+=(desired-blend)*(1.f-std::exp(-10.f*dt));
 if(blend<0.01f && !active){blend=0;return;}
 if(active)localGoal=goal-player->GetPosition();
 auto* skeleton=player->Get3D(false);
 auto* upper=skeleton->GetObjectByName(RE::BSFixedString("NPC R UpperArm [RUar]"));
 auto* fore=skeleton->GetObjectByName(RE::BSFixedString("NPC R Forearm [RLar]"));
 auto* hand=skeleton->GetObjectByName(RE::BSFixedString("NPC R Hand [RHnd]"));
 if(!upper || !fore || !hand || !upper->parent || !fore->parent)return;
 const float length=upper->world.translate.GetDistance(fore->world.translate)+fore->world.translate.GetDistance(hand->world.translate);
 if(length<5.f || length>200.f)return;
 auto offset=player->GetPosition()+localGoal-upper->world.translate;
 const float distance=offset.Length();
 if(distance<1.f)return;
 // Stay slightly inside the natural reach to keep an elbow bend.
 const auto target=upper->world.translate+offset*((std::min)(distance,length*0.92f)/distance);
 root.reset(skeleton);edits[0].node.reset(upper);edits[1].node.reset(fore);
 for(auto& edit:edits)edit.base=edit.node->local.rotate;
 auto aim=[&](RE::NiAVObject* joint,float limit) {
  auto from=hand->world.translate-joint->world.translate;
  auto to=target-joint->world.translate;
  if(from.Unitize()<0.001f || to.Unitize()<0.001f)return;
  auto axis=to.Cross(from);if(axis.Unitize()<0.001f)return;
  const float angle=(std::min)(std::acos(std::clamp(from.Dot(to),-1.f,1.f)),limit)*blend;
  RE::NiMatrix3 turn;turn.MakeRotation(angle,axis);
  joint->local.rotate=joint->parent->world.rotate.Transpose()*turn*joint->world.rotate;
  Refresh(joint);
 };
 // One bounded correction per joint, recalculated from the live animated pose.
 aim(upper,0.95f);aim(fore,1.1f);
 for(auto& edit:edits)edit.applied=edit.node->local.rotate;
}
}
