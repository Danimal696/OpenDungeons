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

#include "MissileObject.h"

#include "RenderRequest.h"
#include "RenderManager.h"
#include "GameMap.h"

#include <iostream>
#include <sstream>

MissileObject::MissileObject(GameMap* gameMap, const std::string& nMeshName, const Ogre::Vector3& nPosition) :
    MovableGameEntity(gameMap)
{
    setObjectType(GameEntity::missileobject);

    std::stringstream tempSS;
    tempSS << "Missile_Object_" << gameMap->nextUniqueNumberMissileObj();
    setName(tempSS.str());

    setMeshName(nMeshName);
    setMeshExisting(false);
    setPosition(nPosition);
}

MissileObject::MissileObject(GameMap* gameMap) :
    MovableGameEntity(gameMap)
{
    setObjectType(GameEntity::missileobject);
    setMeshExisting(false);
}

bool MissileObject::doUpkeep()
{
    // TODO: check if we collide with a creature, if yes, do some damage and delete ourselves
    return true;
}

void MissileObject::stopWalking()
{
    MovableGameEntity::stopWalking();
    if(getGameMap()->isServerGameMap())
    {
        // On the client side, this will be done after receiving the
        // corresponding message
        getGameMap()->removeMissileObject(this);
        deleteYourself();
    }
}

void MissileObject::setPosition(const Ogre::Vector3& v)
{
    MovableGameEntity::setPosition(v);
    if(getGameMap()->isServerGameMap())
        return;

    RenderRequest* request = new RenderRequest;
    request->type = RenderRequest::moveSceneNode;
    request->str = getName() + "_node";
    request->vec = v;
    RenderManager::queueRenderRequest(request);
}

ODPacket& operator<<(ODPacket& os, MissileObject *mo)
{
    std::string nMeshName;
    os << mo->getMeshName() << mo->getPosition() << mo->getName()
        << mo->getMoveSpeed();

    return os;
}

ODPacket& operator>>(ODPacket& is, MissileObject *mo)
{
    std::string name;
    std::string meshName;
    Ogre::Vector3 position;
    double moveSpeed;
    is >> meshName;
    mo->setMeshName(meshName);
    is >> position;
    mo->setPosition(position);
    is >> name;
    mo->setName(name);
    is >> moveSpeed;
    mo->setMoveSpeed(moveSpeed);
    return is;
}
