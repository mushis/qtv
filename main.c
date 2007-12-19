/*
Contains the control routines
*/

#include "qtv.h"
#include <signal.h>


cvar_t developer = {"developer", ""};
cvar_t shownet   = {"shownet", ""};

cvar_t hostname = {"hostname", DEFAULT_HOSTNAME};
cvar_t admin_password = {"admin_password", ""};

int SortFilesByDate(const void *a, const void *b) 
{
	if (((availdemo_t*)a)->time < ((availdemo_t*)b)->time)
		return 1;
	if (((availdemo_t*)a)->time > ((availdemo_t*)b)->time)
		return -1;

	if (((availdemo_t*)a)->smalltime < ((availdemo_t*)b)->smalltime)
		return 1;
	if (((availdemo_t*)a)->smalltime > ((availdemo_t*)b)->smalltime)
		return -1;
	return 0;
}

void Cluster_BuildAvailableDemoList(cluster_t *cluster)
{
	if (cluster->last_demos_update && cluster->last_demos_update + DEMOS_UPDATE_TIME > cluster->curtime)
		return; // do not update demos too fast, this save CPU time

	cluster->last_demos_update = cluster->curtime;

	cluster->availdemoscount = 0;

#ifdef _WIN32
	{
		WIN32_FIND_DATA ffd;
		HANDLE h;
		h = FindFirstFile("*.mvd", &ffd);
		if (h != INVALID_HANDLE_VALUE)
		{
			do
			{
				if (cluster->availdemoscount == sizeof(cluster->availdemos)/sizeof(cluster->availdemos[0]))
					break;
				strlcpy(cluster->availdemos[cluster->availdemoscount].name, ffd.cFileName, sizeof(cluster->availdemos[0].name));
				cluster->availdemos[cluster->availdemoscount].size = ffd.nFileSizeLow;
				cluster->availdemos[cluster->availdemoscount].time = ffd.ftLastWriteTime.dwHighDateTime;
				cluster->availdemos[cluster->availdemoscount].smalltime = ffd.ftLastWriteTime.dwLowDateTime;
				cluster->availdemoscount++;
			} while(FindNextFile(h, &ffd));

			FindClose(h);
		}
	}
#else
// FIXME: yeah
#endif

	qsort(cluster->availdemos, cluster->availdemoscount, sizeof(cluster->availdemos[0]), SortFilesByDate);
}


// sleep and handle keyboard input
void Cluster_Sleep(cluster_t *cluster)
{
	sv_t *sv;
	int m;
	struct timeval timeout;
	fd_set socketset;

	FD_ZERO(&socketset);
	m = 0;

	for (sv = cluster->servers; sv; sv = sv->next)
	{
		if (sv->src.type == SRC_TCP && sv->src.s != INVALID_SOCKET)
		{
			FD_SET(sv->src.s, &socketset);
			if (sv->src.s >= m)
				m = sv->src.s + 1;
		}
	}

	if (cluster->tcpsocket != INVALID_SOCKET)
	{
		FD_SET(cluster->tcpsocket, &socketset);
		if (cluster->tcpsocket >= m)
			m = cluster->tcpsocket + 1;
	}

#ifndef _WIN32
	FD_SET(STDIN, &socketset);
	if (STDIN >= m)
		m = STDIN + 1;
#endif


#ifdef _WIN32

//	if (!m) // FIXME: my'n windows XP eat 50% CPU if here mvdport 0 and no clients, lame work around, any better solution?
		Sleep(1);
#else

	usleep(1000);

#endif		

	if ( 1 )
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;
	}
	else
	{
		timeout.tv_sec = 100/1000; // 0 seconds
		timeout.tv_usec = (100%1000)*1000; // 100 milliseconds timeout
	}

	m = select(m, &socketset, NULL, NULL, &timeout);

	Sys_ReadSTDIN(cluster, socketset);
}

void Cluster_Run(cluster_t *cluster, qbool dowait)
{
	sv_t *sv, *old;

	if (g_cluster.curtime > 2000000000) // ~23 days uptime, better restart program now before time wrapping
	{
		Sys_Printf(NULL, "WARNING: Long uptime detected, quit.\n");
		Sys_Exit(1);
	}

	FixSayFloodProtect();

	if (dowait)
		Cluster_Sleep(cluster); // sleep and serve console input

	cluster->curtime = Sys_Milliseconds();

	Cbuf_Execute ();	// process console commands

	// Cbuf_Execute() may take some time so set current time again, not sure that right way
	cluster->curtime = Sys_Milliseconds();

	for (sv = cluster->servers; sv; )
	{
		old = sv; // save sv_t in old, because QTV_Run(old) may free(old)
		sv = sv->next;
		QTV_Run(cluster, old);
	}

	SV_CheckMVDPort(cluster); // check changes of mvdport variable and do appropriate action

	SV_FindProxies(cluster->tcpsocket, cluster, NULL);	// look for any other proxies wanting to muscle in on the action

	SV_ReadPendingProxies(cluster); // serve pending proxies
}

qbool cluster_initialized;

cluster_t g_cluster; // seems fte qtv tryed do not make it global, see no reason for this

int main(int argc, char **argv)
{
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

#ifdef _WIN32
	{
		WSADATA discard;
		WSAStartup(MAKEWORD(2,0), &discard);
	}
#endif

	memset(&g_cluster, 0, sizeof(g_cluster));

	g_cluster.tcpsocket   = INVALID_SOCKET;
	g_cluster.buildnumber = Sys_Build_Number();
	g_cluster.nextUserId  = 1; // let count users from 1

	Info_Init();	// info strings init
	Cbuf_Init ();	// command buffer init
	Cmd_Init ();	// register basic commands
	Cl_Cmds_Init ();// init client commands
	Cvar_Init ();	// variable system init
	Source_Init (); // add source related commands
	Forward_Init(); // register some vars
	Pending_Init ();// register some vars

	Cvar_Register (&developer);
	Cvar_Register (&shownet);

	Cvar_Register (&hostname);
	Cvar_Register (&admin_password);

	cluster_initialized = true;

	Sys_Printf(&g_cluster, "QTV %s, build %i (build date: %s)\n", PROXY_VERSION, g_cluster.buildnumber, BUILD_DATE);

	// process command line arguments
	Cmd_StuffCmds (argc, argv);
	Cbuf_Execute ();

	while (!g_cluster.wanttoexit)
	{
		Cluster_Run(&g_cluster, true);
	}

	return 0;
}
