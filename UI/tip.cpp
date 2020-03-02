﻿#include "tip.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QFile>

Tip::Tip(QWidget *parent) : CFramelessDialog (tr("Tip"),parent)
{
    QLabel *tipContent = new QLabel(this);
    tipContent->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tipContent->setWordWrap(true);
    tipContent->setOpenExternalLinks(true);
    QFile tipFile(":/res/tip");
    tipFile.open(QFile::ReadOnly);
    if(tipFile.isOpen())
    {
        tipContent->setText(tipFile.readAll());
    }
    QVBoxLayout *tipVLayout=new QVBoxLayout(this);
    tipVLayout->addWidget(tipContent);
    tipVLayout->addStretch(1);
}
