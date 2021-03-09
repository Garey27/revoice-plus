/*
*
*    This program is free software; you can redistribute it and/or modify it
*    under the terms of the GNU General Public License as published by the
*    Free Software Foundation; either version 2 of the License, or (at
*    your option) any later version.
*
*    This program is distributed in the hope that it will be useful, but
*    WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*    General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, write to the Free Software Foundation,
*    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*    In addition, as a special exception, the author gives permission to
*    link the code of this program with the Half-Life Game Engine ("HL
*    Engine") and Modified Game Libraries ("MODs") developed by Valve,
*    L.L.C ("Valve").  You must obey the GNU General Public License in all
*    respects for all of the code used other than the HL Engine and MODs
*    from Valve.  If you modify this file, you may extend this exception
*    to your version of the file, but you are not obligated to do so.  If
*    you do not wish to do so, delete this exception statement from your
*    version.
*
*/
#pragma once

class INetChan;
class IGameClient;

#include "archtypes.h"
#include "const.h"
#include "netadr.h"

#include "common_rehlds.h"
#include "userid_rehlds.h"

#ifdef REHLDS_SELF
#include "server.h"
#endif

class INetChan;
class IGameClient;

class IGameClient {
public:
	virtual int GetId() = 0;

	virtual bool IsActive() = 0;
	virtual void SetActive(bool active) = 0;

	virtual bool IsSpawned() = 0;
	virtual void SetSpawned(bool spawned) = 0;

	virtual INetChan* GetNetChan() = 0;

	virtual sizebuf_t* GetDatagram() = 0;

	virtual edict_t* GetEdict() = 0;

	virtual USERID_t* GetNetworkUserID() = 0;

	virtual const char* GetName() = 0;

	virtual bool IsConnected() = 0;
	virtual void SetConnected(bool connected) = 0;

	virtual uint32 GetVoiceStream(int stream_id) = 0;
	virtual void SetLastVoiceTime(double time) = 0;
	virtual double GetLastVoiceTime() = 0;
	virtual bool GetLoopback() = 0;
	virtual struct usercmd_s *GetLastCmd() = 0;

	// this must be the last virtual function in class
#ifdef REHLDS_SELF
	virtual client_t* GetClient() = 0;
#endif
};

class INetChan {
public:
	virtual const netadr_t* GetRemoteAdr() = 0;
	virtual sizebuf_t* GetMessageBuf() = 0;


	// this must be the last virtual function in class
#ifdef REHLDS_SELF
	virtual netchan_t* GetChan() = 0;
#endif
};

#ifndef REHLDS_SELF


#define WSAENOPROTOOPT ENOPROTOOPT

#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif

typedef Vector vec3_t;
typedef struct usercmd_s
{
	short	lerp_msec;      // Interpolation time on client
	byte	msec;           // Duration in ms of command
	vec3_t	viewangles;     // Command view angles.

// intended velocities
	float	forwardmove;    // Forward velocity.
	float	sidemove;       // Sideways velocity.
	float	upmove;         // Upward velocity.
	byte	lightlevel;     // Light level at spot where we are standing.
	unsigned short  buttons;  // Attack buttons
	byte    impulse;          // Impulse command issued.
	byte	weaponselect;	// Current weapon id

// Experimental player impact stuff.
	int		impact_index;
	vec3_t	impact_position;
} usercmd_t;

const int FRAGMENT_MAX_SIZE = 1024;
// Generic fragment structure
typedef struct fragbuf_s
{
	// Next buffer in chain
	fragbuf_s* next;
	// Id of this buffer
	int bufferid;
	// Message buffer where raw data is stored
	sizebuf_t frag_message;
	// The actual data sits here
	byte frag_message_buf[FRAGMENT_MAX_SIZE];
	// Is this a file buffer?
	qboolean isfile;
	// Is this file buffer from memory ( custom decal, etc. ).
	qboolean isbuffer;
	qboolean iscompressed;
	// Name of the file to save out on remote host
	char filename[MAX_PATH];
	// Offset in file from which to read data
	int foffset;
	// Size of data to read at that offset
	int size;
} fragbuf_t;
// Waiting list of fragbuf chains
typedef struct fragbufwaiting_s
{
	// Next chain in waiting list
	fragbufwaiting_s* next;
	// Number of buffers in this chain
	int fragbufcount;
	// The actual buffers
	fragbuf_t* fragbufs;
} fragbufwaiting_t;

// Used as array indexer
typedef enum netsrc_s
{
	NS_CLIENT = 0,
	NS_SERVER,
	NS_MULTICAST,	// xxxMO
	NS_MAX
} netsrc_t;
// 0 == regular, 1 == file stream
enum
{
	FRAG_NORMAL_STREAM = 0,
	FRAG_FILE_STREAM,

	MAX_STREAMS
};

// Flow control bytes per second limits
const float MAX_RATE = 100000.0f;
const float MIN_RATE = 1000.0f;

// Default data rate
const float DEFAULT_RATE = 30000.0f;

// NETWORKING INFO

// Max size of udp packet payload
const int MAX_UDP_PACKET = 4010; // 9 bytes SPLITHEADER + 4000 payload?

// Max length of a reliable message
const int MAX_MSGLEN = 3990; // 10 reserved for fragheader?

// Max length of unreliable message
const int MAX_DATAGRAM = 4000;

// This is the packet payload without any header bytes (which are attached for actual sending)
const int NET_MAX_PAYLOAD = 65536;
enum
{
	FLOW_OUTGOING = 0,
	FLOW_INCOMING,

	MAX_FLOWS
};

// Message data
typedef struct flowstats_s
{
	// Size of message sent/received
	int size;
	// Time that message was sent/received
	double time;
} flowstats_t;
// Network Connection Channel
const int MAX_LATENT = 32;
typedef struct flow_s
{
	// Data for last MAX_LATENT messages
	flowstats_t stats[MAX_LATENT];
	// Current message position
	int current;
	// Time when we should recompute k/sec data
	double nextcompute;
	// Average data
	float kbytespersec;
	float avgkbytespersec;
} flow_t;
typedef struct netchan_s
{
	// NS_SERVER or NS_CLIENT, depending on channel.
	netsrc_t sock;

	// Address this channel is talking to.
	netadr_t remote_address;

	int player_slot;
	// For timeouts.  Time last message was received.
	float last_received;
	// Time when channel was connected.
	float connect_time;

	// Bandwidth choke
	// Bytes per second
	double rate;
	// If realtime > cleartime, free to send next packet
	double cleartime;

	// Sequencing variables
	//
	// Increasing count of sequence numbers
	int incoming_sequence;
	// # of last outgoing message that has been ack'd.
	int incoming_acknowledged;
	// Toggles T/F as reliable messages are received.
	int incoming_reliable_acknowledged;
	// single bit, maintained local
	int incoming_reliable_sequence;
	// Message we are sending to remote
	int outgoing_sequence;
	// Whether the message contains reliable payload, single bit
	int reliable_sequence;
	// Outgoing sequence number of last send that had reliable data
	int last_reliable_sequence;

	void* connection_status;
	int (*pfnNetchan_Blocksize)(void*);

	// Staging and holding areas
	sizebuf_t message;
	byte message_buf[MAX_MSGLEN];

	// Reliable message buffer. We keep adding to it until reliable is acknowledged. Then we clear it.
	int reliable_length;
	byte reliable_buf[MAX_MSGLEN];

	// Waiting list of buffered fragments to go onto queue. Multiple outgoing buffers can be queued in succession.
	fragbufwaiting_t* waitlist[MAX_STREAMS];

	// Is reliable waiting buf a fragment?
	int reliable_fragment[MAX_STREAMS];
	// Buffer id for each waiting fragment
	unsigned int reliable_fragid[MAX_STREAMS];

	// The current fragment being set
	fragbuf_t* fragbufs[MAX_STREAMS];
	// The total number of fragments in this stream
	int fragbufcount[MAX_STREAMS];

	// Position in outgoing buffer where frag data starts
	short int frag_startpos[MAX_STREAMS];
	// Length of frag data in the buffer
	short int frag_length[MAX_STREAMS];

	// Incoming fragments are stored here
	fragbuf_t* incomingbufs[MAX_STREAMS];
	// Set to true when incoming data is ready
	qboolean incomingready[MAX_STREAMS];

	// Only referenced by the FRAG_FILE_STREAM component
	// Name of file being downloaded
	char incomingfilename[MAX_PATH];

	void* tempbuffer;
	int tempbuffersize;

	// Incoming and outgoing flow metrics
	flow_t flow[MAX_FLOWS];
} netchan_t;
#define MAX_PHYSINFO_STRING 256
typedef struct clientdata_s
{
	vec3_t				origin;
	vec3_t				velocity;

	int					viewmodel;
	vec3_t				punchangle;
	int					flags;
	int					waterlevel;
	int					watertype;
	vec3_t				view_ofs;
	float				health;

	int					bInDuck;

	int					weapons; // remove?

	int					flTimeStepSound;
	int					flDuckTime;
	int					flSwimTime;
	int					waterjumptime;

	float				maxspeed;

	float				fov;
	int					weaponanim;

	int					m_iId;
	int					ammo_shells;
	int					ammo_nails;
	int					ammo_cells;
	int					ammo_rockets;
	float				m_flNextAttack;

	int					tfstate;

	int					pushmsec;

	int					deadflag;

	char				physinfo[MAX_PHYSINFO_STRING];

	// For mods
	int					iuser1;
	int					iuser2;
	int					iuser3;
	int					iuser4;
	float				fuser1;
	float				fuser2;
	float				fuser3;
	float				fuser4;
	vec3_t				vuser1;
	vec3_t				vuser2;
	vec3_t				vuser3;
	vec3_t				vuser4;
} clientdata_t;
// Info about weapons player might have in his/her possession
typedef struct weapon_data_s
{
	int			m_iId;
	int			m_iClip;

	float		m_flNextPrimaryAttack;
	float		m_flNextSecondaryAttack;
	float		m_flTimeWeaponIdle;

	int			m_fInReload;
	int			m_fInSpecialReload;
	float		m_flNextReload;
	float		m_flPumpTime;
	float		m_fReloadTime;

	float		m_fAimedDamage;
	float		m_fNextAimBonus;
	int			m_fInZoom;
	int			m_iWeaponState;

	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
} weapon_data_t;

// For entityType below
#define ENTITY_NORMAL           (1<<0)
#define ENTITY_BEAM             (1<<1)
#define ENTITY_UNINITIALIZED    (1<<30)

// Entity state is used for the baseline and for delta compression of a packet of 
//  entities that is sent to a client.
typedef struct entity_state_s entity_state_t;

struct entity_state_s
{
	// Fields which are filled in by routines outside of delta compression
	int			entityType;
	// Index into cl_entities array for this entity.
	int			number;
	float		msg_time;

	// Message number last time the player/entity state was updated.
	int			messagenum;

	// Fields which can be transitted and reconstructed over the network stream
	vec3_t		origin;
	vec3_t		angles;

	int			modelindex;
	int			sequence;
	float		frame;
	int			colormap;
	short		skin;
	short		solid;
	int			effects;
	float		scale;

	byte		eflags;

	// Render information
	int			rendermode;
	int			renderamt;
	color24		rendercolor;
	int			renderfx;

	int			movetype;
	float		animtime;
	float		framerate;
	int			body;
	byte		controller[4];
	byte		blending[4];
	vec3_t		velocity;

	// Send bbox down to client for use during prediction.
	vec3_t		mins;
	vec3_t		maxs;

	int			aiment;
	// If owned by a player, the index of that player ( for projectiles ).
	int			owner;

	// Friction, for prediction.
	float		friction;
	// Gravity multiplier
	float		gravity;

	// PLAYER SPECIFIC
	int			team;
	int			playerclass;
	int			health;
	qboolean	spectator;
	int			weaponmodel;
	int			gaitsequence;
	// If standing on conveyor, e.g.
	vec3_t		basevelocity;
	// Use the crouched hull, or the regular player hull.
	int			usehull;
	// Latched buttons last time state updated.
	int			oldbuttons;
	// -1 = in air, else pmove entity number
	int			onground;
	int			iStepLeft;
	// How fast we are falling
	float		flFallVelocity;

	float		fov;
	int			weaponanim;

	// Parametric movement overrides
	vec3_t		startpos;
	vec3_t		endpos;
	float		impacttime;
	float		starttime;

	// For mods
	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;
};

typedef struct packet_entities_s
{
	int num_entities;
	unsigned char flags[32];
	entity_state_t* entities;
} packet_entities_t;

typedef struct client_frame_s
{
	double senttime;
	float ping_time;
	clientdata_t clientdata;
	weapon_data_t weapondata[64];
	packet_entities_t entities;
} client_frame_t;

typedef struct event_state_s event_state_t;

typedef struct event_info_s event_info_t;
typedef unsigned int	string_t;

typedef struct event_args_s
{
	int		flags;

	// Transmitted
	int		entindex;

	float	origin[3];
	float	angles[3];
	float	velocity[3];

	int		ducking;

	float	fparam1;
	float	fparam2;

	int		iparam1;
	int		iparam2;

	int		bparam1;
	int		bparam2;
} event_args_t;

struct event_info_s
{
	unsigned short index;			  // 0 implies not in use

	short packet_index;      // Use data from state info for entity in delta_packet .  -1 implies separate info based on event
							 // parameter signature
	short entity_index;      // The edict this event is associated with

	float fire_time;        // if non-zero, the time when the event should be fired ( fixed up on the client )

	event_args_t args;

	// CLIENT ONLY	
	int	  flags;			// Reliable or not, etc.

};
#define MAX_EVENT_QUEUE 64
struct event_state_s
{
	struct event_info_s ei[MAX_EVENT_QUEUE];
};
const int MAX_INFO_STRING = 256;

typedef FILE* FileHandle_t;
typedef struct client_s
{
	qboolean active;
	qboolean spawned;
	qboolean fully_connected;
	qboolean connected;
	qboolean uploading;
	qboolean hasusrmsgs;
	qboolean has_force_unmodified;
	netchan_t netchan;
	int chokecount;
	int delta_sequence;
	qboolean fakeclient;
	qboolean proxy;
	usercmd_t lastcmd;
	double connecttime;
	double cmdtime;
	double ignorecmdtime;
	float latency;
	float packet_loss;
	double localtime;
	double nextping;
	double svtimebase;
	sizebuf_t datagram;
	byte datagram_buf[MAX_DATAGRAM];
	double connection_started;
	double next_messagetime;
	double next_messageinterval;
	qboolean send_message;
	qboolean skip_message;
	client_frame_t* frames;
	event_state_t events;
	edict_t* edict;
	const edict_t* pViewEntity;
	int userid;
	USERID_t network_userid;
	char userinfo[MAX_INFO_STRING];
	qboolean sendinfo;
	float sendinfo_time;
	char hashedcdkey[64];
	char name[32];
	int topcolor;
	int bottomcolor;
	int entityId;
	resource_t resourcesonhand;
	resource_t resourcesneeded;
	FileHandle_t upload;
	qboolean uploaddoneregistering;
	customization_t customdata;
	int crcValue;
	int lw;
	int lc;
	char physinfo[MAX_INFO_STRING];
	qboolean m_bLoopback;
	uint32 m_VoiceStreams[2];
	double m_lastvoicetime;
	int m_sendrescount;
} client_t;

#endif // _WIN32

class IRehldsServerStatic {
public:
	virtual ~IRehldsServerStatic() { }

	virtual int GetMaxClients() = 0;
	virtual bool IsLogActive() = 0;
	virtual IGameClient* GetClient(int id) = 0;
	virtual client_t* GetClient_t(int id) = 0;
	virtual int GetIndexOfClient_t(client_t* client) = 0;
};

class IRehldsServerData {
public:
	virtual ~IRehldsServerData() { }

	virtual const char* GetModelName() = 0;
	virtual const char* GetName() = 0;
	virtual uint32 GetWorldmapCrc() = 0;
	virtual uint8* GetClientDllMd5() = 0;
	virtual sizebuf_t* GetDatagram() = 0;
	virtual sizebuf_t* GetReliableDatagram() = 0;

	virtual void SetModelName(const char* modelname) = 0;
	virtual void SetConsistencyNum(int num) = 0;
	virtual int GetConsistencyNum() = 0;
	virtual int GetResourcesNum() = 0;
	virtual int GetDecalNameNum() = 0;

	virtual double GetTime() = 0;
	virtual void SetResourcesNum(int num) = 0;
	virtual struct resource_s *GetResource(int index) = 0;
	virtual void SetName(const char* name) = 0;
	virtual class ISteamGameServer *GetSteamGameServer() = 0;
	virtual struct netadr_s *GetNetFrom() = 0;
};
