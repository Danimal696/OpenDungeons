#ifndef CLIENTNOTIFICATION_H
#define CLIENTNOTIFICATION_H

#include <string>
using namespace std;
#include <Ogre.h>

/*! \brief A data structure used to pass messages to the clientNotificationProcessor thread.
 *
 */
//TODO:  Make this class a base class and let specific messages be subclasses of this type with each having its own data structure so they don't need the unused fields
class ClientNotification
{
	public:
		enum ClientNotificationType
		{
			creaturePickUp
		};

		ClientNotificationType type;

		void *p;
		void *p2;
};

#endif
