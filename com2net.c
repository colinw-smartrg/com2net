
#include <appf.h>
#include <termios.h>

#define VER_MAJOR 0
#define VER_MINOR 1

int done=0;

af_daemon_t mydaemon;
af_server_t myserver;

af_server_t comserver;

typedef struct _comport {
	int              fd;
	char            *dev;
	int              speed;
	char            *logfile;
	FILE            *logfh;
	int              tcpport;
	af_server_t      comserver;
	af_server_cnx_t *cnx;
	int              telnet_state;
} comport;

int numcoms=0;
comport coms[128];


void c2m_new_cnx( af_server_cnx_t *cnx, void *context );
void c2m_del_cnx( af_server_cnx_t *cnx );
void c2m_cli( char *cmd, af_server_cnx_t *cnx );
void com_new_cnx( af_server_cnx_t *cnx, void *context );
void com_del_cnx( af_server_cnx_t *cnx );
void com_handler( char *cmd, af_server_cnx_t *cnx );

void c2m_signal( int signo )
{
	done = 1;
	return;
}

void c2m_usage( )
{
	printf("mcmonit [opts]\n" );
	printf("opts:\n" );
	printf(" -v    = Version\n" );
	printf(" -f    = run in foreground\n" );
	printf(" -s    = use syslog in foreground\n" );
	printf(" -o    = set log filename\n" );
	printf(" -l    = set log level 0-7\n" );
	printf(" -m    = set log mask (0xfffffff0)\n" );
	printf(" -n    = set application name\n" );
	exit(0);
}

int convert_speed( int sp )
{
	switch (sp)
	{
#ifdef B0
	case 0:
		return B0;
#endif
#ifdef B50
	case 50:
		return B50;
#endif
#ifdef B75
	case 75:
		return B75;
#endif
#ifdef B110
	case 110:
		return B110;
#endif
#ifdef B134
	case 134:
		return B134;
#endif
#ifdef B150
	case 150:
		return B150;
#endif
#ifdef B200
	case 200:
		return B200;
#endif
#ifdef B300
	case 300:
		return B300;
#endif
#ifdef B600
	case 600:
		return B600;
#endif
#ifdef B1200
	case 1200:
		return B1200;
#endif
#ifdef B1800
	case 1800:
		return B1800;
#endif
#ifdef B2400
	case 2400:
		return B2400;
#endif
#ifdef B4800
	case 4800:
		return B4800;
#endif
#ifdef B9600
	case 9600:
		return B9600;
#endif
#ifdef B19200
	case 19200:
		return B19200;
#endif
#ifdef B38400
	case 38400:
		return B38400;
#endif
#ifdef B57600
	case 57600:
		return B57600;
#endif
#ifdef B115200
	case 115200:
		return B115200;
#endif
#ifdef B230400
	case 230400:
		return B230400;
#endif
#ifdef B460800
	case 460800:
		return B460800;
#endif
#ifdef B500000
	case 500000:
		return B500000;
#endif
#ifdef B576000
	case 576000:
		return B576000;
#endif
#ifdef B921600
	case 921600:
		return B921600;
#endif
#ifdef B1000000
	case 1000000:
		return B1000000;
#endif
#ifdef B1152000
	case 1152000:
		return B1152000;
#endif
#ifdef B1500000
	case 1500000:
		return B1500000;
#endif
#ifdef B2000000
	case 2000000:
		return B2000000;
#endif
#ifdef B2500000
	case 2500000:
		return B2500000;
#endif
#ifdef B3000000
	case 3000000:
		return B3000000;
#endif
#ifdef B3500000
	case 3500000:
		return B3500000;
#endif
#ifdef B4000000
	case 4000000:
		return B4000000;
#endif
	default:
		return -1;
	}
}
// Read configfile
// <tcpport>,</dev/ttyXXX>,<speed>,logfile
//
void c2m_read_config( char *conffile )
{
	FILE    *fh;
	char     line[2048], *ptr;
	int      tcpport;
	char    *dev;
	int      speed;
	char    *logfile;
	FILE    *logfh;
	comport *comp;

	fh = fopen( conffile, "r" );
	if ( fh == NULL )
	{
		printf( "Failed to open config file %s\n", conffile );
		exit(-1);
	}

	while ( !feof(fh) )
	{
		ptr = fgets( line, 2047, fh );

		if ( ptr == NULL )
		{
			break;
		}
		if ( line[0] == '#' || strlen( line ) < 3 )
		{
			continue;
		}
		ptr = strtok( line, ", \t\n\r" );
		tcpport = atoi( ptr );
		if ( tcpport < 1024 )
		{
			printf( "Bad TCP port number %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		dev = strdup(ptr);
		if ( strncmp( dev, "/dev", 4 ) != 0 )
		{
			printf( "Bad device %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		speed = atoi( ptr );
		if ( speed <= 0 )
		{
			printf( "Bad speed %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		if ( ptr )
		{
			logfile = strdup(ptr);
			logfh = fopen( logfile, "a+" );
		}
		else
		{
			logfile = NULL;
			logfh = NULL;
		}

		comp = &coms[numcoms];
		comp->tcpport = tcpport;
		comp->dev = dev;
		comp->speed = convert_speed(speed);
		comp->logfile = logfile;
		comp->logfh = logfh;
		comp->fd = -1;
		comp->cnx = NULL;
		numcoms++;
	}
}

int main( int argc, char **argv )
{
	int   ca, i;
	char *conffile = "/etc/com2net.conf";

	mydaemon.appname = argv[0];
	mydaemon.daemonize = 1;
	mydaemon.log_level = LOG_WARNING;
	mydaemon.sig_handler = c2m_signal;
	mydaemon.log_name = argv[0];
	mydaemon.use_syslog = 1;

	/* read command line options */
	while ((ca = getopt(argc, argv, "hvfso:l:m:n:")) != -1)
	{
		switch (ca)
		{
		case 'v':
			printf("%s Version: %d.%d\n", argv[0], VER_MAJOR, VER_MINOR);
			exit(0);
			break;
		case 'f':
			mydaemon.daemonize = 0;
			break;
		case 's':
			mydaemon.use_syslog = 1;
			break;

		case 'o':
			mydaemon.log_filename = optarg;
			break;

		case 'l':
			mydaemon.log_level = atoi(optarg);
			break;

		case 'm':
			if ( (strlen(optarg) > 2) && (optarg[1] == 'x') )
				sscanf(optarg,"%x",&mydaemon.log_mask);
			else
				mydaemon.log_mask = atoi(optarg);
			break;

		case 'n':
			mydaemon.appname = optarg;
			mydaemon.log_name = optarg;
			break;

		case 'h':
		default:
			c2m_usage();
			break;
		}
	}

	c2m_read_config( conffile );
	
	af_daemon_set( &mydaemon );
	af_daemon_start();

	myserver.port = 0x3300;
	myserver.prompt = "com2net>";
	myserver.local = 1;
	myserver.max_cnx = 5;
	myserver.new_connection_callback = c2m_new_cnx;
	myserver.new_connection_context = &myserver;
	myserver.command_handler = c2m_cli;

	af_server_start( &myserver );

	for ( i=0; i<numcoms; i++ )
	{
		coms[i].comserver.port = coms[i].tcpport;
		coms[i].comserver.prompt = "";
		coms[i].comserver.local = 0;
		coms[i].comserver.max_cnx = 2;
		coms[i].comserver.new_connection_callback = com_new_cnx;
		coms[i].comserver.new_connection_context = &coms[i];
		coms[i].comserver.command_handler = com_handler;

		af_server_start( &coms[i].comserver );
	}

	while ( !done )
	{
		af_poll_run( 100 );
	}

	return 0;
}

void c2m_cli( char *cmd, af_server_cnx_t *cnx )
{

	fprintf( cnx->fh, "No commands defined yet\n" );
	fprintf( cnx->fh, " Command NOT FOUND: %s\n", cmd );
	af_server_prompt( cnx );
}

void c2m_del_cnx( af_server_cnx_t *cnx )
{
	// Remove my client structure
	cnx->user_data = NULL;
}
void c2m_new_cnx( af_server_cnx_t *cnx, void *context )
{
	// Create a new client..
	// Set user data.
	cnx->user_data = NULL;
	cnx->disconnect_callback = c2m_del_cnx;
}

/////////////////////////////////////////////////////////////////////
// Com port
//
void com_port_close( comport *comp )
{
	if ( comp->fd >= 0 )
	{
		close( comp->fd );
		comp->fd = -1;
	}
	if ( comp->cnx )
	{
		af_server_disconnect(comp->cnx);
	}
}
void com_port_handler( af_poll_t *ap )
{
	int              len = 0;
	char             buf[2048];
	comport         *comp = (comport *)ap->context;

	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
				af_log_print( LOG_WARNING, "com fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );

				af_poll_rem( ap->fd );
				com_port_close( comp );
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "com to telnet %d -> [%s]", len, buf );
			// Send data to the client
			if ( comp->cnx )
			{
				write( comp->cnx->fd, buf, len );
			}
			// Log it
			if ( comp->logfh )
			{
				fwrite( buf, sizeof(char), len, comp->logfh );
				fflush( comp->logfh );
			}
		}
	}
	else if ( ap->revents )
	{
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "com error, revents: %d", ap->revents );
		af_poll_rem( ap->fd );
		com_port_close( comp );
	}
}


int com_set_port(int fd, int speed, int parity)
{
	struct termios tty;
	memset (&tty, 0, sizeof tty);
	if ( tcgetattr (fd, &tty) != 0 )
	{
		af_log_print( LOG_ERR, "error %d from tcgetattr", errno);
		return -1;
	}

	cfmakeraw( &tty );
	cfsetospeed (&tty, speed);
	cfsetispeed (&tty, speed);

//	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;		// 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	//tty.c_iflag &= ~IGNBRK;			// disable break processing
//	tty.c_iflag = 0;				// no input processing

//	tty.c_lflag = 0;				// no signaling chars, no echo,
									// no canonical processing
//	tty.c_oflag = 0;				// no remapping, no delays
//	tty.c_cc[VMIN]  = 0;			// read doesn't block
//	tty.c_cc[VTIME] = 5;			// 0.5 seconds read timeout

//	tty.c_iflag &= ~(IXON | IXOFF | IXANY);	// shut off xon/xoff ctrl

//	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
									// enable reading
//	tty.c_cflag &= ~(PARENB | PARODD);		// shut off parity
//	tty.c_cflag |= parity;

	tty.c_cc[VDISCARD]= 0;          // no discard
	tty.c_cc[VEOF]    = 0;          // no
	tty.c_cc[VEOL]    = 0;          // no
	tty.c_cc[VEOL2]   = 0;          // no
	tty.c_cc[VERASE]  = 0;          // no
	tty.c_cc[VINTR]   = 0;          // no
	tty.c_cc[VKILL]   = 0;          // no
	tty.c_cc[VLNEXT]  = 0;          // no
	tty.c_cc[VMIN]    = 0;			// read doesn't block
	tty.c_cc[VQUIT]   = 0;          // no
	tty.c_cc[VREPRINT]= 0;          // no
	tty.c_cc[VSTART]  = 0;          // no
	tty.c_cc[VSTOP]   = 0;          // no
	tty.c_cc[VSUSP]   = 0;          // no
	tty.c_cc[VTIME]   = 5;			// 0.5 seconds read timeout
	tty.c_cc[VWERASE] = 0;          // no

	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if ( tcsetattr (fd, TCSANOW, &tty) != 0 )
	{
		af_log_print( LOG_ERR, "error %d from tcsetattr", errno);
		return -1;
	}

	return 0;
}

int open_comport( comport *comp )
{
	comp->fd = open( comp->dev, O_RDWR | O_NOCTTY | O_SYNC );
	if ( comp->fd < 0 )
	{
		af_log_print( LOG_ERR, "Failed to open com port %s, errno %d (%s)", comp->dev, errno, strerror(errno) );
		return -1;
	}

	com_set_port( comp->fd, comp->speed, 0);

	return 0;
}


/////////////////////////////////////////////////////////////////////
// telnet port
//
void com_del_cnx( af_server_cnx_t *cnx )
{
	comport *comp = (comport *)cnx->user_data;
	// Remove my client structure

	if ( comp != NULL )
	{
		comp->cnx = NULL;
	}
	cnx->user_data = NULL;
}

enum {
	TELNET_NONE = 0,
	TELNET_IAC = 1,
	TELNET_OPT = 2,
	TELNET_SUBOPT = 3
};

int com_filter_telnet( comport *comp, unsigned char *buf, int len )
{
	int   i;
	unsigned char  obuf[2048];
	int   olen = 0;

	for ( i=0;i<len; i++ )
	{
		switch ( comp->telnet_state )
		{
		case TELNET_NONE:
			if ( buf[i] == 255 )
			{
				comp->telnet_state = TELNET_IAC;
			}
			else
			{
				obuf[olen++] = buf[i];
			}
			break;
		case TELNET_IAC:
			switch( buf[i] )
			{
			case 255:
				obuf[olen++] = buf[i];
				comp->telnet_state = TELNET_NONE;
				break;
			case 254:
			case 253:
			case 252:
			case 251:
				comp->telnet_state = TELNET_OPT;
				break;
			case 250:
				comp->telnet_state = TELNET_SUBOPT;
				break;
			default:
				comp->telnet_state = TELNET_NONE;
				break;
			}
			break;
		case TELNET_OPT:
			comp->telnet_state = TELNET_NONE;
			break;
		case TELNET_SUBOPT:
			if ( buf[i] == 240 )
			{
				comp->telnet_state = TELNET_NONE;
			}
			break;
		}
	}
	memcpy( buf, obuf, olen );

	return olen;
}

void com_handle_event( af_poll_t *ap )
{
	int              len = 0;
	unsigned char    buf[2048];
	af_server_cnx_t *cnx = (af_server_cnx_t *)ap->context;
	comport *comp = (comport *)cnx->user_data;


	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );

				af_server_disconnect(cnx);
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "telnet to com %d -> [%s]", len, buf );

			// filter telnet data out
			len = com_filter_telnet( comp, buf, len );

			write( comp->fd, buf, len );
		}
	}
	else if ( ap->revents )
	{
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "dcli socket error, revents: %d", ap->revents );
		af_server_disconnect(cnx);
	}
}

void com_new_cnx( af_server_cnx_t *cnx, void *context )
{
	char         buf[128];
	comport     *comp = (comport *)context;

	// If another client is connected, get rid of him.
	if ( comp->cnx != NULL )
	{
		fprintf( comp->cnx->fh, "Someone connected from another server\r\n\n\nBYE!\r\n" );

		af_server_disconnect(comp->cnx);
	}

	// get rid of the stock handler
	af_poll_rem( cnx->fd );

	// Open the comport if it is not open
	if ( comp->fd < 0 )
	{
		if ( open_comport( comp ) < 0 )
		{
			fprintf( cnx->fh, "Failed to open com port %s\r\n\n\nBYE!\r\n", comp->dev );
			af_server_disconnect(cnx);
			return;
		}
	}

	comp->cnx = cnx;

	af_poll_add( comp->fd, POLLIN, com_port_handler, comp );

	af_poll_add( cnx->fd, POLLIN, com_handle_event, cnx );

	// Set user data.
	cnx->user_data = comp;
	cnx->disconnect_callback = com_del_cnx;

	buf[0] = 0xff;
	buf[1] = 0xfb;
	buf[2] = 0x03;
	buf[3] = 0xff;
	buf[4] = 0xfb;
	buf[5] = 0x01;
	buf[6] = 0xff;
	buf[7] = 0xfe;
	buf[8] = 0x01;
	buf[9]  = 0xff;
	buf[10] = 0xfd;
	buf[11] = 0x00;

	write( cnx->fd, buf, 12 );
	
}

void com_handler( char *cmd, af_server_cnx_t *cnx )
{
}

