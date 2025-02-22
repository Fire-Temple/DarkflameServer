#include <algorithm>
#include "RailActivatorComponent.h"
#include "CDClientManager.h"
#include "CDRailActivatorComponent.h"
#include "Entity.h"
#include "GameMessages.h"
#include "QuickBuildComponent.h"
#include "Game.h"
#include "Logger.h"
#include "RenderComponent.h"
#include "EntityManager.h"
#include "eStateChangeType.h"

RailActivatorComponent::RailActivatorComponent(Entity* parent, int32_t componentID) : Component(parent) {
	m_ComponentID = componentID;
	const auto tableData = CDClientManager::GetTable<CDRailActivatorComponentTable>()->GetEntryByID(componentID);;

	m_Path = parent->GetVar<std::u16string>(u"rail_path");
	m_PathDirection = parent->GetVar<bool>(u"rail_path_direction");
	m_PathStart = parent->GetVar<uint32_t>(u"rail_path_start");

	m_StartSound = tableData.startSound;
	m_loopSound = tableData.loopSound;
	m_StopSound = tableData.stopSound;

	m_StartAnimation = tableData.startAnimation;
	m_LoopAnimation = tableData.loopAnimation;
	m_StopAnimation = tableData.stopAnimation;

	m_StartEffect = tableData.startEffectID;
	m_LoopEffect = tableData.loopEffectID;
	m_StopEffect = tableData.stopEffectID;

	m_DamageImmune = parent->GetVar<bool>(u"rail_activator_damage_immune");
	m_NoAggro = parent->GetVar<bool>(u"rail_no_aggro");
	m_NotifyArrived = parent->GetVar<bool>(u"rail_notify_activator_arrived");
	m_ShowNameBillboard = parent->GetVar<bool>(u"rail_show_name_billboard");
	m_UseDB = parent->GetVar<bool>(u"rail_use_db");
	m_CameraLocked = tableData.cameraLocked;
	m_CollisionEnabled = tableData.playerCollision;
}

RailActivatorComponent::~RailActivatorComponent() = default;

void RailActivatorComponent::OnUse(Entity* originator) {
	

	auto* quickBuildComponent = m_Parent->GetComponent<QuickBuildComponent>();
	if (quickBuildComponent != nullptr && quickBuildComponent->GetState() != eQuickBuildState::COMPLETED)
		return;

	if (quickBuildComponent != nullptr) {
		// Don't want it to be destroyed while a player is using it
		quickBuildComponent->SetResetTime(quickBuildComponent->GetResetTime() + 10.0f);
	}

	m_EntitiesOnRail.push_back(originator->GetObjectID());

	// Start the initial effects
	if (!m_StartEffect.second.empty()) {
		GameMessages::SendPlayFXEffect(originator->GetObjectID(), m_StartEffect.first, m_StartEffect.second,
			std::to_string(m_StartEffect.first));

	}
	
//	Start sound 
	std::string StartSoundString(m_StartSound.begin(), m_StartSound.end());		
	GameMessages::SendPlayNDAudioEmitter(originator, UNASSIGNED_SYSTEM_ADDRESS, StartSoundString);		
	
	float animationLength = 0.5f;
	if (!m_StartAnimation.empty()) {
		animationLength = RenderComponent::PlayAnimation(originator, m_StartAnimation);
	}

	const auto originatorID = originator->GetObjectID();

	m_Parent->AddCallbackTimer(animationLength, [originatorID, this]() {
		auto* originator = Game::entityManager->GetEntity(originatorID);

		if (originator == nullptr) {
			return;
		}

		GameMessages::SendStartRailMovement(originator->GetObjectID(), m_Path, m_StartSound,
			m_loopSound, m_StopSound, originator->GetSystemAddress(),
			m_PathStart, m_PathDirection, m_DamageImmune, m_NoAggro, m_NotifyArrived,
			m_ShowNameBillboard, m_CameraLocked, m_CollisionEnabled, m_UseDB, m_ComponentID,
			m_Parent->GetObjectID());
		});
}

void RailActivatorComponent::OnRailMovementReady(Entity* originator) const {		
	
	// Stun the originator
	GameMessages::SendSetStunned(originator->GetObjectID(), eStateChangeType::PUSH, originator->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	if (std::find(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(), originator->GetObjectID()) != m_EntitiesOnRail.end()) {
		// Stop the initial effects
		if (!m_StartEffect.second.empty()) {
			GameMessages::SendStopFXEffect(originator, false, std::to_string(m_StartEffect.first));
		}

		// Start the looping effects
		if (!m_LoopEffect.second.empty()) {
			GameMessages::SendPlayFXEffect(originator->GetObjectID(), m_LoopEffect.first, m_LoopEffect.second,
				std::to_string(m_LoopEffect.first));
				
//			Start looping sound 
			std::string loopSoundString(m_loopSound.begin(), m_loopSound.end());		
			GameMessages::SendPlayNDAudioEmitter(originator, UNASSIGNED_SYSTEM_ADDRESS, loopSoundString);					
		}

		if (!m_LoopAnimation.empty()) {
			RenderComponent::PlayAnimation(originator, m_LoopAnimation);
		}

		GameMessages::SendSetRailMovement(originator->GetObjectID(), m_PathDirection, m_Path, m_PathStart,
			originator->GetSystemAddress(), m_ComponentID,
			m_Parent->GetObjectID());
	}
}

void RailActivatorComponent::OnCancelRailMovement(Entity* originator) {
	// Remove the stun from the originator
	GameMessages::SendSetStunned(originator->GetObjectID(), eStateChangeType::POP, originator->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	auto* quickBuildComponent = m_Parent->GetComponent<QuickBuildComponent>();

	if (quickBuildComponent != nullptr) {
		// Set back reset time
		quickBuildComponent->SetResetTime(quickBuildComponent->GetResetTime() - 10.0f);
	}

	if (std::find(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(), originator->GetObjectID()) != m_EntitiesOnRail.end()) {
		// Stop the looping effects
		if (!m_LoopEffect.second.empty()) {
			GameMessages::SendStopFXEffect(originator, false, std::to_string(m_LoopEffect.first));
			
//			Stop looping sound 
			std::string loopSoundString(m_loopSound.begin(), m_loopSound.end());		
			GameMessages::SendStopNDAudioEmitter(originator, UNASSIGNED_SYSTEM_ADDRESS, loopSoundString);				
			
		}

		// Start the end effects
		if (!m_StopEffect.second.empty()) {
			GameMessages::SendPlayFXEffect(originator->GetObjectID(), m_StopEffect.first, m_StopEffect.second,
				std::to_string(m_StopEffect.first));
				
//			Start stop sound 
			std::string StopSoundString(m_StopSound.begin(), m_StopSound.end());		
			GameMessages::SendPlayNDAudioEmitter(originator, UNASSIGNED_SYSTEM_ADDRESS, StopSoundString);					
		}

		if (!m_StopAnimation.empty()) {
			RenderComponent::PlayAnimation(originator, m_StopAnimation);
		}

		// Remove the player after they've signalled they're done railing
		m_EntitiesOnRail.erase(std::remove(m_EntitiesOnRail.begin(), m_EntitiesOnRail.end(),
			originator->GetObjectID()), m_EntitiesOnRail.end());
	}
}
