// IvyCallback.h : Interface for the IvyMessageCallback Class
//               : Interface for the IvyDirectMessageCallback Class
//               : Interface for the IvyApplicationCallback Class
//


// TODO Use std::function to make all this more C++ish

#ifndef __IVY_CALLBACK_H__
#define __IVY_CALLBACK_H__

class IvyApplication;

/* Callback for the normal bus Message */

class IvyMessageCallback  {
public:
	virtual void OnMessage (IvyApplication *app, int argc, const char **argv )=0;
	virtual ~IvyMessageCallback() = default;
};

class IvyMessageCallbackFunction: public IvyMessageCallback  {
public:
	typedef	void ( *IvyMessageCallback_fun )( IvyApplication *app, void *user_data, int argc, const char **argv );
	IvyMessageCallback_fun MessageCb;
	void *data;

public:
	explicit IvyMessageCallbackFunction (  IvyMessageCallback_fun m_cb, void *udata = nullptr ) : MessageCb( m_cb )
	{
		data = udata;
	}
	~IvyMessageCallbackFunction () override = default;
	void OnMessage (IvyApplication *app, int argc, const char **argv) override
	{
	(*MessageCb) (app, data, argc, argv);
	}
};
/* template Class Callback for the normal bus Message */
template <class T> class IvyMessageCallbackOf : public IvyMessageCallback {

protected:
	T*      Object;
	typedef	void ( T::*IvyMessageCallback_fun )( IvyApplication *app, int argc, const char **argv );
	IvyMessageCallback_fun MessageCb;

public:
	IvyMessageCallbackOf ( T* o, IvyMessageCallback_fun m_cb ) : Object (o), MessageCb( m_cb )
	{
	}
	~IvyMessageCallbackOf () override = default;
	void OnMessage (IvyApplication *app, int argc, const char **argv) override
	{
	(Object->*MessageCb) (app, argc, argv);
	}

/* raccourci d'ecriture */
#define BUS_CALLBACK_OF( cl, m ) new IvyMessageCallbackOf<cl>( this, m )
};
/* Callback for the direct Message */
class IvyDirectMessageCallback {
public:
  virtual void OnDirectMessage (IvyApplication *app, int id, const char *arg ) = 0;
  virtual ~IvyDirectMessageCallback () = default;;
};

/* Application Callback */

class IvyApplicationCallback {
public:
	virtual void OnApplicationConnected (IvyApplication *app) = 0;
	virtual void OnApplicationDisconnected (IvyApplication *app) = 0;
	virtual void OnApplicationCongestion (IvyApplication *app) = 0;
	virtual void OnApplicationDecongestion (IvyApplication *app) = 0;
	virtual void OnApplicationFifoFull (IvyApplication *app) = 0;
	virtual ~IvyApplicationCallback() = default;
};

class IvyApplicationNullCallback : public IvyApplicationCallback {
public:
	void OnApplicationConnected (IvyApplication *app) override
	{(void)app;};
	void OnApplicationDisconnected (IvyApplication *app) override
	{(void)app;};
	void OnApplicationCongestion (IvyApplication *app) override
	{(void)app;};
	void OnApplicationDecongestion (IvyApplication *app) override
	{(void)app;};
	void OnApplicationFifoFull (IvyApplication *app) override
	{(void)app;};
	~IvyApplicationNullCallback() override = default;
};

// Static function CB
class IvyApplicationCallbackFunction: public IvyApplicationCallback  {
public:
	typedef	void ( *IvyApplicationCallback_fun )( IvyApplication *app );
	IvyApplicationCallback_fun ApplicationConnectedCb;
	IvyApplicationCallback_fun ApplicationDisconnectedCb;
        IvyApplicationCallback_fun ApplicationCongestionCb;
        IvyApplicationCallback_fun ApplicationDecongestionCb;
        IvyApplicationCallback_fun ApplicationFifoFullCb;

public:
	IvyApplicationCallbackFunction ( IvyApplicationCallback_fun con_cb,  IvyApplicationCallback_fun disc_cb) 
		: ApplicationConnectedCb( con_cb ), ApplicationDisconnectedCb( disc_cb )
	{
	}
	~IvyApplicationCallbackFunction () override = default;
	void OnApplicationConnected (IvyApplication *app) override
	{
	(*ApplicationConnectedCb) (app);
	};
	void OnApplicationDisconnected (IvyApplication *app) override
	{
	(*ApplicationDisconnectedCb) (app);
	};
	void OnApplicationCongestion (IvyApplication *app) override
	{
	  (*ApplicationCongestionCb) (app);
	};
	void OnApplicationDecongestion (IvyApplication *app) override
	{
	  (*ApplicationDecongestionCb) (app);
	};
	void OnApplicationFifoFull (IvyApplication *app) override
	{
	  (*ApplicationFifoFullCb) (app); 
	};
	
/* raccourci d'ecriture */
#define BUS_APPLICATION_CALLBACK(  conn, disconn ) new IvyApplicationCallbackFunction(  conn, disconn )
};

/* Binding Callback */

class IvyBindingCallback {
public:
	virtual void OnAddBind (IvyApplication *app, int id, const char * regexp) = 0;
	virtual void OnRemoveBind (IvyApplication *app, int id, const char * regexp) = 0;
	virtual void OnFilterBind (IvyApplication *app, int id, const char * regexp) = 0;
	virtual void OnChangeBind (IvyApplication *app, int id, const char * regexp) = 0;
	virtual ~IvyBindingCallback() = default;
};

class IvyBindingNullCallback : public IvyBindingCallback {
public:
	void OnAddBind (IvyApplication *app, int id, const char * regexp) override
	{(void)app;(void)id;(void)regexp;};
	void OnRemoveBind (IvyApplication *app, int id, const char * regexp) override
	{(void)app;(void)id;(void)regexp;};
	void OnFilterBind (IvyApplication *app, int id, const char * regexp) override
	{(void)app;(void)id;(void)regexp;};
	void OnChangeBind (IvyApplication *app, int id, const char * regexp) override
	{(void)app;(void)id;(void)regexp;};
	~IvyBindingNullCallback() override = default;
};
// Static function CB
class IvyBindingCallbackFunction: public IvyBindingCallback  {
public:
	typedef	void ( *IvyBindingCallback_fun )( IvyApplication *app, int id, const char * regexp );
	IvyBindingCallback_fun BindingAddCb;
	IvyBindingCallback_fun BindingRemoveCb;
	IvyBindingCallback_fun BindingFilterCb;
	IvyBindingCallback_fun BindingChangeCb;

public:
	IvyBindingCallbackFunction ( IvyBindingCallback_fun add_cb,  IvyBindingCallback_fun remove_cb, IvyBindingCallback_fun filter_cb ) 
		: BindingAddCb( add_cb ), BindingRemoveCb( remove_cb ), BindingFilterCb( filter_cb )
	{
	}
	~IvyBindingCallbackFunction () override = default;

	void OnAddBind (IvyApplication *app, int id, const char * regexp) override
	{
	if(BindingAddCb) (*BindingAddCb) (app, id, regexp);
	};
	void OnRemoveBind (IvyApplication *app, int id, const char * regexp) override
	{
	if (BindingRemoveCb) (*BindingRemoveCb) (app, id, regexp);
	};
	void OnFilterBind (IvyApplication *app, int id, const char * regexp) override
	{
	if(BindingFilterCb ) (*BindingFilterCb) (app, id, regexp);
	};
	void OnChangeBind (IvyApplication *app, int id, const char * regexp) override
	{
	if(BindingChangeCb ) (*BindingChangeCb) (app, id, regexp);
	};
	
/* raccourci d'ecriture */
#define BUS_BINDING_CALLBACK(  add, remove, filter ) new IvyBindingCallbackFunction(  add, remove, filter )
};


/* Callback for the die Message */
class IvyDieCallback {
public:
	virtual bool OnDie (IvyApplication *app, int id, const char *arg ) = 0;
	virtual ~IvyDieCallback() = default;
};
#endif // __IVY_CALLBACK_H__


