/*
	forward_pending.c
*/

#include "qtv.h"

cvar_t mvdport    		= {"mvdport", PROX_DEFAULT_LISTEN_PORT};
cvar_t maxclients		= {"maxclients", "1000"};
cvar_t allow_http		= {"allow_http", "1"};

// this just can't be done as macro, so I wrote this function
char *QTV_SV_HEADER(oproxy_t *prox, float qtv_ver)
{
	static char header[1024];

	int ext = (prox->qtv_ezquake_ext & QTV_EZQUAKE_EXT_NUM);

	if (ext)
	{
		snprintf(header, sizeof(header), "QTVSV %g\n" QTV_EZQUAKE_EXT ": %d\n", qtv_ver, ext);
	}
	else
	{
		snprintf(header, sizeof(header), "QTVSV %g\n", qtv_ver);
	}

	return header;
}

static void SV_ReadSourceListRequest(cluster_t *cluster, oproxy_t *pend)
{
	sv_t *qtv = NULL;

	// Lists sources that are currently playing.
	Net_ProxyPrintf(pend, "%s", QTV_SV_HEADER(pend, QTV_VERSION));

	if (!cluster->servers)
	{
		Net_ProxyPrintf(pend, "PERROR: No sources currently available\n");
	}
	else
	{
		for (qtv = cluster->servers; qtv; qtv = qtv->next)
			Net_ProxyPrintf(pend, "ASOURCE: %i: %15s: %15s\n", qtv->streamid, qtv->server, qtv->hostname);
	}

	Net_ProxyPrintf(pend, "\n");
	pend->flushing = true;
}

static sv_t *SV_ReadReceiveRequest(cluster_t *cluster, oproxy_t *pend)
{
	sv_t *qtv = NULL;

	// A client connection request without a source.
	if (cluster->NumServers == 1)
	{	
		// Only one stream anyway.
		qtv = cluster->servers;
	}
	else
	{	
		// Try and hunt down an explicit stream (rather than a user-recorded one).
		int numfound = 0;
		sv_t *suitable = NULL;
		
		for (qtv = cluster->servers; qtv; qtv = qtv->next)
		{
			if (!qtv->DisconnectWhenNooneIsWatching)
			{
				suitable = qtv;
				numfound++;
			}
		}

		if (numfound == 1)
			qtv = suitable;
	}

	if (!qtv)
	{
		Net_ProxyPrintf(pend, "%s"
							  "PERROR: Multiple streams are currently playing\n\n",
							  QTV_SV_HEADER(pend, QTV_VERSION));
		pend->flushing = true;
	}

	return qtv;
}

static void SV_ReadDemoListRequest(cluster_t *cluster, oproxy_t *pend)
{
	// Lists sources that are currently playing.
	int i;

	Cluster_BuildAvailableDemoList(cluster);

	Net_ProxyPrintf(pend, "%s", QTV_SV_HEADER(pend, QTV_VERSION));

	if (!cluster->availdemoscount)
	{
		Net_ProxyPrintf(pend, "PERROR: No demos currently available\n");
	}
	else
	{
		for (i = 0; i < cluster->availdemoscount; i++)
		{
			Net_ProxyPrintf(pend, "ADEMO: %i: %15s\n", cluster->availdemos[i].size, cluster->availdemos[i].name);
		}

		//qtv = NULL;
	}

	Net_ProxyPrintf(pend, "\n");
	pend->flushing = true;
}

static sv_t *SV_ReadSourceRequest(cluster_t *cluster, const char *sourcename)
{
	char *s = NULL;
	sv_t *qtv = NULL;

	// Connects, creating a new source.
	while (*sourcename == ' ')
		sourcename++;
	for (s = (char *)sourcename; *s; s++)
	{
		if (*s < '0' || *s > '9')
			break;
	}

	if (*s)
	{
		qtv = QTV_NewServerConnection(cluster, sourcename, "", false, true, true, false);
	}
	else
	{
		// Numerical source, use a stream id.
		for (qtv = cluster->servers; qtv; qtv = qtv->next)
		{
			if (qtv->streamid == atoi(sourcename))
				break;
		}
	}

	return qtv;
}

static sv_t *SV_ReadDemoRequest(cluster_t *cluster, oproxy_t *pend, const char *demoname)
{
	// Starts a demo off the server... source does the same thing though...
	char buf[256];
	sv_t *qtv = NULL;

	snprintf(buf, sizeof(buf), "demo:%s", demoname);
	qtv = QTV_NewServerConnection(cluster, buf, "", false, true, true, false);
	if (!qtv)
	{
		Net_ProxyPrintf(pend, "%s"
							  "PERROR: couldn't open demo\n\n",
							  QTV_SV_HEADER(pend, QTV_VERSION));
		pend->flushing = true;
	}

	return qtv;
}

static float SV_ReadVersionRequest(const char *version)
{
	float usableversion = atof(version);

	switch ((int)usableversion)
	{
		case 1:
			// Got a usable version.
			break;
		default:
			// Not recognised.
			usableversion = 0;
			break;
	}

	return usableversion;
}

static void SV_CheckProxyTimeout(cluster_t *cluster, oproxy_t *pend)
{
	if (pend->io_time + max(10 * 1000, 1000 * downstream_timeout.integer) <= cluster->curtime)
	{
		Sys_DPrintf(NULL, "SV_ReadPendingProxy: id #%d, pending stream timeout, dropping\n", pend->id);
		if (developer.integer > 1)
			Sys_DPrintf(NULL, "SV_ReadPendingProxy: inbuffer: %s\n", pend->inbuffer);

		pend->drop = true;
	}
}

static void SV_ReadUntilDrop(oproxy_t *pend)
{
	// Peform reading on buffer file if we have empty buffer.
	if (!pend->_buffersize_ && pend->buffer_file)
	{
		pend->_buffersize_ += fread(pend->_buffer_, 1, pend->_buffermaxsize_, pend->buffer_file);

		if (!pend->_buffersize_)
		{
			fclose(pend->buffer_file);
			pend->buffer_file = NULL;
		}
	}

	// Ok we have empty buffer, now we can drop.
	if (!pend->_buffersize_) 
	{
		Sys_DPrintf(NULL, "SV_ReadPendingProxy: id #%d, empty buffer, dropping\n", pend->id);
		if (developer.integer > 1)
			Sys_DPrintf(NULL, "SV_ReadPendingProxy: inbuffer: %s\n", pend->inbuffer);

		pend->drop = true;
	}
}

//
// Receives a request from a stream client and makes sure it's something we recognize (HTTP or QTV).
//
static qbool SV_ReceivePendingProxyRequest(cluster_t *cluster, oproxy_t *pend)
{
	int len;
	char *s = NULL;

	len = sizeof(pend->inbuffer) - pend->inbuffersize - 1;
	len = recv(pend->sock, pend->inbuffer + pend->inbuffersize, len, 0);
	
	// Remote side closed connection.
	if (len == 0)
	{
		Sys_DPrintf(NULL, "SV_ReadPendingProxy: id #%d, remove side closed connection, dropping\n", pend->id);
		if (developer.integer > 1)
			Sys_DPrintf(NULL, "SV_ReadPendingProxy: inbuffer: %s\n", pend->inbuffer);

		pend->drop = true;
		return false;
	}

	// No new data, or read error.
	if (len < 0) 
	{
		return false;
	}

	pend->io_time = cluster->curtime; // Update IO activity.

	pend->inbuffersize += len;
	pend->inbuffer[pend->inbuffersize] = 0; // So strings functions are happy.

	if (pend->inbuffersize < 5)
		return false; 	// Don't have enough yet.

	// Make sure we have a request we understand.
	if (     strncmp(pend->inbuffer, "QTV\r", 4)
		&&   strncmp(pend->inbuffer, "QTV\n", 4)
		&& ( !allow_http.integer 
    			|| (    strncmp(pend->inbuffer, "GET ",  4)
    				 && strncmp(pend->inbuffer, "POST ", 5)
    			   )
		   )
	   )
	{	
		Sys_DPrintf(NULL, "SV_ReadPendingProxy: id #%d, unknown client, dropping\n", pend->id);
		if (developer.integer > 1)
			Sys_DPrintf(NULL, "SV_ReadPendingProxy: inbuffer: %s\n", pend->inbuffer);
	
		// I have no idea what the smeg you are.
		pend->drop = true;
		return false;
	}

	// Make sure there's a double \n somewhere.
	for (s = pend->inbuffer; *s; s++)
	{
		if (s[0] == '\n' && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n')))
			break;
	}

	if (!*s)
		return false; // Don't have enough yet.

	return true;
}

static qbool SV_CheckForHTTPRequest(cluster_t *cluster, oproxy_t *pend)
{
	char *s = NULL;

	if (!strncmp(pend->inbuffer, "POST ", 5))
	{
		HTTPSV_PostMethod(cluster, pend);

		return false;	// Not keen on this..
	}
	else if (!strncmp(pend->inbuffer, "GET ", 4))
	{
		HTTPSV_GetMethod(cluster, pend);

		pend->flushing = true;

		return false;
	}

	return true;
}

static qbool SV_CheckForQTVRequest(cluster_t *cluster, oproxy_t *pend)
{
	qbool raw = false;
	sv_t *qtv = pend->defaultstream;
	char *colon = NULL;
	char userinfo[sizeof(pend->inbuffer)] = {0};
	float usableversion = 0;
	int parse_end;
	char *e = pend->inbuffer;
	char *s = e;

	// Parse a QTV request.
	while (*e)
	{
		if (*e == '\n' || *e == '\r')
		{
			*e = '\0';
			colon = strchr(s, ':');

			if (*s)
			{
				if (!colon)
				{
					Sys_DPrintf(NULL, "qtv cl, got (%s)\n", s);

					if (!strcmp(s, "QTV"))
					{
						// Just a qtv request.
					}
					else if (!strcmp(s, "SOURCELIST"))
					{
						SV_ReadSourceListRequest(cluster, pend);
					}
					else if (!strcmp(s, "REVERSE"))
					{	
						// This is actually a server trying to connect to us
						// start up a new stream.
					}
					else if (!strcmp(s, "RECEIVE"))
					{
						qtv = SV_ReadReceiveRequest(cluster, pend);
					}
					else if (!strcmp(s, "DEMOLIST"))
					{	
						SV_ReadDemoListRequest(cluster, pend);
					}
					else if (!strcmp(s, "AUTH"))
					{
						// Part of the connection process, can be ignored if there's no password.
					}
					else
					{
						Sys_Printf(NULL, "Unrecognised token in QTV connection request (%s)\n", s);
					}
				}
				else
				{
					*colon++ = '\0';
					Sys_DPrintf(NULL, "qtv cl, got (%s) (%s)\n", s, colon);

					if (!strcmp(s, "VERSION"))
					{
						usableversion = SV_ReadVersionRequest(colon);
					}
					else if (!strcmp(s, QTV_EZQUAKE_EXT))
					{
						// We set this ASAP.
						pend->qtv_ezquake_ext = (atoi(colon) & QTV_EZQUAKE_EXT_NUM);
					}
					else if (!strcmp(s, "RAW"))
					{
						raw = atoi(colon);
					}
					else if (!strcmp(s, "SOURCE"))
					{
						qtv = SV_ReadSourceRequest(cluster, colon);
					}
					else if (!strcmp(s, "DEMO"))
					{
						qtv = SV_ReadDemoRequest(cluster, pend, colon);
					}
					else if (!strcmp(s, "AUTH"))
					{	
						// Lists the demos available on this proxy
						// part of the connection process, can be ignored if there's no password.
					}
					else if (!strcmp(s, "USERINFO"))
					{
						strlcpy(userinfo, colon, sizeof(userinfo)); // Can't use it right now, qtv may be NULL atm.
					}
					else
					{
						Sys_Printf(NULL, "Unrecognised token in QTV connection request (%s)\n", s);
					}
				}
			}

			s = e + 1;
		}

		e++;
	}

	//
	// Skip connection part in input buffer but not whole buffer because there may be some command from client already.
	//
	parse_end = e - (char *)pend->inbuffer;
	if (parse_end > 0 && parse_end < sizeof(pend->inbuffer))
	{
		pend->inbuffersize -= parse_end;
		memmove(pend->inbuffer, pend->inbuffer + parse_end, pend->inbuffersize);
	}
	else
	{
		pend->inbuffersize = 0; // Something wrong, just skip all input buffer.
	}

	pend->qtv_clversion = usableversion;

	if (pend->flushing)
		return false;

	if (!usableversion)
	{
		Net_ProxyPrintf(pend, "%s"
							  "PERROR: Requested protocol version not supported\n\n",
							  QTV_SV_HEADER(pend, QTV_VERSION));

		pend->flushing = true;
		return false;
	}

	if (!qtv)
	{
		Net_ProxyPrintf(pend, "%s"
							  "PERROR: No stream selected\n\n",
							  QTV_SV_HEADER(pend, QTV_VERSION));

		pend->flushing = true;
		return false;
	}

	if (cluster->numproxies >= maxclients.integer)
	{
		Net_ProxyPrintf(pend, "%s"
							  "TERROR: This QTV has reached it's connection limit\n\n",
							  QTV_SV_HEADER(pend, QTV_VERSION));
		pend->flushing = true;
		return false;
	}

	pend->defaultstream = qtv;

	pend->next = qtv->proxies;
	qtv->proxies = pend;

	if (!raw)
	{
		Net_ProxyPrintf(pend, "%s"
							  "BEGIN: %s\n\n", QTV_SV_HEADER(pend, QTV_VERSION), qtv->server);
	}

	Info_Convert(&pend->ctx, userinfo);
	Prox_FixName(qtv, pend);

	// Send message to all proxies what we have new client.
	Prox_UpdateProxiesUserList(qtv, pend, QUL_ADD);

	Net_SendConnectionMVD(qtv, pend);

	return true;
}

// Returns true if the pending proxy should be unlinked
// truth does not imply that it should be freed/released, just unlinked.
static qbool SV_ReadPendingProxy(cluster_t *cluster, oproxy_t *pend)
{
	pend->inbuffer[pend->inbuffersize] = 0; // So strings functions are happy.

	// Check if this proxy is timing out.
	SV_CheckProxyTimeout(cluster, pend);

	if (pend->drop)
	{
		// Free memory/handles and indicate we want this stream client unlinked by returning true.
		SV_FreeProxy(pend);
		return true; 
	}

	Net_TryFlushProxyBuffer(cluster, pend);

	if (pend->flushing)
	{
		SV_ReadUntilDrop(pend);

		// NOTE: if flushing is true, we do not peform any reading below, just wait until buffer is empty, then drop.
		return false;
	}

	// Try receiving data from the client and make sure it's of a valid type (HTTP or QTV).
	if (!SV_ReceivePendingProxyRequest(cluster, pend))
	{
		return false;
	}

	// Parse HTTP request.
	if (!SV_CheckForHTTPRequest(cluster, pend))
	{
		return false;
	}

	if (!SV_CheckForQTVRequest(cluster, pend))
	{
		return false;
	}

	return true;
}

void SV_ReadPendingProxies(cluster_t *cluster)
{
	oproxy_t *pend, *pend2, *pend3;

	// unlink (probably) from the head
	while (cluster->pendingproxies)
	{
		pend = cluster->pendingproxies->next;
		if (SV_ReadPendingProxy(cluster, cluster->pendingproxies))
			cluster->pendingproxies = pend;
		else
			break;
	}

	// unlink (probably) from the body/tail
	for (pend = cluster->pendingproxies; pend && pend->next; )
	{
		pend2 = pend->next;
		pend3 = pend2->next;
		if (SV_ReadPendingProxy(cluster, pend2))
		{
			pend->next = pend3;
			pend = pend3;
		}
		else
		{
			pend = pend2;
		}
	}
}

// Just allocate memory and set some fields, do not perform any linkage to any list.
oproxy_t *SV_NewProxy(void *s, qbool socket, sv_t *defaultqtv)
{
	oproxy_t *prox = Sys_malloc(sizeof(*prox));

	// { dynamic buffer allocation
	prox->_buffermaxsize_ = MAX_PROXY_BUFFER;
	prox->_buffer_ = Sys_malloc(prox->_buffermaxsize_);
	// }

	prox->sock = (socket ? *(SOCKET*)s : INVALID_SOCKET);
	prox->file = (socket ?        NULL : (FILE*)s);

	g_cluster.numproxies++;

	prox->ctx.max		= MAX_PROXY_INFOS;

	prox->defaultstream = defaultqtv;
	prox->init_time		= Sys_Milliseconds();
	prox->io_time		= Sys_Milliseconds();

	prox->id			= g_cluster.nextUserId++;

	if(!Info_Set(&prox->ctx, "name", "")) // since infostrings count limited, we reserve at least name
	{
		Sys_Printf(NULL, "can't reserve name for client, dropping\n");
		prox->drop = true;
	}

	if (developer.integer > 1)
		Sys_DPrintf(NULL, "SV_NewProxy: new proxy id #%d\n", prox->id);

	return prox;
}

// Just free memory and handles, do not perfrom removing from any list.
void SV_FreeProxy(oproxy_t *prox)
{
	if (prox->defaultstream)
		Prox_UpdateProxiesUserList(prox->defaultstream, prox, QUL_DEL);

	if (prox->file)
		fclose(prox->file);
	if (prox->sock != INVALID_SOCKET)
		closesocket(prox->sock);

	if (prox->download)
		fclose(prox->download);
	if (prox->buffer_file)
		fclose(prox->buffer_file);

	Info_RemoveAll(&prox->ctx); // free malloced data

	g_cluster.numproxies--;

	Sys_free(prox->_buffer_); // free buffer
	Sys_free(prox);
}

void SV_FindProxies(SOCKET qtv_sock, cluster_t *cluster, sv_t *defaultqtv)
{
	oproxy_t *prox;
	SOCKET sock;
#ifdef SOCKET_CLOSE_TIME
	struct linger	lingeropt;
#endif

	unsigned long nonblocking = true;

	if (qtv_sock == INVALID_SOCKET)
		return;

	if ((sock = accept(qtv_sock, NULL, NULL)) == INVALID_SOCKET)
		return;


#ifdef SOCKET_CLOSE_TIME
	// hard close: in case of closesocket(), socket will be closen after SOCKET_CLOSE_TIME or earlier
	memset(&lingeropt, 0, sizeof(lingeropt));
	lingeropt.l_onoff  = 1;
	lingeropt.l_linger = SOCKET_CLOSE_TIME;

	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (void*)&lingeropt, sizeof(lingeropt) ) == -1)
	{
		Sys_Printf(NULL, "SV_FindProxies: Could not set SO_LINGER socket option\n");
		closesocket(sock);
		return;
	}
#endif

	if (ioctlsocket(sock, FIONBIO, &nonblocking) == INVALID_SOCKET) {
		Sys_Printf(NULL, "SV_FindProxies: ioctl FIONBIO: (%i)\n", qerrno);
		closesocket(sock);
		return;
	}

	if (!TCP_Set_KEEPALIVE(sock))
	{
		Sys_Printf(NULL, "SV_FindProxies: TCP_Set_KEEPALIVE: failed\n");
		closesocket(sock);
		return;
	}

	if (cluster->numproxies >= maxclients.integer)
	{
		// FIXME: WTF is going on here?
		// I think recepient will die after recive this "Proxy is full." because of broken parse process
		const char buffer[] = {dem_all, 1, 'P','r','o','x','y',' ','i','s',' ','f','u','l','l','.'};
		send(sock, buffer, strlen(buffer), 0);
		closesocket(sock);
		return;
	}

	prox = SV_NewProxy((void*) &sock, true, defaultqtv);

	prox->next = cluster->pendingproxies;
	cluster->pendingproxies = prox;
}

void SV_CheckMVDPort(cluster_t *cluster)
{
    static unsigned int last_time_check = 0;

	int newp = bound(0, mvdport.integer, 64000); // FIXME: which actual value is max?

	if (cluster->tcpsocket == INVALID_SOCKET && newp && last_time_check + 30 * 1000 < cluster->curtime)
		mvdport.modified = true; // time to time attempt open port if not open

	if (!mvdport.modified)
		return;

	last_time_check = cluster->curtime;

	if (!newp)
	{
		if (cluster->tcpsocket != INVALID_SOCKET)
		{
			closesocket(cluster->tcpsocket);
			cluster->tcpsocket = INVALID_SOCKET;

			Sys_Printf(NULL, "mvdport is now closed\n");
		}
		else
			Sys_Printf(NULL, "mvdport already closed\n");
	}
	else
	{
		SOCKET news = Net_TCPListenPort(newp);

		if (news != INVALID_SOCKET)
		{
			if (cluster->tcpsocket != INVALID_SOCKET)
				closesocket(cluster->tcpsocket);
			cluster->tcpsocket = news;

			Sys_Printf(NULL, "mvdport %d opened\n", newp);
		}
		else
			Sys_Printf(NULL, "mvdport %d failed to open\n", newp);
	}

	mvdport.modified = false;
}

void Prox_FixName(sv_t *qtv, oproxy_t *prox)
{
	oproxy_t *pcl;
	char	*p;
	int		dupc = 1;
	char	newname[MAX_INFO_KEY], tmp[MAX_INFO_KEY], tmp2[MAX_INFO_KEY];

	// get name
	Info_Get(&prox->ctx, "name", tmp, sizeof(tmp)); // save old/current name for some time
	// copy name
	strlcpy (newname, tmp, sizeof(newname));

	for (p = newname; *p && (*p & 127) == ' '; p++)
		; // empty operator, we search where prefixed spaces ends

	if (p != newname) // skip prefixed spaces, if any, even whole string of spaces
		strlcpy(newname, p, sizeof(newname));

	for (p = newname + strlen(newname) - 1; p >= newname; p--)
	{
		if (*p && (*p & 127) != ' ') // skip spaces in suffix, if any
		{
			p[1] = 0;
			break;
		}
	}

	if (strcmp(tmp, newname))
	{
		Info_Set(&prox->ctx, "name", newname); // set name with skipped spaces or trimmed
		Info_Get(&prox->ctx, "name", newname, sizeof(newname));
	}

	if (!newname[0] || !stricmp(newname, "console") || strchr(newname, '#') || strchr(newname, ':'))
	{
		Info_Set(&prox->ctx, "name", "unnamed"); // console or empty name not allowed, using "unnamed" instead
		Info_Get(&prox->ctx, "name", newname, sizeof(newname));
	}

	// check to see if another user by the same name exists

	for (pcl = qtv->proxies; pcl; pcl = pcl->next)
	{
		if (prox == pcl)
			continue; // ignore self

		if (!stricmp(Info_Get(&pcl->ctx, "name", tmp, sizeof(tmp)), newname))
			break; // onoz, dup name
	}

	if (!pcl)
		return; // not found dup name

	p = newname;

	if (p[0] == '(' && isdigit(p[1]))
	{
		p++;

		while(isdigit(p[0]))
			p++;

		if (p[0] != ')')
			p = newname;
		else
			p++;
	}

	strlcpy(tmp, p, sizeof(tmp)); // this must skip from (1)qqshka prefix (1)

	while (1)
	{
		snprintf(newname, sizeof(newname), "(%d)%-.25s", dupc++, tmp);

		for (pcl = qtv->proxies; pcl; pcl = pcl->next)
		{
			if (prox == pcl)
				continue; // ignore self
    
			if (!stricmp(Info_Get(&pcl->ctx, "name", tmp2, sizeof(tmp2)), newname))
				break; // onoz, dup name
		}

		if (!pcl) // not found dup name, so set new name
		{
			if(!Info_Set(&prox->ctx, "name", newname))
			{
				Sys_Printf(NULL, "can't set dup name for client, dropping\n");
				prox->drop = true;
			}

			return;
		}
	}
}

void Pending_Init(void)
{
	Cvar_Register (&mvdport);
	Cvar_Register (&maxclients);
	Cvar_Register (&allow_http);
}
