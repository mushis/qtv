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


//connection notes
//The connection is like http.
//The stream starts with a small header.
//The header is a list of 'key: value' pairs, seperated by new lines.
//The header ends with a totally blank line.
//to record an mvd from telnet or somesuch, you would use:
//"QTV\nRAW: 1\n\n"

//VERSION: a list of the different qtv protocols supported. Multiple versions can be specified. The first is assumed to be the prefered version.
//RAW: if non-zero, send only a raw mvd with no additional markup anywhere (for telnet use). Doesn't work with challenge-based auth, so will only be accepted when proxy passwords are not required.
//AUTH: specifies an auth method, the exact specs varies based on the method
//		PLAIN: the password is sent as a PASSWORD line
//		MD4: the server responds with an "AUTH: MD4\n" line as well as a "CHALLENGE: somerandomchallengestring\n" line, the client sends a new 'initial' request with CHALLENGE: MD4\nRESPONSE: hexbasedmd4checksumhere\n"
//		CCITT: same as md4, but using the CRC stuff common to all quake engines.
//		if the supported/allowed auth methods don't match, the connection is silently dropped.
//SOURCE: which stream to play from, DEFAULT is special. Without qualifiers, it's assumed to be a tcp address.
//COMPRESSION: Suggests a compression method (multiple are allowed). You'll get a COMPRESSION response, and compression will begin with the binary data.
//SOURCELIST: Asks for a list of active sources from the proxy.
//DEMOLIST:	Asks for a list of available mvd demos.

//Response:
//if using RAW, there will be no header or anything
//Otherwise you'll get a QTVSV %f response (%f being the protocol version being used)
//same structure, terminated by a \n
//AUTH: Server requires auth before proceeding. If you don't support the method the server says, then, urm, the server shouldn't have suggested it.
//CHALLENGE: used with auth
//COMPRESSION: Method of compression used. Compression begins with the raw data after the connection process.
//ASOURCE: names a source
//ADEMO: gives a demo file name



#include "qtv.h"

#ifndef _WIN32
#include <signal.h>
#endif

cvar_t maxservers		= {"maxservers", "100"};
cvar_t upstream_timeout	= {"upstream_timeout", "60"};

#define BUFFERTIME 10	// secords for artificial delay, so we can buffer things properly.

// set source to state SRC_BAD, opened sources must be freed/closed before
// thought direct call to this function need only in QTV_NewServerConnection() and QTV_Connect() and sure in close_source()
void init_source(sv_t *qtv)
{
	memset(&(qtv->src), 0, sizeof(qtv->src));

	qtv->src.s 		= INVALID_SOCKET; // sigh, we need it
	qtv->src.type	= SRC_BAD;
}

// close/free opened resources and set source to state SRC_BAD
void close_source(sv_t *qtv, const char *where)
{
    switch (qtv->src.type)
    {
	case SRC_DEMO:
	case SRC_TCP:
	case SRC_BAD:
		break; // its known sources

	default:
		Sys_Printf(NULL, "Error: %s: unknow source type %d\n", (where ? where : "unknow"), qtv->src.type);
    }

	if (qtv->src.f)
		fclose(qtv->src.f);

	if (qtv->src.s != INVALID_SOCKET)
		closesocket(qtv->src.s);

	init_source(qtv); // set source to state SRC_BAD, its safe call it since we free all resorces like SOCKET and FILE
}


char *strchrrev(char *str, char chr)
{
	char *firstchar = str;
	for (str = str + strlen(str)-1; str>=firstchar; str--)
		if (*str == chr)
			return str;

	return NULL;
}

void Net_SendQTVConnectionRequest(sv_t *qtv, char *authmethod, char *challenge)
{
	qtv->qstate = qs_parsingQTVheader;

	Net_UpstreamPrintf(qtv, "QTV\nVERSION: 1\n");

	if (qtv->ServerQuery)
	{
		Net_UpstreamPrintf(qtv, qtv->ServerQuery == 2 ? "DEMOLIST\n" : "SOURCELIST\n");
	}
	else
	{
		char *at;
		char hash[512];

		at = strchrrev(qtv->server, '@');

		if (at)
		{
			char *str;

			*at = '\0'; // here we modifying qtv->server, OUCH

			if (strncmp(qtv->server, "tcp:", 4))
			{
				str = qtv->server;
			}
			else
			{
			 	if ((str = strchr(qtv->server, ':')))
					str++;
			}

			// hm, str may be NULL, so we send "SOURCE: \n", is this correct?
			Net_UpstreamPrintf(qtv, "SOURCE: %s\n", str ? str : "");

			*at = '@'; // restore qtv->server
		}
		else
		{
			Net_UpstreamPrintf(qtv, "RECEIVE\n");
		}

// qqshka: we do not use RAW at all, so do not bother
//		if (!qtv->ParsingQTVheader)
//		{
//			Net_UpstreamPrintf(qtv, "RAW: 1\n");
//		}
//		else
		{
			if (authmethod)
			{
				if (!strcmp(authmethod, "PLAIN"))
				{
					Net_UpstreamPrintf(qtv, "AUTH: PLAIN\nPASSWORD: \"%s\"\n", qtv->ConnectPassword);
				}
				else if (challenge && strlen(challenge)>=32 && !strcmp(authmethod, "CCITT"))
				{
					unsigned short crcvalue;

					snprintf(hash, sizeof(hash), "%s%s", challenge, qtv->ConnectPassword);
					crcvalue = QCRC_Block(hash, strlen(hash));
					snprintf(hash, sizeof(hash), "0x%X", (unsigned int)QCRC_Value(crcvalue));

					Net_UpstreamPrintf(qtv, "AUTH: CCITT\nPASSWORD: \"%s\"\n", hash);
				}
				else if (challenge && strlen(challenge)>=8 && !strcmp(authmethod, "MD4"))
				{
					unsigned int md4sum[4];

					snprintf(hash, sizeof(hash), "%s%s", challenge, qtv->ConnectPassword);
					Com_BlockFullChecksum (hash, strlen(hash), (unsigned char*)md4sum);
					snprintf(hash, sizeof(hash), "%X%X%X%X", md4sum[0], md4sum[1], md4sum[2], md4sum[3]);

					Net_UpstreamPrintf(qtv, "AUTH: MD4\nPASSWORD: \"%s\"\n", hash);
				}
				else if (!strcmp(authmethod, "NONE"))
				{
					Net_UpstreamPrintf(qtv, "AUTH: NONE\nPASSWORD: \n");
				}
				else
				{
					qtv->drop = true;
					qtv->UpstreamBufferSize = 0;
					Sys_Printf(NULL, "Auth method %s was not usable\n", authmethod);
					return;
				}
			}
			else
			{
				Net_UpstreamPrintf(qtv, "AUTH: MD4\nAUTH: CCITT\nAUTH: PLAIN\nAUTH: NONE\n");
			}
		}
	}

	Net_UpstreamPrintf(qtv, "\n");
}

qbool Net_ConnectToTCPServer(sv_t *qtv, char *ip)
{
	int err;
	netadr_t from;
	unsigned long nonblocking = true;
#ifdef SOCKET_CLOSE_TIME
	struct linger	lingeropt;
#endif

	if (qtv->src.type != SRC_TCP) {
		Sys_Printf(NULL, "Source set to wrong type %d, need %d = SRC_TCP\n", qtv->src.type, SRC_TCP);
		return false;
	}

	if (qtv->src.s != INVALID_SOCKET) {
		Sys_Printf(NULL, "Forcing close of source socket, bug\n");
		close_source(qtv, "Net_ConnectToTCPServer");
		qtv->src.type = SRC_TCP; // need SRC_TCP type
	}

	if (!Net_StringToAddr(ip, &qtv->ServerAddress, 27500))
	{
		Sys_Printf(NULL, "Unable to resolve %s\n", ip);
		return false;
	}

	qtv->src.s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (qtv->src.s == INVALID_SOCKET)
		return false;

#ifdef SOCKET_CLOSE_TIME
	// hard close: in case of closesocket(), socket will be closen after SOCKET_CLOSE_TIME or earlier
	memset(&lingeropt, 0, sizeof(lingeropt));
	lingeropt.l_onoff  = 1;
	lingeropt.l_linger = SOCKET_CLOSE_TIME;

	if (setsockopt(qtv->src.s, SOL_SOCKET, SO_LINGER, (void*)&lingeropt, sizeof(lingeropt)) == -1)
		return false;
#endif

	memset(&from, 0, sizeof(from));
	((struct sockaddr*)&from)->sa_family = ((struct sockaddr*)&qtv->ServerAddress)->sa_family;

	if (bind(qtv->src.s, (struct sockaddr *)&from, sizeof(from)) == -1)
		return false;

	if (ioctlsocket (qtv->src.s, FIONBIO, &nonblocking) == -1)
		return false;

	if (connect(qtv->src.s, (struct sockaddr *)&qtv->ServerAddress, sizeof(qtv->ServerAddress)) == INVALID_SOCKET)
	{
		err = qerrno;
		if (err != EINPROGRESS && err != EAGAIN && err != EWOULDBLOCK)	//bsd sockets are meant to return EINPROGRESS, but some winsock drivers use EWOULDBLOCK instead. *sigh*...
			return false;
	}

	if (!TCP_Set_KEEPALIVE(qtv->src.s))
		return false;

	//read the notes at the start of this file for what these text strings mean
	Net_SendQTVConnectionRequest(qtv, NULL, NULL);
	return true;
}

qbool OpenDemoFile(sv_t *qtv, char *demo)
{
	if (qtv->src.type != SRC_DEMO) {
		Sys_Printf(NULL, "Source set to wrong type %d, need %d = SRC_DEMO\n", qtv->src.type, SRC_DEMO);
		return false;
	}

	if (qtv->src.f) {
		Sys_Printf(NULL, "Forcing close of source file, bug\n");
		close_source(qtv, "OpenDemoFile");
		qtv->src.type = SRC_DEMO; // need SRC_DEMO type
	}

	if(stricmp(".mvd", Sys_FileExtension(demo))) // .mvd demos only
		return false;

	if (!Sys_SafePath(demo)) // absolute paths are prohibited
		return false;

	if ((qtv->src.f = fopen(demo, "rb")))
	{
		qtv->src.f_size = Sys_FileLength(qtv->src.f);

		if (qtv->src.f_size > 0)
		{
			qtv->qstate = qs_parsingconnection; // in demo case we does't require parse QTV header, so set this state
			return true;
		}	
	}

	return false;
}

// this memset to 0 too some data and something also
void QTV_SetupFrames(sv_t *qtv)
{
	int i;

	memset(qtv->frame, 0, sizeof(qtv->frame));

	for (i = 0; i < MAX_ENTITY_FRAMES; i++)
		qtv->frame[i].maxents = MAX_DEMO_PACKET_ENTITIES;
}

qbool QTV_Connect(sv_t *qtv, char *serverurl)
{
	char *at;
	char *ip = NULL;
	int offset = ((int)&(((sv_t *)0)->mem_set_point));

	close_source(qtv, "QTV_Connect");

    // and here we memset() not whole sv_t struct, but start from some offset
	memset(((unsigned char*)qtv + offset), 0, sizeof(*qtv) - offset);

	// and set as much as possibile fields
	qtv->curtime 			= Sys_Milliseconds();
	qtv->NextConnectAttempt = Sys_Milliseconds() + RECONNECT_TIME;	//wait half a minuite before trying to reconnect
	qtv->io_time			= Sys_Milliseconds();

	qtv->qstate				= qs_parsingQTVheader;

	strlcpy(qtv->server, serverurl, sizeof(qtv->server));

	init_source(qtv);

	*qtv->serverinfo = '\0';

/* fixme

	{
		char qtv_version[32];
		snprintf(qtv_version, sizeof(qtv_version), "%g", QTV_VERSION);
		Info_SetValueForStarKey(qtv->serverinfo, "*qtv", 	qtv_version,	sizeof(qtv->serverinfo));
	}

	Info_SetValueForStarKey(qtv->serverinfo, "*version",	"FTEQTV",	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "hostname",	qtv->cluster->hostname,	sizeof(qtv->serverinfo));
	Info_SetValueForStarKey(qtv->serverinfo, "maxclients",	"99",	sizeof(qtv->serverinfo));
	if (!strncmp(qtv->server, "file:", 5))
		Info_SetValueForStarKey(qtv->serverinfo, "server",		"file",	sizeof(qtv->serverinfo));
	else
		Info_SetValueForStarKey(qtv->serverinfo, "server",		qtv->server,	sizeof(qtv->serverinfo));
*/

	ip = qtv->server;

	if (!strncmp(ip, "tcp:", 4))
	{
		qtv->src.type = SRC_TCP;
		ip += 4;
	}
	else if (!strncmp(ip, "demo:", 5))
	{
		qtv->src.type = SRC_DEMO;
		ip += 5;
	}
	else if (!strncmp(ip, "file:", 5))
	{
		qtv->src.type = SRC_DEMO;
		ip += 5;
	}
	else
	{
		qtv->src.type = SRC_BAD;
	}

	at = strchrrev(ip, '@');
	if (at && (qtv->src.type == SRC_DEMO || qtv->src.type == SRC_TCP))
	{
		if (qtv->src.type == SRC_DEMO)
			qtv->src.type = SRC_TCP; // seems we quering demo from other proxy, so use TCP type
		ip = at+1;
	}

	switch(qtv->src.type)
	{
	case SRC_DEMO:

		if (OpenDemoFile(qtv, ip)) {  // seems success
			Sys_Printf(NULL, "Playing from file %s\n", ip);
			QTV_SetupFrames(qtv); // this memset to 0 too some data and something also
			qtv->parsetime = Sys_Milliseconds();
			return true;
		}

		// fail of some kind

		Sys_Printf(NULL, "Unable to open file %s\n", ip);

		close_source(qtv, "QTV_Connect");

		return false;

	case SRC_TCP:

		if (Net_ConnectToTCPServer(qtv, ip)) {  // seems success
			QTV_SetupFrames(qtv); // this memset to 0 too some data and something also
			qtv->parsetime = Sys_Milliseconds() + BUFFERTIME * 1000; // some delay before parse
			return true;
		}

		// fail of some kind

		Sys_Printf(NULL, "Unable to connect to %s\n", ip);

		close_source(qtv, "QTV_Connect");

		return false;
	
	default:
		Sys_Printf(NULL, "Unknown source type %s\n", ip);

		qtv->src.type = SRC_BAD; // so no warnings in close_source()

		close_source(qtv, "QTV_Connect");

		qtv->drop = true; // something seriously wrong, drop it

		return false;
	}
}

void QTV_Shutdown(cluster_t *cluster, sv_t *qtv)
{
	oproxy_t *prox, *old;

	qbool found = false;

   	Sys_Printf(NULL, "Closing source %s\n", qtv->server);

	close_source(qtv, "QTV_Shutdown");

    // unlink qtv from qtv list in cluster
	if (cluster->servers == qtv) // in head
	{
		cluster->servers = qtv->next;
		found = true;
	}
	else
	{
		sv_t *peer;

		for (peer = cluster->servers; peer && peer->next; peer = peer->next)
		{
			if (peer->next == qtv) // in midle/end of list
			{
				peer->next = qtv->next;
				found = true;
				break;
			}
		}
	}

	if (!found)
		Sys_Printf(NULL, "Error: QTV_Shutdown: qtv was't found in list\n");

	// free proxys/clients linked to this source
	for (prox = qtv->proxies; prox; )
	{
		old  = prox;
		prox = prox->next;
		SV_FreeProxy(old);
	}

	cluster->NumServers--; // less server connections

	Sys_free(qtv);
}

sv_t *QTV_NewServerConnection(cluster_t *cluster, char *server, char *password, qbool force, qbool autoclose, qbool noduplicates, qbool query)
{
	sv_t *qtv;

	if (noduplicates)
	{
		for (qtv = cluster->servers; qtv; qtv = qtv->next)
		{
			if (!strcmp(qtv->server, server))
				return qtv; // we alredy have connection to this server
		}
	}

// FIXME:
//	if (autoclose)
//		if (cluster->nouserconnects)
//			return NULL;

	if (cluster->NumServers >= maxservers.integer)
		return NULL; // too much sources opened

	qtv = Sys_malloc(sizeof(sv_t)); // well, Sys_malloc may terminate application if not enought memory, is that good or bad?

	// set up a default config
	strlcpy(qtv->server, PROX_DEFAULTSERVER, sizeof(qtv->server));
	strlcpy(qtv->ConnectPassword, password, sizeof(qtv->ConnectPassword));

	init_source(qtv); // set source to proper state, SRC_BAD, thought direct call to init_source() need only here

	qtv->DisconnectWhenNooneIsWatching = autoclose;

	qtv->ServerQuery = query;

	qtv->streamid = ++cluster->nextstreamid; // new source, bad idea, after some time this may grow to huge values

	qtv->next = cluster->servers; // partially link in

	if (!QTV_Connect(qtv, server) && !force)
	{
		Sys_free(qtv);
		return NULL;
	}

	cluster->servers = qtv; // finish link in

	cluster->NumServers++; // one more server connections

	return qtv;
}

void Net_QueueUpstream(sv_t *qtv, int size, char *buffer)
{
	if (qtv->UpstreamBufferSize < 0 || qtv->UpstreamBufferSize > sizeof(qtv->UpstreamBuffer))
		Sys_Error("Wrongly sized upstream buffer %d", qtv->UpstreamBufferSize); // something seriously wrong

	if (qtv->UpstreamBufferSize + size > sizeof(qtv->UpstreamBuffer))
	{
		Sys_Printf(NULL, "Upstream queue overflowed for %s\n", qtv->server);
		qtv->drop = true;
		return;
	}
	memcpy(qtv->UpstreamBuffer + qtv->UpstreamBufferSize, buffer, size);
	qtv->UpstreamBufferSize += size;
}

void Net_UpstreamPrintf(sv_t *qtv, char *fmt, ...)
{
	va_list		argptr;
	// we need somehow detect truncation, but vsnprintf() different on *nix and windows,
	// this + 10 must guarantee truncation detection by Net_QueueUpstream().
	char		string[MAX_PROXY_UPSTREAM + 10] = {0};
	
	va_start (argptr, fmt);
	Q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	Net_QueueUpstream(qtv, strlen(string), string);
}

qbool Net_WriteUpStream(sv_t *qtv)
{
	int len;

	if (qtv->UpstreamBufferSize < 0 || qtv->UpstreamBufferSize > sizeof(qtv->UpstreamBuffer))
		Sys_Error("Wrongly sized upstream buffer %d", qtv->UpstreamBufferSize); // something seriously wrong

	if (qtv->UpstreamBufferSize && qtv->src.type == SRC_TCP)
	{
	    if (qtv->src.s == INVALID_SOCKET) {
			Sys_Printf(NULL, "BUG: Upstream write in invalid socket\n");
			qtv->drop = true; // cricial, drop it. Hrm, may be just close source so this reconnect it?
			return false;
	    }

		len = send(qtv->src.s, qtv->UpstreamBuffer, qtv->UpstreamBufferSize, 0);

		if (len == 0)
			return false; // hm, nothing was sent

		if (len < 0)
		{
			int err = qerrno;

			if (err != EWOULDBLOCK && err != EAGAIN && err != ENOTCONN)
			{
				if (err)
					Sys_Printf(NULL, "Error: source socket error %i\n", err);
				else
					Sys_Printf(NULL, "Error: server disconnected\n");

				qtv->drop = true; // cricial, drop it, REALLY???
			}

			return false; // nothing was sent
		}

		qtv->UpstreamBufferSize -= len;
		memmove(qtv->UpstreamBuffer, qtv->UpstreamBuffer + len, qtv->UpstreamBufferSize);

		qtv->io_time = qtv->curtime; // update IO activity
	}

	return true; // something was sent, or does't require sending
}

int SV_ConsistantMVDData(unsigned char *buffer, int remaining)
{
	int lengthofs;
	int length;
	int available = 0;

	while(1)
	{
		if (remaining < 2)
			return available;

		//buffer[0] is time

		switch (buffer[1]&dem_mask)
		{
		case dem_set:
			length = 10;

			goto gottotallength;

		case dem_multiple:
			lengthofs = 6;
			break;

		default:
			lengthofs = 2;
			break;
		}

		if (lengthofs + 4 > remaining)
			return available;

		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

		length += lengthofs + 4;

		if (length > 1400) // some demos have it more that 1400, is it really corrupted?
			Sys_Printf(NULL, "Corrupt mvd\n");

gottotallength:

		if (remaining < length)
			return available;
		
		remaining -= length;
		available += length;
		buffer += length;
	}
}

qbool Net_ReadStream(sv_t *qtv)
{
	int maxreadable;
	int read = 0;
	char *buffer;
	int err;

	if (qtv->buffersize < 0 || qtv->buffersize > sizeof(qtv->buffer))
		Sys_Error("Wrongly sized downstream buffer %d", qtv->buffersize);

	maxreadable = MAX_PROXY_BUFFER - qtv->buffersize;
	if (!maxreadable)
		return true;	//full buffer, we filled buffer faster than client suck data, this is bad!

	buffer = qtv->buffer + qtv->buffersize;

	switch (qtv->src.type)
	{
	case SRC_DEMO:
	    // seems Spike trying minimize amount of data in buffer in case of file, this is CPU friendly due to faster memmove
		if (maxreadable > PREFERED_PROXY_BUFFER - qtv->buffersize)
			maxreadable = PREFERED_PROXY_BUFFER - qtv->buffersize;
		if (maxreadable <= 0)
			return true;

		if (!qtv->src.f)
		{
			read = 0; // will close_source()
			Sys_Printf(NULL, "BUG: Upstream read from wrong FILE\n");
			break; // step out of switch()
		}

		read = fread(buffer, 1, maxreadable, qtv->src.f);

		break;

	case SRC_TCP:

		if (qtv->src.s == INVALID_SOCKET)
		{
			read = 0; // will close_source()
			Sys_Printf(NULL, "BUG: Upstream read from invalid socket\n");
			break; // step out of switch()
		}

		read = sizeof(err);
		err = 0;
		getsockopt(qtv->src.s, SOL_SOCKET, SO_ERROR, (char*)&err, &read);

		if (err == ECONNREFUSED)
		{
			Sys_Printf(NULL, "Error: server %s refused connection\n", qtv->server);

			close_source(qtv, "Net_ReadStream");
			qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0; //probably contains initial connection request info

			return false;
		}

		read = recv(qtv->src.s, buffer, maxreadable, 0);

		break;

	default:
		return true; // unknown or SRC_BAD type, ignoring
	}

	if (read > 0)
	{
		qtv->io_time = qtv->curtime; // update IO activity

		qtv->buffersize += read;

/* qqshka: turned this off
		if (!qtv->ParsingQTVheader)	//qtv header being the auth part of the connection rather than the stream
		{

			int forwardable = SV_ConsistantMVDData(qtv->buffer + qtv->forwardpoint, qtv->buffersize - qtv->forwardpoint);

			if (forwardable > 0)
			{
				SV_ForwardStream(qtv, qtv->buffer + qtv->forwardpoint, forwardable);
				qtv->forwardpoint += forwardable;
			}
		}
*/
	}
	else
	{
		err = (read ? qerrno : 0);

		if (read == 0 || (err != EWOULDBLOCK && err != EAGAIN && err != ENOTCONN))	//ENOTCONN can be returned whilst waiting for a connect to finish.
		{
		    switch (qtv->src.type)
		    {
			case SRC_DEMO:
				Sys_Printf(NULL, "Error: End of file %s\n", qtv->server);
				break;

			case SRC_TCP:
				if (read)
					Sys_Printf(NULL, "Error: source socket error %i %s\n", qerrno, qtv->server);
				else
					Sys_Printf(NULL, "Error: server disconnected %s\n", qtv->server);
				break;

			default:
				break;
		    }

			close_source(qtv, "Net_ReadStream");

			return false;
		}
	}

	return true;
}

qbool QTV_ParseHeader(sv_t *qtv)
{
	float svversion;
	int length;
	char *start;
	char *nl;
	char *colon;
	char *end;
	char value[128];
	char challenge[128];
	char authmethod[128];

	if (qtv->qstate != qs_parsingQTVheader)
		return false;

//	qtv->buffer[qtv->buffersize] = 0;
//	Sys_Printf(qtv->cluster, "msg: ---%s---\n", qtv->buffer);

	*authmethod = 0;

	qtv->parsetime = qtv->curtime;

	length = min(6, qtv->buffersize);

	if (strncmp(qtv->buffer, "QTVSV ", length))
	{
		Sys_Printf(NULL, "Server is not a QTV server (or is incompatable)\n");
		qtv->drop = true;
		return false;
	}

	if (length < 6) // FIXME: seems logic is broken, this need be before strncmp(qtv->buffer, "QTVSV ", length) ???
		return false;	//not ready yet

	end = qtv->buffer + qtv->buffersize - 1;
	for (nl = qtv->buffer; nl < end; nl++)
	{
		if (nl[0] == '\n' && nl[1] == '\n')
			break;
	}
	if (nl == end)
		return false;	//we need more header still

	//we now have a complete packet.

	svversion = atof(qtv->buffer + 6);

	// server sent float version, but we compare only major version number here
	if ((int)svversion != (int)QTV_VERSION)
	{
		Sys_Printf(NULL, "QTV server doesn't support a compatable protocol version, returned %.2f, need %.2f\n", svversion, QTV_VERSION);
		qtv->drop = true;
		return false;
	}

	qtv->svversion = svversion; // remember version

	length = (nl - (char*)qtv->buffer) + 2;
	end = nl;
	nl[1] = '\0';
	start = strchr(qtv->buffer, '\n')+1;

	while((nl = strchr(start, '\n')))
	{
		*nl = '\0';
		colon = strchr(start, ':');
		if (colon)
		{
			*colon = '\0';
			colon++;
			while (*colon == ' ')
				colon++;
			COM_ParseToken(colon, value, sizeof(value), NULL);
		}
		else
		{
			colon = "";
			*value = '\0';
		}

		Sys_DPrintf(NULL, "qtv sv, got (%s) (%s)\n", start, value);

		//read the notes at the top of this file for which messages to expect
		if (!strcmp(start, "AUTH"))
			strlcpy(authmethod, value, sizeof(authmethod));
		else if (!strcmp(start, "CHALLENGE"))
			strlcpy(challenge, colon, sizeof(challenge));
		else if (!strcmp(start, "COMPRESSION"))
		{	//we don't support compression, we didn't ask for it.
			Sys_Printf(NULL, "QTV server wrongly used compression\n");

			qtv->drop = true;
			qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0;

			return false;
		}
		else if (!strcmp(start, "PERROR"))
		{ // permanent error
			Sys_Printf(NULL, "\nQTV server error: %s\n\n", colon);

			qtv->drop = true;
			qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0;

			return false;
		}
		else if (!strcmp(start, "TERROR") || !strcmp(start, "ERROR"))
		{ // temp error
			Sys_Printf(NULL, "\nQTV server error: %s\n\n", colon);

			qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0;

			if (qtv->DisconnectWhenNooneIsWatching)
				qtv->drop = true;	//if its a user registered stream, drop it immediatly
			else //otherwise close the socket (this will result in a timeout and reconnect)
				close_source(qtv, "QTV_ParseHeader");

			return false;
		}
		else if (!strcmp(start, "ASOURCE"))
		{
			Sys_Printf(NULL, "SRC: %s\n", colon);
		}
		else if (!strcmp(start, "ADEMO"))
		{
			int size;
			size = atoi(colon);
			colon = strchr(colon, ':');
			if (!colon)
				colon = "";
			else
				colon = colon+1;
			while(*colon == ' ')
				colon++;
			if (size > 1024*1024)
				Sys_Printf(NULL, "DEMO: (%3imb) %s\n", size/(1024*1024), colon);
			else
				Sys_Printf(NULL, "DEMO: (%3ikb) %s\n", size/1024, colon);
		}
		else if (!strcmp(start, "PRINT"))
		{
			Sys_Printf(NULL, "QTV server: %s\n", colon);
		}
		else if (!strcmp(start, "BEGIN"))
		{
			qtv->qstate = qs_parsingconnection;
		}
		else
		{
			Sys_Printf(NULL, "DBG: QTV server responded with a %s key\n", start);
		}

		start = nl+1;
	}

	qtv->buffersize -= length;
	memmove(qtv->buffer, qtv->buffer + length, qtv->buffersize);

	if (qtv->ServerQuery)
	{
		Sys_Printf(NULL, "End of list\n");

		qtv->drop = true;
		qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0;

		return false;
	}
	else if (*authmethod)
	{	//we need to send a challenge response now.
		Net_SendQTVConnectionRequest(qtv, authmethod, challenge);
		return false;
	}
	else if (qtv->qstate != qs_parsingconnection)
	{
		Sys_Printf(NULL, "QTV server sent no begin command - assuming incompatable\n\n");

		qtv->drop = true;
		qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0;

		return false;
	}

	qtv->parsetime = Sys_Milliseconds() + BUFFERTIME * 1000;
	Sys_Printf(NULL, "Connection established, buffering for %i seconds\n", BUFFERTIME);

//	qqshka: i turned this off
//	SV_ForwardStream(qtv, qtv->buffer, qtv->forwardpoint); // fixme: thought this is WRONG!!!!

	return true;
}

qbool IsSourceStream(sv_t *qtv)
{
	return (qtv->src.type == SRC_TCP || qtv->src.type == SRC_DEMO) ? true : false;
}

//we will read out as many packets as we can until we're up to date
//note: this can cause real issues when we're overloaded for any length of time
//each new packet comes with a leading msec byte (msecs from last packet)
//then a type, an optional destination mask, and a 4byte size.
//the 4 byte size is probably excessive, a short would do.
//some of the types have thier destination mask encoded inside the type byte, yielding 8 types, and 32 max players.


//if we've no got enough data to read a new packet, we print a message and wait an extra two seconds. this will add a pause, connected clients will get the same pause, and we'll just try to buffer more of the game before playing.
//we'll stay 2 secs behind when the tcp stream catches up, however. This could be bad especially with long up-time.
//All timings are in msecs, which is in keeping with the mvd times, but means we might have issues after 72 or so days.
//the following if statement will reset the parse timer. It might cause the game to play too soon, the buffersize checks in the rest of the function will hopefully put it back to something sensible.

int QTV_ParseMVD(sv_t *qtv)
{
	int lengthofs;
	int packettime;
	int forwards = 0;

	unsigned int length, nextpackettime;
	unsigned char *buffer;

	if (qtv->qstate <= qs_parsingQTVheader)
		return 0; // we are not ready to parse

	while (qtv->curtime >= qtv->parsetime)
	{
		if (qtv->buffersize < 2)
		{	//not enough stuff to play.
			if (qtv->curtime > qtv->parsetime)
			{
				qtv->parsetime = qtv->curtime + 2 * 1000;	//add two seconds
				if (IsSourceStream(qtv))
					Sys_Printf(NULL, "Not enough buffered\n");
			}
			break;
		}

		buffer = qtv->buffer;

		switch (qtv->buffer[1]&dem_mask)
		{
		case dem_set:
			length = 10;

			if (qtv->buffersize < length)
			{	//not enough stuff to play.
				qtv->parsetime = qtv->curtime + 2 * 1000;	//add two seconds
				if (IsSourceStream(qtv))
					Sys_Printf(NULL, "Not enough buffered\n");

				continue;
			}
			qtv->parsetime += buffer[0];	//well this was pointless

			if (qtv->forwardpoint < length)	//we're about to destroy this data, so it had better be forwarded by now!
			{
				forwards++;
				SV_ForwardStream(qtv, qtv->buffer, length); // fixme: though that WRONG !!!!!
				qtv->forwardpoint += length;
			}

			memmove(qtv->buffer, qtv->buffer + length, qtv->buffersize - (length));
			qtv->buffersize   -= length;
			qtv->forwardpoint -= length;

			continue;

		case dem_multiple:
			lengthofs = 6;
			break; // step out of switch()

		default:

			lengthofs = 2;
			break; // step out of switch()
		}

		if (qtv->buffersize < lengthofs + 4)
		{	//the size parameter doesn't fit.
			if (IsSourceStream(qtv))
				Sys_Printf(NULL, "Not enough buffered\n");
			qtv->parsetime = qtv->curtime + 2 * 1000;	//add two seconds
			break;
		}

		// fixme: that with respect to byte order?
		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

		if (length > MAX_MVD_SIZE)
		{	//FIXME: THIS SHOULDN'T HAPPEN!
			//Blame the upstream proxy!
			Sys_Printf(NULL, "Warning: corrupt input packet (%i) too big! Flushing and reconnecting!\n", length);

			close_source(qtv, "QTV_ParseMVD");

			qtv->buffersize = qtv->forwardpoint = qtv->UpstreamBufferSize = 0;			
			break;
		}

		if (length + lengthofs + 4 > qtv->buffersize)
		{
			if (IsSourceStream(qtv))
				Sys_Printf(NULL, "Not enough buffered\n");
			qtv->parsetime = qtv->curtime + 2 * 1000;	//add two seconds
			break;	//can't parse it yet.
		}

		nextpackettime = qtv->parsetime + buffer[0];

		if (nextpackettime >= qtv->curtime)
			break;

		switch(qtv->buffer[1]&dem_mask)
		{
		case dem_multiple:
			// fixme: that with respect to byte order?
			ParseMessage(qtv, buffer + lengthofs + 4, length, qtv->buffer[1]&dem_mask,
				(buffer[lengthofs-4]<<0) + (buffer[lengthofs-3]<<8) + (buffer[lengthofs-2]<<16) + (buffer[lengthofs-1]<<24));
			break;
		case dem_single:
		case dem_stats:
			ParseMessage(qtv, buffer + lengthofs + 4, length, qtv->buffer[1]&dem_mask, 1<<(qtv->buffer[1]>>3));
			break;
		case dem_read:
		case dem_all:
			ParseMessage(qtv, buffer + lengthofs + 4, length, qtv->buffer[1]&dem_mask, 0xffffffff);
			break;
		default:
			Sys_Printf(NULL, "Message type %i\n", qtv->buffer[1]&dem_mask);
			break;
		}

		length = lengthofs + 4 + length;	//make length be the length of the entire packet

		packettime = buffer[0];

		if (qtv->buffersize)
		{	//svc_disconnect in ParseMessage() can flush our input buffer (to prevent the EndOfDemo part from interfering)

			if (qtv->forwardpoint < length)	//we're about to destroy this data, so it had better be forwarded by now!
			{
				forwards++;
				SV_ForwardStream(qtv, qtv->buffer, length); // fixme: though that WRONG !!!!!
				qtv->forwardpoint += length;
			}

			memmove(qtv->buffer, qtv->buffer+length, qtv->buffersize-(length));
			qtv->buffersize   -= length;
			qtv->forwardpoint -= length;
		}

// qqshka: this was in original qtv, cause overflow in some cases
//		if (qtv->src.type == SRC_DEMO)
//			Net_ReadStream(qtv); // FIXME: remove me

		qtv->parsetime += packettime;

		if (qtv->src.type != SRC_BAD) // parse called even when source is closed, that set wrong reconnect time for demos
			qtv->NextConnectAttempt = qtv->curtime + (qtv->src.type == SRC_DEMO ? DEMO_RECONNECT_TIME : RECONNECT_TIME);
	}

	return forwards;
}

void QTV_Run(cluster_t *cluster, sv_t *qtv)
{
	int oldcurtime; // FIXME: must be unsigned, no?

	if (qtv->DisconnectWhenNooneIsWatching && !qtv->proxies)
	{
		Sys_Printf(NULL, "Stream %s became inactive\n", qtv->server);
		qtv->drop = true;
	}
	
	if (!qtv->drop && qtv->src.type == SRC_TCP && qtv->io_time + 1000 * upstream_timeout.integer <= qtv->curtime)
	{
		Sys_Printf(NULL, "Stream %s timed out\n", qtv->server);
		close_source(qtv, "QTV_Run");
//		qtv->drop = true;
	}

	if (qtv->drop)
	{
		QTV_Shutdown(cluster, qtv);
		return;
	}

	oldcurtime   = qtv->curtime;
	qtv->curtime = Sys_Milliseconds();

	// FIXME: I am suggest restart proxy instead of wrapping
	if (oldcurtime > qtv->curtime)
	{
		Sys_Printf(NULL, "Time wrapped\n");
		qtv->parsetime = qtv->curtime;
	}

	if (qtv->src.type == SRC_BAD)
	{
		if (qtv->curtime >= qtv->NextConnectAttempt)
		{
			if (!QTV_Connect(qtv, qtv->server))
				return;
		}
	}

	if (IsSourceStream(qtv))
	{
		if (!Net_ReadStream(qtv))
		{	//Spike's comment:
			//if we have an error reading it, if it's valid, give up
			//what should we do here?
			//obviously, we need to keep reading the stream to keep things smooth
		}

		Net_WriteUpStream(qtv);
	}

	if (qtv->qstate == qs_parsingQTVheader)
	{
		QTV_ParseHeader(qtv);

		if (qtv->drop)
			return;
	}

	if (!QTV_ParseMVD(qtv))
		SV_ForwardStream(qtv, "", 0); // so we update io activity of QTV clients, so we can timeout they

	Proxy_ReadProxies(qtv); // read/parse/execute input from clients
}

