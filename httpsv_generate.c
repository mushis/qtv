/*
	httpsv_generate.c
*/

#include "qtv.h"
#include "stdlib.h"

#define CRLF "\r\n"
#define HTMLFILESPATH "qtv"
#define NOTFOUNDLEVELSHOT "levelshots/_notfound.jpg"
#define HTMLPRINT(str) Net_ProxySend(cluster, dest, str "\n", strlen(str "\n"))

qbool IsPlayer(const playerinfo_t *p)
{
	char buffer[64];

	if (!p->userinfo[0])
		return false;

	return !atoi(Info_ValueForKey(p->userinfo, "*spectator", buffer, sizeof(buffer)));
}

void HTTPSV_GenerateNotFoundError(cluster_t *cluster, oproxy_t *dest)
{
	HTTPSV_SendHTTPHeader(cluster, dest, "404", "", false);
}

void HTTPSV_GenerateCSSFile(cluster_t *cluster, oproxy_t *dest)
{
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateCSSFile: dest->buffer_file");
	
	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, "style.css", &s);
	if (!dest->buffer_file) {
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

    HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/css", false);
}


/***** SCOREBOARD *****/

typedef struct  {

	int		frags;
	char	name[MAX_INFO_KEY];

} scoreboard_teaminfo;

typedef struct {

	int		frags;
	char	name[MAX_INFO_KEY];
	char	team[MAX_INFO_KEY];

} scoreboard_playerinfo;

typedef struct {

	int						players_count;
	int						teams_count;

	scoreboard_playerinfo	players[MAX_CLIENTS];
	scoreboard_teaminfo		teams[MAX_CLIENTS];

} scoreboard;

void ScoreBoard_Init(scoreboard *b)
{
	b->players_count = 0;
	b->teams_count   = 0;
}

void ScoreBoard_AddPlayer(scoreboard *b, playerinfo_t* p)
{
	int i;
	qbool teamfound = false;

	if (b->players_count < 0 || b->players_count >= MAX_CLIENTS)
		return;

	// add player data
	b->players[b->players_count].frags = p->frags;
	Info_ValueForKey(p->userinfo, "name", b->players[b->players_count].name, sizeof(b->players[0].name));
	Info_ValueForKey(p->userinfo, "team", b->players[b->players_count].team, sizeof(b->players[0].team));
	
	// add frags to team
	for (i = 0; i < b->teams_count; i++)
	{
		if (!strcmp(b->teams[i].name, b->players[b->players_count].team))
		{
			b->teams[i].frags += p->frags;
			teamfound = true;
			break;
		}
	}

	if (!teamfound && i < MAX_CLIENTS)
	{ // i == b->teams_count, new team found
		strlcpy(b->teams[i].name, b->players[b->players_count].team, sizeof(b->players[0].team));
		b->teams[i].frags = p->frags;
		b->teams_count++;
	}

	b->players_count++;
}

#define cmpresult(x,y) ((x) < (y) ? -1 : ((x) > (y) ? 1 : 0))

static int ScoreBoard_CompareTeams(const void *t1, const void *t2)
{
	int f1 = ((scoreboard_teaminfo *) t1)->frags;
	int f2 = ((scoreboard_teaminfo *) t2)->frags;
	return -cmpresult(f1, f2);
}

static int ScoreBoard_CompareTeamsTN(const void *t1, const void *t2)
{
	char* f1 = ((scoreboard_teaminfo *) t1)->name;
	char* f2 = ((scoreboard_teaminfo *) t2)->name;
	return strcmp(f1,f2);
}

static int ScoreBoard_ComparePlayers(const void *pv1, const void *pv2)
{
	int f1 = ((scoreboard_playerinfo *) pv1)->frags;
	int f2 = ((scoreboard_playerinfo *) pv2)->frags;
	return -cmpresult(f1, f2);
}

void ScoreBoard_FSort(scoreboard *b)
{ // frag-wise sorting
	qsort(b->teams,   b->teams_count,   sizeof(scoreboard_teaminfo),   ScoreBoard_CompareTeams);
	qsort(b->players, b->players_count, sizeof(scoreboard_playerinfo), ScoreBoard_ComparePlayers);
}

void ScoreBoard_TSort(scoreboard *b)
{ // teamname-wise sorting
	qsort(b->teams,   b->teams_count,   sizeof(scoreboard_teaminfo),   ScoreBoard_CompareTeamsTN);
	qsort(b->players, b->players_count, sizeof(scoreboard_playerinfo), ScoreBoard_ComparePlayers);
}

/**********/

// NOTE: team argument passed as NULL when we need ignore it, for example in duel mode
void HTTPSV_GenerateTableForTeam(cluster_t *cluster, oproxy_t *dest, scoreboard *b, char* team)
{
	int i, p;
	char buffer[128];

	HTMLPRINT("<table class='scores' cellspacing='0'><tr><th>Frags</th><th>Players</th></tr>");

	for (i = 0, p = 0; i < b->players_count; i++)
	{
		if (team && strcmp(b->players[i].team, team))
			continue; // not on the same team?

		// row start
		if (p++ % 2)
			HTMLPRINT("<tr class='scodd'>");
		else
			HTMLPRINT("<tr>");
	
		// frags
		snprintf(buffer, sizeof(buffer), "<td class='frags'>%i</td>", b->players[i].frags);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));

		// nick
		HTMLPRINT("<td class='nick'>");
		HTMLprintf(buffer, sizeof(buffer), true, "%s", b->players[i].name);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		HTMLPRINT("</td>");
		// row end
		HTMLPRINT("</tr>");
	}

	HTMLPRINT("</table>");
}

void HTTPSV_GenerateScoreBoard(cluster_t *cluster, oproxy_t *dest, scoreboard *b, qbool teams)
{
	int i, t;
	char buffer[MAX_INFO_KEY];

	if (teams)
	{
		HTMLPRINT("<table class='overallscores'><tr class='teaminfo'>");
		for (i = 0; i < b->teams_count; i++)
		{
			HTMLPRINT("<td><span>Team: </span><span class='teamname'>");
			HTMLprintf(buffer, sizeof(buffer), true, "%s", b->teams[i].name);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		        HTMLPRINT("</span><span class='frags'> ");
		        HTMLprintf(buffer, sizeof(buffer), true, "[%i]", b->teams[i].frags);
		        Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</span></td>");
		}
		HTMLPRINT("</tr><tr>");
	}

	if (teams)
	{
		for (t = 0; t < b->teams_count; t++)
		{
			HTMLPRINT("<td>");
			HTTPSV_GenerateTableForTeam(cluster, dest, b, b->teams[t].name);
			HTMLPRINT("</td>");
		}
	}
	else
	{
		HTTPSV_GenerateTableForTeam(cluster, dest, b, NULL); // NOTE: team argement sent as NULL
	}
	
	if (teams)
	{
		HTMLPRINT("</tr></table>");
	}
}

void HTTPSV_GenerateNowPlaying(cluster_t *cluster, oproxy_t *dest)
{
	int player;
	char *s, *server;
	char buffer[1024];
	sv_t *streams;
	int oddrow = 0;
	qbool sv_empty, teamplay;
	char mapname[MAX_QPATH];
	scoreboard sboard;

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Now Playing");

	if (!strcmp(hostname.string, DEFAULT_HOSTNAME))
		snprintf(buffer, sizeof(buffer), "<h1>QuakeTV: Now Playing</h1>");	//don't show the hostname if its set to the default
	else
		snprintf(buffer, sizeof(buffer), "<h1>QuakeTV: Now Playing on %s</h1>", hostname.string);
	Net_ProxySend(cluster, dest, buffer, strlen(buffer));

	HTMLPRINT("<table id='nowplaying' cellspacing='0'>");
	for (streams = cluster->servers; streams; streams = streams->next)
	{
		// skip "tcp:" prefix if any
		server = (strncmp(streams->server, "tcp:", sizeof("tcp:") - 1) ? streams->server : streams->server + sizeof("tcp:") - 1);
		
		// get the name of the map
		// FIXME: is "maps/notready.bsp" is ok to show when we are not ready?
		strlcpy(mapname, streams->modellist[1].name[0] ? streams->modellist[1].name : "maps/notready.bsp", sizeof(mapname));
		FS_StripPathAndExtension(mapname);

		// is the server empty? (except spectators)
		sv_empty = 1;
		ScoreBoard_Init(&sboard);

		for (player = 0; player < MAX_CLIENTS; player++)
		{
			if (IsPlayer(streams->players + player))
			{
				sv_empty = 0;
				ScoreBoard_AddPlayer(&sboard, streams->players + player);
			}
		}

		teamplay = false;
		if (sboard.players_count > 2)
			teamplay = atoi(Info_ValueForKey(streams->serverinfo, "teamplay", buffer, sizeof(buffer)));

		if (!sv_empty)
		{
			if (teamplay)
				ScoreBoard_TSort(&sboard);
			else
				ScoreBoard_FSort(&sboard);
		}

		// table row
		snprintf(buffer, sizeof(buffer), "<tr class='%s%s'>\n", (oddrow ? "odd" : ""), (sv_empty ? "" : " notempty netop"));
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		oddrow = !oddrow;

		// 1st cell: watch now button
		snprintf(buffer, sizeof(buffer), "<td class='wn'><span class=\"qtvfile\"><a href=\"/watch.qtv?sid=%i\">Watch Now!</a></span></td>", streams->streamid);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));

		// 2nd cell: server adress
		HTMLPRINT("<td class='adr'>");
		HTMLprintf(buffer, sizeof(buffer), true, "%s", server);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		HTMLPRINT("</td>");

		// 3rd cell: map name
		if (streams->qstate < qs_active)
		{
			HTMLPRINT("<td class='mn'>NOT CONNECTED</td>");
		}
		else if (!strcmp(streams->gamedir, "qw"))
		{
			HTMLPRINT("<td class='mn'>");
			Net_ProxySend(cluster, dest, "<span>", sizeof("<span>")-1);
			HTMLprintf(buffer, sizeof(buffer), true, "%s", streams->mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));			
			HTMLPRINT("</span>");
			HTMLprintf(buffer, sizeof(buffer), true, " (%s)", mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</td>");
		}
		else
		{
			HTMLPRINT("<td class='mn'>");
			HTMLPRINT("<span>");
			HTMLprintf(buffer, sizeof(buffer), true, "%s", streams->mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));			
			HTMLPRINT("</span>");
			HTMLprintf(buffer, sizeof(buffer), true, "(%s: ", streams->gamedir);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLprintf(buffer, sizeof(buffer), true, "%s)", mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("</td>");
		}

		HTMLPRINT("</tr>");
		// end of row
		
		// details if server not empty
		if (!sv_empty) 
		{
			HTMLPRINT("<tr class='notempty nebottom'>");
			
			// map preview
			snprintf(buffer, sizeof(buffer), "<td class='mappic'><img src='/levelshots/%s.jpg' width='144' height='108' alt='%s' title='%s' /></td>", mapname, mapname, mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));

			// scores table
			HTMLPRINT("<td class='svstatus' colspan='2'>");			
			HTTPSV_GenerateScoreBoard(cluster, dest, &sboard, teamplay);

			// number of observers
			snprintf(buffer,sizeof(buffer), "<p class='observers'>Observers: <span>%u</span></p>", Clcmd_UsersCount(streams));
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));

			HTMLPRINT("</td></tr>");
		}
	}
	HTMLPRINT("</table>");

	if (!cluster->servers)
	{
		s = "No streams are currently being played<br/>";
		Net_ProxySend(cluster, dest, s, strlen(s));
	}

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateQTVStub(cluster_t *cluster, oproxy_t *dest, char *streamtype, char *streamid)
{
	char *s;
	char hostname[64];
	char buffer[1024];

	char fname[256];
	s = fname;
	while (*streamid > ' ')
	{
		if (s > fname + sizeof(fname)-4)	//4 cos I'm too lazy to work out what the actual number should be
			break;
		if (*streamid == '%')
		{
			*s = 0;
			streamid++;
			if (*streamid <= ' ')
				break;
			else if (*streamid >= 'a' && *streamid <= 'f')
				*s += 10 + *streamid-'a';
			else if (*streamid >= 'A' && *streamid <= 'F')
				*s += 10 + *streamid-'A';
			else if (*streamid >= '0' && *streamid <= '9')
				*s += *streamid-'0';

			*s <<= 4;

			streamid++;
			if (*streamid <= ' ')
				break;
			else if (*streamid >= 'a' && *streamid <= 'f')
				*s += 10 + *streamid-'a';
			else if (*streamid >= 'A' && *streamid <= 'F')
				*s += 10 + *streamid-'A';
			else if (*streamid >= '0' && *streamid <= '9')
				*s += *streamid-'0';

			// Don't let hackers try adding extra commands to it.
   			if (*s == '$' || *s == ';' || *s == '\r' || *s == '\n')
   				continue;

			s++;
		}
		else if (*streamid == '$' || *streamid == ';' || *streamid == '\r' || *streamid == '\n')
   		{
   			// Don't let hackers try adding extra commands to it.
   			streamid++;
   		}
		else
		{
			*s++ = *streamid++;
		}
	}
	*s = 0;
	streamid = fname;

	// Get the hostname from the header.
	if (!HTTPSV_GetHostname(cluster, dest, hostname, sizeof(hostname)))
	{
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/x-quaketvident", false);

	snprintf(buffer, sizeof(buffer), "[QTV]\r\n"
									 "Stream: %s%s@%s\r\n"
									 "", 
									 streamtype, streamid, hostname);

	Net_ProxySend(cluster, dest, buffer, strlen(buffer));
}

void HTTPSV_GenerateAdmin(cluster_t *cluster, oproxy_t *dest, int streamid, char *postbody)
{
	char pwd[64]      = {0};
	char cmd[256]     = {0};
	char result[8192] = {0};
	char *s = "";
	char *o = "";
	int passwordokay = false;

	if (!admin_password.string[0])
	{
		HTTPSV_SendHTTPHeader(cluster, dest, "403", "text/html", true);
		HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Admin Error");

		s = "The admin password is disabled. You may not log in remotely.</body></html>\n";
		Net_ProxySend(cluster, dest, s, strlen(s));
		return;
	}

	pwd[0] = 0;
	cmd[0] = 0;

	if (postbody)
	while (*postbody)
	{
		if (!strncmp(postbody, "pwd=", 4))
		{
			postbody = HTTPSV_ParsePOST(postbody+4, pwd, sizeof(pwd));
		}
		else if (!strncmp(postbody, "cmd=", 4))
		{
			postbody = HTTPSV_ParsePOST(postbody+4, cmd, sizeof(cmd));
		}
		else
		{
			while(*postbody && *postbody != '&')
			{
				postbody++;
			}
			if (*postbody == '&')
				postbody++;
		}
	}

	if (!*pwd)
	{
		if (postbody)
			o = "No Password";
		else
			o = "";
	}
	else if (!strcmp(pwd, admin_password.string))
	{
		passwordokay = true;
		//small hack (as http connections are considered non-connected proxies)
		cluster->numproxies--;
		if (*cmd)
			o = Cmd_RconCommand(cmd, result, sizeof(result));
		else
			o = "";
		cluster->numproxies++;
	}
	else
	{
		o = "Bad Password";
	}

	if (o != result)
	{
		strlcpy(result, o, sizeof(result));
		o = result;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Admin");

	s = "<H1>QuakeTV Admin: ";
	Net_ProxySend(cluster, dest, s, strlen(s));
	s = hostname.string;
	Net_ProxySend(cluster, dest, s, strlen(s));
	s = "</H1>";
	Net_ProxySend(cluster, dest, s, strlen(s));


	s =	"<FORM action=\"admin.html\" method=\"post\" name=f>"
		"<CENTER>"
		"Password <input name=pwd value=\"";

	Net_ProxySend(cluster, dest, s, strlen(s));
	if (passwordokay)
		Net_ProxySend(cluster, dest, pwd, strlen(pwd));

			
	s =	"\">"
		"<BR />"
		"Command <input name=cmd maxsize=255 size=40 value=\"\">"
		"<input type=submit value=\"Submit\" name=btn>"
		"</CENTER>"
		"</FORM>";
	Net_ProxySend(cluster, dest, s, strlen(s));

	if (passwordokay)
		HTMLPRINT("<script>document.forms[0].elements[1].focus();</script>");
	else
		HTMLPRINT("<script>document.forms[0].elements[0].focus();</script>");

	while(*o)
	{
		s = strchr(o, '\n');
		if (s)
			*s = 0;
		HTMLprintf(cmd, sizeof(cmd), true, "%s", o);
		Net_ProxySend(cluster, dest, cmd, strlen(cmd));
		Net_ProxySend(cluster, dest, "<BR />", 6);
		if (!s)
			break;
		o = s+1;
	}

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateDemoListing(cluster_t *cluster, oproxy_t *dest)
{
	int i;
	char link[1024], name[sizeof(cluster->availdemos[0].name) * 5];
	char *s;

	dest->_bufferautoadjustmaxsize_ = 1024 * 1024; // NOTE: this allow 1MB buffer...

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Demos");

	s = "<h1>QuakeTV: Demo Listing</h1>\n\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	s = "<table id='demos' cellspacing='0'><thead><tr>"
		"<th class='stream'>stream</th>"
		"<th class='save'>Download</th>"
		"<th class='name'>Demoname</th>"
		"<th class='size'>Size</th></tr></thead>\n<tbody>\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	Cluster_BuildAvailableDemoList(cluster);
	for (i = 0; i < cluster->availdemoscount; i++)
	{
		HTMLprintf(name, sizeof(name), false, "%s", cluster->availdemos[i].name);
		snprintf(link, sizeof(link), 
			"<tr class='%s'>"
			"<td class='stream'><a href='/watch.qtv?demo=%s'><img src='/stream.png' width='14' height='15' /></a></td>"
			"<td class='save'><a href='/dl/demos/%s'><img src='/save.png' width='16' height='16' /></a></td>"
			"<td class='name'>%s</td><td class='size'>%i kB</td>"
			"</tr>\n",
			((i % 2) ? "even" : "odd"), name, name, name, cluster->availdemos[i].size/1024);

		Net_ProxySend(cluster, dest, link, strlen(link));
	}
	s = "</tbody></table>\n";
	Net_ProxySend(cluster, dest, s, strlen(s));

	snprintf(link, sizeof(link), "<P>Total: %i demos</P>", cluster->availdemoscount);
	Net_ProxySend(cluster, dest, link, strlen(link));

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateImage(cluster_t *cluster, oproxy_t *dest, char *imgfilename)
{
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateImage: dest->buffer_file");
	
	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, imgfilename, &s);
	if (!dest->buffer_file) 
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "image/png", false);
}

//========================================================================
// LEVELSHOTS
//========================================================================

static qbool MediaPathName(char *buf, size_t bufsize, char *name, char *mediadir) 
{
	char pathname[256];

	buf[0] = 0;

	// parse requested file, its string before first space
	COM_ParseToken(name, pathname, sizeof(pathname), " ");

	if (!pathname[0])
		return false;

	snprintf(buf, bufsize, "%s/%s", mediadir, pathname);
		
	return Sys_SafePath(buf);
}

void HTTPSV_GenerateLevelshot(cluster_t *cluster, oproxy_t *dest, char *name)
{
	char pathname[256];
	int s;

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateLevelshot: dest->buffer_file");

	if (!MediaPathName(pathname, sizeof(pathname), name, "levelshots")) {
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}
	
	dest->buffer_file = FS_OpenFile(HTMLFILESPATH, pathname, &s);
	if (!dest->buffer_file) {
		dest->buffer_file = FS_OpenFile(HTMLFILESPATH, NOTFOUNDLEVELSHOT, &s);
		if (!dest->buffer_file) {
			HTTPSV_GenerateNotFoundError(cluster, dest);
			return;
		}
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "image/jpeg", false);
}

//========================================================================
// DEMO DOWNLOAD
//========================================================================

void HTTPSV_GenerateDemoDownload(cluster_t *cluster, oproxy_t *dest, char *name)
{
	char pathname[256];

	if (dest->buffer_file)
		Sys_Error("HTTPSV_GenerateDemoDownload: dest->buffer_file");

	if (   !MediaPathName(pathname, sizeof(pathname), name, demo_dir.string)
		|| stricmp(".mvd", Sys_FileExtension(pathname))  // .mvd demos only
	   )
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}
	
	dest->buffer_file = fopen(pathname, "rb");
	if (!dest->buffer_file) {
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "application/octet-stream", false);
}

//========================================================================
// RSS
//========================================================================

void HTTPSV_GenerateRSS(cluster_t *cluster, oproxy_t *dest, char *str)
{
	char *header_fmt = NULL;
	char *footer_fmt = NULL;
	char *item_fmt = NULL;
	char *server = NULL;
	char *link_fmt = NULL;
	char link[1024];
	char *s = NULL;
	sv_t *streams;
	char playerlist[1024];
	char playername[32];
	unsigned int name_len = 0;
	unsigned int item_len = 0;
	oproxy_t *qtvspec = NULL;
	char hostname[1024];
	char tmp[64];
	int player;

	// Get the hostname from the header.
	if (!HTTPSV_GetHostname(cluster, dest, hostname, sizeof(hostname)))
	{
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "application/rss+xml", false);

	header_fmt = 
		"<?xml version=\"1.0\"?>"
		"<rss version=\"2.0\">"
			"<channel>"
				"<title>QTV RSS</title>"
				"<link></link>" // TODO: Set channel link for RSS.
				"<description>QTV Current active feeds</description>"
				"<language>en-us</language>"
				"<pubDate></pubDate>"; // TODO: Set date when stream started?
				
	item_fmt = 
				"<item>"
					"<type>text/plain</type>"
					"<title>%s</title>"
					"<link>%s</link>"
					"<description>%s</description>"
					"<pubDate>%s</pubDate>"
					"<guid>%s</guid>"
				"</item>";

	footer_fmt =
			"</channel>"
		"</rss>";

	link_fmt = "http://%s/watch.qtv?sid=%i";

	// Estimate the size of the item buffer.
	item_len = strlen(item_fmt) + strlen(link_fmt) + strlen(hostname) + 2048;
	s = Sys_malloc(item_len);

	// Send RSS header.
	Net_ProxySend(cluster, dest, header_fmt, strlen(header_fmt));

	// Send RSS items.
	for (streams = cluster->servers; streams; streams = streams->next)
	{
		// Skip "tcp:" prefix if any.
		server = (strncmp(streams->server, "tcp:", sizeof("tcp:") - 1) ? streams->server : streams->server + sizeof("tcp:") - 1);

		// Set the url to the stream.
		snprintf(link, sizeof(link), link_fmt, hostname, streams->streamid);

		playerlist[0] = 0;

		// Output the playerlist (this will be shown in the description field of the RSS item).
		for (player = 0; player < MAX_CLIENTS; player++)
		{
			if (IsPlayer(&streams->players[player]))
			{
				Info_ValueForKey(streams->players[player].userinfo, "name", tmp, sizeof(tmp));
				HTMLprintf(playername, sizeof(playername), true, "%s", tmp);

				strlcat(playerlist, playername, sizeof(playerlist));
				strlcat(playerlist, "\n", sizeof(playerlist));
			}
		}

		snprintf(s, item_len, item_fmt, server, link, playerlist, "", "");
		Net_ProxySend(cluster, dest, s, strlen(s));
	}

	Sys_free(s);

	// Send RSS footer.
	Net_ProxySend(cluster, dest, footer_fmt, strlen(footer_fmt));
}

//========================================================================
// STATUS
//========================================================================

void HTTPSV_GenerateQTVStatus(cluster_t *cluster, oproxy_t *dest)
{
	int d, h, m;

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Status");

	Net_ProxyPrintf(dest, "%s", "<h1>QuakeTV: Status</h1>");

	Net_ProxyPrintf(dest, "%s", "<PRE>");

	Net_ProxyPrintf(dest, "servers: %i/%i\n", g_cluster.NumServers, maxservers.integer);
	Net_ProxyPrintf(dest, "clients: %i/%i\n", g_cluster.numproxies, maxclients.integer);

	Get_Uptime(g_cluster.curtime / 1000, &d, &h, &m);

	Net_ProxyPrintf(dest, "uptime: %i days %02d:%02d\n", d, h, m);

	Net_ProxyPrintf(dest, "%s", "</PRE>");

	HTTPSV_SendHTMLFooter(cluster, dest);
}
