#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "config_parser.h"

#include <ctime>
#include <random>

ParserConfigFile config;

/* Функция обратного вызова для события: данные готовы для чтения в buf_ev */
static void read_cb( struct bufferevent *buf_ev, void *arg )
{
	struct bufferevent *partner = (bufferevent*) arg;
	struct evbuffer *src, *dst;
	size_t len;
	src = bufferevent_get_input(buf_ev);
	len = evbuffer_get_length(src);
	if (!partner) {
		evbuffer_drain(src, len);
		return;
	}
	dst = bufferevent_get_output(partner);
	/* Данные просто копируются из буфера ввода в буфер вывода */
	evbuffer_add_buffer(dst, src);
}

static void drain_input_buf( struct bufferevent *buf_ev)
{
	struct evbuffer *src;
	size_t len;
	src = bufferevent_get_input(buf_ev);
	len = evbuffer_get_length(src);
	evbuffer_drain(src, len);	
}

static void echo_event_cb( struct bufferevent *buf_ev, short events, void *arg )
{
	struct bufferevent *partner = (bufferevent*) arg;
	
	if( events & BEV_EVENT_CONNECTED )
		printf( "Соединение установлено\n" );

	if( events & (BEV_EVENT_ERROR|BEV_EVENT_EOF) )
	{
		if( events & BEV_EVENT_EOF )
		{
			printf( "EOF received\n" );
			if (partner) 
			{
				/* Flush all pending data */
				read_cb(buf_ev, arg);				
				bufferevent_free( partner );
			}
			if (buf_ev)
				bufferevent_free( buf_ev );			
		}
		
		if( events & BEV_EVENT_ERROR )
		{	
			perror( "Ошибка объекта bufferevent" );
			if (partner)
				bufferevent_free( partner );
			if (buf_ev)
				bufferevent_free( buf_ev );			
		}
	}
}

static void accept_connection_cb( struct evconnlistener *listener,
		evutil_socket_t fd, struct sockaddr *addr, int sock_len, void *arg )
{
	/* При обработке запроса нового соединения необходимо создать для него
	   объект bufferevent */
	struct event_base *base = evconnlistener_get_base( listener );
	evutil_make_socket_nonblocking(fd);
	struct bufferevent *buf_ev_in = bufferevent_socket_new( base, fd, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS );

	// server part - out connection to server
	//
	struct bufferevent *buf_ev_out = bufferevent_socket_new( base, -1, BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS );

	assert(buf_ev_in && buf_ev_out);

	struct sockaddr_in sin;
	memset( &sin, 0, sizeof(sin) );
	
	sin.sin_family = AF_INET;
	//sin.sin_addr.s_addr = htonl(INADDR_ANY);
	
	std::vector<ParserConfigFile::pair_addr>* servers = static_cast<std::vector<ParserConfigFile::pair_addr>*>(arg);
	//std::cout << "servers arg:" << arg << " servers link:" << servers << std::endl;	
	//auto addr_port_pair = servers->front();
	auto addr_port_pair = servers->data()[std::rand() % servers->size()];
	std::cout << "accepted connection to: " << std::get<0>(addr_port_pair) << ":" << std::get<1>(addr_port_pair) << std::endl;
	
	sin.sin_addr.s_addr = inet_addr( std::get<0>(addr_port_pair).c_str() );
	sin.sin_port = htons( std::get<1>(addr_port_pair) );

	//sin.sin_addr.s_addr = inet_addr( "127.0.0.1" );
	//sin.sin_port = htons( 3100 );

	if( bufferevent_socket_connect( buf_ev_out, (struct sockaddr *)&sin, sizeof(sin) ) < 0 )
	{
		/* Попытка установить соединение была неудачной */
		perror("bufferevent_socket_connect");
		bufferevent_free( buf_ev_in ); /* сокет закроется автоматически; см. флаг при создании */
		bufferevent_free( buf_ev_out );		
		return;
	}

	bufferevent_setcb( buf_ev_in, read_cb, NULL, echo_event_cb, buf_ev_out );
	bufferevent_setcb( buf_ev_out, read_cb, NULL, echo_event_cb, buf_ev_in );
	
	bufferevent_enable( buf_ev_in, (EV_READ | EV_WRITE) );	
	bufferevent_enable( buf_ev_out, (EV_READ | EV_WRITE) );	
	
}

static void accept_error_cb( struct evconnlistener *listener, void *arg )
{
	struct event_base *base = evconnlistener_get_base( listener );
	int error = EVUTIL_SOCKET_ERROR();
	fprintf( stderr, "Ошибка %d (%s) в мониторе соединений. Завершение работы.\n",
			error, evutil_socket_error_to_string( error ) );
	event_base_loopexit( base, NULL );
}


static struct evconnlistener* create_listener(struct event_base *base, int port, std::vector<ParserConfigFile::pair_addr> &servers)
{
	struct evconnlistener *listener;
	struct sockaddr_in sin;
	//int port = 8000;

	if( port < 0 || port > 65535 )
	{
		fprintf( stderr, "Задан некорректный номер порта.\n" );
		return NULL;
	}

	memset( &sin, 0, sizeof(sin) );
	sin.sin_family = AF_INET;    /* работа с доменом IP-адресов */
	sin.sin_addr.s_addr = htonl( INADDR_ANY );  /* принимать запросы с любых адресов */
	sin.sin_port = htons( port );

	listener = evconnlistener_new_bind( base, accept_connection_cb, (void*)&servers,
			(LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE),
			-1, (struct sockaddr *)&sin, sizeof(sin) );
	if( !listener )
	{
		perror( "Ошибка при создании объекта evconnlistener" );
		return NULL;
	}
	evconnlistener_set_error_cb( listener, accept_error_cb );
	return listener;
}

int main( int argc, char **argv )
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "There is no config file. Example: proxy config.txt" << std::endl;
            return 1;
        }

		struct event_base *base;
		base = event_base_new();
		if( !base )
		{
			fprintf( stderr, "Ошибка при создании объекта event_base.\n" );
			return -1;
		}
		
		config.set_config(argv[1]);
		
	    std::srand(std::time(nullptr));
	    std::map<unsigned short, struct evconnlistener*> listeners_map;

	    for (auto port : config.get_ports())
		{
			auto addr_port_pair = config[port].front();
			std::cout << "listen on: " << port << " server: " << std::get<0>(addr_port_pair) << ":" << std::get<1>(addr_port_pair) << std::endl;			
			struct evconnlistener* listener = create_listener(base, port, config[port]);
			if (listener > 0)
				listeners_map[port] = listener;
			else
			{
				fprintf( stderr, "Ошибка при создании объекта evconnlistener.\n" );
				return -1;				
			}
		}		

		std::cout << "starting event_base_dispatch..." << "\n";
		event_base_dispatch( base );
		
	    for (auto port : config.get_ports())
		{
			evconnlistener_free(listeners_map[port]);
			std::cout << "evconnlistener_free port: " << port << "\n";			
		}		
		event_base_free(base);
		
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
	return 0;
}
