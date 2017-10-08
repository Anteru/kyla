#include "OpenSourceRepositoryThread.h"

#include "SetupContext.h"

////////////////////////////////////////////////////////////////////////////////
OpenSourceRepositoryThread::OpenSourceRepositoryThread (SetupContext* context, 
    const QString& sourceRepositoryPath)
: context_ (context)
, sourceRepositoryPath_ (sourceRepositoryPath)
{
}

////////////////////////////////////////////////////////////////////////////////
void OpenSourceRepositoryThread::run ()
{
    const auto sourceRepositoryPath = sourceRepositoryPath_.toUtf8 ();
    const auto result = context_->installer->OpenSourceRepository (
        context_->installer,
        sourceRepositoryPath.data (),
        kylaRepositoryOption_ReadOnly,
        &context_->sourceRepository);

    emit RepositoryOpened (result == kylaResult_Ok);
}