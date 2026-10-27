import QtQuick

Rectangle {
    id: root
    color: Theme.bgTopBar
    height: Theme.topBarHeight

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Rectangle {
            width: 10
            height: 10
            radius: 2
            color: Theme.safe
            opacity: 0.6
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: "SLF-EWN"
            color: Theme.text
            font.family: App.uiFont
            font.pixelSize: 15
            font.weight: Font.Medium
            font.letterSpacing: -0.2
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        id: pill
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: pillRow.implicitWidth + 18
        height: 28
        radius: 14
        color: Theme.bgCard
        border.color: Theme.border
        border.width: 1

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 8
            StatusDot {
                linkState: App.connectionState
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: {
                    if (App.connectionState === 4)
                        return "Simulation"
                    if (App.connectionState === 2)
                        return App.portName + "  ·  Linked"
                    if (App.connectionState === 1)
                        return "Linking…"
                    if (App.connectionState === 3)
                        return "Link failed"
                    return "Offline"
                }
                color: Theme.text
                font.family: App.monoFont
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Text {
            visible: App.dumpStatus.length > 0
            text: App.dumpStatus
            color: App.dumpBusy ? Theme.safe : Theme.textDim
            font.family: App.uiFont
            font.pixelSize: 11
            elide: Text.ElideRight
            width: Math.min(implicitWidth, 220)
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: App.clock
            color: Theme.textMuted
            font.family: App.monoFont
            font.pixelSize: 12
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: dumpLab.implicitWidth + 16
            height: 28
            radius: 8
            color: refreshMouse.containsMouse ? Theme.bgElevated : "transparent"
            border.color: Theme.border
            border.width: 1
            opacity: App.connectionState === 2 ? 1 : 0.4
            Behavior on color { ColorAnimation { duration: Theme.motion } }

            Text {
                id: dumpLab
                anchors.centerIn: parent
                text: App.dumpBusy ? (App.dumpPhase.length ? App.dumpPhase : "Working…") : "TX FULL_DUMP"
                color: Theme.text
                font.family: App.monoFont
                font.pixelSize: 11
                font.weight: Font.Medium
            }
            MouseArea {
                id: refreshMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: App.connectionState === 2 && !App.dumpBusy
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: App.refreshFromReceiver()
            }
        }

        Rectangle {
            width: 1
            height: 20
            color: Theme.border
            anchors.verticalCenter: parent.verticalCenter
        }

        Row {
            spacing: 12
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: Auth.username
                color: Theme.text
                font.family: App.uiFont
                font.pixelSize: 12
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "Sign out"
                color: signOutMouse.containsMouse ? Theme.text : Theme.textDim
                font.family: App.uiFont
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 160 } }
                MouseArea {
                    id: signOutMouse
                    anchors.fill: parent
                    anchors.margins: -6
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Auth.logout()
                }
            }
        }
    }
}
