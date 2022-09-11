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


void checkForUpdates() {
  std::system("pkill -SIGHUP -f selfdrive.updated");
}

SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  // TODO: make the onroad experience nice
  onroadLbl = new QLabel("Updates are only downloaded while the car is off.");
  onroadLbl->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left;");
  addItem(onroadLbl);

  // current version
  versionLbl = new ButtonControl(tr("Current Version"), tr("VIEW"), QString::fromStdString(params.get("ReleaseNotes")).trimmed());
  addItem(versionLbl);

  // download update btn
  downloadBtn = new ButtonControl(tr("Download"), "DOWNLOAD");
  connect(downloadBtn, &ButtonControl::clicked, [=]() {
    downloadBtn->setEnabled(false);
    std::system("pkill -SIGUSR1 -f selfdrive.updated");
  });
  addItem(downloadBtn);

  // install update btn
  installBtn = new ButtonControl(tr("Install Update"), "INSTALL");
  connect(installBtn, &ButtonControl::clicked, [=]() {
    installBtn->setEnabled(false);
    Hardware::reboot();
  });
  addItem(installBtn);

  // TODO: hide this on release
  // branch selecting
  branchSwitcherBtn = new ButtonControl(tr("Target Branch"), tr("CHANGE"), tr("The target branch will be pulled the next time the updater runs."));
  connect(branchSwitcherBtn, &ButtonControl::clicked, [=]() {
    QStringList branches = QString::fromStdString(params.get("UpdaterAvailableBranches")).split(",");
    QString currentVal = QString::fromStdString(params.get("UpdaterTargetBranch"));
    QString selection = MultiOptionDialog::getSelection(tr("Select a branch"), branches, currentVal, this);
    if (!selection.isEmpty()) {
      params.put("UpdaterTargetBranch", selection.toStdString());
      branchSwitcherBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));
    }
    checkForUpdates();
  });
  addItem(branchSwitcherBtn);

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

  connect(uiState(), &UIState::offroadTransition, [=](bool onroad) {
    is_onroad = onroad;
    updateLabels();
  });

  updateLabels();
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  checkForUpdates();
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
  installBtn->setVisible(!is_onroad);
  downloadBtn->setVisible(!is_onroad);

  QString lastUpdate = "never";
  auto tm = params.get("LastUpdateTime");
  if (!tm.empty()) {
    lastUpdate = timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
  }


  branchSwitcherBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));

  auto branch = QString::fromStdString(params.get("GitBranch"));
  auto commit = QString::fromStdString(params.get("GitCommit")).left(7);
  versionLbl->setValue(QString("0.8.17") + " / " + branch + " / " + commit);

  // download update
  QString updater_state = QString::fromStdString(params.get("UpdaterState"));
  if (updater_state != "idle") {
    downloadBtn->setEnabled(false);
    downloadBtn->setText(updater_state.toUpper());
  } else {
    // TODO: handle empty updater state?

    if (params.getBool("UpdaterFetchAvailable")) {
      downloadBtn->setText("DOWNLOAD");
      //downloadBtn->setValue("master (f6398ea) -> master (29f9c53)");
      downloadBtn->setValue("doing stuff...");
    } else {
      downloadBtn->setText("CHECK");
      downloadBtn->setValue("up to date (" + lastUpdate + ")");
    }
    downloadBtn->setEnabled(true);
  }

  installBtn->setVisible(params.getBool("UpdateAvailable"));
  installBtn->setValue(QString::fromStdString(params.get("UpdaterNewDescription")));

  update();

  /*
  osVersionLbl->setText(QString::fromStdString(Hardware::get_os_version()).trimmed());
  */
}
