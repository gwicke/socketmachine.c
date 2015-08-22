
/*
   static struct addrinfo *
   make_addrinfo(const char *address, u_short port)
   {
   struct addrinfo *aitop = NULL;

   struct addrinfo ai;
   char strport[NI_MAXSERV];
   int ai_result;

   memset(&ai, 0, sizeof(ai));
   ai.ai_family = AF_INET;
   ai.ai_socktype = SOCK_STREAM;
   ai.ai_flags = AI_PASSIVE;  // turn NULL host name into INADDR_ANY
   snprintf(strport, sizeof(strport), "%d", port);
   if ((ai_result = getaddrinfo(address, strport, &ai, &aitop)) != 0) {
   if ( ai_result == EAI_SYSTEM )
   event_warn("getaddrinfo");
   else
   event_warnx("getaddrinfo: %s", gai_strerror(ai_result));
   return (NULL);
   }
   return (aitop);
   }
   */
