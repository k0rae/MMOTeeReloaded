#ifndef GAME_SERVER_MMO_COMPONENT_H
#define GAME_SERVER_MMO_COMPONENT_H

class CGameContext;

class CServerComponent
{
protected:
	friend class CGameContext;

	CGameContext *m_pGameServer;

	CGameContext *GameServer() const;
	class CConfig *Config() const;
	class IServer *Server() const;
	class IStorage *Storage() const;
	class IConsole *Console() const;
	class CCollision *Collision() const;

public:
	virtual ~CServerComponent() = default;

	/**
	 * Called on console initialization.
	 * Let component register own console commands.
	 */
	virtual void OnConsoleInit() {}
	/**
	 * Called on component initiation.
	 */
	virtual void OnInit() {}
	/**
	 * Called on server shutdown.
	 */
	virtual void OnShutdown() {}
	/**
	 * Called when player joined to the server.
	 * @param ClientID ID of client who joined.
	 */
	virtual void OnPlayerJoined(int ClientID) {}
	/**
	 * Called when player left from the server.
	 * @param ClientID ID of client who left.
	 */
	virtual void OnPlayerLeft(int ClientID) {}
	/**
	 * Called every server tick.
	 * By default, 50 ticks per second.
	 */
	virtual void OnTick() {}
	/**
	 * Called every snapshot.
	 * @param SnappingClient ClientID who snapping.
	 */
	virtual void OnSnap(int SnappingClient) {}
	/**
	 * Called when receiving a network message.
	 * @param ClientID ID of client who sent message(sender).
	 * @param MsgID Unpacked message ID.
	 * @param pRawMsg Packed message. Could be cast to other message type.
	 * @param InGame When client has state 'in game' or 'bot'.
	 * @see CGameContext::OnMessage()
	 */
	virtual void OnMessage(int ClientID, int MsgID, void *pRawMsg, bool InGame) {}
	/**
	 * Called when server loading all tiles. (Called before OnInit and after default tiles loaded)
	 * @param X Tile position by X (in map coordinate system)
	 * @param Y Tile position by Y (in map coordinate system)
	 * @param TileID ID of tile that is loading
	 * @param Front Is front layer loading
	 */
	virtual void OnTileLoad(int X, int Y, int TileID, bool Front) {}

	/**
	 * Called when character is killing
	 * @param KillerID ID of the killer
	 * @param ClientID ID of character
	 * @param Weapon Killed weapon
	 */
	virtual void OnCharacterDeath(int KillerID, int ClientID, int Weapon) {}

	/**
	 * Called when player is logging into account
	 * @param ClientID
	 */
	virtual void OnPlayerLogged(int ClientID) {}
};

#endif // GAME_SERVER_MMO_COMPONENT_H
