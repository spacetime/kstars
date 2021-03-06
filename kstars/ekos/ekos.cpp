#include "ekos.h"

namespace Ekos
{
    const QString & getGuideStatusString(GuideState state) { return guideStates[state]; }
    const QString & getCaptureStatusString(CaptureState state) { return captureStates[state]; }
    const QString & getFocusStatusString(FocusState state) { return focusStates[state]; }
}
