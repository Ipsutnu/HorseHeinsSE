// Derived from VR Reins, Copyright (C) 2026 SollyStones.
// Non-VR adaptation. SPDX-License-Identifier: GPL-3.0-or-later
#include "Reins.h"
#include "Papyrus.h"
#include "Rope.h"
#include "RopeSim.h"
#undef GetObject
namespace Reins {
namespace {
using NiPoint3=RE::NiPoint3;
constexpr float kBitHalfWidth=6;
#include "Anatomy.inl"
struct TieInfo { int index=RopeSim::kMid; NiPoint3 point; RE::FormID cell=0; RE::FormID marker=0; NiPoint3 position{}, rotation{}; };
std::unordered_map<RE::FormID,TieInfo> g_tied;
std::unordered_map<RE::FormID,bool> equipped;
std::unordered_map<RE::FormID,RE::NiPointer<RE::Actor>> retained;
std::unordered_map<RE::FormID,bool> attached;
RE::BSTSmartPointer<RE::BSScript::Object> horseScript;
float equipTime=0;RE::FormID equipHorse=0,equipItem=0;
void SuspendHorseCall(RE::Actor* horse) {
 if(!horseScript)return;
 auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
 RE::BSScript::Variable value;
 if(vm->GetPropertyValue(horseScript,"Alias_PlayersHorse",value)) {
  auto* alias=value.Unpack<RE::BGSRefAlias*>();
  if(alias && alias->GetActorReference()==horse) {
   auto object=value.GetObject();RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
   vm->DispatchMethodCall(object,RE::BSFixedString("Clear"),RE::MakeFunctionArguments(),result);
   vm->DispatchMethodCall(horseScript,RE::BSFixedString("GoToState"),RE::MakeFunctionArguments(std::string("Waiting")),result);
  }
 }
}

std::unordered_map<RE::FormID,RopeSim::Rope> ropes;
struct AnchorTransition {int mode=-1;float remaining=0;NiPoint3 point{};};
std::unordered_map<RE::FormID,AnchorTransition> transitions;

RE::FormID g_led=0;
bool HasReins(RE::FormID id) {return Controls::automaticReins || equipped.contains(id) || g_tied.contains(id) || id==g_led;}
bool positioning=false, followActive=false, wasMounted=false;
NiPoint3 previousPlayer{};
bool sampledPlayer=false;
float playerSpeed=0;
float leadTurnVelocity=0;
void ClearSpeed(RE::Actor* h) {Papyrus::CallActor(h,"ClearForcedMovement");}
void ReleaseNativeMovement(RE::Actor* h) {
 if(g_tied.contains(h->GetFormID()))return;
 if(h->AsActorState()->actorState1.lifeState==RE::ACTOR_LIFE_STATE::kRestrained)
  h->AsActorState()->actorState1.lifeState=RE::ACTOR_LIFE_STATE::kAlive;
 h->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kMovementBlocked);
}
void ReleaseMovement(RE::Actor* horse) {
 if(g_tied.contains(horse->GetFormID()))return;
 ReleaseNativeMovement(horse);ClearSpeed(horse);
 Papyrus::SetRestrained(horse,false);
 Papyrus::SetDontMove(horse,false);
}
void Idle(RE::Actor* horse) {
 Papyrus::ClearKeepOffsetFromActor(horse);
 Papyrus::SetDontMove(horse,true);
}

void LeadIdle(RE::Actor* horse) {
 ClearSpeed(horse);Idle(horse);Papyrus::SetRestrained(horse,true);
}

// Only used with a held rein while follow is inactive; never while mounted or tied.
void HoldLeadStill(RE::Actor* horse,float dt) {
 if(horse->IsDead())return;
 auto* state=horse->AsActorState();
 // Enforce the same locks synchronously while asynchronous Papyrus commands settle.
 if(state->actorState1.lifeState==RE::ACTOR_LIFE_STATE::kAlive)
  state->actorState1.lifeState=RE::ACTOR_LIFE_STATE::kRestrained;
 horse->GetActorRuntimeData().boolFlags.set(RE::Actor::BOOL_FLAGS::kMovementBlocked);
 horse->StopMoving(dt);
 state->actorState1.movingForward=0;state->actorState1.movingBack=0;
 state->actorState1.movingLeft=0;state->actorState1.movingRight=0;
 state->actorState1.walking=0;state->actorState1.running=0;state->actorState1.sprinting=0;
 state->actorState2.forceRun=0;
 // Only touch a locomotion parameter actually exposed by this horse's graph.
 float speed=0;
 if(horse->GetGraphVariableFloat(RE::BSFixedString("Speed"),speed) && speed!=0.f)
  horse->SetGraphVariableFloat(RE::BSFixedString("Speed"),0.f);
}

// Acceleration-limited angular interpolation, always using the shortest turn.
void SmoothLeadHeading(RE::Actor* horse,const NiPoint3& target,float dt) {
 auto direction=target-horse->GetPosition();direction.z=0;
 if(direction.Length()<=20.f){leadTurnVelocity=0;return;}
 constexpr float maxSpeed=1.8f,acceleration=6.f,deadZone=0.025f;
 const float desired=std::atan2(direction.x,direction.y);
 float heading=horse->GetAngleZ();
 const int steps=(std::max)(1,static_cast<int>(std::ceil(dt*120.f)));
 const float stepTime=dt/steps;
 for(int i=0;i<steps;++i) {
  const float error=std::remainder(desired-heading,6.2831853f);
  if(std::abs(error)<deadZone){leadTurnVelocity=0;break;}
  const float brakingSpeed=std::sqrt(2.f*acceleration*std::abs(error));
  const float desiredSpeed=std::copysign((std::min)({maxSpeed,brakingSpeed,std::abs(error)*5.f}),error);
  leadTurnVelocity+=std::clamp(desiredSpeed-leadTurnVelocity,-acceleration*stepTime,acceleration*stepTime);
  float turn=leadTurnVelocity*stepTime;
  if(turn*error>0 && std::abs(turn)>=std::abs(error)){turn=error;leadTurnVelocity=0;}
  heading+=turn;
 }
 horse->SetHeading(heading);
}

RopeSim::NeckSurface MakeNeck(RE::Actor* horse,const Anatomy& anatomy) {
 const auto v=NeckConfig::Get();const auto f=Forward(horse->GetAngleZ()),r=Right(horse->GetAngleZ());
 const float angle=v[6]*0.01745329252f;const NiPoint3 z{0,0,1};
 RopeSim::NeckSurface shell;
 shell.center=anatomy.withers+f*v[0]+r*v[1]+z*v[2];
 shell.forward=f*std::cos(angle)+z*std::sin(angle);shell.right=r;shell.up=z*std::cos(angle)-f*std::sin(angle);
 shell.radius={v[3],v[4],v[5]};return shell;
}
std::unordered_map<RE::FormID,float> neckBlend;
std::unordered_map<RE::FormID,NiPoint3> neckAnchors; // correction relative to the live hand, never a world-space target
struct MountedFrame {NiPoint3 position;float heading;};
std::unordered_map<RE::FormID,MountedFrame> mountedFrames;
NiPoint3 placement;
bool placementValid=false;

#include "Tying.inl"
void StopLeading() {
 if(auto* h=RE::TESForm::LookupByID<RE::Actor>(g_led)) {
  ReleaseMovement(h); Papyrus::ClearKeepOffsetFromActor(h);
 }
 g_led=0; positioning=false; followActive=false;wasMounted=false;leadTurnVelocity=0;
}
bool Allowed() {
 auto* p=RE::PlayerCharacter::GetSingleton(); auto* ui=RE::UI::GetSingleton();
 RE::NiPointer<RE::Actor> mount;
 return p && p->Is3DLoaded() && !p->IsDead() && ui && !ui->GameIsPaused() &&
  !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) && !ui->IsMenuOpen(RE::Console::MENU_NAME) &&
  !Controls::Blocked() && !Controls::capture && !(p->GetMount(mount)&&mount);
}
NiPoint3 Hand(RE::PlayerCharacter* p) {
 auto* camera=RE::PlayerCamera::GetSingleton();
 const bool first=camera && camera->IsInFirstPerson();
 if(auto* root=p->Get3D(first)) if(auto* n=root->GetObjectByName(RE::BSFixedString("NPC R Hand [RHnd]"))) return n->world.translate;
 return p->GetPosition()+Forward(p->GetAngleZ())*25+Right(p->GetAngleZ())*20+NiPoint3{0,0,80};
}
NiPoint3 FloatingTip(RE::PlayerCharacter* p) {
 auto* camera=RE::PlayerCamera::GetSingleton();
 if(!camera || !camera->cameraRoot)return Hand(p);
 const auto& t=camera->cameraRoot->world;
 NiPoint3 direction{t.rotate.entry[0][1],t.rotate.entry[1][1],t.rotate.entry[2][1]};
 // Third-person camera may be far behind the actor: project onto its forward ray near the player.
 const float ahead=(std::max)(0.f,(Hand(p)-t.translate).Dot(direction));
 const float forward=camera->IsInFirstPerson()?65.f:85.f; // third-person extra reach: 20
 if(!camera->IsInFirstPerson() && !EndpointConfig::hand) {
  const auto v=EndpointConfig::Get();
  const NiPoint3 right{t.rotate.entry[0][0],t.rotate.entry[1][0],t.rotate.entry[2][0]};
  return t.translate+direction*(ahead+v[0])+right*v[1]+NiPoint3{0,0,v[2]};
 }
 return t.translate+direction*(ahead+forward);
}
#include "Reach.inl"
RE::Actor* GrabTarget() {
 auto* p=RE::PlayerCharacter::GetSingleton();auto* pick=RE::CrosshairPickData::GetSingleton();
 if(!p || !pick)return nullptr;
 auto ref=pick->GetActiveTarget().get();auto* horse=ref?ref->As<RE::Actor>():nullptr;
 RE::NiPointer<RE::Actor> rider;
 return horse && HasReins(horse->GetFormID()) && IsHorse(horse) && !horse->IsDead() && horse->Is3DLoaded() &&
  horse->GetPosition().GetDistance(p->GetPosition())<=180.f && !(horse->GetMountedBy(rider)&&rider)?horse:nullptr;
}
void Interact(bool holding) {
 if(!Allowed()) return;
 if(g_led) {
  auto* h=LookupHorse(g_led); if(!h) {StopLeading();return;}
  auto* player=RE::PlayerCharacter::GetSingleton();
  auto simulated=ropes.find(g_led);
  const auto endpoint=simulated!=ropes.end() && simulated->second.init?simulated->second.p[RopeSim::kMid]:placement;
  auto at=TieProbe(player,endpoint);
  if(!at) { Controls::Notify(Translations::Text("Move the rope endpoint against a surface before releasing.","Encoste a ponta da corda numa superficie antes de soltar.")); return; }
  auto an=GetAnatomy(h);
  float leftReach=Config::Get().reinLength,rightReach=leftReach;
  if(simulated!=ropes.end() && simulated->second.init) {
   const auto& points=simulated->second.p;float leftLength=0,rightLength=0;
   for(int i=0;i<RopeSim::kMid;++i)leftLength+=points[i].GetDistance(points[i+1]);
   for(int i=RopeSim::kMid;i<RopeSim::kPoints-1;++i)rightLength+=points[i].GetDistance(points[i+1]);
   leftReach=(std::max)(leftReach,leftLength);rightReach=(std::max)(rightReach,rightLength);
  }
  // Match the visible simulated reach, including the same contact tolerance used by TieProbe.
  // The renderer can accommodate temporary strain; rejecting by nominal length alone contradicted it.
  const float contactTolerance=Config::Get().tieReach+1.f;
  if(at->GetDistance(an.LeftRing())>leftReach+contactTolerance || at->GetDistance(an.RightRing())>rightReach+contactTolerance) {
   Controls::Notify(Translations::Text("The reins cannot reach that point.","A redea nao alcanca esse ponto."));return;
  }
  SuspendHorseCall(h);
  ClearSpeed(h);
  Tie(h,RopeSim::kMid,*at);
  auto& tie=g_tied[h->GetFormID()];
  tie.cell=h->GetParentCell()->GetFormID();tie.position=h->GetPosition();tie.rotation=h->GetAngle();
  if(auto* base=RE::TESForm::LookupByID<RE::TESBoundObject>(0x3B)) {
   auto marker=h->PlaceObjectAtMe(base,true);if(marker)tie.marker=marker->GetFormID();
  }
  attached[h->GetFormID()]=true;retained[h->GetFormID()].reset(h);
  g_led=0;followActive=false;leadTurnVelocity=0;  Controls::Notify(Translations::Text("Horse tied.","Cavalo amarrado."));return;
 }
 if(!holding) return;
 RE::Actor* best=GrabTarget();
 if(best) { SuspendHorseCall(best);Untie(best);LeadIdle(best);sampledPlayer=false;playerSpeed=0;leadTurnVelocity=0;g_led=best->GetFormID();followActive=false;positioning=false; Controls::Notify(Translations::Text("Reins taken.","Redea na mao.")); }
}
class Input final: public RE::BSTEventSink<RE::InputEvent*> {
 bool fired=false,started=false,wasLeading=false;
 RE::FormID candidate=0;
 RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
  if(!events) return RE::BSEventNotifyControl::kContinue;
  for(auto* e=*events;e;e=e->next) if(auto* b=e->AsButtonEvent();b && Controls::ButtonCode(b)==Controls::key) {
   if(!Allowed()) {started=false;fired=false;positioning=false;Controls::grabProgress=-1.f;continue;}
   if(b->IsDown()) {started=true;fired=false;wasLeading=g_led!=0;if(wasLeading){positioning=true;placement=FloatingTip(RE::PlayerCharacter::GetSingleton());placementValid=false;fired=true;}else {auto* target=GrabTarget();if(!target){started=false;Controls::grabProgress=-1.f;continue;}candidate=target->GetFormID();Controls::grabProgress=0.f;}}
   if(started && !fired && (!GrabTarget() || GrabTarget()->GetFormID()!=candidate)){started=false;Controls::grabProgress=-1.f;}
   if(started && !fired)Controls::grabProgress=std::clamp(b->HeldDuration()*2.f,0.f,1.f);
   if(started && !fired && b->HeldDuration()>=0.5f) {fired=true;Interact(true);Controls::grabProgress=-1.f;}
   if(b->IsUp()) {if(started && wasLeading && positioning){Interact(false);positioning=false;} started=false;fired=false;Controls::grabProgress=-1.f;}
  }
  return RE::BSEventNotifyControl::kContinue;
 }
};
Input input;
}
bool Init() {
 Controls::Load(); Controls::RegisterMenu();
 auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
 for(auto* quest:RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESQuest>()) {
  auto* file=quest?quest->GetFile(0):nullptr;
  if(file && file->GetFilename()=="H2Horse.esp") {
   auto handle=vm->GetObjectHandlePolicy()->GetHandleForObject(RE::TESQuest::FORMTYPE,quest);
   if(vm->FindBoundObject(handle,"DES_HorseCallScript",horseScript))break;
  }
 }
 logger::info("Press H to Horse integration: {}",horseScript?"connected":"not detected");
 if(!Rope::Init()) return false;
 RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&input); return true;
}
void Update(float dt) {
 auto* p=RE::PlayerCharacter::GetSingleton(); auto* ui=RE::UI::GetSingleton();
 if(!p || !p->Is3DLoaded() || !ui || ui->GameIsPaused()) {Controls::grabProgress=-1.f;return;}
 {std::scoped_lock lock(NeckConfig::mutex);NeckConfig::wire.clear();}
 const float elapsed=dt;
 dt=std::clamp(dt,0.f,0.05f);
 const auto position=p->GetPosition();
 if(sampledPlayer && elapsed>0 && elapsed<0.25f) {
  auto delta=position-previousPlayer;delta.z=0;
  const float measured=delta.Length()/elapsed;
  if(measured<2.f || measured>1500)playerSpeed=0;
  else playerSpeed+=(measured-playerSpeed)*(std::min)(1.f,elapsed*10.f);
 } else playerSpeed=0;
 previousPlayer=position;sampledPlayer=true;
 if(Controls::grabProgress>=0 && (!Allowed() || !GrabTarget()))Controls::grabProgress=-1.f;
 for(auto& [id,tie]:g_tied) {
  auto* horse=RE::TESForm::LookupByID<RE::Actor>(id);
  if(!horse)continue;
  retained[id].reset(horse);
  if(!horse->Is3DLoaded()){attached[id]=false;continue;}
  const bool displaced=horse->GetPosition().GetDistance(tie.position)>60.f || (horse->GetParentCell() && horse->GetParentCell()->GetFormID()!=tie.cell);
  if(!attached[id] || displaced) {
   SuspendHorseCall(horse);Idle(horse);Papyrus::SetRestrained(horse,true);
   if(auto* marker=RE::TESForm::LookupByID<RE::TESObjectREFR>(tie.marker)){marker->SetPosition(tie.position);horse->MoveTo(marker);}
   else if(horse->GetParentCell() && horse->GetParentCell()->GetFormID()==tie.cell)horse->RE::TESObjectREFR::SetPosition(tie.position);
   horse->SetAngle(tie.rotation);attached[id]=true;
  }
  // Keep tied locomotion stopped while allowing the game to manage idle animations.
  if(!horse->IsDead())horse->StopMoving(dt);
 }
 // Havok-moved objects can keep a stale reference position while grabbed.
 // Read the rendered object's world transform for contact detection.

 auto held=p->GetGrabbedRef();RE::Actor* fitting=nullptr;
 const auto heldPosition=held && held->Get3D()?(held->Get3D()->worldBound.radius>0?held->Get3D()->worldBound.center:held->Get3D()->world.translate):(held?held->GetPosition():NiPoint3{});
 if(held && held->GetBaseObject() && held->GetBaseObject()->GetFormID()==0x800E4 && Allowed()) {
  if(auto* list=RE::ProcessLists::GetSingleton()) for(auto handle:list->highActorHandles) {
   auto horse=handle.get();RE::NiPointer<RE::Actor> rider;
   if(!horse || !IsHorse(horse.get()) || horse->IsDead() || !horse->Is3DLoaded() || HasReins(horse->GetFormID()) || (horse->GetMountedBy(rider)&&rider))continue;
   const auto anatomy=GetAnatomy(horse.get());
   auto* head=horse->Get3D()->GetObjectByName(RE::BSFixedString("HorseScull"));
   const auto headPosition=head?head->world.translate:anatomy.mouth-anatomy.fwd*30.f;
   auto axis=anatomy.mouth-headPosition;const float length2=axis.Dot(axis);
   const float along=length2>0.001f?std::clamp((heldPosition-headPosition).Dot(axis)/length2,0.f,1.f):0.f;
   const auto closest=headPosition+axis*along;
   if(horse->GetPosition().GetDistance(p->GetPosition())<350.f && heldPosition.GetDistance(closest)<55.f) {fitting=horse.get();break;}
  }
 }
 if(!fitting){equipTime=0;equipHorse=0;equipItem=0;}
 else {
  if(equipHorse!=fitting->GetFormID() || equipItem!=held->GetFormID())equipTime=0;
  equipHorse=fitting->GetFormID();equipItem=held->GetFormID();equipTime+=dt;
  if(equipTime>=0.6f) {
   const auto count=(std::max)(1,held->extraList.GetCount());
   p->EndGrabObject();
   if(count>1){held->extraList.SetCount(static_cast<std::uint16_t>(count-1));held->AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData);}
   else {held->Disable();held->SetDelete(true);}
   logger::info("Fitted reins to horse {:08X}, consuming one held Leather Strips reference {:08X}",equipHorse,equipItem);
   equipped[equipHorse]=true;retained[equipHorse].reset(fitting);equipTime=0;
   Controls::Notify(Translations::Text("Reins fitted.","Redea colocada."));
  }
 }

 if(g_led) {
  auto* h=LookupHorse(g_led); RE::NiPointer<RE::Actor> rider;
  const bool mounted=h && h->GetMountedBy(rider) && rider && rider.get()==p;
  if(!h || p->IsDead() || (h->GetMountedBy(rider)&&rider && rider.get()!=p) || (h->GetWorldspace()!=p->GetWorldspace() || (!h->GetWorldspace() && h->GetParentCell()!=p->GetParentCell())) || h->GetPosition().GetDistance(p->GetPosition())>600) StopLeading();
  else if(mounted) {
   if(!wasMounted){ReleaseMovement(h);Papyrus::ClearKeepOffsetFromActor(h);}
   wasMounted=true;followActive=false;positioning=false;leadTurnVelocity=0;
  }
  else {
   if(wasMounted){wasMounted=false;LeadIdle(h);sampledPlayer=false;playerSpeed=0;}
   if(positioning && !Allowed()) positioning=false;
   if(positioning)placement=FloatingTip(p);
   const auto target=positioning?placement:Hand(p);
   auto an=GetAnatomy(h);
   // Displacement alone can come from the horse pushing a stationary player.
   const auto& playerMotion=p->AsActorState()->actorState1;
   const bool playerWalking=playerMotion.movingForward || playerMotion.movingBack ||
    playerMotion.movingLeft || playerMotion.movingRight;
   const bool moving=playerWalking && playerSpeed>8.f && !positioning;
   auto separation=p->GetPosition()-h->GetPosition();separation.z=0;
   const float bodyDistance=separation.Length();
   auto muzzleGap=target-an.mouth;muzzleGap.z=0;
   const float noseDistance=muzzleGap.Length();
   // Stop using both body clearance and muzzle clearance, regardless of rope tension.
   const bool tooClose=bodyDistance<210.f || noseDistance<85.f;
   // A stationary player never triggers a catch-up order, even with a taut rope.
   const bool shouldFollow=moving && !tooClose && bodyDistance>(followActive?212.f:225.f);
   if(shouldFollow && !followActive) {
    ReleaseMovement(h);StartLeading(p,h);followActive=true;
   } else if(!shouldFollow && followActive) {
    LeadIdle(h);followActive=false;
   }
   if(!followActive) {
    HoldLeadStill(h,dt);
   }
   else {
    ReleaseNativeMovement(h);
    // Select the native gait; leave speed and acceleration to the game's AI.
    const bool slow=p->AsActorState()->IsWalking() || playerSpeed<110.f;
    auto* state=h->AsActorState();
    state->actorState1.walking=slow;
    state->actorState1.running=!slow;
    state->actorState1.sprinting=0;
    state->actorState2.forceRun=0;
   }
   if(followActive)SmoothLeadHeading(h,target,dt);
   else leadTurnVelocity=0;
  }
 }
 ReachPose::Apply(p,dt,g_led && positioning && Allowed(),placement);
 // In third person, attach the positioned tip to the hand after applying the reach pose.
 if(g_led && positioning && EndpointConfig::hand && ReachPose::edits[0].node && ReachPose::root.get()==p->Get3D(false)) {
  const auto v=EndpointConfig::Get();
  placement=Hand(p)+Forward(p->GetAngleZ())*v[0]+Right(p->GetAngleZ())*v[1]+NiPoint3{0,0,v[2]};
 }
 std::vector<std::pair<float,RE::Actor*>> horses;
 if(auto* list=RE::ProcessLists::GetSingleton()) for(auto handle:list->highActorHandles) {
  auto h=handle.get(); if(!h || !HasReins(h->GetFormID()) || !IsHorse(h.get()) || h->IsDead() || !h->Is3DLoaded())continue;
  float d=h->GetPosition().GetDistance(p->GetPosition());
  if(d<1500)horses.emplace_back(h->GetFormID()==g_led? -1.f:d,h.get());
 }
 std::sort(horses.begin(),horses.end(),[](auto a,auto b){return a.first<b.first;});
 std::unordered_map<RE::FormID,bool> seen;
 for(size_t i=0;i<horses.size() && i<8;++i) {
  auto* h=horses[i].second; auto id=h->GetFormID(); seen[id]=true;
  auto an=GetAnatomy(h); RopeSim::Pin a,b;
  RE::NiPointer<RE::Actor> rider;
  const bool ridden=h->GetMountedBy(rider) && rider;
  if(id==g_led) a={RopeSim::kMid,positioning?placement:Hand(p)};
  else if(auto t=g_tied.find(id);t!=g_tied.end()) {
   if(h->GetParentCell()) a={t->second.index,t->second.point};
  } else if(ridden) { a={RopeSim::kMid,an.rest}; }
  auto& rope=ropes[id];
  if(ridden) {
   const MountedFrame current{h->GetPosition(),h->GetAngleZ()};
   auto previous=mountedFrames.find(id);
   if(previous!=mountedFrames.end() && rope.init) {
    const float yaw=current.heading-previous->second.heading,c=std::cos(yaw),s=std::sin(yaw);
    auto carry=[&](NiPoint3 point){const auto d=point-previous->second.position;return current.position+NiPoint3{c*d.x+s*d.y,-s*d.x+c*d.y,d.z};};
    // Move both Verlet states with the mounted horse, preserving local swing velocity.
    for(int j=0;j<RopeSim::kPoints;++j){rope.p[j]=carry(rope.p[j]);rope.prev[j]=carry(rope.prev[j]);}
    if(auto transition=transitions.find(id);transition!=transitions.end())transition->second.point=carry(transition->second.point);
   }
   mountedFrames[id]=current;
  } else mountedFrames.erase(id);
  auto& transition=transitions[id];
  const int mode=id==g_led?(positioning?2:1):(g_tied.contains(id)?3:(a.index>0?4:0));
  if(mode!=transition.mode) {
   transition.mode=mode;transition.remaining=0.25f;
   transition.point=rope.init?rope.p[RopeSim::kMid]:a.pos;
   if(rope.init)rope.prev=rope.p;
  }
  if(a.index>0 && transition.remaining>0) {
   const auto desired=a.pos;
   transition.point=transition.point+(desired-transition.point)*(1.f-std::exp(-18.f*dt));
   transition.remaining=(std::max)(0.f,transition.remaining-dt);
   a.pos=transition.point;
   if(rope.init)rope.prev=rope.p;
  }
  auto shell=MakeNeck(h,an);
  const bool protect=ridden && NeckConfig::enabled && a.index==RopeSim::kMid;
  auto& blend=neckBlend[id];blend=std::clamp(blend+(protect?dt:-dt)*4.f,0.f,1.f);shell.strength=blend;
  if(protect) {
   const auto correction=shell.OutsideAnchor(a.pos)-a.pos;
   auto [entry,inserted]=neckAnchors.try_emplace(id,NiPoint3{});
   const auto change=correction-entry->second;
   const float distance=change.Length(),maximum=250.f*dt;
   entry->second=distance>maximum && distance>0.001f?entry->second+change*(maximum/distance):correction;
   a.pos=a.pos+entry->second;
  } else neckAnchors.erase(id);
  if(protect && NeckConfig::preview && rider.get()==p) {
   std::vector<std::pair<NiPoint3,NiPoint3>> lines;
   for(int ring=-2;ring<=2;++ring) {
    const float x=ring*0.35f,span=std::sqrt(1-x*x);
    for(int j=0;j<48;++j){const float t=j*6.2831853f/48.f,next=(j+1)*6.2831853f/48.f;
     lines.emplace_back(shell.center+shell.Vector({x,span*std::cos(t),span*std::sin(t)}),shell.center+shell.Vector({x,span*std::cos(next),span*std::sin(next)}));
    }
   }
   std::scoped_lock lock(NeckConfig::mutex);NeckConfig::wire=std::move(lines);
  }
  rope.Step(an.LeftRing(),an.RightRing(),a,b,Config::Get().reinLength/RopeSim::kMid,h->GetPosition().z+2,dt,blend>0?&shell:nullptr);
  Rope::DrawPoints(h,0,rope.p.data(),RopeSim::kMid+1);
  Rope::DrawPoints(h,1,rope.p.data()+RopeSim::kMid,RopeSim::kPoints-RopeSim::kMid);
 }
 std::erase_if(ropes,[&](auto& entry){return !seen.contains(entry.first);});
 std::erase_if(mountedFrames,[&](auto& entry){return !seen.contains(entry.first);});
 std::erase_if(neckAnchors,[&](auto& entry){return !seen.contains(entry.first);});
 std::erase_if(neckBlend,[&](auto& entry){return !seen.contains(entry.first);});
 std::erase_if(transitions,[&](auto& entry){return !seen.contains(entry.first);});
 Rope::EndFrame();
}
void BeforeUpdate() {ReachPose::Restore();}
void OnGameLoad() {
 ReachPose::Reset();
 attached.clear();
 for(auto& [id,t]:g_tied) if(auto* h=RE::TESForm::LookupByID<RE::Actor>(id)) {Idle(h);Papyrus::SetRestrained(h,true);}
}
void Save(SKSE::SerializationInterface* s) {
 if(s->OpenRecord('EQPD',1))for(auto& [id,value]:equipped)s->WriteRecordData(id);
 if(!s->OpenRecord('TIED',2))return;
 for(auto& [id,record]:g_tied) {
  auto t=record;
  s->WriteRecordData(id);s->WriteRecordData(t.cell);s->WriteRecordData(t.point);
  s->WriteRecordData(t.marker);s->WriteRecordData(t.position);s->WriteRecordData(t.rotation);
 }
}
void Load(SKSE::SerializationInterface* s) {
 std::uint32_t type,version,length;
 while(s->GetNextRecordInfo(type,version,length)) {
  if(type=='EQPD' && version==1 && length%4==0) {
   for(unsigned i=0;i<length/4;++i){RE::FormID id=0,resolved=0;if(!s->ReadRecordData(id))break;if(s->ResolveFormID(id,resolved))equipped[resolved]=true;}
   continue;
  }
  const unsigned size=version==1?20:48;
  if(type!='TIED' || (version!=1 && version!=2) || length%size!=0)continue;
  for(unsigned i=0;i<length/size;++i) {
   RE::FormID id=0,cell=0,resolved=0,resolvedCell=0;TieInfo t;
   if(!s->ReadRecordData(id)||!s->ReadRecordData(cell)||!s->ReadRecordData(t.point))break;
   if(version==2) {
    if(!s->ReadRecordData(t.marker)||!s->ReadRecordData(t.position)||!s->ReadRecordData(t.rotation))break;
    RE::FormID marker=0;if(s->ResolveFormID(t.marker,marker))t.marker=marker;else t.marker=0;
   }
   if(s->ResolveFormID(id,resolved)&&s->ResolveFormID(cell,resolvedCell)&&std::isfinite(t.point.x)&&std::isfinite(t.point.y)&&std::isfinite(t.point.z)) {
    t.cell=resolvedCell;g_tied[resolved]=t;equipped[resolved]=true;
    if(version==1)if(auto* horse=RE::TESForm::LookupByID<RE::Actor>(resolved)){g_tied[resolved].position=horse->GetPosition();g_tied[resolved].rotation=horse->GetAngle();}
   }
  }
 }
}
bool BlockHorseCall(RE::InputEvent* event) {
 if(!horseScript)return false;
 auto* b=event?event->AsButtonEvent():nullptr;
 if(!b || b->GetDevice()!=RE::INPUT_DEVICE::kKeyboard)return false;
 auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();RE::BSScript::Variable key;
 if(!vm->GetPropertyValue(horseScript,"horseKey",key) || b->GetIDCode()!=static_cast<unsigned>(key.GetSInt()))return false;
 auto* p=RE::PlayerCharacter::GetSingleton();if(!p)return false;
 RE::Actor* selected=nullptr;RE::BSScript::Variable value;
 if(vm->GetPropertyValue(horseScript,"SelectedHorse",value))selected=value.Unpack<RE::Actor*>();
 RE::BSScript::Variable aliasValue;
 if(selected && vm->GetPropertyValue(horseScript,"Alias_PlayersHorse",aliasValue)) {
  auto* alias=aliasValue.Unpack<RE::BGSRefAlias*>();
  if(!alias || alias->GetActorReference()!=selected)selected=nullptr;
 }
 if(!selected){auto horse=p->GetPlayerRuntimeData().lastRiddenMount.get();selected=horse.get();}
 if(!selected || !g_tied.contains(selected->GetFormID()))return false;
 if(b->IsDown())Controls::Notify(Translations::Text("Untie your horse before calling it.","Desamarre o cavalo antes de chama-lo."));
 return true;
}
void Revert() {ReachPose::Reset();leadTurnVelocity=0;mountedFrames.clear();neckAnchors.clear();neckBlend.clear();{std::scoped_lock lock(NeckConfig::mutex);NeckConfig::wire.clear();}transitions.clear();retained.clear();attached.clear();equipped.clear();equipTime=0;equipHorse=0;equipItem=0;wasMounted=false;Controls::grabProgress=-1.f;sampledPlayer=false;playerSpeed=0;positioning=false;followActive=false;g_led=0;g_tied.clear();ropes.clear();Rope::Clear();}
}
