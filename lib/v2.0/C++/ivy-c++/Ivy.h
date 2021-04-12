// Ivy.h: interface for the Ivy class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(__IVY_H)
#define __IVY_H

#include <mutex>
#include <thread>

namespace IvyC
{
 #include <Ivy/ivyloop.h>
 #include <Ivy/ivysocket.h>
 #include <Ivy/ivy.h>
 #include <Ivy/ivybuffer.h>
}
#include "IvyCallback.h"

class IvyApplication;

class  Ivy  
{

public:

  // TODO Implement singleton pattern => IVY C cannot cope with two buses

  // TODO Add another mutex on the access to the bus

	Ivy();

	Ivy(const char *name, const char* ready, IvyApplicationCallback *callback,
      bool pthreaded, IvyC::IvyDieCallback _dieCB = nullptr);

	virtual ~Ivy();

	static long  BindMsg (const char *regexp, IvyMessageCallback *cb );
        static long  BindMsg (IvyMessageCallback *cb, const char *regexp, ... )
	  __attribute__((format(printf,2,3)));
	static void UnbindMsg( long id );

  static int  SendMsg(const char *fmt, ... )
	  __attribute__((format(printf,1,2))) ;

  static void SendDirectMsg( IvyApplication *app, int id,
				   const char *message);

  static void BindDirectMsg( IvyDirectMessageCallback *callback );

  static void SetBindCallback(IvyBindingCallback* bind_callback );

  static void SetPongCallback(IvyC::IvyPongCallback pong_callback);

  static void SetFilter( int argc, const char **argv );

  static void start(const char *domain);

  static void stop();

  static void ivyMainLoop ();

  static std::thread* getThread();

  static std::mutex* getMutex();
protected:

  static void ApplicationCb( IvyC::IvyClientPtr app, void *user_data,
				   IvyC::IvyApplicationEvent event ) ;

  static void DieCb( IvyC::IvyClientPtr app, void *user_data, int id ) ;

  static void MsgCb( IvyC::IvyClientPtr app, void *user_data, int argc, char **argv ) ;

  static void MsgDirectCb( IvyC::IvyClientPtr app, void *user_data, int id, char *msg ) ;

  static void BindCallbackCb( IvyC::IvyClientPtr app, void *user_data, int id, const char *regexp,
			            IvyC::IvyBindEvent event) ;
private:
  static std::mutex* ivyMutex;
  static bool threaded;
  static std::thread* ivyThr;
};

#endif // !defined(__IVY_H)

