#include "autodownloadmanager.h"
#include "Script/scriptmanager.h"
#include "globalobjects.h"
#include <QDateTime>
#include <QBrush>
#include <QFile>
#include <QXmlStreamReader>
#define DownloadRuleIdRole Qt::UserRole+1
AutoDownloadManager::AutoDownloadManager(QObject *parent) : QAbstractItemModel(parent), ruleChanged(false)
{
    ruleFileName=GlobalObjects::dataPath+"downloadRules.xml";
    logModel = new LogModel(this);
    urlModel = new URLModel(this);
    QObject::connect(urlModel, &URLModel::ruleChanged, this, [this](){
       ruleChanged=true;
    });
    qRegisterMetaType<QSharedPointer<DownloadRule> >("QSharedPointer<DownloadRule>");
    qRegisterMetaType<DownloadRuleLog>("DownloadRuleLog");
    qRegisterMetaType<QList<QSharedPointer<DownloadRule> > >("QList<QSharedPointer<DownloadRule> >");
    checker=new DownloadRuleChecker();
    checker->moveToThread(&checkerThread);
    QObject::connect(&checkerThread, &QThread::finished, checker, &QObject::deleteLater);
    QObject::connect(checker, &DownloadRuleChecker::updateState, this, [this](QSharedPointer<DownloadRule> rule){
        int row = downloadRules.indexOf(rule);
        if(row==-1) return;
        QModelIndex itemIndex(createIndex(row, 0));
        emit dataChanged(itemIndex,itemIndex.sibling(itemIndex.row(),headers.count()-1));
        ruleChanged=true;
    });
    QObject::connect(this, &AutoDownloadManager::checkRules, checker, &DownloadRuleChecker::checkRules);
    QObject::connect(checker, &DownloadRuleChecker::log, this, [this](const DownloadRuleLog &log){
        DownloadRule *rule = ruleIdHash.value(log.ruleId, nullptr);
        if(!rule) return;
        if(log.type==1)
        {
            if(rule->download)
            {
                emit addTask(log.addition, rule->filePath);
            }
            else
            {
                if(urlModel->getRule()==rule) urlModel->addURI(log.content, log.addition);
                else rule->discoveredURLs.append(QPair<QString,QString>(log.content, log.addition));
                ruleChanged=true;
            }
        }
        logModel->addLog(log);
    });
    checkerThread.setObjectName(QStringLiteral("checkerThread"));
    checkerThread.start(QThread::NormalPriority);
    timer = new QTimer(this);
    QObject::connect(timer,&QTimer::timeout, this, [this](){
        QList<QSharedPointer<DownloadRule> > rules;
        for (auto &rule: downloadRules)
        {
            if(rule->state==0)
            {
                qint64 time = QDateTime::currentDateTime().toSecsSinceEpoch();
                if(time - rule->lastCheckTime < rule->searchInterval*60) continue;
                rules.append(rule);
            }
        }
        if(rules.count()>0) emit this->checkRules(rules);
    });
    loadRules();
    timer->start(60*1000);
}

AutoDownloadManager::~AutoDownloadManager()
{
    if(ruleChanged) saveRules();
    checkerThread.quit();
    checkerThread.wait();
}

void AutoDownloadManager::addRule(DownloadRule *rule)
{
    int maxId=0;
    for(auto &r:downloadRules)
    {
        if(r->id>=maxId) maxId=r->id+1;
    }
    rule->id = maxId;
    int insertPosition = downloadRules.count();
    beginInsertRows(QModelIndex(), insertPosition, insertPosition);
    downloadRules.append(QSharedPointer<DownloadRule>(rule));
    ruleIdHash.insert(rule->id, rule);
    endInsertRows();
    saveRules();
    ruleChanged=false;
}

void AutoDownloadManager::removeRules(const QModelIndexList &removeIndexes)
{
    QList<int> rows;
    for(const QModelIndex &index : removeIndexes)
    {
        if (index.isValid())
        {
            int row=index.row();
            rows.append(row);
        }
    }
    std::sort(rows.rbegin(),rows.rend());
    for(auto iter=rows.begin();iter!=rows.end();++iter)
    {
        auto &rule=downloadRules.at(*iter);
        if(rule->state==1) continue;
        logModel->removeLog(rule->id);
        if(urlModel->getRule()==rule) urlModel->setRule(nullptr);
        beginRemoveRows(QModelIndex(), *iter, *iter);
        ruleIdHash.remove(rule->id);
        downloadRules.removeAt(*iter);
        endRemoveRows();
    }
    saveRules();
    ruleChanged=false;
}

DownloadRule *AutoDownloadManager::getRule(const QModelIndex &index, bool lockForEdit)
{
    if(!index.isValid()) return nullptr;
    DownloadRule *rule=downloadRules.at(index.row()).get();
    if(!lockForEdit) return rule;
    if(!rule->lock.tryLock())
    {
        return nullptr;
    }
    if(rule->state!=2) rule->state=3;
    rule->lock.unlock();
    return rule;
}

void AutoDownloadManager::applyRule(DownloadRule *rule, bool ruleModified)
{
    if(!rule) return;
    if(rule->state==3) rule->state=0;
    if(ruleModified) ruleChanged=true;
    return;
}

void AutoDownloadManager::stopRules(const QModelIndexList &indexes)
{
    for(auto &index:indexes)
    {
        if(!index.isValid()) continue;
        auto &rule=downloadRules.at(index.row());
        if(rule->state==2) continue;
        if(!rule->lock.tryLock()) continue;
        rule->state=2;
        rule->lock.unlock();
        emit dataChanged(index,index);
        ruleChanged=true;
    }
}

void AutoDownloadManager::startRules(const QModelIndexList &indexes)
{
    for(auto &index:indexes)
    {
        if(!index.isValid()) continue;
        auto &rule=downloadRules.at(index.row());
        if(rule->state==0) continue;
        rule->state=0;
        emit dataChanged(index,index);
        ruleChanged=true;
    }
}

void AutoDownloadManager::checkAtOnce(const QModelIndexList &indexes)
{
    QList<QSharedPointer<DownloadRule> > rules;
    for(auto &index:indexes)
    {
        if(!index.isValid()) continue;
        auto &rule=downloadRules.at(index.row());
        if(rule->state==0) rules.append(rule);
    }
    if(rules.count()>0) emit checkRules(rules);
}

void AutoDownloadManager::loadRules()
{
    QFile ruleFile(ruleFileName);
    bool ret=ruleFile.open(QIODevice::ReadOnly|QIODevice::Text);
    if(!ret) return;
    beginResetModel();
    QXmlStreamReader reader(&ruleFile);
    DownloadRule *curRule(nullptr);
    while(!reader.atEnd())
    {
        if(reader.isStartElement())
        {
            QStringRef name=reader.name();
            if(name=="rule")
            {
                DownloadRule *rule = new DownloadRule;
                rule->id=reader.attributes().value("id").toInt();
                rule->name=reader.attributes().value("name").toString();
                rule->searchWord=reader.attributes().value("searchWord").toString();
                rule->filterWord=reader.attributes().value("filterWord").toString();
                rule->scriptId=reader.attributes().value("scriptId").toString();
                rule->filePath=reader.attributes().value("filePath").toString();
                rule->minSize=reader.attributes().value("minSize").toInt();
                rule->maxSize=reader.attributes().value("maxSize").toInt();
                rule->lastCheckTime=reader.attributes().value("lastCheckTime").toLongLong();
                rule->state=(reader.attributes().value("enable")=="true"?0:2);
                rule->searchInterval=reader.attributes().value("searchInterval").toInt();
                rule->download=(reader.attributes().value("download")=="true");
                curRule = rule;
                ruleIdHash.insert(rule->id, rule);
                downloadRules.append(QSharedPointer<DownloadRule>(rule));
            }
            else if(name=="lastCheckPosition" && curRule)
            {
                curRule->lastCheckPosition<<reader.readElementText();
            }
            else if(name=="discoveredURLs" && curRule)
            {
                QString title(reader.attributes().value("title").toString());
                QString url(reader.attributes().value("url").toString());
                curRule->discoveredURLs.append(QPair<QString, QString>(title, url));
            }
        }
        else if(reader.isEndElement())
        {
            if(reader.name()=="rule") curRule=nullptr;
        }
        reader.readNext();
    }
    endResetModel();
}

void AutoDownloadManager::saveRules()
{
    QFile ruleFile(ruleFileName);
    bool ret=ruleFile.open(QIODevice::WriteOnly|QIODevice::Text);
    if(!ret) return;
    QXmlStreamWriter writer(&ruleFile);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("rules");
    for(auto &rule:downloadRules)
    {
        writer.writeStartElement("rule");
        writer.writeAttribute("id", QString::number(rule->id));
        writer.writeAttribute("name", rule->name);
        writer.writeAttribute("searchWord", rule->searchWord);
        writer.writeAttribute("filterWord", rule->filterWord);
        writer.writeAttribute("scriptId", rule->scriptId);
        writer.writeAttribute("filePath", rule->filePath);
        writer.writeAttribute("minSize", QString::number(rule->minSize));
        writer.writeAttribute("maxSize", QString::number(rule->maxSize));
        writer.writeAttribute("lastCheckTime", QString::number(rule->lastCheckTime));
        writer.writeAttribute("enable",rule->state==2?"false":"true");
        writer.writeAttribute("searchInterval", QString::number(rule->searchInterval));
        writer.writeAttribute("download", rule->download?"true":"false");

        for(QString &checkPosition: rule->lastCheckPosition)
        {
            writer.writeStartElement("lastCheckPosition");
            writer.writeCharacters(checkPosition);
            writer.writeEndElement();
        }
        for(auto &pair: rule->discoveredURLs)
        {
            writer.writeStartElement("discoveredURLs");
            writer.writeAttribute("title", pair.first);
            writer.writeAttribute("url", pair.second);
            writer.writeEndElement();
        }

        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();
}

QVariant AutoDownloadManager::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) return QVariant();
    const auto &rule=downloadRules.at(index.row());
    int col=index.column();
    switch (col)
    {
    case 0:
        if(role==Qt::DisplayRole)
            return status.at(rule->state);
        else if(role==Qt::DecorationRole)
            return statusIcons[rule->state];
        break;
    case 1:
        if(role==Qt::DisplayRole || role==Qt::ToolTipRole)
            return rule->name;
        break;
    case 2:
        if(role==Qt::DisplayRole || role==Qt::ToolTipRole)
            return rule->searchWord;
        break;
    case 3:
        if(role==Qt::DisplayRole || role==Qt::ToolTipRole)
            return rule->filterWord;
        break;
    case 4:
        if(role==Qt::DisplayRole)
            return rule->searchInterval;
        break;
    case 5:
        if(role==Qt::DisplayRole)
        {
            if(rule->lastCheckTime==0) return "--";
            return QDateTime::fromSecsSinceEpoch(rule->lastCheckTime).toString("yyyy-MM-dd hh:mm:ss");
        }
        break;
    default:
        break;
    }
    return QVariant();
}

QVariant AutoDownloadManager::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole&&orientation == Qt::Horizontal)
    {
        if(section<headers.count())return headers.at(section);
    }
    return QVariant();
}

DownloadRuleChecker::DownloadRuleChecker(QObject *parent) : QObject(parent), isChecking(false)
{

}

void DownloadRuleChecker::checkRules(QList<QSharedPointer<DownloadRule> > rules)
{
    for(auto &rule : rules)
    {
        if(ruleQueue.contains(rule)) continue;
        ruleQueue.append(rule);
    }
    if(isChecking) return;
    check();
}

void DownloadRuleChecker::check()
{
    isChecking = true;
    while(!ruleQueue.isEmpty())
    {
        auto rule = ruleQueue.dequeue();
        if(rule->state != 0) continue;
        if(!rule->lock.tryLock())
        {
            emit log(DownloadRuleLog::setLog(rule.get(), 2, tr("The rule is in use. Cancel check")));
            continue;
        }
        rule->state = 1;
        emit log(DownloadRuleLog::setLog(rule.get(), 0, tr("Begin checking...")));
        emit updateState(rule);
        fetchInfo(rule.get());
        rule->state = 0;
        emit log(DownloadRuleLog::setLog(rule.get(), 0, tr("Check done")));
        emit updateState(rule);
        rule->lock.unlock();
    }
    isChecking = false;
}

void DownloadRuleChecker::fetchInfo(DownloadRule *rule)
{
    QList<ResItem> searchResults;
    int page = 1, pageCount=1;
    const int maxPageCount = 3;
    QStringList newCheckPosition;
    QStringList filters(rule->filterWord.split(';'));
    QList<QRegExp> filterRegExps;
    for(const QString &s:filters)
        filterRegExps<<QRegExp(s);
    do
    {
        bool positionFinded=false;
        QString errInfo = GlobalObjects::scriptManager->search(rule->scriptId, rule->searchWord, page, pageCount, searchResults);
        if(errInfo.isEmpty())
        {
            if(page==1)
            {
                for(int i=0; i<3 && i<searchResults.size(); ++i)
                {
                    newCheckPosition<<searchResults.at(i).title;
                }
            }
            if(rule->lastCheckPosition.isEmpty()) break;
            for(auto &item:searchResults)
            {
                if(rule->lastCheckPosition.contains(item.title))
                {
                    positionFinded=true;
                    break;
                }
                if(satisfyRule(&item, rule, filterRegExps))
                {
                    emit log(DownloadRuleLog::setLog(rule, 1, QString("%1 %2").arg(item.size, item.title), item.magnet));
                }
            }
        }
        else
        {
            emit log(DownloadRuleLog::setLog(rule, 2, tr("Error occur while checking: %1").arg(errInfo)));
            break;
        }
        searchResults.clear();
        if(positionFinded) break;
    } while(page<pageCount && page<maxPageCount);
    if(!newCheckPosition.isEmpty())
    {
        rule->lastCheckPosition = newCheckPosition;
        rule->lastCheckTime = QDateTime::currentDateTime().toSecsSinceEpoch();
    }
}

bool DownloadRuleChecker::satisfyRule(ResItem *item, DownloadRule *rule, const QList<QRegExp> &filterRegExps)
{
    static QRegExp re("(\\d+(?:\\.\\d+)?)\\s*([KkMmGgTt])i?B|b");
    for(auto &r:filterRegExps)
    {
        if(!item->title.contains(r)) return false;
    }
    if(rule->minSize==0 && rule->maxSize==0) return true;
    if(re.indexIn(item->size)==-1) return false;
    if(re.matchedLength()!=item->size.length()) return false;
    QStringList captured=re.capturedTexts();
    float size = captured[1].toFloat();
    char s = captured[2][0].toUpper().toLatin1();
    switch (s)
    {
    case 'K':
        size /= 1024;
        break;
    case 'T':
        size *= 1024;
    case 'G':
        size *= 1024;
    default:
        break;
    }
    if(rule->minSize>0 && rule->maxSize==0)
        return size>rule->minSize;
    if(rule->minSize==0 && rule->maxSize>0)
        return size<rule->maxSize;
    return size>rule->minSize && size<rule->maxSize;
}

DownloadRuleLog DownloadRuleLog::setLog(DownloadRule *rule, int type, const QString &content, const QString &addition)
{
    DownloadRuleLog log;
    log.ruleId=rule->id;
    log.name=rule->name;
    log.time=QDateTime::currentDateTime().toSecsSinceEpoch();
    log.type=type;
    log.content=content;
    log.addition=addition;
    return log;
}

void LogModel::addLog(const DownloadRuleLog &log)
{
    if(logList.count()>1000)
    {
        beginRemoveRows(QModelIndex(), 0, 500);
        logList.erase(logList.begin(), logList.begin()+501);
        endRemoveRows();
    }
    beginInsertRows(QModelIndex(), logList.count(), logList.count());
    logList.append(log);
    endInsertRows();
}

void LogModel::removeLog(int ruleId)
{
    for(int i = logList.count()-1;i>=0;--i)
    {
        if(logList[i].ruleId==ruleId)
        {
            beginRemoveRows(QModelIndex(), i, i);
            logList.removeAt(i);
            endRemoveRows();
        }
    }
}

QString LogModel::getLog(const QModelIndex &index)
{
    const DownloadRuleLog &log=logList.at(index.row());
    return QString("[%1][%2]%3").arg(log.name, QDateTime::fromSecsSinceEpoch(log.time).toString("yyyy-MM-dd hh:mm:ss"), log.content);
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid()) return QVariant();
    const DownloadRuleLog &log=logList.at(index.row());
    int col=index.column();
    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        if(col==0) return QDateTime::fromSecsSinceEpoch(log.time).toString("yyyy-MM-dd hh:mm:ss");
        else if(col==1) return log.name;
        else return log.content;
    case Qt::ForegroundRole:
    {
        static QBrush foregroundBrush[] = {QColor(100,100,100),QColor(66,147,245),QColor(245,69,152)};
        return foregroundBrush[log.type];
    }
    case DownloadRuleIdRole:
        return log.ruleId;
    }
    return QVariant();
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole&&orientation == Qt::Horizontal)
    {
        if(section<headers.count())return headers.at(section);
    }
    return QVariant();
}

void URLModel::setRule(DownloadRule *curRule)
{
    beginResetModel();
    rule=curRule;
    endResetModel();
}

void URLModel::addURI(const QString &title, const QString &url)
{
    if(!rule) return;
    int pos = rule->discoveredURLs.count();
    beginInsertRows(QModelIndex(), pos, pos);
    rule->discoveredURLs.append(QPair<QString,QString>(title, url));
    endInsertRows();
}

QStringList URLModel::getSelectedURIs(const QModelIndexList &indexes)
{
    QStringList uris;
    if(!rule) return uris;
    for(auto &index:indexes)
    {
        if(!index.isValid()) continue;
        uris<<rule->discoveredURLs.at(index.row()).second;
    }
    return uris;
}

void URLModel::removeSelectedURIs(const QModelIndexList &indexes)
{
    if(!rule) return;
    QList<int> rows;
    for(const QModelIndex &index : indexes)
    {
        if (index.isValid())
        {
            int row=index.row();
            rows.append(row);
        }
    }
    std::sort(rows.rbegin(),rows.rend());
    for(auto iter=rows.begin();iter!=rows.end();++iter)
    {

        beginRemoveRows(QModelIndex(), *iter, *iter);
        rule->discoveredURLs.removeAt(*iter);
        endRemoveRows();
    }
    emit ruleChanged();
}

int URLModel::rowCount(const QModelIndex &parent) const
{
    if(!rule) return 0;
    return parent.isValid()?0:rule->discoveredURLs.count();
}

QVariant URLModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || !rule) return QVariant();
    const auto &pair=rule->discoveredURLs.at(index.row());
    switch (role)
    {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return index.column()==0?pair.first:pair.second;
    }
    return QVariant();
}

QVariant URLModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole&&orientation == Qt::Horizontal)
    {
        if(section<headers.count())return headers.at(section);
    }
    return QVariant();
}

void LogFilterProxyModel::setFilterId(int id)
{
    filterId=id;
    invalidateFilter();
}

bool LogFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if(filterId==-1) return true;
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    int id = sourceModel()->data(index, DownloadRuleIdRole).toInt();
    return id==filterId;
}
