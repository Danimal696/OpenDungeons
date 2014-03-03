/*
 *  Copyright (C) 2011-2014  OpenDungeons Team
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

#include "EditorMode.h"

#include "MapLoader.h"
#include "Socket.h"
#include "Network.h"
#include "ClientNotification.h"
#include "ODFrameListener.h"
#include "LogManager.h"
#include "Gui.h"
#include "ODApplication.h"
#include "ResourceManager.h"
#include "TextRenderer.h"
#include "Creature.h"
#include "MapLight.h"
#include "Seat.h"
#include "Trap.h"
#include "Player.h"
#include "RenderRequest.h"
#include "RenderManager.h"
#include "CameraManager.h"
#include "Console.h"

#include <OgreEntity.h>

#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>

EditorMode::EditorMode(ModeManager* modeManager):
    AbstractApplicationMode(modeManager, ModeManager::EDITOR),
    mChanged(false),
    mCurrentFullness(100),
    mCurrentTileRadius(1),
    mBrushMode(false),
    mCurrentTileType(Tile::dirt),
    mDragType(nullDragType),
    mGameMap(ODFrameListener::getSingletonPtr()->getGameMap()),
    mCreatureSceneNode(NULL),
    mRoomSceneNode(NULL),
    mFieldSceneNode(NULL),
    mLightSceneNode(NULL)
{
    RenderManager* renderManager = RenderManager::getSingletonPtr();
    renderManager->setGameMap(mGameMap);

    LogManager* logManager = LogManager::getSingletonPtr();

    // Init the editor mode
    try
    {
        logManager->logMessage("Creating scene...", Ogre::LML_NORMAL);
        renderManager->createScene();
        logManager->logMessage("Creating compositors...", Ogre::LML_NORMAL);
        renderManager->createCompositors();
    }
    catch(Ogre::Exception& e)
    {
        ODApplication::displayErrorMessage("Ogre exception when initialising the render manager:\n"
            + e.getFullDescription(), false);
        // TODO: Cleanly exit instead
        exit(0);
        //cleanUp();
        //return;
    }
    catch (std::exception& e)
    {
        ODApplication::displayErrorMessage("Exception when initialising the render manager:\n"
            + std::string(e.what()), false);
        // TODO: Cleanly exit instead
        exit(0);
        //cleanUp();
        //return;
    }

    logManager->logMessage("Created camera manager");

    mGameMap->createTilesMeshes();
}

EditorMode::~EditorMode()
{
}

bool EditorMode::mouseMoved(const OIS::MouseEvent &arg)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectMousePosition((float)arg.state.X.abs, (float)arg.state.Y.abs);

    if (!isConnected())
        return true;

    if (ODFrameListener::getSingletonPtr()->isTerminalActive())
        return true;

    //If we have a room or trap (or later spell) selected, show what we
    //have selected
    //TODO: This should be changed, or combined with an icon or something later.
    if (mGameMap->getLocalPlayer()->getNewRoomType() || mGameMap->getLocalPlayer()->getNewTrapType())
    {
        TextRenderer::getSingleton().moveText(ODApplication::POINTER_INFO_STRING,
                                                (Ogre::Real)(arg.state.X.abs + 30), (Ogre::Real)arg.state.Y.abs);
    }

    // Handles drag type logic
    handleMouseMovedDragType(arg);

    CameraManager* cm = ODFrameListener::getSingleton().cm;

    if (arg.state.Z.rel > 0)
    {
        if (getKeyboard()->isModifierDown(OIS::Keyboard::Ctrl))
        {
            mGameMap->getLocalPlayer()->rotateCreaturesInHand(1);
        }
        else
        {
            cm->move(CameraManager::moveDown);
        }
    }
    else if (arg.state.Z.rel < 0)
    {
        if (getKeyboard()->isModifierDown(OIS::Keyboard::Ctrl))
        {
            mGameMap->getLocalPlayer()->rotateCreaturesInHand(-1);
        }
        else
        {
            cm->move(CameraManager::moveUp);
        }
    }
    else
    {
        cm->stopZooming();
    }

    return true;
}

void EditorMode::handleMouseMovedDragType(const OIS::MouseEvent &arg)
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    CameraManager* cm = frameListener->cm;
    if (!cm)
        return;

    Ogre::RaySceneQueryResult& result = frameListener->doRaySceneQuery(arg);

    Ogre::RaySceneQueryResult::iterator itr = result.begin();
    Ogre::RaySceneQueryResult::iterator end = result.end();
    Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();

    InputManager* inputManager = mModeManager->getInputManager();

    switch(inputManager->mDragType)
    {
    case rotateAxisX:
        cm->move(CameraManager::randomRotateX, arg.state.X.rel);
        return;

    case rotateAxisY:
        cm->move(CameraManager::randomRotateY, arg.state.Y.rel);
        return;

    case tileSelection:
    case addNewRoom:
    case nullDragType:
        // Since this is a tile selection query we loop over the result set and look for the first object which is actually a tile.
        for (; itr != end; ++itr)
        {
            if (itr->movable == NULL)
                continue;

            // Check to see if the current query result is a tile.
            std::string resultName = itr->movable->getName();

            if (resultName.find("Level_") == std::string::npos)
                continue;

            //Make the mouse light follow the mouse
            //TODO - we should make a pointer to the light or something.
            Ogre::RaySceneQuery* rq = frameListener->getRaySceneQuery();
            Ogre::Real dist = itr->distance;
            Ogre::Vector3 point = rq->getRay().getPoint(dist);
            mSceneMgr->getLight("MouseLight")->setPosition(point.x, point.y, 4.0);

            // Get the x-y coordinates of the tile.

            //warning obsolete cstdio function
            sscanf(resultName.c_str(), "Level_%i_%i", &inputManager->mXPos, &inputManager->mYPos);
            RenderRequest *request = new RenderRequest;

            request->type = RenderRequest::showSquareSelector;
            request->p = static_cast<void*>(&inputManager->mXPos);
            request->p2 = static_cast<void*>(&inputManager->mYPos);

            // Add the request to the queue of rendering operations to be performed before the next frame.
            RenderManager::queueRenderRequest(request);

            // Make sure the "square selector" mesh is visible and position it over the current tile.
            //mSceneMgr->getLight("MouseLight")->setPosition(mMc->xPos, mMc->yPos, 2.0);

            if (!inputManager->mLMouseDown)
                continue;

            // Handle when dragging using the left mouse button

            // Loop over the tiles in the rectangular selection region and set their setSelected flag accordingly.
            //TODO: This function is horribly inefficient, it should loop over a rectangle selecting tiles by x-y coords
            // rather than the reverse that it is doing now.
            std::vector<Tile*> affectedTiles =
                mGameMap->rectangularRegion(inputManager->mXPos, inputManager->mYPos,
                                            inputManager->mLStartDragX, inputManager->mLStartDragY);

            for (int jj = 0; jj < mGameMap->getMapSizeY(); ++jj)
            {
                for (int ii = 0; ii < mGameMap->getMapSizeX(); ++ii)
                {
                    mGameMap->getTile(ii,jj)->setSelected(false, mGameMap->getLocalPlayer());
                }
            }


            for( std::vector<Tile*>::iterator itr =  affectedTiles.begin(); itr != affectedTiles.end(); ++itr )
            {
                (*itr)->setSelected(true, mGameMap->getLocalPlayer());
            }
            break;
        }
        return;

    case mapLight:
    {
        // If we are dragging a map light we need to update its position to the current x-y location.
        if (!inputManager->mLMouseDown)
            return;

        MapLight* tempMapLight = mGameMap->getMapLight(mDraggedMapLight);

        if (tempMapLight != NULL)
            tempMapLight->setPosition((Ogre::Real)inputManager->mXPos, (Ogre::Real)inputManager->mYPos,
                                      tempMapLight->getPosition().z);

        return;
    }

    case tileBrushSelection:
    {
        // If we are drawing with the brush in the map editor.
        if (!inputManager->mLMouseDown)
            return;

        // Loop over the square region surrounding current mouse location
        // and either set the tile type of the affected tiles or create new ones.
        std::vector<Tile*> affectedTiles;
        int radiusSquared = mCurrentTileRadius * mCurrentTileRadius;

        for (int i = -1 * (mCurrentTileRadius - 1); i <= (mCurrentTileRadius - 1); ++i)
        {
            for (int j = -1 * (mCurrentTileRadius - 1); j <= (mCurrentTileRadius - 1); ++j)
            {
                // Check to see if the current location falls inside a circle with a radius of mCurrentTileRadius.
                int distSquared = i * i + j * j;

                if (distSquared > radiusSquared)
                    continue;

                Tile* currentTile = mGameMap->getTile(inputManager->mXPos + i, inputManager->mYPos + j);

                // Check to see if the current tile already exists.
                if (currentTile != NULL)
                {
                    // It does exist so set its type and fullness.
                    affectedTiles.push_back(currentTile);
                    currentTile->setType((Tile::TileType)mCurrentTileType);
                    currentTile->setFullness((Tile::TileType)mCurrentFullness);
                    continue;
                }

                // The current tile does not exist so we need to create it.
                //currentTile = new Tile;
                stringstream ss;

                ss.str(std::string());
                ss << "Level";
                ss << "_";
                ss << inputManager->mXPos + 1;
                ss << "_";
                ss << inputManager->mYPos + 1;

                currentTile = new Tile(inputManager->mXPos + i, inputManager->mYPos + j,
                                       (Tile::TileType)mCurrentTileType, (Tile::TileType)mCurrentFullness);
                currentTile->setName(ss.str());
                mGameMap->addTile(currentTile);
                currentTile->createMesh();
            }
        }

        // Add any tiles which border the affected region to the affected tiles list
        // as they may alo want to switch meshes to optimize polycount now too.
        std::vector<Tile*> borderingTiles = mGameMap->tilesBorderedByRegion(affectedTiles);

        affectedTiles.insert(affectedTiles.end(), borderingTiles.begin(),
                                borderingTiles.end());

        // Loop over all the affected tiles and force them to examine their
        // neighbors.  This allows them to switch to a mesh with fewer
        // polygons if some are hidden by the neighbors.
        for (unsigned int i = 0; i < affectedTiles.size(); ++i)
            affectedTiles[i]->setFullness(affectedTiles[i]->getFullness());
        return;
    }

    default:
        // We are dragging a creature but we want to loop over the result set to find the first tile entry,
        // we do this to get the current x-y location of where the "square selector" should be drawn.
        for (; itr != end; ++itr)
        {
            if (itr->movable == NULL)
                continue;

            // Check to see if the current query result is a tile.
            std::string resultName = itr->movable->getName();

            if (resultName.find("Level_") == std::string::npos)
                continue;

            // Get the x-y coordinates of the tile.
            sscanf(resultName.c_str(), "Level_%i_%i", &inputManager->mXPos, &inputManager->mYPos);

            RenderRequest *request = new RenderRequest;

            request->type = RenderRequest::showSquareSelector;
            request->p = static_cast<void*>(&inputManager->mXPos);
            request->p2 = static_cast<void*>(&inputManager->mYPos);
            // Make sure the "square selector" mesh is visible and position it over the current tile.
        }
        return;

    } // switch drag type
}

bool EditorMode::mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
    CEGUI::GUIContext& ctxt = CEGUI::System::getSingleton().getDefaultGUIContext();
    ctxt.injectMouseButtonDown(Gui::getSingletonPtr()->convertButton(id));

    // If the mouse press is on a CEGUI window ignore it
    CEGUI::Window *tempWindow = ctxt.getWindowContainingMouse();

    InputManager* inputManager = mModeManager->getInputManager();

    if (tempWindow != 0 && tempWindow->getName().compare("EDITORGUI") != 0)
    {
        inputManager->mMouseDownOnCEGUIWindow = true;
        return true;
    }

    inputManager->mMouseDownOnCEGUIWindow = false;

    Ogre::RaySceneQueryResult &result = ODFrameListener::getSingleton().doRaySceneQuery(arg);
    Ogre::RaySceneQueryResult::iterator itr = result.begin();

    std::string resultName;

    // Left mouse button down
    if (id == OIS::MB_Left)
    {
        inputManager->mLMouseDown = true;
        inputManager->mLStartDragX = inputManager->mXPos;
        inputManager->mLStartDragY = inputManager->mYPos;

        // FIXME This should be moved to key use only as it is getting in the way when playing.
        if(arg.state.Y.abs < 0.1 * arg.state.height || arg.state.Y.abs > 0.9 * arg.state.height)
        {
            inputManager->mDragType = rotateAxisX;
            return true;
        }
        else if(arg.state.X.abs > 0.9 * arg.state.width || arg.state.X.abs < 0.1 * arg.state.width)
        {
            inputManager->mDragType = rotateAxisY;
            return true;
        }

        // See if the mouse is over any creatures
        for (; itr != result.end(); ++itr)
        {
            if (itr->movable == NULL)
                continue;

            resultName = itr->movable->getName();

            if (resultName.find("Creature_") == std::string::npos)
                continue;

            // Begin dragging the creature
            Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();
            mSceneMgr->getEntity("SquareSelector")->setVisible(false);

            mDraggedCreature = resultName.substr(
                                    ((std::string) "Creature_").size(),
                                    resultName.size());
            Ogre::SceneNode *node = mSceneMgr->getSceneNode(
                                        mDraggedCreature + "_node");
            ODFrameListener::getSingleton().getCreatureSceneNode()->removeChild(node);
            mSceneMgr->getSceneNode("Hand_node")->addChild(node);
            node->setPosition(0, 0, 0);
            inputManager->mDragType = creature;

            SoundEffectsHelper::getSingleton().playInterfaceSound(SoundEffectsHelper::PICKUP);

            return true;
        }

        // If no creatures are under the  mouse run through the list again to check for lights
        //TODO: These other code blocks that loop over the result list should probably use this same loop structure.
        for (itr = result.begin(); itr != result.end(); ++itr)
        {
            if (itr->movable == NULL)
                continue;

            resultName = itr->movable->getName();

            if (resultName.find("MapLightIndicator_") == std::string::npos)
                continue;

            inputManager->mDragType = mapLight;
            mDraggedMapLight = resultName.substr(((std::string) "MapLightIndicator_").size(),
                                                    resultName.size());

            SoundEffectsHelper::getSingleton().playInterfaceSound(SoundEffectsHelper::PICKUP);

            return true;
        }

        // If no creatures or lights are under the  mouse run through the list again to check for tiles
        for (itr = result.begin(); itr != result.end(); ++itr)
        {
            if (itr->movable == NULL)
                continue;

            if (resultName.find("Level_") == std::string::npos)
                continue;

            // Start by assuming this is a tileSelection drag.
            inputManager->mDragType = tileSelection;

            // If we are in the map editor, use a brush selection if it has been activated.

            if (mBrushMode)
                inputManager->mDragType = tileBrushSelection;

            // If we have selected a room type to add to the map, use a addNewRoom drag type.
            if (mGameMap->getLocalPlayer()->getNewRoomType() != Room::nullRoomType)
            {
                inputManager->mDragType = addNewRoom;
            }

            else
            {
                // If we have selected a trap type to add to the map, use a addNewTrap drag type.
                if (mGameMap->getLocalPlayer()->getNewTrapType() != Trap::nullTrapType)
                    inputManager->mDragType = addNewTrap;
            }

            break;
        }
    }

    // Right mouse button down
    if (id == OIS::MB_Right)
    {
        inputManager->mRMouseDown = true;
        inputManager->mRStartDragX = inputManager->mXPos;
        inputManager->mRStartDragY = inputManager->mYPos;

        // Stop creating rooms, traps, etc.
        inputManager->mDragType = nullDragType;
        mGameMap->getLocalPlayer()->setNewRoomType(Room::nullRoomType);
        mGameMap->getLocalPlayer()->setNewTrapType(Trap::nullTrapType);
        TextRenderer::getSingleton().setText(ODApplication::POINTER_INFO_STRING, "");

        // If we right clicked with the mouse over a valid map tile, try to drop a creature onto the map.
        Tile *curTile = mGameMap->getTile(inputManager->mXPos, inputManager->mYPos);

        if (curTile != NULL)
        {
            mGameMap->getLocalPlayer()->dropCreature(curTile);

            if (mGameMap->getLocalPlayer()->numCreaturesInHand() > 0)
                SoundEffectsHelper::getSingleton().playInterfaceSound(SoundEffectsHelper::DROP);
        }
    }

    if (id == OIS::MB_Middle)
    {
        // See if the mouse is over any creatures
        for (itr = result.begin(); itr != result.end(); ++itr)
        {
            if (itr->movable == NULL)
                continue;

            resultName = itr->movable->getName();

            if (resultName.find("Creature_") == std::string::npos)
                continue;

            Creature* tempCreature = mGameMap->getCreature(resultName.substr(
                                            ((std::string) "Creature_").size(), resultName.size()));

            if (tempCreature != NULL)
                tempCreature->createStatsWindow();

            return true;
        }
    }
    return true;
}


bool EditorMode::mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonUp(Gui::getSingletonPtr()->convertButton(id));

    InputManager* inputManager = mModeManager->getInputManager();

    // If the mouse press was on a CEGUI window ignore it
    if (inputManager->mMouseDownOnCEGUIWindow)
        return true;

    for (int jj = 0; jj < mGameMap->getMapSizeY(); ++jj)
    {
        for (int ii = 0; ii < mGameMap->getMapSizeX(); ++ii)
        {
            mGameMap->getTile(ii,jj)->setSelected(false, mGameMap->getLocalPlayer());
        }
    }

    // Unselect all tiles
    // for (TileMap_t::iterator itr = mMc->gameMap->firstTile(), last = mMc->gameMap->lastTile();
    //         itr != last; ++itr)
    // {
    //     itr->second->setSelected(false,mMc->gameMap->getLocalPlayer());
    // }

    // Right mouse button up
    if (id == OIS::MB_Right)
    {
        inputManager->mRMouseDown = false;
        return true;
    }

    // Left mouse button up
    if (!id != OIS::MB_Left)
        return true;

    inputManager->mLMouseDown = false;

    if(inputManager->mDragType == rotateAxisX)
    {
        inputManager->mDragType = nullDragType;
    }
    else if(inputManager->mDragType == rotateAxisY)
    {
        inputManager->mDragType=nullDragType;
    }

    // Check to see if we are moving a creature
    else if (inputManager->mDragType == creature)
    {
        Ogre::SceneManager* mSceneMgr = RenderManager::getSingletonPtr()->getSceneManager();
        Ogre::SceneNode *node = mSceneMgr->getSceneNode(mDraggedCreature + "_node");
        mSceneMgr->getSceneNode("Hand_node")->removeChild(node);
        ODFrameListener::getSingleton().getCreatureSceneNode()->addChild(node);
        inputManager->mDragType = nullDragType;
        mGameMap->getCreature(mDraggedCreature)->setPosition(Ogre::Vector3((Ogre::Real)inputManager->mXPos,
                                                                           (Ogre::Real)inputManager->mYPos,
                                                                           (Ogre::Real)0));
    }

    // Check to see if we are dragging a map light.
    if (inputManager->mDragType == mapLight)
    {
        MapLight *tempMapLight = mGameMap->getMapLight(mDraggedMapLight);

        if (tempMapLight != NULL)
            tempMapLight->setPosition((Ogre::Real)inputManager->mXPos, (Ogre::Real)inputManager->mYPos,
                                      tempMapLight->getPosition().z);
        return true;
    }

    // Check to see if we are dragging out a selection of tiles or creating a new room
    if (inputManager->mDragType != tileSelection && inputManager->mDragType != addNewRoom
        && inputManager->mDragType != addNewTrap)
        return true;

    // Loop over the valid tiles in the affected region.  If we are doing a tileSelection (changing the tile type and fullness) this
    // loop does that directly.  If, instead, we are doing an addNewRoom, this loop prunes out any tiles from the affectedTiles vector
    // which cannot have rooms placed on them, then if the player has enough gold, etc to cover the selected tiles with the given room
    // the next loop will actually create the room.  A similar pruning is done for traps.
    std::vector<Tile*> affectedTiles = mGameMap->rectangularRegion(inputManager->mXPos,
                                                                   inputManager->mYPos,
                                                                   inputManager->mLStartDragX,
                                                                   inputManager->mLStartDragY);
    std::vector<Tile*>::iterator itr = affectedTiles.begin();

    while (itr != affectedTiles.end())
    {
        Tile *currentTile = *itr;

        // If we are dragging out tiles.
        if (inputManager->mDragType == tileSelection)
        {
            // In the map editor:  Fill the current tile with the new value
            currentTile->setType((Tile::TileType)mCurrentTileType);
            currentTile->setFullness((Tile::TileType)mCurrentFullness);
        }
        else // if(inputManager->mDragType == addNewRoom || inputManager->mDragType == addNewTrap)
        {
            // If the tile already contains a room, prune it from the list of affected tiles.
            if (!currentTile->isBuildableUpon())
            {
                itr = affectedTiles.erase(itr);
                continue;
            }

            // If the currentTile is not empty and claimed, then remove it from the affectedTiles vector.
            if (!(currentTile->getFullness() < 1 && currentTile->getType() == Tile::claimed))
            {
                itr = affectedTiles.erase(itr);
                continue;
            }
        }

        ++itr;
    }

    // If we are adding new rooms the above loop will have pruned out the tiles not eligible
    // for adding rooms to.  This block then actually adds rooms to the remaining tiles.
    if (inputManager->mDragType == addNewRoom && !affectedTiles.empty())
    {
        Room* newRoom = Room::buildRoom(mGameMap, mGameMap->getLocalPlayer()->getNewRoomType(),
                                        affectedTiles, mGameMap->getLocalPlayer(), true);

        if (newRoom == NULL)
        {
            //TODO:  play sound or something.
        }
    }

    // If we are adding new traps the above loop will have pruned out the tiles not eligible
    // for adding traps to.  This block then actually adds traps to the remaining tiles.
    else if (inputManager->mDragType == addNewTrap && !affectedTiles.empty())
    {
        Trap* newTrap = Trap::buildTrap(mGameMap, mGameMap->getLocalPlayer()->getNewTrapType(),
                                        affectedTiles, mGameMap->getLocalPlayer(), true);

        if (newTrap == NULL)
        {
            //TODO:  play sound or something.
        }
    }

    // Add the tiles which border the affected region to the affectedTiles vector since they may need to have their meshes changed.
    std::vector<Tile*> borderTiles = mGameMap->tilesBorderedByRegion(affectedTiles);

    affectedTiles.insert(affectedTiles.end(), borderTiles.begin(),
                            borderTiles.end());

    // Loop over all the affected tiles and force them to examine their neighbors.  This allows
    // them to switch to a mesh with fewer polygons if some are hidden by the neighbors, etc.
    for (itr = affectedTiles.begin(); itr != affectedTiles.end() ; ++itr)
        (*itr)->refreshMesh();

    return true;
}

bool EditorMode::keyPressed(const OIS::KeyEvent &arg)
{
    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    if (frameListener->isTerminalActive())
        return true;

    // Inject key to Gui
    CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyDown((CEGUI::Key::Scan) arg.key);
    CEGUI::System::getSingleton().getDefaultGUIContext().injectChar(arg.text);

    CameraManager& camMgr = *(frameListener->cm);
    InputManager* inputManager = mModeManager->getInputManager();

    switch (arg.key)
    {
    case OIS::KC_F11:
        frameListener->toggleDebugInfo();
        break;

    case OIS::KC_GRAVE:
    case OIS::KC_F12:
        progressMode(ModeManager::CONSOLE);
        frameListener->setTerminalActive(true);
        Console::getSingleton().setVisible(true);
        getKeyboard()->setTextTranslation(OIS::Keyboard::Ascii);
        break;

    case OIS::KC_LEFT:
    case OIS::KC_A:
        inputManager->mDirectionKeyPressed = true;
        camMgr.move(camMgr.moveLeft); // Move left
        break;

    case OIS::KC_RIGHT:
    case OIS::KC_D:
        inputManager->mDirectionKeyPressed = true;
        camMgr.move(camMgr.moveRight); // Move right
        break;

    case OIS::KC_UP:
    case OIS::KC_W:
        inputManager->mDirectionKeyPressed = true;
        camMgr.move(camMgr.moveForward); // Move forward
        break;

    case OIS::KC_DOWN:
    case OIS::KC_S:
        inputManager->mDirectionKeyPressed = true;
        camMgr.move(camMgr.moveBackward); // Move backward
        break;

    case OIS::KC_PGUP:
    case OIS::KC_E:
        camMgr.move(camMgr.moveDown); // Move down
        break;

    case OIS::KC_INSERT:
    case OIS::KC_Q:
        camMgr.move(camMgr.moveUp); // Move up
        break;

    case OIS::KC_HOME:
        camMgr.move(camMgr.rotateUp); // Tilt up
        break;

    case OIS::KC_END:
        camMgr.move(camMgr.rotateDown); // Tilt down
        break;

    case OIS::KC_DELETE:
        camMgr.move(camMgr.rotateLeft); // Turn left
        break;

    case OIS::KC_PGDOWN:
        camMgr.move(camMgr.rotateRight); // Turn right
        break;

    //Toggle mCurrentTileType
    case OIS::KC_R:
    {
        mCurrentTileType = Tile::nextTileType((Tile::TileType)mCurrentTileType);
        std::stringstream tempSS("");
        tempSS << "Tile type:  " << Tile::tileTypeToString((Tile::TileType)mCurrentTileType);
        ODApplication::MOTD = tempSS.str();
    }
        break;

    //Decrease brush radius
    case OIS::KC_COMMA:
        if (mCurrentTileRadius > 1)
            --mCurrentTileRadius;

        ODApplication::MOTD = "Brush size:  "
                                + Ogre::StringConverter::toString(mCurrentTileRadius);
        break;

    //Increase brush radius
    case OIS::KC_PERIOD:
        if (mCurrentTileRadius < 10)
            ++mCurrentTileRadius;

        ODApplication::MOTD = "Brush size:  "
                                + Ogre::StringConverter::toString(mCurrentTileRadius);
        break;

    //Toggle mBrushMode
    case OIS::KC_B:
        mBrushMode = !mBrushMode;
        ODApplication::MOTD = (mBrushMode)
                                ? "Brush mode turned on"
                                : "Brush mode turned off";
        break;

    //Toggle mCurrentFullness
    case OIS::KC_T:
        mCurrentFullness = Tile::nextTileFullness(mCurrentFullness);
        ODApplication::MOTD = "Tile fullness:  "
                                + Ogre::StringConverter::toString(mCurrentFullness);
        break;

    // Quit the Editor Mode
    case OIS::KC_ESCAPE:
        //MapLoader::writeGameMapToFile(std::string("levels/Test.level") + ".out", *mMc->gameMap);
        //mMc->frameListener->requestExit();
        regressMode();
        Gui::getSingletonPtr()->switchGuiMode();
        break;

    // Print a screenshot
    case OIS::KC_SYSRQ:
        ResourceManager::getSingleton().takeScreenshot();
        break;

    case OIS::KC_1:
    case OIS::KC_2:
    case OIS::KC_3:
    case OIS::KC_4:
    case OIS::KC_5:
    case OIS::KC_6:
    case OIS::KC_7:
    case OIS::KC_8:
    case OIS::KC_9:
    case OIS::KC_0:
        handleHotkeys(arg.key);
        break;

    default:
        break;
    }

    return true;
}

bool EditorMode::keyReleased(const OIS::KeyEvent& arg)
{
    CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyUp((CEGUI::Key::Scan) arg.key);

    ODFrameListener* frameListener = ODFrameListener::getSingletonPtr();
    if (frameListener->isTerminalActive())
        return true;

    CameraManager& camMgr = *(frameListener->cm);
    InputManager* inputManager = mModeManager->getInputManager();

    switch (arg.key)
    {
    case OIS::KC_LEFT:
    case OIS::KC_A:
        inputManager->mDirectionKeyPressed = false;
        camMgr.move(camMgr.stopLeft);
        break;

    case OIS::KC_RIGHT:
    case OIS::KC_D:
        inputManager->mDirectionKeyPressed = false;
        camMgr.move(camMgr.stopRight);
        break;

    case OIS::KC_UP:
    case OIS::KC_W:
        inputManager->mDirectionKeyPressed = false;
        camMgr.move(camMgr.stopForward);
        break;

    case OIS::KC_DOWN:
    case OIS::KC_S:
        inputManager->mDirectionKeyPressed = false;
        camMgr.move(camMgr.stopBackward);
        break;

    case OIS::KC_PGUP:
    case OIS::KC_E:
        camMgr.move(camMgr.stopDown);
        break;

    case OIS::KC_INSERT:
    case OIS::KC_Q:
        camMgr.move(camMgr.stopUp);
        break;

    case OIS::KC_HOME:
        camMgr.move(camMgr.stopRotUp);
        break;

    case OIS::KC_END:
        camMgr.move(camMgr.stopRotDown);
        break;

    case OIS::KC_DELETE:
        camMgr.move(camMgr.stopRotLeft);
        break;

    case OIS::KC_PGDOWN:
        camMgr.move(camMgr.stopRotRight);
        break;

    default:
        break;
    }

    return true;
}

void EditorMode::handleHotkeys(OIS::KeyCode keycode)
{
    //keycode minus two because the codes are shifted by two against the actual number
    unsigned int keynumber = keycode - 2;

    CameraManager* cm = ODFrameListener::getSingletonPtr()->cm;
    InputManager* inputManager = mModeManager->getInputManager();

    if (getKeyboard()->isModifierDown(OIS::Keyboard::Shift))
    {
        inputManager->mHotkeyLocationIsValid[keynumber] = true;
        inputManager->mHotkeyLocation[keynumber] = cm->getCameraViewTarget();
    }
    else
    {
        if (inputManager->mHotkeyLocationIsValid[keynumber])
            cm->flyTo(inputManager->mHotkeyLocation[keynumber]);
    }
}

//! Rendering methods
void EditorMode::onFrameStarted(const Ogre::FrameEvent& evt)
{
    CameraManager* cm = ODFrameListener::getSingletonPtr()->cm;
    cm->moveCamera(evt.timeSinceLastFrame);

    mGameMap->getMiniMap()->draw();
    mGameMap->getMiniMap()->swap();
}

void EditorMode::onFrameEnded(const Ogre::FrameEvent& evt)
{
}
