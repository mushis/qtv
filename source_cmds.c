/*
source related commadns
*/

#include "qtv.h"

// connect via tcp to qtv source, that may be other qtv or server or whatever
void qtv_f(void)
{
	char addr[MAX_QPATH] = {0};

	if (Cmd_Argc() < 2 || !*Cmd_Argv(1)) // not less than one param, first param non empty
	{
		Sys_Printf (NULL, "Usage: %s ip:port [password]\n", Cmd_Argv(0));
		return;
	}

	snprintf(addr, sizeof(addr), "tcp:%s", Cmd_Argv(1));

	if (!QTV_NewServerConnection(&g_cluster, addr, Cmd_Argv(2), false, false, false, false)) {
		Sys_Printf (NULL, "Failed to connect to server %s, connection aborted\n", addr);
		return;
	}

	Sys_Printf (NULL, "Source registered %s\n", addr);
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
	int id, cnt;
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

	id  = atoi(Cmd_Argv(1));
	all = !stricmp(Cmd_Argv(1), "all");
	cnt = 0;

	for (qtv = g_cluster.servers; qtv; qtv = qtv->next)
		if (all || qtv->streamid == id) {
			Sys_Printf (NULL, "Source id:%d aka %s will be dropped asap\n", qtv->streamid, qtv->server);
			qtv->drop = true;
			cnt++;
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
	Sys_Printf(NULL, " sources: %4i/%i\n", g_cluster.NumServers, maxsources.integer);
	Sys_Printf(NULL, " proxies: %4i/%i\n", g_cluster.numproxies, maxproxies.integer);

	Sys_Printf(NULL, "Options:\n");
	Sys_Printf(NULL, "   hostname: %s\n", hostname.string);
	Sys_Printf(NULL, "    mvdport: %i%s\n", mvdport.integer, g_cluster.tcpsocket == INVALID_SOCKET ? " (INVALID)" : "");
	Sys_Printf(NULL, " allow_http: %s\n", allow_http.integer ? "yes" : "no");
}

void quit_f(void)
{
	Sys_Exit(0);
}

void Source_Init(void)
{
	Cvar_Register (&maxsources);
	Cvar_Register (&upstream_timeout);

	Cmd_AddCommand ("qtv", qtv_f);
	Cmd_AddCommand ("playdemo", playdemo_f);
	Cmd_AddCommand ("sourcelist", sourcelist_f);
	Cmd_AddCommand ("sourceclose", sourceclose_f);
	Cmd_AddCommand ("status", status_f);
	Cmd_AddCommand ("quit", quit_f);
}
