/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'études de la Navigation Aérienne
 *
 * 	Main loop based on the Qt trolltech Toolkit
 *
 *	Authors: Alexandre Bustico
 *
 *	$Id: ivyqtloop.c 3243 2008-03-21 09:03:34Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifdef WIN32
#include <windows.h>
#endif

#include <cstdlib>
#include <cstdio>

#include "ivyqtloop.h"
#include "Ivy/ivychannel.h"

struct _channel {
  IvyQt *ivyQt;

  int fd;
  void *data;
  ChannelHandleDelete handle_delete;
  ChannelHandleRead handle_read;
  ChannelHandleWrite handle_write;
};

IvyQt::IvyQt (struct _channel *chan, QObject *parent): QObject(parent),
						       channel (chan)
{
  id_read = new QSocketNotifier(channel->fd, QSocketNotifier::Read, this); 
  id_read->setEnabled(false);
  connect(id_read, SIGNAL(activated(int)), SLOT(ivyRead(int)));

  id_delete = new QSocketNotifier(channel->fd, QSocketNotifier::Exception, this);
  id_delete->setEnabled(false);
  connect(id_delete, SIGNAL(activated(int)), SLOT(ivyDelete(int)));

  id_write = new QSocketNotifier(channel->fd, QSocketNotifier::Write, this);
  id_write->setEnabled(false);
  connect(id_write, SIGNAL(activated(int)), SLOT(ivyWrite(int)));
}

void IvyQt::startNotifiersRead()
{
  id_read->setEnabled(true);
  id_delete->setEnabled(true);
}

void IvyQt::startNotifiersWrite()
{
  printf ("DEBUG>  IvyQt::startNotifiersWrite\n");
  id_write->setEnabled(true);
}

void IvyQt::removeNotifiersWrite()
{
  id_write->setEnabled(false);
}

void IvyQt::removeNotifiersRead()
{
  id_read->setEnabled(false);
  id_delete->setEnabled(false);
}


void IvyQt::ivyRead (int fd)
{
  //  TRACE("Handle Channel read %d\n",fd );
  (*channel->handle_read)(channel,fd,channel->data);
}

void IvyQt::ivyWrite (int fd)
{
  //printf ("DEBUG>  IvyQt::ivyWrite\n");
  //  TRACE("Handle Channel write %d\n",fd );
  (*channel->handle_write)(channel,fd,channel->data);
}

void IvyQt::ivyDelete (int fd)
{
  //  TRACE("Handle Channel delete %d\n",*source );
  (*channel->handle_delete)(channel->data);
}



void IvyChannelInit(void)
{
}

void IvyChannelRemove( Channel channel )
{

  if ( channel->handle_delete )
    (*channel->handle_delete)( channel->data );
  channel->ivyQt->removeNotifiersRead();
}



Channel IvyChannelAdd(IVY_HANDLE fd, void *data,
		      ChannelHandleDelete handle_delete,
		      ChannelHandleRead handle_read,
		      ChannelHandleWrite handle_write
		      )						
{
  Channel channel;

  channel = (Channel) malloc (sizeof (struct _channel));
  if ( !channel ) {
    fprintf(stderr,"NOK Memory Alloc Error\n");
    exit(0);
  }

  channel->handle_delete = handle_delete;
  channel->handle_read = handle_read;
  channel->handle_write = handle_write;
  channel->data = data;
  channel->fd = fd;
  channel->ivyQt =  new IvyQt(channel);
  channel->ivyQt->startNotifiersRead();

  return channel;
}




void IvyChannelAddWritableEvent(Channel channel)
{
   channel->ivyQt->startNotifiersWrite ();
}

void IvyChannelClearWritableEvent(Channel channel)
{
   channel->ivyQt->removeNotifiersWrite ();
}

void
IvyChannelStop ()
{
}


