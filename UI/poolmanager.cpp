#include "poolmanager.h"
#include <QTreeView>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QStackedLayout>
#include <QCheckBox>
#include <QLabel>
#include <QAction>
#include <QSortFilterProxyModel>
#include <QApplication>
#include "Play/Danmu/Manager/managermodel.h"
#include "Play/Danmu/Manager/danmumanager.h"
#include "Play/Danmu/Manager/pool.h"
#include "Play/Playlist/playlistitem.h"
#include "Play/Playlist/playlist.h"
#include "timelineedit.h"
#include "adddanmu.h"
#include "addpool.h"
#include "danmuview.h"
#include "inputdialog.h"
#include "globalobjects.h"

PoolManager::PoolManager(QWidget *parent) : CFramelessDialog(tr("Danmu Pool Manager"),parent)
{
    setFont(QFont("Microsoft Yahei UI",10));
    QTreeView *poolView=new QTreeView(this);
    poolView->setSelectionMode(QAbstractItemView::SingleSelection);
    poolView->setFont(this->font());
    poolView->setAlternatingRowColors(true);

    DanmuManagerModel *managerModel=new DanmuManagerModel(this);
    PoolSortProxyModel *proxyModel=new PoolSortProxyModel(this);
    proxyModel->setSourceModel(managerModel);
    proxyModel->setFilterKeyColumn(0);
    proxyModel->setRecursiveFilteringEnabled(true);
    poolView->setModel(proxyModel);
    poolView->setSortingEnabled(true);
    poolView->setContextMenuPolicy(Qt::ActionsContextMenu);

    QLabel *stateLabel=new QLabel(this);
    stateLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Minimum);
    QObject::connect(GlobalObjects::danmuManager,&DanmuManager::workerStateMessage,stateLabel,[stateLabel,managerModel](const QString &msg){
       if(msg=="Done")
           stateLabel->setText(tr("Pool: %1 Danmu: %2").arg(managerModel->totalPoolCount()).arg(managerModel->totalDanmuCount()));
       else
           stateLabel->setText(msg);
    });

    QAction *act_editTimeLine=new QAction(tr("Edit TimeLine"),this);
    QObject::connect(act_editTimeLine,&QAction::triggered,this,[this,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        DanmuPoolSourceNode *srcNode=managerModel->getSourceNode(proxyModel->mapToSource(indexList.first()));
        if(!srcNode)return;
        QList<SimpleDanmuInfo> simpleDanmuList;
        Pool *pool=GlobalObjects::danmuManager->getPool(srcNode->parent->idInfo);
        if(pool)
        {
            pool->exportSimpleInfo(srcNode->srcId,simpleDanmuList);
            DanmuSourceInfo srcInfo(pool->sources()[srcNode->srcId]);
            TimelineEdit timeLineEdit(&srcInfo,simpleDanmuList,this);
            if(QDialog::Accepted==timeLineEdit.exec())
            {
                srcNode->setTimeline(srcInfo);
                pool->setTimeline(srcNode->srcId,srcInfo.timelineInfo);
            }
        }

    });
    QAction *act_addWebSource=new QAction(tr("Add Web Source"),this);
    QObject::connect(act_addWebSource,&QAction::triggered,this,[this,stateLabel,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        DanmuPoolNode *poolNode=managerModel->getPoolNode(proxyModel->mapToSource(indexList.first()));
        if(!poolNode)return;
        PlayListItem item;
        item.title=poolNode->title;
        item.animeTitle=poolNode->parent->title;
        QMap<QString,DanmuPoolNode *> poolNodeMap;
        QStringList poolTitles;
        for(auto node:*poolNode->parent->children)
        {
            poolNodeMap.insert(node->title,node);
            poolTitles<<node->title;
        }
        std::sort(poolTitles.begin(),poolTitles.end());
        AddDanmu addDanmuDialog(&item, this,false,poolTitles);
        if(QDialog::Accepted==addDanmuDialog.exec())
        {
            poolView->setEnabled(false);
            this->showBusyState(true);
            int i = 0;
            //bool hasCurPlaying=false;
            for(auto iter=addDanmuDialog.selectedDanmuList.begin();iter!=addDanmuDialog.selectedDanmuList.end();++iter)
            {
                DanmuPoolNode *curNode=poolNodeMap.value(addDanmuDialog.danmuToPoolList.at(i++));
                Q_ASSERT(curNode);
                Pool *pool=GlobalObjects::danmuManager->getPool(curNode->idInfo);
                Q_ASSERT(pool);
                DanmuSourceInfo &sourceInfo=(*iter).first;
                QList<DanmuComment *> &danmuList=(*iter).second;
                int srcId=pool->addSource(sourceInfo,danmuList,true);
                if(srcId<0)
                {
                    showMessage(tr("Add %1 Failed").arg(sourceInfo.name),1);
                    qDeleteAll(danmuList);
                    continue;
                }
                DanmuPoolSourceNode *sourceNode(nullptr);
                for(auto n:*curNode->children)
                {
                    DanmuPoolSourceNode *srcNode=static_cast<DanmuPoolSourceNode *>(n);
                    if(srcNode->idInfo==sourceInfo.url)
                    {
                        sourceNode=srcNode;
                        break;
                    }
                }
                if(sourceNode)
                {
                    managerModel->addSrcNode(curNode,nullptr);
                }
                else
                {
                    sourceNode=new DanmuPoolSourceNode();
                    sourceNode->title=sourceInfo.name;
                    sourceNode->srcId=srcId;
                    sourceNode->delay=sourceInfo.delay;
                    sourceNode->idInfo=sourceInfo.url;
                    sourceNode->danmuCount=sourceInfo.count;
                    managerModel->addSrcNode(curNode,sourceNode);
                }
            }
            //if(hasCurPlaying) GlobalObjects::danmuPool->resetModel();
            stateLabel->setText(tr("Pool: %1 Danmu: %2").arg(managerModel->totalPoolCount()).arg(managerModel->totalDanmuCount()));
            poolView->setEnabled(true);
            this->showBusyState(false);
        }
    });

    QAction *act_pastePoolCode=new QAction(tr("Paste Danmu Pool Code"),this);
    QObject::connect(act_pastePoolCode,&QAction::triggered,this,[this,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        DanmuPoolNode *poolNode=managerModel->getPoolNode(proxyModel->mapToSource(indexList.first()));
        if(!poolNode)return;
        QClipboard *cb = QApplication::clipboard();
        QString code(cb->text());
        if(code.isEmpty() ||
                (!code.startsWith("kikoplay:pool=") &&
                !code.startsWith("kikoplay:anime="))) return;
        poolView->setEnabled(false);
        this->showBusyState(true);
        Pool *pool = GlobalObjects::danmuManager->getPool(poolNode->idInfo,false);
        bool ret = pool->addPoolCode(
                    code.mid(code.startsWith("kikoplay:pool=")?14:15),
                    code.startsWith("kikoplay:anime="));
        if(ret)
        {
            for(const DanmuSourceInfo &sourceInfo:pool->sources())
            {
                bool isNewSource=true;
                for(auto n:*poolNode->children)
                {
                    DanmuPoolSourceNode *srcNode=static_cast<DanmuPoolSourceNode *>(n);
                    if(srcNode->idInfo==sourceInfo.url)
                    {
                        isNewSource=false;
                        break;
                    }
                }
                if(isNewSource)
                {
                    DanmuPoolSourceNode *sourceNode=new DanmuPoolSourceNode();
                    sourceNode->title=sourceInfo.name;
                    sourceNode->srcId=sourceInfo.id;
                    sourceNode->delay=sourceInfo.delay;
                    sourceNode->idInfo=sourceInfo.url;
                    sourceNode->danmuCount=sourceInfo.count;
                    managerModel->addSrcNode(poolNode,sourceNode);
                }
            }
        }
        poolView->setEnabled(true);
        this->showBusyState(false);
        this->showMessage(ret?tr("Code Added"):tr("Code Error"),ret?0:1);
    });
    QAction *act_copyPoolCode=new QAction(tr("Copy Danmu Pool Code"),this);
    QObject::connect(act_copyPoolCode,&QAction::triggered,this,[this,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        DanmuPoolNode *poolNode=managerModel->getPoolNode(proxyModel->mapToSource(indexList.first()));
        if(!poolNode)return;
        Pool *pool = GlobalObjects::danmuManager->getPool(poolNode->idInfo,false);
        QString code(pool->getPoolCode());
        if(code.isEmpty())
        {
            showMessage(tr("No Danmu Source to Share"),1);
        }
        else
        {
            QClipboard *cb = QApplication::clipboard();
            cb->setText("kikoplay:pool="+code);
            showMessage(tr("Pool Code has been Copied to Clipboard"));
        }
    });

    QAction *act_addPool=new QAction(tr("Add Pool"),this);
    QObject::connect(act_addPool,&QAction::triggered,this,[this,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        QString animeTitle=managerModel->getAnime(proxyModel->mapToSource(indexList.first()));;
        AddPool addPool(this,animeTitle);
        if(QDialog::Accepted==addPool.exec())
        {
            QString pid(GlobalObjects::danmuManager->createPool(addPool.animeTitle,addPool.epTitle));
            managerModel->addPoolNode(addPool.animeTitle,addPool.epTitle,pid);
        }

    });

    QAction *act_renamePool=new QAction(tr("Rename Pool"),this);
    QObject::connect(act_renamePool,&QAction::triggered,this,[this,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        DanmuPoolNode *poolNode=managerModel->getPoolNode(proxyModel->mapToSource(indexList.first()));
        if(!poolNode)return;
        AddPool addPool(this,poolNode->parent->title,poolNode->title);
        if(QDialog::Accepted==addPool.exec())
        {
            QString opid(poolNode->idInfo);
            QString npid = GlobalObjects::danmuManager->renamePool(opid,addPool.animeTitle,addPool.epTitle);
            if(npid.isEmpty())
            {
                showMessage(tr("Rename Failed, Try Again?"),1);
                return;
            }
            if(opid==npid) return;
            managerModel->renamePoolNode(poolNode,addPool.animeTitle,addPool.epTitle,npid);
            GlobalObjects::playlist->renameItemPoolId(opid,npid,addPool.animeTitle,addPool.epTitle);
        }

    });
    QAction *actView=new QAction(tr("View Danmu"),this);
    QObject::connect(actView,&QAction::triggered,this,[this,managerModel,poolView,proxyModel](){
        QModelIndexList indexList = poolView->selectionModel()->selectedRows();
        if(indexList.size()==0)return;
        DanmuPoolNode *poolNode=managerModel->getPoolNode(proxyModel->mapToSource(indexList.first()));
        if(!poolNode)return;
        Pool *pool=GlobalObjects::danmuManager->getPool(poolNode->idInfo);
        DanmuView view(&pool->comments(),this);
        view.exec();
    });

    poolView->addAction(actView);
    poolView->addAction(act_addWebSource);
    poolView->addAction(act_editTimeLine);
    QAction *act_separator0=new QAction(this);
    act_separator0->setSeparator(true);
    poolView->addAction(act_separator0);

    poolView->addAction(act_addPool);
    poolView->addAction(act_renamePool);

    QAction *act_separator1=new QAction(this);
    act_separator1->setSeparator(true);
    poolView->addAction(act_separator1);

    poolView->addAction(act_copyPoolCode);
    poolView->addAction(act_pastePoolCode);

    QPushButton *cancel=new QPushButton(tr("Cancel"),this);
    cancel->hide();

    QWidget *exportPage=new QWidget(this);
    QHBoxLayout *exportHLayout=new QHBoxLayout(exportPage);
    exportHLayout->setContentsMargins(0,0,0,0);
    QCheckBox *exportKdFile=new QCheckBox(tr("Export KikoPlay Format"), exportPage);
    QCheckBox *useTimelineCheck=new QCheckBox(tr("Apply delay and timeline info"), exportPage);
    useTimelineCheck->setChecked(true);
    QCheckBox *useBlockRule=new QCheckBox(tr("Apply block rules"),exportPage);
    QPushButton *exportConfirm=new QPushButton(tr("Export"),exportPage);
    exportHLayout->addWidget(exportKdFile);
    exportHLayout->addWidget(useTimelineCheck);
    exportHLayout->addWidget(useBlockRule);
    exportHLayout->addStretch(1);
    exportHLayout->addWidget(exportConfirm);

    QObject::connect(exportKdFile,&QCheckBox::stateChanged, this,[useBlockRule,useTimelineCheck](int state){
       useBlockRule->setEnabled(state!=Qt::Checked);
       useTimelineCheck->setEnabled(state!=Qt::Checked);
    });

    QObject::connect(exportConfirm,&QPushButton::clicked,[this,poolView,managerModel,exportConfirm,cancel,useTimelineCheck,useBlockRule,exportKdFile](){
        if(!managerModel->hasSelected())return;
        QString directory = QFileDialog::getExistingDirectory(this,
                                    tr("Select folder"), "",
                                    QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly);
        if (directory.isEmpty())return;
        this->showBusyState(true);
        exportConfirm->setText(tr("Exporting..."));
        exportConfirm->setEnabled(false);
        poolView->setEnabled(false);
        cancel->setEnabled(false);
        useTimelineCheck->setEnabled(false);
        useBlockRule->setEnabled(false);
        if(exportKdFile->isChecked())
        {
            InputDialog inputDialog(tr("Set Comment"),tr("Comment(Optional)"),"",true,this);
            inputDialog.exec();
            managerModel->exportKdFile(directory, inputDialog.text);
        }
        else
        {
            managerModel->exportPool(directory,useTimelineCheck->isChecked(),useBlockRule->isChecked());
        }
        this->showBusyState(false);
        exportConfirm->setText(tr("Export"));
        exportConfirm->setEnabled(true);
        poolView->setEnabled(true);
        cancel->setEnabled(true);
        useTimelineCheck->setEnabled(true);
        useBlockRule->setEnabled(true);
    });

    QWidget *deletePage=new QWidget(this);
    QHBoxLayout *deleteHLayout=new QHBoxLayout(deletePage);
    deleteHLayout->setContentsMargins(0,0,0,0);
    QLabel *deleteTipLabel=new QLabel(tr("Check the items to delete"),deletePage);
    QPushButton *deleteConfirm=new QPushButton(tr("Delete"),deletePage);
    deleteHLayout->addWidget(deleteTipLabel);
    deleteHLayout->addStretch(1);
    deleteHLayout->addWidget(deleteConfirm);
    QObject::connect(deleteConfirm,&QPushButton::clicked,[this,stateLabel,poolView,managerModel,deleteConfirm,cancel](){
        if(!managerModel->hasSelected())return;
        this->showBusyState(true);
        deleteConfirm->setText(tr("Deleting..."));
        deleteConfirm->setEnabled(false);
        poolView->setEnabled(false);
        cancel->setEnabled(false);
        managerModel->deletePool();
        this->showBusyState(false);
        deleteConfirm->setText(tr("Delete"));
        deleteConfirm->setEnabled(true);
        poolView->setEnabled(true);
        cancel->setEnabled(true);
        stateLabel->setText(tr("Pool: %1 Danmu: %2").arg(managerModel->totalPoolCount()).arg(managerModel->totalDanmuCount()));
    });

    QWidget *updatePage=new QWidget(this);
    QHBoxLayout *updateHLayout=new QHBoxLayout(updatePage);
    updateHLayout->setContentsMargins(0,0,0,0);
    QLabel *updateTipLabel=new QLabel(tr("Check the items to update danmu"),updatePage);
    QPushButton *updateConfirm=new QPushButton(tr("Update"),updatePage);
    updateHLayout->addWidget(updateTipLabel);
    updateHLayout->addStretch(1);
    updateHLayout->addWidget(updateConfirm);
    QObject::connect(updateConfirm,&QPushButton::clicked,[this,stateLabel,poolView,managerModel,updateConfirm,cancel](){
        if(!managerModel->hasSelected())return;
        this->showBusyState(true);
        updateConfirm->setText(tr("Updating..."));
        cancel->setEnabled(false);
        poolView->setEnabled(false);
        updateConfirm->setEnabled(false);
        managerModel->updatePool();
        this->showBusyState(false);
        updateConfirm->setText(tr("Update"));
        poolView->setEnabled(true);
        updateConfirm->setEnabled(true);
        cancel->setEnabled(true);
        stateLabel->setText(tr("Pool: %1 Danmu: %2").arg(managerModel->totalPoolCount()).arg(managerModel->totalDanmuCount()));
    });

    QWidget *mainPage=new QWidget(this);
    QPushButton *importKdFile=new QPushButton(tr("Import"), mainPage);
    QPushButton *exportPool=new QPushButton(tr("Export Pool(s)"),mainPage);
    QPushButton *addDanmuPool=new QPushButton(tr("Add"), mainPage);
    QPushButton *deletePool=new QPushButton(tr("Delete Pool(s)"),mainPage);
    QPushButton *updatePool=new QPushButton(tr("Update Pool(s)"),mainPage);
    QLineEdit *searchEdit=new QLineEdit(this);
    searchEdit->setPlaceholderText(tr("Search"));
    searchEdit->setMinimumWidth(150*logicalDpiX()/96);
    searchEdit->setClearButtonEnabled(true);
    QObject::connect(searchEdit,&QLineEdit::textChanged,[proxyModel](const QString &keyword){
        proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxyModel->setFilterRegExp(keyword);
    });
    QHBoxLayout *mainHLayout=new QHBoxLayout(mainPage);
    mainHLayout->setContentsMargins(0,0,0,0);
    mainHLayout->addWidget(importKdFile);
    mainHLayout->addWidget(exportPool);
    mainHLayout->addWidget(addDanmuPool);
    mainHLayout->addWidget(deletePool);
    mainHLayout->addWidget(updatePool);
    mainHLayout->addStretch(1);
    mainHLayout->addWidget(searchEdit);

    QStackedLayout *funcStackLayout=new QStackedLayout();
    funcStackLayout->setContentsMargins(0,0,0,0);
    funcStackLayout->addWidget(mainPage);
    funcStackLayout->addWidget(exportPage);
    funcStackLayout->addWidget(deletePage);
    funcStackLayout->addWidget(updatePage);

    QObject::connect(cancel,&QPushButton::clicked,[funcStackLayout](){
       funcStackLayout->setCurrentIndex(0);
    });
    QObject::connect(importKdFile,&QPushButton::clicked,[this,poolView,managerModel,
                     importKdFile,exportPool,deletePool,updatePool,addDanmuPool](){
        QStringList files = QFileDialog::getOpenFileNames(this,tr("Select KikoPlay Danmu Pool File"),"","KikoPlay Danmu Pool File(*.kd)");
        if(files.isEmpty()) return;
        this->showBusyState(true);
        importKdFile->setText(tr("Importing..."));
        importKdFile->setEnabled(false);
        poolView->setEnabled(false);
        exportPool->setEnabled(false);
        deletePool->setEnabled(false);
        updatePool->setEnabled(false);
        addDanmuPool->setEnabled(false);
        bool refreshList=false;
        for(auto &file:files)
        {
            if(GlobalObjects::danmuManager->importKdFile(file, this)>0)
                refreshList=true;
        }
        if(refreshList) managerModel->refreshList();
        this->showBusyState(false);
        importKdFile->setText(tr("Import"));
        poolView->setEnabled(true);
        importKdFile->setEnabled(true);
        exportPool->setEnabled(true);
        deletePool->setEnabled(true);
        addDanmuPool->setEnabled(true);
        updatePool->setEnabled(true);
    });
    QObject::connect(exportPool,&QPushButton::clicked,[cancel, exportHLayout,funcStackLayout](){
        exportHLayout->addWidget(cancel);
        cancel->show();
        funcStackLayout->setCurrentIndex(1);
    });
    QObject::connect(addDanmuPool,&QPushButton::clicked,[this,managerModel](){
        AddPool addPool(this);
        if(QDialog::Accepted==addPool.exec())
        {
            QString pid(GlobalObjects::danmuManager->createPool(addPool.animeTitle,addPool.epTitle));
            managerModel->addPoolNode(addPool.animeTitle,addPool.epTitle,pid);
        }
    });
    QObject::connect(deletePool,&QPushButton::clicked,[cancel, deleteHLayout,funcStackLayout](){
        deleteHLayout->addWidget(cancel);
        cancel->show();
        funcStackLayout->setCurrentIndex(2);
    });
    QObject::connect(updatePool,&QPushButton::clicked,[cancel, updateHLayout,funcStackLayout](){
        updateHLayout->addWidget(cancel);
        cancel->show();
        funcStackLayout->setCurrentIndex(3);
    }); 

    QGridLayout *managerGLayout=new QGridLayout(this);
    managerGLayout->addLayout(funcStackLayout,0,0);
    managerGLayout->addWidget(poolView,1,0);
    managerGLayout->addWidget(stateLabel,2,0);
    managerGLayout->setRowStretch(1,1);
    managerGLayout->setColumnStretch(0,1);
    managerGLayout->setContentsMargins(0, 0, 0, 0);

    resize(620*logicalDpiX()/96, 420*logicalDpiY()/96);
    QHeaderView *poolHeader = poolView->header();
    poolHeader->setFont(this->font());
    poolHeader->resizeSection(0, 260*logicalDpiX()/96); //Pool
    poolHeader->resizeSection(1, 150*logicalDpiX()/96); //Source
    poolHeader->resizeSection(2, 50*logicalDpiX()/96); //Delay
    poolHeader->resizeSection(3, 120*logicalDpiX()/96); //Count


    QTimer::singleShot(0,[this,managerModel,importKdFile,addDanmuPool](){
        this->showBusyState(true);
        importKdFile->setEnabled(false);
        addDanmuPool->setEnabled(false);
        managerModel->refreshList();
        this->showBusyState(false);
        importKdFile->setEnabled(true);
        addDanmuPool->setEnabled(true);
    });
}
