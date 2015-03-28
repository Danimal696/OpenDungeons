/*
 *  Copyright (C) 2011-2015  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "GameEditorModeBase.h"

#include "game/Player.h"
#include "gamemap/GameMap.h"
#include "render/Gui.h"
#include "render/ODFrameListener.h"
#include "rooms/Room.h"
#include "traps/Trap.h"
#include "utils/Helper.h"

#include <CEGUI/widgets/PushButton.h>

namespace {
    //Functors for binding gui actions to spell/room/trap selection.
    class RoomSelector
    {
    public:
        bool operator()(const CEGUI::EventArgs& e)
        {
            gameMap->getLocalPlayer()->setCurrentAction(Player::SelectedAction::buildRoom);
            gameMap->getLocalPlayer()->setNewRoomType(roomType);
            return true;
        }
        RoomType roomType;
        GameMap* gameMap;
    };
    class TrapSelector
    {
    public:
        bool operator()(const CEGUI::EventArgs& e)
        {
            gameMap->getLocalPlayer()->setCurrentAction(Player::SelectedAction::buildTrap);
            gameMap->getLocalPlayer()->setNewTrapType(trapType);
            return true;
        }
        TrapType trapType;
        GameMap* gameMap;
    };
    class ActionSelector
    {
    public:
        bool operator()(const CEGUI::EventArgs& e)
        {
            gameMap->getLocalPlayer()->setCurrentAction(action);
            return true;
        }
        Player::SelectedAction action;
        GameMap* gameMap;
    };
    //! \brief Functor for calling notifyGuiAction
    // only used in game and editor currently.
    class GuiNotifier
    {
    public:
        bool operator()(const CEGUI::EventArgs&)
        {
            mode->notifyGuiAction(action);
            return true;
        }
        AbstractApplicationMode::GuiAction action;
        GameEditorModeBase* mode;
    };
}

GameEditorModeBase::GameEditorModeBase(ModeManager *modeManager, ModeManager::ModeType modeType, CEGUI::Window* rootWindow)
    : AbstractApplicationMode(modeManager, modeType),
      mRootWindow(rootWindow),
      mGameMap(ODFrameListener::getSingletonPtr()->getClientGameMap()),
      mMiniMap(rootWindow->getChild(Gui::MINIMAP))
{
    addEventConnection(
        rootWindow->getChild(Gui::MINIMAP)->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(&GameEditorModeBase::onMinimapClick, this)
    ));

    //Rooms
    addEventConnection(
        rootWindow->getChild(Gui::BUTTON_DESTROY_ROOM)->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(ActionSelector{Player::SelectedAction::destroyRoom, mGameMap})
        )
    );
    connectRoomSelect(Gui::BUTTON_CRYPT, RoomType::crypt);
    connectRoomSelect(Gui::BUTTON_DORMITORY, RoomType::dormitory);
    connectRoomSelect(Gui::BUTTON_TEMPLE, RoomType::dungeonTemple);
    connectRoomSelect(Gui::BUTTON_WORKSHOP, RoomType::workshop);
    connectRoomSelect(Gui::BUTTON_HATCHERY, RoomType::hatchery);
    connectRoomSelect(Gui::BUTTON_LIBRARY, RoomType::library);
    connectRoomSelect(Gui::BUTTON_PORTAL, RoomType::portal);
    connectRoomSelect(Gui::BUTTON_TRAININGHALL, RoomType::trainingHall);
    connectRoomSelect(Gui::BUTTON_TREASURY, RoomType::treasury);

    //Traps
    addEventConnection(
        rootWindow->getChild(Gui::BUTTON_DESTROY_TRAP)->subscribeEvent(
            CEGUI::Window::EventMouseClick,
            CEGUI::Event::Subscriber(ActionSelector{Player::SelectedAction::destroyTrap, mGameMap})
        )
    );
    connectTrapSelect(Gui::BUTTON_TRAP_BOULDER, TrapType::boulder);
    connectTrapSelect(Gui::BUTTON_TRAP_CANNON, TrapType::cannon);
    connectTrapSelect(Gui::BUTTON_TRAP_SPIKE, TrapType::spike);

    //Creature buttons
    connectGuiAction(Gui::BUTTON_CREATURE_WORKER, AbstractApplicationMode::GuiAction::ButtonPressedCreatureWorker);
    connectGuiAction(Gui::BUTTON_CREATURE_FIGHTER, AbstractApplicationMode::GuiAction::ButtonPressedCreatureFighter);
}

void GameEditorModeBase::connectGuiAction(const std::string& buttonName, AbstractApplicationMode::GuiAction action)
{
    addEventConnection(
        mRootWindow->getChild(buttonName)->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(GuiNotifier{action, this})
        )
    );
}

GameEntity* GameEditorModeBase::getEntityFromOgreName(const std::string& entityName)
{
    // We check the prefix to know the kind of object the user clicked. Then, we call the corresponding
    // GameMap function to retrieve the entity
    std::string::size_type index = entityName.find("-");
    if(index == std::string::npos)
        return nullptr;

    int32_t intGameEntityType = Helper::toInt(entityName.substr(0, index));
    GameEntityType type = static_cast<GameEntityType>(intGameEntityType);
    return mGameMap->getEntityFromTypeAndName(type, entityName.substr(index + 1));
}


bool GameEditorModeBase::onMinimapClick(const CEGUI::EventArgs& arg)
{
    const CEGUI::MouseEventArgs& mouseEvt = static_cast<const CEGUI::MouseEventArgs&>(arg);

    ODFrameListener& frameListener = ODFrameListener::getSingleton();

    Ogre::Vector2 cc = mMiniMap.camera_2dPositionFromClick(static_cast<int>(mouseEvt.position.d_x),
        static_cast<int>(mouseEvt.position.d_y));
    frameListener.getCameraManager()->onMiniMapClick(cc);

    return true;
}

void GameEditorModeBase::connectRoomSelect(const std::string& buttonName, RoomType roomType)
{
    addEventConnection(
        mRootWindow->getChild(buttonName)->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(RoomSelector{roomType, mGameMap})
        )
    );
}

void GameEditorModeBase::connectTrapSelect(const std::string& buttonName, TrapType trapType)
{
    addEventConnection(
        mRootWindow->getChild(buttonName)->subscribeEvent(
          CEGUI::PushButton::EventClicked,
          CEGUI::Event::Subscriber(TrapSelector{trapType, mGameMap})
        )
    );
}