/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef __CYGWIN__
#undef  _WIN32 // qqshka: this way it compiled on my CYGWIN
#endif

#ifdef __GNUC__
#define LittleLong(x) ({ typeof(x) _x = (x); _x = (((unsigned char *)&_x)[0]|(((unsigned char *)&_x)[1]<<8)|(((unsigned char *)&_x)[2]<<16)|(((unsigned char *)&_x)[3]<<24)); _x; })
#define LittleShort(x) ({ typeof(x) _x = (x); _x = (((unsigned char *)&_x)[0]|(((unsigned char *)&_x)[1]<<8)); _x; })
#else
#define LittleLong(x) (x)
#define LittleShort(x) (x)
#endif

#ifdef _WIN32
	#include <conio.h>
	#include <winsock.h>	//this includes windows.h and is the reason for much compiling slowness with windows builds.
	#include <mmsystem.h>
	#include <stdlib.h>

	#ifdef _MSC_VER
		#pragma comment (lib, "wsock32.lib")
	#endif
	#define qerrno WSAGetLastError()
	#define EWOULDBLOCK WSAEWOULDBLOCK
	#define EINPROGRESS WSAEINPROGRESS
	#define ECONNREFUSED WSAECONNREFUSED
	#define ENOTCONN WSAENOTCONN

	//we have special functions to properly terminate sprintf buffers in windows.
	//we assume other systems are designed with even a minor thought to security.
	#if !defined(__MINGW32_VERSION)
		#define unlink _unlink	//why do MS have to be so awkward?
	#else
		#define unlink remove	//seems mingw misses something
	#endif

	#ifdef _MSC_VER
		//okay, so warnings are here to help... they're ugly though.
		#pragma warning(disable: 4761)	//integral size mismatch in argument
		#pragma warning(disable: 4244)	//conversion from float to short
		#pragma warning(disable: 4018)	//signed/unsigned mismatch
	#endif

#elif defined(__CYGWIN__)

	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/errno.h>
	#include <arpa/inet.h>
	#include <stdarg.h>
	#include <netdb.h>
	#include <stdlib.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
	#include <ctype.h>

	#define ioctlsocket ioctl
	#define closesocket close

#elif defined(__linux__) || defined(ixemul)
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netinet/tcp.h>
	#include <netdb.h>
	#include <stdarg.h>
	#include <stdlib.h>
	#include <string.h>
	#include <errno.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
	#include <ctype.h>

	#define ioctlsocket ioctl
	#define closesocket close
#else
#error "Please insert required headers here"
//try the cygwin ones
#endif

#ifndef EAGAIN
#define EAGAIN EWOULDBLOCK
#endif

#ifndef pgetaddrinfo
	#ifndef _WIN32
		#define pgetaddrinfo getaddrinfo
		#define pfreeaddrinfo freeaddrinfo
	#endif
#endif
#ifndef SOCKET
	#define SOCKET int
#endif
#ifndef INVALID_SOCKET
	#define INVALID_SOCKET -1
#endif
#ifndef qerrno
	#define qerrno errno
#endif

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

int		snprintf(char *str, size_t n, char const *fmt, ...);
int		Q_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);

#else

#define Q_vsnprintf vsnprintf

#endif


#if defined(__linux__) || defined(_WIN32) || defined(__CYGWIN__)
size_t	strlcpy (char *dst, char *src, size_t siz);
size_t	strlcat (char *dst, char *src, size_t siz);
#endif

#ifndef _WIN32
//stricmp is ansi, strcasecmp is unix.
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif

#ifndef _WIN32
	#ifndef STDIN
		#define STDIN 0
	#endif
#endif

#ifndef __cplusplus
typedef enum {false, true} qbool;
#else
typedef int qbool;
extern "C" {
#endif

//======================================

#define BUILD_DATE				__DATE__ ", " __TIME__

#define VERSION					"1.3"	//this will be added to the serverinfo
#define QTVSVHEADER				"QTVSV " VERSION "\n"

#define DEFAULT_HOSTNAME			"Unnamed"

#define PROX_DEFAULTSERVER			"localhost:27500"
#define PROX_DEFAULT_LISTEN_PORT	"27599"

#define MAX_PROXY_INBUFFER		4096
#define MAX_PROXY_BUFFER 		(1<<16)		// must be power-of-two, 1<<16 is 2^16 = 65536
#define PREFERED_PROXY_BUFFER	4096		// the ammount of data we try to leave in our input buffer (must be large enough to contain any single mvd frame)
#define MAX_PROXY_UPSTREAM 		2048

#define MAX_PROXY_INFOS			128			// how much settings (count, not bytes) may have one client 

#define MAX_MSGLEN 				1450
#define	FILE_TRANSFER_BUF_SIZE	((MAX_MSGLEN) - 100)

#define	MSG_BUF_SIZE			8192
// qqshka: Its all messy.
// For example ezquake (and FTE?) expect maximum message is MSG_BUF_SIZE == 8192 with mvd header which have not fixed size,
// however fuhquake uses less msg size as I recall.
// mvd header max size is 10 bytes.
// 
// MAX_MVD_SIZE - max size of single mvd message _WITHOUT_ header
#define	MAX_MVD_SIZE			(MSG_BUF_SIZE - 100)

#define DEMOS_UPDATE_TIME		(1000 * 60) // update demos not so fast, this save CPU time

#define RECONNECT_TIME			(1000 * 30)
#define DEMO_RECONNECT_TIME		(1000 * 2)  // if demo end, start play it again faster, do not wait 30 seconds

// I turned this off atm, probably cause problems
//#define SOCKET_CLOSE_TIME		(10*60)		// seconds OS wait before finally close socket, this way we can loose data, but at least ports will be accessible

//======================================

#include "qconst.h"
#include "cmd.h"
#include "cvar.h"
#include "info.h"

typedef struct {
	char name[128];
	int size;
	int time, smalltime;
} availdemo_t;

typedef struct {
	unsigned int readpos;
	unsigned int cursize;
	unsigned int maxsize;
	char *data;
	unsigned int startpos;
	qbool overflowed;
	qbool allowoverflow;
} netmsg_t;

typedef struct { // FIXME: hehe, nice struct

	unsigned int incoming_sequence;

} netchan_t;


//======================================

typedef struct { // hrm, why we need this type?
	char name[MAX_QPATH];
} filename_t;

typedef struct {
	short origin[3];
	unsigned char soundindex;
	unsigned char volume;
	unsigned char attenuation;
} staticsound_t;

typedef struct {
	float gravity;
	float maxspeed;
	float spectatormaxspeed;
	float accelerate;
	float airaccelerate;
	float waterfriction;
	float entgrav;
	float stopspeed;
	float wateraccelerate;
	float friction;
} movevars_t;

typedef struct {
	unsigned char frame;
	unsigned char modelindex;
	unsigned char skinnum;
	short origin[3];
	short velocity[3];
	short angles[3];
	unsigned char effects;
	unsigned char weaponframe;
} player_state_t;

typedef struct {
	unsigned int stats[MAX_STATS];
	char userinfo[MAX_USERINFO];

	int userid;

	int ping;
	int packetloss;
	int frags;
	float entertime;

	qbool active;
	qbool gibbed;
	qbool dead;
	player_state_t current;
	player_state_t old;
} playerinfo_t;

typedef struct {
	unsigned char frame;
	unsigned char modelindex;
	unsigned char colormap;
	unsigned char skinnum;
	short origin[3];
	char angles[3];
	unsigned char effects;
} entity_state_t;

typedef struct {

	entity_state_t baseline;

} entity_t;

#define MAX_DEMO_PACKET_ENTITIES 300

typedef struct {
	int oldframe;
	int numents;
	int maxents;
	entity_state_t ents[MAX_DEMO_PACKET_ENTITIES];
	unsigned short entnums[MAX_DEMO_PACKET_ENTITIES];
} frame_t;

//======================================

// cmd flood protection

#define MAX_FP_CMDS (10)

typedef struct fp_cmd_s {
	unsigned int locked;
	unsigned int cmd_time[MAX_FP_CMDS];
	int   last_cmd;
	int   warnings;
} fp_cmd_t;

typedef unsigned char netadr_t[64];

typedef struct sv_s sv_t;

//'other proxy', these are mvd stream clients.
typedef struct oproxy_s {
	qbool			flushing;
	qbool			drop;

	sv_t			*defaultstream;

	FILE			*file;	//recording a demo
	SOCKET			sock;	//playing to a proxy

	unsigned char	inbuffer[MAX_PROXY_INBUFFER];
	unsigned int	inbuffersize;	//amount of data available.

	unsigned char	buffer[MAX_PROXY_BUFFER];
	unsigned int	buffersize;	//use cyclic buffering.
	unsigned int	bufferpos;

	unsigned int	init_time; // when this client was created, so we can timeout it
	unsigned int	io_time; // when was IO activity, so we can timeout it

	int				id; // user id for this client
	int				pov; // which player id this client tracks, zero if freefly
	int				follow_id; // which qtv client we follow

	fp_cmd_t		fp_s; // say flood protect

	float			clversion; // client version, float

	ctxinfo_t		ctx; // info context, we store here user setting like name and etc

//{ download related
	FILE			*download;
	int				downloadsize;			// total bytes
	int				downloadcount;			// bytes sent
	int				file_percent;
//}

	struct			oproxy_s *next;
} oproxy_t;

typedef	enum {
		SRC_BAD = 0,
		SRC_DEMO,
		SRC_TCP
} src_t; // source type enum

//
// NOTE: if you change source_s you probably need to take a look in close_source() and init_source() also
//
typedef struct source_s { // our source struct
	src_t			type;		// sources type, so we know we use file or socket or whatever

	FILE			*f;			// file
	int				f_size;		// file size

	SOCKET			s;			// socket

} source_t;

//
// NOTE: all this states do not represet is our qtv connected to server or not, since we may have some data
//       in internal qtv buffers and successfully stream data to qtv clients and at the same time be not connected to server.
//
typedef enum {
	qs_parsingQTVheader,	// we in process parsing qtv answer
	qs_parsingconnection,	// we need recive some data before will be able stream
	qs_active				// we are ready forward/stream data
} qtv_state_t;

struct sv_s {	//details about a server connection (also known as stream)

	struct sv_s		*next;					// next cluster->servers connection

	char			ConnectPassword[64];	// password given to server
	char			server[MAX_QPATH];		// something like tcp:localhost:25000 or demo:dag_vs_griffin_dm6.mvd
	int				streamid;				// unique stream/source id

	qbool			ServerQuery;			// we quering some info from server/qtv but not trying stream something
	qbool			DisconnectWhenNooneIsWatching; // no comments

	oproxy_t		*proxies;				// list of clients for this QTV stream

//
// fields above saved on each QTV_Connect()
//

// ======= !!!! READ ME !!!! =======
// we need memset(qtv, 0, sizeof(sv_s)) on each QTV_Connect(), so we will be sure all reset properly,
// but some fields need to be saved between connections, like password and server name,
// so we memset() not whole sv_s struct, but only part after this variable "mem_set_point",
// so if you need save field between connections put it above this variable, otherwise below
//

	char			mem_set_point;

// ======= !!!! READ ME !!!! =======

//
// fields below AUTOMATICALY set to 0 on each QTV_Connect()
//

	// hm, I am not sure where I must put drop, put it here for now, so it reset to 0 on QTV_Connect, hope that right
	qbool			drop;						// something bad, close/free this sv_t

	netadr_t		ServerAddress;				// hrm, ip:port for socket

	float			svversion;					// server/upstream version, NOTE it float: 1.0 or 1.1 or 1.2 and so on...

	// UpStream, requests to qtv
	unsigned char	UpstreamBuffer[MAX_PROXY_UPSTREAM];
	int				UpstreamBufferSize;

	// DownStream, answers on request and sure MVD stream at some point
	unsigned char	buffer[MAX_PROXY_BUFFER];	// this doesn't cycle
	int				buffersize;					// it memmoves down
	int				forwardpoint;				// the point in the stream that we're forwarded up to

	qtv_state_t		qstate;						// in which state our qtv

	// different times in milleseconds
	unsigned int	parsetime;
	unsigned int	curtime;

	unsigned int	io_time; // when was IO activity, so we can timeout it

	unsigned int	NextConnectAttempt;			// time of next reconnect attempt

	char			status[64];					// set this to status, which this sv_t in

	source_t		src;						// our source type

// ============= something what we fill after parsing data =============

	netchan_t		netchan; // FIXME: this will be memset to 0 on each QTV_Connect(), correct?

	char			mapname[256];
	char			hostname[MAX_QPATH];
	char			gamedir[MAX_QPATH];
	char			serverinfo[MAX_SERVERINFO_STRING];
	movevars_t		movevars;

	int				clservercount;

// below fields better memset() to 0 on each level change
	playerinfo_t	players[MAX_CLIENTS];
	filename_t		modellist[MAX_MODELS];
	filename_t		soundlist[MAX_SOUNDS];
	filename_t		lightstyle[MAX_LIGHTSTYLES];
	entity_t		entity[MAX_ENTITIES];
	staticsound_t	staticsound[MAX_STATICSOUNDS];
	int				staticsound_count;
	entity_state_t	spawnstatic[MAX_STATICENTITIES];
	int				spawnstatic_count;

	frame_t			frame[MAX_ENTITY_FRAMES];

	int				cdtrack;

};

typedef struct cluster_s {
	SOCKET tcpsocket;					// tcp listening socket (for mvd and listings and stuff)

	char commandinput[512]; 			// our console input buffer
	int inputlength; 					// how much data we have in console buffer, after user press enter buffer sent to interpreter and this set to 0

	char info[MAX_SERVERINFO_STRING];	// used by cvars which mirrored in serverinfo, this is useless unless UDP will be added to our project

	unsigned int curtime;				// milleseconds

	qbool wanttoexit;					// is this set to true our program will decide to die

	int buildnumber;					// just our buildnumber

	int nextstreamid;                   // this is used to assign id for new source

	sv_t *servers;						// list of connection to servers
	int NumServers;						// how much servers in list

	int	numproxies;						// how much clients we have

	oproxy_t *pendingproxies;			// incoming request queued here, after request is served it unlinked from here

	availdemo_t availdemos[64];			// more demos is a bad idea IMO
	int availdemoscount;
	unsigned int last_demos_update;		// milleseconds, last time when Cluster_BuildAvailableDemoList() was issued, saving CPU time

	int nextUserId;						// out clients need name/userid

} cluster_t;

//
// main.c
//

extern			cvar_t developer, shownet, hostname, admin_password;

extern			cluster_t g_cluster;

extern			qbool cluster_initialized;

void			Cluster_BuildAvailableDemoList(cluster_t *cluster);

//
// sys.c
//

extern char		*redirect_buf;
extern int		redirect_buf_size;

void			Sys_RedirectStart(char *buf, int size); // so Sys_Printf() messages goes to redirect_buf[] too
void			Sys_RedirectStop(void);

unsigned int	Sys_Milliseconds (void);
void			Sys_Printf (cluster_t *cluster, char *fmt, ...);
// this is just wrapper for Sys_Printf(), but print nothing if developer 0
void			Sys_DPrintf(cluster_t *cluster, char *fmt, ...);
void			Sys_Exit(int code);
void			Sys_Error (char *error, ...);
int				Sys_Build_Number (void);
void			*Sys_malloc (size_t size);
char			*Sys_strdup (const char *src);
#define			Sys_free(ptr) if(ptr) { free(ptr); ptr = NULL; }

// return file extension with dot, or empty string if dot not found at all
const char		*Sys_FileExtension (const char *in);
// absolute paths are prohibited
qbool			Sys_SafePath(const char *in);
// return file size, use it on file which open in BINARY mode
int				Sys_FileLength (FILE *f);
// open file in BINARY mode and return file size, if open failed then -1 returned
int				Sys_FileOpenRead (const char *path, FILE **hndl);

// qqshka - hmm, seems in C this is macroses, i don't like macroses,
// however this functions work wrong on unsigned types!!!
#undef max
#undef min

#ifndef min
double			min( double a, double b );
#define KTX_MIN
#endif

#ifndef max
double			max( double a, double b );
#define KTX_MAX
#endif

double			bound( double a, double b, double c );

void			Sys_ReadSTDIN(cluster_t *cluster, fd_set socketset);

// Replace char in string
void			Sys_ReplaceChar(char *s, char from, char to);

unsigned long	Sys_HashKey(const char *str);

//
// token.c
//

#define MAX_COM_TOKEN	1024
extern char		com_token[MAX_COM_TOKEN];

// Parse a token out of a string
char			*COM_Parse (char *data);

// fte token function
char			*COM_ParseToken (char *data, char *out, int outsize, const char *punctuation);

//
// source.c
//

extern cvar_t	maxservers;
extern cvar_t	upstream_timeout;


#define dem_cmd			0
#define dem_read		1
#define dem_set			2
#define dem_multiple	3
#define dem_single		4
#define dem_stats		5
#define dem_all			6

#define dem_mask		7


// checking is buffer contain at least one parse able packet
int				SV_ConsistantMVDData(unsigned char *buffer, int remaining);
// read upstream (mvd or connection header), forawrd stream to clients/qtvs
qbool			Net_ReadStream(sv_t *qtv);
// put string in upstream queue, used memcpy(), that mean we not relay on nul termination but on size
void			Net_QueueUpstream(sv_t *qtv, int size, char *buffer);
// same but with var args
void			Net_UpstreamPrintf(sv_t *qtv, char *fmt, ...);
// actualy write string to socket
qbool			Net_WriteUpStream(sv_t *qtv);
// send connection request to qtv via upstream
void			Net_SendQTVConnectionRequest(sv_t *qtv, char *authmethod, char *challenge);
// connect to qtv/server via TCP
qbool			Net_ConnectToTCPServer(sv_t *qtv, char *ip);
// open demo file on file system and use it as source
qbool			OpenDemoFile(sv_t *qtv, char *demo);
// this memset to 0 too some data and something also
void			QTV_SetupFrames(sv_t *qtv);
// select which method use (demo or tcp or what ever) and connect/open to this source
qbool			QTV_Connect(sv_t *qtv, char *serverurl);
// free() memory and etc, unlink from cluster servers list
void			QTV_Shutdown(cluster_t *cluster, sv_t *qtv);
// malloc(qtv) and init, call QTV_Connect and link to servers list
sv_t			*QTV_NewServerConnection(cluster_t *cluster, char *server, char *password, 
								qbool force, qbool autoclose, qbool noduplicates, qbool query);
// parse qtv connection header
qbool			QTV_ParseHeader(sv_t *qtv);
// check is qtv have source type SRC_DEMO or SRC_TCP, sreamable types in other words, just some small utility
qbool			IsSourceStream(sv_t *qtv);
// parse mvd and store data in some structs,
// so we can generate initial connection mvd data to clients which connect in midle action,
// this is a main qtv's trick
int				QTV_ParseMVD(sv_t *qtv);
// main hook to source.c module called from main.c, here we read/write/parse streams, connects/reconnects/shutdown sources
void			QTV_Run(cluster_t *cluster, sv_t *qtv);

//
// source_cmds.c
//

// add source related commands
void			Source_Init(void);

//
// crc.c
//

unsigned short	QCRC_Block(unsigned char *start, int count);
unsigned short	QCRC_Value(unsigned short crcvalue);

//
// mdfour.c
//

void			Com_BlockFullChecksum(void *buffer, int len, unsigned char *outbuf);

//
// net_utils.c
//

qbool			Net_StringToAddr(char *s, netadr_t *sadr, int defaultport);
qbool			Net_CompareAddress(netadr_t *s1, netadr_t *s2, int qp1, int qp2);
SOCKET			Net_TCPListenPort(int port);

qbool			TCP_Set_KEEPALIVE(int sock);

//
// msg.c
//

void			InitNetMsg	(netmsg_t *b, char *buffer, int bufferlength);
//probably not the place for these any more..
unsigned char	ReadByte	(netmsg_t *b);
unsigned short	ReadShort	(netmsg_t *b);
unsigned int	ReadLong	(netmsg_t *b);
unsigned int	BigLong		(unsigned int val);
unsigned int	SwapLong	(unsigned int val);
float			ReadFloat	(netmsg_t *b);
void			ReadString	(netmsg_t *b, char *string, int maxlen);
void			WriteByte	(netmsg_t *b, unsigned char c);
void			WriteShort	(netmsg_t *b, unsigned short l);
void			WriteLong	(netmsg_t *b, unsigned int l);
void			WriteFloat	(netmsg_t *b, float f);
//no null terminator, convienience function.
void			WriteString2(netmsg_t *b, const char *str);
void			WriteString (netmsg_t *b, const char *str);
void			WriteData	(netmsg_t *b, const char *data, int length);

//
// forward.c
//

extern cvar_t	downstream_timeout;

void			Net_TryFlushProxyBuffer(cluster_t *cluster, oproxy_t *prox);
void			Net_ProxySend(cluster_t *cluster, oproxy_t *prox, char *buffer, int length);
// printf() to downstream to particular "proxy", handy in some cases, instead of Net_ProxySend()
void			Net_ProxyPrintf(oproxy_t *prox, char *fmt, ...);
void			Prox_SendMessage(cluster_t *cluster, oproxy_t *prox, char *buf, int length, int dem_type, unsigned int playermask);
// send text as "svc_print" to particular "proxy"
void			Prox_Printf(cluster_t *cluster, oproxy_t *prox, int dem_type, unsigned int playermask, int level, char *fmt, ...);
// FIXME: move to different file, forward.c intended keep something different than this raw protocol things
void			Prox_SendInitialPlayers(sv_t *qtv, oproxy_t *prox, netmsg_t *msg);
void			Net_SendConnectionMVD(sv_t *qtv, oproxy_t *prox);
oproxy_t		*Net_FileProxy(sv_t *qtv, char *filename);
qbool			Net_StopFileProxy(sv_t *qtv);
//forward the stream on to connected clients
void			SV_ForwardStream(sv_t *qtv, char *buffer, int length);
// register some vars
void			Forward_Init(void);

//
// forward_pending.c
//

extern cvar_t	mvdport;
extern cvar_t	maxclients;
extern cvar_t	allow_http;
extern cvar_t	pending_livetime;

// look for any other proxies wanting to muscle in on the action
void			SV_ReadPendingProxies(cluster_t *cluster);
// serve pending proxies
void			SV_FindProxies(SOCKET qtv_sock, cluster_t *cluster, sv_t *defaultqtv);
// check changes of mvdport variable and do appropriate action
void			SV_CheckMVDPort(cluster_t *cluster);
// register some vars
void			Pending_Init(void);

// just allocate memory and set some fields, do not perform any linkage to any list
oproxy_t		*SV_NewProxy(void *s, qbool socket, sv_t *defaultqtv);
// just free memory and handles, do not perfrom removing from any list
void			SV_FreeProxy(oproxy_t *prox);

// this will check what prox do not have duplicate name
void			Prox_FixName(sv_t *qtv, oproxy_t *prox);

//
// qw.c
//

void			BuildServerData(sv_t *tv, netmsg_t *msg, int servercount);
int				SendList(sv_t *qtv, int first, const filename_t *list, int svc, netmsg_t *msg);
// returns the next prespawn 'buffer' number to use, or -1 if no more
int				Prespawn(sv_t *qtv, int curmsgsize, netmsg_t *msg, int bufnum, int thisplayer);
void			Prox_SendInitialEnts(sv_t *qtv, oproxy_t *prox, netmsg_t *msg);

//
// parse.c
//

void			ParseMessage(sv_t *tv, char *buffer, int length, int to, int mask);

//
// httpsv.c
//

void			HTMLprintf(char *outb, int outl, char *fmt, ...);
void			HTTPSV_SendHTTPHeader(cluster_t *cluster, oproxy_t *dest, char *error_code, char *content_type, qbool nocache);
void			HTTPSV_SendHTMLHeader(cluster_t *cluster, oproxy_t *dest, char *title);
void			HTTPSV_SendHTMLFooter(cluster_t *cluster, oproxy_t *dest);
qbool			HTTPSV_GetHeaderField(char *s, char *field, char *buffer, int buffersize);
char			*HTTPSV_ParsePOST(char *post, char *buffer, int buffersize);
void			HTTPSV_PostMethod(cluster_t *cluster, oproxy_t *pend, char *postdata);
void			HTTPSV_GetMethod(cluster_t *cluster, oproxy_t *pend);

//
// httpsv_generate.c
//

void			HTTPSV_GenerateCSSFile(cluster_t *cluster, oproxy_t *dest);
void			HTTPSV_GenerateNowPlaying(cluster_t *cluster, oproxy_t *dest);
void			HTTPSV_GenerateQTVStub(cluster_t *cluster, oproxy_t *dest, char *streamtype, char *streamid);
void			HTTPSV_GenerateAdmin(cluster_t *cluster, oproxy_t *dest, int streamid, char *postbody);
void			HTTPSV_GenerateDemoListing(cluster_t *cluster, oproxy_t *dest);
void			HTTPSV_GenerateHTMLBackGroundImg(cluster_t *cluster, oproxy_t *dest);


//
// cl_cmds.c
//

extern cvar_t	floodprot;

void			FixSayFloodProtect(void);

void			Proxy_ReadProxies(sv_t *qtv);
void			Cl_Cmds_Init(void);

//
// fs.c
//

FILE			*FS_OpenFile(char *gamedir, char *filename, int *size);
// open and load file in memory.
// may be used in two ways: 
// 1) user provide buffer, in this case "size" provides buffer size.
// 2) or function automatically allocate it, in this case need _FREE_ memory when it no more needed.
//
// in both cases after returning from function "size" will reflect actual data length.
char			*FS_ReadFile(char *gamedir, char *filename, char *buf, int *size);
void			FS_StripPathAndExtension(char *filepath);

#ifdef __cplusplus
}
#endif
