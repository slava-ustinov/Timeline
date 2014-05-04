  
 /* ================ Basic Structures used by sockets ========= */ 
 
  struct sockaddr
  {
	  short int sa_family;
	  char sa_data[14];
  };
  
  
  struct sockaddr_in
  {
	  short int sin_family;	/* always AF_INET  */
	  struct in_addr sin_addr;
	  unsigned short int sin_port;
	  char sin_zero[8];
  };
  
  struct in_addr
  {
	  unsigned long int s_addr;
  };
  
  
  
  
  
  /* ======== Extracting host information ================ */
  
  
  struct hostent
  {
	  char 	*h_name; 		/* This is the "official" name of the host */
	  char 	**h_aliases; 	/* These are alternative names for the host */
	  int 	h_addrtype;   	/* usual  AF_INET */
	  int 	h_length;		/* length, in bytes, of each address */
	  char 	**h_addr_list; 	/* This is the vector of addresses for the host */
	  char 	*h_addr;		/* This is a synonym for h_addr_list[0] */
  };
  
  struct hostent * gethostbyname (const char *name)
  
  struct hostent * gethostbyaddr (const char *addr, int length, int format)

  
  
  
  /* ================ Byte order ==================== */
  
  unsigned short int htons (unsigned short int hostshort)
  unsigned short int ntohs (unsigned short int netshort)
  unsigned long  int htonl (unsigned long int hostlong)
  unsigned long  int ntohl (unsigned long int netlong)


   