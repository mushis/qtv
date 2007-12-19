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

	if (*p->userinfo) return !atoi(
		Info_ValueForKey(p->userinfo, "*spectator", buffer, sizeof(buffer)));
	else return false;
}

void HTTPSV_GenerateNotFoundError(cluster_t *cluster, oproxy_t *dest)
{
	HTTPSV_SendHTTPHeader(cluster, dest, "404", "", false);
}

void HTTPSV_GenerateCSSFile(cluster_t *cluster, oproxy_t *dest)
{
	char buffer[MAX_PROXY_BUFFER] = {0};
	int  len = sizeof(buffer);

	if (!FS_ReadFile(HTMLFILESPATH, "style.css", buffer, &len))
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

    HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/css", false);

	// sending a lot of data here!
	Net_ProxySend(cluster, dest, buffer, len);
}


/***** SCOREBOARD *****/
typedef struct  {
	int frags;
	char name[MAX_INFO_KEY];
} scoreboard_teaminfo;

typedef struct {
	int frags;
	char name[MAX_INFO_KEY];
	char team[MAX_INFO_KEY];
} scoreboard_playerinfo;

typedef struct {
	int players_count;
	int teams_count;
	scoreboard_playerinfo players[MAX_CLIENTS];
	scoreboard_teaminfo teams[MAX_CLIENTS];
} scoreboard;

void ScoreBoard_Init(scoreboard *b) { b->players_count = 0; b->teams_count = 0; }
void ScoreBoard_AddPlayer(scoreboard *b, playerinfo_t* p) {
	int i;
	qbool teamfound = false;

	if (b->players_count >= MAX_CLIENTS) return;

	// add player data
	b->players[b->players_count].frags = p->frags;
	Info_ValueForKey(p->userinfo, "name", b->players[b->players_count].name, MAX_INFO_KEY);
	Info_ValueForKey(p->userinfo, "team", b->players[b->players_count].team, MAX_INFO_KEY);
	
	// add frags to team
	for (i = 0; i < b->teams_count; i++) {
		if (!strcmp(b->teams[i].name, b->players[b->players_count].team)) {
			b->teams[i].frags += p->frags;
			teamfound = true;
			break;
		}
	}
	if (!teamfound && i < MAX_CLIENTS) { // i == b->teams_count, new team found
		strlcpy(b->teams[i].name, b->players[b->players_count].team, MAX_INFO_KEY);
		b->teams[i].frags = p->frags;
		b->teams_count++;
	}
	b->players_count++;
}
#define cmpresult(x,y) ((x) < (y) ? -1 : ((x) > (y) ? 1 : 0))
static int ScoreBoard_CompareTeams(const void *t1, const void *t2) {
	int f1 = ((scoreboard_teaminfo *) t1)->frags;
	int f2 = ((scoreboard_teaminfo *) t2)->frags;
	return -cmpresult(f1, f2);
}
static int ScoreBoard_CompareTeamsTN(const void *t1, const void *t2) {
	char* f1 = ((scoreboard_teaminfo *) t1)->name;
	char* f2 = ((scoreboard_teaminfo *) t2)->name;
	return strcmp(f1,f2);
}
static int ScoreBoard_ComparePlayers(const void *pv1, const void *pv2) {
	int f1 = ((scoreboard_playerinfo *) pv1)->frags;
	int f2 = ((scoreboard_playerinfo *) pv2)->frags;
	return -cmpresult(f1, f2);
}
void ScoreBoard_FSort(scoreboard *b) { // frag-wise sorting
	qsort(b->teams, b->teams_count, sizeof(scoreboard_teaminfo), ScoreBoard_CompareTeams);
	qsort(b->players, b->players_count, sizeof(scoreboard_playerinfo), ScoreBoard_ComparePlayers);
}
void ScoreBoard_TSort(scoreboard *b) { // teamname-wise sorting
	qsort(b->teams, b->teams_count, sizeof(scoreboard_teaminfo), ScoreBoard_CompareTeamsTN);
	qsort(b->players, b->players_count, sizeof(scoreboard_playerinfo), ScoreBoard_ComparePlayers);
}
/**********/

void HTTPSV_GenerateTableForTeam(cluster_t *cluster, oproxy_t *dest, scoreboard *b, char* team)
{
	int i, p;
	char buffer[128];
	HTMLPRINT("<table class='scores' cellspacing='0'><tr><th>Frags</th><th>Players</th></tr>");

	for (i = 0, p = 0; i < b->players_count; i++)
	{
		if (*team && strcmp(b->players[i].team, team)) continue;

		// row start
		if (p++ % 2) HTMLPRINT("<tr class='scodd'>"); else HTMLPRINT("<tr>");
	
		// frags
		snprintf(buffer, sizeof(buffer), "<td class='frags'>%i</td>", b->players[i].frags);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));

		// nick
		HTMLPRINT("<td class='nick'>");
		HTMLprintf(buffer, sizeof(buffer), "%s", b->players[i].name);
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

	if (teams) {
		HTMLPRINT("<table class='overallscores'><tr class='teaminfo'>");
		for (i = 0; i < b->teams_count; i++) {
			HTMLPRINT("<td><span>Team: </span><span class='teamname'>");
			HTMLprintf(buffer, sizeof(buffer), "%s", b->teams[i].name);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
      HTMLPRINT("</span><span class='frags'>");
      HTMLprintf(buffer, sizeof(buffer), "%i", b->teams[i].name, b->teams[i].frags);
      Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT("]</span></td>");
		}
		HTMLPRINT("</tr><tr>");
	}

	if (teams) {
		for (t = 0; t < b->teams_count; t++) {
			HTMLPRINT("<td>");
			HTTPSV_GenerateTableForTeam(cluster, dest, b, b->teams[t].name);
			HTMLPRINT("</td>");
		}
	} else
		HTTPSV_GenerateTableForTeam(cluster, dest, b, "");
	
	if (teams) HTMLPRINT("</tr></table>");
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
		// FIXME: replace "maps/dm6.bsp" with something like "maps/unknown.bsp"
		strlcpy(mapname, streams->modellist[1].name[0] ? streams->modellist[1].name : "maps/dm6.bsp", sizeof(mapname));
		FS_StripPathAndExtension(mapname);

		// is the server empty? (except spectators)
		sv_empty = 1;
		ScoreBoard_Init(&sboard);
		for (player = 0; player < MAX_CLIENTS; player++) {
			if (IsPlayer(streams->players + player)) {
				sv_empty = 0;
				ScoreBoard_AddPlayer(&sboard, streams->players + player);
			}
		}
		teamplay = (sboard.players_count > 2) ? atoi(
			Info_ValueForKey(streams->serverinfo, "teamplay", buffer, sizeof(buffer)))
			: false;
		if (!sv_empty) {
			if (teamplay) ScoreBoard_TSort(&sboard); else ScoreBoard_FSort(&sboard);
		}

		// table row
		snprintf(buffer, sizeof(buffer), "<tr class='%s%s'>\n", (oddrow ? "odd" : ""), (sv_empty ? "" : " notempty netop"));
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		oddrow = !oddrow;

		// 1st cell: watch now button
		sprintf(buffer, "<td class='wn'><span class=\"qtvfile\"><a href=\"/watch.qtv?sid=%i\">Watch Now!</a></span></td>", streams->streamid);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));

		// 2nd cell: server adress
		HTMLPRINT("<td class='adr'>");
		HTMLprintf(buffer, sizeof(buffer), "%s", server);
		Net_ProxySend(cluster, dest, buffer, strlen(buffer));
		HTMLPRINT("</td>");

		// 3rd cell: map name
		if (streams->qstate < qs_active)
		{
			HTMLPRINT("</td><td class='mn'>NOT CONNECTED</td>");
		}
		else if (!strcmp(streams->gamedir, "qw"))
		{
			HTMLPRINT("<td class='mn'>");
			HTMLprintf(buffer, sizeof(buffer), "%s", mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			Net_ProxySend(cluster, dest, " &ldquo;<span>", sizeof(" &ldquo;<span>")-1);
			HTMLprintf(buffer, sizeof(buffer), "%s", streams->mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));			
			HTMLPRINT("</span>&rdquo;</td>");
		}
		else
		{
			HTMLPRINT("<td class='mn'>");
			HTMLprintf(buffer, sizeof(buffer), "%s: ", streams->gamedir);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLprintf(buffer, sizeof(buffer), "%s", mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));
			HTMLPRINT(" <span>&ldquo;");
			HTMLprintf(buffer, sizeof(buffer), "%s", streams->mapname);
			Net_ProxySend(cluster, dest, buffer, strlen(buffer));			
			HTMLPRINT("&rdquo;</span></td>");
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

			s++;
		}
		else
			*s++ = *streamid++;
	}
	*s = 0;
	streamid = fname;


	if (!HTTPSV_GetHeaderField(dest->inbuffer, "Host", hostname, sizeof(hostname)))
	{
		HTTPSV_SendHTTPHeader(cluster, dest, "400", "text/html", true);
		HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Error");

		s = "Your client did not send a Host field, which is required in HTTP/1.1\n<BR />"
			"Please try a different browser.\n"
			"</body>"
			"</html>";

		Net_ProxySend(cluster, dest, s, strlen(s));
		return;
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/x-quaketvident", false);

	sprintf(buffer, "[QTV]\r\n"
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
		HTMLprintf(cmd, sizeof(cmd), "%s", o);
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
	char link[256];
	char *s;

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "text/html", true);
	HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Demos");

	s = "<h1>QuakeTV: Demo Listing</h1>";
	Net_ProxySend(cluster, dest, s, strlen(s));

	Cluster_BuildAvailableDemoList(cluster);
	for (i = 0; i < cluster->availdemoscount; i++)
	{
		snprintf(link, sizeof(link), "<A HREF=\"/watch.qtv?demo=%s\">%s</A> (%ikb)<br/>", cluster->availdemos[i].name, cluster->availdemos[i].name, cluster->availdemos[i].size/1024);
		Net_ProxySend(cluster, dest, link, strlen(link));
	}

	sprintf(link, "<P>Total: %i demos</P>", cluster->availdemoscount);
	Net_ProxySend(cluster, dest, link, strlen(link));

	HTTPSV_SendHTMLFooter(cluster, dest);
}

void HTTPSV_GenerateHTMLBackGroundImg(cluster_t *cluster, oproxy_t *dest)
{
	char buffer[MAX_PROXY_BUFFER] = {0};
	int  len = sizeof(buffer);

	if (!FS_ReadFile(HTMLFILESPATH, "qtvbg01.png", buffer, &len))
	{
		HTTPSV_GenerateNotFoundError(cluster, dest);
		return;
	}

	Net_ProxyPrintf(dest,	"HTTP/1.1 200 OK" CRLF
							"Content-Type: image/png" CRLF
							"Content-Length: %i" CRLF
							"Connection: close" CRLF
							CRLF, len);

	// sending a lot of data here!
	Net_ProxySend(cluster, dest, buffer, len);
}

static qbool LevelshotFilenameValid(const char *name)
{
	size_t i, len = strlen(name);
	for (i = 0; i < len; i++) {
		if (!isalnum(name[i]) && !(name[i] == '.')) return false;
	}
	return true;
}

static qbool LevelshotPathName(char *buf, size_t bufsize, const char *name) 
{
	char pathname[MAX_QPATH];
	char *space = strchr(name,' ');
	size_t spacepos;

	if (!space) return false;
	spacepos = space-name+1;
	strlcpy(pathname,name,(spacepos < MAX_QPATH) ? spacepos : MAX_QPATH);
	
	if (!LevelshotFilenameValid(pathname)) return false;

	snprintf(buf,bufsize,"levelshots/%s",pathname);
	
	return true;
}

void HTTPSV_GenerateLevelshot(cluster_t *cluster, oproxy_t *dest, const char *name)
{
	FILE *f;
	char pathname[MAX_QPATH];
	int s;
	char readbuf[512];

	if (!LevelshotPathName(pathname,MAX_QPATH,name)) {
		HTTPSV_GenerateNotFoundError(cluster,dest);
		return;
	}
	
	f = FS_OpenFile(HTMLFILESPATH, pathname, &s);
	if (!f) {
		f = FS_OpenFile(HTMLFILESPATH, NOTFOUNDLEVELSHOT, &s);
		if (!f) {
			HTTPSV_GenerateNotFoundError(cluster,dest);
			return;
		}
	}

	HTTPSV_SendHTTPHeader(cluster, dest, "200", "image/jpeg", false);

	while(!feof(f)) {
		int len = (int) fread(readbuf,1,512,f);
		Net_ProxySend(cluster,dest,readbuf,len);
	}

	fclose(f);
}
