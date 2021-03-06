#ifndef CONNECTION_H
#define CONNECTION_H

#include "../Base/typeDefs.h"
#include "../Base/win32.h"
#include "../Base/winsock.h"
#include "../Base/Atomic.h"
#include "../Base/ServerDefs.h"
#include "../Base/Enums/ConnectionEnum.h"
#include "../Base/Enums/AccountEnums.h"
#include "../Base/WorkItems.h"

#include "../Network/Crypt.h"
#include "../Opcodes/Opcodes.h"
#include "../Models/Player.h"

struct Account {
	UID		id;
	UID		dbId;
	DWORD	flags;
	UINT64  remainingPlayTimeUTC;
	UINT64  lastOnlineUTC;

	inline const BOOL HasFlag(const EAccountFlags flag) const noexcept {
		return flags & flag;
	}
	inline void AddFlag(const EAccountFlags flag) noexcept {
		flags |= flag;
	}
	inline void RemoveFlag(const EAccountFlags flag) noexcept {
		flags &= !flag;
	}
};

struct Connection {
	UID id; //index

	a_flag  connected;
	a_flag  inLobby;
	a_flag  changingWorldServer;

	UINT16   playerCount;
	Player * players;
	Player * selectedPlayer;
	DWORD	flags;

	inline const BOOL HasFlag(const EConnectionFlags flag) const noexcept {
		return flags & flag;
	}
	inline void AddFlag(const EConnectionFlags flag) noexcept {
		flags |= flag;
	}
	inline void RemoveFlag(const EConnectionFlags flag) noexcept {
		flags &= !flag;
	}
	inline Player * SelectPlayer(UINT32 dbId) {
		if (!players) {
			return nullptr;
		}

		for (UINT32 i = 0; i < playerCount; i++) {
			if (players[i].dbId == dbId) {
				selectedPlayer = players + i;
				break;
			}
		}

		return selectedPlayer;
	}
};
struct ConnectionNetPartial :Work {

	ConnectionNetPartial() : Work(WorkItemType_RecvFromConnection) {}

	UID		id; //index
	SOCKET	sock;
	DWORD	flags;

	UINT16	recvState;
	ClientOpcodes	opcode;
	UINT16  packetSize;
	UINT8*	recvBuffer;
	WSABUF	wsaBuff;

	Session session;

	inline const BOOL HasFlag(const EConnectionNetFlags flag) const noexcept {
		return flags & flag;
	}
	inline void AddFlag(const EConnectionNetFlags flag) noexcept {
		flags |= flag;
	}
	inline void RemoveFlag(const EConnectionNetFlags flag) noexcept {
		flags &= !flag;
	}
};

#endif
