/*
 * Copyright (C) 2011 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     lq <longqi_cm@deepin.com>
 *
 * Maintainer: lq <longqi_cm@deepin.com>
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

#include "secondaryscreendialog.h"
#include "widgets/dccslider.h"
#include "widgets/titlelabel.h"
#include "widgets/titledslideritem.h"
#include "window/gsettingwatcher.h"
#include "resolutionwidget.h"
#include "refreshratewidget.h"
#include "rotatewidget.h"
#include "modules/display/displaymodel.h"
#include "modules/display/monitorcontrolwidget.h"

#include <DFontSizeManager>
#include <QKeyEvent>
#include <QVBoxLayout>

using namespace dcc::widgets;
using namespace dcc::display;
using namespace DCC_NAMESPACE::display;
DWIDGET_USE_NAMESPACE

const int ComboxWidth = 200;
const int PercentageNum = 100;
const double BrightnessMaxScale = 100.0;
const double DoubleZero = 0.01; //后端传入的doube指为浮点型，有效位数为2位小数，存在精度丢失

SecondaryScreenDialog::SecondaryScreenDialog(QWidget *parent)
    : DAbstractDialog(parent)
    , m_contentLayout(new QVBoxLayout(this))
    , m_monitorControlWidget(new MonitorControlWidget(180, this))
    , m_resolutionWidget(new ResolutionWidget(ComboxWidth, this))
    , m_refreshRateWidget(new RefreshRateWidget(ComboxWidth, this))
    , m_rotateWidget(new RotateWidget(ComboxWidth, this))
    , m_model(nullptr)
    , m_monitor(nullptr)
{
    setFixedWidth(410);
    setMinimumHeight(480);
    setWindowFlags(Qt::CoverWindow | Qt::WindowStaysOnTopHint);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    m_monitorControlWidget->setAccessibleName("monitorControlWidget");

    m_contentLayout->setSpacing(10);
    m_contentLayout->setContentsMargins(35, 20, 35, 40);
    m_contentLayout->addWidget(m_monitorControlWidget);
    m_contentLayout->addSpacing(10);
    m_contentLayout->addWidget(m_resolutionWidget);
    m_contentLayout->addWidget(m_refreshRateWidget);
    m_contentLayout->addWidget(m_rotateWidget);
    m_contentLayout->addStretch();

    GSettingWatcher::instance()->bind("displayResolution", m_resolutionWidget);
    GSettingWatcher::instance()->bind("displayRefreshRate", m_refreshRateWidget);
    GSettingWatcher::instance()->bind("displayRotate", m_rotateWidget);

    setLayout(m_contentLayout);
}

SecondaryScreenDialog::~SecondaryScreenDialog()
{
    GSettingWatcher::instance()->erase("displayResolution", m_resolutionWidget);
    GSettingWatcher::instance()->erase("displayRefreshRate", m_refreshRateWidget);
    GSettingWatcher::instance()->erase("displayRotate", m_rotateWidget);
}

void SecondaryScreenDialog::OnRequestResizeDesktopVisibleChanged(bool visible)
{
    if (visible)
        setMinimumHeight(m_model->brightnessEnable() ? 610+48 : 480+48);
    else
        setMinimumHeight(m_model->brightnessEnable() ? 610 : 480);
}

void SecondaryScreenDialog::setModel(DisplayModel *model, dcc::display::Monitor *monitor)
{
    m_model = model;
    m_monitor = monitor;

    // 缩放大于1时,adjustSize()后对话框高度错误,设置最小高度
    setMinimumHeight(m_model->brightnessEnable() ? 610 : 480);
    m_monitorControlWidget->setScreensMerged(m_model->displayMode());
    m_monitorControlWidget->setModel(m_model, m_monitor);
    m_resolutionWidget->setModel(m_model, m_monitor);
    m_refreshRateWidget->setModel(m_model, m_monitor);
    m_rotateWidget->setModel(m_model, m_monitor);

    connect(m_monitorControlWidget, &MonitorControlWidget::requestRecognize, this, &SecondaryScreenDialog::requestRecognize);
    connect(m_monitorControlWidget, &MonitorControlWidget::requestGatherWindows, this, &SecondaryScreenDialog::requestGatherWindows);
    connect(this, &SecondaryScreenDialog::requestGatherEnabled, m_monitorControlWidget, &MonitorControlWidget::onGatherEnabled);
    connect(m_resolutionWidget, &ResolutionWidget::requestSetResolution, this, &SecondaryScreenDialog::requestSetResolution);
    connect(m_resolutionWidget, &ResolutionWidget::requestSetFillMode, this, &SecondaryScreenDialog::requestSetFillMode);
    connect(m_resolutionWidget, &ResolutionWidget::requestCurrFillModeChanged, this, &SecondaryScreenDialog::requestCurrFillModeChanged);
    connect(m_resolutionWidget, &ResolutionWidget::requestResizeDesktopVisibleChanged, this, &SecondaryScreenDialog::OnRequestResizeDesktopVisibleChanged);
    connect(m_refreshRateWidget, &RefreshRateWidget::requestSetResolution, this, &SecondaryScreenDialog::requestSetResolution);
    connect(m_rotateWidget, &RotateWidget::requestSetRotate, this, &SecondaryScreenDialog::requestSetRotate);

    auto tfunc = [this](const double tb) {
        int tmini = int(m_model->minimumBrightnessScale() * BrightnessMaxScale);
        int tnum = int(tb * BrightnessMaxScale);
        tnum = tnum > tmini ? tnum : tmini;
        return QString::number(int(tnum)) + "%";
    };

    if (m_monitor->canBrightness()) {
        TitleLabel *headTitle = new TitleLabel(tr("Brightness"), this); //亮度
        DFontSizeManager::instance()->bind(headTitle, DFontSizeManager::T7, QFont::Normal);

        //单独显示每个亮度调节名
        TitledSliderItem *slideritem = new TitledSliderItem(m_monitor->name(), this);
        slideritem->addBackground();
        DCCSlider *slider = slideritem->slider();
        int maxBacklight = static_cast<int>(m_model->maxBacklightBrightness());
        if (maxBacklight == 0) {
            int miniScale = int(m_model->minimumBrightnessScale() * BrightnessMaxScale);
            double brightness = m_monitor->brightness();
            slideritem->setValueLiteral(tfunc(brightness));
            slider->setRange(miniScale, int(BrightnessMaxScale));
            slider->setType(DCCSlider::Vernier);
            slider->setTickPosition(QSlider::TicksBelow);
            slider->setLeftIcon(QIcon::fromTheme("dcc_brightnesslow"));
            slider->setRightIcon(QIcon::fromTheme("dcc_brightnesshigh"));
            slider->setIconSize(QSize(24, 24));
            slider->setTickInterval(int((BrightnessMaxScale - miniScale) / 5.0));
            slider->setValue(int(brightness * BrightnessMaxScale));
            slider->setPageStep(1);

            auto onValueChanged = [=](int pos) {
                Q_EMIT requestSetMonitorBrightness(m_monitor, pos / BrightnessMaxScale);
                Q_EMIT requestAmbientLightAdjustBrightness(false);
            };

            connect(slider, &DCCSlider::valueChanged, this, onValueChanged);
            connect(slider, &DCCSlider::sliderMoved, this, onValueChanged);

            connect(m_monitor, &Monitor::brightnessChanged, this, [=](const double rb) {
                slider->blockSignals(true);
                if ((rb - m_model->minimumBrightnessScale()) < 0.00001) {
                    slideritem->setValueLiteral(QString("%1%").arg(int(m_model->minimumBrightnessScale() * BrightnessMaxScale)));
                    slider->setValue(int(m_model->minimumBrightnessScale() * BrightnessMaxScale));
                } else {
                    slideritem->setValueLiteral(QString("%1%").arg(int(rb * BrightnessMaxScale)));
                    slider->setValue(int(rb * BrightnessMaxScale));
                }
                slider->blockSignals(false);
            });

            connect(m_model, &DisplayModel::minimumBrightnessScaleChanged,
                    this, [=](const double ms) {
                        double rb = m_monitor->brightness();
                        int tmini = int(ms * PercentageNum);
                        slider->setMinimum(tmini);
                        slider->setTickInterval(int((BrightnessMaxScale - tmini) / 5.0));

                        slider->blockSignals(true);
                        slideritem->setValueLiteral(tfunc(rb));
                        slider->setValue(int(rb * BrightnessMaxScale));
                        slider->blockSignals(false);
                    });
        } else {
            int m_miniScales = int(m_model->minimumBrightnessScale() * maxBacklight);
            if (m_miniScales == 0) {
                m_miniScales = 1;
            }
            double brightness = m_monitor->brightness();
            slider->setRange(m_miniScales, maxBacklight);
            slider->setType(DCCSlider::Vernier);
            slider->setTickPosition(QSlider::TicksBelow);
            slider->setLeftIcon(QIcon::fromTheme("dcc_brightnesslow"));
            slider->setRightIcon(QIcon::fromTheme("dcc_brightnesshigh"));
            slider->setIconSize(QSize(24, 24));
            slider->setTickInterval(1);
            slider->setValue(int((brightness + DoubleZero) * maxBacklight));
            slider->setPageStep(1);
            QStringList speedList;
            int j = m_miniScales;
            for (; j <= maxBacklight; j++) {
                speedList << "";
            }
            slideritem->setAnnotations(speedList);

            auto onValueChanged = [=](int pos) {
                Q_EMIT requestSetMonitorBrightness(m_monitor, pos / double(maxBacklight));
                Q_EMIT requestAmbientLightAdjustBrightness(false);
            };

            connect(slider, &DCCSlider::valueChanged, this, onValueChanged);
            connect(slider, &DCCSlider::sliderMoved, this, onValueChanged);

            connect(m_monitor, &Monitor::brightnessChanged, this, [=](const double rb) {
                slider->blockSignals(true);
                if ((rb - m_model->minimumBrightnessScale()) < 0.00001) {
                    slider->setValue(int((m_model->minimumBrightnessScale() + DoubleZero) * maxBacklight));
                } else {
                    slider->setValue(int((rb + DoubleZero) * maxBacklight));
                }
                slider->blockSignals(false);
            });

            connect(m_model, &DisplayModel::minimumBrightnessScaleChanged,
                    this, [=](const double ms) {
                        double rb = m_monitor->brightness();
                        int tmini = int(ms * PercentageNum);
                        slider->setMinimum(tmini);
                        slider->setTickInterval(int((BrightnessMaxScale - tmini) / 5.0));

                        slider->blockSignals(true);
                        slideritem->setValueLiteral(tfunc(rb));
                        slider->setValue(int((rb + DoubleZero) * BrightnessMaxScale));
                        slider->blockSignals(false);
                    });
        }
        QWidget *brightnessWidget = new QWidget(this);
        brightnessWidget->setAccessibleName("SecondaryScreenDialog_brightnessWidget");
        QVBoxLayout *brightnessLayout = new QVBoxLayout(brightnessWidget);
        brightnessLayout->setContentsMargins(0, 10, 0, 0);
        brightnessLayout->addWidget(headTitle);
        brightnessLayout->setSpacing(10);
        brightnessLayout->addWidget(slideritem);
        brightnessWidget->setLayout(brightnessLayout);
        m_contentLayout->insertWidget(1, brightnessWidget);
        brightnessWidget->setVisible(m_model->brightnessEnable());
        connect(m_model, &DisplayModel::brightnessEnableChanged, this, [this, brightnessWidget](const bool enable) {
            brightnessWidget->setVisible(enable);
            resetDialog();
        });
    }

    resetDialog();
}

void SecondaryScreenDialog::resetDialog()
{
    adjustSize();

    auto rt = rect();
    if (rt.width() > m_monitor->w())
        rt.setWidth(m_monitor->w());

    if (rt.height() > m_monitor->h())
        rt.setHeight(m_monitor->h());

    auto mrt = m_monitor->rect();
    auto tsize = (mrt.size() / m_model->monitorScale(m_monitor) - rt.size()) / 2;
    rt.moveTo(m_monitor->x() + tsize.width(), m_monitor->y() + tsize.height());

    setGeometry(rt);
    show();
}

void SecondaryScreenDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        Q_EMIT requestCloseRecognize();
        break;
    default:
        QDialog::keyPressEvent(event);
        break;
    }
}
