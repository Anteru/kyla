#include "Repository.h"

namespace kyla {
///////////////////////////////////////////////////////////////////////////////
void IRepository::Validate (const ValidationCallback& validationCallback)
{
	ValidateImpl (validationCallback);
}

///////////////////////////////////////////////////////////////////////////////
void DeployedRepository::ValidateImpl (const ValidationCallback& validationCallback)
{

}
}