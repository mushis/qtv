/*
commands which comes from clients
*/

#include "qtv.h"

//==========================================
// download vars

cvar_t	allow_download 			= {"allow_download", 		"0"};
cvar_t	allow_download_skins	= {"allow_download_skins",	"0"};
cvar_t	allow_download_models	= {"allow_download_models",	"0"};
cvar_t	allow_download_sounds	= {"allow_download_sounds",	"0"};
cvar_t	allow_download_maps		= {"allow_download_maps",	"0"};
//cvar_t	allow_download_pakmaps	= {"allow_download_pakmaps", "0"};
cvar_t	allow_download_demos	= {"allow_download_demos",	"0"};
cvar_t	allow_download_other	= {"allow_download_other",	"0"};

cvar_t	demo_dir				= {"demo_dir", 	"demos"};

//==========================================
// say/say_team vars

cvar_t floodprot = {"floodprot", "4 2 2"};


//============================================================================
// search utils

oproxy_t *proxy_by_id(sv_t *qtv, int id)
{
	oproxy_t *prox;

	if (!qtv || id < 1)
		return NULL;

	for (prox = qtv->proxies; prox; prox = prox->next)
		if (prox->id == id)
			return prox;

	return NULL;
}



//============================================================================
// say/say_team
//

int fp_messages = 1, fp_persecond = 1, fp_secondsdead = 10;

void FixSayFloodProtect(void)
{
	if (!floodprot.modified)
		return;

	Cmd_TokenizeString (floodprot.string);

	fp_messages		= bound(1, atoi(Cmd_Argv(0)), MAX_FP_CMDS-1);
	fp_persecond	= bound(1, atoi(Cmd_Argv(1)), 999999);
	fp_secondsdead	= bound(1, atoi(Cmd_Argv(2)), 999999);

	floodprot.modified = false;

	Sys_Printf (NULL, "floodprot: after %d msgs per %d seconds, silence for %d seconds\n",
													 fp_messages, fp_persecond, fp_secondsdead);
}

qbool isSayFlood(sv_t *qtv, oproxy_t *p)
{
	int idx;
	unsigned int say_time;

	idx = bound(0, p->fp_s.last_cmd, MAX_FP_CMDS-1);
	say_time = p->fp_s.cmd_time[idx];

	if ( g_cluster.curtime < p->fp_s.locked )
	{
		Prox_Printf(&g_cluster, p, dem_all, (unsigned)-1, PRINT_CHAT, 
				"You can't talk for %d more seconds\n",	(p->fp_s.locked - g_cluster.curtime) / 1000);

		return true; // flooder
	}

	if ( say_time && ( g_cluster.curtime - say_time < fp_persecond * 1000 ) )
	{
		Prox_Printf(&g_cluster, p, dem_all, (unsigned)-1, PRINT_CHAT, 
				"FloodProt: You can't talk for %d seconds\n", fp_secondsdead);

		p->fp_s.locked = g_cluster.curtime + 1000 * fp_secondsdead;
		p->fp_s.warnings += 1; // collected but unused stat

		return true; // flooder
	}

	p->fp_s.cmd_time[idx] = g_cluster.curtime;

	if ( ++idx >= fp_messages )
		idx = 0;

	p->fp_s.last_cmd = idx;

	return false;
}

void Clcmd_Say_f(sv_t *qtv, oproxy_t *prox)
{
	char buffer[1024 + 100], text[1024];
	netmsg_t msg;

	if (isSayFlood(qtv, prox))
		return; // flooder

//	snprintf(text, sizeof(text), "#%d: %s\n", prox->id, Cmd_Args());
	snprintf(text, sizeof(text), "#%d:%s: %s\n", prox->id, Info_Get(&prox->ctx, "name"), Cmd_Args());

	InitNetMsg(&msg, buffer, sizeof(buffer));

	WriteByte(&msg, 0); // 0 ms
	WriteByte(&msg, dem_all); // msg type
	WriteLong(&msg, strlen(text) + 3); // msg len, txt must be not longer than 1024

	WriteByte 	(&msg, svc_print);
	WriteByte	(&msg, PRINT_CHAT);
	WriteString (&msg, text);

	// FIXME: well, it a good question how to send this to users
	SV_ForwardStream(qtv, msg.data, msg.cursize);
}

//============================================================================
// /users
//

void Clcmd_Users_f(sv_t *qtv, oproxy_t *prox)
{
	int c;
	oproxy_t *tmp;

	Sys_Printf (NULL, "userid name\n"
					  "------ ----\n");

	for (c = 0, tmp = qtv->proxies; tmp; tmp = tmp->next)
	{
		if (tmp->drop)
			continue;

		Sys_Printf(NULL, "%6d %s\n", tmp->id, Info_Get(&tmp->ctx, "name"));
		c++;
	}	

	Sys_Printf(NULL, "%i total users\n", c);
}


//============================================================================
// download
//

/*
==================
Clcmd_CompleteDownoload
==================

This is a sub routine for  Clcmd_NextDownload_f(), called when download complete, we set up some fields for prox.
*/

static void Clcmd_CompleteDownoload(sv_t *qtv, oproxy_t *prox)
{
	if (prox->download)
		fclose (prox->download);

	prox->download 		= NULL;
//	prox->downloadsize	= prox->downloadcount = 0;
	prox->file_percent	= 0; //bliP: file percent


	// if map changed tell the client to reconnect
// FIXME: we need do something like prox->flushing = true; instead of commented out code
/*
	if (prox->spawncount != svs.spawncount)
	{
		char *str = "changing\nreconnect\n";

		ClientReliableWrite_Begin (prox, svc_stufftext, strlen(str)+2);
		ClientReliableWrite_String (prox, str);
	}
*/
}

/*
==================
Clcmd_NextDownload_f
==================
*/

static qbool Sub_Clcmd_NextDownload (sv_t *qtv, oproxy_t *prox)
{
	char	read_buf[FILE_TRANSFER_BUF_SIZE];
	int		r, tmp;

	char 	buffer[FILE_TRANSFER_BUF_SIZE + 100];
	netmsg_t msg;

	if (!prox->download)
		return true; // no thx

	InitNetMsg(&msg, buffer, sizeof(buffer));

	tmp = prox->downloadsize - prox->downloadcount;

	r = FILE_TRANSFER_BUF_SIZE;

	if (r > tmp)
		r = tmp;

	if (prox->buffersize - prox->bufferpos + r > 0.5 * MAX_PROXY_BUFFER)
		return true; // we alredy have too much in buffer, can't add data to buffer

	r = fread (read_buf, 1, r, prox->download);
	prox->downloadcount += r;
	// simple one: if we sent all data, percent is 100, if not all data sent, then percent is not more than 99
	prox->file_percent = ( prox->downloadcount >= prox->downloadsize ? 100 : min(99, 100.0 * prox->downloadcount / max(1, prox->downloadsize)) );

	WriteByte (&msg, svc_download);
	WriteShort(&msg, r);
	WriteByte (&msg, prox->file_percent);
	WriteData (&msg, read_buf, r);

	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_all, (unsigned)-1);

	if (prox->downloadcount == prox->downloadsize)
	{
		Clcmd_CompleteDownoload(qtv, prox);
		return true; // we done all we want
	}

	return false; // mean we can call this function one more time, since seems buffer is not filled enough
}

static void Clcmd_NextDownload_f (sv_t *qtv, oproxy_t *prox)
{
	int i;
//	static unsigned int last = 0;

	if (prox->drop)
		return;

//	Sys_Printf(NULL, "time %u\n", g_cluster.curtime - last);

	for (i = 0; i < 3; i++)
	{
//		Sys_Printf(NULL, "n: %d\n", i);
		if (Sub_Clcmd_NextDownload(qtv, prox))
			break;
	}

//	last = g_cluster.curtime;
}


/*
==================
Clcmd_Download_f
==================
*/

static void Clcmd_Download_f(sv_t *qtv, oproxy_t *prox)
{
	char	*name, n[1024], *p;
	qbool	allow_dl = false;
//	qbool	file_from_pak; // ZOID did file come from pak?

	char 	buffer[6];
	netmsg_t msg;

	if (Cmd_Argc() != 2)
	{
		Sys_Printf(NULL, "download [filename]\n");
		return;
	}

	InitNetMsg(&msg, buffer, sizeof(buffer));

	name = Cmd_Argv(1);

	if (!strncmp(name, "demos/", sizeof("demos/")-1) && demo_dir.string[0])
	{
		snprintf(n, sizeof(n), "%s/%s", demo_dir.string, name + sizeof("demos/")-1);
		name = n;
	}

	Sys_ReplaceChar(name, '\\', '/');

	// MUST be in a subdirectory and not abosolute path
	if (!allow_download.integer || !Sys_SafePath(name) || !strstr(name, "/"))
		goto deny_download;

	if      (!strncmp(name, "skins/", sizeof("skins/")-1))
		allow_dl = allow_download_skins.integer;
	else if (!strncmp(name, "progs/", sizeof("progs/")-1))
		allow_dl = allow_download_models.integer;
	else if (!strncmp(name, "sound/", sizeof("sound/")-1))
		allow_dl = allow_download_sounds.integer;
	else if (!strncmp(name, "maps/",  sizeof("maps/") -1))
		allow_dl = allow_download_maps.integer;
	else if (!strncmp(name, "demos/", sizeof("demos/")-1))
		allow_dl = allow_download_demos.integer;
	else
		allow_dl = allow_download_other.integer;

	if (!allow_dl)
		goto deny_download;

	// close previous download if any
	Clcmd_CompleteDownoload(qtv, prox);

	// lowercase name (needed for casesen file systems)
	for (p = name; *p; p++)
		*p = (char)tolower(*p);

	prox->download = FS_OpenFile(qtv->gamedir, name, &prox->downloadsize);
	prox->downloadcount = 0;

	if (!prox->download)
	{
//		Sys_Printf (NULL, "Couldn't download %s\n", name);
		goto deny_download;
	}

/*
	// special check for maps that came from a pak file
	if (!strncmp(name, "maps/", 5) && file_from_pak && !allow_download_pakmaps.integer)
	{
		// close download if any
		Clcmd_CompleteDownoload(qtv, prox);
		goto deny_download;
	}
*/

	// all checks passed, start downloading
	Clcmd_NextDownload_f (qtv, prox);

//	Sys_Printf (NULL, "Downloading %s\n", name);

	//download info
	Sys_Printf(NULL, "File %s is %.0fKB (%.2fMB)\n", name, (float)prox->downloadsize / 1024, (float)prox->downloadsize / 1024 / 1024);

	return;

deny_download:

	WriteByte (&msg, svc_download);
	WriteShort(&msg, -1);
	WriteByte (&msg, 0);

	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_all, (unsigned)-1);

	return;
}

/*
==================
Clcmd_StopDownload_f
==================
*/
static void Clcmd_StopDownload_f(sv_t *qtv, oproxy_t *prox)
{
	char 	buffer[6];
	netmsg_t msg;

	if (!prox->download)
		return;

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->downloadcount = prox->downloadsize;
	fclose (prox->download);
	prox->download = NULL;
	prox->file_percent = 0; //bliP: file percent

	WriteByte (&msg, svc_download);
	WriteShort(&msg, 0);
	WriteByte (&msg, 100);

	Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_all, (unsigned)-1);

	Sys_Printf(NULL, "Download stopped\n");
}


//============================================================================
// ptrack/follow

static void apply_pov(sv_t *qtv, oproxy_t *p)
{
	oproxy_t *prox;
	char 	buffer[128 + 10], buf[128];
	netmsg_t msg;

	if (!p->pov)
		return; // FIXME

	InitNetMsg(&msg, buffer, sizeof(buffer));

	WriteByte (&msg, svc_stufftext);
	if (p->pov)
		snprintf(buf, sizeof(buf), "track %d\n", p->pov - 1);
	else
		snprintf(buf, sizeof(buf), "track off\n"); // FIXME
	WriteData (&msg, buf, strlen(buf) + 1);

	for (prox = qtv->proxies; prox; prox = prox->next)
	{
		if (prox->flushing)
			continue;

		if (prox->follow_id && prox->follow_id == p->id)
			Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_all, (unsigned)-1);
	}
}

static void Clcmd_Ptrack_f(sv_t *qtv, oproxy_t *prox)
{
	if (Cmd_Argc() != 2)
	{
		prox->pov = 0;
		apply_pov(qtv, prox);
		return;
	}

	prox->pov = atoi(Cmd_Argv(1));

	if (prox->pov < 0 || prox->pov >= MAX_CLIENTS)
	{
		Sys_Printf(NULL, "Invalid client to track: %d\n", prox->pov);
		prox->pov = 0;
		apply_pov(qtv, prox);
		return;
	}

	prox->pov++; // and here we increase it

	apply_pov(qtv, prox);
}

static void Clcmd_Follow_f(sv_t *qtv, oproxy_t *prox)
{
	oproxy_t *tmp;

	if (Cmd_Argc() != 2)
	{
		prox->follow_id = 0;
		Sys_Printf(NULL, "follow: turned off\n");
		return;
	}

	prox->follow_id = atoi(Cmd_Argv(1));

	if (prox->follow_id == prox->id)
	{
		Sys_Printf(NULL, "follow: Is it really wise for Silicon Valley to sic the government on Microsoft?\n");
		prox->follow_id = 0;
		return;
	}

	if (!(tmp = proxy_by_id(qtv, prox->follow_id)))
	{
		Sys_Printf(NULL, "follow: no user with id %d\n", prox->follow_id);
		prox->follow_id = 0;
		return;
	}

	Sys_Printf(NULL, "follow: %d\n", tmp->id);
}

//============================================================================
// new/soundlist/modellist/spawn

/*
static void Clcmd_New_f(sv_t *qtv, oproxy_t *prox)
{
	if (prox->flushing == true)
	{
		Sys_Printf(NULL, "wrong usage of %s\n", Cmd_Argv(0));
		return; // kidding us?
	}

	prox->flushing = true;
}
*/

static void Clcmd_send_list(sv_t *qtv, oproxy_t *prox, int svc)
{
	char buffer[MAX_MSGLEN*8];
	netmsg_t msg;
	int prespawn;

	if (!prox->flushing)
	{
		Sys_Printf(NULL, "wrong usage of %s\n", Cmd_Argv(0));
		return; // kidding us?
	}

	if (qtv->qstate != qs_active || !*qtv->mapname)
	{
		Sys_Printf(NULL, "proxy not ready for %s\n", Cmd_Argv(0));
		prox->flushing = true; // so they get Net_SendConnectionMVD() ASAP later
		return;
	}

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->flushing = false; // so we can catch overflow, set it to appropriate values below

	for (prespawn = 0;prespawn >= 0;)
	{
		prespawn = SendList(qtv, prespawn, svc == svc_soundlist ? qtv->soundlist : qtv->modellist, svc, &msg);
		Prox_SendMessage(&g_cluster, prox, msg.data, msg.cursize, dem_read, (unsigned)-1);
		msg.cursize = 0;
	}

	if (prox->flushing)
	{
		Sys_Printf(NULL, "Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
		prox->flushing = (svc == svc_soundlist ? PS_WAIT_MODELLIST : PS_WAIT_SPAWN);
}

static void Clcmd_Soundlist_f(sv_t *qtv, oproxy_t *prox)
{
	if (prox->flushing != PS_WAIT_SOUNDLIST)
	{
		Sys_Printf(NULL, "wrong usage of %s\n", Cmd_Argv(0));
		return;
	}

	Clcmd_send_list(qtv, prox, svc_soundlist);
}

static void Clcmd_Modellist_f(sv_t *qtv, oproxy_t *prox)
{
	if (prox->flushing != PS_WAIT_MODELLIST)
	{
		Sys_Printf(NULL, "wrong usage of %s\n", Cmd_Argv(0));
		return;
	}

	Clcmd_send_list(qtv, prox, svc_modellist);
}

static void Clcmd_Spawn_f(sv_t *qtv, oproxy_t *prox)
{
	char buffer[MAX_MSGLEN*8];
	netmsg_t msg;
	int prespawn;

	if (prox->flushing != PS_WAIT_SPAWN)
	{
		// ignore warning if different spawn count, since its probably ok
		if (atoi(Cmd_Argv(1)) == qtv->clservercount)
			Sys_Printf(NULL, "wrong usage of %s\n", Cmd_Argv(0));

		return;
	}

	if (qtv->qstate != qs_active || !*qtv->mapname)
	{
		Sys_Printf(NULL, "proxy not ready for %s\n", Cmd_Argv(0));
		prox->flushing = true; // so they get Net_SendConnectionMVD() ASAP later
		return;
	}

	InitNetMsg(&msg, buffer, sizeof(buffer));

	prox->flushing = false; // so we can catch overflow

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

	Net_ProxySend(&g_cluster, prox, qtv->buffer, qtv->forwardpoint);	//send all the info we've not yet processed.

	if (prox->flushing)
	{
		Sys_Printf(NULL, "Connection data is too big, dropping proxy client\n");
		prox->drop = true;	//this is unfortunate...
	}
	else
		Net_TryFlushProxyBuffer(&g_cluster, prox);
}
//============================================================================

//Allow clients to change userinfo
static void Clcmd_SetInfo_f (sv_t *qtv, oproxy_t *prox)
{
	char oldval[MAX_INFO_KEY];

	if (Cmd_Argc() == 1) {
		Sys_Printf(NULL, "User info settings:\n");
		Info_PrintList(&prox->ctx);
		return;
	}

	if (Cmd_Argc() != 3) {
		Sys_Printf(NULL, "Usage: setinfo [ <key> <value> ]\n");
		return;
	}

// we don't have such limitations atm in QTV?
//	if (Cmd_Argv(1)[0] == '*')
//		return;		// don't set priveledged values

	strlcpy(oldval, Info_Get(&prox->ctx, Cmd_Argv(1)), sizeof(oldval)); // save current as old

	Info_Set(&prox->ctx, Cmd_Argv(1), Cmd_Argv(2)); // set new value

	if (!strcmp(Info_Get(&prox->ctx, Cmd_Argv(1)), oldval)) // compare new and old
		return; // key hasn't changed

	// process any changed values
	if (!strcmp(Cmd_Argv(1), "name"))
		Prox_FixName(qtv, prox);
}



//============================================================================

unsigned int Clcmd_UsersCount(const sv_t *qtv)
{
	unsigned int c = 0;
	const oproxy_t *tmp;

	for (tmp = qtv->proxies; tmp; tmp = tmp->next)
	{
		if (tmp->drop)
			continue;
		c++;
	}	
	
	return c;
}

char cl_cmd[MAX_PROXY_INBUFFER]; // global so it does't allocated on stack, this save some CPU I think


typedef struct
{
	char	*name;
	void	(*func) (sv_t *qtv, oproxy_t *prox);
}
ucmd_t;

static ucmd_t ucmds[] =
{
	{"say", 			Clcmd_Say_f},
	{"say_team",		Clcmd_Say_f},

	{"setinfo",			Clcmd_SetInfo_f},

	{"users",			Clcmd_Users_f},

	{"download",		Clcmd_Download_f},
// no, we do it different way, no need to spam it
//	{"nextdl",			Clcmd_NextDownload_f},
	{"stopdl",			Clcmd_StopDownload_f},

	{"ptrack",			Clcmd_Ptrack_f},
	{"follow",			Clcmd_Follow_f},

//	{"qtvnew",			Clcmd_New_f},
	{"qtvsoundlist",	Clcmd_Soundlist_f},
	{"qtvmodellist",	Clcmd_Modellist_f},
	{"qtvspawn",		Clcmd_Spawn_f},

// argh, server send it in mvd demo too, FIXME

	{"new",				NULL},
	{"soundlist",		NULL},
	{"modellist",		NULL},
	{"spawn",			NULL},
	{"prespawn",		NULL},
	{"begin",			NULL},

	{NULL, NULL}
};

void Proxy_ExecuteClCmd(sv_t *qtv, oproxy_t *prox, char *cmd)
{
    char *arg0, result[1024] = {0};
    qbool found = false;
	ucmd_t *u;

//	Sys_Printf(NULL, "cl cmd: %s\n", cmd);

	Cmd_TokenizeString (cmd);

	arg0 = Cmd_Argv(0);

	Sys_RedirectStart(result, sizeof(result));

	for (u = ucmds; u->name; u++)
	{
		if (!strcmp(arg0, u->name))
		{
			if (u->func)
				u->func(qtv, prox);
			found = true;
			break;
		}
	}

	if (!found)
		Sys_Printf(NULL, "Bad user command: %s\n", arg0);

	Sys_RedirectStop();

	if (result[0])
		Prox_Printf(&g_cluster, prox, dem_all, (unsigned)-1, PRINT_HIGH, "%s\n", result);
}

void Proxy_ReadInput(sv_t *qtv, oproxy_t *prox)
{
	int len, parse_end, clc;
	netmsg_t buf;

	if (prox->drop)
		return;

//	if (prox->flushing)
//		return;

	len = sizeof(prox->inbuffer) - prox->inbuffersize - 1; // -1 since it null terminated

	if (len)
	{
		len = recv(prox->sock, prox->inbuffer + prox->inbuffersize, len, 0);

		if (len == 0)
		{
			Sys_Printf(NULL, "read error from QTV client, dropping\n");
			prox->drop = true;
			return;
		}
		else if (len < 0)
		{
//			return;
			len = 0;
		}

		prox->inbuffersize += len;
		prox->inbuffer[prox->inbuffersize] = 0; // null terminated
	}

	if (prox->inbuffersize < 2)
		return; // we need at least size

	buf.data		= prox->inbuffer;
	buf.cursize		= prox->inbuffersize;
	buf.maxsize		= prox->inbuffersize;
	buf.readpos		= 0;
	buf.startpos	= 0;

	parse_end 		= 0;

	while(buf.readpos < buf.cursize)
	{
//		Sys_Printf(NULL, "%d %d\n", buf.readpos, buf.cursize);

		if (buf.readpos > buf.cursize)
		{
			prox->drop = true;
			Sys_Printf(NULL, "Proxy_ReadInput: Read past end of parse buffer\n");
			return;
		}

		buf.startpos = buf.readpos;

		if (buf.cursize - buf.startpos < 2)
			break; // we need at least size

		len = ReadShort(&buf);

		if (len > (int)sizeof(prox->inbuffer) - 1 || len < 3)
		{
			prox->drop = true;
			Sys_Printf(NULL, "Proxy_ReadInput: can't handle such long/short message: %i\n", len);
			return;
		}

		if (len > buf.cursize - buf.startpos)
			break; // not enough data yet

		parse_end = buf.startpos + len; // so later we know which part of buffer we alredy served

		switch (clc = ReadByte(&buf))
		{
		case qtv_clc_stringcmd:
			cl_cmd[0] = 0;
			ReadString(&buf, cl_cmd, sizeof(cl_cmd));
			Proxy_ExecuteClCmd(qtv, prox, cl_cmd);
			break;

		default:
			prox->drop = true;
			Sys_Printf(NULL, "Proxy_ReadInput: can't handle clc %i\n", clc);
			return;
		}
	}

	if (parse_end)
	{
		prox->inbuffersize -= parse_end;
		memmove(prox->inbuffer, prox->inbuffer + parse_end, prox->inbuffersize);
	}
}

void Proxy_ReadProxies(sv_t *qtv)
{
	oproxy_t *prox;

	for (prox = qtv->proxies; prox; prox = prox->next)
	{
		Proxy_ReadInput(qtv, prox);

		if (prox->download)
			Clcmd_NextDownload_f(qtv, prox);
	}
}

//============================================================================

void Cl_Cmds_Init(void)
{
	Cvar_Register(&floodprot);

	Cvar_Register(&allow_download);
	Cvar_Register(&allow_download_skins);
	Cvar_Register(&allow_download_models);
	Cvar_Register(&allow_download_sounds);
	Cvar_Register(&allow_download_maps);
//	Cvar_Register(&allow_download_pakmaps);
	Cvar_Register(&allow_download_demos);
	Cvar_Register(&allow_download_other);

	Cvar_Register(&demo_dir);
}
