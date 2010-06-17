#ifndef ROOMQUARTERS_H
#define ROOMQUARTERS_H

#include "Room.h"

class RoomQuarters : public Room
{
	public:
		RoomQuarters();

		// Functions overriding virtual functions in the Room base class.
		void absorbRoom(Room *r);
		void doUpkeep(Room *r);
		void addCoveredTile(Tile* t, double nHP = Room::defaultTileHP);
		void removeCoveredTile(Tile* t);
		void clearCoveredTiles();

		// Functions specific to this class.
		vector<Tile*> getOpenTiles();
		bool claimTileForSleeping(Tile *t, Creature *c);
		bool releaseTileForSleeping(Tile *t, Creature *c);
		Tile* getLocationForBed(int xDim, int yDim);
		void destroyBedMeshes();

	private:
		bool tileCanAcceptBed(Tile *tile, int xDim, int yDim);

		map<Tile*,Creature*> creatureSleepingInTile;
		map<Tile*,bool> bedOrientationForTile;
};

#endif

