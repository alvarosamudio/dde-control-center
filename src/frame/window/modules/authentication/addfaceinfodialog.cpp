/*
 * Copyright (C) 2011 ~ 2021 Deepin Technology Co., Ltd.
 *
 * Author:     duanhongyu <duanhongyu@uniontech.com>

 * Maintainer: duanhongyu <duanhongyu@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "addfaceinfodialog.h"
#include "modules/authentication/charamangermodel.h"

#include <DTitlebar>
#include <DFontSizeManager>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QCloseEvent>
#include <QTimer>
#include <QDebug>
#include <QDialog>

DWIDGET_USE_NAMESPACE

using namespace dcc::authentication;
using namespace DCC_NAMESPACE::authentication;

AddFaceInfoDialog::AddFaceInfoDialog(dcc::authentication::CharaMangerModel *model, Dtk::Widget::DAbstractDialog *parent)
    : DAbstractDialog(parent)
    , m_faceModel(model)
    , m_mainLayout(new QVBoxLayout(this))
    , m_cancelBtn(new QPushButton(this))
    , m_acceptBtn(new DSuggestButton(this))
    , m_currentState(dcc::authentication::CharaMangerModel::AddInfoState::StartState)
{
    initWidget();
    initConnect();
    QWidget::installEventFilter(this);
}

AddFaceInfoDialog::~AddFaceInfoDialog()
{

}

void AddFaceInfoDialog::closeEvent(QCloseEvent *event)
{
    Q_EMIT requesetCloseDlg();
    QDialog::closeEvent(event);
}

bool AddFaceInfoDialog::eventFilter(QObject *o, QEvent *e)
{
    if (o == this && QEvent::WindowDeactivate == e->type()) {
        clearFocus();
        setFocus();
        return true;
    }
    return false;
}

void AddFaceInfoDialog::initWidget()
{
    setFixedSize(QSize(454, 542));
    m_mainLayout->setAlignment(Qt::AlignHCenter);

    DTitlebar *titleIcon = new DTitlebar();
    titleIcon->setFrameStyle(QFrame::NoFrame);//无边框
    titleIcon->setBackgroundTransparent(true);//透明
    titleIcon->setMenuVisible(false);
    titleIcon->setTitle(tr("Enroll Face"));

    // 人脸图片
    m_facePic = new QLabel(this);
    m_facePic->setPixmap(QIcon::fromTheme(getFacePicture()).pixmap(128,128));

    // 提示信息
    m_resultTips = new QLabel(this);
    m_resultTips->hide();
    m_explainTips = new QLabel(tr("Make sure all parts of your face are not covered by objects and are clearly visible. Your face should be well-lit as well."));
    m_explainTips->setWordWrap(true);
    m_explainTips->setAlignment(Qt::AlignCenter);
    DFontSizeManager::instance()->bind(m_explainTips, DFontSizeManager::T8);

    QHBoxLayout *tips = new QHBoxLayout(this);
    tips->addWidget(m_explainTips, 0, Qt::AlignHCenter);
    tips->setContentsMargins(42, 10, 42, 10);

    // 免责声明
    m_disclaimersItem = new DisclaimersItem(this);
    m_disclaimersItem->show();

    // 下方按钮
    QHBoxLayout *btnLayout = new QHBoxLayout(this);
    m_cancelBtn->setText(tr("Cancel"));
    m_cancelBtn->hide();
    m_acceptBtn->setText(tr("Next"));
    m_acceptBtn->setDisabled(true);

    btnLayout->addWidget(m_cancelBtn, Qt::AlignCenter);
    btnLayout->addSpacing(10);
    btnLayout->addWidget(m_acceptBtn, Qt::AlignCenter);
    btnLayout->setContentsMargins(20, 10, 20, 20);

    m_mainLayout->addWidget(titleIcon, Qt::AlignTop | Qt::AlignRight);
    m_mainLayout->addSpacing(55);
    m_mainLayout->addWidget(m_facePic, 0, Qt::AlignHCenter);
    m_mainLayout->addSpacing(15);
    m_mainLayout->addWidget(m_resultTips, 0, Qt::AlignHCenter);
    m_mainLayout->addSpacing(10);
    m_mainLayout->addLayout(tips);
    m_mainLayout->addSpacing(100);
    m_mainLayout->addWidget(m_disclaimersItem, 0, Qt::AlignCenter);
    m_mainLayout->addLayout(btnLayout);

    m_mainLayout->setMargin(0);
    m_mainLayout->setSpacing(0);
    setLayout(m_mainLayout);

    this->activateWindow();
    this->setFocus();
}

void AddFaceInfoDialog::initConnect()
{
    connect(m_faceModel, &CharaMangerModel::enrollInfoState, this, &AddFaceInfoDialog::responseEnrollInfoState);

    connect(m_disclaimersItem, &DisclaimersItem::requestSetWindowEnabled, this, &AddFaceInfoDialog::onSetWindowEnabled);
    connect(m_disclaimersItem, &DisclaimersItem::requestStateChange, m_acceptBtn, &QPushButton::setDisabled);
    connect(m_cancelBtn, &QPushButton::clicked, this, &AddFaceInfoDialog::close);
    connect(m_acceptBtn, &QPushButton::clicked, this, &AddFaceInfoDialog::requestShowFaceInfoDialog);
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]{
        if (m_acceptBtn->text() == "Done") {
            this->close();
        }
    });
}

QString AddFaceInfoDialog::getFacePicture()
{
    QString theme;
    QString icon;
    DGuiApplicationHelper::ColorType type = DGuiApplicationHelper::instance()->themeType();
    switch (type) {
    case DGuiApplicationHelper::UnknownType:
        break;
    case DGuiApplicationHelper::LightType:
        theme = QString("light");
        break;
    case DGuiApplicationHelper::DarkType:
        theme = QString("dark");
        break;
    }

    switch (m_currentState) {
    case dcc::authentication::CharaMangerModel::AddInfoState::StartState:
        icon = QString("start");
        break;
    case dcc::authentication::CharaMangerModel::AddInfoState::Success:
        icon = QString("success");
        break;
    case dcc::authentication::CharaMangerModel::AddInfoState::Fail:
        icon = QString("fail");
        break;
    }

    return QString(":/authentication/themes/%1/icons/icon_face-%2.svg").arg(theme).arg(icon);
}

void AddFaceInfoDialog::responseEnrollInfoState(CharaMangerModel::AddInfoState state, const QString &tips)
{
    m_currentState = state;

    m_facePic->setPixmap(QIcon::fromTheme(getFacePicture()).pixmap(128, 128));
    if (m_currentState == CharaMangerModel::AddInfoState::Success) {
        m_resultTips->setText(tr("Face enrolled"));
        m_resultTips->show();

        m_explainTips->setText(tr("Use your face to unlock the device and make settings later"));

        m_disclaimersItem->hide();
        m_acceptBtn->hide();
        m_cancelBtn->show();
        m_cancelBtn->setText(tr("Done"));
    }

    if (m_currentState == CharaMangerModel::AddInfoState::Fail) {
        m_resultTips->setText(tr("Failed to enroll your face"));
        m_resultTips->show();

        m_explainTips->setText(tips);

        m_disclaimersItem->hide();
        m_acceptBtn->show();
        m_acceptBtn->setText(tr("Try Again"));
        m_acceptBtn->setDisabled(false);
        m_cancelBtn->show();
        m_cancelBtn->setText(tr("Close"));
    }

    this->show();
    Q_EMIT requestStopEnroll();
}

// 处理界面失焦效果 配合 模态对话框
void AddFaceInfoDialog::onSetWindowEnabled(const bool isEnabled)
{
    this->setEnabled(isEnabled);
}

