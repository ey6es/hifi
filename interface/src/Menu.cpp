//
//  Menu.cpp
//  interface/src
//
//  Created by Stephen Birarda on 8/12/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <cstdlib>

#include <QBoxLayout>
#include <QColorDialog>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QShortcut>
#include <QSlider>
#include <QUuid>
#include <QHBoxLayout>
#include <QDesktopServices>

#include <AccountManager.h>
#include <AddressManager.h>
#include <DependencyManager.h>
#include <MainWindow.h>
#include <GlowEffect.h>
#include <PathUtils.h>
#include <UUID.h>
#include <UserActivityLogger.h>
#include <XmppClient.h>

#include "Application.h"
#include "AccountManager.h"
#include "Audio.h"
#include "audio/AudioIOStatsRenderer.h"
#include "audio/AudioScope.h"
#include "devices/RealSense.h"
#include "devices/Visage.h"
#include "Menu.h"
#include "scripting/LocationScriptingInterface.h"
#include "scripting/MenuScriptingInterface.h"
#include "Util.h"
#include "ui/AddressBarDialog.h"
#include "ui/AnimationsDialog.h"
#include "ui/AttachmentsDialog.h"
#include "ui/BandwidthDialog.h"
#include "ui/CachesSizeDialog.h"
#include "ui/DataWebDialog.h"
#include "ui/HMDToolsDialog.h"
#include "ui/LodToolsDialog.h"
#include "ui/LoginDialog.h"
#include "ui/OctreeStatsDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/InfoView.h"
#include "ui/MetavoxelEditor.h"
#include "ui/MetavoxelNetworkSimulator.h"
#include "ui/ModelsBrowser.h"
#include "ui/NodeBounds.h"

Menu* Menu::_instance = NULL;

Menu* Menu::getInstance() {
    static QMutex menuInstanceMutex;

    // lock the menu instance mutex to make sure we don't race and create two menus and crash
    menuInstanceMutex.lock();

    if (!_instance) {
        qDebug("First call to Menu::getInstance() - initing menu.");

        _instance = new Menu();
    }

    menuInstanceMutex.unlock();

    return _instance;
}

Menu::Menu() :
    _lastAdjust(usecTimestampNow()),
    _lastAvatarDetailDrop(usecTimestampNow())
{
    Application *appInstance = Application::getInstance();

    QMenu* fileMenu = addMenu("File");

#ifdef Q_OS_MAC
    addActionToQMenuAndActionHash(fileMenu,
                                  MenuOption::AboutApp,
                                  0,
                                  this,
                                  SLOT(aboutApp()),
                                  QAction::AboutRole);
#endif

    AccountManager& accountManager = AccountManager::getInstance();

    _loginAction = addActionToQMenuAndActionHash(fileMenu, MenuOption::Logout);

    // call our toggle login function now so the menu option is setup properly
    toggleLoginMenuItem();

    // connect to the appropriate signal of the AccountManager so that we can change the Login/Logout menu item
    connect(&accountManager, &AccountManager::profileChanged, this, &Menu::toggleLoginMenuItem);
    connect(&accountManager, &AccountManager::logoutComplete, this, &Menu::toggleLoginMenuItem);

    addDisabledActionAndSeparator(fileMenu, "Scripts");
    addActionToQMenuAndActionHash(fileMenu, MenuOption::LoadScript, Qt::CTRL | Qt::Key_O, appInstance, SLOT(loadDialog()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::LoadScriptURL,
                                    Qt::CTRL | Qt::SHIFT | Qt::Key_O, appInstance, SLOT(loadScriptURLDialog()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::StopAllScripts, 0, appInstance, SLOT(stopAllScripts()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::ReloadAllScripts, Qt::CTRL | Qt::Key_R,
                                  appInstance, SLOT(reloadAllScripts()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::RunningScripts, Qt::CTRL | Qt::Key_J,
                                  appInstance, SLOT(toggleRunningScriptsWidget()));

    addDisabledActionAndSeparator(fileMenu, "Location");
    addActionToQMenuAndActionHash(fileMenu, MenuOption::BookmarkLocation, 0,
                                  this, SLOT(bookmarkLocation()));
    _bookmarksMenu = fileMenu->addMenu(MenuOption::Bookmarks);
    _bookmarksMenu->setEnabled(false);
    _deleteBookmarksMenu = addActionToQMenuAndActionHash(fileMenu,
                                  MenuOption::DeleteBookmark, 0,
                                  this, SLOT(deleteBookmark()));
    _deleteBookmarksMenu->setEnabled(false);
    loadBookmarks();
    addActionToQMenuAndActionHash(fileMenu,
                                  MenuOption::AddressBar,
                                  Qt::Key_Enter,
                                  this,
                                  SLOT(toggleAddressBar()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::CopyAddress, 0,
                                  this, SLOT(copyAddress()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::CopyPath, 0,
                                  this, SLOT(copyPath()));

    addDisabledActionAndSeparator(fileMenu, "Upload Avatar Model");
    addActionToQMenuAndActionHash(fileMenu, MenuOption::UploadHead, 0,
                                  Application::getInstance(), SLOT(uploadHead()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::UploadSkeleton, 0,
                                  Application::getInstance(), SLOT(uploadSkeleton()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::UploadAttachment, 0,
                                  Application::getInstance(), SLOT(uploadAttachment()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::UploadEntity, 0,
                                  Application::getInstance(), SLOT(uploadEntity()));
    addDisabledActionAndSeparator(fileMenu, "Settings");
    addActionToQMenuAndActionHash(fileMenu, MenuOption::SettingsImport, 0, this, SLOT(importSettings()));
    addActionToQMenuAndActionHash(fileMenu, MenuOption::SettingsExport, 0, this, SLOT(exportSettings()));

    addActionToQMenuAndActionHash(fileMenu,
                                  MenuOption::Quit,
                                  Qt::CTRL | Qt::Key_Q,
                                  appInstance,
                                  SLOT(quit()),
                                  QAction::QuitRole);


    QMenu* editMenu = addMenu("Edit");

    QUndoStack* undoStack = Application::getInstance()->getUndoStack();
    QAction* undoAction = undoStack->createUndoAction(editMenu);
    undoAction->setShortcut(Qt::CTRL | Qt::Key_Z);
    addActionToQMenuAndActionHash(editMenu, undoAction);

    QAction* redoAction = undoStack->createRedoAction(editMenu);
    redoAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Z);
    addActionToQMenuAndActionHash(editMenu, redoAction);

    addActionToQMenuAndActionHash(editMenu,
                                  MenuOption::Preferences,
                                  Qt::CTRL | Qt::Key_Comma,
                                  this,
                                  SLOT(editPreferences()),
                                  QAction::PreferencesRole);

    addActionToQMenuAndActionHash(editMenu, MenuOption::Attachments, 0, this, SLOT(editAttachments()));
    addActionToQMenuAndActionHash(editMenu, MenuOption::Animations, 0, this, SLOT(editAnimations()));

    QMenu* toolsMenu = addMenu("Tools");
    addActionToQMenuAndActionHash(toolsMenu, MenuOption::MetavoxelEditor, 0, this, SLOT(showMetavoxelEditor()));
    addActionToQMenuAndActionHash(toolsMenu, MenuOption::ScriptEditor,  Qt::ALT | Qt::Key_S, this, SLOT(showScriptEditor()));

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    QAction* speechRecognizerAction = addCheckableActionToQMenuAndActionHash(toolsMenu, MenuOption::ControlWithSpeech,
            Qt::CTRL | Qt::SHIFT | Qt::Key_C, _speechRecognizer.getEnabled(), &_speechRecognizer, SLOT(setEnabled(bool)));
    connect(&_speechRecognizer, SIGNAL(enabledUpdated(bool)), speechRecognizerAction, SLOT(setChecked(bool)));
#endif

#ifdef HAVE_QXMPP
    _chatAction = addActionToQMenuAndActionHash(toolsMenu,
                                                MenuOption::Chat,
                                                Qt::Key_Backslash,
                                                this,
                                                SLOT(showChat()));

    const QXmppClient& xmppClient = XmppClient::getInstance().getXMPPClient();
    toggleChat();
    connect(&xmppClient, &QXmppClient::connected, this, &Menu::toggleChat);
    connect(&xmppClient, &QXmppClient::disconnected, this, &Menu::toggleChat);

    QDir::setCurrent(PathUtils::resourcesPath());
    // init chat window to listen chat
    _chatWindow = new ChatWindow(Application::getInstance()->getWindow());
#endif

    addActionToQMenuAndActionHash(toolsMenu,
                                  MenuOption::ToolWindow,
                                  Qt::CTRL | Qt::ALT | Qt::Key_T,
                                  this,
                                  SLOT(toggleToolWindow()));

    addActionToQMenuAndActionHash(toolsMenu,
                                  MenuOption::Console,
                                  Qt::CTRL | Qt::ALT | Qt::Key_J,
                                  this,
                                  SLOT(toggleConsole()));

    addActionToQMenuAndActionHash(toolsMenu,
                                  MenuOption::ResetSensors,
                                  Qt::Key_Apostrophe,
                                  appInstance,
                                  SLOT(resetSensors()));

    QMenu* avatarMenu = addMenu("Avatar");

    QMenu* avatarSizeMenu = avatarMenu->addMenu("Size");
    addActionToQMenuAndActionHash(avatarSizeMenu,
                                  MenuOption::IncreaseAvatarSize,
                                  Qt::Key_Plus,
                                  appInstance->getAvatar(),
                                  SLOT(increaseSize()));
    addActionToQMenuAndActionHash(avatarSizeMenu,
                                  MenuOption::DecreaseAvatarSize,
                                  Qt::Key_Minus,
                                  appInstance->getAvatar(),
                                  SLOT(decreaseSize()));
    addActionToQMenuAndActionHash(avatarSizeMenu,
                                  MenuOption::ResetAvatarSize,
                                  Qt::Key_Equal,
                                  appInstance->getAvatar(),
                                  SLOT(resetSize()));

    QObject* avatar = appInstance->getAvatar();
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::KeyboardMotorControl, 
            Qt::CTRL | Qt::SHIFT | Qt::Key_K, true, avatar, SLOT(updateMotionBehavior()));
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::ScriptedMotorControl, 0, true,
            avatar, SLOT(updateMotionBehavior()));
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::ChatCircling, 0, false);
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::NamesAboveHeads, 0, true);
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::GlowWhenSpeaking, 0, true);
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::BlueSpeechSphere, 0, true);
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::ObeyEnvironmentalGravity, Qt::SHIFT | Qt::Key_G, false,
            avatar, SLOT(updateMotionBehavior()));
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::StandOnNearbyFloors, 0, true,
            avatar, SLOT(updateMotionBehavior()));
    addCheckableActionToQMenuAndActionHash(avatarMenu, MenuOption::ShiftHipsForIdleAnimations, 0, false,
            avatar, SLOT(updateMotionBehavior()));

    QMenu* collisionsMenu = avatarMenu->addMenu("Collide With");
    addCheckableActionToQMenuAndActionHash(collisionsMenu, MenuOption::CollideAsRagdoll, 0, false, 
            avatar, SLOT(onToggleRagdoll()));
    addCheckableActionToQMenuAndActionHash(collisionsMenu, MenuOption::CollideWithAvatars,
            0, true, avatar, SLOT(updateCollisionGroups()));
    addCheckableActionToQMenuAndActionHash(collisionsMenu, MenuOption::CollideWithEnvironment,
            0, false, avatar, SLOT(updateCollisionGroups()));

    QMenu* viewMenu = addMenu("View");

#ifdef Q_OS_MAC
    addCheckableActionToQMenuAndActionHash(viewMenu,
                                           MenuOption::Fullscreen,
                                           Qt::CTRL | Qt::META | Qt::Key_F,
                                           false,
                                           appInstance,
                                           SLOT(setFullscreen(bool)));
#else
    addCheckableActionToQMenuAndActionHash(viewMenu,
                                           MenuOption::Fullscreen,
                                           Qt::CTRL | Qt::Key_F,
                                           false,
                                           appInstance,
                                           SLOT(setFullscreen(bool)));
#endif
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::FirstPerson, Qt::Key_P, true,
                                            appInstance,SLOT(cameraMenuChanged()));
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::Mirror, Qt::SHIFT | Qt::Key_H, true);
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::FullscreenMirror, Qt::Key_H, false,
                                            appInstance, SLOT(cameraMenuChanged()));
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::UserInterface, Qt::Key_Slash, true);

    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::HMDTools, Qt::META | Qt::Key_H,
                                           false,
                                           this,
                                           SLOT(hmdTools(bool)));


    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::EnableVRMode, 0,
                                           false,
                                           appInstance,
                                           SLOT(setEnableVRMode(bool)));

    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::Enable3DTVMode, 0,
                                           false,
                                           appInstance,
                                           SLOT(setEnable3DTVMode(bool)));


    QMenu* nodeBordersMenu = viewMenu->addMenu("Server Borders");
    NodeBounds& nodeBounds = appInstance->getNodeBoundsDisplay();
    addCheckableActionToQMenuAndActionHash(nodeBordersMenu, MenuOption::ShowBordersEntityNodes,
                                           Qt::CTRL | Qt::SHIFT | Qt::Key_1, false,
                                           &nodeBounds, SLOT(setShowEntityNodes(bool)));

    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::OffAxisProjection, 0, false);
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::TurnWithHead, 0, false);


    addDisabledActionAndSeparator(viewMenu, "Stats");
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::Stats, Qt::Key_Percent);
    addActionToQMenuAndActionHash(viewMenu, MenuOption::Log, Qt::CTRL | Qt::Key_L, appInstance, SLOT(toggleLogDialog()));
    addCheckableActionToQMenuAndActionHash(viewMenu, MenuOption::Bandwidth, 0, true);
    addActionToQMenuAndActionHash(viewMenu, MenuOption::BandwidthDetails, 0, this, SLOT(bandwidthDetails()));
    addActionToQMenuAndActionHash(viewMenu, MenuOption::OctreeStats, 0, this, SLOT(octreeStatsDetails()));
    addActionToQMenuAndActionHash(viewMenu, MenuOption::EditEntitiesHelp, 0, this, SLOT(showEditEntitiesHelp()));

    QMenu* developerMenu = addMenu("Developer");

    QMenu* renderOptionsMenu = developerMenu->addMenu("Render");
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::Atmosphere, Qt::SHIFT | Qt::Key_A, true);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::Avatars, 0, true);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::Metavoxels, 0, true);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::Entities, 0, true);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::AmbientOcclusion);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::DontFadeOnOctreeServerChanges);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::DisableAutoAdjustLOD);

    QMenu* ambientLightMenu = renderOptionsMenu->addMenu(MenuOption::RenderAmbientLight);
    QActionGroup* ambientLightGroup = new QActionGroup(ambientLightMenu);
    ambientLightGroup->setExclusive(true);
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLightGlobal, 0, true));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight0, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight1, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight2, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight3, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight4, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight5, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight6, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight7, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight8, 0, false));
    ambientLightGroup->addAction(addCheckableActionToQMenuAndActionHash(ambientLightMenu, MenuOption::RenderAmbientLight9, 0, false));
    
    QMenu* shadowMenu = renderOptionsMenu->addMenu("Shadows");
    QActionGroup* shadowGroup = new QActionGroup(shadowMenu);
    shadowGroup->addAction(addCheckableActionToQMenuAndActionHash(shadowMenu, "None", 0, true));
    shadowGroup->addAction(addCheckableActionToQMenuAndActionHash(shadowMenu, MenuOption::SimpleShadows, 0, false));
    shadowGroup->addAction(addCheckableActionToQMenuAndActionHash(shadowMenu, MenuOption::CascadedShadows, 0, false));

    {
        QMenu* framerateMenu = renderOptionsMenu->addMenu(MenuOption::RenderTargetFramerate);
        QActionGroup* framerateGroup = new QActionGroup(framerateMenu);
        framerateGroup->setExclusive(true);
        framerateGroup->addAction(addCheckableActionToQMenuAndActionHash(framerateMenu, MenuOption::RenderTargetFramerateUnlimited, 0, true));
        framerateGroup->addAction(addCheckableActionToQMenuAndActionHash(framerateMenu, MenuOption::RenderTargetFramerate60, 0, false));
        framerateGroup->addAction(addCheckableActionToQMenuAndActionHash(framerateMenu, MenuOption::RenderTargetFramerate50, 0, false));
        framerateGroup->addAction(addCheckableActionToQMenuAndActionHash(framerateMenu, MenuOption::RenderTargetFramerate40, 0, false));
        framerateGroup->addAction(addCheckableActionToQMenuAndActionHash(framerateMenu, MenuOption::RenderTargetFramerate30, 0, false));

#if defined(Q_OS_MAC)
#else
        addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::RenderTargetFramerateVSyncOn, 0, true, this, SLOT(changeVSync()));
#endif
    }


    QMenu* resolutionMenu = renderOptionsMenu->addMenu(MenuOption::RenderResolution);
    QActionGroup* resolutionGroup = new QActionGroup(resolutionMenu);
    resolutionGroup->setExclusive(true);
    resolutionGroup->addAction(addCheckableActionToQMenuAndActionHash(resolutionMenu, MenuOption::RenderResolutionOne, 0, true));
    resolutionGroup->addAction(addCheckableActionToQMenuAndActionHash(resolutionMenu, MenuOption::RenderResolutionTwoThird, 0, false));
    resolutionGroup->addAction(addCheckableActionToQMenuAndActionHash(resolutionMenu, MenuOption::RenderResolutionHalf, 0, false));
    resolutionGroup->addAction(addCheckableActionToQMenuAndActionHash(resolutionMenu, MenuOption::RenderResolutionThird, 0, false));
    resolutionGroup->addAction(addCheckableActionToQMenuAndActionHash(resolutionMenu, MenuOption::RenderResolutionQuarter, 0, false));

    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::Stars, Qt::Key_Asterisk, true);
    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::EnableGlowEffect, 0, true, 
                                            DependencyManager::get<GlowEffect>().data(), SLOT(toggleGlowEffect(bool)));

    addCheckableActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::Wireframe, Qt::ALT | Qt::Key_W, false);
    addActionToQMenuAndActionHash(renderOptionsMenu, MenuOption::LodTools, Qt::SHIFT | Qt::Key_L, this, SLOT(lodTools()));

    QMenu* avatarDebugMenu = developerMenu->addMenu("Avatar");
#ifdef HAVE_FACESHIFT
    addCheckableActionToQMenuAndActionHash(avatarDebugMenu,
                                           MenuOption::Faceshift,
                                           0,
                                           true,
                                           DependencyManager::get<Faceshift>().data(),
                                           SLOT(setTCPEnabled(bool)));
#endif
#ifdef HAVE_VISAGE
    addCheckableActionToQMenuAndActionHash(avatarDebugMenu, MenuOption::Visage, 0, false,
            DependencyManager::get<Visage>().data(), SLOT(updateEnabled()));
#endif

    addCheckableActionToQMenuAndActionHash(avatarDebugMenu, MenuOption::RenderSkeletonCollisionShapes);
    addCheckableActionToQMenuAndActionHash(avatarDebugMenu, MenuOption::RenderHeadCollisionShapes);
    addCheckableActionToQMenuAndActionHash(avatarDebugMenu, MenuOption::RenderBoundingCollisionShapes);
    addCheckableActionToQMenuAndActionHash(avatarDebugMenu, MenuOption::RenderLookAtVectors, 0, false);
    addCheckableActionToQMenuAndActionHash(avatarDebugMenu, MenuOption::RenderFocusIndicator, 0, false);

    QMenu* metavoxelOptionsMenu = developerMenu->addMenu("Metavoxels");
    addCheckableActionToQMenuAndActionHash(metavoxelOptionsMenu, MenuOption::DisplayHermiteData, 0, false);
    addCheckableActionToQMenuAndActionHash(metavoxelOptionsMenu, MenuOption::RenderHeightfields, 0, true);
    addCheckableActionToQMenuAndActionHash(metavoxelOptionsMenu, MenuOption::RenderDualContourSurfaces, 0, true);
    addActionToQMenuAndActionHash(metavoxelOptionsMenu, MenuOption::NetworkSimulator, 0, this,
        SLOT(showMetavoxelNetworkSimulator()));
    
    QMenu* handOptionsMenu = developerMenu->addMenu("Hands");
    addCheckableActionToQMenuAndActionHash(handOptionsMenu, MenuOption::AlignForearmsWithWrists, 0, false);
    addCheckableActionToQMenuAndActionHash(handOptionsMenu, MenuOption::AlternateIK, 0, false);
    addCheckableActionToQMenuAndActionHash(handOptionsMenu, MenuOption::DisplayHands, 0, true);
    addCheckableActionToQMenuAndActionHash(handOptionsMenu, MenuOption::DisplayHandTargets, 0, false);
    addCheckableActionToQMenuAndActionHash(handOptionsMenu, MenuOption::ShowIKConstraints, 0, false);
    
    QMenu* sixenseOptionsMenu = handOptionsMenu->addMenu("Sixense");
#ifdef __APPLE__
    addCheckableActionToQMenuAndActionHash(sixenseOptionsMenu,
                                           MenuOption::SixenseEnabled,
                                           0, false,
                                           this,
                                           SLOT(toggleSixense(bool)));
#endif
    addCheckableActionToQMenuAndActionHash(sixenseOptionsMenu,
                                           MenuOption::FilterSixense,
                                           0,
                                           true,
                                           &SixenseManager::getInstance(),
                                           SLOT(setFilter(bool)));
    addCheckableActionToQMenuAndActionHash(sixenseOptionsMenu,
                                           MenuOption::LowVelocityFilter,
                                           0,
                                           true,
                                           appInstance,
                                           SLOT(setLowVelocityFilter(bool)));
    addCheckableActionToQMenuAndActionHash(sixenseOptionsMenu, MenuOption::SixenseMouseInput, 0, true);
    addCheckableActionToQMenuAndActionHash(sixenseOptionsMenu, MenuOption::SixenseLasers, 0, false);

    QMenu* leapOptionsMenu = handOptionsMenu->addMenu("Leap Motion");
    addCheckableActionToQMenuAndActionHash(leapOptionsMenu, MenuOption::LeapMotionOnHMD, 0, false);

#ifdef HAVE_RSSDK
    QMenu* realSenseOptionsMenu = handOptionsMenu->addMenu("RealSense");
    addActionToQMenuAndActionHash(realSenseOptionsMenu, MenuOption::LoadRSSDKFile, 0, this, SLOT(loadRSSDKFile()));
#endif

    QMenu* networkMenu = developerMenu->addMenu("Network");
    addCheckableActionToQMenuAndActionHash(networkMenu, MenuOption::DisableNackPackets, 0, false);
    addCheckableActionToQMenuAndActionHash(networkMenu,
                                           MenuOption::DisableActivityLogger,
                                           0,
                                           false,
                                           &UserActivityLogger::getInstance(),
                                           SLOT(disable(bool)));
    addActionToQMenuAndActionHash(networkMenu, MenuOption::CachesSize, 0, this, SLOT(cachesSizeDialog()));
    
    addActionToQMenuAndActionHash(developerMenu, MenuOption::WalletPrivateKey, 0, this, SLOT(changePrivateKey()));

    QMenu* timingMenu = developerMenu->addMenu("Timing and Stats");
    QMenu* perfTimerMenu = timingMenu->addMenu("Performance Timer");
    addCheckableActionToQMenuAndActionHash(perfTimerMenu, MenuOption::DisplayTimingDetails, 0, true);
    addCheckableActionToQMenuAndActionHash(perfTimerMenu, MenuOption::ExpandUpdateTiming, 0, false);
    addCheckableActionToQMenuAndActionHash(perfTimerMenu, MenuOption::ExpandMyAvatarTiming, 0, false);
    addCheckableActionToQMenuAndActionHash(perfTimerMenu, MenuOption::ExpandMyAvatarSimulateTiming, 0, false);
    addCheckableActionToQMenuAndActionHash(perfTimerMenu, MenuOption::ExpandOtherAvatarTiming, 0, false);
    addCheckableActionToQMenuAndActionHash(perfTimerMenu, MenuOption::ExpandPaintGLTiming, 0, false);

    addCheckableActionToQMenuAndActionHash(timingMenu, MenuOption::TestPing, 0, true);
    addCheckableActionToQMenuAndActionHash(timingMenu, MenuOption::FrameTimer);
    addActionToQMenuAndActionHash(timingMenu, MenuOption::RunTimingTests, 0, this, SLOT(runTests()));
    addCheckableActionToQMenuAndActionHash(timingMenu, MenuOption::PipelineWarnings);
    addCheckableActionToQMenuAndActionHash(timingMenu, MenuOption::SuppressShortTimings);

    auto audioIO = DependencyManager::get<Audio>();
    QMenu* audioDebugMenu = developerMenu->addMenu("Audio");
    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::AudioNoiseReduction,
                                           0,
                                           true,
                                           audioIO.data(),
                                           SLOT(toggleAudioNoiseReduction()));

    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::EchoServerAudio, 0, false,
                                           audioIO.data(), SLOT(toggleServerEcho()));
    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::EchoLocalAudio, 0, false,
                                           audioIO.data(), SLOT(toggleLocalEcho()));
    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::StereoAudio, 0, false,
                                           audioIO.data(), SLOT(toggleStereoInput()));
    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::MuteAudio,
                                           Qt::CTRL | Qt::Key_M,
                                           false,
                                           audioIO.data(),
                                           SLOT(toggleMute()));
    addActionToQMenuAndActionHash(audioDebugMenu,
                                  MenuOption::MuteEnvironment,
                                  0,
                                  audioIO.data(),
                                  SLOT(sendMuteEnvironmentPacket()));

    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::AudioSourceInject,
                                           0,
                                           false,
                                           audioIO.data(),
                                           SLOT(toggleAudioSourceInject()));
    QMenu* audioSourceMenu = audioDebugMenu->addMenu("Generated Audio Source"); 
    {
        QAction *pinkNoise = addCheckableActionToQMenuAndActionHash(audioSourceMenu, MenuOption::AudioSourcePinkNoise,
                                                               0,
                                                               false,
                                                               audioIO.data(),
                                                               SLOT(selectAudioSourcePinkNoise()));
        
        QAction *sine440 = addCheckableActionToQMenuAndActionHash(audioSourceMenu, MenuOption::AudioSourceSine440,
                                                                    0,
                                                                    true,
                                                                    audioIO.data(),
                                                                    SLOT(selectAudioSourceSine440()));

        QActionGroup* audioSourceGroup = new QActionGroup(audioSourceMenu);
        audioSourceGroup->addAction(pinkNoise);
        audioSourceGroup->addAction(sine440);
    }
    
    auto scope = DependencyManager::get<AudioScope>();

    QMenu* audioScopeMenu = audioDebugMenu->addMenu("Audio Scope");
    addCheckableActionToQMenuAndActionHash(audioScopeMenu, MenuOption::AudioScope,
                                           Qt::CTRL | Qt::Key_P, false,
                                           scope.data(),
                                           SLOT(toggle()));
    addCheckableActionToQMenuAndActionHash(audioScopeMenu, MenuOption::AudioScopePause,
                                           Qt::CTRL | Qt::SHIFT | Qt::Key_P ,
                                           false,
                                           scope.data(),
                                           SLOT(togglePause()));
    addDisabledActionAndSeparator(audioScopeMenu, "Display Frames");
    {
        QAction *fiveFrames = addCheckableActionToQMenuAndActionHash(audioScopeMenu, MenuOption::AudioScopeFiveFrames,
                                               0,
                                               true,
                                               scope.data(),
                                               SLOT(selectAudioScopeFiveFrames()));

        QAction *twentyFrames = addCheckableActionToQMenuAndActionHash(audioScopeMenu, MenuOption::AudioScopeTwentyFrames,
                                               0,
                                               false,
                                               scope.data(),
                                               SLOT(selectAudioScopeTwentyFrames()));

        QAction *fiftyFrames = addCheckableActionToQMenuAndActionHash(audioScopeMenu, MenuOption::AudioScopeFiftyFrames,
                                               0,
                                               false,
                                               scope.data(),
                                               SLOT(selectAudioScopeFiftyFrames()));

        QActionGroup* audioScopeFramesGroup = new QActionGroup(audioScopeMenu);
        audioScopeFramesGroup->addAction(fiveFrames);
        audioScopeFramesGroup->addAction(twentyFrames);
        audioScopeFramesGroup->addAction(fiftyFrames);
    }
    
    auto statsRenderer = DependencyManager::get<AudioIOStatsRenderer>();
    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::AudioStats,
                                           Qt::CTRL | Qt::SHIFT | Qt::Key_A,
                                           false,
                                           statsRenderer.data(),
                                           SLOT(toggle()));

    addCheckableActionToQMenuAndActionHash(audioDebugMenu, MenuOption::AudioStatsShowInjectedStreams,
                                            0,
                                            false,
                                            statsRenderer.data(),
                                            SLOT(toggleShowInjectedStreams()));

    connect(audioIO.data(), SIGNAL(muteToggled()), this, SLOT(audioMuteToggled()));

#ifndef Q_OS_MAC
    QMenu* helpMenu = addMenu("Help");
    QAction* helpAction = helpMenu->addAction(MenuOption::AboutApp);
    connect(helpAction, SIGNAL(triggered()), this, SLOT(aboutApp()));
#endif
}

void Menu::loadSettings(QSettings* settings) {
    bool lockedSettings = false;
    if (!settings) {
        settings = Application::getInstance()->lockSettings();
        lockedSettings = true;
    }

    _receivedAudioStreamSettings._dynamicJitterBuffers = settings->value("dynamicJitterBuffers", DEFAULT_DYNAMIC_JITTER_BUFFERS).toBool();
    _receivedAudioStreamSettings._maxFramesOverDesired = settings->value("maxFramesOverDesired", DEFAULT_MAX_FRAMES_OVER_DESIRED).toInt();
    _receivedAudioStreamSettings._staticDesiredJitterBufferFrames = settings->value("staticDesiredJitterBufferFrames", DEFAULT_STATIC_DESIRED_JITTER_BUFFER_FRAMES).toInt();
    _receivedAudioStreamSettings._useStDevForJitterCalc = settings->value("useStDevForJitterCalc", DEFAULT_USE_STDEV_FOR_JITTER_CALC).toBool();
    _receivedAudioStreamSettings._windowStarveThreshold = settings->value("windowStarveThreshold", DEFAULT_WINDOW_STARVE_THRESHOLD).toInt();
    _receivedAudioStreamSettings._windowSecondsForDesiredCalcOnTooManyStarves = settings->value("windowSecondsForDesiredCalcOnTooManyStarves", DEFAULT_WINDOW_SECONDS_FOR_DESIRED_CALC_ON_TOO_MANY_STARVES).toInt();
    _receivedAudioStreamSettings._windowSecondsForDesiredReduction = settings->value("windowSecondsForDesiredReduction", DEFAULT_WINDOW_SECONDS_FOR_DESIRED_REDUCTION).toInt();
    _receivedAudioStreamSettings._repetitionWithFade = settings->value("repetitionWithFade", DEFAULT_REPETITION_WITH_FADE).toBool();

    auto audio = DependencyManager::get<Audio>();
    audio->setOutputStarveDetectionEnabled(settings->value("audioOutputStarveDetectionEnabled", DEFAULT_AUDIO_OUTPUT_STARVE_DETECTION_ENABLED).toBool());
    audio->setOutputStarveDetectionThreshold(settings->value("audioOutputStarveDetectionThreshold", DEFAULT_AUDIO_OUTPUT_STARVE_DETECTION_THRESHOLD).toInt());
    audio->setOutputStarveDetectionPeriod(settings->value("audioOutputStarveDetectionPeriod", DEFAULT_AUDIO_OUTPUT_STARVE_DETECTION_PERIOD).toInt());
    int bufferSize = settings->value("audioOutputBufferSize", DEFAULT_AUDIO_OUTPUT_BUFFER_SIZE_FRAMES).toInt();
    QMetaObject::invokeMethod(audio.data(), "setOutputBufferSize", Q_ARG(int, bufferSize));
    
    _fieldOfView = loadSetting(settings, "fieldOfView", DEFAULT_FIELD_OF_VIEW_DEGREES);
    _realWorldFieldOfView = loadSetting(settings, "realWorldFieldOfView", DEFAULT_REAL_WORLD_FIELD_OF_VIEW_DEGREES);
    _faceshiftEyeDeflection = loadSetting(settings, "faceshiftEyeDeflection", DEFAULT_FACESHIFT_EYE_DEFLECTION);
    _faceshiftHostname = settings->value("faceshiftHostname", DEFAULT_FACESHIFT_HOSTNAME).toString();
    _maxOctreePacketsPerSecond = loadSetting(settings, "maxOctreePPS", DEFAULT_MAX_OCTREE_PPS);
    _octreeSizeScale = loadSetting(settings, "octreeSizeScale", DEFAULT_OCTREE_SIZE_SCALE);
    _automaticAvatarLOD = settings->value("automaticAvatarLOD", true).toBool();
    _avatarLODDecreaseFPS = loadSetting(settings, "avatarLODDecreaseFPS", DEFAULT_ADJUST_AVATAR_LOD_DOWN_FPS);
    _avatarLODIncreaseFPS = loadSetting(settings, "avatarLODIncreaseFPS", ADJUST_LOD_UP_FPS);
    _avatarLODDistanceMultiplier = loadSetting(settings, "avatarLODDistanceMultiplier",
        DEFAULT_AVATAR_LOD_DISTANCE_MULTIPLIER);
    _boundaryLevelAdjust = loadSetting(settings, "boundaryLevelAdjust", 0);
    _snapshotsLocation = settings->value("snapshotsLocation",
                                         QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).toString();
    setScriptsLocation(settings->value("scriptsLocation", QString()).toString());

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    _speechRecognizer.setEnabled(settings->value("speechRecognitionEnabled", false).toBool());
#endif

    _walletPrivateKey = settings->value("privateKey").toByteArray();

    scanMenuBar(&loadAction, settings);
    Application::getInstance()->getAvatar()->loadData(settings);
    Application::getInstance()->updateWindowTitle();

    // notify that a settings has changed
    connect(&DependencyManager::get<NodeList>()->getDomainHandler(), &DomainHandler::hostnameChanged, this, &Menu::bumpSettings);

    // MyAvatar caches some menu options, so we have to update them whenever we load settings.
    // TODO: cache more settings in MyAvatar that are checked with very high frequency.
    setIsOptionChecked(MenuOption::KeyboardMotorControl , true);
    MyAvatar* myAvatar = Application::getInstance()->getAvatar();
    myAvatar->updateCollisionGroups();
    myAvatar->onToggleRagdoll();
    myAvatar->updateMotionBehavior();

    if (lockedSettings) {
        Application::getInstance()->unlockSettings();
    }
}

void Menu::saveSettings(QSettings* settings) {
    bool lockedSettings = false;
    if (!settings) {
        settings = Application::getInstance()->lockSettings();
        lockedSettings = true;
    }

    settings->setValue("dynamicJitterBuffers", _receivedAudioStreamSettings._dynamicJitterBuffers);
    settings->setValue("maxFramesOverDesired", _receivedAudioStreamSettings._maxFramesOverDesired);
    settings->setValue("staticDesiredJitterBufferFrames", _receivedAudioStreamSettings._staticDesiredJitterBufferFrames);
    settings->setValue("useStDevForJitterCalc", _receivedAudioStreamSettings._useStDevForJitterCalc);
    settings->setValue("windowStarveThreshold", _receivedAudioStreamSettings._windowStarveThreshold);
    settings->setValue("windowSecondsForDesiredCalcOnTooManyStarves", _receivedAudioStreamSettings._windowSecondsForDesiredCalcOnTooManyStarves);
    settings->setValue("windowSecondsForDesiredReduction", _receivedAudioStreamSettings._windowSecondsForDesiredReduction);
    settings->setValue("repetitionWithFade", _receivedAudioStreamSettings._repetitionWithFade);

    auto audio = DependencyManager::get<Audio>();
    settings->setValue("audioOutputStarveDetectionEnabled", audio->getOutputStarveDetectionEnabled());
    settings->setValue("audioOutputStarveDetectionThreshold", audio->getOutputStarveDetectionThreshold());
    settings->setValue("audioOutputStarveDetectionPeriod", audio->getOutputStarveDetectionPeriod());
    settings->setValue("audioOutputBufferSize", audio->getOutputBufferSize());

    settings->setValue("fieldOfView", _fieldOfView);
    settings->setValue("faceshiftEyeDeflection", _faceshiftEyeDeflection);
    settings->setValue("faceshiftHostname", _faceshiftHostname);
    settings->setValue("maxOctreePPS", _maxOctreePacketsPerSecond);
    settings->setValue("octreeSizeScale", _octreeSizeScale);
    settings->setValue("automaticAvatarLOD", _automaticAvatarLOD);
    settings->setValue("avatarLODDecreaseFPS", _avatarLODDecreaseFPS);
    settings->setValue("avatarLODIncreaseFPS", _avatarLODIncreaseFPS);
    settings->setValue("avatarLODDistanceMultiplier", _avatarLODDistanceMultiplier);
    settings->setValue("boundaryLevelAdjust", _boundaryLevelAdjust);
    settings->setValue("snapshotsLocation", _snapshotsLocation);
    settings->setValue("scriptsLocation", _scriptsLocation);
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    settings->setValue("speechRecognitionEnabled", _speechRecognizer.getEnabled());
#endif
    settings->setValue("privateKey", _walletPrivateKey);

    scanMenuBar(&saveAction, settings);
    Application::getInstance()->getAvatar()->saveData(settings);
    
    DependencyManager::get<AddressManager>()->storeCurrentAddress();

    if (lockedSettings) {
        Application::getInstance()->unlockSettings();
    }
}

void Menu::importSettings() {
    QString locationDir(QStandardPaths::displayName(QStandardPaths::DesktopLocation));
    QString fileName = QFileDialog::getOpenFileName(Application::getInstance()->getWindow(),
                                                    tr("Open .ini config file"),
                                                    locationDir,
                                                    tr("Text files (*.ini)"));
    if (fileName != "") {
        QSettings tmp(fileName, QSettings::IniFormat);
        loadSettings(&tmp);
    }
}

void Menu::exportSettings() {
    QString locationDir(QStandardPaths::displayName(QStandardPaths::DesktopLocation));
    QString fileName = QFileDialog::getSaveFileName(Application::getInstance()->getWindow(),
                                                    tr("Save .ini config file"),
                                                    locationDir,
                                                    tr("Text files (*.ini)"));
    if (fileName != "") {
        QSettings tmp(fileName, QSettings::IniFormat);
        saveSettings(&tmp);
        tmp.sync();
    }
}

void Menu::loadAction(QSettings* set, QAction* action) {
    if (action->isChecked() != set->value(action->text(), action->isChecked()).toBool()) {
        action->trigger();
    }
}

void Menu::saveAction(QSettings* set, QAction* action) {
    set->setValue(action->text(),  action->isChecked());
}

void Menu::scanMenuBar(settingsAction modifySetting, QSettings* set) {
    QList<QMenu*> menus = this->findChildren<QMenu *>();

    for (QList<QMenu *>::const_iterator it = menus.begin(); menus.end() != it; ++it) {
        scanMenu(*it, modifySetting, set);
    }
}

void Menu::scanMenu(QMenu* menu, settingsAction modifySetting, QSettings* set) {
    QList<QAction*> actions = menu->actions();

    set->beginGroup(menu->title());
    for (QList<QAction *>::const_iterator it = actions.begin(); actions.end() != it; ++it) {
        if ((*it)->menu()) {
            scanMenu((*it)->menu(), modifySetting, set);
        }
        if ((*it)->isCheckable()) {
            modifySetting(set, *it);
        }
    }
    set->endGroup();
}

bool Menu::getShadowsEnabled() const {
    return isOptionChecked(MenuOption::SimpleShadows) || isOptionChecked(MenuOption::CascadedShadows);
}

void Menu::addDisabledActionAndSeparator(QMenu* destinationMenu, const QString& actionName, int menuItemLocation) {
    QAction* actionBefore = NULL;
    if (menuItemLocation >= 0 && destinationMenu->actions().size() > menuItemLocation) {
        actionBefore = destinationMenu->actions()[menuItemLocation];
    }
    if (actionBefore) {
        QAction* separator = new QAction("",destinationMenu);
        destinationMenu->insertAction(actionBefore, separator);
        separator->setSeparator(true);

        QAction* separatorText = new QAction(actionName,destinationMenu);
        separatorText->setEnabled(false);
        destinationMenu->insertAction(actionBefore, separatorText);

    } else {
        destinationMenu->addSeparator();
        (destinationMenu->addAction(actionName))->setEnabled(false);
    }
}

QAction* Menu::addActionToQMenuAndActionHash(QMenu* destinationMenu,
                                             const QString& actionName,
                                             const QKeySequence& shortcut,
                                             const QObject* receiver,
                                             const char* member,
                                             QAction::MenuRole role,
                                             int menuItemLocation) {
    QAction* action = NULL;
    QAction* actionBefore = NULL;

    if (menuItemLocation >= 0 && destinationMenu->actions().size() > menuItemLocation) {
        actionBefore = destinationMenu->actions()[menuItemLocation];
    }

    if (!actionBefore) {
        if (receiver && member) {
            action = destinationMenu->addAction(actionName, receiver, member, shortcut);
        } else {
            action = destinationMenu->addAction(actionName);
            action->setShortcut(shortcut);
        }
    } else {
        action = new QAction(actionName, destinationMenu);
        action->setShortcut(shortcut);
        destinationMenu->insertAction(actionBefore, action);

        if (receiver && member) {
            connect(action, SIGNAL(triggered()), receiver, member);
        }
    }
    action->setMenuRole(role);

    _actionHash.insert(actionName, action);

    return action;
}

QAction* Menu::addActionToQMenuAndActionHash(QMenu* destinationMenu,
                                             QAction* action,
                                             const QString& actionName,
                                             const QKeySequence& shortcut,
                                             QAction::MenuRole role,
                                             int menuItemLocation) {
    QAction* actionBefore = NULL;

    if (menuItemLocation >= 0 && destinationMenu->actions().size() > menuItemLocation) {
        actionBefore = destinationMenu->actions()[menuItemLocation];
    }

    if (!actionName.isEmpty()) {
        action->setText(actionName);
    }

    if (shortcut != 0) {
        action->setShortcut(shortcut);
    }

    if (role != QAction::NoRole) {
        action->setMenuRole(role);
    }

    if (!actionBefore) {
        destinationMenu->addAction(action);
    } else {
        destinationMenu->insertAction(actionBefore, action);
    }

    _actionHash.insert(action->text(), action);

    return action;
}

QAction* Menu::addCheckableActionToQMenuAndActionHash(QMenu* destinationMenu,
                                                      const QString& actionName,
                                                      const QKeySequence& shortcut,
                                                      const bool checked,
                                                      const QObject* receiver,
                                                      const char* member,
                                                      int menuItemLocation) {

    QAction* action = addActionToQMenuAndActionHash(destinationMenu, actionName, shortcut, receiver, member,
                                                        QAction::NoRole, menuItemLocation);
    action->setCheckable(true);
    action->setChecked(checked);
    connect(action, SIGNAL(changed()), this, SLOT(bumpSettings()));

    return action;
}

void Menu::removeAction(QMenu* menu, const QString& actionName) {
    menu->removeAction(_actionHash.value(actionName));
    _actionHash.remove(actionName);
}

void Menu::setIsOptionChecked(const QString& menuOption, bool isChecked) {
    QAction* menu = _actionHash.value(menuOption);
    if (menu) {
        menu->setChecked(isChecked);
    }
}

bool Menu::isOptionChecked(const QString& menuOption) const {
    const QAction* menu = _actionHash.value(menuOption);
    if (menu) {
        return menu->isChecked();
    }
    return false;
}

void Menu::triggerOption(const QString& menuOption) {
    QAction* action = _actionHash.value(menuOption);
    if (action) {
        action->trigger();
    } else {
        qDebug() << "NULL Action for menuOption '" << menuOption << "'";
    }
}

QAction* Menu::getActionForOption(const QString& menuOption) {
    return _actionHash.value(menuOption);
}

void Menu::aboutApp() {
    InfoView::forcedShow(INFO_HELP_PATH);
}

void Menu::showEditEntitiesHelp() {
    InfoView::forcedShow(INFO_EDIT_ENTITIES_PATH);
}

void Menu::bumpSettings() {
    Application::getInstance()->bumpSettings();
}

void sendFakeEnterEvent() {
    QPoint lastCursorPosition = QCursor::pos();
    auto glCanvas = DependencyManager::get<GLCanvas>();

    QPoint windowPosition = glCanvas->mapFromGlobal(lastCursorPosition);
    QEnterEvent enterEvent = QEnterEvent(windowPosition, windowPosition, lastCursorPosition);
    QCoreApplication::sendEvent(glCanvas.data(), &enterEvent);
}

const float DIALOG_RATIO_OF_WINDOW = 0.30f;

void Menu::clearLoginDialogDisplayedFlag() {
    // Needed for domains that don't require login.
    _hasLoginDialogDisplayed = false;
}

void Menu::loginForCurrentDomain() {
    if (!_loginDialog && !_hasLoginDialogDisplayed) {
        _loginDialog = new LoginDialog(Application::getInstance()->getWindow());
        _loginDialog->show();
        _loginDialog->resizeAndPosition(false);
    }

    _hasLoginDialogDisplayed = true;
}

void Menu::showLoginForCurrentDomain() {
    _hasLoginDialogDisplayed = false;
    loginForCurrentDomain();
}

void Menu::editPreferences() {
    if (!_preferencesDialog) {
        _preferencesDialog = new PreferencesDialog();
        _preferencesDialog->show();
    } else {
        _preferencesDialog->close();
    }
}

void Menu::editAttachments() {
    if (!_attachmentsDialog) {
        _attachmentsDialog = new AttachmentsDialog();
        _attachmentsDialog->show();
    } else {
        _attachmentsDialog->close();
    }
}

void Menu::editAnimations() {
    if (!_animationsDialog) {
        _animationsDialog = new AnimationsDialog();
        _animationsDialog->show();
    } else {
        _animationsDialog->close();
    }
}

void Menu::toggleSixense(bool shouldEnable) {
    SixenseManager& sixenseManager = SixenseManager::getInstance();
    
    if (shouldEnable && !sixenseManager.isInitialized()) {
        sixenseManager.initialize();
        sixenseManager.setFilter(isOptionChecked(MenuOption::FilterSixense));
        sixenseManager.setLowVelocityFilter(isOptionChecked(MenuOption::LowVelocityFilter));
    }
    
    sixenseManager.setIsEnabled(shouldEnable);
}

void Menu::changePrivateKey() {
    // setup the dialog
    QInputDialog privateKeyDialog(Application::getInstance()->getWindow());
    privateKeyDialog.setWindowTitle("Change Private Key");
    privateKeyDialog.setLabelText("RSA 2048-bit Private Key:");
    privateKeyDialog.setWindowFlags(Qt::Sheet);
    privateKeyDialog.setTextValue(QString(_walletPrivateKey));
    privateKeyDialog.resize(privateKeyDialog.parentWidget()->size().width() * DIALOG_RATIO_OF_WINDOW,
                            privateKeyDialog.size().height());
    
    int dialogReturn = privateKeyDialog.exec();
    if (dialogReturn == QDialog::Accepted) {
        // pull the private key from the dialog
        _walletPrivateKey = privateKeyDialog.textValue().toUtf8();
    }

    bumpSettings();
    
    sendFakeEnterEvent();
}

void Menu::toggleAddressBar() {
    if (!_addressBarDialog) {
        _addressBarDialog = new AddressBarDialog();
    }
    
    if (!_addressBarDialog->isVisible()) {
        _addressBarDialog->show();
    }
}

void Menu::copyAddress() {
    auto addressManager = DependencyManager::get<AddressManager>();
    QString address = addressManager->currentAddress().toString();
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(address);
}

void Menu::copyPath() {
    auto addressManager = DependencyManager::get<AddressManager>();
    QString path = addressManager->currentPath();
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(path);
}

void Menu::changeVSync() {
    Application::getInstance()->setVSyncEnabled(isOptionChecked(MenuOption::RenderTargetFramerateVSyncOn));
}

void Menu::loadBookmarks() {
    QVariantMap* bookmarks = Application::getInstance()->getBookmarks()->getBookmarks();
    if (bookmarks->count() > 0) {

        QMapIterator<QString, QVariant> i(*bookmarks);
        while (i.hasNext()) {
            i.next();

            QString bookmarkName = i.key();
            QString bookmarkAddress = i.value().toString();

            QAction* teleportAction = new QAction(getMenu(MenuOption::Bookmarks));
            teleportAction->setData(bookmarkAddress);
            connect(teleportAction, SIGNAL(triggered()), this, SLOT(teleportToBookmark()));

            addActionToQMenuAndActionHash(_bookmarksMenu, teleportAction, bookmarkName, 0, QAction::NoRole);
        }

        _bookmarksMenu->setEnabled(true);
        _deleteBookmarksMenu->setEnabled(true);
    }
}

void Menu::bookmarkLocation() {

    QInputDialog bookmarkLocationDialog(Application::getInstance()->getWindow());
    bookmarkLocationDialog.setWindowTitle("Bookmark Location");
    bookmarkLocationDialog.setLabelText("Name:");
    bookmarkLocationDialog.setInputMode(QInputDialog::TextInput);
    bookmarkLocationDialog.resize(400, 200);

    if (bookmarkLocationDialog.exec() == QDialog::Rejected) {
        return;
    }

    QString bookmarkName = bookmarkLocationDialog.textValue().trimmed();
    bookmarkName = bookmarkName.replace(QRegExp("(\r\n|[\r\n\t\v ])+"), " ");
    if (bookmarkName.length() == 0) {
        return;
    }

    auto addressManager = DependencyManager::get<AddressManager>();
    QString bookmarkAddress = addressManager->currentAddress().toString();

    Bookmarks* bookmarks = Application::getInstance()->getBookmarks();
    if (bookmarks->contains(bookmarkName)) {
        QMessageBox duplicateBookmarkMessage;
        duplicateBookmarkMessage.setIcon(QMessageBox::Warning);
        duplicateBookmarkMessage.setText("The bookmark name you entered already exists in your list.");
        duplicateBookmarkMessage.setInformativeText("Would you like to overwrite it?");
        duplicateBookmarkMessage.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        duplicateBookmarkMessage.setDefaultButton(QMessageBox::Yes);
        if (duplicateBookmarkMessage.exec() == QMessageBox::No) {
            return;
        }
        removeAction(_bookmarksMenu, bookmarkName);
    }
    
    QAction* teleportAction = new QAction(getMenu(MenuOption::Bookmarks));
    teleportAction->setData(bookmarkAddress);
    connect(teleportAction, SIGNAL(triggered()), this, SLOT(teleportToBookmark()));

    QList<QAction*> menuItems = _bookmarksMenu->actions();
    int position = 0;
    while (position < menuItems.count() && bookmarkName > menuItems[position]->text()) {
        position += 1;
    }

    addActionToQMenuAndActionHash(_bookmarksMenu, teleportAction, bookmarkName, 0,
                                  QAction::NoRole, position);

    bookmarks->insert(bookmarkName, bookmarkAddress);  // Overwrites any item with the same bookmarkName.

    _bookmarksMenu->setEnabled(true);
    _deleteBookmarksMenu->setEnabled(true);
}

void Menu::teleportToBookmark() {
    QAction *action = qobject_cast<QAction *>(sender());
    QString address = action->data().toString();
    DependencyManager::get<AddressManager>()->handleLookupString(address);
}

void Menu::deleteBookmark() {

    QStringList bookmarkList;
    QList<QAction*> menuItems = _bookmarksMenu->actions();
    for (int i = 0; i < menuItems.count(); i += 1) {
        bookmarkList.append(menuItems[i]->text());
    }

    QInputDialog deleteBookmarkDialog(Application::getInstance()->getWindow());
    deleteBookmarkDialog.setWindowTitle("Delete Bookmark");
    deleteBookmarkDialog.setLabelText("Select the bookmark to delete");
    deleteBookmarkDialog.resize(400, 400);
    deleteBookmarkDialog.setOption(QInputDialog::UseListViewForComboBoxItems);
    deleteBookmarkDialog.setComboBoxItems(bookmarkList);
    deleteBookmarkDialog.setOkButtonText("Delete");

    if (deleteBookmarkDialog.exec() == QDialog::Rejected) {
        return;
    }

    QString bookmarkName = deleteBookmarkDialog.textValue().trimmed();
    if (bookmarkName.length() == 0) {
        return;
    }

    removeAction(_bookmarksMenu, bookmarkName);

    Bookmarks* bookmarks = Application::getInstance()->getBookmarks();
    bookmarks->remove(bookmarkName);

    if (_bookmarksMenu->actions().count() == 0) {
        _bookmarksMenu->setEnabled(false);
        _deleteBookmarksMenu->setEnabled(false);
    }
}

void Menu::displayNameLocationResponse(const QString& errorString) {

    if (!errorString.isEmpty()) {
        QMessageBox msgBox;
        msgBox.setText(errorString);
        msgBox.exec();
    }    
}

void Menu::toggleLoginMenuItem() {
    AccountManager& accountManager = AccountManager::getInstance();

    disconnect(_loginAction, 0, 0, 0);

    if (accountManager.isLoggedIn()) {
        // change the menu item to logout
        _loginAction->setText("Logout " + accountManager.getAccountInfo().getUsername());
        connect(_loginAction, &QAction::triggered, &accountManager, &AccountManager::logout);
    } else {
        // change the menu item to login
        _loginAction->setText("Login");

        connect(_loginAction, &QAction::triggered, this, &Menu::showLoginForCurrentDomain);
    }
}

void Menu::bandwidthDetails() {
    if (! _bandwidthDialog) {
        _bandwidthDialog = new BandwidthDialog(DependencyManager::get<GLCanvas>().data(),
                                               Application::getInstance()->getBandwidthMeter());
        connect(_bandwidthDialog, SIGNAL(closed()), _bandwidthDialog, SLOT(deleteLater()));

        _bandwidthDialog->show();
        
        if (_hmdToolsDialog) {
            _hmdToolsDialog->watchWindow(_bandwidthDialog->windowHandle());
        }
    }
    _bandwidthDialog->raise();
}

void Menu::showMetavoxelEditor() {
    if (!_MetavoxelEditor) {
        _MetavoxelEditor = new MetavoxelEditor();
    }
    _MetavoxelEditor->raise();
}

void Menu::showMetavoxelNetworkSimulator() {
    if (!_metavoxelNetworkSimulator) {
        _metavoxelNetworkSimulator = new MetavoxelNetworkSimulator();
    }
    _metavoxelNetworkSimulator->raise();
}

void Menu::showScriptEditor() {
    if(!_ScriptEditor || !_ScriptEditor->isVisible()) {
        _ScriptEditor = new ScriptEditorWindow();
    }
    _ScriptEditor->raise();
}

void Menu::showChat() {
    if (AccountManager::getInstance().isLoggedIn()) {
        QMainWindow* mainWindow = Application::getInstance()->getWindow();
        if (!_chatWindow) {
            _chatWindow = new ChatWindow(mainWindow);
        }

        if (_chatWindow->isHidden()) {
            _chatWindow->show();
        }
        _chatWindow->raise();
        _chatWindow->activateWindow();
        _chatWindow->setFocus();
    } else {
        Application::getInstance()->getTrayIcon()->showMessage("Interface", "You need to login to be able to chat with others on this domain.");
    }
}

void Menu::loadRSSDKFile() {
    RealSense::getInstance()->loadRSSDKFile();
}

void Menu::toggleChat() {
#ifdef HAVE_QXMPP
    _chatAction->setEnabled(XmppClient::getInstance().getXMPPClient().isConnected());
    if (!_chatAction->isEnabled() && _chatWindow && AccountManager::getInstance().isLoggedIn()) {
        if (_chatWindow->isHidden()) {
            _chatWindow->show();
            _chatWindow->raise();
            _chatWindow->activateWindow();
            _chatWindow->setFocus();
        } else {
            _chatWindow->hide();
        }
    }
#endif
}

void Menu::toggleConsole() {
    QMainWindow* mainWindow = Application::getInstance()->getWindow();
    if (!_jsConsole) {
        QDialog* dialog = new QDialog(mainWindow, Qt::WindowStaysOnTopHint);
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        dialog->setLayout(new QVBoxLayout(dialog));

        dialog->resize(QSize(CONSOLE_WIDTH, CONSOLE_HEIGHT));
        layout->setMargin(0);
        layout->setSpacing(0);
        layout->addWidget(new JSConsole(dialog));
        dialog->setWindowOpacity(CONSOLE_WINDOW_OPACITY);
        dialog->setWindowTitle(CONSOLE_TITLE);

        _jsConsole = dialog;
    }
    _jsConsole->setVisible(!_jsConsole->isVisible());
}

void Menu::toggleToolWindow() {
    QMainWindow* toolWindow = Application::getInstance()->getToolWindow();
    toolWindow->setVisible(!toolWindow->isVisible());
    if (_hmdToolsDialog) {
        _hmdToolsDialog->watchWindow(toolWindow->windowHandle());
    }
}

void Menu::audioMuteToggled() {
    QAction *muteAction = _actionHash.value(MenuOption::MuteAudio);
    if (muteAction) {
        muteAction->setChecked(DependencyManager::get<Audio>()->isMuted());
    }
}

void Menu::octreeStatsDetails() {
    if (!_octreeStatsDialog) {
        _octreeStatsDialog = new OctreeStatsDialog(DependencyManager::get<GLCanvas>().data(),
                                                 Application::getInstance()->getOcteeSceneStats());
        connect(_octreeStatsDialog, SIGNAL(closed()), _octreeStatsDialog, SLOT(deleteLater()));
        _octreeStatsDialog->show();
        if (_hmdToolsDialog) {
            _hmdToolsDialog->watchWindow(_octreeStatsDialog->windowHandle());
        }
    }
    _octreeStatsDialog->raise();
}

QString Menu::getLODFeedbackText() {
    // determine granularity feedback
    int boundaryLevelAdjust = getBoundaryLevelAdjust();
    QString granularityFeedback;

    switch (boundaryLevelAdjust) {
        case 0: {
            granularityFeedback = QString("at standard granularity.");
        } break;
        case 1: {
            granularityFeedback = QString("at half of standard granularity.");
        } break;
        case 2: {
            granularityFeedback = QString("at a third of standard granularity.");
        } break;
        default: {
            granularityFeedback = QString("at 1/%1th of standard granularity.").arg(boundaryLevelAdjust + 1);
        } break;
    }

    // distance feedback
    float octreeSizeScale = getOctreeSizeScale();
    float relativeToDefault = octreeSizeScale / DEFAULT_OCTREE_SIZE_SCALE;
    QString result;
    if (relativeToDefault > 1.01) {
        result = QString("%1 further %2").arg(relativeToDefault,8,'f',2).arg(granularityFeedback);
    } else if (relativeToDefault > 0.99) {
            result = QString("the default distance %1").arg(granularityFeedback);
    } else {
        result = QString("%1 of default %2").arg(relativeToDefault,8,'f',3).arg(granularityFeedback);
    }
    return result;
}

void Menu::autoAdjustLOD(float currentFPS) {
    // NOTE: our first ~100 samples at app startup are completely all over the place, and we don't
    // really want to count them in our average, so we will ignore the real frame rates and stuff
    // our moving average with simulated good data
    const int IGNORE_THESE_SAMPLES = 100;
    const float ASSUMED_FPS = 60.0f;
    if (_fpsAverage.getSampleCount() < IGNORE_THESE_SAMPLES) {
        currentFPS = ASSUMED_FPS;
    }
    _fpsAverage.updateAverage(currentFPS);
    _fastFPSAverage.updateAverage(currentFPS);

    quint64 now = usecTimestampNow();

    const quint64 ADJUST_AVATAR_LOD_DOWN_DELAY = 1000 * 1000;
    if (_automaticAvatarLOD) {
        if (_fastFPSAverage.getAverage() < _avatarLODDecreaseFPS) {
            if (now - _lastAvatarDetailDrop > ADJUST_AVATAR_LOD_DOWN_DELAY) {
                // attempt to lower the detail in proportion to the fps difference
                float targetFps = (_avatarLODDecreaseFPS + _avatarLODIncreaseFPS) * 0.5f;
                float averageFps = _fastFPSAverage.getAverage();
                const float MAXIMUM_MULTIPLIER_SCALE = 2.0f;
                _avatarLODDistanceMultiplier = qMin(MAXIMUM_AVATAR_LOD_DISTANCE_MULTIPLIER, _avatarLODDistanceMultiplier *
                    (averageFps < EPSILON ? MAXIMUM_MULTIPLIER_SCALE :
                        qMin(MAXIMUM_MULTIPLIER_SCALE, targetFps / averageFps)));
                _lastAvatarDetailDrop = now;
            }
        } else if (_fastFPSAverage.getAverage() > _avatarLODIncreaseFPS) {
            // let the detail level creep slowly upwards
            const float DISTANCE_DECREASE_RATE = 0.05f;
            _avatarLODDistanceMultiplier = qMax(MINIMUM_AVATAR_LOD_DISTANCE_MULTIPLIER,
                _avatarLODDistanceMultiplier - DISTANCE_DECREASE_RATE);
        }
    }

    bool changed = false;
    quint64 elapsed = now - _lastAdjust;

    if (elapsed > ADJUST_LOD_DOWN_DELAY && _fpsAverage.getAverage() < ADJUST_LOD_DOWN_FPS
            && _octreeSizeScale > ADJUST_LOD_MIN_SIZE_SCALE) {

        _octreeSizeScale *= ADJUST_LOD_DOWN_BY;

        if (_octreeSizeScale < ADJUST_LOD_MIN_SIZE_SCALE) {
            _octreeSizeScale = ADJUST_LOD_MIN_SIZE_SCALE;
        }
        changed = true;
        _lastAdjust = now;
        qDebug() << "adjusting LOD down... average fps for last approximately 5 seconds=" << _fpsAverage.getAverage()
                     << "_octreeSizeScale=" << _octreeSizeScale;
    }

    if (elapsed > ADJUST_LOD_UP_DELAY && _fpsAverage.getAverage() > ADJUST_LOD_UP_FPS
            && _octreeSizeScale < ADJUST_LOD_MAX_SIZE_SCALE) {
        _octreeSizeScale *= ADJUST_LOD_UP_BY;
        if (_octreeSizeScale > ADJUST_LOD_MAX_SIZE_SCALE) {
            _octreeSizeScale = ADJUST_LOD_MAX_SIZE_SCALE;
        }
        changed = true;
        _lastAdjust = now;
        qDebug() << "adjusting LOD up... average fps for last approximately 5 seconds=" << _fpsAverage.getAverage()
                     << "_octreeSizeScale=" << _octreeSizeScale;
    }

    if (changed) {
        _shouldRenderTableNeedsRebuilding = true;
        if (_lodToolsDialog) {
            _lodToolsDialog->reloadSliders();
        }
    }
}

void Menu::resetLODAdjust() {
    _fpsAverage.reset();
    _fastFPSAverage.reset();
    _lastAvatarDetailDrop = _lastAdjust = usecTimestampNow();
}

void Menu::setOctreeSizeScale(float sizeScale) {
    _octreeSizeScale = sizeScale;
    _shouldRenderTableNeedsRebuilding = true;
    bumpSettings();
}

void Menu::setBoundaryLevelAdjust(int boundaryLevelAdjust) {
    _boundaryLevelAdjust = boundaryLevelAdjust;
    _shouldRenderTableNeedsRebuilding = true;
    bumpSettings();
}

// TODO: This is essentially the same logic used to render octree cells, but since models are more detailed then octree cells
//       I've added a voxelToModelRatio that adjusts how much closer to a model you have to be to see it.
bool Menu::shouldRenderMesh(float largestDimension, float distanceToCamera) {
    const float octreeToMeshRatio = 4.0f; // must be this many times closer to a mesh than a voxel to see it.
    float octreeSizeScale = getOctreeSizeScale();
    int boundaryLevelAdjust = getBoundaryLevelAdjust();
    float maxScale = (float)TREE_SCALE;
    float visibleDistanceAtMaxScale = boundaryDistanceForRenderLevel(boundaryLevelAdjust, octreeSizeScale) / octreeToMeshRatio;
    
    if (_shouldRenderTableNeedsRebuilding) {
        _shouldRenderTable.clear();

        float SMALLEST_SCALE_IN_TABLE = 0.001f; // 1mm is plenty small
        float scale = maxScale;
        float visibleDistanceAtScale = visibleDistanceAtMaxScale;

        while (scale > SMALLEST_SCALE_IN_TABLE) {
            scale /= 2.0f;
            visibleDistanceAtScale /= 2.0f;
            _shouldRenderTable[scale] = visibleDistanceAtScale;
        }
        _shouldRenderTableNeedsRebuilding = false;
    }

    float closestScale = maxScale;
    float visibleDistanceAtClosestScale = visibleDistanceAtMaxScale;
    QMap<float, float>::const_iterator lowerBound = _shouldRenderTable.lowerBound(largestDimension);
    if (lowerBound != _shouldRenderTable.constEnd()) {
        closestScale = lowerBound.key();
        visibleDistanceAtClosestScale = lowerBound.value();
    }
    
    if (closestScale < largestDimension) {
        visibleDistanceAtClosestScale *= 2.0f;
    }

    return (distanceToCamera <= visibleDistanceAtClosestScale);
}

void Menu::cachesSizeDialog() {
    qDebug() << "Caches size:" << _cachesSizeDialog.isNull();
    if (!_cachesSizeDialog) {
        _cachesSizeDialog = new CachesSizeDialog(DependencyManager::get<GLCanvas>().data());
        connect(_cachesSizeDialog, SIGNAL(closed()), _cachesSizeDialog, SLOT(deleteLater()));
        _cachesSizeDialog->show();
        if (_hmdToolsDialog) {
            _hmdToolsDialog->watchWindow(_cachesSizeDialog->windowHandle());
        }
    }
    _cachesSizeDialog->raise();
}

void Menu::lodTools() {
    if (!_lodToolsDialog) {
        _lodToolsDialog = new LodToolsDialog(DependencyManager::get<GLCanvas>().data());
        connect(_lodToolsDialog, SIGNAL(closed()), _lodToolsDialog, SLOT(deleteLater()));
        _lodToolsDialog->show();
        if (_hmdToolsDialog) {
            _hmdToolsDialog->watchWindow(_lodToolsDialog->windowHandle());
        }
    }
    _lodToolsDialog->raise();
}

void Menu::hmdTools(bool showTools) {
    if (showTools) {
        if (!_hmdToolsDialog) {
            _hmdToolsDialog = new HMDToolsDialog(DependencyManager::get<GLCanvas>().data());
            connect(_hmdToolsDialog, SIGNAL(closed()), SLOT(hmdToolsClosed()));
        }
        _hmdToolsDialog->show();
        _hmdToolsDialog->raise();
    } else {
        hmdToolsClosed();
    }
    Application::getInstance()->getWindow()->activateWindow();
}

void Menu::hmdToolsClosed() {
    Menu::getInstance()->getActionForOption(MenuOption::HMDTools)->setChecked(false);
    _hmdToolsDialog->hide();
}

void Menu::runTests() {
    runTimingTests();
}

QAction* Menu::getActionFromName(const QString& menuName, QMenu* menu) {
    QList<QAction*> menuActions;
    if (menu) {
        menuActions = menu->actions();
    } else {
        menuActions = actions();
    }

    foreach (QAction* menuAction, menuActions) {
        if (menuName == menuAction->text()) {
            return menuAction;
        }
    }
    return NULL;
}

QMenu* Menu::getSubMenuFromName(const QString& menuName, QMenu* menu) {
    QAction* action = getActionFromName(menuName, menu);
    if (action) {
        return action->menu();
    }
    return NULL;
}

QMenu* Menu::getMenuParent(const QString& menuName, QString& finalMenuPart) {
    QStringList menuTree = menuName.split(">");
    QMenu* parent = NULL;
    QMenu* menu = NULL;
    foreach (QString menuTreePart, menuTree) {
        parent = menu;
        finalMenuPart = menuTreePart.trimmed();
        menu = getSubMenuFromName(finalMenuPart, parent);
        if (!menu) {
            break;
        }
    }
    return parent;
}

QMenu* Menu::getMenu(const QString& menuName) {
    QStringList menuTree = menuName.split(">");
    QMenu* parent = NULL;
    QMenu* menu = NULL;
    int item = 0;
    foreach (QString menuTreePart, menuTree) {
        menu = getSubMenuFromName(menuTreePart.trimmed(), parent);
        if (!menu) {
            break;
        }
        parent = menu;
        item++;
    }
    return menu;
}

QAction* Menu::getMenuAction(const QString& menuName) {
    QStringList menuTree = menuName.split(">");
    QMenu* parent = NULL;
    QAction* action = NULL;
    foreach (QString menuTreePart, menuTree) {
        action = getActionFromName(menuTreePart.trimmed(), parent);
        if (!action) {
            break;
        }
        parent = action->menu();
    }
    return action;
}

int Menu::findPositionOfMenuItem(QMenu* menu, const QString& searchMenuItem) {
    int position = 0;
    foreach(QAction* action, menu->actions()) {
        if (action->text() == searchMenuItem) {
            return position;
        }
        position++;
    }
    return UNSPECIFIED_POSITION; // not found
}

int Menu::positionBeforeSeparatorIfNeeded(QMenu* menu, int requestedPosition) {
    QList<QAction*> menuActions = menu->actions();
    if (requestedPosition > 1 && requestedPosition < menuActions.size()) {
        QAction* beforeRequested = menuActions[requestedPosition - 1];
        if (beforeRequested->isSeparator()) {
            requestedPosition--;
        }
    }
    return requestedPosition;
}


QMenu* Menu::addMenu(const QString& menuName) {
    QStringList menuTree = menuName.split(">");
    QMenu* addTo = NULL;
    QMenu* menu = NULL;
    foreach (QString menuTreePart, menuTree) {
        menu = getSubMenuFromName(menuTreePart.trimmed(), addTo);
        if (!menu) {
            if (!addTo) {
                menu = QMenuBar::addMenu(menuTreePart.trimmed());
            } else {
                menu = addTo->addMenu(menuTreePart.trimmed());
            }
        }
        addTo = menu;
    }

    QMenuBar::repaint();
    return menu;
}

void Menu::removeMenu(const QString& menuName) {
    QAction* action = getMenuAction(menuName);

    // only proceed if the menu actually exists
    if (action) {
        QString finalMenuPart;
        QMenu* parent = getMenuParent(menuName, finalMenuPart);
        if (parent) {
            parent->removeAction(action);
        } else {
            QMenuBar::removeAction(action);
        }

        QMenuBar::repaint();
    }
}

bool Menu::menuExists(const QString& menuName) {
    QAction* action = getMenuAction(menuName);

    // only proceed if the menu actually exists
    if (action) {
        return true;
    }
    return false;
}

void Menu::addSeparator(const QString& menuName, const QString& separatorName) {
    QMenu* menuObj = getMenu(menuName);
    if (menuObj) {
        addDisabledActionAndSeparator(menuObj, separatorName);
    }
}

void Menu::removeSeparator(const QString& menuName, const QString& separatorName) {
    QMenu* menu = getMenu(menuName);
    bool separatorRemoved = false;
    if (menu) {
        int textAt = findPositionOfMenuItem(menu, separatorName);
        QList<QAction*> menuActions = menu->actions();
        QAction* separatorText = menuActions[textAt];
        if (textAt > 0 && textAt < menuActions.size()) {
            QAction* separatorLine = menuActions[textAt - 1];
            if (separatorLine) {
                if (separatorLine->isSeparator()) {
                    menu->removeAction(separatorText);
                    menu->removeAction(separatorLine);
                    separatorRemoved = true;
                }
            }
        }
    }
    if (separatorRemoved) {
        QMenuBar::repaint();
    }
}

void Menu::addMenuItem(const MenuItemProperties& properties) {
    QMenu* menuObj = getMenu(properties.menuName);
    if (menuObj) {
        QShortcut* shortcut = NULL;
        if (!properties.shortcutKeySequence.isEmpty()) {
            shortcut = new QShortcut(properties.shortcutKeySequence, this);
        }

        // check for positioning requests
        int requestedPosition = properties.position;
        if (requestedPosition == UNSPECIFIED_POSITION && !properties.beforeItem.isEmpty()) {
            requestedPosition = findPositionOfMenuItem(menuObj, properties.beforeItem);
            // double check that the requested location wasn't a separator label
            requestedPosition = positionBeforeSeparatorIfNeeded(menuObj, requestedPosition);
        }
        if (requestedPosition == UNSPECIFIED_POSITION && !properties.afterItem.isEmpty()) {
            int afterPosition = findPositionOfMenuItem(menuObj, properties.afterItem);
            if (afterPosition != UNSPECIFIED_POSITION) {
                requestedPosition = afterPosition + 1;
            }
        }

        QAction* menuItemAction = NULL;
        if (properties.isSeparator) {
            addDisabledActionAndSeparator(menuObj, properties.menuItemName, requestedPosition);
        } else if (properties.isCheckable) {
            menuItemAction = addCheckableActionToQMenuAndActionHash(menuObj, properties.menuItemName,
                                    properties.shortcutKeySequence, properties.isChecked,
                                    MenuScriptingInterface::getInstance(), SLOT(menuItemTriggered()), requestedPosition);
        } else {
            menuItemAction = addActionToQMenuAndActionHash(menuObj, properties.menuItemName, properties.shortcutKeySequence,
                                    MenuScriptingInterface::getInstance(), SLOT(menuItemTriggered()),
                                    QAction::NoRole, requestedPosition);
        }
        if (shortcut && menuItemAction) {
            connect(shortcut, SIGNAL(activated()), menuItemAction, SLOT(trigger()));
        }
        QMenuBar::repaint();
    }
}

void Menu::removeMenuItem(const QString& menu, const QString& menuitem) {
    QMenu* menuObj = getMenu(menu);
    if (menuObj) {
        removeAction(menuObj, menuitem);
        QMenuBar::repaint();
    }
};

bool Menu::menuItemExists(const QString& menu, const QString& menuitem) {
    QAction* menuItemAction = _actionHash.value(menuitem);
    if (menuItemAction) {
        return (getMenu(menu) != NULL);
    }
    return false;
};

QString Menu::getSnapshotsLocation() const {
    if (_snapshotsLocation.isNull() || _snapshotsLocation.isEmpty() || QDir(_snapshotsLocation).exists() == false) {
        return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }
    return _snapshotsLocation;
}

void Menu::setScriptsLocation(const QString& scriptsLocation) {
    _scriptsLocation = scriptsLocation;
    bumpSettings();
    emit scriptLocationChanged(scriptsLocation);
}
