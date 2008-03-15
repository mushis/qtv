/*
This is the file responsible for handling incoming tcp connections.
This includes mvd recording.
Password checks and stuff are implemented here. This is server side stuff.
*/

#include "qtv.h"
#include "time.h"

cvar_t downstream_timeout = {"downstream_timeout", "60"};

void CheckMVDConsistancy(unsigned char *buffer, int pos, int size)
{
	// code removed, look original qtv sources if u need it
}

void Net_TryFlushProxyBuffer(cluster_t *cluster, oproxy_t *prox)
{
	int length;

	if (prox->drop)
		return;

	if (prox->_buffersize_ < 0 || prox->_buffersize_ > prox->_buffermaxsize_)
	{
		Sys_Printf(cluster, "Net_TryFlushProxyBuffer: prox->buffersize fucked\n");
		prox->drop = true;
		return;
	}

	if (!prox->_buffersize_)
		return;	//already flushed.

//	CheckMVDConsistancy(prox->buffer, 0, prox->buffersize);

	if (prox->file)
	{
		length = fwrite(prox->_buffer_, 1, prox->_buffersize_, prox->file);
		length = ( length == prox->_buffersize_ ) ? length : -1;
	}
	else
	{
		length = send(prox->sock, prox->_buffer_, prox->_buffersize_, 0);
	}

	switch (length)
	{
	case 0:	//eof / they disconnected
// qqshka: think 0 does't mean here they disconnected, so I turned it off
//		prox->drop = true;
		break;

	case -1:
		if (qerrno != EWOULDBLOCK && qerrno != EAGAIN)	//not a problem, so long as we can flush it later.
		{
			Sys_Printf(cluster, "write error to QTV client, dropping\n");
			prox->drop = true;	//drop them if we get any errors
		}
		break;

	default:
		prox->_buffersize_ -= length;
		memmove(prox->_buffer_, prox->_buffer_ + length, prox->_buffersize_);
		prox->io_time = cluster->curtime; // update IO activity
	}
}

void Net_ProxySend(cluster_t *cluster, oproxy_t *prox, char *buffer, int length)
{
	if (prox->_buffersize_ < 0 || prox->_buffersize_ > prox->_buffermaxsize_)
	{
		Sys_Printf(cluster, "Net_ProxySend: prox->buffersize fucked\n");
		prox->drop = true;
		return;
	}

	// can't fit in current buffer
	if (prox->_buffersize_ + length > prox->_buffermaxsize_)
	{
		// try flushing
		Net_TryFlushProxyBuffer(cluster, prox);

		if (prox->_buffersize_ + length > prox->_buffermaxsize_) // damn, still too big.
		{
			// check do it fit if we expand buffer
			if (prox->_bufferautoadjustmaxsize_ >= prox->_buffersize_ + length) 
			{
				// try bigger buffer, but not bigger than prox->_bufferautoadjustmaxsize_
				int  new_size;
				char *new_buf;

				// basically we wants two times bigger buffer but not less than prox->_buffersize_ + length
				new_size = max(prox->_buffersize_ + length, prox->_buffermaxsize_ * 2);
				// at the same time buffer must be not bigger than prox->_bufferautoadjustmaxsize_
				new_size = min(prox->_bufferautoadjustmaxsize_, new_size);
				new_buf  = realloc(prox->_buffer_, new_size);

				if (new_buf)
				{
					Sys_DPrintf(NULL, "EXPANDED %d -> %d\n", prox->_buffermaxsize_, new_size);
					prox->_buffer_        = new_buf;
					prox->_buffermaxsize_ = new_size;
				}
				else
				{
					Sys_DPrintf(NULL, "FAILED TO EXPAND %d -> %d\n", prox->_buffermaxsize_, new_size);
				}
			}
			else
			{
				Sys_DPrintf(NULL, "IMPOSSIBLE TO EXPAND %d -> %d\n", prox->_bufferautoadjustmaxsize_, prox->_buffersize_ + length);
			}

			if (prox->_buffersize_ + length > prox->_buffermaxsize_) // damn, still too big.
			{
				// they're too slow. hopefully it was just momentary lag
				Sys_Printf(NULL, "QTV client is too lagged\n");
				prox->flushing = true;
				return;
			}
		}
	}

	memmove(prox->_buffer_ + prox->_buffersize_, buffer, length);
	prox->_buffersize_ += length;
}

// printf() to downstream to particular "proxy", handy in some cases, instead of Net_ProxySend()
void Net_ProxyPrintf(oproxy_t *prox, char *fmt, ...)
{
	va_list		argptr;
	char		string[2048] = {0};
	
	va_start (argptr, fmt);
	Q_vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	Net_ProxySend(&g_cluster, prox, string, strlen(string));
}

void Prox_SendMessage(cluster_t *cluster, oproxy_t *prox, char *buf, int length, int dem_type, unsigned int playermask)
{
	netmsg_t msg;
	char tbuf[16];

	InitNetMsg(&msg, tbuf, sizeof(tbuf));
	
	WriteByte(&msg, 0);
	WriteByte(&msg, dem_type);
	WriteLong(&msg, length);

	if (dem_type == dem_multiple)
		WriteLong(&msg, playermask);

	if (prox->_buffersize_ + length + msg.cursize > prox->_buffermaxsize_)
	{
		Net_TryFlushProxyBuffer(cluster, prox);	//try flushing
		if (prox->_buffersize_ + length + msg.cursize > prox->_buffermaxsize_)	//damn, still too big.
		{	//they're too slow. hopefully it was just momentary lag
			Sys_Printf(NULL, "QTV client is too lagged\n");
			prox->flushing = true;
			return;
		}
	}


	Net_ProxySend(cluster, prox, msg.data, msg.cursize);

	Net_ProxySend(cluster, prox, buf, length);
}

// send text as "svc_print" to particular "proxy"
void Prox_Printf(cluster_t *cluster, oproxy_t *prox, int dem_type, unsigned int playermask, int level, char *fmt, ...)
{
#define PREFIX_LEN	2

	va_list		argptr;
	char		string[PREFIX_LEN + 1024 * 8];

	if (prox->flushing)
		return; // no thx, we trying empty their buffer
	
	va_start (argptr, fmt);
	Q_vsnprintf (string + PREFIX_LEN, sizeof(string) - PREFIX_LEN, fmt, argptr);
	va_end (argptr);

	// two bytes prefix
	string[0] = svc_print;
	string[1] = level;

	Prox_SendMessage(cluster, prox, string, PREFIX_LEN + strlen(string + PREFIX_LEN) + 1, dem_type, playermask);

#undef PREFIX_LEN
}

// FIXME: move to different file, forward.c intended keep something different than this raw protocol things
void Prox_SendPlayerStats(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN];
	netmsg_t msg;
	int player, snum;

	InitNetMsg(&msg, buffer, sizeof(buffer));

	for (player = 0; player < MAX_CLIENTS; player++)
	{
		for (snum = 0; snum < MAX_STATS; snum++)
		{
			if (qtv->players[player].stats[snum])
			{
				if ((unsigned)qtv->players[player].stats[snum] > 255)
				{
					WriteByte(&msg, svc_updatestatlong);
					WriteByte(&msg, snum);
					WriteLong(&msg, qtv->players[player].stats[snum]);
				}
				else
				{
					WriteByte(&msg, svc_updatestat);
					WriteByte(&msg, snum);
					WriteByte(&msg, qtv->players[player].stats[snum]);
				}
			}
		}

		if (msg.cursize)
		{
			Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_stats|(player<<3), (1<<player));
			msg.cursize = 0;
		}
	}
}

// FIXME: move to different file, forward.c intended keep something different than this raw protocol things
void Prox_SendInitialPlayers(sv_t *qtv, oproxy_t *prox, netmsg_t *msg)
{
	int i, j, flags;
	char buffer[64];

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (!qtv->players[i].active) // interesting, is this set to false if player disconnect from server?
			continue;

		flags =   (DF_ORIGIN << 0) | (DF_ORIGIN << 1) | (DF_ORIGIN << 2)
				| (DF_ANGLES << 0) | (DF_ANGLES << 1) | (DF_ANGLES << 2) // angles is something what changed frequently, so may be not send it?
				| DF_EFFECTS
				| DF_SKINNUM // though it rare thingie, so better send it?
				| (qtv->players[i].dead   ? DF_DEAD : 0)
				| (qtv->players[i].gibbed ? DF_GIB  : 0)
				| DF_WEAPONFRAME // do we so really need it?
				| DF_MODEL; // generally, that why we wrote this function, so YES send this

		if (*qtv->players[i].userinfo && atoi(Info_ValueForKey(qtv->players[i].userinfo, "*spectator", buffer, sizeof(buffer))))
			flags = DF_MODEL; // oh, that spec, just sent his model, may be even better ignore him?

		WriteByte (msg, svc_playerinfo);
		WriteByte (msg, i);
		WriteShort (msg, flags);

		WriteByte (msg, qtv->players[i].current.frame); // always sent

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ORIGIN << j))
				WriteShort (msg, qtv->players[i].current.origin[j]);

		for (j = 0 ; j < 3 ; j++)
			if (flags & (DF_ANGLES << j))
				WriteShort (msg, qtv->players[i].current.angles[j]);

		if (flags & DF_MODEL) // generally, that why we wrote this function, so YES send this
			WriteByte (msg, qtv->players[i].current.modelindex);

		if (flags & DF_SKINNUM)
			WriteByte (msg, qtv->players[i].current.skinnum);

		if (flags & DF_EFFECTS)
			WriteByte (msg, qtv->players[i].current.effects);

		if (flags & DF_WEAPONFRAME)
			WriteByte (msg, qtv->players[i].current.weaponframe);
	}
}

void Net_SendConnectionMVD_1_0(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN*8];
	netmsg_t msg;
	int prespawn;

//	Sys_Printf(NULL, "DBG: Net_SendConnectionMVD: map: %s\n", qtv->mapname);

	if (qtv->qstate != qs_active || !*qtv->mapname || prox->download)
	{
//		Sys_Printf(NULL, "DBG: Net_SendConnectionMVD: set flush\n");
		prox->flushing = true; // so they get Net_SendConnectionMVD() ASAP later
		return;
	}

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->flushing = false;

	BuildServerData(qtv, &msg, qtv->clservercount);
	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, qtv->soundlist, svc_soundlist, &msg);
		Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, qtv->modellist, svc_modellist, &msg);
		Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	Net_TryFlushProxyBuffer(&g_cluster, prox);	//that should be enough data to fill a packet.

	for(prespawn = 0;prespawn>=0;)
	{
		prespawn = Prespawn(qtv, 0, &msg, prespawn, MAX_CLIENTS-1);

		Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	//playerstates arn't actually delta-compressed, so the first send (simply forwarded from server) entirly replaces the old.
	// not really, we need send something from what client/other proxy/whatever will deltaing
	// at least model index is really required, otherwise we got invisible models!
	Prox_SendInitialPlayers(qtv, prox, &msg);
	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	//we do need to send entity states.
	Prox_SendInitialEnts(qtv, prox, &msg);
	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	WriteByte(&msg, svc_stufftext);
	WriteString(&msg, "skins\n");
	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	Net_TryFlushProxyBuffer(&g_cluster, prox);

	if (prox->flushing)
	{
		Sys_Printf(NULL, "Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
	{
		prox->connected_at_least_once = true;
		Net_TryFlushProxyBuffer(&g_cluster, prox);
	}
}

void Net_SendConnectionMVD_NEW(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN*8];
	netmsg_t msg;

//	Sys_Printf(NULL, "DBG: Net_SendConnectionMVD: map: %s\n", qtv->mapname);

	if (qtv->qstate != qs_active || !*qtv->mapname || prox->download)
	{
//		Sys_Printf(NULL, "DBG: Net_SendConnectionMVD: set flush\n");
		prox->flushing = true; // so they get Net_SendConnectionMVD() ASAP later
		return;
	}

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->flushing = false; // so we can catch overflow, set it to PS_WAIT_SOUNDLIST below if no overflow

	// send userlist to prox, do it once, so we do not send it on each level change
	Prox_SendInitialUserList(qtv, prox);

	BuildServerData(qtv, &msg, qtv->clservercount);
	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
	msg.cursize = 0;

	if (prox->flushing)
	{
		Sys_Printf(NULL, "Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
		prox->flushing = PS_WAIT_SOUNDLIST; // no overflow, wait soundlist
}

void Net_SendConnectionMVD(sv_t *qtv, oproxy_t *prox)
{
	if (prox->qtv_ezquake_ext & QTV_EZQUAKE_EXT_DOWNLOAD)
	{
		// for full support of download we need a bit different conntection method
		Net_SendConnectionMVD_NEW(qtv, prox); // new way
	}
	else
	{
		Net_SendConnectionMVD_1_0(qtv, prox); // backward compatibility
	}
}

/*

oproxy_t *Net_FileProxy(sv_t *qtv, char *filename)
{
	oproxy_t *prox;
	FILE *f;

	f = fopen(filename, "wb");
	if (!f)
		return NULL;

	//no full proxy check, this is going to be used by proxy admins, who won't want to have to raise the limit to start recording.

	prox = SV_NewProxy((void*)f, false, qtv);
	prox->qtv_clversion = 1.0; // using VERSION: 1 for file proxy
	prox->qtv_ezquake_ext = 0; // we does't support any qtv ezquake extensions in such case

	prox->next = qtv->proxies;
	qtv->proxies = prox;

	Net_SendConnectionMVD(qtv, prox);

	return prox;
}

qbool Net_StopFileProxy(sv_t *qtv)
{
	oproxy_t *prox;
	for (prox = qtv->proxies; prox; prox = prox->next)
	{
		if (prox->file)
		{
			prox->drop = true;
			return true;
		}
	}
	return false;
}

*/

//
//forward the stream on to connected clients
//
void SV_ForwardStream(sv_t *qtv, char *buffer, int length)
{
	oproxy_t *prox, *next;

	CheckMVDConsistancy(buffer, 0, length);

	// timeout check
	for (prox = qtv->proxies; prox; prox = prox->next)
	{
#if 0 // debug
		if (prox->buffersize)
			Sys_Printf(NULL, "DBG: prx buffer: %5d\n", prox->buffersize);
#endif

		if (prox->file)
			continue; // timeout valid for socket only

		// FIXME: this will drop down stream if our qtv not connected since we does't have any activity on downstream socket
		if (prox->io_time + max(RECONNECT_TIME + 10 * 1000, 1000 * downstream_timeout.integer) <= qtv->curtime)
		{
			Sys_Printf(NULL, "down stream proxy timeout\n");
			prox->drop = true;
		}
	}

	// drop some proxies in head, if dropable
	while (qtv->proxies && qtv->proxies->drop)
	{
		next = qtv->proxies->next;
		SV_FreeProxy(qtv->proxies);
		qtv->proxies = next;
	}

	for (prox = qtv->proxies; prox; prox = prox->next)
	{
		// drop some proxies after "prox", if dropable
		while (prox->next && prox->next->drop)
		{
			next = prox->next->next;
			SV_FreeProxy(prox->next);
			prox->next = next;
		}

		// we're trying to empty thier buffer, NOTE: we use prox->flushing == true so it does't trigger when flushing is PS_WAIT_MODELLIST and etc
		if (prox->flushing == true)
			if (!prox->_buffersize_) // ok, they have empty buffer
				if (qtv->qstate == qs_active) // ok, our qtv is ready
					Net_SendConnectionMVD(qtv, prox); // resend the connection info and set flushing to false

		if (prox->drop)
			continue;

		// add the new data if not flushing and we have appropriate qtv state.
		// in most cases flushing mean client does't have proper game state, so add new data is useless or just wrong.
		// so we wait untill Net_SendConnectionMVD() will set flushing to false.
		if (!prox->flushing && qtv->qstate >= qs_parsingconnection)
			Net_ProxySend(&g_cluster, prox, buffer, length);

		// try and flush it
		Net_TryFlushProxyBuffer(&g_cluster, prox);
	}
}

void Forward_Init(void)
{
	Cvar_Register (&downstream_timeout);
}
