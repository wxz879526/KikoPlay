﻿#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include "playlistitem.h"
struct MatchInfo;
class PlayListPrivate;
class MatchWorker : public QObject
{
    Q_OBJECT
public:
    explicit MatchWorker(QObject *parent = nullptr):QObject(parent){}
    void match(const QList<PlayListItem *> &items);
signals:
    void matchDown(const QList<PlayListItem *> &matchedItems);
    void message(const QString &msg,int flag);
};

class PlayList : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit PlayList(QObject *parent = nullptr);
    ~PlayList();
    enum LoopMode
    {
        NO_Loop_One,
        NO_Loop_All,
        Loop_One,
        Loop_All,
        Random
    };
    const int maxRecentItems=4;
    const PlayListItem *getCurrentItem() const;
    QModelIndex getCurrentIndex() const;
    inline const PlayListItem *getItem(const QModelIndex &index){return index.isValid()?static_cast<PlayListItem*>(index.internalPointer()):nullptr; }
    QList<const PlayListItem *> getSiblings(const PlayListItem *item);
    LoopMode getLoopMode() const;
    bool canPaste() const;
    QList<QPair<QString,QString> > &recent();

signals:
    void currentInvaild();
    void currentMatchChanged(const QString &pid);
    void message(const QString &msg,int flag);
    void recentItemsUpdated();
    void matchStatusChanged(bool on);
public slots :
    int addItems(QStringList &items, QModelIndex parent);
    int addFolder(QString folderStr, QModelIndex parent);
    QModelIndex addCollection(QModelIndex parent,QString title);

    void deleteItems(const QModelIndexList &deleteIndexes);
    void clear();

    void sortItems(const QModelIndex &parent,bool ascendingOrder);
    void sortAllItems(bool ascendingOrder);

    void cutItems(const QModelIndexList &cutIndexes);
    void pasteItems(QModelIndex parent);
    void moveUpDown(QModelIndex index,bool up);
    void switchBgmCollection(const QModelIndex &index);
    void autoMoveToBgmCollection(const QModelIndex &index);

    const PlayListItem *setCurrentItem(const QModelIndex &index,bool playChild=true);
    const PlayListItem *setCurrentItem(const QString &path);
	void cleanCurrentItem();
    const PlayListItem *playPrevOrNext(bool prev);
    void setLoopMode(LoopMode newMode);
    void checkCurrentItem(PlayListItem *itemDeleted);
    void setAutoMatch(bool on);
    void matchItems(const QModelIndexList &matchIndexes);
    void matchIndex(QModelIndex &index,MatchInfo *matchInfo);
    void updateItemsDanmu(const QModelIndexList &itemIndexes);
    void setCurrentPlayTime(int playTime);
    QModelIndex mergeItems(const QModelIndexList &mergeIndexes);
    void exportDanmuItems(const QModelIndexList &exportIndexes);

    
    void dumpJsonPlaylist(QJsonDocument &jsonDoc,QHash<QString,QString> &mediaHash);
    void updatePlayTime(const QString &path, int time, int state);
    void renameItemPoolId(const QString &opid, const QString &npid, const QString &animeTitle, const QString &epTitle);

    // QAbstractItemModel interface
public:
    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
    virtual QModelIndex parent(const QModelIndex &child) const;
    virtual int rowCount(const QModelIndex &parent) const;
    inline virtual int columnCount(const QModelIndex &) const {return 1;}
    virtual QVariant data(const QModelIndex &index, int role) const;
    inline virtual Qt::DropActions supportedDropActions() const{return Qt::MoveAction;}
    virtual Qt::ItemFlags flags(const QModelIndex &index) const;
    inline virtual QStringList mimeTypes() const { return QStringList()<<"application/x-kikoplaylistitem"; }
    virtual QMimeData *mimeData(const QModelIndexList &indexes) const;
    virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role);
private:
    PlayListPrivate * const d_ptr;
    MatchWorker *matchWorker;
    Q_DECLARE_PRIVATE(PlayList)
    Q_DISABLE_COPY(PlayList)

};
#endif // PLAYLIST_H
