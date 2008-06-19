/*
	httpsv.c
*/

#include "qtv.h"
#define CRLF "\r\n"

//main reason to use connection close is because we're lazy and don't want to give sizes in advance (yes, we could use chunks..)

static const char qfont_table[256] = {
	  0, '#', '#', '#', '#', '.', '#', '#',
	'#',   9, 10,  '#', ' ',  13, '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<',
 
	'<', '=', '>', '#', '#', '.', '#', '#',
	'#', '#', ' ', '#', ' ', '>', '.', '.',
	'[', ']', '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', '.', '<', '=', '>',
	' ', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '{', '|', '}', '~', '<'
};

void HTMLprintf(char *outb, int outl, qbool qfont, char *fmt, ...)
{
	va_list val;
	char qfmt[8192*4];
	char *inb = qfmt;

	va_start(val, fmt);
	Q_vsnprintf(qfmt, sizeof(qfmt), fmt, val);
	va_end(val);

	outl--;
	outl -= 5;
	while (outl > 0 && *inb)
	{
		if (*inb == '<')
		{
			*outb++ = '&';
			*outb++ = 'l';
			*outb++ = 't';
			*outb++ = ';';
			outl -= 4;
		}
		else if (*inb == '>')
		{
			*outb++ = '&';
			*outb++ = 'g';
			*outb++ = 't';
			*outb++ = ';';
			outl -= 4;
		}
		else if (*inb == '&')
		{
			*outb++ = '&';
			*outb++ = 'a';
			*outb++ = 'm';
			*outb++ = 'p';
			*outb++ = ';';
			outl -= 5;
		}
		else
		{
			if (qfont)
				*outb++ = qfont_table[*(unsigned char*)inb];
			else
				*outb++ = *inb;

			outl -= 1;
		}
		inb++;
	}
	*outb++ = 0;
}

void HTTPSV_SendHTTPHeader(cluster_t *cluster, oproxy_t *dest, char *error_code, char *content_type, qbool nocache)
{
	char *s;
	char buffer[2048];

	if (nocache)
	{
		s =	"HTTP/1.1 %s OK" CRLF
			"Content-Type: %s" CRLF
			"Cache-Control: no-cache, must-revalidate" CRLF
			"Expires: Mon, 26 Jul 1997 05:00:00 GMT" CRLF
			"Connection: close" CRLF
			CRLF;
	}
	else
	{
		s =	"HTTP/1.1 %s OK" CRLF
			"Content-Type: %s" CRLF
			"Connection: close" CRLF
			CRLF;
	}

	snprintf(buffer, sizeof(buffer), s, error_code, content_type);

	Net_ProxySend(cluster, dest, buffer, strlen(buffer));
}

void HTTPSV_SendHTMLHeader(cluster_t *cluster, oproxy_t *dest, char *title)
{
	char *s;
	char buffer[2048];

	s =	
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN' 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>\n"
		"<html xmlns='http://www.w3.org/1999/xhtml' xml:lang='en' lang='en'>\n"
		"<head>\n"
		"  <meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-1\" />\n"
		"  <title>%s</title>\n"
		"  <link rel=\"StyleSheet\" href=\"/style.css\" type=\"text/css\" />\n"
		"  <link rel=\"alternate\" title=\"RSS\" href=\"/rss\" type=\"application/rss+xml\" />\n"
		"</head>\n"
		"<body><div id=\"navigation\"><div><p><a href=\"/nowplaying/\">Live</a> | <a href=\"/demos/\">Demos</a> | <a href=\"/admin/\">Admin</a></p></div></div>";

	snprintf(buffer, sizeof(buffer), s, title);

	Net_ProxySend(cluster, dest, buffer, strlen(buffer));
}

void HTTPSV_SendHTMLFooter(cluster_t *cluster, oproxy_t *dest)
{
	char *s;
	char buffer[2048];

	snprintf(buffer, sizeof(buffer), "<p id='version'><strong><a href='http://qtv.qw-dev.net'>QTV</a> %s, build %i</strong></p>", PROXY_VERSION, cluster->buildnumber);
	Net_ProxySend(cluster, dest, buffer, strlen(buffer));

	s = "</body>\n"
		"</html>\n";
	Net_ProxySend(cluster, dest, s, strlen(s));
}

qbool HTTPSV_GetHeaderField(const char *s, const char *field, char *buffer, int buffersize)
{
	char *start = NULL;
	char *end = NULL;
	char *copy = NULL;
	char *colon = NULL;
	qbool status = false;
	int fieldnamelen = strlen(field);
	#define EAT_WHITESPACE(str) while (*str && (*str == ' ' || *str == '\t')) str++;

	buffer[0] = 0;

	copy = Sys_strdup(s);
	start = end = copy;

	while (*end)
	{
		if (*end == '\n')
		{
			*end = '\0';
			colon = strchr(start, ':');
			
			if (!colon)
			{
				// Header exists, but has no value. (Not sure if this is proper according to RFC).
				if (!strncmp(field, start, fieldnamelen))
				{
					if (start[fieldnamelen] <= ' ')
					{
						status = true;
						break;
					}
				}
			}
			else
			{
				if (fieldnamelen == colon - start)
				{
					if (!strncmp(field, start, colon - start))
					{
						colon++;
						
						EAT_WHITESPACE(colon);

						while (--buffersize > 0)
						{
							if (*colon == '\r' || *colon == '\n')
								break;
							*buffer++ = *colon++;
						}
						*buffer = 0;
						
						status = true;
						break;
					}
				}
			}
			start = end + 1;
		}

		end++;
	}

	Sys_free(copy);

	return status;
}

char *HTTPSV_ParsePOST(char *post, char *buffer, int buffersize)
{
	while(*post && *post != '&')
	{
		if (--buffersize>0)
		{
			if (*post == '+')
				*buffer++ = ' ';
			else if  (*post == '%')
			{
				*buffer = 0;
				post++;
				if (*post == '\0' || *post == '&')
					break;
				else if (*post >= 'a' && *post <= 'f')
					*buffer += 10 + *post-'a';
				else if (*post >= 'A' && *post <= 'F')
					*buffer += 10 + *post-'A';
				else if (*post >= '0' && *post <= '9')
					*buffer += *post-'0';

				*buffer <<= 4;

				post++;
				if (*post == '\0' || *post == '&')
					break;
				else if (*post >= 'a' && *post <= 'f')
					*buffer += 10 + *post-'a';
				else if (*post >= 'A' && *post <= 'F')
					*buffer += 10 + *post-'A';
				else if (*post >= '0' && *post <= '9')
					*buffer += *post-'0';

				buffer++;
			}
			else
				*buffer++ = *post;
		}
		post++;
	}
	*buffer = 0;

	return post;
}

void HTTPSV_PostMethod(cluster_t *cluster, oproxy_t *pend) //, char *postdata)
{
	char tempbuf[512];
	char *s;
	char *postpath = pend->inbuffer + sizeof("POST ") - 1;
	char *postdata = NULL;
	int len;

	// Get the post data.
	{
		for (s = pend->inbuffer; *s; s++)
		{
			if (s[0] == '\n' && (s[1] == '\n' || (s[1] == '\r' && s[2] == '\n')))
				break;
		}

		if (s[0] == '\n' && s[1] == '\n')
		{
			s += 2;
		}
		else if (s[0] == '\n' && s[1] == '\r' && s[2] == '\n')
		{
			s += 3;
		}

		postdata = s;
	}

	if (!HTTPSV_GetHeaderField(pend->inbuffer, "Content-Length", tempbuf, sizeof(tempbuf)))
	{
		s = "HTTP/1.1 411 OK" CRLF
			"Content-Type: text/html" CRLF
			"Connection: close" CRLF
			CRLF
			"<html><HEAD><TITLE>QuakeTV</TITLE></HEAD><BODY>No Content-Length was provided.</BODY>\n";
		Net_ProxySend(cluster, pend, s, strlen(s));
		pend->flushing = true;
		return;
	}

	len = max(0, atoi(tempbuf));
	if (pend->inbuffersize + len >= sizeof(pend->inbuffer) - 20)
	{	
		// Too much data.
		pend->flushing = true;
		return;
	}

	len = postdata - (char*)pend->inbuffer + len;

	if (len > pend->inbuffersize)
		return;	// Still need the body.

	if (!strncmp(postpath, "/admin", 6))
	{
		HTTPSV_GenerateAdmin(cluster, pend, 0, postdata);
	}
	else
	{
		s = "HTTP/1.1 404 OK" CRLF
			"Content-Type: text/html" CRLF
			"Connection: close" CRLF
			CRLF
			"<html><HEAD><TITLE>QuakeTV</TITLE></HEAD><BODY>That HTTP method is not supported for that URL.</BODY></html>\n";
		Net_ProxySend(cluster, pend, s, strlen(s));
	}

	pend->flushing = true;
	return;
}

void HTTPSV_GetMethod(cluster_t *cluster, oproxy_t *pend)
{
	char *s;
	char *getpath = pend->inbuffer + sizeof("GET ") - 1;

	if (!strncmp(getpath, "/nowplaying", 11))
	{
		HTTPSV_GenerateNowPlaying(cluster, pend);
	}
	else if (!strncmp(getpath, "/watch.qtv?sid=", 15))
	{
		HTTPSV_GenerateQTVStub(cluster, pend, "", pend->inbuffer + 19);
	}
	else if (!strncmp(getpath, "/watch.qtv?demo=", 16))
	{
		HTTPSV_GenerateQTVStub(cluster, pend, "file:", pend->inbuffer + 20);
	}
	else if (!strncmp(getpath, "/about", 6))
	{	
		// Redirect them to our funky website.
		s = "HTTP/1.0 302 Found" CRLF
			"Location: http://qtv.qw-dev.net" CRLF
			CRLF;
		Net_ProxySend(cluster, pend, s, strlen(s));
	}
	else if (!strncmp(getpath, "/admin", 6))
	{
		HTTPSV_GenerateAdmin(cluster, pend, 0, NULL);
	}
	else if (!strncmp(getpath, "/ ", 2))
	{
		s = "HTTP/1.0 302 Found" CRLF
			"Location: /nowplaying/" CRLF
			CRLF;
		Net_ProxySend(cluster, pend, s, strlen(s));
	}
	else if (!strncmp(getpath, "/demos", 6))
	{
		HTTPSV_GenerateDemoListing(cluster, pend);
	}
	else if (!strncmp(getpath, "/style.css", 10))
	{
		HTTPSV_GenerateCSSFile(cluster, pend);
	}
	else if (!strncmp(getpath, "/qtvbg01.png", sizeof("/qtvbg01.png")-1))
	{
		HTTPSV_GenerateImage(cluster, pend, "qtvbg01.png");
	}
	else if (!strncmp(getpath, "/stream.png", sizeof("/stream.png")-1))
	{
		HTTPSV_GenerateImage(cluster, pend, "stream.png");
	}
	else if (!strncmp(getpath, "/save.png", sizeof("/save.png")-1))
	{
		HTTPSV_GenerateImage(cluster, pend, "save.png");
	}
	else if (!strncmp(getpath, "/levelshots/", sizeof("/levelshots/")-1))
	{
		HTTPSV_GenerateLevelshot(cluster, pend, getpath + sizeof("/levelshots/")-1);
	}
	else if (!strncmp(getpath, "/dl/demos/", sizeof("/dl/demos/")-1))
	{
		HTTPSV_GenerateDemoDownload(cluster, pend, getpath + sizeof("/dl/demos/")-1);
	}
	else if (!strncmp(getpath, "/rss", sizeof("/rss")-1))
	{
		HTTPSV_GenerateRSS(cluster, pend, "");
	}
	else if (!strncmp(getpath, "/status", sizeof("/status")-1))
	{
		HTTPSV_GenerateQTVStatus(cluster, pend);
	}
	else
	{
		HTTPSV_SendHTTPHeader(cluster, pend, "404", "text/html", true);
		HTTPSV_SendHTMLHeader(cluster, pend, "Address not recognised");
		s = "<h1>Address not recognised</h1>\n";
		Net_ProxySend(cluster, pend, s, strlen(s));
		HTTPSV_SendHTMLFooter(cluster, pend);
	}
}

//
// Gets the hostname from the header provided by the web client.
//
qbool HTTPSV_GetHostname(cluster_t *cluster, oproxy_t *dest, char *hostname, int buffersize)
{
	char *s = NULL;

	if (!HTTPSV_GetHeaderField(dest->inbuffer, "Host", hostname, buffersize))
	{
		HTTPSV_SendHTTPHeader(cluster, dest, "400", "text/html", true);
		HTTPSV_SendHTMLHeader(cluster, dest, "QuakeTV: Error");

		s = "Your client did not send a Host field, which is required in HTTP/1.1\n<BR />"
			"Please try a different browser.\n"
			"</body>"
			"</html>";

		Net_ProxySend(cluster, dest, s, strlen(s));
		return false;
	}

	return true;
}
