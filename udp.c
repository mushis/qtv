/*
UDP protocl related things. At the moment just support "status" connectionless feature.
*/

#include "qtv.h"

// out of band message id bytes (check protocol.h in QW server for full list)
// M = master, S = server, C = client, A = any
#define	A2C_PRINT			'n'	// print a message on client
#define	A2A_PING			'k'	// respond with an A2A_ACK
#define	A2A_ACK				'l'	// general acknowledgement without info

void UDP_Init(void)
{
	// Init net_message.
	InitNetMsg(&g_cluster.net_message, g_cluster.net_message_buffer, sizeof(g_cluster.net_message_buffer));
}

void SV_CheckUDPPort(cluster_t *cluster, int port)
{
	// remember current time so we does not hammer it too fast.
	cluster->udpport_last_time_check = cluster->curtime;

	// close socket first.
	if (cluster->udpsocket != INVALID_SOCKET)
	{
		closesocket(cluster->udpsocket);
		cluster->udpsocket = INVALID_SOCKET;

		Sys_Printf("%s is now closed\n", "updport");
	}

	// open socket if required.
	if (port)
	{
		SOCKET news = Net_UDPOpenSocket(port);

		if (news != INVALID_SOCKET)
		{
			cluster->udpsocket = news;

			Sys_Printf("%s %d opened\n", "updport", port);
		}
		else
		{
			Sys_Printf("%s %d failed to open\n", "updport", port);
		}
	}
}

/*
================
SVC_Ping

Just responds with an acknowledgement
================
*/
static void SVC_Ping (cluster_t *cluster)
{
	char data = A2A_ACK;
	Net_SendPacket(cluster, 1, &data, &cluster->net_from);
}

/*
================
SVC_Status

Responds with all the info that qplug or qspy can see
================
*/
#define STATUS_OLDSTYLE					0
#define	STATUS_SERVERINFO				1
#define	STATUS_PLAYERS					2
#define	STATUS_SPECTATORS				4
//#define STATUS_SPECTATORS_AS_PLAYERS	8 //for ASE - change only frags: show as "S"
//#define STATUS_SHOWTEAMS				16

static void SVC_Status (cluster_t *cluster)
{
	static char buf_data[MSG_BUF_SIZE]; // static  - so it not allocated each time
	static char tmp[1024]; // static too

	netmsg_t buf;
	int opt = (Cmd_Argc() > 1) ? atoi(Cmd_Argv(1)) : 0; // there possible to provide options for status command.

	InitNetMsg(&buf, buf_data, sizeof(buf_data));

	WriteLong(&buf, (unsigned int)-1);	// -1 sequence means out of band
	WriteByte(&buf, A2C_PRINT);

	if (opt == STATUS_OLDSTYLE || (opt & STATUS_SERVERINFO))
	{
		snprintf(tmp, sizeof(tmp), "%s\n", cluster->info);
		WriteString2(&buf, tmp);
	}

	if (opt == STATUS_OLDSTYLE || (opt & (STATUS_PLAYERS | STATUS_SPECTATORS)))
	{
		int top = 0, bottom = 0, ping = 666, connect_time;
		char *frags = "0", *skin = "";
		sv_t *stream;
		oproxy_t *prx;
		char name[MAX_INFO_KEY];

		for (stream = cluster->servers; stream; stream = stream->next)
		{
			for (prx = stream->proxies; prx; prx = prx->next)
			{
				if (prx->drop)
					continue;

				Info_Get(&prx->ctx, "name", name, sizeof(name));
				connect_time = (int)((cluster->curtime - prx->init_time)/1000/60); // milliseconds to seconds and then to minutes.

				snprintf(tmp, sizeof(tmp), "%i %s %i %i \"%s\" \"%s\" %i %i\n", prx->id, frags, connect_time, ping, name, skin, top, bottom);

				WriteString2(&buf, tmp);
			}
		}
	}

	if (buf.cursize>=buf.maxsize)
	{
		Sys_DPrintf("SVC_Status: buffer overflowed %u/%u\n", buf.cursize, buf.maxsize);
		return; // overflowed
	}

	// send the datagram
	Net_SendPacket(cluster, buf.cursize, buf.data, &cluster->net_from);
}

static void SV_ConnectionlessPacket(cluster_t *cluster)
{
	char	cmd[128] = ""; // nor more than 128 bytes. I don't know such long commands anyway.
	char	*c;

	// skip the -1 marker
	ReadLong(&cluster->net_message);
	// Red string in.
	ReadString(&cluster->net_message, cmd, sizeof(cmd));
	// Tokenize it.
	Cmd_TokenizeString(cmd);

	c = Cmd_Argv(0);

	if (!strcmp(c, "ping") || (c[0] == A2A_PING && (c[1] == 0 || c[1] == '\n')))
		SVC_Ping(cluster);
	else if (!strcmp(c, "status"))
		SVC_Status(cluster);
	else
		Sys_DPrintf("bad connectionless packet from %s:\n%s\n", inet_ntoa(cluster->net_from.sin_addr), cmd);
}

void SV_ReadPackets(cluster_t *cluster)
{
	// now deal with new packets
	while (Net_GetPacket(cluster, &cluster->net_message))
	{
		if (SV_IsBanned(&cluster->net_from))
		{
//			SV_SendBan ();	// tell them we aren't listening...
			continue;
		}

		// check for connectionless packet (0xffffffff) first
		if (*(int *)cluster->net_message.data == -1)
		{
			SV_ConnectionlessPacket(cluster);
			continue;
		}
	}
}