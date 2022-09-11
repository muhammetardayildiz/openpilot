#include "selfdrive/ui/qt/offroad/settings.h"

#include <cassert>
#include <cmath>
#include <string>

#include <QDebug>

#include "common/params.h"
#include "common/util.h"
#include "system/hardware/hw.h"
#include "selfdrive/ui/qt/widgets/controls.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"
#include "selfdrive/ui/qt/widgets/toggle.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/qt/qt_window.h"
#include "selfdrive/ui/qt/widgets/input.h"


void checkForUpdates() {
  std::system("pkill -SIGHUP -f selfdrive.updated");
}


SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  versionLbl = new ButtonControl(tr("Current Version"), tr("VIEW"), QString::fromStdString(params.get("ReleaseNotes")).trimmed());

  // TODO: define the onroad experience.

  // download update btn
  downloadBtn = new ButtonControl(tr("Download"), "DOWNLOAD");
  connect(downloadBtn, &ButtonControl::clicked, [=]() {
    downloadBtn->setEnabled(false);
    std::system("pkill -SIGUSR1 -f selfdrive.updated");
  });

  // install update btn
  installBtn = new ButtonControl(tr("Install Update"), "INSTALL");
  connect(installBtn, &ButtonControl::clicked, [=]() {
    Hardware::reboot();
  });
  installBtn->setVisible(false);

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

  // uninstall button
  auto uninstallBtn = new ButtonControl(tr("Uninstall %1").arg(getBrand()), tr("UNINSTALL"));
  connect(uninstallBtn, &ButtonControl::clicked, [&]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to uninstall?"), this)) {
      params.putBool("DoUninstall", true);
    }
  });
  connect(uiState(), &UIState::offroadTransition, uninstallBtn, &QPushButton::setEnabled);

  QWidget *widgets[] = {versionLbl, downloadBtn, installBtn, branchSwitcherBtn, uninstallBtn};
  for (QWidget* w : widgets) {
    if (w == branchSwitcherBtn && params.getBool("IsTestedBranch")) {
      continue;
    }
    addItem(w);
  }

  fs_watch = new QFileSystemWatcher(this);
  QObject::connect(fs_watch, &QFileSystemWatcher::fileChanged, [=](const QString path) {
    updateLabels();
  });
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  updateLabels();
}

void SoftwarePanel::updateLabels() {
  // add these back in case the files got removed
  fs_watch->addPath(QString::fromStdString(params.getParamPath("LastUpdateTime")));
  fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateFailedCount")));
  fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdaterState")));
  fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateAvailable")));

  QString lastUpdate = "never";
  auto tm = params.get("LastUpdateTime");
  if (!tm.empty()) {
    lastUpdate = timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
  }


  branchSwitcherBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));

  versionLbl->setValue(getBrandVersion());

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
      downloadBtn->setValue("update available");
    } else {
      downloadBtn->setText("CHECK");
      downloadBtn->setValue("up to date (" + lastUpdate + ")");
    }
    downloadBtn->setEnabled(true);
  }

  installBtn->setValue(QString::fromStdString(params.get("UpdaterNewDescription")));
  installBtn->setVisible(params.getBool("UpdateAvailable"));

  update();

  /*
  gitBranchLbl->setText(QString::fromStdString(params.get("GitBranch")));
  gitCommitLbl->setText(QString::fromStdString(params.get("GitCommit")).left(10));
  osVersionLbl->setText(QString::fromStdString(Hardware::get_os_version()).trimmed());
  */
}
