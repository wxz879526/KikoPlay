#include "player.h"
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QCheckBox>
#include <QFontComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QFileDialog>
#include <QActionGroup>
#include <QApplication>
#include <QToolTip>
#include <QMenu>
#include <QGraphicsOpacityEffect>
#include <QButtonGroup>

#include "widgets/clickslider.h"
#include "capture.h"
#include "mediainfo.h"
#include "mpvparametersetting.h"
#include "mpvlog.h"
#include "Play/Playlist/playlist.h"
#include "Play/Danmu/Render/danmurender.h"
#include "Play/Danmu/Provider/localprovider.h"
#include "Play/Danmu/danmupool.h"
#include "Play/Danmu/Manager/pool.h"
#include "Play/Danmu/blocker.h"
#include "MediaLibrary/animelibrary.h"
#include "globalobjects.h"
namespace
{
class InfoTip : public QWidget
{
public:
    explicit InfoTip(QWidget *parent=nullptr):QWidget(parent)
    {
        setObjectName(QStringLiteral("PlayInfoBar"));
        infoText=new QLabel(this);
        infoText->setObjectName(QStringLiteral("labelPlayInfo"));
        infoText->setFont(QFont("Microsoft YaHei UI",10));
        infoText->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
        QHBoxLayout *infoBarHLayout=new QHBoxLayout(this);
        infoBarHLayout->addWidget(infoText);
        QObject::connect(&hideTimer,&QTimer::timeout,this,&InfoTip::hide);
    }
    void showMessage(const QString &msg)
    {
        if(hideTimer.isActive())
            hideTimer.stop();
        hideTimer.setSingleShot(true);
        hideTimer.start(2500);
        infoText->setText(msg);
        infoText->adjustSize();
        adjustSize();
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(eff);
        eff->setOpacity(0);
        show();
        QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
        a->setDuration(400);
        a->setStartValue(0);
        a->setEndValue(1);
        a->setEasingCurve(QEasingCurve::Linear);
        a->start(QPropertyAnimation::DeleteWhenStopped);
    }
private:
    QLabel *infoText;
    QTimer hideTimer;
};
class RecentItem : public QWidget
{
public:
    explicit RecentItem(QWidget *parent=nullptr):QWidget(parent)
    {
        setObjectName(QStringLiteral("RecentItem"));
        titleLabel=new QLabel(this);
        titleLabel->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
        titleLabel->setObjectName(QStringLiteral("RecentItemLabel"));
        deleteItem=new QPushButton(this);
        deleteItem->setObjectName(QStringLiteral("RecentItemDeleteButton"));
        QObject::connect(deleteItem,&QPushButton::clicked,[this](){
            GlobalObjects::playlist->recent().removeAt(index);
            hide();
        });
        GlobalObjects::iconfont.setPointSize(10);
        deleteItem->setFont(GlobalObjects::iconfont);
        deleteItem->setText(QChar(0xe60b));
        deleteItem->hide();
        QHBoxLayout *itemHLayout=new QHBoxLayout(this);
        itemHLayout->addWidget(titleLabel);
        itemHLayout->addWidget(deleteItem);
        setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
    }
    void setData(QPair<QString,QString> &pair,int index)
    {
        this->index=index;
        path=pair.first;
        titleLabel->setText(pair.second);
        titleLabel->adjustSize();
        adjustSize();
        show();
    }
private:
    QLabel *titleLabel;
    QPushButton *deleteItem;
    int index;
    QString path;
protected:
    virtual void mousePressEvent(QMouseEvent *event)
    {
        if(event->button()==Qt::LeftButton)
        {
            const PlayListItem *curItem = GlobalObjects::playlist->setCurrentItem(path);
            if (curItem)
            {
                GlobalObjects::danmuPool->reset();
                GlobalObjects::danmuRender->cleanup();
                GlobalObjects::mpvplayer->setMedia(curItem->path);
            }
        }
        QWidget::mousePressEvent(event);
    }
    virtual void enterEvent(QEvent *event)
    {
        deleteItem->show();
        QWidget::enterEvent(event);
    }
    virtual void leaveEvent(QEvent *event)
    {
        deleteItem->hide();
        QWidget::leaveEvent(event);
    }
};
class PlayerContent : public QWidget
{
public:
    explicit PlayerContent(QWidget *parent=nullptr):QWidget(parent)
    {
        setObjectName(QStringLiteral("PlayerContent"));
        QLabel *logo=new QLabel(this);
        logo->setPixmap(QPixmap(":/res/images/kikoplay-5.png"));
        logo->setAlignment(Qt::AlignCenter);
        QVBoxLayout *pcVLayout=new QVBoxLayout(this);
		pcVLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
        pcVLayout->addWidget(logo);
        for(int i=0;i<GlobalObjects::playlist->maxRecentItems;++i)
        {
            RecentItem *recentItem=new RecentItem(this);
            pcVLayout->addWidget(recentItem);
            items.append(recentItem);
        }
		pcVLayout->addSpacing(20);
		pcVLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
        refreshItems();
        resize(400*logicalDpiX()/96, 400*logicalDpiY()/96);
    }
    void refreshItems()
    {
        int i=0;
        const int maxRecentCount=GlobalObjects::playlist->maxRecentItems;
        auto &recent=GlobalObjects::playlist->recent();
        for(;i<maxRecentCount && i<recent.count();++i)
            items[i]->setData(recent[i],i);
        while(i<maxRecentCount)
            items[i++]->hide();
    }
private:
    QList<RecentItem *> items;
};
class DanmuStatisInfo : public QWidget
{
public:
    explicit DanmuStatisInfo(QWidget *parent=nullptr):QWidget(parent),duration(0)
    {
        QObject::connect(GlobalObjects::danmuPool,&DanmuPool::statisInfoChange,this,(void (DanmuStatisInfo:: *)())&DanmuStatisInfo::update);
        setObjectName(QStringLiteral("DanmuStatisBar"));
    }
    int duration;
protected:
    virtual void paintEvent(QPaintEvent *event)
    {
        static QColor bgColor(0,0,0,150),barColor(51,168,255,200),penColor(255,255,255);
        static QRect bRect;
        bRect=rect();
        QPainter painter(this);
        painter.fillRect(bRect,bgColor);
        if(duration==0)return;
        bRect.adjust(1, 0, -1, 0);
        auto &statisInfo=GlobalObjects::danmuPool->getStatisInfo();
        float hRatio=(float)bRect.height()/statisInfo.maxCountOfMinute;
        float margin=8*logicalDpiX()/96;
        float wRatio=(float)(bRect.width()-margin*2)/duration;
        float bHeight=bRect.height();
        for(auto iter=statisInfo.countOfMinute.cbegin();iter!=statisInfo.countOfMinute.cend();++iter)
        {
            float l((*iter).first*wRatio);
            float h(floor((*iter).second*hRatio));
            painter.fillRect(l+margin,bHeight-h,wRatio<1.f?1.f:wRatio,h,barColor);
        }
        painter.setPen(penColor);
        painter.drawText(bRect,Qt::AlignLeft|Qt::AlignTop,QObject::tr("Total:%1 Max:%2 Block:%3 Merge:%4")
                         .arg(QString::number(statisInfo.totalCount),
                              QString::number(statisInfo.maxCountOfMinute),
                              QString::number(statisInfo.blockCount),
                              QString::number(statisInfo.mergeCount)));
    }
};
}
PlayerWindow::PlayerWindow(QWidget *parent) : QMainWindow(parent),autoHideControlPanel(true),
    onTopWhilePlaying(false),updatingTrack(false),isFullscreen(false),resizePercent(-1),jumpForwardTime(5),jumpBackwardTime(5)
{
    setWindowFlags(Qt::FramelessWindowHint);
    GlobalObjects::mpvplayer->setParent(this);
    setCentralWidget(GlobalObjects::mpvplayer);
    GlobalObjects::mpvplayer->setMouseTracking(true);
    setContentsMargins(0,0,0,0);

    logDialog=new MPVLog(this);

    playInfo=new InfoTip(this);
    playInfo->hide();

    timeInfoTip=new QLabel(this);
    timeInfoTip->setObjectName(QStringLiteral("TimeInfoTip"));
    timeInfoTip->hide();

    playerContent=new PlayerContent(this);
    playerContent->show();
    playerContent->raise();

    playInfoPanel=new QWidget(this);
    playInfoPanel->setObjectName(QStringLiteral("widgetPlayInfo"));
    playInfoPanel->hide();

    playListCollapseButton=new QPushButton(this);
    playListCollapseButton->setObjectName(QStringLiteral("widgetPlayListCollapse"));
    GlobalObjects::iconfont.setPointSize(12);
    playListCollapseButton->setFont(GlobalObjects::iconfont);
    playListCollapseButton->setText(QChar(GlobalObjects::appSetting->value("MainWindow/ListVisibility",true).toBool()?0xe945:0xe946));
    playListCollapseButton->hide();

    danmuStatisBar=new DanmuStatisInfo(this);
    danmuStatisBar->hide();

    QFont normalFont;
    normalFont.setFamily("Microsoft YaHei");
    normalFont.setPointSize(12);

    titleLabel=new QLabel(playInfoPanel);
    titleLabel->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Minimum);
    titleLabel->setObjectName(QStringLiteral("labelTitle"));
    titleLabel->setFont(normalFont);

    GlobalObjects::iconfont.setPointSize(12);

    mediaInfo=new QToolButton(playInfoPanel);
    mediaInfo->setFont(GlobalObjects::iconfont);
    mediaInfo->setText(QChar(0xe727));
    mediaInfo->setObjectName(QStringLiteral("ListEditButton"));
    mediaInfo->setToolButtonStyle(Qt::ToolButtonTextOnly);
    mediaInfo->setPopupMode(QToolButton::InstantPopup);
    mediaInfo->setToolTip(tr("Media Info"));

    windowSize=new QToolButton(playInfoPanel);
    windowSize->setFont(GlobalObjects::iconfont);
    windowSize->setText(QChar(0xe720));
    windowSize->setObjectName(QStringLiteral("ListEditButton"));
    windowSize->setToolButtonStyle(Qt::ToolButtonTextOnly);
    windowSize->setPopupMode(QToolButton::InstantPopup);
    windowSize->setToolTip(tr("Window Size"));

    screenshot=new QToolButton(playInfoPanel);
    screenshot->setFont(GlobalObjects::iconfont);
    screenshot->setText(QChar(0xe741));
    screenshot->setObjectName(QStringLiteral("ListEditButton"));
    screenshot->setToolButtonStyle(Qt::ToolButtonTextOnly);
    screenshot->setPopupMode(QToolButton::InstantPopup);
    screenshot->setToolTip(tr("Screenshot"));

    stayOnTop=new QToolButton(playInfoPanel);
    stayOnTop->setFont(GlobalObjects::iconfont);
    stayOnTop->setText(QChar(0xe65a));
    stayOnTop->setObjectName(QStringLiteral("ListEditButton"));
    stayOnTop->setToolButtonStyle(Qt::ToolButtonTextOnly);
    stayOnTop->setPopupMode(QToolButton::InstantPopup);
    stayOnTop->setToolTip(tr("On Top"));

    QHBoxLayout *infoHLayout=new QHBoxLayout(playInfoPanel);
    infoHLayout->setSpacing(0);
    infoHLayout->addWidget(titleLabel);
    infoHLayout->addItem(new QSpacerItem(1,1,QSizePolicy::MinimumExpanding));
    infoHLayout->addWidget(mediaInfo);
    infoHLayout->addWidget(windowSize);
    infoHLayout->addWidget(screenshot);
    infoHLayout->addWidget(stayOnTop);


    playControlPanel=new QWidget(this);
    playControlPanel->setObjectName(QStringLiteral("widgetPlayControl"));
    playControlPanel->hide();

    QVBoxLayout *controlVLayout=new QVBoxLayout(playControlPanel);
    controlVLayout->setSpacing(0);
    process=new ClickSlider(playControlPanel);
    process->setObjectName(QStringLiteral("widgetProcessSlider"));
    process->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
    process->setTracking(false);
    processPressed=false;
    process->setMouseTracking(true);
    controlVLayout->addWidget(process);

    GlobalObjects::iconfont.setPointSize(24);

    int buttonWidth=36*logicalDpiX()/96,buttonHeight=36*logicalDpiY()/96;
    timeLabel=new QLabel("00:00/00:00",playControlPanel);
    timeLabel->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Minimum);
    timeLabel->setObjectName(QStringLiteral("labelTime"));
    normalFont.setPointSize(10);
    timeLabel->setFont(normalFont);

    play_pause=new QPushButton(playControlPanel);
    play_pause->setFont(GlobalObjects::iconfont);
    play_pause->setText(QChar(0xe606));
    //play_pause->setFixedSize(buttonWidth,buttonHeight);
    play_pause->setObjectName(QStringLiteral("widgetPlayControlButtons"));
    play_pause->setToolTip(tr("Play/Pause(Space)"));
    GlobalObjects::iconfont.setPointSize(20);

    prev=new QPushButton(playControlPanel);
    prev->setFont(GlobalObjects::iconfont);
    prev->setText(QChar(0xe69b));
    prev->setFixedSize(buttonWidth,buttonHeight);
    prev->setToolTip(tr("Prev(PageUp)"));
    prev->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    next=new QPushButton(playControlPanel);
    next->setFont(GlobalObjects::iconfont);
    next->setText(QChar(0xe940));
    next->setFixedSize(buttonWidth,buttonHeight);
    next->setToolTip(tr("Next(PageDown)"));
    next->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    stop=new QPushButton(playControlPanel);
    stop->setFont(GlobalObjects::iconfont);
    stop->setText(QChar(0xe6fa));
    stop->setFixedSize(buttonWidth,buttonHeight);
    stop->setToolTip(tr("Stop"));
    stop->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    mute=new QPushButton(playControlPanel);
    mute->setFont(GlobalObjects::iconfont);
    mute->setText(QChar(0xe62c));
    mute->setFixedSize(buttonWidth,buttonHeight);
    mute->setToolTip(tr("Mute/Unmute"));
    mute->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    GlobalObjects::iconfont.setPointSize(18);

    setting=new QPushButton(playControlPanel);
    setting->setFont(GlobalObjects::iconfont);
    setting->setText(QChar(0xe607));
    setting->setFixedSize(buttonWidth,buttonHeight);
    setting->setToolTip(tr("Play Setting"));
    setting->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    danmu=new QPushButton(playControlPanel);
    danmu->setFont(GlobalObjects::iconfont);
    danmu->setText(QChar(0xe622));
    danmu->setFixedSize(buttonWidth,buttonHeight);
    danmu->setToolTip(tr("Danmu Setting"));
    danmu->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    fullscreen=new QPushButton(playControlPanel);
    fullscreen->setFont(GlobalObjects::iconfont);
    fullscreen->setText(QChar(0xe621));
    fullscreen->setFixedSize(buttonWidth,buttonHeight);
    fullscreen->setToolTip(tr("FullScreen"));
    fullscreen->setObjectName(QStringLiteral("widgetPlayControlButtons"));

    volume=new QSlider(Qt::Horizontal,playControlPanel);
    volume->setObjectName(QStringLiteral("widgetVolumeSlider"));
    volume->setFixedWidth(90*logicalDpiX()/96);
    volume->setMinimum(0);
    volume->setMaximum(100);
    volume->setSingleStep(1);

    ctrlPressCount=0;
    altPressCount=0;
    QObject::connect(&doublePressTimer,&QTimer::timeout,[this](){
        ctrlPressCount=0;
        altPressCount=0;
        doublePressTimer.stop();
    });
    QObject::connect(&hideCursorTimer,&QTimer::timeout,[this](){
        if(isFullscreen)setCursor(Qt::BlankCursor);
    });

    QHBoxLayout *buttonHLayout=new QHBoxLayout();
    buttonHLayout->addWidget(timeLabel);
    buttonHLayout->addStretch(1);
    buttonHLayout->addWidget(stop);
    buttonHLayout->addWidget(prev);
    buttonHLayout->addWidget(play_pause);
    buttonHLayout->addWidget(next);
    buttonHLayout->addWidget(mute);
    buttonHLayout->addWidget(volume);
    buttonHLayout->addStretch(1);
    buttonHLayout->addWidget(setting);
    buttonHLayout->addWidget(danmu);
    buttonHLayout->addWidget(fullscreen);

    controlVLayout->addLayout(buttonHLayout);

    setupDanmuSettingPage();
    setupPlaySettingPage();
    initActions();
    setupSignals();
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
}

void PlayerWindow::initActions()
{
    windowSizeGroup=new QActionGroup(this);
    QAction *act_size50p = new QAction("50%",this);
    act_size50p->setCheckable(true);
    act_size50p->setData(QVariant(50));
    windowSizeGroup->addAction(act_size50p);
    QAction *act_size75p = new QAction("75%",this);
    act_size75p->setCheckable(true);
    act_size75p->setData(QVariant(75));
    windowSizeGroup->addAction(act_size75p);
    QAction *act_size100p = new QAction("100%",this);
    act_size100p->setCheckable(true);
    act_size100p->setData(QVariant(100));
    windowSizeGroup->addAction(act_size100p);
    QAction *act_size150p = new QAction("150%",this);
    act_size150p->setCheckable(true);
    act_size150p->setData(QVariant(150));
    windowSizeGroup->addAction(act_size150p);
    QAction *act_size200p = new QAction("200%",this);
    act_size200p->setCheckable(true);
    act_size200p->setData(QVariant(200));
    windowSizeGroup->addAction(act_size200p);
    QAction *act_sizeFix = new QAction(tr("Fix"),this);
    act_sizeFix->setCheckable(true);
    act_sizeFix->setData(QVariant(-1));
    windowSizeGroup->addAction(act_sizeFix);

    QObject::connect(windowSizeGroup,&QActionGroup::triggered,[this](QAction *action){
        resizePercent=action->data().toInt();
        if(resizePercent==-1)return;
        adjustPlayerSize(resizePercent);
    });
    windowSizeGroup->actions().at(GlobalObjects::appSetting->value("Play/WindowSize",2).toInt())->trigger();
    windowSize->addActions(windowSizeGroup->actions());

    act_screenshotSrc = new QAction(tr("Original Video"),this);
    QObject::connect(act_screenshotSrc,&QAction::triggered,[this](){
        QTemporaryFile tmpImg("XXXXXX.jpg");
        if(tmpImg.open())
        {
            GlobalObjects::mpvplayer->screenshot(tmpImg.fileName());
            QImage captureImage(tmpImg.fileName());
            const PlayListItem *curItem=GlobalObjects::playlist->getCurrentItem();
            Capture captureDialog(captureImage,screenshot,curItem);
            QRect geo(captureDialog.geometry());
            geo.moveCenter(this->geometry().center());
            captureDialog.move(geo.topLeft());
            captureDialog.exec();
        }
    });
    act_screenshotAct = new QAction(tr("Actual content"),this);
    QObject::connect(act_screenshotAct,&QAction::triggered,[this](){
        QImage captureImage(GlobalObjects::mpvplayer->grabFramebuffer());
        const PlayListItem *curItem=GlobalObjects::playlist->getCurrentItem();
        Capture captureDialog(captureImage,screenshot,curItem);
        QRect geo(captureDialog.geometry());
        geo.moveCenter(this->geometry().center());
        captureDialog.move(geo.topLeft());
        captureDialog.exec();
    });
    screenshot->addAction(act_screenshotSrc);
    screenshot->addAction(act_screenshotAct);

    stayOnTopGroup=new QActionGroup(this);
    QAction *act_onTopPlaying = new QAction(tr("While Playing"),this);
    act_onTopPlaying->setCheckable(true);
    act_onTopPlaying->setData(QVariant(0));
    stayOnTopGroup->addAction(act_onTopPlaying);
    QAction *act_onTopAlways = new QAction(tr("Always"),this);
    act_onTopAlways->setCheckable(true);
    act_onTopAlways->setData(QVariant(1));
    stayOnTopGroup->addAction(act_onTopAlways);
    QAction *act_onTopNever = new QAction(tr("Never"),this);
    act_onTopNever->setCheckable(true);
    act_onTopNever->setData(QVariant(2));
    stayOnTopGroup->addAction(act_onTopNever);
    QObject::connect(stayOnTopGroup,&QActionGroup::triggered,[this](QAction *action){
        switch (action->data().toInt())
        {
        case 0:
            onTopWhilePlaying=true;
            if(GlobalObjects::mpvplayer->getState()==MPVPlayer::Play && GlobalObjects::playlist->getCurrentItem() != nullptr)
                emit setStayOnTop(true);
			else
				emit setStayOnTop(false);
            break;
        case 1:
            onTopWhilePlaying=false;
            emit setStayOnTop(true);
            break;
        case 2:
            onTopWhilePlaying=false;
            emit setStayOnTop(false);
            break;
        default:
            break;
        }
    });
    stayOnTopGroup->actions().at(GlobalObjects::appSetting->value("Play/StayOnTop",0).toInt())->trigger();
    stayOnTop->addActions(stayOnTopGroup->actions());

    actPlayPause=new QAction(tr("Play/Pause"),this);
    QObject::connect(actPlayPause,&QAction::triggered,[](){
        MPVPlayer::PlayState state=GlobalObjects::mpvplayer->getState();
        switch(state)
        {
        case MPVPlayer::Play:
            GlobalObjects::mpvplayer->setState(MPVPlayer::Pause);
            break;
        case MPVPlayer::EndReached:
        {
            GlobalObjects::mpvplayer->seek(0);
            GlobalObjects::mpvplayer->setState(MPVPlayer::Play);
            break;
        }
        case MPVPlayer::Pause:
            GlobalObjects::mpvplayer->setState(MPVPlayer::Play);
            break;
        case MPVPlayer::Stop:
            break;
        }
    });
    actFullscreen=new QAction(tr("Fullscreen"),this);
    QObject::connect(actFullscreen,&QAction::triggered,[this](){
        isFullscreen=!isFullscreen;
        emit showFullScreen(isFullscreen);
        if(isFullscreen) fullscreen->setText(QChar(0xe6ac));
        else fullscreen->setText(QChar(0xe621));
        if(isFullscreen)
        {
            hideCursorTimer.start(hideCursorTimeout);
        }
        else
        {
            hideCursorTimer.stop();
            setCursor(Qt::ArrowCursor);
        }
    });
    actPrev=new QAction(tr("Prev"),this);
    QObject::connect(actPrev,&QAction::triggered,[this](){
        switchItem(true,tr("No prev item"));
    });
    actNext=new QAction(tr("Next"),this);
    QObject::connect(actNext,&QAction::triggered,[this](){
        switchItem(false,tr("No next item"));
    });
    contexMenu=new QMenu(this);
    ctx_Text=contexMenu->addAction("");
    QObject::connect(ctx_Text,&QAction::triggered,[this](){
        currentDanmu=nullptr;
    });
    ctx_Copy=contexMenu->addAction(tr("Copy Text"));
    QObject::connect(ctx_Copy,&QAction::triggered,[this](){
        if(currentDanmu.isNull())return;
        QClipboard *cb = QApplication::clipboard();
        cb->setText(currentDanmu->text);
        currentDanmu=nullptr;
    });
    contexMenu->addSeparator();
    ctx_BlockText=contexMenu->addAction(tr("Block Text"));
    QObject::connect(ctx_BlockText,&QAction::triggered,[this](){
        if(currentDanmu.isNull())return;
        BlockRule *rule=new BlockRule;
        rule->blockField=BlockRule::Field::DanmuText;
        rule->relation=BlockRule::Relation::Contain;
        rule->enable=true;
        rule->isRegExp=false;
        rule->content=currentDanmu->text;
        rule->usePreFilter=false;
        GlobalObjects::blocker->addBlockRule(rule);
        currentDanmu=nullptr;
        showMessage(tr("Block Rule Added"));
    });
    ctx_BlockUser=contexMenu->addAction(tr("Block User"));
    QObject::connect(ctx_BlockUser,&QAction::triggered,[this](){
        if(currentDanmu.isNull())return;
        BlockRule *rule=new BlockRule;
        rule->blockField=BlockRule::Field::DanmuSender;
        rule->relation=BlockRule::Relation::Equal;
        rule->enable=true;
        rule->isRegExp=false;
        rule->usePreFilter=false;
        rule->content=currentDanmu->sender;
        GlobalObjects::blocker->addBlockRule(rule);
        currentDanmu=nullptr;
        showMessage(tr("Block Rule Added"));
    });
    ctx_BlockColor=contexMenu->addAction(tr("Block Color"));
    QObject::connect(ctx_BlockColor,&QAction::triggered,[this](){
        if(currentDanmu.isNull())return;
        BlockRule *rule=new BlockRule;
        rule->blockField=BlockRule::Field::DanmuColor;
        rule->relation=BlockRule::Relation::Equal;
        rule->enable=true;
        rule->isRegExp=false;
        rule->usePreFilter=false;
        rule->content=QString::number(currentDanmu->color,16);
        GlobalObjects::blocker->addBlockRule(rule);
        currentDanmu=nullptr;
        showMessage(tr("Block Rule Added"));
    });
}

void PlayerWindow::setupDanmuSettingPage()
{
    danmuSettingPage=new QWidget(this);
    danmuSettingPage->setObjectName(QStringLiteral("PopupPage"));
    danmuSettingPage->installEventFilter(this);
    danmuSettingPage->resize(360 *logicalDpiX()/96,220*logicalDpiY()/96);
    danmuSettingPage->hide();

    danmuSwitch=new QCheckBox(tr("Hide Danmu"),danmuSettingPage);
    danmuSwitch->setToolTip("Double Press Ctrl");
    QObject::connect(danmuSwitch,&QCheckBox::stateChanged,[this](int state){
        if(state==Qt::Checked)
        {
            GlobalObjects::mpvplayer->hideDanmu(true);
            this->danmu->setText(QChar(0xe620));
        }
        else
        {
            GlobalObjects::mpvplayer->hideDanmu(false);
            this->danmu->setText(QChar(0xe622));
        }
    });
    danmuSwitch->setChecked(GlobalObjects::appSetting->value("Play/HideDanmu",false).toBool());

    hideRollingDanmu=new QCheckBox(tr("Hide Rolling"),danmuSettingPage);
    QObject::connect(hideRollingDanmu,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->hideDanmu(DanmuComment::Rolling,state==Qt::Checked?true:false);
    });
    hideRollingDanmu->setChecked(GlobalObjects::appSetting->value("Play/HideRolling",false).toBool());

    hideTopDanmu=new QCheckBox(tr("Hide Top"),danmuSettingPage);
    QObject::connect(hideTopDanmu,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->hideDanmu(DanmuComment::Top,state==Qt::Checked?true:false);
    });
    hideTopDanmu->setChecked(GlobalObjects::appSetting->value("Play/HideTop",false).toBool());

    hideBottomDanmu=new QCheckBox(tr("Hide Bottom"),danmuSettingPage);
    QObject::connect(hideBottomDanmu,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->hideDanmu(DanmuComment::Bottom,state==Qt::Checked?true:false);
    });
    hideBottomDanmu->setChecked(GlobalObjects::appSetting->value("Play/HideBottom",false).toBool());

    QLabel *speedLabel=new QLabel(tr("Rolling Speed"),danmuSettingPage);
    speedSlider=new QSlider(Qt::Horizontal,danmuSettingPage);
    speedSlider->setRange(50,500);
    QObject::connect(speedSlider,&QSlider::valueChanged,[this](int val){
        GlobalObjects::danmuRender->setSpeed(val);
        speedSlider->setToolTip(QString::number(val));
    });
    speedSlider->setValue(GlobalObjects::appSetting->value("Play/DanmuSpeed",200).toInt());

    QLabel *alphaLabel=new QLabel(tr("Danmu Alpha"),danmuSettingPage);
    alphaSlider=new QSlider(Qt::Horizontal,danmuSettingPage);
    alphaSlider->setRange(0,100);
    QObject::connect(alphaSlider,&QSlider::valueChanged,[this](int val){
        GlobalObjects::danmuRender->setOpacity(val/100.f);
        alphaSlider->setToolTip(QString::number(val));
    });
    alphaSlider->setValue(GlobalObjects::appSetting->value("Play/DanmuAlpha",100).toInt());

    QLabel *strokeWidthLabel=new QLabel(tr("Stroke Width"),danmuSettingPage);
    strokeWidthSlider=new QSlider(Qt::Horizontal,danmuSettingPage);
    strokeWidthSlider->setRange(0,80);
    QObject::connect(strokeWidthSlider,&QSlider::valueChanged,[this](int val){
        GlobalObjects::danmuRender->setStrokeWidth(val/10.f);
        strokeWidthSlider->setToolTip(QString::number(val/10.));
    });
    strokeWidthSlider->setValue(GlobalObjects::appSetting->value("Play/DanmuStroke",35).toInt());

    QLabel *fontSizeLabel=new QLabel(tr("Font Size"),danmuSettingPage);
    fontSizeSlider=new QSlider(Qt::Horizontal,danmuSettingPage);
    fontSizeSlider->setRange(10,50);
    QObject::connect(fontSizeSlider,&QSlider::valueChanged,[this](int val){
        GlobalObjects::danmuRender->setFontSize(val);
        fontSizeSlider->setToolTip(QString::number(val));
    });
    fontSizeSlider->setValue(GlobalObjects::appSetting->value("Play/DanmuFontSize",20).toInt());

    bold=new QCheckBox(tr("Bold"),danmuSettingPage);
    QObject::connect(bold,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->setBold(state==Qt::Checked?true:false);
    });
    bold->setChecked(GlobalObjects::appSetting->value("Play/DanmuBold",false).toBool());

    bottomSubtitleProtect=new QCheckBox(tr("Protect Bottom Sub"),danmuSettingPage);
	bottomSubtitleProtect->setChecked(true);
    QObject::connect(bottomSubtitleProtect,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->setBottomSubtitleProtect(state==Qt::Checked?true:false);
    });
    bottomSubtitleProtect->setChecked(GlobalObjects::appSetting->value("Play/BottomSubProtect",true).toBool());

    topSubtitleProtect=new QCheckBox(tr("Protect Top Sub"),danmuSettingPage);
    QObject::connect(topSubtitleProtect,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->setTopSubtitleProtect(state==Qt::Checked?true:false);
    });
    topSubtitleProtect->setChecked(GlobalObjects::appSetting->value("Play/TopSubProtect",false).toBool());

    randomSize=new QCheckBox(tr("Random Size"),danmuSettingPage);
    QObject::connect(randomSize,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->setRandomSize(state==Qt::Checked?true:false);
    });
    randomSize->setChecked(GlobalObjects::appSetting->value("Play/RandomSize",false).toBool());

    QLabel *maxDanmuCountLabel=new QLabel(tr("Max Count"),danmuSettingPage);
    maxDanmuCount=new QSlider(Qt::Horizontal,danmuSettingPage);
    maxDanmuCount->setRange(40,300);
    QObject::connect(maxDanmuCount,&QSlider::valueChanged,[this](int val){
        GlobalObjects::danmuRender->setMaxDanmuCount(val);
        maxDanmuCount->setToolTip(QString::number(val));
    });
    maxDanmuCount->setValue(GlobalObjects::appSetting->value("Play/MaxCount",100).toInt());

    QLabel *denseLabel=new QLabel(tr("Dense Level"),danmuSettingPage);
    denseLevel=new QComboBox(danmuSettingPage);
    denseLevel->addItems(QStringList()<<tr("Uncovered")<<tr("General")<<tr("Dense"));
    QObject::connect(denseLevel,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[](int index){
         GlobalObjects::danmuRender->dense=index;
    });
    denseLevel->setCurrentIndex(GlobalObjects::appSetting->value("Play/Dense",1).toInt());

    fontFamilyCombo=new QFontComboBox(danmuSettingPage);
    fontFamilyCombo->setMaximumWidth(160 *logicalDpiX()/96);
    QLabel *fontLabel=new QLabel(tr("Font"),danmuSettingPage);
    QObject::connect(fontFamilyCombo,&QFontComboBox::currentFontChanged,[](const QFont &newFont){
        GlobalObjects::danmuRender->setFontFamily(newFont.family());
    });
    fontFamilyCombo->setCurrentFont(QFont(GlobalObjects::appSetting->value("Play/DanmuFont","Microsoft Yahei").toString()));

    enableAnalyze=new QCheckBox(tr("Enable Danmu Event Analyze"),danmuSettingPage);
    enableAnalyze->setChecked(true);
    QObject::connect(enableAnalyze,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuPool->setAnalyzeEnable(state==Qt::Checked?true:false);
    });
    enableAnalyze->setChecked(GlobalObjects::appSetting->value("Play/EnableAnalyze",true).toBool());

    enableMerge=new QCheckBox(tr("Enable Danmu Merge"),danmuSettingPage);
    enableMerge->setChecked(true);
    QObject::connect(enableMerge,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuPool->setMergeEnable(state==Qt::Checked?true:false);
    });
    enableMerge->setChecked(GlobalObjects::appSetting->value("Play/EnableMerge",true).toBool());

    enlargeMerged=new QCheckBox(tr("Enlarge Merged Danmu"),danmuSettingPage);
    enlargeMerged->setChecked(true);
    QObject::connect(enlargeMerged,&QCheckBox::stateChanged,[](int state){
        GlobalObjects::danmuRender->setEnlargeMerged(state==Qt::Checked?true:false);
    });
    enlargeMerged->setChecked(GlobalObjects::appSetting->value("Play/EnlargeMerged",true).toBool());

    QLabel *mergeIntervalLabel=new QLabel(tr("Merge Interval(s)"),danmuSettingPage);
    mergeInterval=new QSpinBox(danmuSettingPage);
    mergeInterval->setRange(1,60);
    mergeInterval->setAlignment(Qt::AlignCenter);
    mergeInterval->setObjectName(QStringLiteral("Delay"));
    QObject::connect(mergeInterval,&QSpinBox::editingFinished,this,[this](){
        GlobalObjects::danmuPool->setMergeInterval(mergeInterval->value()*1000);
    });
    mergeInterval->setValue(GlobalObjects::appSetting->value("Play/MergeInterval",15).toInt());
    GlobalObjects::danmuPool->setMergeInterval(mergeInterval->value()*1000);

    QLabel *contentSimLabel=new QLabel(tr("MaxDiff Char Count"),danmuSettingPage);
    contentSimCount=new QSpinBox(danmuSettingPage);
    contentSimCount->setRange(1,11);
    contentSimCount->setAlignment(Qt::AlignCenter);
    contentSimCount->setObjectName(QStringLiteral("Delay"));
    QObject::connect(contentSimCount,&QSpinBox::editingFinished,this,[this](){
        GlobalObjects::danmuPool->setMaxUnSimCount(contentSimCount->value());
    });
    contentSimCount->setValue(GlobalObjects::appSetting->value("Play/MaxDiffCount",4).toInt());
    GlobalObjects::danmuPool->setMaxUnSimCount(contentSimCount->value());

    QLabel *minMergeCountLabel=new QLabel(tr("Min Similar Danmu Count"),danmuSettingPage);
    minMergeCount=new QSpinBox(danmuSettingPage);
    minMergeCount->setAlignment(Qt::AlignCenter);
    minMergeCount->setRange(1,11);
    minMergeCount->setObjectName(QStringLiteral("Delay"));
    QObject::connect(minMergeCount,&QSpinBox::editingFinished,this,[this](){
        GlobalObjects::danmuPool->setMinMergeCount(minMergeCount->value());
    });
    minMergeCount->setValue(GlobalObjects::appSetting->value("Play/MinSimCount",3).toInt());
    GlobalObjects::danmuPool->setMinMergeCount(minMergeCount->value());

    QLabel *mergeCountTipLabel=new QLabel(tr("Merge Count Tip Position"),danmuSettingPage);
    mergeCountTipPos=new QComboBox(danmuSettingPage);
    mergeCountTipPos->addItems(QStringList()<<tr("Hidden")<<tr("Forward")<<tr("Backward"));
    QObject::connect(mergeCountTipPos,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[](int index){
        GlobalObjects::danmuRender->setMergeCountPos(index);
    });
    mergeCountTipPos->setCurrentIndex(GlobalObjects::appSetting->value("Play/MergeCountTip",1).toInt());


    QToolButton *generalPage=new QToolButton(danmuSettingPage);
    generalPage->setText(tr("General"));
    generalPage->setCheckable(true);
    generalPage->setToolButtonStyle(Qt::ToolButtonTextOnly);
    generalPage->setMaximumHeight(28 * logicalDpiY()/96);
    generalPage->setObjectName(QStringLiteral("DialogPageButton"));
    generalPage->setChecked(true);

    QToolButton *appearancePage=new QToolButton(danmuSettingPage);
    appearancePage->setText(tr("Appearance"));
    appearancePage->setCheckable(true);
    appearancePage->setToolButtonStyle(Qt::ToolButtonTextOnly);
    appearancePage->setMaximumHeight(28 * logicalDpiY()/96);
    appearancePage->setObjectName(QStringLiteral("DialogPageButton"));

    QToolButton *advancedPage=new QToolButton(danmuSettingPage);
    advancedPage->setText(tr("Advanced"));
    advancedPage->setCheckable(true);
    advancedPage->setToolButtonStyle(Qt::ToolButtonTextOnly);
    advancedPage->setMaximumHeight(28 * logicalDpiY()/96);
    advancedPage->setObjectName(QStringLiteral("DialogPageButton"));


    QStackedLayout *danmuSettingSLayout=new QStackedLayout();
    QButtonGroup *btnGroup=new QButtonGroup(this);
    btnGroup->addButton(generalPage,0);
    btnGroup->addButton(appearancePage,1);
    btnGroup->addButton(advancedPage,2);
    QObject::connect(btnGroup,(void (QButtonGroup:: *)(int, bool))&QButtonGroup::buttonToggled,[danmuSettingSLayout](int id, bool checked){
        if(checked)
        {
            danmuSettingSLayout->setCurrentIndex(id);
        }
    });

    QHBoxLayout *pageButtonHLayout=new QHBoxLayout();
    pageButtonHLayout->addWidget(generalPage);
    pageButtonHLayout->addWidget(appearancePage);
    pageButtonHLayout->addWidget(advancedPage);

    QVBoxLayout *danmuSettingVLayout=new QVBoxLayout(danmuSettingPage);
    danmuSettingVLayout->addLayout(pageButtonHLayout);
    danmuSettingVLayout->addLayout(danmuSettingSLayout);

    QWidget *pageGeneral=new QWidget(danmuSettingPage);
    danmuSettingSLayout->addWidget(pageGeneral);
    danmuSettingSLayout->setContentsMargins(4,4,4,4);
    QGridLayout *generalGLayout=new QGridLayout(pageGeneral);
    generalGLayout->setContentsMargins(0,0,0,0);
    generalGLayout->setColumnStretch(0,1);
    generalGLayout->setColumnStretch(1,1);
    generalGLayout->addWidget(danmuSwitch,0,0);
    generalGLayout->addWidget(hideRollingDanmu,1,0);
    generalGLayout->addWidget(hideTopDanmu,2,0);
    generalGLayout->addWidget(hideBottomDanmu,3,0);
    generalGLayout->addWidget(bottomSubtitleProtect,4,0);
    generalGLayout->addWidget(topSubtitleProtect,5,0);
    generalGLayout->addWidget(denseLabel,0,1);
    generalGLayout->addWidget(denseLevel,1,1);
    generalGLayout->addWidget(speedLabel,2,1);
    generalGLayout->addWidget(speedSlider,3,1);
    generalGLayout->addWidget(maxDanmuCountLabel,4,1);
    generalGLayout->addWidget(maxDanmuCount,5,1);

    QWidget *pageAppearance=new QWidget(danmuSettingPage);
    danmuSettingSLayout->addWidget(pageAppearance);
    QGridLayout *appearanceGLayout=new QGridLayout(pageAppearance);
    appearanceGLayout->setContentsMargins(0,0,0,0);
    appearanceGLayout->setColumnStretch(0,1);
    appearanceGLayout->setColumnStretch(1,1);
    appearanceGLayout->addWidget(fontLabel,0,0);
    appearanceGLayout->addWidget(fontFamilyCombo,1,0);
    appearanceGLayout->addWidget(fontSizeLabel,2,0);
    appearanceGLayout->addWidget(fontSizeSlider,3,0);
    appearanceGLayout->addWidget(strokeWidthLabel,4,0);
    appearanceGLayout->addWidget(strokeWidthSlider,5,0);
    appearanceGLayout->addWidget(alphaLabel,0,1);
    appearanceGLayout->addWidget(alphaSlider,1,1);
    appearanceGLayout->addWidget(bold,2,1);
    appearanceGLayout->addWidget(randomSize,3,1);

    QWidget *pageAdvanced=new QWidget(danmuSettingPage);
    danmuSettingSLayout->addWidget(pageAdvanced);
    QGridLayout *mergeGLayout=new QGridLayout(pageAdvanced);
    mergeGLayout->setContentsMargins(0,0,0,0);
    mergeGLayout->setColumnStretch(0,1);
    mergeGLayout->setColumnStretch(1,1);
    mergeGLayout->addWidget(enableAnalyze,0,0);
    mergeGLayout->addWidget(enableMerge,1,0);
    mergeGLayout->addWidget(enlargeMerged,2,0);
    mergeGLayout->addWidget(mergeIntervalLabel,3,0);
    mergeGLayout->addWidget(mergeInterval,4,0);
    mergeGLayout->addWidget(contentSimLabel,0,1);
    mergeGLayout->addWidget(contentSimCount,1,1);
    mergeGLayout->addWidget(minMergeCountLabel,2,1);
    mergeGLayout->addWidget(minMergeCount,3,1);
    mergeGLayout->addWidget(mergeCountTipLabel,4,1);
    mergeGLayout->addWidget(mergeCountTipPos,5,1);
}

void PlayerWindow::setupPlaySettingPage()
{
    playSettingPage=new QWidget(this);
    playSettingPage->setObjectName(QStringLiteral("PopupPage"));
    playSettingPage->installEventFilter(this);
    playSettingPage->resize(320 *logicalDpiX()/96,200*logicalDpiY()/96);
    playSettingPage->hide();

    QLabel *audioTrackLabel=new QLabel(tr("Audio Track"),playSettingPage);
    QComboBox *audioTrackCombo=new QComboBox(playSettingPage);
    QObject::connect(audioTrackCombo,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[this](int index){
        if(updatingTrack)return;
        GlobalObjects::mpvplayer->setTrackId(0,index);
    });

    QLabel *subtitleTrackLabel=new QLabel(tr("Subtitle Track"),playSettingPage);
    QComboBox *subtitleTrackCombo=new QComboBox(playSettingPage);
    QObject::connect(subtitleTrackCombo,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[subtitleTrackCombo, this](int index){
        if(updatingTrack)return;
        if(index==subtitleTrackCombo->count()-1)
        {
            GlobalObjects::mpvplayer->hideSubtitle(true);
        }
        else
        {
            GlobalObjects::mpvplayer->hideSubtitle(false);
            GlobalObjects::mpvplayer->setTrackId(1,index);
        }
    });
    QPushButton *addSubtitleButton=new QPushButton(tr("Add"),playSettingPage);
    QObject::connect(addSubtitleButton,&QPushButton::clicked,[this](){
        bool restorePlayState = false;
        if (GlobalObjects::mpvplayer->getState() == MPVPlayer::Play)
        {
            restorePlayState = true;
            GlobalObjects::mpvplayer->setState(MPVPlayer::Pause);
        }
        QString file(QFileDialog::getOpenFileName(this,tr("Select Sub File"),"",tr("Subtitle (%0)").arg(GlobalObjects::mpvplayer->subtitleFormats.join(" "))));
        if(!file.isEmpty())
                GlobalObjects::mpvplayer->addSubtitle(file);
        if(restorePlayState)GlobalObjects::mpvplayer->setState(MPVPlayer::Play);
    });
    QLabel *subtitleDelayLabel=new QLabel(tr("Subtitle Delay(s)"),playSettingPage);
    QObject::connect(GlobalObjects::mpvplayer,&MPVPlayer::trackInfoChange,[subtitleTrackCombo,audioTrackCombo,this](int type){
        updatingTrack=true;
        if(type==0)
        {
            audioTrackCombo->clear();
            audioTrackCombo->addItems(GlobalObjects::mpvplayer->getTrackList(0));
            audioTrackCombo->setCurrentIndex(GlobalObjects::mpvplayer->getCurrentAudioTrack());
        }
        else if(type==1)
        {
            subtitleTrackCombo->clear();
            subtitleTrackCombo->addItems(GlobalObjects::mpvplayer->getTrackList(1));
            subtitleTrackCombo->addItem(tr("Hide"));;
            subtitleTrackCombo->setCurrentIndex(GlobalObjects::mpvplayer->getCurrentSubTrack());
        }
        updatingTrack=false;
    });
    QSpinBox *delaySpinBox=new QSpinBox(playSettingPage);
    delaySpinBox->setRange(INT_MIN,INT_MAX);
    delaySpinBox->setObjectName(QStringLiteral("Delay"));
    delaySpinBox->setAlignment(Qt::AlignCenter);
    QObject::connect(delaySpinBox,&QSpinBox::editingFinished,[delaySpinBox](){
       GlobalObjects::mpvplayer->setSubDelay(delaySpinBox->value());
    });

    QLabel *aspectSpeedLabel=new QLabel(tr("Aspect & Speed"),playSettingPage);
    aspectRatioCombo=new QComboBox(playSettingPage);
    aspectRatioCombo->addItems(GlobalObjects::mpvplayer->videoAspect);
    QObject::connect(aspectRatioCombo,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[this](int index){
        GlobalObjects::mpvplayer->setVideoAspect(index);
    });
    aspectRatioCombo->setCurrentIndex(GlobalObjects::appSetting->value("Play/VidoeAspectRatio",0).toInt());

    playSpeedCombo=new QComboBox(playSettingPage);
    playSpeedCombo->addItems(GlobalObjects::mpvplayer->speedLevel);
    QObject::connect(playSpeedCombo,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[this](int index){
        GlobalObjects::mpvplayer->setSpeed(index);
    });
    playSpeedCombo->setCurrentIndex(GlobalObjects::appSetting->value("Play/PlaySpeed",GlobalObjects::mpvplayer->speedLevel.indexOf("1")).toInt());

    QPushButton *editMpvOptions=new QPushButton(tr("MPV Parameter Settings"), playSettingPage);
    QObject::connect(editMpvOptions,&QPushButton::clicked,[this](){
        MPVParameterSetting mpvParameterDialog(this);
        mpvParameterDialog.exec();
    });

    QPushButton *showMpvLog=new QPushButton(tr("Show Mpv Log"), playSettingPage);
    QObject::connect(showMpvLog,&QPushButton::clicked,[this](){
        logDialog->show();
    });

    QLabel *clickBehaivorLabel=new QLabel(tr("Click Behavior"),playSettingPage);
    clickBehaviorCombo=new QComboBox(playSettingPage);
    clickBehaviorCombo->addItem(tr("Play/Pause"));
    clickBehaviorCombo->addItem(tr("Show/Hide PlayControl"));
    QObject::connect(clickBehaviorCombo,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[this](int index){
        clickBehavior=index;
        GlobalObjects::appSetting->setValue("Play/ClickBehavior",index);
    });
    clickBehaviorCombo->setCurrentIndex(GlobalObjects::appSetting->value("Play/ClickBehavior",0).toInt());
    clickBehavior=clickBehaviorCombo->currentIndex();

    QLabel *dbClickBehaivorLabel=new QLabel(tr("Double Click Behavior"),playSettingPage);
    dbClickBehaviorCombo=new QComboBox(playSettingPage);
    dbClickBehaviorCombo->addItem(tr("FullScreen"));
    dbClickBehaviorCombo->addItem(tr("Play/Pause"));
    QObject::connect(dbClickBehaviorCombo,(void (QComboBox:: *)(int))&QComboBox::currentIndexChanged,[this](int index){
        dbClickBehaivior=index;
        GlobalObjects::appSetting->setValue("Play/DBClickBehavior",index);
    });
    dbClickBehaviorCombo->setCurrentIndex(GlobalObjects::appSetting->value("Play/DBClickBehavior",0).toInt());
    dbClickBehaivior=dbClickBehaviorCombo->currentIndex();

    QLabel *forwardTimeLabel=new QLabel(tr("Forward Jump Time(s)"),playSettingPage);
    QSpinBox *forwardTimeSpin=new QSpinBox(playSettingPage);
    forwardTimeSpin->setRange(1,20);
    forwardTimeSpin->setAlignment(Qt::AlignCenter);
    forwardTimeSpin->setObjectName(QStringLiteral("Delay"));
    QObject::connect(forwardTimeSpin,&QSpinBox::editingFinished,this,[this,forwardTimeSpin](){
        jumpForwardTime=forwardTimeSpin->value();
        GlobalObjects::appSetting->setValue("Play/ForwardJump",jumpForwardTime);
    });
    forwardTimeSpin->setValue(GlobalObjects::appSetting->value("Play/ForwardJump",5).toInt());
    jumpForwardTime=forwardTimeSpin->value();

    QLabel *backwardTimeLabel=new QLabel(tr("Backward Jump Time(s)"),playSettingPage);
    QSpinBox *backwardTimeSpin=new QSpinBox(playSettingPage);
    backwardTimeSpin->setRange(1,20);
    backwardTimeSpin->setAlignment(Qt::AlignCenter);
    backwardTimeSpin->setObjectName(QStringLiteral("Delay"));
    QObject::connect(backwardTimeSpin,&QSpinBox::editingFinished,this,[this,backwardTimeSpin](){
        jumpBackwardTime=backwardTimeSpin->value();
        GlobalObjects::appSetting->setValue("Play/BackwardJump",jumpBackwardTime);
    });
    backwardTimeSpin->setValue(GlobalObjects::appSetting->value("Play/BackwardJump",5).toInt());
    jumpBackwardTime=backwardTimeSpin->value();

    QToolButton *playPage=new QToolButton(playSettingPage);
    playPage->setText(tr("Play"));
    playPage->setCheckable(true);
    playPage->setToolButtonStyle(Qt::ToolButtonTextOnly);
    playPage->setMaximumHeight(28 * logicalDpiY()/96);
    playPage->setObjectName(QStringLiteral("DialogPageButton"));
    playPage->setChecked(true);

    QToolButton *behaviorPage=new QToolButton(danmuSettingPage);
    behaviorPage->setText(tr("Behavior"));
    behaviorPage->setCheckable(true);
    behaviorPage->setToolButtonStyle(Qt::ToolButtonTextOnly);
    behaviorPage->setMaximumHeight(28 * logicalDpiY()/96);
    behaviorPage->setObjectName(QStringLiteral("DialogPageButton"));

    QStackedLayout *playSettingSLayout=new QStackedLayout();
    QObject::connect(playPage,&QToolButton::clicked,[playSettingSLayout,playPage,behaviorPage](){
        playSettingSLayout->setCurrentIndex(0);
        playPage->setChecked(true);
        behaviorPage->setChecked(false);
    });
    QObject::connect(behaviorPage,&QToolButton::clicked,[playSettingSLayout,playPage,behaviorPage](){
        playSettingSLayout->setCurrentIndex(1);
        playPage->setChecked(false);
        behaviorPage->setChecked(true);
    });

    QHBoxLayout *pageButtonHLayout=new QHBoxLayout();
    pageButtonHLayout->addWidget(playPage);
    pageButtonHLayout->addWidget(behaviorPage);

    QVBoxLayout *playSettingVLayout=new QVBoxLayout(playSettingPage);
    playSettingVLayout->addLayout(pageButtonHLayout);
    playSettingVLayout->addLayout(playSettingSLayout);

    QWidget *pagePlay=new QWidget(playSettingPage);
    playSettingSLayout->addWidget(pagePlay);
    playSettingSLayout->setContentsMargins(4,4,4,4);

    QGridLayout *playSettingGLayout=new QGridLayout(pagePlay);
    //playSettingGLayout->setColumnStretch(0, 2);
    playSettingGLayout->setColumnStretch(1, 1);
    //playSettingGLayout->setColumnStretch(2, 1);
    playSettingGLayout->addWidget(audioTrackLabel,0,0);
    playSettingGLayout->addWidget(audioTrackCombo,0,1,1,2);
    playSettingGLayout->addWidget(subtitleTrackLabel,1,0);
    playSettingGLayout->addWidget(subtitleTrackCombo,1,1,1,2);
    playSettingGLayout->addWidget(subtitleDelayLabel,2,0);
    playSettingGLayout->addWidget(delaySpinBox,2,1);
    playSettingGLayout->addWidget(addSubtitleButton,2,2);
    playSettingGLayout->addWidget(aspectSpeedLabel,3,0);
    playSettingGLayout->addWidget(aspectRatioCombo,3,1);
    playSettingGLayout->addWidget(playSpeedCombo,3,2);
    QHBoxLayout *bottomBtnHLayout=new QHBoxLayout();
    bottomBtnHLayout->addWidget(editMpvOptions);
    bottomBtnHLayout->addWidget(showMpvLog);
    playSettingGLayout->addLayout(bottomBtnHLayout,4,0,1,3);

    QWidget *pageBehavior=new QWidget(playSettingPage);
    playSettingSLayout->addWidget(pageBehavior);
    QGridLayout *appearanceGLayout=new QGridLayout(pageBehavior);
    appearanceGLayout->setContentsMargins(0,0,0,0);
    appearanceGLayout->setColumnStretch(1, 1);
    appearanceGLayout->setRowStretch(4,1);
    appearanceGLayout->addWidget(clickBehaivorLabel,0,0);
    appearanceGLayout->addWidget(clickBehaviorCombo,0,1);
    appearanceGLayout->addWidget(dbClickBehaivorLabel,1,0);
    appearanceGLayout->addWidget(dbClickBehaviorCombo,1,1);
    appearanceGLayout->addWidget(forwardTimeLabel,2,0);
    appearanceGLayout->addWidget(forwardTimeSpin,2,1);
    appearanceGLayout->addWidget(backwardTimeLabel,3,0);
    appearanceGLayout->addWidget(backwardTimeSpin,3,1);
}

void PlayerWindow::setupSignals()
{
    QObject::connect(GlobalObjects::mpvplayer,&MPVPlayer::durationChanged,[this](int duration){
        QCoreApplication::processEvents();
        int ts=duration/1000;
        int lmin=ts/60;
        int ls=ts-lmin*60;
        process->setRange(0,duration);
        process->setSingleStep(1);
        totalTimeStr=QString("/%1:%2").arg(lmin,2,10,QChar('0')).arg(ls,2,10,QChar('0'));
        timeLabel->setText("00:00"+this->totalTimeStr);
        static_cast<DanmuStatisInfo *>(danmuStatisBar)->duration=ts;
        const PlayListItem *currentItem=GlobalObjects::playlist->getCurrentItem();
        if(currentItem->playTime>15 && currentItem->playTime<ts-15)
        {
            GlobalObjects::mpvplayer->seek(currentItem->playTime*1000);
            showMessage(tr("Jumped to the last play position"));
        }
        if(resizePercent!=-1) adjustPlayerSize(resizePercent);
        qDebug()<<"Duration Changed";
    });
    QObject::connect(GlobalObjects::mpvplayer,&MPVPlayer::fileChanged,[this](){
        const PlayListItem *currentItem=GlobalObjects::playlist->getCurrentItem();
        Q_ASSERT(currentItem);
        process->setEventMark(QList<DanmuEvent>());
        if(currentItem->animeTitle.isEmpty())
            titleLabel->setText(currentItem->title);
        else
            titleLabel->setText(QString("%1-%2").arg(currentItem->animeTitle,currentItem->title));
        if(!currentItem->poolID.isEmpty())
        {
            GlobalObjects::danmuPool->setPoolID(currentItem->poolID);
        }
        else
        {
            showMessage(tr("File is not associated with Danmu Pool"));
            if (GlobalObjects::danmuPool->hasPool())
            {
                GlobalObjects::danmuPool->setPoolID("");
            }
        }
        qDebug()<<"File Changed,Current Item: "<<currentItem->title;
    });

    QObject::connect(GlobalObjects::playlist,&PlayList::currentMatchChanged,[this](){
        const PlayListItem *currentItem=GlobalObjects::playlist->getCurrentItem();
        if(currentItem->animeTitle.isEmpty())
            titleLabel->setText(currentItem->title);
        else
            titleLabel->setText(QString("%1-%2").arg(currentItem->animeTitle).arg(currentItem->title));
    });
    QObject::connect(GlobalObjects::danmuPool, &DanmuPool::eventAnalyzeFinished,process,&ClickSlider::setEventMark);
    QObject::connect(GlobalObjects::playlist,&PlayList::recentItemsUpdated,[this](){
        static_cast<PlayerContent *>(playerContent)->refreshItems();
    });
    QObject::connect(GlobalObjects::mpvplayer,&MPVPlayer::positionChanged,[this](int newtime){
        int cs=newtime/1000;
        int cmin=cs/60;
        int cls=cs-cmin*60;
        if(!processPressed)
            process->setValue(newtime);
        timeLabel->setText(QString("%1:%2%3").arg(cmin,2,10,QChar('0')).arg(cls,2,10,QChar('0')).arg(this->totalTimeStr));
    });
    QObject::connect(GlobalObjects::mpvplayer,&MPVPlayer::stateChanged,[this](MPVPlayer::PlayState state){
        if(GlobalObjects::playlist->getCurrentItem()!=nullptr)
        {
#ifdef Q_OS_WIN
            if(state==MPVPlayer::Play)
                SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
            else
                SetThreadExecutionState(ES_CONTINUOUS);
#endif
            if(onTopWhilePlaying)
            {
                emit setStayOnTop(state==MPVPlayer::Play);
            }
        }
        if(onTopWhilePlaying && GlobalObjects::playlist->getCurrentItem() != nullptr)
        {

        }
        switch(state)
        {
        case MPVPlayer::Play:
			if (GlobalObjects::playlist->getCurrentItem() != nullptr)
            {
				this->play_pause->setText(QChar(0xe6fb));
                playerContent->hide();
            }
            break;
        case MPVPlayer::Pause:
			if (GlobalObjects::playlist->getCurrentItem() != nullptr)
				this->play_pause->setText(QChar(0xe606));
            break;
        case MPVPlayer::EndReached:
		{
			this->play_pause->setText(QChar(0xe606));
			GlobalObjects::danmuPool->reset();
			GlobalObjects::danmuRender->cleanup();
            //GlobalObjects::playlist->setCurrentPlayTime(0);
            setPlayTime();
			PlayList::LoopMode loopMode = GlobalObjects::playlist->getLoopMode();
			if (loopMode == PlayList::Loop_One)
			{
				GlobalObjects::mpvplayer->seek(0);
				GlobalObjects::mpvplayer->setState(MPVPlayer::Play);
			}
            else if (loopMode != PlayList::NO_Loop_One)
			{
                actNext->trigger();
			}
			break;
		}
        case MPVPlayer::Stop:
        {
            QCoreApplication::processEvents();
            setPlayTime();
            this->play_pause->setText(QChar(0xe606));
            this->process->setValue(0);
			this->process->setRange(0, 0);
            this->timeLabel->setText("00:00/00:00");
            titleLabel->setText(QString());
            GlobalObjects::playlist->cleanCurrentItem();
            GlobalObjects::danmuPool->cleanUp();
			GlobalObjects::danmuRender->cleanup();			
            GlobalObjects::mpvplayer->update();
            playerContent->raise();
            playerContent->show();
            break;
        }
        }
    });
    QObject::connect(play_pause,&QPushButton::clicked,actPlayPause,&QAction::trigger);

    QObject::connect(stop,&QPushButton::clicked,[](){
        QCoreApplication::processEvents();
        GlobalObjects::mpvplayer->setState(MPVPlayer::Stop);
    });
    QObject::connect(process,&ClickSlider::sliderClick,[](int pos){
        MPVPlayer::PlayState state=GlobalObjects::mpvplayer->getState();
        switch(state)
        {
        case MPVPlayer::Play:
        case MPVPlayer::Pause:
            GlobalObjects::mpvplayer->seek(pos);
            break;
        default:
            break;
        }
    });
    QObject::connect(process,&ClickSlider::sliderUp,[this](int pos){
        this->processPressed=false;
        MPVPlayer::PlayState state=GlobalObjects::mpvplayer->getState();
        switch(state)
        {
        case MPVPlayer::Play:
        case MPVPlayer::Pause:
            GlobalObjects::mpvplayer->seek(pos);
            break;
        default:
            break;
        }
    });
    QObject::connect(process,&ClickSlider::sliderPressed,[this](){
        this->processPressed=true;
    });
    QObject::connect(process,&ClickSlider::sliderReleased,[this](){
        this->processPressed=false;
    });
    QObject::connect(process,&ClickSlider::mouseMove,[this](int x,int ,int pos, const QString &desc){
        int cs=pos/1000;
        int cmin=cs/60;
        int cls=cs-cmin*60;
        QString timeTip(QString("%1:%2").arg(cmin,2,10,QChar('0')).arg(cls,2,10,QChar('0')));
        if(!desc.isEmpty())
            timeTip+="\n"+desc;
        timeInfoTip->setText(timeTip);
        timeInfoTip->adjustSize();
        int ty=danmuStatisBar->isHidden()?height()-controlPanelHeight-timeInfoTip->height():height()-controlPanelHeight-timeInfoTip->height()-statisBarHeight-1;
        int nx = x-timeInfoTip->width()/3;
        if(nx+timeInfoTip->width()>width()) nx = width()-timeInfoTip->width();
        timeInfoTip->move(nx<0?0:nx,ty);
        timeInfoTip->show();
        timeInfoTip->raise();
        //QToolTip::showText(QPoint(x,process->geometry().topLeft()-40),QString("%1:%2").arg(cmin,2,10,QChar('0')).arg(cls,2,10,QChar('0')),process,process->rect());
    });
    QObject::connect(process,&ClickSlider::mouseEnter,[this](){
		if (GlobalObjects::playlist->getCurrentItem() != nullptr && GlobalObjects::danmuPool->totalCount()>0)
			danmuStatisBar->show();
    });
    QObject::connect(process,&ClickSlider::mouseLeave,[this](){
        danmuStatisBar->hide();
        timeInfoTip->hide();
    });
    QObject::connect(volume,&QSlider::valueChanged,[this](int val){
        GlobalObjects::mpvplayer->setVolume(val);
        volume->setToolTip(QString::number(val));
    });
    volume->setValue(GlobalObjects::appSetting->value("Play/Volume",30).toInt());

    QObject::connect(mute,&QPushButton::clicked,[this](){
        if(GlobalObjects::mpvplayer->getMute())
        {
            GlobalObjects::mpvplayer->setMute(false);
            this->mute->setText(QChar(0xe62c));
        }
        else
        {
            GlobalObjects::mpvplayer->setMute(true);
            this->mute->setText(QChar(0xe61e));
        }
    });
    if(GlobalObjects::appSetting->value("Play/Mute",false).toBool())
        mute->click();

	QObject::connect(danmu, &QPushButton::clicked, [this]() {
        if (!playSettingPage->isHidden() ||!danmuSettingPage->isHidden())
		{
            playSettingPage->hide();
			danmuSettingPage->hide();
			return;
		}
        danmuSettingPage->show();
		danmuSettingPage->raise();
        QPoint leftTop(width() - danmuSettingPage->width() - 10, height() - controlPanelHeight - danmuSettingPage->height()+40);
        danmuSettingPage->move(leftTop-QPoint(0,20));
    });
    QObject::connect(setting,&QPushButton::clicked,[this](){
        if (!playSettingPage->isHidden() || !danmuSettingPage->isHidden())
		{
			playSettingPage->hide();
            danmuSettingPage->hide();
			return;
		}
        playSettingPage->show();
		playSettingPage->raise();
        QPoint leftTop(width() - playSettingPage->width() - 10, height() - controlPanelHeight - playSettingPage->height() + 40);
        playSettingPage->move(leftTop-QPoint(0,20));
    });
    QObject::connect(next,&QPushButton::clicked,actNext,&QAction::trigger);
    QObject::connect(prev,&QPushButton::clicked,actPrev,&QAction::trigger);
    QObject::connect(fullscreen,&QPushButton::clicked,actFullscreen,&QAction::trigger);

    QObject::connect(playListCollapseButton,&QPushButton::clicked,this,[this](){
        if(playListCollapseButton->text()==QChar(0xe946))
            playListCollapseButton->setText(QChar(0xe945));
        else
            playListCollapseButton->setText(QChar(0xe946));
        emit toggleListVisibility();
    });

    QObject::connect(mediaInfo,&QToolButton::clicked,[this](){
		if (GlobalObjects::playlist->getCurrentItem() == nullptr)return;
        MediaInfo mediaInfoDialog(this);
        mediaInfoDialog.exec();
    });
}

void PlayerWindow::adjustPlayerSize(int percent)
{
    MPVPlayer::VideoSizeInfo videoSize=GlobalObjects::mpvplayer->getVideoSizeInfo();
    if(videoSize.width == 0 || videoSize.height == 0)return;
    double aspectRatio;
    if(videoSize.dwidth == 0 || videoSize.dheight == 0)
        aspectRatio = double(videoSize.width)/videoSize.height;
    else
        aspectRatio = double(videoSize.dwidth)/videoSize.dheight;
    double scale = percent / 100.0;
    emit resizePlayer(videoSize.width * scale,videoSize.height * scale,aspectRatio);
}

void PlayerWindow::setPlayTime()
{
    int playTime=process->value()/1000;
    GlobalObjects::playlist->setCurrentPlayTime(playTime);
}

void PlayerWindow::showMessage(const QString &msg)
{
    static_cast<InfoTip *>(playInfo)->showMessage(msg);
    playInfo->move((width()-playInfo->width())/2,height()-controlPanelHeight-playInfo->height()-20);
    playInfo->raise();
}

void PlayerWindow::switchItem(bool prev, const QString &nullMsg)
{
    setPlayTime();
    while(true)
    {
        const PlayListItem *item = GlobalObjects::playlist->playPrevOrNext(prev);
        if (item)
        {
            QFileInfo fi(item->path);
            if(!fi.exists())
            {
                showMessage(tr("File not exist: %0").arg(item->title));
                continue;
            }
            else
            {
                QCoreApplication::processEvents();
                GlobalObjects::danmuPool->reset();
                GlobalObjects::danmuRender->cleanup();
                GlobalObjects::mpvplayer->setMedia(item->path);
                break;
            }
        }
        else
        {
            showMessage(nullMsg);
            break;
        }
    }
}

void PlayerWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(!autoHideControlPanel)return;
    if(isFullscreen)
    {
        setCursor(Qt::ArrowCursor);
        hideCursorTimer.start(hideCursorTimeout);
    }
    if(clickBehavior==1)return;
    const QPoint pos=event->pos();
    if(this->height()-pos.y()<controlPanelHeight+40)
    {
        playControlPanel->show();
        playInfoPanel->show();
        hideCursorTimer.stop();
    }
    else if(pos.y()<infoPanelHeight)
    {
         playInfoPanel->show();
         playControlPanel->hide();
         hideCursorTimer.stop();
    } 
    else
    {
        playInfoPanel->hide();
        playControlPanel->hide();
    }
    if(this->width()-pos.x()<playlistCollapseWidth+20)
    {
        playListCollapseButton->show();
    }
    else
    {
        playListCollapseButton->hide();
    }
}

void PlayerWindow::mouseDoubleClickEvent(QMouseEvent *)
{
    if(dbClickBehaivior==0)
        actFullscreen->trigger();
    else
        actPlayPause->trigger();
}

void PlayerWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button()==Qt::LeftButton)
    {
        if(!danmuSettingPage->isHidden()||!playSettingPage->isHidden())
        {
            if(!danmuSettingPage->underMouse())
                danmuSettingPage->hide();
            if(!playSettingPage->underMouse())
                playSettingPage->hide();
        }
        else
        {
            if(clickBehavior==1)
            {
                if(playControlPanel->isHidden())
                {
                    playControlPanel->show();
                    playInfoPanel->show();
                    playListCollapseButton->show();
                }
                else
                {
                    playInfoPanel->hide();
                    playControlPanel->hide();
                    playListCollapseButton->hide();
                }
            }
            else
            {
                actPlayPause->trigger();
            }
        }
    }
}

void PlayerWindow::resizeEvent(QResizeEvent *)
{
    playControlPanel->setGeometry(0,height()-controlPanelHeight,width(),controlPanelHeight);
    playInfoPanel->setGeometry(0,0,width(),infoPanelHeight);
    playListCollapseButton->setGeometry(width()-playlistCollapseWidth,(height()-playlistCollapseHeight)/2,playlistCollapseWidth,playlistCollapseHeight);
    danmuStatisBar->setGeometry(0,height()-controlPanelHeight-statisBarHeight,width(),statisBarHeight);
    if(!playSettingPage->isHidden())
    {
		QPoint leftTop(width() - playSettingPage->width() - 10, height() - controlPanelHeight - playSettingPage->height() + 20);
        playSettingPage->move(leftTop);
    }
	if (!danmuSettingPage->isHidden())
	{
		QPoint leftTop(width() - danmuSettingPage->width() - 10, height() - controlPanelHeight - danmuSettingPage->height() + 20);
		danmuSettingPage->move(leftTop);
	}
    playerContent->move((width()-playerContent->width())/2,(height()-playerContent->height())/2);
    playInfo->move((width()-playInfo->width())/2,height()-controlPanelHeight-playInfo->height()-20);
}

void PlayerWindow::leaveEvent(QEvent *)
{
    if(autoHideControlPanel)
    {
        QTimer::singleShot(500, [this](){
            this->playControlPanel->hide();
            this->playInfoPanel->hide();
            this->playListCollapseButton->hide();
        });
    }
}

bool PlayerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(watched==danmuSettingPage || watched==playSettingPage)
    {
        if (event->type() == QEvent::Show)
        {
            autoHideControlPanel=false;
            return true;
        }
        else if(event->type() == QEvent::Hide)
        {
            autoHideControlPanel=true;
            return true;
        }
        return false;
    }
    return QMainWindow::eventFilter(watched,event);
}

void PlayerWindow::keyPressEvent(QKeyEvent *event)
{
	int key = event->key();
	switch (key)
	{
    case Qt::Key_Control:
    {
        if(altPressCount>0)
        {
            altPressCount=0;
            doublePressTimer.stop();
        }
        if(!doublePressTimer.isActive())
        {
            doublePressTimer.start(500);
        }
        ctrlPressCount++;
        if(ctrlPressCount==2)
        {
            danmuSwitch->click();
            doublePressTimer.stop();
            ctrlPressCount=0;
        }
        break;
    }
    case Qt::Key_Alt:
    {
        if(ctrlPressCount>0)
        {
            ctrlPressCount=0;
            doublePressTimer.stop();
        }
        if(!doublePressTimer.isActive())
        {
            doublePressTimer.start(500);
        }
        altPressCount++;
        if(altPressCount==2)
        {
            doublePressTimer.stop();
            altPressCount=0;
            const PlayListItem *curItem=GlobalObjects::playlist->getCurrentItem();
            if(curItem && !curItem->animeTitle.isEmpty())
            {
                int curTime=GlobalObjects::mpvplayer->getTime();
                int cmin=curTime/60;
                int cls=curTime-cmin*60;
                QString info=QString("%1:%2 - %3").arg(cmin,2,10,QChar('0')).arg(cls,2,10,QChar('0')).arg(curItem->title);
                QTemporaryFile tmpImg("XXXXXX.jpg");
                if(tmpImg.open())
                {
                    GlobalObjects::mpvplayer->screenshot(tmpImg.fileName());
                    QImage captureImage(tmpImg.fileName());
                    GlobalObjects::library->saveCapture(curItem->animeTitle,curItem->path,info,captureImage);
                    showMessage(tr("Capture has been add to library: %1").arg(curItem->animeTitle));
                }
            }
            else
            {
                act_screenshotSrc->trigger();
            }
        }
        break;
    }
	case Qt::Key_Space:
		actPlayPause->trigger();
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		actFullscreen->trigger();
        break;
    case Qt::Key_Escape:
        if(isFullscreen)
            actFullscreen->trigger();
        break;
	case Qt::Key_Down:
	case Qt::Key_Up:
		QApplication::sendEvent(volume, event);
        showMessage(tr("Volume: %0").arg(volume->value()));
		break;
	case Qt::Key_Right:
		if (event->modifiers() == Qt::ControlModifier)
        {
			GlobalObjects::mpvplayer->frameStep();
            showMessage(tr("Frame Step:Forward"));
        }
		else
            GlobalObjects::mpvplayer->seek(jumpForwardTime, true);
		break;
	case Qt::Key_Left:
		if (event->modifiers() == Qt::ControlModifier)
        {
			GlobalObjects::mpvplayer->frameStep(false);
            showMessage(tr("Frame Step:Backward"));
        }
		else
            GlobalObjects::mpvplayer->seek(-jumpBackwardTime, true);
		break;
    case Qt::Key_PageUp:
        actPrev->trigger();
        break;
    case Qt::Key_PageDown:
        actNext->trigger();
        break;
	default:
		QMainWindow::keyPressEvent(event);
    }
}

void PlayerWindow::contextMenuEvent(QContextMenuEvent *)
{
    currentDanmu=GlobalObjects::danmuRender->danmuAt(mapFromGlobal(QCursor::pos()));
    if(currentDanmu.isNull())return;
    ctx_Text->setText(currentDanmu->text);
    contexMenu->exec(QCursor::pos());
}

void PlayerWindow::closeEvent(QCloseEvent *)
{
    setPlayTime();
    GlobalObjects::appSetting->beginGroup("Play");
    GlobalObjects::appSetting->setValue("HideDanmu",danmuSwitch->isChecked());
    GlobalObjects::appSetting->setValue("HideRolling",hideRollingDanmu->isChecked());
    GlobalObjects::appSetting->setValue("HideTop",hideTopDanmu->isChecked());
    GlobalObjects::appSetting->setValue("HideBottom",hideBottomDanmu->isChecked());
    GlobalObjects::appSetting->setValue("DanmuSpeed",speedSlider->value());
    GlobalObjects::appSetting->setValue("DanmuAlpha",alphaSlider->value());
    GlobalObjects::appSetting->setValue("DanmuStroke",strokeWidthSlider->value());
    GlobalObjects::appSetting->setValue("DanmuFontSize",fontSizeSlider->value());
    GlobalObjects::appSetting->setValue("DanmuBold",bold->isChecked());
    GlobalObjects::appSetting->setValue("BottomSubProtect",bottomSubtitleProtect->isChecked());
    GlobalObjects::appSetting->setValue("TopSubProtect",topSubtitleProtect->isChecked());
    GlobalObjects::appSetting->setValue("RandomSize",randomSize->isChecked());
    GlobalObjects::appSetting->setValue("DanmuFont",fontFamilyCombo->currentFont().family());
    GlobalObjects::appSetting->setValue("VidoeAspectRatio",aspectRatioCombo->currentIndex());
    GlobalObjects::appSetting->setValue("PlaySpeed",playSpeedCombo->currentIndex());
    GlobalObjects::appSetting->setValue("WindowSize",windowSizeGroup->actions().indexOf(windowSizeGroup->checkedAction()));
    GlobalObjects::appSetting->setValue("StayOnTop",stayOnTopGroup->actions().indexOf(stayOnTopGroup->checkedAction()));
    GlobalObjects::appSetting->setValue("Volume",volume->value());
    GlobalObjects::appSetting->setValue("Mute",GlobalObjects::mpvplayer->getMute());
    GlobalObjects::appSetting->setValue("MaxCount",maxDanmuCount->value());
    GlobalObjects::appSetting->setValue("Dense",denseLevel->currentIndex());
    GlobalObjects::appSetting->setValue("EnableMerge",enableMerge->isChecked());
	GlobalObjects::appSetting->setValue("EnableAnalyze", enableAnalyze->isChecked());
    GlobalObjects::appSetting->setValue("EnlargeMerged",enlargeMerged->isChecked());
    GlobalObjects::appSetting->setValue("MergeInterval",mergeInterval->value());
    GlobalObjects::appSetting->setValue("MaxDiffCount",contentSimCount->value());
    GlobalObjects::appSetting->setValue("MinSimCount",minMergeCount->value());
    GlobalObjects::appSetting->setValue("MergeCountTip",mergeCountTipPos->currentIndex());
    GlobalObjects::appSetting->endGroup();
}

void PlayerWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void PlayerWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls(event->mimeData()->urls());
    QStringList fileList,dirList;
    for(QUrl &url:urls)
    {
        if(url.isLocalFile())
        {
            QFileInfo fi(url.toLocalFile());
            if(fi.isFile())
            {
                if(GlobalObjects::mpvplayer->videoFileFormats.contains("*."+fi.suffix().toLower()))
                {
                    fileList.append(fi.filePath());
                }
                else if(GlobalObjects::mpvplayer->subtitleFormats.contains("*."+fi.suffix().toLower()))
                {
                    if(GlobalObjects::playlist->getCurrentItem()!=nullptr)
                    {
                        GlobalObjects::mpvplayer->addSubtitle(fi.filePath());
                        showMessage(tr("Subtitle has been added"));
                    }
                }
                else if("xml"==fi.suffix())
                {
                    QList<DanmuComment *> tmplist;
                    LocalProvider::LoadXmlDanmuFile(fi.filePath(),tmplist);
                    DanmuSourceInfo sourceInfo;
                    sourceInfo.delay=0;
                    sourceInfo.name=fi.fileName();
                    sourceInfo.show=true;
                    sourceInfo.url=fi.filePath();
                    sourceInfo.count=tmplist.count();
                    Pool *pool=GlobalObjects::danmuPool->getPool();
                    if(pool->addSource(sourceInfo,tmplist,true)>=0)
                        showMessage(tr("Danmu has been added"));
                    else
                    {
                        qDeleteAll(tmplist);
                        showMessage(tr("Add Faied: Pool is busy"));
                    }
                }
            }
            else
            {
                dirList.append(fi.filePath());
            }
        }
    }
    if(!fileList.isEmpty())
    {
        GlobalObjects::playlist->addItems(fileList,QModelIndex());
        if(fileList.size()==1)
        {
            const PlayListItem *curItem = GlobalObjects::playlist->setCurrentItem(fileList.first());
            if (curItem)
            {
                GlobalObjects::danmuPool->reset();
                GlobalObjects::danmuRender->cleanup();
                GlobalObjects::mpvplayer->setMedia(curItem->path);
            }
        }
    }
    for(QString dir:dirList)
    {
        GlobalObjects::playlist->addFolder(dir,QModelIndex());
    }
}

void PlayerWindow::wheelEvent(QWheelEvent *event)
{
	int val = volume->value();
	if (val > 0 && val < 100 || val == 0 && event->angleDelta().y()>0 || val == 100 && event->angleDelta().y() < 0)
	{
		static bool inProcess = false;
		if (!inProcess)
		{
			inProcess = true;
			QApplication::sendEvent(volume, event);
			inProcess = false;
		}
	}
    showMessage(tr("Volume: %0").arg(volume->value()));
}
