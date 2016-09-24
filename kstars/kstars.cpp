/***************************************************************************
                          kstars.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Feb  5 01:11:45 PST 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstars.h"

#include <QApplication>
#include <QDockWidget>
#include <QDebug>
#include <QStatusBar>
#include <QIcon>
#include <QMenu>

#include <KLocalizedString>
#include <KActionCollection>
#include <KToolBar>
#include <KSharedConfig>

#include "Options.h"
#include "kstarsdata.h"
#include "kstarssplash.h"
#include "kactionmenu.h"
#include "skymap.h"
#include "ksutils.h"
#include "simclock.h"
#include "fov.h"
#include "dialogs/finddialog.h"
#include "dialogs/exportimagedialog.h"
#include "observinglist.h"
//#include "whatsinteresting/wiview.h"

// For profiling only
#include "auxiliary/dms.h"

#include "kstarsadaptor.h"

#include <config-kstars.h>

#ifdef HAVE_INDI
#include "indi/drivermanager.h"
#include "indi/guimanager.h"
#include "ekos/ekosmanager.h"
#endif

#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsviewer.h"
#endif

KStars *KStars::pinstance = 0;

KStars::KStars( bool doSplash, bool clockrun, const QString &startdate )
    : KXmlGuiWindow(), colorActionMenu(0), fovActionMenu(0), m_KStarsData(0), m_SkyMap(0), m_TimeStepBox(0),
      m_ExportImageDialog(0),  m_PrintingWizard(0), m_FindDialog(0), m_AstroCalc(0), m_AltVsTime(0), m_SkyCalendar(0), m_ScriptBuilder(0),
      m_PlanetViewer(0), m_WUTDialog(0), m_JMoonTool(0), m_MoonPhaseTool(0), m_FlagManager(0), m_HorizonManager(0), m_EyepieceView(0),
      m_addDSODialog(0), m_WIView(0), m_ObsConditions(0), m_wiDock(0), DialogIsObsolete(false), StartClockRunning( clockrun ), StartDateString( startdate )
{
    // Initialize logging settings
    if (Options::disableLogging())
        KSUtils::Logging::Disable();
    else if (Options::logToFile())
        KSUtils::Logging::UseFile();
    else
        KSUtils::Logging::UseDefault();

    new KstarsAdaptor(this); // NOTE the weird case convention, which cannot be changed as the file is generated by the moc.

    QDBusConnection::sessionBus().registerObject("/KStars",  this);
    QDBusConnection::sessionBus().registerService("org.kde.kstars");

    #ifdef HAVE_CFITSIO
    m_GenericFITSViewer.clear();
    #endif

    #ifdef HAVE_INDI
    m_EkosManager.clear();
    #endif

    // Set pinstance to yourself
    pinstance = this;

    connect( qApp, SIGNAL( aboutToQuit() ), this, SLOT( slotAboutToQuit() ) );

    //Initialize QActionGroups
    projectionGroup = new QActionGroup( this );
    cschemeGroup    = new QActionGroup( this );

    m_KStarsData = KStarsData::Create();
    Q_ASSERT( m_KStarsData );
    //Set Geographic Location from Options
    m_KStarsData->setLocationFromOptions();

    //Initialize Time and Date
    if (StartDateString.isEmpty() == false)
    {
        KStarsDateTime startDate = KStarsDateTime::fromString( StartDateString );
        if (startDate.isValid() )
            data()->changeDateTime( data()->geo()->LTtoUT( startDate ) );
        else
            data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );
    }
    else
        data()->changeDateTime( KStarsDateTime::currentDateTimeUtc() );

    // Initialize clock. If --paused is not in the comand line, look in options
    if ( clockrun )
        StartClockRunning =  Options::runClock();

    // Setup splash screen
    KStarsSplash *splash = 0;
    if ( doSplash ) {
        splash = new KStarsSplash(0);
        connect( m_KStarsData, SIGNAL( progressText(QString) ), splash, SLOT( setMessage(QString) ));
        splash->show();
    } else {
        connect( m_KStarsData, SIGNAL( progressText(QString) ), m_KStarsData, SLOT( slotConsoleMessage(QString) ) );
    }

    //set up Dark color scheme for application windows
    DarkPalette = QPalette(QColor("black"), QColor("black"));
    DarkPalette.setColor( QPalette::Inactive, QPalette::WindowText, QColor( "red" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::WindowText, QColor( "red" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Base, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::Highlight, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Normal, QPalette::HighlightedText, QColor( "black" ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Text, QColor( 238, 0, 0 ) );
    DarkPalette.setColor( QPalette::Inactive, QPalette::Base, QColor( 30, 10, 10 ) );
    //store original color scheme
    OriginalPalette = QApplication::palette();

    //Initialize data.  When initialization is complete, it will run dataInitFinished()
    if( !m_KStarsData->initialize() )
        return;
    delete splash;
    datainitFinished();

#if ( __GLIBC__ >= 2 &&__GLIBC_MINOR__ >= 1  && !defined(__UCLIBC__) )
    qDebug() << "glibc >= 2.1 detected.  Using GNU extension sincos()";
#else
    qDebug() << "Did not find glibc >= 2.1.  Will use ANSI-compliant sin()/cos() functions.";
#endif
}

KStars *KStars::createInstance( bool doSplash, bool clockrun, const QString &startdate ) {
    delete pinstance;
    // pinstance is set directly in constructor.
    new KStars( doSplash, clockrun, startdate );
    Q_ASSERT( pinstance && "pinstance must be non NULL");
    return pinstance;
}

KStars::~KStars()
{
    Q_ASSERT( pinstance );

    delete m_KStarsData;
    pinstance = 0;

    #ifdef HAVE_INDI
    delete m_EkosManager;
    GUIManager::Instance()->close();
    #endif

    QSqlDatabase::removeDatabase("userdb");
    QSqlDatabase::removeDatabase("skydb");

#ifdef COUNT_DMS_SINCOS_CALLS
    qDebug() << "Constructed " << dms::dms_constructor_calls << " dms objects, of which " << dms::dms_with_sincos_called << " had trigonometric functions called on them = " << ( float( dms::dms_with_sincos_called ) / float( dms::dms_constructor_calls ) ) * 100. << "%";
    qDebug() << "Of the " << dms::trig_function_calls << " calls to sin/cos/sincos on dms objects, " << dms::redundant_trig_function_calls << " were redundant = " << ( ( float( dms::redundant_trig_function_calls ) / float( dms::trig_function_calls ) ) * 100. ) << "%";
#endif

}

void KStars::clearCachedFindDialog() {
    if ( m_FindDialog  ) {  // dialog is cached
        /** Delete findDialog only if it is not opened */
        if ( m_FindDialog->isHidden() ) {
            delete m_FindDialog;
            m_FindDialog = 0;
            DialogIsObsolete = false;
        }
        else
            DialogIsObsolete = true;  // dialog was opened so it could not deleted
    }
}

void KStars::applyConfig( bool doApplyFocus ) {
    if ( Options::isTracking() ) {
        actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
        actionCollection()->action("track_object")->setIcon( QIcon::fromTheme("document-encrypt") );
    }

    actionCollection()->action("coordsys")->setText(
        Options::useAltAz() ? i18n("Switch to star globe view (Equatorial &Coordinates)"): i18n("Switch to horizonal view (Horizontal &Coordinates)") );

    actionCollection()->action("show_time_box"        )->setChecked( Options::showTimeBox() );
    actionCollection()->action("show_location_box"    )->setChecked( Options::showGeoBox() );
    actionCollection()->action("show_focus_box"       )->setChecked( Options::showFocusBox() );
    actionCollection()->action("show_statusBar"       )->setChecked( Options::showStatusBar() );
    actionCollection()->action("show_sbAzAlt"         )->setChecked( Options::showAltAzField() );
    actionCollection()->action("show_sbRADec"         )->setChecked( Options::showRADecField() );
    actionCollection()->action("show_sbJ2000RADec"    )->setChecked( Options::showJ2000RADecField() );
    actionCollection()->action("show_stars"           )->setChecked( Options::showStars() );
    actionCollection()->action("show_deepsky"         )->setChecked( Options::showDeepSky() );
    actionCollection()->action("show_planets"         )->setChecked( Options::showSolarSystem() );
    actionCollection()->action("show_clines"          )->setChecked( Options::showCLines() );
    actionCollection()->action("show_constellationart")->setChecked( Options::showConstellationArt() );
    actionCollection()->action("show_cnames"          )->setChecked( Options::showCNames() );
    actionCollection()->action("show_cbounds"         )->setChecked( Options::showCBounds() );
    actionCollection()->action("show_mw"              )->setChecked( Options::showMilkyWay() );
    actionCollection()->action("show_equatorial_grid" )->setChecked( Options::showEquatorialGrid() );
    actionCollection()->action("show_horizontal_grid" )->setChecked( Options::showHorizontalGrid() );
    actionCollection()->action("show_horizon"         )->setChecked( Options::showGround() );
    actionCollection()->action("show_flags"           )->setChecked( Options::showFlags() );
    actionCollection()->action("show_supernovae"      )->setChecked( Options::showSupernovae() );
    statusBar()->setVisible( Options::showStatusBar() );

    //color scheme
    m_KStarsData->colorScheme()->loadFromConfig();
    QApplication::setPalette( Options::darkAppColors() ? DarkPalette : OriginalPalette );

    //Set toolbar options from config file
    toolBar("kstarsToolBar")->applySettings( KSharedConfig::openConfig()->group( "MainToolBar" ) );
    toolBar( "viewToolBar" )->applySettings( KSharedConfig::openConfig()->group( "ViewToolBar" ) );

    //Geographic location
    data()->setLocationFromOptions();

    //Focus
    if ( doApplyFocus ) {
        SkyObject *fo = data()->objectNamed( Options::focusObject() );
        if ( fo && fo != map()->focusObject() ) {
            map()->setClickedObject( fo );
            map()->setClickedPoint( fo );
            map()->slotCenter();
        }

        if ( ! fo ) {
            SkyPoint fp( Options::focusRA(), Options::focusDec() );
            if ( fp.ra().Degrees() != map()->focus()->ra().Degrees() || fp.dec().Degrees() != map()->focus()->dec().Degrees() ) {
                map()->setClickedPoint( &fp );
                map()->slotCenter();
            }
        }
    }
}

void KStars::showImgExportDialog() {
    if(m_ExportImageDialog)
        m_ExportImageDialog->show();
}

void KStars::syncFOVActions() {
    foreach(QAction *action, fovActionMenu->menu()->actions()) {
        if(action->text().isEmpty()) {
            continue;
        }

        if(Options::fOVNames().contains(action->text().remove(0, 1))) {
            action->setChecked(true);
        } else {
            action->setChecked(false);
        }
    }
}

void KStars::hideAllFovExceptFirst()
{
    // When there is only one visible FOV symbol, we don't need to do anything
    // Also, don't do anything if there are no available FOV symbols.
    if(data()->visibleFOVs.size() == 1 ||
       data()->availFOVs.size() == 0) {
        return;
    } else {
        // If there are no visible FOVs, select first available
        if(data()->visibleFOVs.size() == 0) {
            Q_ASSERT( !data()->availFOVs.isEmpty() );
            Options::setFOVNames(QStringList(data()->availFOVs.first()->name()));
        } else {
            Options::setFOVNames(QStringList(data()->visibleFOVs.first()->name()));
        }

        // Sync FOV and update skymap
        data()->syncFOV();
        syncFOVActions();
        map()->update(); // SkyMap::forceUpdate() is not required, as FOVs are drawn as overlays
    }
}

void KStars::selectNextFov()
{

    if( data()->getVisibleFOVs().isEmpty() )
        return;

    Q_ASSERT( ! data()->getAvailableFOVs().isEmpty() ); // The available FOVs had better not be empty if the visible ones are not.

    FOV *currentFov = data()->getVisibleFOVs().first();
    int currentIdx = data()->availFOVs.indexOf(currentFov);

    // If current FOV is not the available FOV list or there is only 1 FOV available, then return
    if(currentIdx == -1 || data()->availFOVs.size() < 2) {
        return;
    }

    QStringList nextFovName;
    if(currentIdx == data()->availFOVs.size() - 1) {
        nextFovName << data()->availFOVs.first()->name();
    } else {
        nextFovName << data()->availFOVs.at(currentIdx + 1)->name();
    }

    Options::setFOVNames(nextFovName);
    data()->syncFOV();
    syncFOVActions();
    map()->update();
}

void KStars::selectPreviousFov()
{
    if( data()->getVisibleFOVs().isEmpty() )
        return;

    Q_ASSERT( ! data()->getAvailableFOVs().isEmpty() ); // The available FOVs had better not be empty if the visible ones are not.

    FOV *currentFov = data()->getVisibleFOVs().first();
    int currentIdx = data()->availFOVs.indexOf(currentFov);

    // If current FOV is not the available FOV list or there is only 1 FOV available, then return
    if(currentIdx == -1 || data()->availFOVs.size() < 2) {
        return;
    }

    QStringList prevFovName;
    if(currentIdx == 0) {
        prevFovName << data()->availFOVs.last()->name();
    } else {
        prevFovName << data()->availFOVs.at(currentIdx - 1)->name();
    }

    Options::setFOVNames(prevFovName);
    data()->syncFOV();
    syncFOVActions();
    map()->update();
}

//FIXME Port to QML2
//#if 0
void KStars::showWISettingsUI()
{
    slotWISettings();
}
//#endif

void KStars::updateTime( const bool automaticDSTchange ) {
    // Due to frequently use of this function save data and map pointers for speedup.
    // Save options and geo() to a pointer would not speedup because most of time options
    // and geo will accessed only one time.
    KStarsData *Data = data();
    // dms oldLST( Data->lst()->Degrees() );

    Data->updateTime( Data->geo(), automaticDSTchange );

    //We do this outside of kstarsdata just to get the coordinates
    //displayed in the infobox to update every second.
    //	if ( !Options::isTracking() && LST()->Degrees() > oldLST.Degrees() ) {
    //		int nSec = int( 3600.*( LST()->Hours() - oldLST.Hours() ) );
    //		Map->focus()->setRA( Map->focus()->ra().Hours() + double( nSec )/3600. );
    //		if ( Options::useAltAz() ) Map->focus()->EquatorialToHorizontal( LST(), geo()->lat() );
    //		Map->showFocusCoords();
    //	}

    //If time is accelerated beyond slewTimescale, then the clock's timer is stopped,
    //so that it can be ticked manually after each update, in order to make each time
    //step exactly equal to the timeScale setting.
    //Wrap the call to manualTick() in a singleshot timer so that it doesn't get called until
    //the skymap has been completely updated.
    if ( Data->clock()->isManualMode() && Data->clock()->isActive() ) {
        QTimer::singleShot( 0, Data->clock(), SLOT( manualTick() ) );
    }
}

#ifdef HAVE_CFITSIO
FITSViewer * KStars::genericFITSViewer()
{
    if (m_GenericFITSViewer.isNull())
    {
        m_GenericFITSViewer = new FITSViewer(Options::independentWindowFITS() ? NULL : this);
        m_GenericFITSViewer->setAttribute(Qt::WA_DeleteOnClose);
    }

    return m_GenericFITSViewer;
}
#endif

#ifdef HAVE_INDI
EkosManager *KStars::ekosManager()
{
    if (m_EkosManager.isNull())
        m_EkosManager   = new EkosManager(Options::independentWindowEkos() ? NULL : this);

    return m_EkosManager;
}


#endif
