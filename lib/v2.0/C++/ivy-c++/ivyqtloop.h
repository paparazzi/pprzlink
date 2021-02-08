/*
 *	Ivy, C interface
 *
 *	Copyright (C) 1997-2000
 *	Centre d'études de la Navigation Aérienne
 *
 * 	Main loop based on the QT Toolkit (troltech)
 *
 *	Authors: Alexandre Bustico
 *		 
 *
 *	$Id: ivyqtloop.h 1231 2011-01-11 16:34:15Z bustico $
 * 
 *	Please refer to file version.h for the
 *	copyright notice regarding this software
 */

#ifndef IVYQTLOOP_H
#define IVYQTLOOP_H

using namespace std;

#include <QSocketNotifier>

//class IvyQt;
struct _channel;

class IvyQt : public QObject {
  Q_OBJECT
public:
  explicit IvyQt(struct _channel *chan, QObject *parent=0);

  ~IvyQt() override = default;   //pas besoin de detruire les objets enfants, qt s'en charge

private slots:
  void ivyRead (int);
  void ivyWrite (int);
  void ivyDelete (int);


private:
  struct _channel *channel;
  QSocketNotifier* id_read;
  QSocketNotifier* id_write;
  QSocketNotifier* id_delete;

public:
  void startNotifiersRead ();
  void startNotifiersWrite ();
  void removeNotifiersRead ();
  void removeNotifiersWrite ();
};

#endif //  IVYQTLOOP_H


