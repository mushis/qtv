/*
source related commadns
*/

#include "qtv.h"

// connect via tcp to qtv source, that may be other qtv or server or whatever
void qtv_f(void)
{
	char addr[MAX_QPATH] = {0};
	sv_t *qtv;

	if (Cmd_Argc() < 2 || !*Cmd_Argv(1)) // not less than one param, first param non empty
	{
		Sys_Printf (NULL, "Usage: %s ip:port [password]\n", Cmd_Argv(0));
		return;
	}

	snprintf(addr, sizeof(addr), "tcp:%s", Cmd_Argv(1));
	qtv = QTV_NewServerConnection(&g_cluster, addr, Cmd_Argv(2), false, false, false, false);

	if (!qtv)
	{
		Sys_Printf (NULL, "Failed to connect to server %s, connection aborted\n", addr);
		return;
	}

	Sys_Printf (NULL, "Source registered %s (#%d)\n", addr, qtv->streamid);
}

// opened .mvd demo as source
// absolute path prohibitet, but playdemo qw/demo.mvd is ok
void playdemo_f(void)
{
	char addr[MAX_QPATH] = {0};

	if (Cmd_Argc() < 2 || !*Cmd_Argv(1))  // not less than one param, first param non empty
	{
		Sys_Printf (NULL, "Usage: %s demoname.mvd\n", Cmd_Argv(0));
		return;
	}

	snprintf(addr, sizeof(addr), "demo:%s", Cmd_Argv(1));

	if (!QTV_NewServerConnection(&g_cluster, addr, Cmd_Argv(2), false, false, false, false)) {
		Sys_Printf (NULL, "Failed to open demo %s\n", addr);
		return;
	}

	Sys_Printf (NULL, "Source registered %s\n", addr);
}

void sourceclose_f(void)
{
	sv_t *qtv;
	unsigned int id, cnt;
	qbool all;

	if (Cmd_Argc() < 2 || !*Cmd_Argv(1))  // not less than one param, first param non empty
	{
		Sys_Printf (NULL, "Usage: %s <#id | all>\n", Cmd_Argv(0));
		return;
	}

	if (!g_cluster.servers) {
		Sys_Printf (NULL, "source list alredy empty\n");
		return;
	}

	id  = (unsigned int) atoi(Cmd_Argv(1));
	all = !stricmp(Cmd_Argv(1), "all");
	cnt = 0;

	for (qtv = g_cluster.servers; qtv; qtv = qtv->next)
	{
		if (all || qtv->streamid == id) 
		{
			Sys_Printf (NULL, "Source id:%d aka %s will be dropped asap\n", qtv->streamid, qtv->server);
			qtv->drop = true;
			cnt++;
		}
	}

	if (!cnt)
		Sys_Printf (NULL, "Source id:%d not found\n", id);
}


void sourcelist_f(void)
{
    char *type, *tmp;
	sv_t *qtv;
	int cnt;

	for (cnt = 0, qtv = g_cluster.servers; qtv; qtv = qtv->next)
	{
		if (!cnt) // print banner
			Sys_Printf (NULL,   "Sources list:\n"
								"%4.4s %4.4s %3.3s %s\n", "#Id", "Type", "Tmp", "Name");

		cnt++;

		switch(qtv->src.type) {
		case SRC_DEMO: type = "demo"; break;
		case SRC_TCP:  type = "tcp";  break;
		case SRC_BAD:  type = "bad";  break;
		default:	   type = "unkn"; break; // unknown
		}

		tmp = qtv->DisconnectWhenNooneIsWatching ? "yes" : "no";

		Sys_Printf (NULL, "%4d %4.4s %3.3s %s\n", qtv->streamid, type, tmp, qtv->server);
	}

	if (!cnt)
		Sys_Printf (NULL, "Sources list: empty\n");
}

void status_f(void)
{
	Sys_Printf(NULL, "Status:\n");
	Sys_Printf(NULL, " servers: %4i/%i\n", g_cluster.NumServers, maxservers.integer);
	Sys_Printf(NULL, " clients: %4i/%i\n", g_cluster.numproxies, get_maxclients());

	Sys_Printf(NULL, "Options:\n");
	Sys_Printf(NULL, "   hostname: %s\n", hostname.string);
	Sys_Printf(NULL, "    mvdport: %i%s\n", mvdport.integer, g_cluster.tcpsocket == INVALID_SOCKET ? " (INVALID)" : "");
	Sys_Printf(NULL, " allow_http: %s\n", allow_http.integer ? "yes" : "no");
}

void quit_f(void)
{
	// if at least some parameter provided, then use non clean exit
	if (Cmd_Argc() > 1)
		Sys_Exit(0); // immidiate, non clean
	else
		g_cluster.wanttoexit = true; // delayed exit, clean
}

void clientlist(sv_t *qtv, qbool showempty)
{
	int c;
	oproxy_t *tmp;
	char name[MAX_INFO_KEY];
	char ip[] = "xxx.xxx.xxx.xxx";

	for (c = 0, tmp = qtv->proxies; tmp; tmp = tmp->next)
	{
		if (tmp->drop)
			continue;

		if (!c)
		{
			Sys_Printf (NULL, "source: %d, %s\n"
							  "userid ip              name\n"
							  "------ --------------- ----\n", qtv->streamid, qtv->server);
		}

		Sys_Printf(NULL, "%6d %15s %s\n", tmp->id, NET_BaseAdrToString(&tmp->addr, ip, sizeof(ip)), Info_Get(&tmp->ctx, "name", name, sizeof(name)));
		c++;
	}	

	if (c)
	{
		Sys_Printf(NULL, "%d total users\n", c);
	}
	else
	{
		if (showempty)
			Sys_Printf (NULL, "source: %d, %s, no clients\n", qtv->streamid, qtv->server);
	}
}

void clientlist_f(void)
{
	int c;
	sv_t *qtv = NULL;
	unsigned int id = (unsigned int) atoi(Cmd_Argv(1));
	qbool showempty = !!id;

	for (c = 0, qtv = g_cluster.servers; qtv; qtv = qtv->next)
	{
		if (id && qtv->streamid != id)
			continue;

		if (!c)
			Sys_Printf(NULL, "Client list:\n");

		clientlist(qtv, showempty);
		c++;
	}

	if (c)
		Sys_Printf(NULL, "%d sources found%s\n", c, showempty ? "" : ", empty sources are not shown");
	else
		Sys_Printf(NULL, "source list empty or no particular stream was found\n", id);
}

void kick_f(void)
{
	sv_t *qtv = NULL;
	int id = atoi(Cmd_Argv(1));
	oproxy_t *tmp;
	char name[MAX_INFO_KEY];
	char ip[] = "xxx.xxx.xxx.xxx";

	for (qtv = g_cluster.servers; qtv; qtv = qtv->next)
	{
		for (tmp = qtv->proxies; tmp; tmp = tmp->next)
		{
			if (tmp->id != id)
				continue;

			tmp->drop = true;
			Sys_Printf(NULL, "kicked: %d %s %s\n", tmp->id, NET_BaseAdrToString(&tmp->addr, ip, sizeof(ip)), Info_Get(&tmp->ctx, "name", name, sizeof(name)));
			return;
		}	
	}

	Sys_Printf(NULL, "client with id %d not found\n", id);
}

void showoutput_f(void)
{
	sv_t *qtv;
	unsigned int id, cnt;
	qbool all;
	qbool show;

	if (Cmd_Argc() < 3 || !*Cmd_Argv(1) || !*Cmd_Argv(2))  // not less than one param, first param non empty
	{
		Sys_Printf (NULL, "Usage: %s <#id | all> <0|1>\n", Cmd_Argv(0));
		return;
	}

	if (!g_cluster.servers) 
	{
		Sys_Printf (NULL, "There are no sources.\n");
		return;
	}

	id  = (unsigned int) atoi(Cmd_Argv(1));
	all = !stricmp(Cmd_Argv(1), "all");
	show = (qbool) atoi(Cmd_Argv(2));
	cnt = 0;

	for (qtv = g_cluster.servers; qtv; qtv = qtv->next)
	{
		if (all || qtv->streamid == id) 
		{
			qtv->EchoInServerConsole = show;
		}
	}
}

void Source_Init(void)
{
	Cvar_Register (&maxservers);
	Cvar_Register (&upstream_timeout);
	Cvar_Register (&parse_delay);
	Cvar_Register (&qtv_reconnect_delay);
	Cvar_Register (&qtv_max_reconnect_delay);
	Cvar_Register (&qtv_backoff);

	Cmd_AddCommand ("qtv", qtv_f);
	Cmd_AddCommand ("playdemo", playdemo_f);
	Cmd_AddCommand ("sourcelist", sourcelist_f);
	Cmd_AddCommand ("sourceclose", sourceclose_f);
	Cmd_AddCommand ("showoutput", showoutput_f);
	Cmd_AddCommand ("status", status_f);
	Cmd_AddCommand ("quit", quit_f);

	Cmd_AddCommand ("clientlist", clientlist_f);
	Cmd_AddCommand ("kick", kick_f);
}
