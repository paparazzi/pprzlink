// IvyApplication.cpp : implementation file
//

#include "IvyApplication.h"

/////////////////////////////////////////////////////////////////////////////
// IvyApplication

IvyApplication::IvyApplication(IvyC::IvyClientPtr ptr)
{
appptr = ptr;
}

const char *IvyApplication::GetName()
{
  return IvyC::IvyGetApplicationName( appptr );
}


const char *IvyApplication::GetHost()
{
  return IvyC::IvyGetApplicationHost( appptr );
}





