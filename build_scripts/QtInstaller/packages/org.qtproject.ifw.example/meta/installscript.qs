/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the FOO module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

function Component()
{
    // default constructor
    installer.gainAdminRights();
    component.loaded.connect(this, this.installerLoaded);
}

Component.prototype.createOperations = function()
{
    // call default implementation to actually install README.txt!
    component.createOperations();
    var targetDir = installer.value("TargetDir");
    var homeDir = installer.value("HomeDir"); 
    
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut", "@TargetDir@/unseenp2p.exe", "@DesktopDir@/Unseen Communicator.lnk","workingDirectory=@TargetDir@");
        component.addOperation("CreateShortcut", "@TargetDir@/unseenp2p.exe", "@StartMenuDir@/Unseen Communicator.lnk","workingDirectory=@TargetDir@");
    }
    else if (systemInfo.productType === "osx") {
	maintenanceToolPath = installer.value("TargetDir") + "/MaintenanceTool";
    }
    else {
	var desktopFileTarget = installer.value("HomeDir") + "/.local/share/applications";

        component.addOperation("CreateDesktopEntry", 
                                  targetDir + "/unseenp2p.desktop", 
                                  "Type=Application\nTerminal=false\nExec= " + targetDir + "/unseenp2p.app\nName=unseenp2p\nIcon= " + targetDir + "/unseenp2p.xpm");

    	component.addOperation("CreateDesktopEntry", 
                                  homeDir + "/Desktop/unseenp2p.desktop", 
                                  "Type=Application\nTerminal=false\nExec= " + targetDir + "/unseenp2p.app\nName=unseenp2p\nIcon= " + targetDir + "/unseenp2p.xpm");

	// in case autostart is not exist.
	component.addOperation("Mkdir", homeDir + "/.config/autostart");

    	component.addOperation("CreateDesktopEntry", 
                                  homeDir + "/.config/autostart/unseenp2p.desktop", 
                                  "Type=Application\nTerminal=false\nExec= " + targetDir + "/unseenp2p.app\nName=unseenp2p\nIcon= " + targetDir + "/unseenp2p.xpm");

    }
}

Component.prototype.installerLoaded = function()
{

    installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
    installer.addWizardPage(component, "TargetWidget", QInstaller.TargetDirectory);

    targetDirectoryPage = gui.pageWidgetByObjectName("DynamicTargetWidget");
    targetDirectoryPage.windowTitle = "Choose Installation Directory";
    targetDirectoryPage.description.setText("Please select where the UnseenP2P Communicator will be installed:");
    targetDirectoryPage.targetDirectory.textChanged.connect(this, this.targetDirectoryChanged);
    targetDirectoryPage.targetDirectory.setText(installer.value("TargetDir"));
    targetDirectoryPage.targetChooser.released.connect(this, this.targetChooserClicked);

    gui.pageById(QInstaller.ComponentSelection).entered.connect(this, this.componentSelectionPageEntered);
    //gui.pageById(QInstaller.PerformInstallation).entered.connect(this, this.componentPerformInstallationPageEntered);
}

Component.prototype.componentPerformInstallationPageEntered = function ()
{
    // before we install, we'll run the uninstaller silently (not shown)
}

Component.prototype.targetChooserClicked = function()
{
    var dir = QFileDialog.getExistingDirectory("", targetDirectoryPage.targetDirectory.text);
    targetDirectoryPage.targetDirectory.setText(dir);
}


Component.prototype.targetDirectoryChanged = function()
{
    var dir = targetDirectoryPage.targetDirectory.text;
    if (installer.fileExists(dir) && installer.fileExists(dir + "/maintenancetool.exe")) {
        targetDirectoryPage.warning.setText("<p style=\"color: red\">Existing installation detected and will be overwritten.</p>");
    }
    else if (installer.fileExists(dir)) {
        targetDirectoryPage.warning.setText("<p style=\"color: red\">Installing in existing directory. It will be wiped on uninstallation.</p>");
    }
    else {
        targetDirectoryPage.warning.setText("");
    }
    installer.setValue("TargetDir", dir);
}


Component.prototype.componentSelectionPageEntered = function()
{
    var dir = installer.value("TargetDir");
    if (installer.fileExists(dir) && installer.fileExists(dir + "/maintenancetool.exe")) {
        installer.execute(dir + "/maintenancetool.exe", "--script=" + dir + "/scripts/auto_uninstall.qs");
    }
}
