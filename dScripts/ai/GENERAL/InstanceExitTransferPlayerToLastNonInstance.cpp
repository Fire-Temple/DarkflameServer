#include "InstanceExitTransferPlayerToLastNonInstance.h"
#include "GameMessages.h"
#include "CharacterComponent.h"
#include "Character.h"
#include "dServer.h"
#include "eTerminateType.h"

void InstanceExitTransferPlayerToLastNonInstance::OnUse(Entity* self, Entity* user) {
	auto transferText = self->GetVar<std::u16string>(u"transferText");
	if (transferText.empty())
		transferText = u"DRAGON_EXIT_QUESTION";

	GameMessages::SendDisplayMessageBox(
		user->GetObjectID(),
		true,
		self->GetObjectID(),
		u"Instance_Exit",
		1,
		transferText,
		u"",
		user->GetSystemAddress()
	);
}

void InstanceExitTransferPlayerToLastNonInstance::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	if (!sender->IsPlayer()) return;
	
	auto* character = sender->GetCharacter();
	if (character != nullptr) {
		if (identifier == u"Instance_Exit" && button == 1) {
			auto lastInstance = character->GetLastNonInstanceZoneID();

			// Sanity check
			if (lastInstance == 0) {
				switch (Game::server->GetZoneID()) {
				case 2100:
					lastInstance = 2000;
					break;
				case 2001:
					lastInstance = 2000;
					break;
				case 1402:
					lastInstance = 1400;
					break;
				default:
					lastInstance = 1100;
					break;
				}
			}
			if (lastInstance == 1300) {
				switch (Game::server->GetZoneID()) {
				case 2100:
					lastInstance = 2000;
				case 2001:
					lastInstance = 2000;
					break;
				case 1402:
					lastInstance = 1400;
					break;
				default:
					lastInstance = 1100;
					break;
				}
			}

			auto* characterComponent = sender->GetComponent<CharacterComponent>();
			if (characterComponent) characterComponent->SendToZone(lastInstance);
		}
	}

	GameMessages::SendTerminateInteraction(sender->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
}


