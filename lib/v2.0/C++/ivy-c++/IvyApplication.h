#if !defined(IVYAPPLICATION_H)
#define IVYAPPLICATION_H


// IvyApplication.h : header file
//


#include "Ivy.h"


/////////////////////////////////////////////////////////////////////////////
// IvyApplication command target

class IvyApplication 
{
// Attributes
public:
IvyC::IvyClientPtr appptr;
// Operations
public:
	explicit IvyApplication(IvyC::IvyClientPtr ptr );
	virtual ~IvyApplication() = default;
// Overrides
public:
	const char *GetName();
	const char *GetHost();
};

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(IVYAPPLICATION_H)




