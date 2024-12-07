# Copyright (c) 2022 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from PyQt5 import QtCore, QtWidgets
from PyQt5.QtWidgets import QMainWindow, QTextEdit, QGridLayout, QWidget, QHBoxLayout
from PyQt5.QtWidgets import QPushButton
from PyQt5.QtCore import QSize, Qt

from .util import wait_until

class MainWindow(QMainWindow):
    def __init__(self, app, steps):
        self.app = app
        self.steps = steps
        self.currentStep = 0
        self.maxReachedStep = 0

        QMainWindow.__init__(self)

        screenrect = app.primaryScreen().geometry()
        self.move(screenrect.left(), screenrect.top())
        self.setMinimumSize(QSize(300, 260))
        self.resize(QSize(300, 260))
        self.setWindowTitle("Bitcoin Cash Node - UI Test Plan")
        self.setWindowFlags(QtCore.Qt.WindowStaysOnTopHint)


        self.horizontalGroupBox = QWidget(self)
        self.horizontalGroupBox.resize(self.size())

        layout = QGridLayout()
        layout.setRowStretch(1, 1)

        self.stepDescription = QTextEdit()
        self.stepDescription.setText("Step 1/6")
        self.stepDescription.setReadOnly(True)
        # self.stepDescription.setStyleSheet("background: red")
        layout.addWidget(self.stepDescription, 0,0,0,3, Qt.AlignTop)


        buttons = QWidget()
        hLayout = QHBoxLayout()

        self.backButton = QPushButton('Back', self)
        self.backButton.setEnabled(False)
        self.backButton.clicked.connect(self.back)
        hLayout.addWidget(self.backButton)

        self.advanceButton = QPushButton('Advance', self)
        self.advanceButton.clicked.connect(self.advance)
        hLayout.addWidget(self.advanceButton)

        buttons.setLayout(hLayout)
        layout.addWidget(buttons,1,0,1,3, Qt.AlignBottom)

        self.horizontalGroupBox.setLayout(layout)

        self.setCentralWidget(self.horizontalGroupBox)

        self.updateUI()

    def updateUI(self):
        step = self.steps[self.currentStep]

        self.stepDescription.setText(f"Step {self.currentStep+1}/{len(self.steps)}\n{step['description']}")
        self.backButton.setEnabled(self.currentStep > 0)
        self.advanceButton.setText("Finish" if self.currentStep+1 == len(self.steps) else "Advance")

    def advance(self):
        self.currentStep += 1
        if (self.currentStep > self.maxReachedStep):
            self.maxReachedStep = self.currentStep

        if (self.currentStep >= len(self.steps)):
            self.close()
        else:
            self.updateUI()

    def back(self):
        if (self.currentStep > 0):
            self.currentStep -= 1
        self.updateUI()

    def waitUntilMaxReachedStep(self, step):
        def process():
            self.app.processEvents()
            return self.maxReachedStep >= step
        wait_until(lambda: process(), timeout=1e10)

def RunTestPlan(steps, framework):
    app = QtWidgets.QApplication([''])
    mainWindow = MainWindow(app, steps)
    framework.testPlan = mainWindow
    mainWindow.show()
    framework.main()
    app.exec()
    return mainWindow

