#include "selfdrive/ui/qt/offroad/settings.h"

#include <cassert>
#include <cmath>
#include <string>

#include <QDebug>
#include <QLabel>

#include "common/params.h"
#include "common/util.h"
#include "system/hardware/hw.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"
#include "selfdrive/ui/qt/widgets/toggle.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/widgets/input.h"


void SoftwarePanel::checkForUpdates() {
  std::system("pkill -SIGHUP -f selfdrive.updated");
}

SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  onroadLbl = new QLabel("Updates are only downloaded while the car is off.");
  onroadLbl->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left; padding-top: 30px; padding-bottom: 30px;");
  addItem(onroadLbl);

  // current version
  versionLbl = new ButtonControl(tr("Current Version"), tr("VIEW"));
  addItem(versionLbl);

  // download update btn
  downloadBtn = new ButtonControl(tr("Download"), "DOWNLOAD");
  connect(downloadBtn, &ButtonControl::clicked, [=]() {
    downloadBtn->setEnabled(false);

    if (downloadBtn->text() == "CHECK") {
      checkForUpdates();
    } else {
      std::system("pkill -SIGUSR1 -f selfdrive.updated");
    }
  });
  addItem(downloadBtn);

  // install update btn
  installBtn = new ButtonControl(tr("Install Update"), "INSTALL");
  connect(installBtn, &ButtonControl::clicked, [=]() {
    installBtn->setEnabled(false);
    Hardware::reboot();
  });
  addItem(installBtn);

  // branch selecting
  targetBranchBtn = new ButtonControl(tr("Target Branch"), tr("SELECT"));
  connect(targetBranchBtn, &ButtonControl::clicked, [=]() {
    QStringList branches = QString::fromStdString(params.get("UpdaterAvailableBranches")).split(",");
    QString currentVal = QString::fromStdString(params.get("UpdaterTargetBranch"));
    QString selection = MultiOptionDialog::getSelection(tr("Select a branch"), branches, currentVal, this);
    if (!selection.isEmpty()) {
      params.put("UpdaterTargetBranch", selection.toStdString());
      targetBranchBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));
      checkForUpdates();
    }
  });
  if (!params.getBool("IsTestedBranch")) {
    addItem(targetBranchBtn);
  }

  // uninstall button
  auto uninstallBtn = new ButtonControl(tr("Uninstall %1").arg(getBrand()), tr("UNINSTALL"));
  connect(uninstallBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to uninstall?"), this)) {
      params.putBool("DoUninstall", true);
    }
  });
  addItem(uninstallBtn);

  fs_watch = new QFileSystemWatcher(this);
  QObject::connect(fs_watch, &QFileSystemWatcher::fileChanged, [=](const QString path) {
    updateLabels();
  });

  connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    is_onroad = !offroad;
    updateLabels();
  });

  updateLabels();
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  // nice for testing on PC
  installBtn->setEnabled(true);

  updateLabels();
}

void SoftwarePanel::updateLabels() {
  // add these back in case the files got removed
  fs_watch->addPath(QString::fromStdString(params.getParamPath("LastUpdateTime")));
  fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateFailedCount")));
  fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdaterState")));
  fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateAvailable")));


  // updater only runs offroad
  onroadLbl->setVisible(is_onroad);
  downloadBtn->setVisible(!is_onroad);

  // download update
  QString updater_state = QString::fromStdString(params.get("UpdaterState"));
  if (updater_state != "idle") {
    downloadBtn->setEnabled(false);
    downloadBtn->setValue(updater_state);
  } else {
    if (std::atoi(params.get("UpdateFailedCount").c_str()) > 0) {
      downloadBtn->setText("CHECK");
      downloadBtn->setValue("failed to fetch update");
    } else if (params.getBool("UpdaterFetchAvailable")) {
      downloadBtn->setText("DOWNLOAD");
      downloadBtn->setValue("update available");
    } else {
      QString lastUpdate = "never";
      auto tm = params.get("LastUpdateTime");
      if (!tm.empty()) {
        lastUpdate = timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
      }
      downloadBtn->setText("CHECK");
      downloadBtn->setValue("up to date (" + lastUpdate + ")");
    }
    downloadBtn->setEnabled(true);
  }
  targetBranchBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));

  // current + new versions
  auto branch = QString::fromStdString(params.get("GitBranch"));
  auto commit = QString::fromStdString(params.get("GitCommit")).left(7);
  versionLbl->setValue(QString(QString("0.8.17") + " / " + branch + " / " + commit).left(35));

  installBtn->setVisible(!is_onroad && params.getBool("UpdateAvailable"));
  QString desc = QString::fromStdString(params.get("UpdaterNewDescription"));
  installBtn->setValue(desc.left(35));

  update();
}
