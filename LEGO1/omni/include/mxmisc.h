#ifndef MXMISC_H
#define MXMISC_H

#include "lego1_export.h"
#include "mxtypes.h"

class MxAtomSet;
class MxDSAction;
class MxEventManager;
class MxNotificationManager;
class MxObjectFactory;
class MxSoundManager;
class MxStreamer;
class MxTickleManager;
class MxTimer;
class MxVariableTable;
class MxVideoManager;

LEGO1_EXPORT MxTickleManager* TickleManager();
LEGO1_EXPORT MxTimer* Timer();
LEGO1_EXPORT MxStreamer* Streamer();
MxSoundManager* MSoundManager();
LEGO1_EXPORT MxVariableTable* VariableTable();
MxEventManager* EventManager();
LEGO1_EXPORT MxResult Start(MxDSAction*);
MxNotificationManager* NotificationManager();
MxVideoManager* MVideoManager();
MxAtomSet* AtomSet();
MxObjectFactory* ObjectFactory();
void DeleteObject(MxDSAction& p_dsAction);

#endif // MXMISC_H
