/*
Misc net functions/utils
*/

#include "qtv.h"

qbool Net_StringToAddr (char *s, netadr_t *sadr, int defaultport)
{
	struct hostent	*h;
	char	*colon;
	char	copy[128];

	memset (sadr, 0, sizeof(netadr_t));

	((struct sockaddr_in *)sadr)->sin_family = AF_INET;

	((struct sockaddr_in *)sadr)->sin_port = htons(defaultport);

	strlcpy (copy, s, sizeof(copy));
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = htons((short)atoi(colon+1));
		}

	if (copy[0] >= '0' && copy[0] <= '9')	//this is the wrong way to test. a server name may start with a number.
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		if (! (h = gethostbyname(copy)) )
			return 0;
		if (h->h_addrtype != AF_INET)
			return 0;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}

	return true;
}

qbool Net_CompareAddress(netadr_t *s1, netadr_t *s2, int qp1, int qp2)
{
	struct sockaddr_in *i1=(void*)s1, *i2=(void*)s2;
	if (i1->sin_family != i2->sin_family)
		return false;
	if (i1->sin_family == AF_INET)
	{
		if (*(unsigned int*)&i1->sin_addr != *(unsigned int*)&i2->sin_addr)
			return false;
		if (i1->sin_port != i2->sin_port && qp1 != qp2)	//allow qports to match instead of ports, if required.
			return false;
		return true;
	}
	return false;
}

char *Net_BaseAdrToString (const netadr_t *a, char *buf, size_t bufsize)
{
	unsigned char ip[4];

	*(unsigned int*)ip = (((struct sockaddr_in *)a)->sin_addr.s_addr); // FIXME: wonder is this work...

	snprintf(buf, bufsize, "%i.%i.%i.%i", ip[0], ip[1], ip[2], ip[3]);

	return buf;
}

qbool TCP_Set_KEEPALIVE(int sock)
{
	int		iOptVal = 1;

	if (sock == INVALID_SOCKET)
	{
		Sys_Printf("TCP_Set_KEEPALIVE: invalid socket\n");
		return false;
	}

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void*)&iOptVal, sizeof(iOptVal)) == -1) 
	{
		Sys_Printf("TCP_Set_KEEPALIVE: setsockopt SO_KEEPALIVE: (%i): %s\n", qerrno, strerror (qerrno));
		return false;
	}

#if defined(__linux__)

	//	The time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes, 
	//  if the socket option SO_KEEPALIVE has been set on this socket.

	iOptVal = 60;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, (void*)&iOptVal, sizeof(iOptVal)) == -1) 
	{
		Sys_Printf("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPIDLE: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}

	//  The time (in seconds) between individual keepalive probes.
	iOptVal = 30;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, (void*)&iOptVal, sizeof(iOptVal)) == -1) 
	{
		Sys_Printf("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPINTVL: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}

	//  The maximum number of keepalive probes TCP should send before dropping the connection. 
	iOptVal = 6;

	if (setsockopt(sock, SOL_TCP, TCP_KEEPCNT, (void*)&iOptVal, sizeof(iOptVal)) == -1) 
	{
		Sys_Printf("TCP_Set_KEEPALIVE: setsockopt TCP_KEEPCNT: (%i): %s\n", qerrno, strerror(qerrno));
		return false;
	}
#else
	// FIXME: windows, bsd etc...
#endif

	return true;
}

SOCKET Net_TCPListenPort(int port)
{
	SOCKET				sock = INVALID_SOCKET;
	qbool				error = true;
	struct sockaddr_in	address = {0};
	unsigned long		nonblocking = true;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);

	if ((sock = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		// error.
	}
	else if (ioctlsocket (sock, FIONBIO, &nonblocking) == -1)
	{
		// error.
	}
	else if (bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		// error.
	}
	else if (listen(sock, 3) == -1)	// Don't listen for too many clients.
	{
		// error.
	}
	else if (!TCP_Set_KEEPALIVE(sock))
	{
		// error.
	}
	else
	{
		// success.
		error = false;	
	}

	// clean up in casse of error.
	if (error)
	{
		if (sock != INVALID_SOCKET)
		{
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}

	return sock;
}

//=============================================================================

SOCKET Net_UDPOpenSocket(int port)
{
	SOCKET				sock = INVALID_SOCKET;
	qbool				error = true;
	struct sockaddr_in	address = {0};
	unsigned long		nonblocking = true;

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons((u_short)port);

	if ((sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
	{
		// error.
	}
	else if (ioctlsocket (sock, FIONBIO, &nonblocking) == -1)
	{
		// error.
	}
	else if (bind (sock, (void *)&address, sizeof(address)) == -1)
	{
		// error.
	}
	else
	{
		// success.
		error = false;	
	}

	// clean up in casse of error.
	if (error)
	{
		if (sock != INVALID_SOCKET)
		{
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}

	return sock;
}

int Net_GetPacket(cluster_t *cluster, netmsg_t *msg)
{
	int ret;
	socklen_t fromlen = sizeof (cluster->net_from);

	ClearNetMsg(msg);

	if (cluster->udpsocket == INVALID_SOCKET)
		return false;

	ret = recvfrom(cluster->udpsocket, (char *)msg->data, msg->maxsize, 0, (struct sockaddr *) &cluster->net_from, &fromlen);

	if (ret == SOCKET_ERROR)
	{
		if (qerrno == EWOULDBLOCK)
			return false;

		if (qerrno == EMSGSIZE)
		{
			Sys_Printf ("NET_GetPacket: Oversize packet from %s\n", inet_ntoa(cluster->net_from.sin_addr));
			return false;
		}

		if (qerrno == ECONNRESET)
		{
			Sys_DPrintf ("NET_GetPacket: Connection was forcibly closed by %s\n", inet_ntoa(cluster->net_from.sin_addr));
			return false;
		}

		Sys_Printf ("NET_GetPacket: recvfrom: (%i): %s\n", qerrno, strerror (qerrno));
		return false;
	}

	if (ret >= msg->maxsize)
	{
		Sys_Printf ("NET_GetPacket: Oversize packet from %s\n", inet_ntoa(cluster->net_from.sin_addr));
		return false;
	}

	msg->cursize = ret;
	msg->data[ret] = 0;

	return ret;
}

void Net_SendPacket(cluster_t *cluster, int length, const void *data, struct sockaddr_in *to)
{
	socklen_t addrlen = sizeof(*to);

	if (cluster->udpsocket == INVALID_SOCKET)
		return;

	if (sendto(cluster->udpsocket, (const char *) data, length, 0, (struct sockaddr *)to, addrlen) == SOCKET_ERROR)
	{
		if (qerrno == EWOULDBLOCK)
			return;

		if (qerrno == ECONNREFUSED)
			return;

		Sys_Printf ("NET_SendPacket: sendto: (%i): %s\n", qerrno, strerror (qerrno));
	}
}
