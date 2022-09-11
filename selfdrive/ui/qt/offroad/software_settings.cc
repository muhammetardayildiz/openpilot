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
  std::system("pkill -1 -f selfdrive.updated");
}


SoftwarePanel::SoftwarePanel(QWidget* parent) : ListWidget(parent) {
  // TODO: move these into the versionLbl description
  gitBranchLbl = new LabelControl(tr("Git Branch"));
  gitCommitLbl = new LabelControl(tr("Git Commit"));
  osVersionLbl = new LabelControl(tr("OS Version"));
  versionLbl = new LabelControl(tr("Current Version"), "", QString::fromStdString(params.get("ReleaseNotes")).trimmed());

  //
  updateBtn = new ButtonControl(tr("Check for update"), "");
  connect(updateBtn, &ButtonControl::clicked, [=]() {
    fs_watch->addPath(QString::fromStdString(params.getParamPath("LastUpdateTime")));
    fs_watch->addPath(QString::fromStdString(params.getParamPath("UpdateFailedCount")));
    updateBtn->setText(tr("CHECKING"));
    updateBtn->setEnabled(false);
    checkForUpdates();
  });
  connect(uiState(), &UIState::offroadTransition, updateBtn, &QPushButton::setEnabled);


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

  QWidget *widgets[] = {versionLbl, updateBtn, branchSwitcherBtn, uninstallBtn};
  for (QWidget* w : widgets) {
    if (w == branchSwitcherBtn && params.getBool("IsTestedBranch")) {
      continue;
    }
    addItem(w);
  }

  fs_watch = new QFileSystemWatcher(this);
  QObject::connect(fs_watch, &QFileSystemWatcher::fileChanged, [=](const QString path) {
    if (path.contains("UpdateFailedCount") && std::atoi(params.get("UpdateFailedCount").c_str()) > 0) {
      updateBtn->setValue(tr("failed to fetch update"));
      updateBtn->setText(tr("CHECK"));
      updateBtn->setEnabled(true);
    }
    updateLabels();
  });
}

void SoftwarePanel::showEvent(QShowEvent *event) {
  updateLabels();
}

void SoftwarePanel::updateLabels() {
  QString lastUpdate = "";
  auto tm = params.get("LastUpdateTime");
  if (!tm.empty()) {
    lastUpdate = "Last checked " + timeAgo(QDateTime::fromString(QString::fromStdString(tm + "Z"), Qt::ISODate));
  }


  branchSwitcherBtn->setValue(QString::fromStdString(params.get("UpdaterTargetBranch")));

  versionLbl->setText(getBrandVersion());
  updateBtn->setValue(lastUpdate);
  updateBtn->setText(tr("CHECK"));
  updateBtn->setEnabled(true);

  gitBranchLbl->setText(QString::fromStdString(params.get("GitBranch")));
  gitCommitLbl->setText(QString::fromStdString(params.get("GitCommit")).left(10));
  osVersionLbl->setText(QString::fromStdString(Hardware::get_os_version()).trimmed());
}
